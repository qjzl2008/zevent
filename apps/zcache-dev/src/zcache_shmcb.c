/* *  zcache_shmcb.c
 *  Session Cache via Shared Memory (Cyclic Buffer Variant)
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "zcache.h"

/*
 * Forward function prototypes.
 */

/* Functions for working around data-alignment-picky systems (sparcs,
   Irix, etc). These use "memcpy" as a way of foxing these systems into
   treating the composite types as byte-arrays rather than higher-level
   primitives that it prefers to have 4-(or 8-)byte aligned. I don't
   envisage this being a performance issue as a couple of 2 or 4 byte
   memcpys can hardly make a dent on the massive memmove operations this
   cache technique avoids, nor the overheads of ASN en/decoding. */
static unsigned int shmcb_get_safe_uint(unsigned int *);
static void shmcb_set_safe_uint_ex(unsigned char *, const unsigned char *);
#define shmcb_set_safe_uint(pdest, src) \
    do { \
        unsigned int tmp_uint = src; \
        shmcb_set_safe_uint_ex((unsigned char *)pdest, \
            (const unsigned char *)(&tmp_uint)); \
    } while(0)
#if 0 /* Unused so far */
static unsigned long shmcb_get_safe_ulong(unsigned long *);
static void shmcb_set_safe_ulong_ex(unsigned char *, const unsigned char *);
#define shmcb_set_safe_ulong(pdest, src) \
    do { \
        unsigned long tmp_ulong = src; \
        shmcb_set_safe_ulong_ex((unsigned char *)pdest, \
            (const unsigned char *)(&tmp_ulong)); \
    } while(0)
#endif
static time_t shmcb_get_safe_time(time_t *);
static void shmcb_set_safe_time_ex(unsigned char *, const unsigned char *);
#define shmcb_set_safe_time(pdest, src) \
    do { \
        time_t tmp_time = src; \
        shmcb_set_safe_time_ex((unsigned char *)pdest, \
            (const unsigned char *)(&tmp_time)); \
    } while(0)

/* This is used to persuade the compiler from using an inline memset()
 * which has no respect for alignment, since the size parameter is
 * often a compile-time constant.  GCC >= 4 will aggressively inline
 * static functions, so it's marked as explicitly not-inline. */
#if defined(__GNUC__) && __GNUC__ > 3
__attribute__((__noinline__))
#endif
static void shmcb_safe_clear(void *ptr, size_t size)
{
    memset(ptr, 0, size);
}

/* Underlying functions for session-caching */
static BOOL shmcb_init_memory( void *, unsigned int,unsigned int,unsigned int);
static BOOL shmcb_store( void *, UCHAR *, int, void *, int, time_t);
static void *shmcb_retrieve( void *, UCHAR *, int, int*);
static BOOL shmcb_remove( void *, UCHAR *, int);

/* Utility functions for manipulating the structures */
static void shmcb_get_header(void *, SHMCBHeader **);
static BOOL shmcb_get_division(SHMCBHeader *, SHMCBQueue *, SHMCBCache *, unsigned int);
static SHMCBIndex *shmcb_get_index(const SHMCBQueue *, unsigned int);
static unsigned int shmcb_expire_division( SHMCBQueue *, SHMCBCache *);
static BOOL shmcb_insert_internal( SHMCBQueue *, SHMCBCache *, unsigned char *, unsigned int, void *, unsigned int, time_t);
static void *shmcb_lookup_internal( SHMCBQueue *, SHMCBCache *, UCHAR *, unsigned int, int*);
static BOOL shmcb_remove_internal( SHMCBQueue *, SHMCBCache *, UCHAR *, unsigned int);

unsigned int zcache_shmcb_get_safe_uint(unsigned int *i)
{
	return shmcb_get_safe_uint(i);
}
time_t zcache_shmcb_get_safe_time(time_t *t){
	return shmcb_get_safe_time(t);
}
SHMCBIndex *zcache_shmcb_get_index(const SHMCBQueue *q, unsigned int i)
{
	return shmcb_get_index(q,i);
}
unsigned int zcache_shmcb_expire_division( SHMCBQueue *q, SHMCBCache *c)
{
	return shmcb_expire_division(q, c);
}

BOOL zcache_shmcb_get_division(SHMCBHeader *h, SHMCBQueue *q, SHMCBCache *c, unsigned int i)
{
	return shmcb_get_division(h, q, c,i);
}
/*
 * Data-alignment functions (a.k.a. avoidance tactics)
 *
 * NB: On HPUX (and possibly others) there is a *very* mischievous little
 * "optimisation" in the compilers where it will convert the following;
 *      memcpy(dest_ptr, &source, sizeof(unsigned int));
 * (where dest_ptr is of type (unsigned int *) and source is (unsigned int))
 * into;
 *      *dest_ptr = source; (or *dest_ptr = *(&source), not sure).
 * Either way, it completely destroys the whole point of these _safe_
 * functions, because the assignment operation will fall victim to the
 * architecture's byte-alignment dictations, whereas the memcpy (as a
 * byte-by-byte copy) should not. sigh. So, if you're wondering about the
 * apparently unnecessary conversions to (unsigned char *) in these
 * functions, you now have an explanation. Don't just revert them back and
 * say "ooh look, it still works" - if you try it on HPUX (well, 32-bit
 * HPUX 11.00 at least) you may find it fails with a SIGBUS. :-(
 */

