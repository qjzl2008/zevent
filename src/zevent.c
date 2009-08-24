#include "apr.h"
#include "apr_portable.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_thread_proc.h"
#include "apr_signal.h"
#include "apr_thread_mutex.h"
#include "apr_proc_mutex.h"
#include "apr_poll.h"
#include "apr_ring.h"
#include "apr_queue.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if APR_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if !APR_HAS_THREADS
#error The Event MPM requires APR threads, but they are unavailable.
#endif

#include "zevent_hooks.h"
#include "iniparser.h"
#include "dictionary.h"
#include "zevent.h"
#include "pod.h"
#include "zevent_mpm.h"
#include "zevent_network.h"
#include "scoreboard.h"
#include "fdqueue.h"
#include "log.h"

#include <signal.h>

/*
 * Actual definitions of config globals
 */
static dictionary *d = NULL;
static char *log = NULL;
int zevent_threads_per_child = 0;   /* Worker threads per child */
static int zevent_daemons_to_start = 0;
static int min_spare_threads = 0;
static int max_spare_threads = 0;
static int zevent_daemons_limit = 0;
static int server_limit = DEFAULT_SERVER_LIMIT;
static int first_server_limit = 0;
static int thread_limit = DEFAULT_THREAD_LIMIT;
static int first_thread_limit = 0;
static int dying = 0;
static int workers_may_exit = 0;
static int start_thread_may_exit = 0;
static int listener_may_exit = 0;
static int resource_shortage = 0;
static fd_queue_t *worker_queue;
static fd_queue_info_t *worker_queue_info;
static int mpm_state = ZEVENT_MPMQ_STARTING;
static int sick_child_detected;

static apr_pollset_t *event_pollset;

/* The structure used to pass unique initialization info to each thread */
typedef struct
{
    int pid;
    int tid;
    int sd;
} proc_info;

/* Structure used to pass information to the thread responsible for
 * creating the rest of the threads.
 */
typedef struct
{
    apr_thread_t **threads;
    apr_thread_t *listener;
    int child_num_arg;
    apr_threadattr_t *threadattr;
} thread_starter;

typedef enum
{
    PT_CSD,
    PT_ACCEPT
} poll_type_e;

typedef struct
{
    poll_type_e type;
    void *baton;
} listener_poll_type;

#define ID_FROM_CHILD_THREAD(c, t)    ((c * thread_limit) + t)

/*
 * The max child slot ever assigned, preserved across restarts.  Necessary
 * to deal with MaxClients changes across ZEVENT_SIG_GRACEFUL restarts.  We
 * use this value to optimize routines that have to scan the entire
 * scoreboard.
 */
int zevent_max_daemons_limit = -1;

static zevent_pod_t *pod;

/* *Non*-shared http_main globals... */


/* The worker MPM respects a couple of runtime flags that can aid
 * in debugging. Setting the -DNO_DETACH flag will prevent the root process
 * from detaching from its controlling terminal. Additionally, setting
 * the -DONE_PROCESS flag (which implies -DNO_DETACH) will get you the
 * child_main loop running in the process which originally started up.
 * This gives you a pretty nice debugging environment.  (You'll get a SIGHUP
 * early in standalone_main; just continue through.  This is the server
 * trying to kill off any child processes which it might have lying
 * around --- Apache doesn't keep track of their pids, it just sends
 * SIGHUP to the process group, ignoring it in the root process.
 * Continue through and you'll be fine.).
 */

static int one_process = 0;

static apr_pool_t *pconf;       /* Pool for config stuff */
static apr_pool_t *pchild;      /* Pool for httpd child stuff */

static pid_t zevent_my_pid;         /* Linux getpid() doesn't work except in main
                                   thread. Use this instead */
static pid_t parent_pid;
static apr_os_thread_t *listener_os_thread;

/* The LISTENER_SIGNAL signal will be sent from the main thread to the
 * listener thread to wake it up for graceful termination (what a child
 * process from an old generation does when the admin does "apachectl
 * graceful").  This signal will be blocked in all threads of a child
 * process except for the listener thread.
 */
#define LISTENER_SIGNAL     SIGHUP

/* An array of socket descriptors in use by each thread used to
 * perform a non-graceful (forced) shutdown of the server.
 */
static apr_socket_t **worker_sockets;

static void close_worker_sockets(void)
{
    int i;
    for (i = 0; i < zevent_threads_per_child; i++) {
        if (worker_sockets[i]) {
            apr_socket_close(worker_sockets[i]);
            worker_sockets[i] = NULL;
        }
    }
}

static void wakeup_listener(void)
{
    listener_may_exit = 1;
    if (!listener_os_thread) {
        /* XXX there is an obscure path that this doesn't handle perfectly:
         *     right after listener thread is created but before
         *     listener_os_thread is set, the first worker thread hits an
         *     error and starts graceful termination
         */
        return;
    }
    /*
     * we should just be able to "kill(zevent_my_pid, LISTENER_SIGNAL)" on all
     * platforms and wake up the listener thread since it is the only thread
     * with SIGHUP unblocked, but that doesn't work on Linux
     */
    pthread_kill(*listener_os_thread, LISTENER_SIGNAL);
}

#define ST_INIT              0
#define ST_GRACEFUL          1
#define ST_UNGRACEFUL        2

static int terminate_mode = ST_INIT;

static void signal_threads(int mode)
{
    if (terminate_mode == mode) {
        return;
    }
    terminate_mode = mode;
    mpm_state = ZEVENT_MPMQ_STOPPING;

    /* in case we weren't called from the listener thread, wake up the
     * listener thread
     */
    wakeup_listener();

    /* for ungraceful termination, let the workers exit now;
     * for graceful termination, the listener thread will notify the
     * workers to exit once it has stopped accepting new connections
     */
    if (mode == ST_UNGRACEFUL) {
        workers_may_exit = 1;
        zevent_queue_interrupt_all(worker_queue);
        zevent_queue_info_term(worker_queue_info);
        close_worker_sockets(); /* forcefully kill all current connections */
    }
}

ZEVENT_DECLARE(apr_status_t) zevent_mpm_query(int query_code, int *result)
{
    switch (query_code) {
    case ZEVENT_MPMQ_MAX_DAEMON_USED:
        *result = zevent_max_daemons_limit;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_IS_ASYNC:
        *result = 1;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_HARD_LIMIT_DAEMONS:
        *result = server_limit;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_HARD_LIMIT_THREADS:
        *result = thread_limit;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_MAX_THREADS:
        *result = zevent_threads_per_child;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_MIN_SPARE_DAEMONS:
        *result = 0;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_MIN_SPARE_THREADS:
        *result = min_spare_threads;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_MAX_SPARE_DAEMONS:
        *result = 0;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_MAX_SPARE_THREADS:
        *result = max_spare_threads;
        return APR_SUCCESS;
   case ZEVENT_MPMQ_MAX_DAEMONS:
        *result = zevent_daemons_limit;
        return APR_SUCCESS;
    case ZEVENT_MPMQ_MPM_STATE:
        *result = mpm_state;
        return APR_SUCCESS;
    }
    return APR_ENOTIMPL;
}


