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

/**
 * @file  zevent_mpm.h
 * @brief Multi-Processing Modules functions
 *
 * @{
 */

#ifndef ZEVENT_MPM_COMMON_H
#define ZEVENT_MPM_COMMON_H

#include "zevent_config.h"

#if APR_HAVE_NETINET_TCP_H
#include <netinet/tcp.h>    /* for TCP_NODELAY */
#endif

#include "scoreboard.h"

#ifdef __cplusplus
extern "C" {
#endif

       
/* Signal used to gracefully restart */
#define ZEVENT_SIG_GRACEFUL SIGUSR1

/* Signal used to gracefully restart (without SIG prefix) */
#define ZEVENT_SIG_GRACEFUL_SHORT USR1

/* Signal used to gracefully restart (as a quoted string) */
#define ZEVENT_SIG_GRACEFUL_STRING "SIGUSR1"

/* Signal used to gracefully stop */
#define ZEVENT_SIG_GRACEFUL_STOP SIGWINCH

/* Signal used to gracefully stop (without SIG prefix) */
#define ZEVENT_SIG_GRACEFUL_STOP_SHORT WINCH

/* Signal used to gracefully stop (as a quoted string) */
#define ZEVENT_SIG_GRACEFUL_STOP_STRING "SIGWINCH"


/**
 * Make sure all child processes that have been spawned by the parent process
 * have died.  This includes process registered as "other_children".
 * @warning This is only defined if the MPM defines 
 *          ZEVENT_MPM_WANT_RECLAIM_CHILD_PROCESSES
 * @param terminate Either 1 or 0.  If 1, send the child processes SIGTERM
 *        each time through the loop.  If 0, give the process time to die
 *        on its own before signalling it.
 * @tip This function requires that some macros are defined by the MPM: <pre>
 *  MPM_CHILD_PID -- Get the pid from the specified spot in the scoreboard
 *  MPM_NOTE_CHILD_KILLED -- Note the child died in the scoreboard
 * </pre>
 * @tip The MPM child processes which are reclaimed are those listed
 * in the scoreboard as well as those currently registered via
 * zevent_register_extra_mpm_process().
 */
void zevent_reclaim_child_processes(int terminate);

/**
 * Catch any child processes that have been spawned by the parent process
 * which have exited. This includes processes registered as "other_children".
 * @warning This is only defined if the MPM defines 
 *          ZEVENT_MPM_WANT_RECLAIM_CHILD_PROCESSES
 * @tip This function requires that some macros are defined by the MPM: <pre>
 *  MPM_CHILD_PID -- Get the pid from the specified spot in the scoreboard
 *  MPM_NOTE_CHILD_KILLED -- Note the child died in the scoreboard
 * </pre>
 * @tip The MPM child processes which are relieved are those listed
 * in the scoreboard as well as those currently registered via
 * zevent_register_extra_mpm_process().
 */
void zevent_relieve_child_processes(void);

/**
 * Tell zevent_reclaim_child_processes() and ap_relieve_child_processes() about 
 * an MPM child process which has no entry in the scoreboard.
 * @warning This is only defined if the MPM defines
 *          ZEVENT_MPM_WANT_RECLAIM_CHILD_PROCESSES
 * @param pid The process id of an MPM child process which should be
 * reclaimed when zevent_reclaim_child_processes() is called.
 * @tip If an extra MPM child process terminates prior to calling
 * zevent_reclaim_child_processes(), remove it from the list of such processes
 * by calling zevent_unregister_extra_mpm_process().
 */
void zevent_register_extra_mpm_process(pid_t pid);

/**
 * Unregister an MPM child process which was previously registered by a
 * call to zevent_register_extra_mpm_process().
 * @warning This is only defined if the MPM defines
 *          ZEVENT_MPM_WANT_RECLAIM_CHILD_PROCESSES
 * @param pid The process id of an MPM child process which no longer needs to
 * be reclaimed.
 * @return 1 if the process was found and removed, 0 otherwise
 */
int zevent_unregister_extra_mpm_process(pid_t pid);

/**
 * Safely signal an MPM child process, if the process is in the
 * current process group.  Otherwise fail.
 * @param pid the process id of a child process to signal
 * @param sig the signal number to send
 * @return APR_SUCCESS if signal is sent, otherwise an error as per kill(3);
 * APR_EINVAL is returned if passed either an invalid (< 1) pid, or if
 * the pid is not in the current process group
 */
apr_status_t zevent_mpm_safe_kill(pid_t pid, int sig);

/**
 * Determine if any child process has died.  If no child process died, then
 * this process sleeps for the amount of time specified by the MPM defined
 * macro SCOREBOARD_MAINTENANCE_INTERVAL.
 * @param status The return code if a process has died
 * @param ret The process id of the process that died
 * @param p The pool to allocate out of
 */
void zevent_wait_or_timeout(apr_exit_why_e *status, int *exitcode, apr_proc_t *ret, 
                        apr_pool_t *p);

/**
 * Log why a child died to the error log, if the child died without the
 * parent signalling it.
 * @param pid The child that has died
 * @param status The status returned from zevent_wait_or_timeout
 * @return 0 on success, APEXIT_CHILDFATAL if MPM should terminate
 */
int zevent_process_child_status(apr_proc_t *pid, apr_exit_why_e why, int status);

/**
 * Convert a username to a numeric ID
 * @param name The name to convert
 * @return The user id corresponding to a name
 * @deffunc uid_t zevent_uname2id(const char *name)
 */
ZEVENT_DECLARE(uid_t) zevent_uname2id(const char *name);

/**
 * Convert a group name to a numeric ID
 * @param name The name to convert
 * @return The group id corresponding to a name
 * @deffunc gid_t zevent_gname2id(const char *name)
 */
ZEVENT_DECLARE(gid_t) zevent_gname2id(const char *name);


#ifdef __cplusplus
}
#endif

#endif /* !ZEVENT_MPM_COMMON_H */
/** @} */