static unsigned int shmcb_get_safe_uint(unsigned int *ptr)
{
    unsigned int ret;
    shmcb_set_safe_uint_ex((unsigned char *)(&ret),
            (const unsigned char *)ptr);
    return ret;
}

static void shmcb_set_safe_uint_ex(unsigned char *dest,
                const unsigned char *src)
{
    memcpy(dest, src, sizeof(unsigned int));
}

#if 0 /* Unused so far */
static unsigned long shmcb_get_safe_ulong(unsigned long *ptr)
{
    unsigned long ret;
    shmcb_set_safe_ulong_ex((unsigned char *)(&ret),
            (const unsigned char *)ptr);
    return ret;
}

static void shmcb_set_safe_ulong_ex(unsigned char *dest,
                const unsigned char *src)
{
    memcpy(dest, src, sizeof(unsigned long));
}
#endif

static time_t shmcb_get_safe_time(time_t * ptr)
{
    time_t ret;
    shmcb_set_safe_time_ex((unsigned char *)(&ret),
            (const unsigned char *)ptr);
    return ret;
}

static void shmcb_set_safe_time_ex(unsigned char *dest,
                const unsigned char *src)
{
    memcpy(dest, src, sizeof(time_t));
}
/*
**
** High-Level "handlers" as per storage.c
**
*/

void zcache_shmcb_init(MCConfigRecord *mc,apr_pool_t *p)
{
    void *shm_segment;
    apr_size_t shm_segsize;
    apr_status_t rv;

    /*
     * Create shared memory segment
     */
    if (mc->szStorageDataFile == NULL) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "LUASessionCache required");*/
        zcache_die();
    }
/*
//     Use anonymous shm by default, fall back on name-based. 
    rv = apr_shm_create(&(mc->pStorageDataMM), 
                        mc->nStorageDataSize, 
                        NULL, mc->pPool);
*/
 //   if (APR_STATUS_IS_ENOTIMPL(rv)) {
        /* For a name-based segment, remove it first in case of a
         * previous unclean shutdown. */
        apr_shm_remove(mc->szStorageDataFile, mc->pPool);
        rv = apr_shm_create(&(mc->pStorageDataMM), 
                            mc->nStorageDataSize, 
                            mc->szStorageDataFile,
                            mc->pPool);
   // }

    if (rv != APR_SUCCESS) {
        //char buf[100];
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "Cannot allocate shared memory: (%d)%s", rv,
                     apr_strerror(rv, buf, sizeof(buf)));*/
        zcache_die();
    }
    shm_segment = apr_shm_baseaddr_get(mc->pStorageDataMM);
    shm_segsize = apr_shm_size_get(mc->pStorageDataMM);

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "shmcb_init allocated %" APR_SIZE_T_FMT 
                 " bytes of shared memory",
                 shm_segsize);*/
    if (!shmcb_init_memory(shm_segment, shm_segsize,
			    mc->idx_nums_perdivision,mc->division_nums)) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "Failure initialising 'shmcb' shared memory");*/
        zcache_die();
    }
    /*ap_log_error(APLOG_MARK, APLOG_INFO, 0, s,
                 "Shared memory session cache initialised");*/

   
    mc->tStorageDataTable = shm_segment;
    return;
}

void zcache_shmcb_attach(MCConfigRecord *mc,apr_pool_t *p)
{
    void *shm_segment;
    apr_size_t shm_segsize;
    apr_status_t rv;

    /*
     * Create shared memory segment
     */
    if (mc->szStorageDataFile == NULL) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "LUASessionCache required");*/
        zcache_die();
    }
    rv = apr_shm_attach(&(mc->pStorageDataMM),mc->szStorageDataFile,
		    mc->pPool);
    
    if (rv != APR_SUCCESS) {
        //char buf[100];
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "Cannot allocate shared memory: (%d)%s", rv,
                     apr_strerror(rv, buf, sizeof(buf)));*/
        zcache_die();
    }
    shm_segment = apr_shm_baseaddr_get(mc->pStorageDataMM);
    shm_segsize = apr_shm_size_get(mc->pStorageDataMM);

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "shmcb_init allocated %" APR_SIZE_T_FMT 
                 " bytes of shared memory",
                 shm_segsize);*/
 //   if (!shmcb_init_memory(shm_segment, shm_segsize)) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "Failure initialising 'shmcb' shared memory");*/
   //     zcache_die();
   // }
    /*ap_log_error(APLOG_MARK, APLOG_INFO, 0, s,
                 "Shared memory session cache initialised");*/

    mc->tStorageDataTable = shm_segment;
    return;
}

void zcache_shmcb_kill(MCConfigRecord *mc)
{
    if (mc->pStorageDataMM != NULL) {
        apr_shm_destroy(mc->pStorageDataMM);
        mc->pStorageDataMM = NULL;
    }
    return;
}

void zcache_shmcb_get_header(void *shm_mem, SHMCBHeader **header)
{
	shmcb_get_header(shm_mem, header);
}

