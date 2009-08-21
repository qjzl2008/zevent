#include <stdlib.h>
#include "apr.h"
#include "apr_strings.h"
#include "apr_portable.h"
#include "apr_lib.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "zevent_config.h"
#include "zevent.h"

#include "scoreboard.h"

ZEVENT_DECLARE_DATA scoreboard *zevent_scoreboard_image = NULL;
ZEVENT_DECLARE_DATA const char *zevent_scoreboard_fname = NULL;
ZEVENT_DECLARE_DATA int zevent_extended_status = 0;
ZEVENT_DECLARE_DATA int zevent_mod_status_reqtail = 0;

#if APR_HAS_SHARED_MEMORY

#include "apr_shm.h"

#ifndef WIN32
static /* but must be exported to mpm_winnt */
#endif
        apr_shm_t *zevent_scoreboard_shm = NULL;

#endif

struct zevent_sb_handle_t {
    int child_num;
    int thread_num;
};

static int server_limit, thread_limit;
static apr_size_t scoreboard_size;

/*
 * ToDo:
 * This function should be renamed to cleanup_shared
 * and it should handle cleaning up a scoreboard shared
 * between processes using any form of IPC (file, shared memory
 * segment, etc.). Leave it as is now because it is being used
 * by various MPMs.
 */
static apr_status_t zevent_cleanup_shared_mem(void *d)
{
#if APR_HAS_SHARED_MEMORY
    free(zevent_scoreboard_image);
    zevent_scoreboard_image = NULL;
    apr_shm_destroy(zevent_scoreboard_shm);
#endif
    return APR_SUCCESS;
}
ZEVENT_DECLARE(int) zevent_calc_scoreboard_size(void)
{
	zevent_mpm_query(ZEVENT_MPMQ_HARD_LIMIT_THREADS, &thread_limit);
	zevent_mpm_query(ZEVENT_MPMQ_HARD_LIMIT_DAEMONS, &server_limit);

	scoreboard_size = sizeof(global_score);
	scoreboard_size += sizeof(process_score) * server_limit;
	scoreboard_size += sizeof(worker_score) * server_limit * thread_limit;

	return scoreboard_size;
}
void zevent_init_scoreboard(void *shared_score)
{
    char *more_storage;
    int i;

    zevent_calc_scoreboard_size();
    zevent_scoreboard_image =
        calloc(1, sizeof(scoreboard) + server_limit * sizeof(worker_score *));
    more_storage = shared_score;
    zevent_scoreboard_image->global = (global_score *)more_storage;
    more_storage += sizeof(global_score);
    zevent_scoreboard_image->parent = (process_score *)more_storage;
    more_storage += sizeof(process_score) * server_limit;
    zevent_scoreboard_image->servers =
        (worker_score **)((char*)zevent_scoreboard_image + sizeof(scoreboard));
    for (i = 0; i < server_limit; i++) {
        zevent_scoreboard_image->servers[i] = (worker_score *)more_storage;
        more_storage += thread_limit * sizeof(worker_score);
    }
       ///zevent_assert(more_storage == (char*)shared_score + scoreboard_size);
    zevent_scoreboard_image->global->server_limit = server_limit;
    zevent_scoreboard_image->global->thread_limit = thread_limit;
}

/**
 * Create a name-based scoreboard in the given pool using the
 * given filename.
 */
static apr_status_t create_namebased_scoreboard(apr_pool_t *pool,
                                                const char *fname)
{
#if APR_HAS_SHARED_MEMORY
    apr_status_t rv;

    /* The shared memory file must not exist before we create the
     * segment. */
    apr_shm_remove(fname, pool); /* ignore errors */

    rv = apr_shm_create(&zevent_scoreboard_shm, scoreboard_size, fname, pool);
    if (rv != APR_SUCCESS) {
        return rv;
    }
#endif /* APR_HAS_SHARED_MEMORY */
    return APR_SUCCESS;
}

/* ToDo: This function should be made to handle setting up
 * a scoreboard shared between processes using any IPC technique,
 * not just a shared memory segment
 */
