#include <stdio.h>
#include <assert.h>
#ifdef WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <string.h>
#endif
#include <stdlib.h>

#include "reslist.h"
#include "ring.h"
#include "thread_mutex.h"
#include "thread_cond.h"
//#include "allocator.h"

#if HAS_THREADS

/**
 * A single resource element.
 */
struct res_t {
    long long freed;//time
    void *opaque;
    RING_ENTRY(res_t) link;
};
typedef struct res_t res_t;

/**
 * A ring of resources representing the list of available resources.
 */
RING_HEAD(resring_t, res_t);
typedef struct resring_t resring_t;

struct reslist_t {
//    allocator_t *allocator; /* used in constructor and destructor calls */
    int ntotal;     /* total number of resources managed by this list */
    int nidle;      /* number of available resources */
    int min;  /* desired minimum number of available resources */
    int smax; /* soft maximum on the total number of resources */
    int hmax; /* hard maximum on the total number of resources */
    long long ttl; /* TTL when we have too many resources */
    long long timeout; /* Timeout for waiting on resource */
    reslist_constructor constructor;
    reslist_destructor destructor;
    void *params; /* opaque data passed to constructor and destructor calls */
    resring_t avail_list;
    resring_t free_list;
    thread_mutex_t *listlock;
    thread_cond_t *avail;
};

/**
 * Grab a resource from the front of the resource list.
 * Assumes: that the reslist is locked.
 */
static res_t *pop_resource(reslist_t *reslist)
{
    res_t *res;
    res = RING_FIRST(&reslist->avail_list);
    RING_REMOVE(res, link);
    reslist->nidle--;
    return res;
}

#ifdef WIN32
#define DELTA_EPOCH_IN_USEC   (long long)(11644473600000000)
static void FileTimeToTime(long long *result, FILETIME *input)
{
    /* Convert FILETIME one 64 bit number so we can work with it. */
    *result = input->dwHighDateTime;
    *result = (*result) << 32;
    *result |= input->dwLowDateTime;
    *result /= 10;    /* Convert from 100 nano-sec periods to micro-seconds. */
    *result -= DELTA_EPOCH_IN_USEC;  /* Convert from Windows epoch to Unix epoch */
    return;
}

static long long time_now(void)
{
    LONGLONG ltime = 0;
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    FileTimeToTime(&ltime, &time);
    return ltime; 
}
#else
#define USEC_PER_SEC (long long)(1000000)
static long long time_now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * USEC_PER_SEC + tv.tv_usec;
}
#endif
/**
 * Add a resource to the beginning of the list, set the time at which
 * it was added to the list.
 * Assumes: that the reslist is locked.
 */
static void push_resource(reslist_t *reslist, res_t *resource)
{
    RING_INSERT_HEAD(&reslist->avail_list, resource, res_t, link);
    resource->freed = time_now();
    reslist->nidle++;
}

/**
 * Get an resource container from the free list or create a new one.
 */
static res_t *get_container(reslist_t *reslist)
{
    res_t *res;

    
    if (!RING_EMPTY(&reslist->free_list, res_t, link)) {
        res = RING_FIRST(&reslist->free_list);
        RING_REMOVE(res, link);
    }
    else
	    //fix me may be use allocator
        res = (res_t *)malloc(sizeof(*res));
	
    return res;
}

/**
 * Free up a resource container by placing it on the free list.
 */
static void free_container(reslist_t *reslist, res_t *container)
{
    RING_INSERT_TAIL(&reslist->free_list, container, res_t, link);
}

/**
 * Create a new resource and return it.
 * Assumes: that the reslist is locked.
 */
static int create_resource(reslist_t *reslist, res_t **ret_res)
{
    int rv;
    res_t *res;

    res = get_container(reslist);

    rv = reslist->constructor(&res->opaque, reslist->params);

    *ret_res = res;
    return rv;
}

/**
 * Destroy a single idle resource.
 * Assumes: that the reslist is locked.
 */