BOOL zcache_shmcb_store(MCConfigRecord *mc,UCHAR *id, int idlen,
                           time_t timeout, void * pdata, int nlen)
{
    void *shm_segment;
    BOOL to_return = FALSE;

    /* We've kludged our pointer into the other cache's member variable. */
    shm_segment = (void *) mc->tStorageDataTable;
    zcache_mutex_on(mc);
    if (!shmcb_store(shm_segment, id, idlen, pdata, nlen, timeout))
        /* in this cache engine, "stores" should never fail. */
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "'shmcb' code was unable to store a "
                     "session in the cache.");*/
    {
	    to_return = FALSE;
    }
    else {
       /* ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "shmcb_store successful");*/
        to_return = TRUE;
    }
    zcache_mutex_off(mc);
    return to_return;
}

void *zcache_shmcb_retrieve(MCConfigRecord *mc,UCHAR *id, int idlen, int*nlen)
{
    void *shm_segment;
    void *pdata;

    /* We've kludged our pointer into the other cache's member variable. */
    shm_segment = (void *) mc->tStorageDataTable;
    zcache_mutex_on(mc);
    pdata = shmcb_retrieve(shm_segment, id, idlen, nlen);
    zcache_mutex_off(mc);
    if (pdata)
        /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "shmcb_retrieve had a hit");*/
	    ;
    else {
        /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "shmcb_retrieve had a miss");
        ap_log_error(APLOG_MARK, APLOG_INFO, 0, s,
                     "Client requested a 'session-resume' but "
                     "we have no such session.");*/
    }
    return pdata;
}

void zcache_shmcb_remove(MCConfigRecord *mc,UCHAR *id, int idlen)
{
    void *shm_segment;

    /* We've kludged our pointer into the other cache's member variable. */
    shm_segment = (void *) mc->tStorageDataTable;
    zcache_mutex_on(mc);
    shmcb_remove(shm_segment, id, idlen);
    zcache_mutex_off(mc);
}

void zcache_shmcb_expire()
{
    /* NOP */
    return;
}

void zcache_shmcb_status(MCConfigRecord *mc,apr_pool_t *p,
                            void (*func) (char *, void *), void *arg)
{
    SHMCBHeader *header;
    SHMCBQueue queue;
    SHMCBCache cache;
    SHMCBIndex *idx;
    void *shm_segment;
    unsigned int loop, total, cache_total, non_empty_divisions;
    int index_pct, cache_pct;
    double expiry_total;
    time_t average_expiry, now, max_expiry, min_expiry, idxexpiry;

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, 
                 "inside zcache_shmcb_status");*/

    /* We've kludged our pointer into the other cache's member variable. */
    shm_segment = (void *) mc->tStorageDataTable;

    /* Get the header structure. */
    shmcb_get_header(shm_segment, &header);
    total = cache_total = non_empty_divisions = 0;
    average_expiry = max_expiry = min_expiry = 0;
    expiry_total = 0;

    /* It may seem strange to grab "now" at this point, but in theory
     * we should never have a negative threshold but grabbing "now" after
     * the loop (which performs expiries) could allow that chance. */
    now = time(NULL);
    for (loop = 0; loop <= header->division_mask; loop++) {
        if (shmcb_get_division(header, &queue, &cache, loop)) {
            shmcb_expire_division(&queue, &cache);
            total += shmcb_get_safe_uint(queue.pos_count);
            cache_total += shmcb_get_safe_uint(cache.pos_count);
            if (shmcb_get_safe_uint(queue.pos_count) > 0) {
                idx = shmcb_get_index(&queue,
                                     shmcb_get_safe_uint(queue.first_pos));
                non_empty_divisions++;
                idxexpiry = shmcb_get_safe_time(&(idx->expires));
                expiry_total += (double) idxexpiry;
                max_expiry = (idxexpiry > max_expiry ? idxexpiry :
                              max_expiry);
                if (min_expiry == 0)
                    min_expiry = idxexpiry;
                else
                    min_expiry = (idxexpiry < min_expiry ? idxexpiry :
                                  min_expiry);
            }
        }
    }
    index_pct = (100 * total) / (header->index_num * (header->division_mask + 1));
    cache_pct = (100 * cache_total) / (header->cache_data_size * (header->division_mask + 1));
    func(apr_psprintf(p, "cache type: <b>SHMCB</b>, shared memory: <b>%d</b> "
                     "bytes, current sessions: <b>%d</b><br>",
                     mc->nStorageDataSize, total), arg);
    func(apr_psprintf(p, "sub-caches: <b>%d</b>, indexes per sub-cache: "
                     "<b>%d</b><br>", (int) header->division_mask + 1,
                     (int) header->index_num), arg);
    if (non_empty_divisions != 0) {
        average_expiry = (time_t)(expiry_total / (double)non_empty_divisions);
        func(apr_psprintf(p, "time left on oldest entries' SSL sessions: "), arg);
        if (now < average_expiry)
            func(apr_psprintf(p, "avg: <b>%d</b> seconds, (range: %d...%d)<br>",
                            (int)(average_expiry - now), (int) (min_expiry - now),
                            (int)(max_expiry - now)), arg);
        else
            func(apr_psprintf(p, "expiry threshold: <b>Calculation Error!</b>" 
                             "<br>"), arg);

    }
    func(apr_psprintf(p, "index usage: <b>%d%%</b>, cache usage: <b>%d%%</b>"
                     "<br>", index_pct, cache_pct), arg);
    func(apr_psprintf(p, "total sessions stored since starting: <b>%lu</b><br>",
                     header->num_stores), arg);
    func(apr_psprintf(p,"total sessions expired since starting: <b>%lu</b><br>",
                     header->num_expiries), arg);
    func(apr_psprintf(p, "total (pre-expiry) sessions scrolled out of the "
                     "cache: <b>%lu</b><br>", header->num_scrolled), arg);
    func(apr_psprintf(p, "total retrieves since starting: <b>%lu</b> hit, "
                     "<b>%lu</b> miss<br>", header->num_retrieves_hit,
                     header->num_retrieves_miss), arg);
    func(apr_psprintf(p, "total removes since starting: <b>%lu</b> hit, "
                     "<b>%lu</b> miss<br>", header->num_removes_hit,
                     header->num_removes_miss), arg);
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, 
                 "leaving shmcb_status");*/
    return;
}

