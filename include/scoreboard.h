/**
 * @file  scoreboard.h
 */

#ifndef ZEVENT_SCOREBOARD_H
#define ZEVENT_SCOREBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
#include <sys/times.h>

#include "zevent_config.h"
#include "apr_hooks.h"
#include "apr_thread_proc.h"
#include "apr_portable.h"
#include "apr_shm.h"

/* Scoreboard file, if there is one */
#ifndef DEFAULT_SCOREBOARD
#define DEFAULT_SCOREBOARD "logs/runtime_status"
#endif

#define MPM_CHILD_PID(i) (zevent_scoreboard_image->parent[i].pid)
#define MPM_NOTE_CHILD_KILLED(i) (MPM_CHILD_PID(i) = 0)

#ifndef SCOREBOARD_MAINTENANCE_INTERVAL
#define SCOREBOARD_MAINTENANCE_INTERVAL 1000000
#endif



/* Scoreboard info on a process is, for now, kept very brief --- 
 * just status value and pid (the latter so that the caretaker process
 * can properly update the scoreboard when a process dies).  We may want
 * to eventually add a separate set of long_score structures which would
 * give, for each process, the number of requests serviced, and info on
 * the current, or most recent, request.
 *
 * Status values:
 */

#define SERVER_DEAD 0
#define SERVER_STARTING 1	/* Server Starting up */
#define SERVER_READY 2		/* Waiting for connection (or accept() lock) */
#define SERVER_BUSY_READ 3	/* Reading a client request */
#define SERVER_BUSY_WRITE 4	/* Processing a client request */
#define SERVER_BUSY_KEEPALIVE 5	/* Waiting for more requests via keepalive */
#define SERVER_BUSY_LOG 6	/* Logging the request */
#define SERVER_BUSY_DNS 7	/* Looking up a hostname */
#define SERVER_CLOSING 8	/* Closing the connection */
#define SERVER_GRACEFUL 9	/* server is gracefully finishing request */
#define SERVER_IDLE_KILL 10     /* Server is cleaning up idle children. */
#define SERVER_NUM_STATUS 11	/* number of status settings */

/* Type used for generation indicies.  Startup and every restart cause a
 * new generation of children to be spawned.  Children within the same
 * generation share the same configuration information -- pointers to stuff
 * created at config time in the parent are valid across children.  However,
 * this can't work effectively with non-forked architectures.  So while the
 * arrays in the scoreboard never change between the parent and forked
 * children, so they do not require shm storage, the contents of the shm
 * may contain no pointers.
 */
typedef int zevent_generation_t;

/* Is the scoreboard shared between processes or not? 
 * Set by the MPM when the scoreboard is created.
 */
typedef enum {
    SB_NOT_SHARED = 1,
    SB_SHARED = 2
} zevent_scoreboard_e;

#define SB_WORKING  0  /* The server is busy and the child is useful. */
#define SB_IDLE_DIE 1  /* The server is idle and the child is superfluous. */
                       /*   The child should check for this and exit gracefully. */

/* stuff which is worker specific */
/***********************WARNING***************************************/
/* These are things that are used by mod_status. Do not put anything */
/*   in here that you cannot live without. This structure will not   */
/*   be available if mod_status is not loaded.                       */
/*********************************************************************/
typedef struct worker_score worker_score;

struct worker_score {
    int thread_num;
#if APR_HAS_THREADS
    apr_os_thread_t tid;
#endif
    /* With some MPMs (e.g., worker), a worker_score can represent
     * a thread in a terminating process which is no longer
     * represented by the corresponding process_score.  These MPMs
     * should set pid and generation fields in the worker_score.
     */
    pid_t pid;
    zevent_generation_t generation;
    unsigned char status;
    unsigned long access_count;
    apr_off_t     bytes_served;
    unsigned long my_access_count;
    apr_off_t     my_bytes_served;
    apr_off_t     conn_bytes;
    unsigned short conn_count;
    apr_time_t start_time;
    apr_time_t stop_time;
    apr_time_t last_used;
    char client[32];		/* Keep 'em small... */
    char request[64];		/* We just want an idea... */
    char vhost[32];	        /* What virtual host is being accessed? */
};

typedef struct {
    int             server_limit;
    int             thread_limit;
    zevent_scoreboard_e sb_type;
    zevent_generation_t running_generation; /* the generation of children which
                                         * should still be serving requests.
                                         */
    apr_time_t restart_time;
    int             lb_limit;
} global_score;

/* stuff which the parent generally writes and the children rarely read */
typedef struct process_score process_score;
struct process_score{
    pid_t pid;
    zevent_generation_t generation;	/* generation of this child */
    zevent_scoreboard_e sb_type;
    int quiescing;          /* the process whose pid is stored above is
                             * going down gracefully
                             */
};

/* stuff which is lb specific */
typedef struct lb_score lb_score;
struct lb_score{
    /* TODO: make a real stuct from this */
    unsigned char data[1024];
};

/* Scoreboard is now in 'local' memory, since it isn't updated once created,
 * even in forked architectures.  Child created-processes (non-fork) will
 * set up these indicies into the (possibly relocated) shmem records.
 */
typedef struct {
    global_score *global;
    process_score *parent;
    worker_score **servers;
    lb_score     *balancers;
} scoreboard;

typedef struct zevent_sb_handle_t zevent_sb_handle_t;

ZEVENT_DECLARE(int) zevent_exists_scoreboard_image(void);
//ZEVENT_DECLARE(void) zevent_increment_counts(zevent_sb_handle_t *sbh, request_rec *r);

int zevent_create_scoreboard(apr_pool_t *p, zevent_scoreboard_e t);
apr_status_t zevent_reopen_scoreboard(apr_pool_t *p, apr_shm_t **shm, int detached);
void zevent_init_scoreboard(void *shared_score);
ZEVENT_DECLARE(int) zevent_calc_scoreboard_size(void);
apr_status_t zevent_cleanup_scoreboard(void *d);

ZEVENT_DECLARE(void) zevent_create_sb_handle(zevent_sb_handle_t **new_sbh, apr_pool_t *p,
                                     int child_num, int thread_num);
    
int find_child_by_pid(apr_proc_t *pid);

ZEVENT_DECLARE(int) zevent_update_child_status_from_indexes(int child_num,
                                                    int thread_num,
                                                    int status);

//ZEVENT_DECLARE(int) zevent_update_child_status(zevent_sb_handle_t *sbh, int status, request_rec *r);
//void zevent_time_process_request(zevent_sb_handle_t *sbh, int status);

ZEVENT_DECLARE(worker_score *) zevent_get_scoreboard_worker(int x, int y);
ZEVENT_DECLARE(process_score *) zevent_get_scoreboard_process(int x);
ZEVENT_DECLARE(global_score *) zevent_get_scoreboard_global(void);
ZEVENT_DECLARE(lb_score *) zevent_get_scoreboard_lb(int lb_num);

ZEVENT_DECLARE_DATA extern scoreboard *zevent_scoreboard_image;
ZEVENT_DECLARE_DATA extern const char *zevent_scoreboard_fname;
ZEVENT_DECLARE_DATA extern int zevent_extended_status;
ZEVENT_DECLARE_DATA extern int zevent_mod_status_reqtail;

ZEVENT_DECLARE_DATA extern zevent_generation_t volatile zevent_my_generation;

#define START_PREQUEST 1
#define STOP_PREQUEST  2

#ifdef __cplusplus
}
#endif

#endif	/* !ZEVENT_SCOREBOARD_H */