/* a clean exit from a child with proper cleanup */
static void clean_child_exit(int code) __attribute__ ((noreturn));
static void clean_child_exit(int code)
{
    mpm_state = ZEVENT_MPMQ_STOPPING;
    zevent_run_child_fini(pchild);
    if (pchild) {
        apr_pool_destroy(pchild);
    }
    exit(code);
}

static void just_die(int sig)
{
    clean_child_exit(0);
}

/*****************************************************************
 * Connection structures and accounting...
 */

/* volatile just in case */
static int volatile shutdown_pending;
static int volatile restart_pending;
static int volatile is_graceful;
static volatile int child_fatal;
zevent_generation_t volatile zevent_my_generation;

/*
 * zevent_start_shutdown() and zevent_start_restart(), below, are a first stab at
 * functions to initiate shutdown or restart without relying on signals.
 * Previously this was initiated in sig_term() and restart() signal handlers,
 * but we want to be able to start a shutdown/restart from other sources --
 * e.g. on Win32, from the service manager. Now the service manager can
 * call zevent_start_shutdown() or zevent_start_restart() as appropiate.  Note that
 * these functions can also be called by the child processes, since global
 * variables are no longer used to pass on the required action to the parent.
 *
 * These should only be called from the parent process itself, since the
 * parent process will use the shutdown_pending and restart_pending variables
 * to determine whether to shutdown or restart. The child process should
 * call signal_parent() directly to tell the parent to die -- this will
 * cause neither of those variable to be set, which the parent will
 * assume means something serious is wrong (which it will be, for the
 * child to force an exit) and so do an exit anyway.
 */

static void zevent_start_shutdown(int graceful)
{
    mpm_state = ZEVENT_MPMQ_STOPPING;
    if (shutdown_pending == 1) {
        /* Um, is this _probably_ not an error, if the user has
         * tried to do a shutdown twice quickly, so we won't
         * worry about reporting it.
         */
        return;
    }
    shutdown_pending = 1;
    is_graceful = graceful;
}

/* do a graceful restart if graceful == 1 */
static void zevent_start_restart(int graceful)
{
    mpm_state = ZEVENT_MPMQ_STOPPING;
    if (restart_pending == 1) {
        /* Probably not an error - don't bother reporting it */
        return;
    }
    restart_pending = 1;
    is_graceful = graceful;
}

static void sig_term(int sig)
{
    zevent_start_shutdown(sig == ZEVENT_SIG_GRACEFUL_STOP);
}

static void restart(int sig)
{
    zevent_start_restart(sig == ZEVENT_SIG_GRACEFUL);
}

static void set_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = sig_term;
    if (sigaction(SIGTERM, &sa, NULL) < 0)
	    zevent_log_error(APLOG_MARK,NULL,"sigaction(SIGTERM)");
#ifdef SIGINT
    if (sigaction(SIGINT, &sa, NULL) < 0)
	    zevent_log_error(APLOG_MARK,NULL,"sigaction(SIGINT)");
#endif
#ifdef SIGXCPU
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGXCPU, &sa, NULL) < 0)
	    zevent_log_error(APLOG_MARK,NULL,"sigaction(SIGXCPU)");
#endif
#ifdef SIGXFSZ
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGXFSZ, &sa, NULL) < 0)
	    zevent_log_error(APLOG_MARK,NULL,"sigaction(SIGXFSZ)");
#endif
#ifdef SIGPIPE
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) < 0)
	    zevent_log_error(APLOG_MARK,NULL,"sigaction(SIGPIPE)");
#endif

    /* we want to ignore HUPs and ZEVENT_SIG_GRACEFUL while we're busy
     * processing one */
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, ZEVENT_SIG_GRACEFUL);
    sa.sa_handler = restart;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
	    zevent_log_error(APLOG_MARK,NULL,"sigaction(SIGHUP)");
    if (sigaction(ZEVENT_SIG_GRACEFUL, &sa, NULL) < 0)
	    zevent_log_error(APLOG_MARK,NULL,
			    "sigaction("ZEVENT_SIG_GRACEFUL_STOP_STRING ")");
}

/*****************************************************************
 * Here follows a long bunch of generic server bookkeeping stuff...
 */

int zevent_graceful_stop_signalled(void)
    /* XXX this is really a bad confusing obsolete name
     * maybe it should be zevent_process_exiting?
     */
{
    /* note: for a graceful termination, listener_may_exit will be set before
     *       workers_may_exit, so check listener_may_exit
     */
    return listener_may_exit;
}
/*****************************************************************
 * Child process main loop.
 */

static int process_socket(apr_pool_t * p, apr_socket_t * sock,
                          conn_state_t * cs, int my_child_num,
                          int my_thread_num)
{
	if(zevent_run_process_connection(cs) < 0)
	{
		zevent_lingering_close(sock);

		apr_brigade_cleanup(cs->bbin);
		apr_brigade_destroy(cs->bbin);
		cs->bbin = NULL;
		apr_bucket_alloc_destroy(cs->bain);
		cs->bain = NULL;

		apr_brigade_cleanup(cs->bbout);
		apr_brigade_destroy(cs->bbout);
		cs->bbout = NULL;
		apr_bucket_alloc_destroy(cs->baout);
		cs->baout = NULL;

		apr_pool_clear(p);
		zevent_push_pool(worker_queue_info, p);

	}
	return 0;
}


static void unblock_signal(int sig)
{
    sigset_t sig_mask;

    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, sig);
#if defined(SIGPROCMASK_SETS_THREAD_MASK)
    sigprocmask(SIG_UNBLOCK, &sig_mask, NULL);
#else
    pthread_sigmask(SIG_UNBLOCK, &sig_mask, NULL);
#endif
}

static void dummy_signal_handler(int sig)
{
    /* XXX If specifying SIG_IGN is guaranteed to unblock a syscall,
     *     then we don't need this goofy function.
     */
}