/*
**
** Memory manipulation and low-level cache operations 
**
*/

static BOOL shmcb_init_memory(
     void *shm_mem,
    unsigned int shm_mem_size,
    unsigned int index_nums,
    unsigned int granularity)
{
    SHMCBHeader *header;
    SHMCBQueue queue;
    SHMCBCache cache;
    unsigned int temp,loop;

    /* OK, we're sorted - from here on in, the return should be TRUE */
    header = (SHMCBHeader *)shm_mem;
    header->division_mask = (unsigned char)(granularity - 1);
    header->division_offset = sizeof(SHMCBHeader);
    header->index_num = index_nums;
    header->index_offset = (2 * sizeof(unsigned int));
    header->index_size = sizeof(SHMCBIndex);
    header->queue_size = header->index_offset +
                         (header->index_num * header->index_size);

    /* Now calculate the space for each division */
    temp = shm_mem_size - header->division_offset;
    header->division_size = temp / granularity;

    /* Calculate the space left in each division for the cache */
    temp -= header->queue_size;
    header->cache_data_offset = (2 * sizeof(unsigned int));
    //add by zhousihai
    int ncache_data_size = header->division_size -
                              header->queue_size - header->cache_data_offset;

    if(ncache_data_size <= 0)
    {
	    return FALSE;
    }
    //end

    header->cache_data_size = header->division_size -
                              header->queue_size - header->cache_data_offset;

    printf("shmcb_init_memory choices follow\n");
    printf("division_mask = 0x%02x\n",header->division_mask);
    printf("division_offset = %u\n",header->division_offset);
    printf("division_size = %u\n",header->division_size);
    printf("queue_size = %u\n",header->queue_size);
    printf("index_num = %u\n",header->index_num);
    printf("index_offset = %u\n",header->index_offset);
    printf("index_size = %u\n",header->index_size);
    printf("cache_data_offset = %u\n",header->cache_data_offset);
    printf("cache_data_size = %u\n",header->cache_data_size);

    /* The header is done, make the caches empty */
    for (loop = 0; loop < granularity; loop++) {
        if (!shmcb_get_division(header, &queue, &cache, loop))
            /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "shmcb_init_memory, " "internal error");*/
        shmcb_set_safe_uint(cache.first_pos, 0);
        shmcb_set_safe_uint(cache.pos_count, 0);
        shmcb_set_safe_uint(queue.first_pos, 0);
        shmcb_set_safe_uint(queue.pos_count, 0);
    }

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "leaving shmcb_init_memory()");*/
    return TRUE;
}

static BOOL shmcb_store(
     void *shm_segment, UCHAR *id,
    int idlen, void * pdata, int nlen,
    time_t timeout)
{
    SHMCBHeader *header;
    SHMCBQueue queue;
    SHMCBCache cache;
    unsigned int masked_index;
    time_t expiry_time;
    unsigned int key = hash(id,idlen,0);

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "inside shmcb_store");*/

    /* Get the header structure, which division this session will fall into etc. */
    shmcb_get_header(shm_segment, &header);
    masked_index = key & header->division_mask;
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "key=%u, masked index=%u",
                 key, masked_index);*/
    if (!shmcb_get_division(header, &queue, &cache, masked_index)) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "shmcb_store internal error");*/
        return FALSE;
    }

    expiry_time = timeout;
    if (!shmcb_insert_internal(&queue, &cache,id,idlen, pdata, nlen, expiry_time)) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "can't store a session!");*/
        return FALSE;
    }
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "leaving shmcb_store successfully");*/
    header->num_stores++;
    return TRUE;
}

static void *shmcb_retrieve(
     void *shm_segment,
    UCHAR *id, int idlen, int*nlen)
{
    SHMCBHeader *header;
    SHMCBQueue queue;
    SHMCBCache cache;
    void* pdata;
    unsigned int masked_index;
    unsigned int key = hash(id,idlen,0);

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "inside shmcb_retrieve_session");*/

    /* Get the header structure, which division this session lookup
     * will come from etc. */
    shmcb_get_header(shm_segment, &header);
    masked_index = key & header->division_mask;
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "key=%u, masked index=%u", key, masked_index);*/
    if (!shmcb_get_division(header, &queue, &cache,  masked_index)) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "shmcb_retrieve_session internal error");*/
        header->num_retrieves_miss++;
        return FALSE;
    }

    /* Get the session corresponding to the session_id or NULL if it
     * doesn't exist (or is flagged as "removed"). */
    pdata = shmcb_lookup_internal(&queue, &cache, id, idlen, nlen);
    if (pdata)
        header->num_retrieves_hit++;
    else
        header->num_retrieves_miss++;
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "leaving shmcb_retrieve_session");*/
    return pdata;
}