static apr_status_t open_scoreboard(apr_pool_t *pconf)
{
#if APR_HAS_SHARED_MEMORY
    apr_status_t rv;
    char *fname = NULL;
    apr_pool_t *global_pool;

    /* We don't want to have to recreate the scoreboard after
     * restarts, so we'll create a global pool and never clean it.
     */
    rv = apr_pool_create(&global_pool, NULL);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    /* The config says to create a name-based shmem */
    if (zevent_scoreboard_fname) {
        
        return create_namebased_scoreboard(global_pool, fname);
    }
    else { /* config didn't specify, we get to choose shmem type */
        rv = apr_shm_create(&zevent_scoreboard_shm, scoreboard_size, NULL,
                            global_pool); /* anonymous shared memory */
        if ((rv != APR_SUCCESS) && (rv != APR_ENOTIMPL)) {
            return rv;
        }
        /* Make up a filename and do name-based shmem */
        else if (rv == APR_ENOTIMPL) {
            /* Make sure it's an absolute pathname */
            zevent_scoreboard_fname = DEFAULT_SCOREBOARD;
            return create_namebased_scoreboard(global_pool, fname);
        }
    }
#endif /* APR_HAS_SHARED_MEMORY */
    return APR_SUCCESS;
}

/* If detach is non-zero, this is a seperate child process,
 * if zero, it is a forked child.
 */
apr_status_t zevent_reopen_scoreboard(apr_pool_t *p, apr_shm_t **shm, int detached)
{
#if APR_HAS_SHARED_MEMORY
    if (!detached) {
        return APR_SUCCESS;
    }
    if (apr_shm_size_get(zevent_scoreboard_shm) < scoreboard_size) {
        apr_shm_detach(zevent_scoreboard_shm);
        zevent_scoreboard_shm = NULL;
        return APR_EINVAL;
    }
    /* everything will be cleared shortly */
    if (*shm) {
        *shm = zevent_scoreboard_shm;
    }
#endif
    return APR_SUCCESS;
}

apr_status_t zevent_cleanup_scoreboard(void *d)
{

    if (zevent_scoreboard_image == NULL) {
        return APR_SUCCESS;
    }

    if (zevent_scoreboard_image->global->sb_type == SB_SHARED) {
        zevent_cleanup_shared_mem(NULL);
    }
    else {
        free(zevent_scoreboard_image->global);
        free(zevent_scoreboard_image);
        zevent_scoreboard_image = NULL;
    }
    return APR_SUCCESS;
}

/* Create or reinit an existing scoreboard. The MPM can control whether
 * the scoreboard is shared across multiple processes or not
 */
int zevent_create_scoreboard(apr_pool_t *p, zevent_scoreboard_e sb_type)
{
    int running_gen = 0;
    int i;
#if APR_HAS_SHARED_MEMORY
    apr_status_t rv;
#endif

    if (zevent_scoreboard_image) {
        running_gen = zevent_scoreboard_image->global->running_generation;
        zevent_scoreboard_image->global->restart_time = apr_time_now();
        memset(zevent_scoreboard_image->parent, 0,
               sizeof(process_score) * server_limit);
        for (i = 0; i < server_limit; i++) {
            memset(zevent_scoreboard_image->servers[i], 0,
                   sizeof(worker_score) * thread_limit);
	}
	return OK;
    }

    zevent_calc_scoreboard_size();
#if APR_HAS_SHARED_MEMORY
    if (sb_type == SB_SHARED) {
        void *sb_shared;
        rv = open_scoreboard(p);
        if (rv || !(sb_shared = apr_shm_baseaddr_get(zevent_scoreboard_shm))) {
            return -1;
        }
        memset(sb_shared, 0, scoreboard_size);
        zevent_init_scoreboard(sb_shared);
    }
    else
#endif
    {
        /* A simple malloc will suffice */
        void *sb_mem = calloc(1, scoreboard_size);
        if (sb_mem == NULL) {
            return -1;
        }
        zevent_init_scoreboard(sb_mem);
    }

    zevent_scoreboard_image->global->sb_type = sb_type;
    zevent_scoreboard_image->global->running_generation = running_gen;
    zevent_scoreboard_image->global->restart_time = apr_time_now();

    apr_pool_cleanup_register(p, NULL, zevent_cleanup_scoreboard, apr_pool_cleanup_null);

    return OK;
}