static apr_status_t push2worker(const apr_pollfd_t * pfd,
                                apr_pollset_t * pollset)
{

    listener_poll_type *pt = (listener_poll_type *) pfd->client_data;
    conn_state_t *cs = (conn_state_t *) pt->baton;
   // cs->pfd->desc = pfd->desc;
    cs->pfd->rtnevents = pfd->rtnevents;

    apr_status_t rc;

    rc = apr_pollset_remove(pollset, pfd);

    /*
     * Some of the pollset backends, like KQueue or Epoll
     * automagically remove the FD if the socket is closed,
     * therefore, we can accept _SUCCESS or _NOTFOUND,
     * and we still want to keep going
     */
   /* if (rc != APR_SUCCESS && rc != APR_NOTFOUND) {
        cs->state = CONN_STATE_LINGER;
    }*/

    rc = zevent_queue_push(worker_queue, cs->pfd->desc.s, cs, cs->p);
    if (rc != APR_SUCCESS) {
        /* trash the connection; we couldn't queue the connected
         * socket to a worker
         */

        apr_socket_close(cs->pfd->desc.s);
        apr_pool_clear(cs->p);
        zevent_push_pool(worker_queue_info, cs->p);
    }

    return APR_SUCCESS;
}

/* get_worker:
 *     reserve a worker thread, block if all are currently busy.
 *     this prevents the worker queue from overflowing and lets
 *     other processes accept new connections in the mean time.
 */
static int get_worker(int *have_idle_worker_p)
{
    apr_status_t rc;

    if (!*have_idle_worker_p) {
        rc = zevent_queue_info_wait_for_idler(worker_queue_info);

        if (rc == APR_SUCCESS) {
            *have_idle_worker_p = 1;
            return 1;
        }
        else {
            if (!APR_STATUS_IS_EOF(rc)) {
                signal_threads(ST_GRACEFUL);
            }
            return 0;
        }
    }
    else {
        /* already reserved a worker thread - must have hit a
         * transient error on a previous pass
         */
        return 1;
    }
}

static void *listener_thread(apr_thread_t * thd, void *dummy)
{
    apr_status_t rc;
    proc_info *ti = dummy;
    int process_slot = ti->pid;
    apr_pool_t *tpool = apr_thread_pool_get(thd);
    void *csd = NULL;
    apr_pool_t *ptrans;         /* Pool for per-transaction stuff */
    zevent_listen_rec *lr;
    int have_idle_worker = 0;
    conn_state_t *cs;
    const apr_pollfd_t *out_pfd;
    apr_int32_t num = 0;
    apr_interval_time_t timeout_interval;
    listener_poll_type *pt;

    free(ti);
    
    /* We set this to force apr_pollset to wakeup if there hasn't been any IO
     * on any of its sockets.  This allows sockets to have been added
     * when no other keepalive operations where going on.
     *
     * current value is 1 second
     */
    timeout_interval = 1000000;

    /* the following times out events that are really close in the future
     *   to prevent extra poll calls
     *
     * current value is .1 second
     */
#define TIMEOUT_FUDGE_FACTOR 100000

    /* POLLSET_SCALE_FACTOR * zevent_threads_per_child sets the size of
     * the pollset.  I've seen 15 connections per active worker thread
     * running SPECweb99.
     *
     * However, with the newer apr_pollset, this is the number of sockets that
     * we will return to any *one* call to poll().  Therefore, there is no
     * reason to make it more than zevent_threads_per_child.
     */
#define POLLSET_SCALE_FACTOR 1

    /* Create the main pollset */
    rc = apr_pollset_create(&event_pollset,
                            zevent_threads_per_child * POLLSET_SCALE_FACTOR,
                            tpool, APR_POLLSET_THREADSAFE);
    if (rc != APR_SUCCESS) {
        signal_threads(ST_GRACEFUL);
        return NULL;
    }

    for (lr = zevent_listeners; lr != NULL; lr = lr->next) {
        apr_pollfd_t pfd = { 0 };
        pt = apr_pcalloc(tpool, sizeof(*pt));
        pfd.desc_type = APR_POLL_SOCKET;
        pfd.desc.s = lr->sd;
        pfd.reqevents = APR_POLLIN;

        pt->type = PT_ACCEPT;
        pt->baton = lr;

        pfd.client_data = pt;

        apr_socket_opt_set(pfd.desc.s, APR_SO_NONBLOCK, 1);
        apr_pollset_add(event_pollset, &pfd);
    }

    /* Unblock the signal used to wake this thread up, and set a handler for
     * it.
     */
    unblock_signal(LISTENER_SIGNAL);
    apr_signal(LISTENER_SIGNAL, dummy_signal_handler);

    while (!listener_may_exit) {

        rc = apr_pollset_poll(event_pollset, timeout_interval, &num,
                              &out_pfd);


        if (rc != APR_SUCCESS) {

            if (APR_STATUS_IS_EINTR(rc)) {

                continue;
            }
            if (!APR_STATUS_IS_TIMEUP(rc)) {
                signal_threads(ST_GRACEFUL);
            }
        }

        if (listener_may_exit)
            break;

        while (num && get_worker(&have_idle_worker)) {

            pt = (listener_poll_type *) out_pfd->client_data;

            if (pt->type == PT_CSD) {

                /* one of the sockets is readable */
                cs = (conn_state_t *) pt->baton;

                rc = push2worker(out_pfd, event_pollset);
                if (rc != APR_SUCCESS) {
			;
                }
                else {
                    have_idle_worker = 0;
                }
            }
            else {

                /* A Listener Socket is ready for an accept() */
                apr_pool_t *recycled_pool = NULL;

                lr = (zevent_listen_rec *) pt->baton;

                zevent_pop_pool(&recycled_pool, worker_queue_info);

                if (recycled_pool == NULL) {

                    /* create a new transaction pool for each accepted socket */
                    apr_allocator_t *allocator;

                    apr_allocator_create(&allocator);
		    //zsh
                    apr_allocator_max_free_set(allocator,
                                               APR_ALLOCATOR_MAX_FREE_UNLIMITED);
                    apr_pool_create_ex(&ptrans, pconf, NULL, allocator);
                    apr_allocator_owner_set(allocator, ptrans);
                    if (ptrans == NULL) {
                        signal_threads(ST_GRACEFUL);
                        return NULL;
                    }
                }
                else {
                    ptrans = recycled_pool;
                }

                apr_pool_tag(ptrans, "transaction");

                rc = lr->accept_func(&csd, lr, ptrans);

                /* later we trash rv and rely on csd to indicate
                 * success/failure
                 */

                if (rc == APR_EGENERAL) {
                    /* E[NM]FILE, ENOMEM, etc */
                    resource_shortage = 1;
                    signal_threads(ST_GRACEFUL);
                }

                if (csd != NULL) {
			//////////////add to pollset/////////////////
			apr_pollfd_t *pfd = apr_pcalloc(ptrans,sizeof(*pfd));
			pfd->desc_type = APR_POLL_SOCKET;
			pfd->desc.s = csd;
			pfd->reqevents = APR_POLLIN;

			conn_state_t *cs = apr_pcalloc(ptrans,sizeof(*cs));
			cs->p = ptrans;
			cs->pfd = pfd;
			cs->pollset = event_pollset;

			cs->bain = apr_bucket_alloc_create(cs->p);
			cs->bbin = apr_brigade_create(cs->p,cs->bain);
			cs->baout = apr_bucket_alloc_create(cs->p);
			cs->bbout = apr_brigade_create(cs->p,cs->baout);

			listener_poll_type *cpt = apr_pcalloc(ptrans,sizeof(*pt));
			cpt->type = PT_CSD;
			cpt->baton = cs;
		        pfd->client_data = cpt;

			apr_pollset_add(event_pollset,pfd);

                }
                else {
                    apr_pool_clear(ptrans);
                    zevent_push_pool(worker_queue_info, ptrans);
                }
            }               /* if:else on pt->type */
            out_pfd++;
            num--;
        }                   /* while for processing poll */
      
    }     /* listener main loop */

    zevent_close_listeners();
    zevent_queue_term(worker_queue);
    dying = 1;
    zevent_scoreboard_image->parent[process_slot].quiescing = 1;

    /* wake up the main thread */
    kill(zevent_my_pid, SIGTERM);

    apr_thread_exit(thd, APR_SUCCESS);

    return NULL;
}