static BOOL shmcb_remove(
     void *shm_segment,
    UCHAR *id, int idlen)
{
    SHMCBHeader *header;
    SHMCBQueue queue;
    SHMCBCache cache;
    unsigned int masked_index;
    BOOL res;
    unsigned int key = hash(id,idlen,0);

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "inside shmcb_remove_session");*/
    if (id == NULL) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "remove called with NULL session_id!");*/
        return FALSE;
    }

    /* Get the header structure, which division this session remove
     * will happen in etc. */
    shmcb_get_header(shm_segment, &header);
    masked_index = key & header->division_mask;
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "key=%u, masked index=%u", key, masked_index);*/
    if (!shmcb_get_division(header, &queue, &cache, masked_index)) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "shmcb_remove_session, internal error");*/
        header->num_removes_miss++;
        return FALSE;
    }
    res = shmcb_remove_internal(&queue, &cache, id, idlen);
    if (res)
        header->num_removes_hit++;
    else
        header->num_removes_miss++;
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "leaving shmcb_remove_session");*/
    return res;
}


/* 
**
** Weirdo cyclic buffer functions
**
*/

/* This gets used in the cyclic "index array" (in the 'Queue's) and
 * in the cyclic 'Cache's too ... you provide the "width" of the
 * cyclic store, the starting position and how far to move (with
 * wrapping if necessary). Basically it's addition modulo buf_size. */
static unsigned int shmcb_cyclic_increment(
    unsigned int buf_size,
    unsigned int start_pos,
    unsigned int to_add)
{
    start_pos += to_add;
    while (start_pos >= buf_size)
        start_pos -= buf_size;
    return start_pos;
}

/* Given two positions in a cyclic buffer, calculate the "distance".
 * This is to cover the case ("non-trivial") where the 'next' offset
 * is to the left of the 'start' offset. NB: This calculates the
 * space inclusive of one end-point but not the other. There is an
 * ambiguous case (which is why we use the <start_pos,offset>
 * coordinate system rather than <start_pos,end_pos> one) when 'start'
 * is the same as 'next'. It could indicate the buffer is full or it
 * can indicate the buffer is empty ... I choose the latter as it's
 * easier and usually necessary to check if the buffer is full anyway
 * before doing incremental logic (which is this useful for), but we
 * definitely need the empty case handled - in fact it's our starting
 * state!! */
static unsigned int shmcb_cyclic_space(
    unsigned int buf_size,
    unsigned int start_offset,
    unsigned int next_offset)
{
    /* Is it the trivial case? */
    if (start_offset <= next_offset)
        return (next_offset - start_offset);              /* yes */
    else
        return ((buf_size - start_offset) + next_offset); /* no */
}

/* A "normal-to-cyclic" memcpy ... this takes a linear block of
 * memory and copies it onto a cyclic buffer. The purpose and
 * function of this is pretty obvious, you need to cover the case
 * that the destination (cyclic) buffer has to wrap round. */
static void shmcb_cyclic_ntoc_memcpy(
    unsigned int buf_size,
    unsigned char *data,
    unsigned int dest_offset,
    unsigned char *src, unsigned int src_len)
{
    /* Cover the case that src_len > buf_size */
    if (src_len > buf_size)
        src_len = buf_size;

    /* Can it be copied all in one go? */
    if (dest_offset + src_len < buf_size)
        /* yes */
        memcpy(data + dest_offset, src, src_len);
    else {
        /* no */
        memcpy(data + dest_offset, src, buf_size - dest_offset);
        memcpy(data, src + buf_size - dest_offset,
               src_len + dest_offset - buf_size);
    }
    return;
}

/* A "cyclic-to-normal" memcpy ... given the last function, this
 * one's purpose is clear, it copies out of a cyclic buffer handling
 * wrapping. */
static void shmcb_cyclic_cton_memcpy(
    unsigned int buf_size,
    unsigned char *dest,
    unsigned char *data,
    unsigned int src_offset,
    unsigned int src_len)
{
    /* Cover the case that src_len > buf_size */
    if (src_len > buf_size)
        src_len = buf_size;

    /* Can it be copied all in one go? */
    if (src_offset + src_len < buf_size)
        /* yes */
        memcpy(dest, data + src_offset, src_len);
    else {
        /* no */
        memcpy(dest, data + src_offset, buf_size - src_offset);
        memcpy(dest + buf_size - src_offset, data,
               src_len + src_offset - buf_size);
    }
    return;
}

/* Here's the cool hack that makes it all work ... by simply
 * making the first collection of bytes *be* our header structure
 * (casting it into the C structure), we have the perfect way to
 * maintain state in a shared-memory session cache from one call
 * (and process) to the next, use the shared memory itself! The
 * original mod_ssl shared-memory session cache uses variables
 * inside the context, but we simply use that for storing the
 * pointer to the shared memory itself. And don't forget, after
 * Apache's initialisation, this "header" is constant/read-only
 * so we can read it outside any locking.
 * <grin> - sometimes I just *love* coding y'know?!  */
static void shmcb_get_header(void *shm_mem, SHMCBHeader **header)
{
    *header = (SHMCBHeader *)shm_mem;
    return;
}

