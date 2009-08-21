/* The purpose of this file is to store the code that MOST mpm's will need
 * this does not mean a function only goes into this file if every MPM needs
 * it.  It means that if a function is needed by more than one MPM, and
 * future maintenance would be served by making the code common, then the
 * function belongs here.
 *
 * This is going in src/main because it is not platform specific, it is
 * specific to multi-process servers, but NOT to Unix.  Which is why it
 * does not belong in src/os/unix
 */

#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>

#include "apr.h"
#include "apr_thread_proc.h"
#include "apr_signal.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "apr_getopt.h"
#include "apr_allocator.h"

#include "zevent_mpm.h"
#include "zevent.h"
#include "log.h"

#include "scoreboard.h"

typedef enum {DO_NOTHING, SEND_SIGTERM, SEND_SIGKILL, GIVEUP} action_t;

typedef struct extra_process_t {
    struct extra_process_t *next;
    pid_t pid;
} extra_process_t;

static extra_process_t *extras;

void zevent_register_extra_mpm_process(pid_t pid)
{
    extra_process_t *p = (extra_process_t *)malloc(sizeof(extra_process_t));

    p->next = extras;
    p->pid = pid;
    extras = p;
}

int zevent_unregister_extra_mpm_process(pid_t pid)
{
    extra_process_t *cur = extras;
    extra_process_t *prev = NULL;

    while (cur && cur->pid != pid) {
        prev = cur;
        cur = cur->next;
    }

    if (cur) {
        if (prev) {
            prev->next = cur->next;
        }
        else {
            extras = cur->next;
        }
        free(cur);
        return 1; /* found */
    }
    else {
        /* we don't know about any such process */
        return 0;
    }
}

static int reclaim_one_pid(pid_t pid, action_t action)
{
    apr_proc_t proc;
    apr_status_t waitret;

    /* Ensure pid sanity. */
    if (pid < 1) {
        return 1;
    }        

    proc.pid = pid;
    waitret = apr_proc_wait(&proc, NULL, NULL, APR_NOWAIT);
    if (waitret != APR_CHILD_NOTDONE) {
        return 1;
    }

    switch(action) {
    case DO_NOTHING:
        break;

    case SEND_SIGTERM:
        /* ok, now it's being annoying */
        kill(pid, SIGTERM);
        break;

    case SEND_SIGKILL:
#ifndef BEOS
        kill(pid, SIGKILL);
#else
        /* sending a SIGKILL kills the entire team on BeOS, and as
         * httpd thread is part of that team it removes any chance
         * of ever doing a restart.  To counter this I'm changing to
         * use a kinder, gentler way of killing a specific thread
         * that is just as effective.
         */
        kill_thread(pid);
#endif
        break;

    case GIVEUP:
        /* gave it our best shot, but alas...  If this really
         * is a child we are trying to kill and it really hasn't
         * exited, we will likely fail to bind to the port
         * after the restart.
         */
        break;
    }

    return 0;
}