/* XXX For ungraceful termination/restart, we definitely don't want to
 *     wait for active connections to finish but we may want to wait
 *     for idle workers to get out of the queue code and release mutexes,
 *     since those mutexes are cleaned up pretty soon and some systems
 *     may not react favorably (i.e., segfault) if operations are attempted
 *     on cleaned-up mutexes.
 */
static void *APR_THREAD_FUNC worker_thread(apr_thread_t * thd, void *dummy)
{
    proc_info *ti = dummy;
    int process_slot = ti->pid;
    int thread_slot = ti->tid;
    apr_socket_t *csd = NULL;
    conn_state_t *cs;
    apr_pool_t *ptrans;         /* Pool for per-transaction stuff */
    apr_status_t rv;
    int is_idle = 0;

    free(ti);

    zevent_scoreboard_image->servers[process_slot][thread_slot].pid = zevent_my_pid;
    zevent_scoreboard_image->servers[process_slot][thread_slot].generation = zevent_my_generation;
    zevent_update_child_status_from_indexes(process_slot, thread_slot,
                                        SERVER_STARTING);

    while (!workers_may_exit) {
        if (!is_idle) {
            rv = zevent_queue_info_set_idle(worker_queue_info, NULL);
            if (rv != APR_SUCCESS) {
                signal_threads(ST_GRACEFUL);
                break;
            }
            is_idle = 1;
        }

        zevent_update_child_status_from_indexes(process_slot, thread_slot,
                                            SERVER_READY);
      worker_pop:
        if (workers_may_exit) {
            break;
        }
        rv = zevent_queue_pop(worker_queue, &csd, &cs, &ptrans);

        if (rv != APR_SUCCESS) {
            /* We get APR_EOF during a graceful shutdown once all the
             * connections accepted by this server process have been handled.
             */
            if (APR_STATUS_IS_EOF(rv)) {
                break;
            }
            /* We get APR_EINTR whenever zevent_queue_pop() has been interrupted
             * from an explicit call to zevent_queue_interrupt_all(). This allows
             * us to unblock threads stuck in zevent_queue_pop() when a shutdown
             * is pending.
             *
             * If workers_may_exit is set and this is ungraceful termination/
             * restart, we are bound to get an error on some systems (e.g.,
             * AIX, which sanity-checks mutex operations) since the queue
             * may have already been cleaned up.  Don't log the "error" if
             * workers_may_exit is set.
             */
            else if (APR_STATUS_IS_EINTR(rv)) {
                goto worker_pop;
            }
            /* We got some other error. */
            else if (!workers_may_exit) {
            }
            continue;
        }
        is_idle = 0;
        worker_sockets[thread_slot] = csd;

        process_socket(ptrans, csd, cs, process_slot, thread_slot);
        worker_sockets[thread_slot] = NULL;
    }

    zevent_update_child_status_from_indexes(process_slot, thread_slot,
                                        (dying) ? SERVER_DEAD :
                                        SERVER_GRACEFUL);

    apr_thread_exit(thd, APR_SUCCESS);
    return NULL;
}

static int check_signal(int signum)
{
    switch (signum) {
    case SIGTERM:
    case SIGINT:
        return 1;
    }
    return 0;
}



static void create_listener_thread(thread_starter * ts)
{
    int my_child_num = ts->child_num_arg;
    apr_threadattr_t *thread_attr = ts->threadattr;
    proc_info *my_info;
    apr_status_t rv;

    my_info = (proc_info *) malloc(sizeof(proc_info));
    my_info->pid = my_child_num;
    my_info->tid = -1;          /* listener thread doesn't have a thread slot */
    my_info->sd = 0;
    rv = apr_thread_create(&ts->listener, thread_attr, listener_thread,
                           my_info, pchild);
    if (rv != APR_SUCCESS) {
        /* let the parent decide how bad this really is */
        clean_child_exit(ZEVENTEXIT_CHILDSICK);
    }
    apr_os_thread_get(&listener_os_thread, ts->listener);
}

/* XXX under some circumstances not understood, children can get stuck
 *     in start_threads forever trying to take over slots which will
 *     never be cleaned up; for now there is an APLOG_DEBUG message issued
 *     every so often when this condition occurs
 */