/* This is what populates our "interesting" structures. Given a
 * pointer to the header, and an index into the appropriate
 * division (this must have already been masked using the
 * division_mask by the caller!), we can populate the provided
 * SHMCBQueue and SHMCBCache structures with values and
 * pointers to the underlying shared memory. Upon returning
 * (if not FALSE), the caller can meddle with the pointer
 * values and they will map into the shared-memory directly,
 * as such there's no need to "free" or "set" the Queue or
 * Cache values, they were themselves references to the *real*
 * data. */
static BOOL shmcb_get_division(
    SHMCBHeader *header, SHMCBQueue *queue,
    SHMCBCache *cache, unsigned int idx)
{
    unsigned char *pQueue;
    unsigned char *pCache;

    /* bounds check */
    if (idx > header->division_mask)
        return FALSE;

    /* Locate the blocks of memory storing the corresponding data */
    pQueue = ((unsigned char *) header) + header->division_offset +
        (idx * header->division_size);
    pCache = pQueue + header->queue_size;

    /* Populate the structures with appropriate pointers */
    queue->first_pos = (unsigned int *) pQueue;

    /* Our structures stay packed, no matter what the system's
     * data-alignment regime is. */
    queue->pos_count = (unsigned int *) (pQueue + sizeof(unsigned int));
    queue->indexes = (SHMCBIndex *) (pQueue + (2 * sizeof(unsigned int)));
    cache->first_pos = (unsigned int *) pCache;
    cache->pos_count = (unsigned int *) (pCache + sizeof(unsigned int));
    cache->data = (unsigned char *) (pCache + (2 * sizeof(unsigned int)));
    queue->header = cache->header = header;

    return TRUE;
}

/* This returns a pointer to the piece of shared memory containing
 * a specified 'Index'. SHMCBIndex, like SHMCBHeader, is a fixed
 * width non-referencing structure of primitive types that can be
 * cast onto the corresponding block of shared memory. Thus, by
 * returning a cast pointer to that section of shared memory, the
 * caller can read and write values to and from the "structure" and
 * they are actually reading and writing the underlying shared
 * memory. */
static SHMCBIndex *shmcb_get_index(
    const SHMCBQueue *queue, unsigned int idx)
{
    /* bounds check */
    if (idx > queue->header->index_num)
        return NULL;

    /* Return a pointer to the index. NB: I am being horribly pendantic
     * here so as to avoid any potential data-alignment assumptions being
     * placed on the pointer arithmetic by the compiler (sigh). */
    return (SHMCBIndex *)(((unsigned char *) queue->indexes) +
                          (idx * sizeof(SHMCBIndex)));
}

/* This functions rolls expired cache (and index) entries off the front
 * of the cyclic buffers in a division. The function returns the number
 * of expired sessions. */
static unsigned int shmcb_expire_division(
     SHMCBQueue *queue, SHMCBCache *cache)
{
    SHMCBIndex *idx;
    time_t now;
    unsigned int loop, index_num, pos_count, new_pos;
    SHMCBHeader *header;

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "entering shmcb_expire_division");*/

    /* We must calculate num and space ourselves based on expiry times. */
    now = time(NULL);
    loop = 0;
    new_pos = shmcb_get_safe_uint(queue->first_pos);

    /* Cache useful values */
    header = queue->header;
    index_num = header->index_num;
    pos_count = shmcb_get_safe_uint(queue->pos_count);
    while (loop < pos_count) {
        idx = shmcb_get_index(queue, new_pos);
        if (shmcb_get_safe_time(&(idx->expires)) > now)
            /* it hasn't expired yet, we're done iterating */
            break;
        /* This one should be expired too. Shift to the next entry. */
        loop++;
        new_pos = shmcb_cyclic_increment(index_num, new_pos, 1);
    }

    /* Find the new_offset and make the expiries happen. */
    if (loop > 0) {
        /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "will be expiring %u sessions", loop);*/
        /* We calculate the new_offset by "peeking" (or in the
         * case it's the last entry, "sneaking" ;-). */
        if (loop == pos_count) {
            /* We are expiring everything! This is easy to do... */
            shmcb_set_safe_uint(queue->pos_count, 0);
            shmcb_set_safe_uint(cache->pos_count, 0);
        }
        else {
            /* The Queue is easy to adjust */
            shmcb_set_safe_uint(queue->pos_count,
                               shmcb_get_safe_uint(queue->pos_count) - loop);
            shmcb_set_safe_uint(queue->first_pos, new_pos);
            /* peek to the start of the next session */
            idx = shmcb_get_index(queue, new_pos);
            /* We can use shmcb_cyclic_space because we've guaranteed 
             * we don't fit the ambiguous full/empty case. */
            shmcb_set_safe_uint(cache->pos_count,
                               shmcb_get_safe_uint(cache->pos_count) -
                               shmcb_cyclic_space(header->cache_data_size,
                                                  shmcb_get_safe_uint(cache->first_pos),
                                                  shmcb_get_safe_uint(&(idx->offset))));
            shmcb_set_safe_uint(cache->first_pos, shmcb_get_safe_uint(&(idx->offset)));
        }
        /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "we now have %u sessions",
                     shmcb_get_safe_uint(queue->pos_count));*/
    }
    header->num_expiries += loop;
    return loop;
}

/* Inserts a new encoded session into a queue/cache pair - expiring
 * (early or otherwise) any leading sessions as necessary to ensure
 * there is room. An error return (FALSE) should only happen in the
 * event of surreal values being passed on, or ridiculously small
 * cache sizes. NB: For tracing purposes, this function is also given
 * the server_rec to allow "ssl_log()". */