static int destroy_resource(reslist_t *reslist, res_t *res)
{
    int rv = reslist->destructor(res->opaque, reslist->params);
    return rv;
}

static int reslist_cleanup(void *data_)
{
    int rv = 0;
    reslist_t *rl = (reslist_t *)data_;
    res_t *res;
    
    if(!rl)
	    return rv;

    thread_mutex_lock(rl->listlock);

    while (rl->nidle > 0) {
        int rv1;
        res = pop_resource(rl);
        rl->ntotal--;
        rv1 = destroy_resource(rl, res);
        if (rv1 != 0) {
            rv = rv1;  /* loses info in the unlikely event of
                        * multiple *different* failures */
        }
        free_container(rl, res);
    }

    assert(rl->nidle == 0);
    assert(rl->ntotal == 0);

    thread_mutex_unlock(rl->listlock);
    thread_mutex_destroy(rl->listlock);
    thread_cond_destroy(rl->avail);

    free(rl);
    rl = NULL;

    return rv;
}

/**
 * Perform routine maintenance on the resource list. This call
 * may instantiate new resources or expire old resources.
 */
static int reslist_maint(reslist_t *reslist)
{
    long long now;
    int rv;
    res_t *res;
    int created_one = 0;

    thread_mutex_lock(reslist->listlock);

    /* Check if we need to create more resources, and if we are allowed to. */
    while (reslist->nidle < reslist->min && reslist->ntotal < reslist->hmax) {
        /* Create the resource */
        rv = create_resource(reslist, &res);
        if (rv != 0) {
            //FIX:memory leak we reconnect by zhou
            //free_container(reslist, res);
            free(res);
            thread_mutex_unlock(reslist->listlock);
            return rv;
        }
        /* Add it to the list */
        push_resource(reslist, res);
        /* Update our counters */
        reslist->ntotal++;
        /* If someone is waiting on that guy, wake them up. */
        rv = thread_cond_signal(reslist->avail);
        if (rv != 0) {
            thread_mutex_unlock(reslist->listlock);
            return rv;
        }
        created_one++;
    }

    /* We don't need to see if we're over the max if we were under it before */
    if (created_one) {
        thread_mutex_unlock(reslist->listlock);
        return 0;
    }

    /* Check if we need to expire old resources */
    now = time_now();
    while (reslist->nidle > reslist->smax && reslist->nidle > 0) {
        /* Peak at the last resource in the list */
        res = RING_LAST(&reslist->avail_list);
        /* See if the oldest entry should be expired */
        if (now - res->freed < reslist->ttl) {
            /* If this entry is too young, none of the others
             * will be ready to be expired either, so we are done. */
            break;
        }
        RING_REMOVE(res, link);
        reslist->nidle--;
        reslist->ntotal--;
        rv = destroy_resource(reslist, res);
        free_container(reslist, res);
        if (rv != 0) {
            thread_mutex_unlock(reslist->listlock);
            return rv;
        }
    }

    thread_mutex_unlock(reslist->listlock);
    return 0;
}

int reslist_create(reslist_t **reslist,
                                             int min, int smax, int hmax,
                                             long long ttl,
                                             reslist_constructor con,
                                             reslist_destructor de,
                                             void *params
                                             /*,allocator_t *pool*/)
{
    int rv;
    reslist_t *rl;

    /* Do some sanity checks so we don't thrash around in the
     * maintenance routine later. */
    if (min < 0 || min > smax || min > hmax || smax > hmax || hmax == 0 ||
        ttl < 0) {
        return -1;
    }

    rl = (reslist_t *)malloc(sizeof(*rl));

	memset(rl,0,sizeof(*rl));
    rl->min = min;
    rl->smax = smax;
    rl->hmax = hmax;
    rl->ttl = ttl;
    rl->constructor = con;
    rl->destructor = de;
    rl->params = params;

    RING_INIT(&rl->avail_list, res_t, link);
    RING_INIT(&rl->free_list, res_t, link);

    rv = thread_mutex_create(&rl->listlock, THREAD_MUTEX_DEFAULT);

    if (rv != 0) {
        return rv;
    }
    rv = thread_cond_create(&rl->avail);
    if (rv != 0) {
        return rv;
    }

    rv = reslist_maint(rl);
    if (rv != 0) {
        /* Destroy what we've created so far.
         */
        reslist_cleanup(rl);
        return rv;
    }

    *reslist = rl;

    return 0;
}