static void *APR_THREAD_FUNC start_threads(apr_thread_t * thd, void *dummy)
{
    thread_starter *ts = dummy;
    apr_thread_t **threads = ts->threads;
    apr_threadattr_t *thread_attr = ts->threadattr;
    int child_num_arg = ts->child_num_arg;
    int my_child_num = child_num_arg;
    proc_info *my_info;
    apr_status_t rv;
    int i;
    int threads_created = 0;
    int listener_started = 0;
    int loops;
    int prev_threads_created;

    /* We must create the fd queues before we start up the listener
     * and worker threads. */
    worker_queue = apr_pcalloc(pchild, sizeof(*worker_queue));
    rv = zevent_queue_init(worker_queue, zevent_threads_per_child, pchild);
    if (rv != APR_SUCCESS) {
        clean_child_exit(ZEVENTEXIT_CHILDFATAL);
    }

    rv = zevent_queue_info_create(&worker_queue_info, pchild,
                              zevent_threads_per_child);
    if (rv != APR_SUCCESS) {
        clean_child_exit(ZEVENTEXIT_CHILDFATAL);
    }

    worker_sockets = apr_pcalloc(pchild, zevent_threads_per_child
                                 * sizeof(apr_socket_t *));

    loops = prev_threads_created = 0;
    while (1) {

        /* zevent_threads_per_child does not include the listener thread */
        for (i = 0; i < zevent_threads_per_child; i++) {

            int status =
                zevent_scoreboard_image->servers[child_num_arg][i].status;

            if (status != SERVER_GRACEFUL && status != SERVER_DEAD) {
                continue;
            }

            my_info = (proc_info *) malloc(sizeof(proc_info));
            if (my_info == NULL) {
                clean_child_exit(ZEVENTEXIT_CHILDFATAL);
            }
            my_info->pid = my_child_num;
            my_info->tid = i;
            my_info->sd = 0;

            /* We are creating threads right now */
            zevent_update_child_status_from_indexes(my_child_num, i,
                                                SERVER_STARTING);
            /* We let each thread update its own scoreboard entry.  This is
             * done because it lets us deal with tid better.
             */
            rv = apr_thread_create(&threads[i], thread_attr,
                                   worker_thread, my_info, pchild);
            if (rv != APR_SUCCESS) {
                /* let the parent decide how bad this really is */
                clean_child_exit(ZEVENTEXIT_CHILDSICK);
            }
            threads_created++;
        }

        /* Start the listener only when there are workers available */
        if (!listener_started && threads_created) {
            create_listener_thread(ts);
            listener_started = 1;
        }


        if (start_thread_may_exit || threads_created == zevent_threads_per_child) {
            break;
        }
        /* wait for previous generation to clean up an entry */
        apr_sleep(apr_time_from_sec(1));
        ++loops;
        if (loops % 120 == 0) { /* every couple of minutes */
            if (prev_threads_created == threads_created) {
		    ;
            }
            prev_threads_created = threads_created;
        }
    }

    /* What state should this child_main process be listed as in the
     * scoreboard...?
     *  zevent_update_child_status_from_indexes(my_child_num, i, SERVER_STARTING,
     *                                      (request_rec *) NULL);
     *
     *  This state should be listed separately in the scoreboard, in some kind
     *  of process_status, not mixed in with the worker threads' status.
     *  "life_status" is almost right, but it's in the worker's structure, and
     *  the name could be clearer.   gla
     */
    apr_thread_exit(thd, APR_SUCCESS);
    return NULL;
}

static void join_workers(apr_thread_t * listener, apr_thread_t ** threads)
{
    int i;
    apr_status_t rv, thread_rv;

    if (listener) {
        int iter;

        /* deal with a rare timing window which affects waking up the
         * listener thread...  if the signal sent to the listener thread
         * is delivered between the time it verifies that the
         * listener_may_exit flag is clear and the time it enters a
         * blocking syscall, the signal didn't do any good...  work around
         * that by sleeping briefly and sending it again
         */

        iter = 0;
        while (iter < 10 &&
               pthread_kill(*listener_os_thread, 0)
               == 0) {

            /* listener not dead yet */
            apr_sleep(apr_time_make(0, 500000));
            wakeup_listener();
            ++iter;
        }
        if (iter >= 10) {
		zevent_log_error(APLOG_MARK,NULL,
				"the listener thread didn't exit");
        }
        else {
            rv = apr_thread_join(&thread_rv, listener);
            if (rv != APR_SUCCESS) {
		    zevent_log_error(APLOG_MARK,NULL,
				    "apr_thread_join:unable to join listener"
				    "thread");
            }
        }
    }

    for (i = 0; i < zevent_threads_per_child; i++) {
        if (threads[i]) {       /* if we ever created this thread */
            rv = apr_thread_join(&thread_rv, threads[i]);
            if (rv != APR_SUCCESS) {
		    zevent_log_error(APLOG_MARK,NULL,"apr_thread_join:unable to join "
				    "worker thread %d",i);
            }
        }
    }
}

static void join_start_thread(apr_thread_t * start_thread_id)
{
    apr_status_t rv, thread_rv;

    start_thread_may_exit = 1;  /* tell it to give up in case it is still
                                 * trying to take over slots from a
                                 * previous generation
                                 */
    rv = apr_thread_join(&thread_rv, start_thread_id);
    if (rv != APR_SUCCESS) {
	    zevent_log_error(APLOG_MARK,NULL,"apr_thread_join:unable to join"
			    " the start thread");
    }
}

static void child_main(int child_num_arg)
{
    apr_thread_t **threads;
    apr_status_t rv;
    thread_starter *ts;
    apr_threadattr_t *thread_attr;
    apr_thread_t *start_thread_id;

    mpm_state = ZEVENT_MPMQ_STARTING;       /* for benefit of any hooks that run as this
                                         * child initializes
                                         */
    zevent_my_pid = getpid();
//    zevent_fatal_signal_child_setup();
    apr_pool_create(&pchild, pconf);

    /*stuff to do before we switch id's, so we have permissions. */
    zevent_reopen_scoreboard(pchild, NULL, 0);

    /* done with init critical section */

    /* Just use the standard apr_setup_signal_thread to block all signals
     * from being received.  The child processes no longer use signals for
     * any communication with the parent process.
     */
    rv = apr_setup_signal_thread();
    if (rv != APR_SUCCESS) {
        clean_child_exit(ZEVENTEXIT_CHILDFATAL);
    }

    zevent_run_child_init(pchild);

    /* Setup worker threads */

    /* clear the storage; we may not create all our threads immediately,
     * and we want a 0 entry to indicate a thread which was not created
     */
    threads = (apr_thread_t **) calloc(1,
                                       sizeof(apr_thread_t *) *
                                       zevent_threads_per_child);
    if (threads == NULL) {
        clean_child_exit(ZEVENTEXIT_CHILDFATAL);
    }

    ts = (thread_starter *) apr_palloc(pchild, sizeof(*ts));

    apr_threadattr_create(&thread_attr, pchild);
    /* 0 means PTHREAD_CREATE_JOINABLE */
    apr_threadattr_detach_set(thread_attr, 0);

    ts->threads = threads;
    ts->listener = NULL;
    ts->child_num_arg = child_num_arg;
    ts->threadattr = thread_attr;

    rv = apr_thread_create(&start_thread_id, thread_attr, start_threads,
                           ts, pchild);
    if (rv != APR_SUCCESS) {
        /* let the parent decide how bad this really is */
        clean_child_exit(ZEVENTEXIT_CHILDSICK);
    }

    mpm_state = ZEVENT_MPMQ_RUNNING;

    /* If we are only running in one_process mode, we will want to
     * still handle signals. */
    if (one_process) {
        /* Block until we get a terminating signal. */
        apr_signal_thread(check_signal);
        /* make sure the start thread has finished; signal_threads()
         * and join_workers() depend on that
         */
        /* XXX join_start_thread() won't be awakened if one of our
         *     threads encounters a critical error and attempts to
         *     shutdown this child
         */
        join_start_thread(start_thread_id);

        /* helps us terminate a little more quickly than the dispatch of the
         * signal thread; beats the Pipe of Death and the browsers
         */
        signal_threads(ST_UNGRACEFUL);

        /* A terminating signal was received. Now join each of the
         * workers to clean them up.
         *   If the worker already exited, then the join frees
         *   their resources and returns.
         *   If the worker hasn't exited, then this blocks until
         *   they have (then cleans up).
         */
        join_workers(ts->listener, threads);
    }
    else {                      /* !one_process */
        /* remove SIGTERM from the set of blocked signals...  if one of
         * the other threads in the process needs to take us down
         * (e.g., for MaxRequestsPerChild) it will send us SIGTERM
         */
        unblock_signal(SIGTERM);
        apr_signal(SIGTERM, dummy_signal_handler);
        /* Watch for any messages from the parent over the POD */
        while (1) {
            rv = zevent_pod_check(pod);
            if (rv == ZEVENT_NORESTART) {
                /* see if termination was triggered while we slept */
                switch (terminate_mode) {
                case ST_GRACEFUL:
                    rv = ZEVENT_GRACEFUL;
                    break;
                case ST_UNGRACEFUL:
                    rv = ZEVENT_RESTART;
                    break;
                }
            }
            if (rv == ZEVENT_GRACEFUL || rv == ZEVENT_RESTART) {
                /* make sure the start thread has finished;
                 * signal_threads() and join_workers depend on that
                 */
                join_start_thread(start_thread_id);
                signal_threads(rv ==
                               ZEVENT_GRACEFUL ? ST_GRACEFUL : ST_UNGRACEFUL);
                break;
            }
        }

        /* A terminating signal was received. Now join each of the
         * workers to clean them up.
         *   If the worker already exited, then the join frees
         *   their resources and returns.
         *   If the worker hasn't exited, then this blocks until
         *   they have (then cleans up).
         */

        join_workers(ts->listener, threads);
    }

    free(threads);

    clean_child_exit(resource_shortage ? ZEVENTEXIT_CHILDSICK : 0);
}