static BOOL shmcb_insert_internal(
     SHMCBQueue * queue,
    SHMCBCache * cache,
    unsigned char *id,
    unsigned int idlen,
    void *pdata,
    unsigned int nlen,
    time_t expiry_time)
{
    SHMCBHeader *header;
    SHMCBIndex *idx = NULL;
    unsigned int gap, new_pos, loop, new_offset;
    int need;
    void *pvalue = NULL;

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "entering shmcb_insert_internal, "
                 "*queue->pos_count = %u",
                 shmcb_get_safe_uint(queue->pos_count));*/

    /* If there's entries to expire, ditch them first thing. */
    shmcb_expire_division(queue, cache);
    //modify by zhousihai
    int valuelen = 0;
    pvalue = shmcb_lookup_internal(queue, cache, id, idlen, &valuelen);
    if (pvalue)
    {
	    shmcb_remove_internal(queue, cache, id, idlen);
    }
    //end modify

    header = cache->header;
    gap = header->cache_data_size - shmcb_get_safe_uint(cache->pos_count);
    if (gap < nlen) {
	    new_pos = shmcb_get_safe_uint(queue->first_pos);
	    loop = 0;
	    need = (int) nlen - (int) gap;
	    while ((need > 0) && (loop + 1 < shmcb_get_safe_uint(queue->pos_count))) {
		    new_pos = shmcb_cyclic_increment(header->index_num, new_pos, 1);
		    loop += 1;
		    idx = shmcb_get_index(queue, new_pos);
		    //modify by zhousihai.keep old key-value
		    if(idx->length > 0 && !idx->removed)
		    {
			    return FALSE;
		    }
		    need = (int) nlen - (int) gap -
			    shmcb_cyclic_space(header->cache_data_size,
					    shmcb_get_safe_uint(cache->first_pos),
					    shmcb_get_safe_uint(&(idx->offset)));
	    }
	    if (loop > 0) {
		    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
		      "about to scroll %u sessions from %u",
		      loop, shmcb_get_safe_uint(queue->pos_count));*/
		    /* We are removing "loop" items from the cache. */
		    shmcb_set_safe_uint(cache->pos_count,
				    shmcb_get_safe_uint(cache->pos_count) -
				    shmcb_cyclic_space(header->cache_data_size,
					    shmcb_get_safe_uint(cache->first_pos),
					    shmcb_get_safe_uint(&(idx->offset))));
		    shmcb_set_safe_uint(cache->first_pos, shmcb_get_safe_uint(&(idx->offset)));
		    shmcb_set_safe_uint(queue->pos_count, shmcb_get_safe_uint(queue->pos_count) - loop);
		    shmcb_set_safe_uint(queue->first_pos, new_pos);
		    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
		      "now only have %u sessions",
		      shmcb_get_safe_uint(queue->pos_count));*/
		    /* Update the stats!!! */
		    header->num_scrolled += loop;
	    }
    }

    /* probably unecessary checks, but I'll leave them until this code
     * is verified. */
    if (shmcb_get_safe_uint(cache->pos_count) + nlen >
        header->cache_data_size) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "shmcb_insert_internal internal error");*/
        return FALSE;
    }
    if (shmcb_get_safe_uint(queue->pos_count) == header->index_num) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "shmcb_insert_internal internal error");*/
        return FALSE;
    }
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "we have %u bytes and %u indexes free - enough",
                 header->cache_data_size -
                 shmcb_get_safe_uint(cache->pos_count), header->index_num -
                 shmcb_get_safe_uint(queue->pos_count));*/


    /* HERE WE ASSUME THAT THE NEW SESSION SHOULD GO ON THE END! I'M NOT
     * CHECKING WHETHER IT SHOULD BE GENUINELY "INSERTED" SOMEWHERE.
     *
     * We either fix that, or find out at a "higher" (read "mod_ssl")
     * level whether it is possible to have distinct session caches for
     * any attempted tomfoolery to do with different session timeouts.
     * Knowing in advance that we can have a cache-wide constant timeout
     * would make this stuff *MUCH* more efficient. Mind you, it's very
     * efficient right now because I'm ignoring this problem!!!
     */

    /* Increment to the first unused byte */
    new_offset = shmcb_cyclic_increment(header->cache_data_size,
                                        shmcb_get_safe_uint(cache->first_pos),
                                        shmcb_get_safe_uint(cache->pos_count));
    /* Copy the DER-encoded session into place */
    shmcb_cyclic_ntoc_memcpy(header->cache_data_size, cache->data,
                            new_offset, pdata, nlen);
    /* Get the new index that this session is stored in. */
    new_pos = shmcb_cyclic_increment(header->index_num,
                                     shmcb_get_safe_uint(queue->first_pos),
                                     shmcb_get_safe_uint(queue->pos_count));
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "storing in index %u, at offset %u",
                 new_pos, new_offset);*/
    idx = shmcb_get_index(queue, new_pos);
    if (idx == NULL) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                     "shmcb_insert_internal internal error");*/
        return FALSE;
    }
    shmcb_safe_clear(idx, sizeof(SHMCBIndex));
    shmcb_set_safe_time(&(idx->expires), expiry_time);
    shmcb_set_safe_uint(&(idx->offset), new_offset);
	shmcb_set_safe_uint(&(idx->length), nlen);

    /* idx->removed = (unsigned char)0; */ /* Not needed given the memset above. */
    idx->key = hash(id,idlen,0);

    /* All that remains is to adjust the cache's and queue's "pos_count"s. */
    shmcb_set_safe_uint(cache->pos_count,
                       shmcb_get_safe_uint(cache->pos_count) + nlen);
    shmcb_set_safe_uint(queue->pos_count,
                       shmcb_get_safe_uint(queue->pos_count) + 1);

    /* And just for good debugging measure ... */
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "leaving now with %u bytes in the cache and %u indexes",
                 shmcb_get_safe_uint(cache->pos_count),
                 shmcb_get_safe_uint(queue->pos_count));
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "leaving shmcb_insert_internal");*/
    return TRUE;
}