void zevent_reclaim_child_processes(int terminate)
{
    apr_time_t waittime = 1024 * 16;
    int i;
    extra_process_t *cur_extra;
    int not_dead_yet;
    int max_daemons = 16;
    zevent_mpm_query(ZEVENT_MPMQ_MAX_DAEMON_USED, &max_daemons);
    apr_time_t starttime = apr_time_now();
    /* this table of actions and elapsed times tells what action is taken
     * at which elapsed time from starting the reclaim
     */
    struct {
        action_t action;
        apr_time_t action_time;
    } action_table[] = {
        {DO_NOTHING, 0}, /* dummy entry for iterations where we reap
                          * children but take no action against
                          * stragglers
                          */
        {SEND_SIGTERM, apr_time_from_sec(3)},
        {SEND_SIGTERM, apr_time_from_sec(5)},
        {SEND_SIGTERM, apr_time_from_sec(7)},
        {SEND_SIGKILL, apr_time_from_sec(9)},
        {GIVEUP,       apr_time_from_sec(10)}
    };
    int cur_action;      /* index of action we decided to take this
                          * iteration
                          */
    int next_action = 1; /* index of first real action */


    do {
        apr_sleep(waittime);
        /* don't let waittime get longer than 1 second; otherwise, we don't
         * react quickly to the last child exiting, and taking action can
         * be delayed
         */
        waittime = waittime * 4;
        if (waittime > apr_time_from_sec(1)) {
            waittime = apr_time_from_sec(1);
        }

        /* see what action to take, if any */
        if (action_table[next_action].action_time <= apr_time_now() - starttime) {
            cur_action = next_action;
            ++next_action;
        }
        else {
            cur_action = 0; /* nothing to do */
        }

        /* now see who is done */
        not_dead_yet = 0;
        for (i = 0; i < max_daemons; ++i) {
            pid_t pid = MPM_CHILD_PID(i);

            if (pid == 0) {
                continue; /* not every scoreboard entry is in use */
            }

            if (reclaim_one_pid(pid, action_table[cur_action].action)) {
                MPM_NOTE_CHILD_KILLED(i);
            }
            else {

                ++not_dead_yet;
            }

        }

        cur_extra = extras;
        while (cur_extra) {
            extra_process_t *next = cur_extra->next;

            if (reclaim_one_pid(cur_extra->pid, action_table[cur_action].action)) {
                zevent_unregister_extra_mpm_process(cur_extra->pid);
            }
            else {
                ++not_dead_yet;
            }
            cur_extra = next;
        }

#if APR_HAS_OTHER_CHILD
        apr_proc_other_child_refresh_all(APR_OC_REASON_RESTART);
#endif

    } while (not_dead_yet > 0 &&
             action_table[cur_action].action != GIVEUP);
}

void zevent_relieve_child_processes(void)
{
    int i;
    extra_process_t *cur_extra;
    int max_daemons=16;

    zevent_mpm_query(ZEVENT_MPMQ_MAX_DAEMON_USED, &max_daemons);

    /* now see who is done */
    for (i = 0; i < max_daemons; ++i) {
        pid_t pid = MPM_CHILD_PID(i);

        if (pid == 0) {
            continue; /* not every scoreboard entry is in use */
        }

        if (reclaim_one_pid(pid, DO_NOTHING)) {
            MPM_NOTE_CHILD_KILLED(i);
        }
    }

    cur_extra = extras;
    while (cur_extra) {
        extra_process_t *next = cur_extra->next;

        if (reclaim_one_pid(cur_extra->pid, DO_NOTHING)) {
            zevent_unregister_extra_mpm_process(cur_extra->pid);
        }
        cur_extra = next;
    }
}

/* Before sending the signal to the pid this function verifies that
 * the pid is a member of the current process group; either using
 * apr_proc_wait(), where waitpid() guarantees to fail for non-child
 * processes; or by using getpgid() directly, if available. */
apr_status_t zevent_mpm_safe_kill(pid_t pid, int sig)
{
    pid_t pg;

    /* Ensure pid sanity. */
    if (pid < 1) {
        return APR_EINVAL;
    }

    pg = getpgid(pid);    
    if (pg == -1) {
        /* Process already dead... */
        return errno;
    }

    if (pg != getpgrp()) {
        return APR_EINVAL;
    }

    return kill(pid, sig) ? errno : APR_SUCCESS;
}


/* number of calls to wait_or_timeout between writable probes */
#ifndef INTERVAL_OF_WRITABLE_PROBES
#define INTERVAL_OF_WRITABLE_PROBES 10
#endif
static int wait_or_timeout_counter;