static int make_child(int slot)
{
    int pid;

    if (slot + 1 > zevent_max_daemons_limit) {
        zevent_max_daemons_limit = slot + 1;
    }

    if (one_process) {
        set_signals();
        zevent_scoreboard_image->parent[slot].pid = getpid();
        child_main(slot);
    }

    if ((pid = fork()) == -1) {

        /* fork didn't succeed. Fix the scoreboard or else
         * it will say SERVER_STARTING forever and ever
         */
        zevent_update_child_status_from_indexes(slot, 0, SERVER_DEAD);

        /* In case system resources are maxxed out, we don't want
           Apache running away with the CPU trying to fork over and
           over and over again. */
        apr_sleep(apr_time_from_sec(10));

        return -1;
    }

    if (!pid) {
        apr_signal(SIGTERM, just_die);
        child_main(slot);

        clean_child_exit(0);
    }
    zevent_scoreboard_image->parent[slot].quiescing = 0;
    zevent_scoreboard_image->parent[slot].pid = pid;
    return 0;
}

/* start up a bunch of children */
static void startup_children(int number_to_start)
{

    int i;

    for (i = 0; number_to_start && i < zevent_daemons_limit; ++i) {

        if (zevent_scoreboard_image->parent[i].pid != 0) {
            continue;
        }
        if (make_child(i) < 0) {
            break;
        }
        --number_to_start;
    }
}


/*
 * idle_spawn_rate is the number of children that will be spawned on the
 * next maintenance cycle if there aren't enough idle servers.  It is
 * doubled up to MAX_SPAWN_RATE, and reset only when a cycle goes by
 * without the need to spawn.
 */
static int idle_spawn_rate = 1;
#ifndef MAX_SPAWN_RATE
#define MAX_SPAWN_RATE        (32)
#endif
static int hold_off_on_exponential_spawning;

static void perform_idle_server_maintenance(void)
{
    int i, j;
    int idle_thread_count;
    worker_score *ws;
    process_score *ps;
    int free_length;
    int totally_free_length = 0;
    int free_slots[MAX_SPAWN_RATE];
    int last_non_dead;
    int total_non_dead;
    int active_thread_count = 0;

    /* initialize the free_list */
    free_length = 0;

    idle_thread_count = 0;
    last_non_dead = -1;
    total_non_dead = 0;

    for (i = 0; i < zevent_daemons_limit; ++i) {
        /* Initialization to satisfy the compiler. It doesn't know
         * that zevent_threads_per_child is always > 0 */
        int status = SERVER_DEAD;
        int any_dying_threads = 0;
        int any_dead_threads = 0;
        int all_dead_threads = 1;

        if (i >= zevent_max_daemons_limit
            && totally_free_length == idle_spawn_rate)
            break;
        ps = &zevent_scoreboard_image->parent[i];
        for (j = 0; j < zevent_threads_per_child; j++) {
            ws = &zevent_scoreboard_image->servers[i][j];
            status = ws->status;

            /* XXX any_dying_threads is probably no longer needed    GLA */
            any_dying_threads = any_dying_threads ||
                (status == SERVER_GRACEFUL);
            any_dead_threads = any_dead_threads || (status == SERVER_DEAD);
            all_dead_threads = all_dead_threads &&
                (status == SERVER_DEAD || status == SERVER_GRACEFUL);

            /* We consider a starting server as idle because we started it
             * at least a cycle ago, and if it still hasn't finished starting
             * then we're just going to swamp things worse by forking more.
             * So we hopefully won't need to fork more if we count it.
             * This depends on the ordering of SERVER_READY and SERVER_STARTING.
             */
            if (ps->pid != 0) { /* XXX just set all_dead_threads in outer
                                   for loop if no pid?  not much else matters */
                if (status <= SERVER_READY &&
                        !ps->quiescing && ps->generation == zevent_my_generation) {
                    ++idle_thread_count;
                }
                if (status >= SERVER_READY && status < SERVER_GRACEFUL) {
                    ++active_thread_count;
                }
            }
        }
        if (any_dead_threads
            && totally_free_length < idle_spawn_rate
            && free_length < MAX_SPAWN_RATE
            && (!ps->pid      /* no process in the slot */
                  || ps->quiescing)) {  /* or at least one is going away */
            if (all_dead_threads) {
                /* great! we prefer these, because the new process can
                 * start more threads sooner.  So prioritize this slot
                 * by putting it ahead of any slots with active threads.
                 *
                 * first, make room by moving a slot that's potentially still
                 * in use to the end of the array
                 */
                free_slots[free_length] = free_slots[totally_free_length];
                free_slots[totally_free_length++] = i;
            }
            else {
                /* slot is still in use - back of the bus
                 */
                free_slots[free_length] = i;
            }
            ++free_length;
        }
        /* XXX if (!ps->quiescing)     is probably more reliable  GLA */
        if (!any_dying_threads) {
            last_non_dead = i;
            ++total_non_dead;
        }
    }

    if (sick_child_detected) {
        if (active_thread_count > 0) {
            /* some child processes appear to be working.  don't kill the
             * whole server.
             */
            sick_child_detected = 0;
        }
        else {
            /* looks like a basket case.  give up.
             */
            shutdown_pending = 1;
            child_fatal = 1;
            /* the child already logged the failure details */
            return;
        }
    }

    zevent_max_daemons_limit = last_non_dead + 1;

    if (idle_thread_count > max_spare_threads) {
        /* Kill off one child */
        zevent_pod_signal(pod, TRUE);
        idle_spawn_rate = 1;
    }
    else if (idle_thread_count < min_spare_threads) {
        /* terminate the free list */
        if (free_length == 0) {
            /* only report this condition once */
            static int reported = 0;

            if (!reported) {
                reported = 1;
            }
            idle_spawn_rate = 1;
        }
        else {
            if (free_length > idle_spawn_rate) {
                free_length = idle_spawn_rate;
            }
	    if (idle_spawn_rate >= 8) {
		    zevent_log_error(APLOG_MARK, NULL,
				    "server seems busy, (you may need "
				    "to increase StartServers, ThreadsPerChild "
				    "or Min/MaxSpareThreads), "
				    "spawning %d children, there are around %d idle "
				    "threads, and %d total children",
				    free_length,
				    idle_thread_count, total_non_dead);
	    }

            for (i = 0; i < free_length; ++i) {
                make_child(free_slots[i]);
            }
            /* the next time around we want to spawn twice as many if this
             * wasn't good enough, but not if we've just done a graceful
             */
            if (hold_off_on_exponential_spawning) {
                --hold_off_on_exponential_spawning;
            }
            else if (idle_spawn_rate < MAX_SPAWN_RATE) {
                idle_spawn_rate *= 2;
            }
        }
    }
    else {
        idle_spawn_rate = 1;
    }
}