int reslist_destroy(reslist_t *reslist)
{
    return reslist_cleanup(reslist);
}

int reslist_acquire(reslist_t *reslist,
                                              void **resource)
{
    int rv;
    res_t *res;
    long long now;

    thread_mutex_lock(reslist->listlock);
    /* If there are idle resources on the available list, use
     * them right away. */
    now = time_now();
    while (reslist->nidle > 0) {
        /* Pop off the first resource */
        res = pop_resource(reslist);
        if (reslist->ttl && (now - res->freed >= reslist->ttl)) {
            /* this res is expired - kill it */
            reslist->ntotal--;
            rv = destroy_resource(reslist, res);
            free_container(reslist, res);
            if (rv != 0) {
                thread_mutex_unlock(reslist->listlock);
                return rv;  /* FIXME: this might cause unnecessary fails */
            }
            continue;
        }
        *resource = res->opaque;
        free_container(reslist, res);
        thread_mutex_unlock(reslist->listlock);
        return 0;
    }
    /* If we've hit our max, block until we're allowed to create
     * a new one, or something becomes free. */
    while (reslist->ntotal >= reslist->hmax && reslist->nidle <= 0) {
        if (reslist->timeout) {
            if ((rv = thread_cond_timedwait(reslist->avail, 
                reslist->listlock, reslist->timeout)) != 0) {
                thread_mutex_unlock(reslist->listlock);
                return rv;
            }
        }
        else {
            thread_cond_wait(reslist->avail, reslist->listlock);
        }
    }
    /* If we popped out of the loop, first try to see if there
     * are new resources available for immediate use. */
    if (reslist->nidle > 0) {
        res = pop_resource(reslist);
        *resource = res->opaque;
        free_container(reslist, res);
        thread_mutex_unlock(reslist->listlock);
        return 0;
    }
    /* Otherwise the reason we dropped out of the loop
     * was because there is a new slot available, so create
     * a resource to fill the slot and use it. */
    else {
        rv = create_resource(reslist, &res);
        if (rv == 0) {
            reslist->ntotal++;
            *resource = res->opaque;
        }
        free_container(reslist, res);
        thread_mutex_unlock(reslist->listlock);
        return rv;
    }
}

int reslist_release(reslist_t *reslist,
                                              void *resource)
{
    res_t *res;

    thread_mutex_lock(reslist->listlock);
    res = get_container(reslist);
    res->opaque = resource;
    push_resource(reslist, res);
    thread_cond_signal(reslist->avail);
    thread_mutex_unlock(reslist->listlock);

    return reslist_maint(reslist);
}

void reslist_timeout_set(reslist_t *reslist,
                                          long long timeout)
{
    reslist->timeout = timeout;
}

unsigned int reslist_acquired_count(reslist_t *reslist)
{
    unsigned int count;

    thread_mutex_lock(reslist->listlock);
    count = reslist->ntotal - reslist->nidle;
    thread_mutex_unlock(reslist->listlock);

    return count;
}

int reslist_invalidate(reslist_t *reslist,
                                                 void *resource)
{
    int ret;
    thread_mutex_lock(reslist->listlock);
    ret = reslist->destructor(resource, reslist->params);
    reslist->ntotal--;
    thread_cond_signal(reslist->avail);
    thread_mutex_unlock(reslist->listlock);
    return ret;
}

#endif  /* HAS_THREADS */