void zevent_wait_or_timeout(apr_exit_why_e *status, int *exitcode, apr_proc_t *ret,
                        apr_pool_t *p)
{
    apr_status_t rv;

    ++wait_or_timeout_counter;
    if (wait_or_timeout_counter == INTERVAL_OF_WRITABLE_PROBES) {
        wait_or_timeout_counter = 0;
        //zevent_run_monitor(p);
    }

    rv = apr_proc_wait_all_procs(ret, exitcode, status, APR_NOWAIT, p);
    if (APR_STATUS_IS_EINTR(rv)) {
        ret->pid = -1;
        return;
    }

    if (APR_STATUS_IS_CHILD_DONE(rv)) {
        return;
    }

#ifdef NEED_WAITPID
    if ((ret = rezevent_children(exitcode, status)) > 0) {
        return;
    }
#endif

    apr_sleep(SCOREBOARD_MAINTENANCE_INTERVAL);
    ret->pid = -1;
    return;
}

int zevent_process_child_status(apr_proc_t *pid, apr_exit_why_e why, int status)
{
    int signum = status;
    /* Child died... if it died due to a fatal error,
     * we should simply bail out.  The caller needs to
     * check for bad rc from us and exit, running any
     * appropriate cleanups.
     *
     * If the child died due to a resource shortage,
     * the parent should limit the rate of forking
     */
    if (APR_PROC_CHECK_EXIT(why)) {
        if (status == ZEVENTEXIT_CHILDSICK) {
            return status;
        }

        if (status == ZEVENTEXIT_CHILDFATAL) {
            return ZEVENTEXIT_CHILDFATAL;
        }

        return 0;
    }

    if (APR_PROC_CHECK_SIGNALED(why)) {
        switch (signum) {
        case SIGTERM:
        case SIGHUP:
        case ZEVENT_SIG_GRACEFUL:
        case SIGKILL:
            break;

        default:
            if (APR_PROC_CHECK_CORE_DUMP(why)) {
		    ;
            }
            else {
		    ;
            }
        }
    }
    return 0;
}

#if defined(TCP_NODELAY) && !defined(MPE) && !defined(TPF)
void zevent_sock_disable_nagle(apr_socket_t *s)
{
    /* The Nagle algorithm says that we should delay sending partial
     * packets in hopes of getting more data.  We don't want to do
     * this; we are not telnet.  There are bad interactions between
     * persistent connections and Nagle's algorithm that have very severe
     * performance penalties.  (Failing to disable Nagle is not much of a
     * problem with simple HTTP.)
     *
     * In spite of these problems, failure here is not a shooting offense.
     */
    apr_status_t status = apr_socket_opt_set(s, APR_TCP_NODELAY, 1);

    if (status != APR_SUCCESS) {
	    ;
    }
}
#endif

ZEVENT_DECLARE(uid_t) zevent_uname2id(const char *name)
{
    struct passwd *ent;

    if (name[0] == '#')
        return (atoi(&name[1]));

    if (!(ent = getpwnam(name))) {
        exit(1);
    }

    return (ent->pw_uid);
}

ZEVENT_DECLARE(gid_t) zevent_gname2id(const char *name)
{
    struct group *ent;

    if (name[0] == '#')
        return (atoi(&name[1]));

    if (!(ent = getgrnam(name))) {
        exit(1);
    }

    return (ent->gr_gid);
}

int initgroups(const char *name, gid_t basegid)
{
#if defined(QNX) || defined(MPE) || defined(BEOS) || defined(_OSD_POSIX) || defined(TPF) || defined(__TANDEM) || defined(OS2) || defined(WIN32) || defined(NETWARE)
/* QNX, MPE and BeOS do not appear to support supplementary groups. */
    return 0;
#else /* ndef QNX */
    gid_t groups[NGROUPS_MAX];
    struct group *g;
    int index = 0;

    setgrent();

    groups[index++] = basegid;

    while (index < NGROUPS_MAX && ((g = getgrent()) != NULL)) {
        if (g->gr_gid != basegid) {
            char **names;

            for (names = g->gr_mem; *names != NULL; ++names) {
                if (!strcmp(*names, name))
                    groups[index++] = g->gr_gid;
            }
        }
    }

    endgrent();

    return setgroups(index, groups);
#endif /* def QNX */
}