static void server_main_loop(int remaining_children_to_start)
{
    int child_slot;
    apr_exit_why_e exitwhy;
    int status, processed_status;
    apr_proc_t pid;
    int i;

    while (!restart_pending && !shutdown_pending) {
        zevent_wait_or_timeout(&exitwhy, &status, &pid, pconf);
        if (pid.pid != -1) {
            processed_status = zevent_process_child_status(&pid, exitwhy, status);
            if (processed_status == ZEVENTEXIT_CHILDFATAL) {
                shutdown_pending = 1;
                child_fatal = 1;
                return;
            }
            else if (processed_status == ZEVENTEXIT_CHILDSICK) {
                /* tell perform_idle_server_maintenance to check into this
                 * on the next timer pop
                 */
                sick_child_detected = 1;
            }
            /* non-fatal death... note that it's gone in the scoreboard. */
            child_slot = find_child_by_pid(&pid);

            if (child_slot >= 0) {
                for (i = 0; i < zevent_threads_per_child; i++)
                    zevent_update_child_status_from_indexes(child_slot, i,
                                                        SERVER_DEAD
                                                        );

                zevent_scoreboard_image->parent[child_slot].pid = 0;
                zevent_scoreboard_image->parent[child_slot].quiescing = 0;
                if (processed_status == ZEVENTEXIT_CHILDSICK) {
                    /* resource shortage, minimize the fork rate */
                    idle_spawn_rate = 1;
                }
                else if (remaining_children_to_start
                         && child_slot < zevent_daemons_limit) {
                    /* we're still doing a 1-for-1 replacement of dead
                     * children with new children
                     */
                    make_child(child_slot);
                    --remaining_children_to_start;
                }
#if APR_HAS_OTHER_CHILD
            }
            else if (apr_proc_other_child_alert(&pid, APR_OC_REASON_DEATH,
                                                status) == 0) {
                /* handled */
#endif
            }
            else if (is_graceful) {
                /* Great, we've probably just lost a slot in the
                 * scoreboard.  Somehow we don't know about this child.
                 */
		    zevent_log_error(APLOG_MARK,NULL,
                             "long lost child came home! (pid %ld)",
                             (long) pid.pid);
            }
            /* Don't perform idle maintenance when a child dies,
             * only do it when there's a timeout.  Remember only a
             * finite number of children can die, and it's pretty
             * pathological for a lot to die suddenly.
             */
            continue;
        }
        else if (remaining_children_to_start) {
            /* we hit a 1 second timeout in which none of the previous
             * generation of children needed to be reaped... so assume
             * they're all done, and pick up the slack if any is left.
             */
            startup_children(remaining_children_to_start);
            remaining_children_to_start = 0;
            /* In any event we really shouldn't do the code below because
             * few of the servers we just started are in the IDLE state
             * yet, so we'd mistakenly create an extra server.
             */
            continue;
        }

        perform_idle_server_maintenance();
    }
}

static int worker_open_pod(apr_pool_t * p)
{
    apr_status_t rv;

    if (!one_process) {
        if ((rv = zevent_pod_open(p, &pod))) {
            return DONE;
        }
    }
    return OK;
}

static int worker_pre_init(apr_pool_t * p)
{
    static int restart_num = 0;
    int no_detach, foreground;
    char path[APR_PATH_MAX];
    char *pwd = NULL;
    foreground = 0;
    no_detach = 0;
    apr_status_t rv;

    one_process = iniparser_getint(d,"misc:debug",0);
    log = (char*)iniparser_getstring(d,"misc:logfile",NULL);
    if(log)
	    zevent_replace_stderr_log(p,log);

    mpm_state = ZEVENT_MPMQ_STARTING;

    
    if (++restart_num == 1) {
	    is_graceful = 0;
	    rv = apr_pollset_create(&event_pollset, 1, p,
			    APR_POLLSET_THREADSAFE);
	    if (rv != APR_SUCCESS) {
		    return -1;
	    }
	    apr_pollset_destroy(event_pollset);

	    if (!one_process && !foreground) {
                    pwd = getcwd(path,APR_PATH_MAX);
		    rv = apr_proc_detach(no_detach ? APR_PROC_DETACH_FOREGROUND
				    : APR_PROC_DETACH_DAEMONIZE);

		    if (rv != APR_SUCCESS) {
			    return -1;
		    }
		    rv = chdir(pwd);
		    if(log)
			    zevent_replace_stderr_log(p,log);
	    }
	    parent_pid = zevent_my_pid = getpid();
    }

    zevent_extended_status = 0;

    zevent_daemons_to_start = iniparser_getint(d,"mpm_event:StartServers",-1);
    min_spare_threads = iniparser_getint(d,"mpm_event:MinSpareThreads",-1);
    max_spare_threads = iniparser_getint(d,"mpm_event:MaxSpareThreads",-1);
    server_limit = iniparser_getint(d,"mpm_event:ServerLimit",-1);
    zevent_daemons_limit = server_limit;
    thread_limit = iniparser_getint(d,"mpm_event:ThreadLimit",-1);
    zevent_threads_per_child = iniparser_getint(d,"mpm_event:ThreadsPerChild",-1);

    
    return OK;
}