/* Performs a lookup into a queue/cache pair for a
 * session_id. If found, the session is deserialised
 * and returned, otherwise NULL. */
static void *shmcb_lookup_internal(
     SHMCBQueue *queue,
    SHMCBCache *cache, UCHAR *id,
    unsigned int idlen, int *len)
{
    SHMCBIndex *idx;
    SHMCBHeader *header;
    unsigned int curr_pos, loop, count;
	unsigned int key;
    time_t now;
    void* pdata;

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "entering shmcb_lookup_internal");*/

    /* If there are entries to expire, ditch them first thing. */
    shmcb_expire_division(queue, cache);
    now = time(NULL);
    curr_pos = shmcb_get_safe_uint(queue->first_pos);
    count = shmcb_get_safe_uint(queue->pos_count);
    header = queue->header;
    key = hash(id, idlen, 0);
    for (loop = 0; loop < count; loop++) 
    {
        /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "loop=%u, count=%u, curr_pos=%u",
                     loop, count, curr_pos);*/
        idx = shmcb_get_index(queue, curr_pos);
        /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "idx->key=%u, key=%u, offset=%u",idx->key,key, shmcb_get_safe_uint(&(idx->offset)));*/
        /* Only look into the session further if;
         * (a) the second byte of the session_id matches,
         * (b) the "removed" flag isn't set,
         * (c) the session hasn't expired yet.
         * We do (c) like this so that it saves us having to
         * do natural expiries ... naturally expired sessions
         * scroll off the front anyway when the cache is full and
         * "rotating", the only real issue that remains is the
         * removal or disabling of forcibly killed sessions. */
        if ((idx->key == key) && !idx->removed && (shmcb_get_safe_time(&(idx->expires)) > now)) 
        {
            /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                         "at index %u, found possible session match",
                         curr_pos);*/
            
            pdata = malloc(idx->length);
            *len = idx->length;
            if (pdata == NULL) {
                /*ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                    "scach2_lookup_session_id internal error");*/
                return NULL;
            }
            shmcb_cyclic_cton_memcpy(header->cache_data_size,
                                     pdata, cache->data,
                                     shmcb_get_safe_uint(&(idx->offset)),
                                     idx->length);
            /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                "a match!");*/
            return pdata;
        }
        curr_pos = shmcb_cyclic_increment(header->index_num, curr_pos, 1);
    }
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "no matching sessions were found");*/
    return NULL;
}

static BOOL shmcb_remove_internal(
     SHMCBQueue *queue,
    SHMCBCache *cache, UCHAR *id, unsigned int idlen)
{
    SHMCBIndex *idx;
    SHMCBHeader *header;
    unsigned int curr_pos, loop, count;
	unsigned int key;
    BOOL to_return = FALSE;

    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "entering shmcb_remove_internal");*/

    /* If there's entries to expire, ditch them first thing. */
    /* shmcb_expire_division(queue, cache); */

    /* Regarding the above ... hmmm ... I know my expiry code is slightly
     * "faster" than all this remove stuff ... but if the higher level
     * code calls a "remove" operation (and this *only* seems to happen
     * when it has spotted an expired session before we had a chance to)
     * then it should get credit for a remove (stats-wise). Also, in the
     * off-chance that the server *requests* a renegotiate and wants to
     * wipe the session clean we should give that priority over our own
     * routine expiry handling. So I've moved the expiry check to *after*
     * this general remove stuff. */
    curr_pos = shmcb_get_safe_uint(queue->first_pos);
    count = shmcb_get_safe_uint(queue->pos_count);
    header = cache->header;

    key = hash(id,idlen, 0);
    
    for (loop = 0; loop < count; loop++) {
        /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "loop=%u, count=%u, curr_pos=%u",
                loop, count, curr_pos);*/
        idx = shmcb_get_index(queue, curr_pos);
        /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "idx->key=%u, key=%u", idx->key, key);*/
        /* Only look into the session further if the second byte of the
         * session_id matches. */
        if (idx->key == key) {
            /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                         "at index %u, found possible "
                         "match", curr_pos);*/
            idx->removed = (unsigned char) 1;
            to_return = TRUE;
            goto end;
        }
        curr_pos = shmcb_cyclic_increment(header->index_num, curr_pos, 1);
    }
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "no matching sessions were found");*/

    /* If there's entries to expire, ditch them now. */
    shmcb_expire_division(queue, cache);
end:
    /*ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "leaving shmcb_remove_internal");*/
    return to_return;
}