/* Routines called to deal with the scoreboard image
 * --- note that we do *not* need write locks, since update_child_status
 * only updates a *single* record in place, and only one process writes to
 * a given scoreboard slot at a time (either the child process owning that
 * slot, or the parent, noting that the child has died).
 *
 * As a final note --- setting the score entry to getpid() is always safe,
 * since when the parent is writing an entry, it's only noting SERVER_DEAD
 * anyway.
 */

ZEVENT_DECLARE(int) zevent_exists_scoreboard_image(void)
{
    return (zevent_scoreboard_image ? 1 : 0);
}

int find_child_by_pid(apr_proc_t *pid)
{
    int i;
    int max_daemons_limit;

    zevent_mpm_query(ZEVENT_MPMQ_MAX_DAEMONS, &max_daemons_limit);

    for (i = 0; i < max_daemons_limit; ++i) {
        if (zevent_scoreboard_image->parent[i].pid == pid->pid) {
            return i;
        }
    }

    return -1;
}

ZEVENT_DECLARE(void) zevent_create_sb_handle(zevent_sb_handle_t **new_sbh, apr_pool_t *p,
                                     int child_num, int thread_num)
{
    *new_sbh = (zevent_sb_handle_t *)apr_palloc(p, sizeof(zevent_sb_handle_t));
    (*new_sbh)->child_num = child_num;
    (*new_sbh)->thread_num = thread_num;
}

ZEVENT_DECLARE(int) zevent_update_child_status_from_indexes(int child_num,
                                                    int thread_num,
                                                    int status)
{
    int old_status;
    worker_score *ws;
    process_score *ps;

    if (child_num < 0) {
        return -1;
    }

    ws = &zevent_scoreboard_image->servers[child_num][thread_num];
    old_status = ws->status;
    ws->status = status;

    ps = &zevent_scoreboard_image->parent[child_num];

    if (status == SERVER_READY
        && old_status == SERVER_STARTING) {
        ws->thread_num = child_num * thread_limit + thread_num;
        ps->generation = zevent_my_generation;
    }

    if (zevent_extended_status) {
        ws->last_used = apr_time_now();
        if (status == SERVER_READY || status == SERVER_DEAD) {
            /*
             * Reset individual counters
             */
            if (status == SERVER_DEAD) {
                ws->my_access_count = 0L;
                ws->my_bytes_served = 0L;
            }
            ws->conn_count = 0;
            ws->conn_bytes = 0;
        }
   }

    return old_status;
}

ZEVENT_DECLARE(int) zevent_update_child_status(zevent_sb_handle_t *sbh, int status)
{
    if (!sbh)
        return -1;

    return zevent_update_child_status_from_indexes(sbh->child_num, sbh->thread_num,
                                               status);
}

void zevent_time_process_request(zevent_sb_handle_t *sbh, int status)
{
    worker_score *ws;

    if (!sbh)
        return;

    if (sbh->child_num < 0) {
        return;
    }

    ws = &zevent_scoreboard_image->servers[sbh->child_num][sbh->thread_num];

    if (status == START_PREQUEST) {
        ws->start_time = apr_time_now();
    }
    else if (status == STOP_PREQUEST) {
        ws->stop_time = apr_time_now();
    }
}

ZEVENT_DECLARE(worker_score *) zevent_get_scoreboard_worker(int x, int y)
{
    if (((x < 0) || (server_limit < x)) ||
        ((y < 0) || (thread_limit < y))) {
        return(NULL); /* Out of range */
    }
    return &zevent_scoreboard_image->servers[x][y];
}

ZEVENT_DECLARE(process_score *) zevent_get_scoreboard_process(int x)
{
    if ((x < 0) || (server_limit < x)) {
        return(NULL); /* Out of range */
    }
    return &zevent_scoreboard_image->parent[x];
}

ZEVENT_DECLARE(global_score *) zevent_get_scoreboard_global()
{
    return zevent_scoreboard_image->global;
}