ZEVENT_DECLARE(int) zevent_init(const char *inifile,apr_pool_t **pglobal)
{
	apr_initialize();
//	atexit(apr_terminate);
	apr_pool_create(pglobal,NULL);
        apr_hook_global_pool = *pglobal;
	//apr_proc_detach(1);
	d = (dictionary*)iniparser_load(inifile);
	if(!d){
		apr_pool_destroy(*pglobal);
		apr_terminate();
		return -1;
	}
	zevent_open_stderr_log(*pglobal);
	if(worker_pre_init(*pglobal) != OK || worker_open_pod(*pglobal) != OK)
		return -1;
	return 0;
}

ZEVENT_DECLARE(int) zevent_fini(apr_pool_t **pglobal)
{
	zevent_run_zevent_fini(*pglobal);
	zevent_log_close();
	apr_pool_destroy(*pglobal);
	apr_terminate();
	iniparser_freedict(d);
	return 0;
}

static int set_listener(apr_pool_t *p,dictionary *d)
{
    const char *port =(char *)iniparser_getstring(d,"network:port",NULL);
    const char *protocol = (char *)iniparser_getstring(d,"network:protocol",NULL);

    const char * argv[2]={port,protocol};
    int argc = 2;
    zevent_set_listener(p,argc,argv);
    if(zevent_open_listeners(p) == -1)
	    return -1;
    return 0;
}

ZEVENT_DECLARE(int) zevent_run(apr_pool_t * p)
{
    int remaining_children_to_start;

    if(set_listener(p,d)<0)
    {
	    zevent_close_listeners();
	    return -1;
    }

    pconf = p;
    first_server_limit = server_limit;
    first_thread_limit = thread_limit;

    if (!is_graceful) {

	    if(zevent_create_scoreboard(p,SB_SHARED)!=OK)
	    {

		    mpm_state = ZEVENT_MPMQ_STOPPING;
		    return -1;
	    }
	    zevent_scoreboard_image->global->running_generation = zevent_my_generation;
    }

    set_signals();

    zevent_run_zevent_init(p);
    /* Don't thrash... */
    if (max_spare_threads < min_spare_threads + zevent_threads_per_child)
        max_spare_threads = min_spare_threads + zevent_threads_per_child;

    /* If we're doing a graceful_restart then we're going to see a lot
     * of children exiting immediately when we get into the main loop
     * below (because we just sent them ZEVENT_SIG_GRACEFUL).  This happens pretty
     * rapidly... and for each one that exits we'll start a new one until
     * we reach at least daemons_min_free.  But we may be permitted to
     * start more than that, so we'll just keep track of how many we're
     * supposed to start up without the 1 second penalty between each fork.
     */
    remaining_children_to_start = zevent_daemons_to_start;
    if (remaining_children_to_start > zevent_daemons_limit) {
        remaining_children_to_start = zevent_daemons_limit;
    }
    if (!is_graceful) {
        startup_children(remaining_children_to_start);
        remaining_children_to_start = 0;
    }
    else {
        /* give the system some time to recover before kicking into
         * exponential mode */
        hold_off_on_exponential_spawning = 10;
    }

    restart_pending = shutdown_pending = 0;
    mpm_state = ZEVENT_MPMQ_RUNNING;

    server_main_loop(remaining_children_to_start);
    mpm_state = ZEVENT_MPMQ_STOPPING;

    if (shutdown_pending && !is_graceful) {
        /* Time to shut down:
         * Kill child processes, tell them to call child_exit, etc...
         */

        zevent_pod_killpg(pod, zevent_daemons_limit, FALSE);

        zevent_reclaim_child_processes(1);  /* Start with SIGTERM */


        if (!child_fatal) {
		 zevent_log_error(APLOG_MARK,NULL,"caught SIGTERM, shutting down");
        }
        return 1;
    } else if (shutdown_pending) {
        /* Time to gracefully shut down:
         * Kill child processes, tell them to call child_exit, etc...
         */
        int active_children;
        int index;

        /* Close our listeners, and then ask our children to do same */
        zevent_close_listeners();
        zevent_pod_killpg(pod, zevent_daemons_limit, TRUE);
        zevent_relieve_child_processes();

        if (!child_fatal) {
            /* cleanup pid file on normal shutdown */
		zevent_log_error(APLOG_MARK, NULL,
                         "caught " ZEVENT_SIG_GRACEFUL_STOP_STRING
                         ", shutting down gracefully");
        }

                /* Don't really exit until each child has finished */
        shutdown_pending = 0;
        do {
            /* Pause for a second */
            apr_sleep(apr_time_from_sec(1));

            /* Relieve any children which have now exited */
            zevent_relieve_child_processes();

            active_children = 0;
            for (index = 0; index < zevent_daemons_limit; ++index) {
                if (zevent_mpm_safe_kill(MPM_CHILD_PID(index), 0) == APR_SUCCESS) {
                    active_children = 1;
                    /* Having just one child is enough to stay around */
                    break;
                }
            }
        } while (!shutdown_pending && active_children);

        /* We might be here because we received SIGTERM, either
         * way, try and make sure that all of our processes are
         * really dead.
         */
        zevent_pod_killpg(pod, zevent_daemons_limit, FALSE);
        zevent_reclaim_child_processes(1);

        return 1;
    }

    /* we've been told to restart */
    apr_signal(SIGHUP, SIG_IGN);

    if (one_process) {
        /* not worth thinking about */
        return 1;
    }

    /* advance to the next generation */
    /* XXX: we really need to make sure this new generation number isn't in
     * use by any of the children.
     */
    ++zevent_my_generation;
    zevent_scoreboard_image->global->running_generation = zevent_my_generation;


    if (is_graceful) {
        /* wake up the children...time to die.  But we'll have more soon */
        zevent_pod_killpg(pod, zevent_daemons_limit, TRUE);


        /* This is mostly for debugging... so that we know what is still
         * gracefully dealing with existing request.
         */

    }
    else {
        /* Kill 'em all.  Since the child acts the same on the parents SIGTERM
         * and a SIGHUP, we may as well use the same signal, because some user
         * pthreads are stealing signals from us left and right.
         */
        zevent_pod_killpg(pod, zevent_daemons_limit, FALSE);

        zevent_reclaim_child_processes(1);  /* Start with SIGTERM */
    }

    return 0;
}

