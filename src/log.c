#include "apr.h"
#include "apr_general.h"        /* for signal stuff */
#include "apr_strings.h"
#include "apr_errno.h"

#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include "apr_want.h"

#if APR_HAVE_STDARG_H
#include <stdarg.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "log.h"

static apr_file_t *logfile = NULL;

static void log_child_errfn(apr_pool_t *pool, apr_status_t err,
                            const char *description)
{
    zevent_log_error(APLOG_MARK, NULL,"%s",
			description);
}

static int log_child(apr_pool_t *p, const char *progname,
                     apr_file_t **fpin, int dummy_stderr)
{
    /* Child process code for 'ErrorLog "|..."';
     * may want a common framework for this, since I expect it will
     * be common for other foo-loggers to want this sort of thing...
     */
    apr_status_t rc;
    apr_procattr_t *procattr;
    apr_proc_t *procnew;
    apr_file_t *outfile, *errfile;

    if (((rc = apr_procattr_create(&procattr, p)) == APR_SUCCESS)
        && ((rc = apr_procattr_cmdtype_set(procattr,
                                           APR_SHELLCMD_ENV)) == APR_SUCCESS)
        && ((rc = apr_procattr_io_set(procattr,
                                      APR_FULL_BLOCK,
                                      APR_NO_PIPE,
                                      APR_NO_PIPE)) == APR_SUCCESS)
        && ((rc = apr_procattr_error_check_set(procattr, 1)) == APR_SUCCESS)
        && ((rc = apr_procattr_child_errfn_set(procattr, log_child_errfn)) 
                == APR_SUCCESS)) {
        char **args;
        const char *pname;

        apr_tokenize_to_argv(progname, &args, p);
        pname = apr_pstrdup(p, args[0]);
        procnew = (apr_proc_t *)apr_pcalloc(p, sizeof(*procnew));

        if ((rc = apr_file_open_stdout(&outfile, p)) == APR_SUCCESS) {
            rc = apr_procattr_child_out_set(procattr, outfile, NULL);
            if (dummy_stderr)
                rc = apr_procattr_child_err_set(procattr, outfile, NULL);
            else if ((rc = apr_file_open_stderr(&errfile, p)) == APR_SUCCESS)
                rc = apr_procattr_child_err_set(procattr, errfile, NULL);
        }

        rc = apr_proc_create(procnew, pname, (const char * const *)args,
                             NULL, procattr, p);

        if (rc == APR_SUCCESS) {
            apr_pool_note_subprocess(p, procnew, APR_KILL_AFTER_TIMEOUT);
            (*fpin) = procnew->in;
            /* read handle to pipe not kept open, so no need to call
             * close_handle_in_child()
             */
        }
    }

    return rc;
}


ZEVENT_DECLARE(apr_status_t) zevent_open_log(apr_pool_t *p,const char *filename)
{
	apr_status_t rc;
	if(!filename){
		return APR_EBADPATH;
	}

	if (*filename == '|') {
		int is_main = 1;

		/* Spawn a new child logger.  If this is the main server,
		 * the new child must use a dummy stderr since the current
		 * stderr might be a pipe to the old logger.  Otherwise, the
		 * child inherits the parents stderr. */
		rc = log_child(p, filename + 1, &logfile, is_main);
		if (rc != APR_SUCCESS) {
			zevent_log_error(APLOG_MARK, NULL,
					"Couldn't start ErrorLog process");
			return DONE;
		}
	}
	else{

		if ((rc = apr_file_open(&logfile, filename,
						APR_APPEND | APR_WRITE | APR_CREATE | APR_LARGEFILE,
						APR_OS_DEFAULT, p)) != APR_SUCCESS) {
			return rc;
		}
	}

	return rc;

}

ZEVENT_DECLARE(apr_status_t) zevent_open_stderr_log(apr_pool_t *p)
{
	apr_file_open_stderr(&logfile,p);
	return APR_SUCCESS;
}

ZEVENT_DECLARE(apr_status_t) zevent_replace_stderr_log(apr_pool_t *p,
                                               const char *filename)
{
    apr_file_t *stderr_file;
    apr_status_t rc;
    if (!filename) {
        return APR_EBADPATH;

    }
    if ((rc = apr_file_open(&stderr_file, filename,
                            APR_APPEND | APR_WRITE | APR_CREATE | APR_LARGEFILE,
                            APR_OS_DEFAULT, p)) != APR_SUCCESS) {
        return rc;
    }
    if ((rc = apr_file_open_stderr(&logfile, p)) 
            == APR_SUCCESS) {
        apr_file_flush(logfile);
        if ((rc = apr_file_dup2(logfile, stderr_file, p)) 
                == APR_SUCCESS) {
            apr_file_close(stderr_file);
        }
    }

    return rc;
}


static void log_error_core(const char *file, int line,apr_pool_t *pool,
                           const char *fmt, va_list args)
{
	char errstr[MAX_LOG_LEN];
	apr_size_t len=0;

	if (file) {
#if defined(_OSD_POSIX) || defined(WIN32) || defined(__MVS__)
		char tmp[256];
		char *e = strrchr(file, '/');
#ifdef WIN32
		if (!e) {
			e = strrchr(file, '\\');
		}
#endif

		/* In OSD/POSIX, the compiler returns for __FILE__
		 * a string like: __FILE__="*POSIX(/usr/include/stdio.h)"
		 * (it even returns an absolute path for sources in
		 * the current directory). Here we try to strip this
		 * down to the basename.
		 */
		if (e != NULL && e[1] != '\0') {
			apr_snprintf(tmp, sizeof(tmp), "%s", &e[1]);
			e = &tmp[strlen(tmp)-1];
			if (*e == ')') {
				*e = '\0';
			}
			file = tmp;
		}
#else /* _OSD_POSIX || WIN32 */
		const char *p;
		/* On Unix, __FILE__ may be an absolute path in a
		 * VPATH build. */
		if (file[0] == '/' && (p = strrchr(file, '/')) != NULL) {
			file = p + 1;
		}
#endif /*_OSD_POSIX || WIN32 */
		char time_str[APR_CTIME_LEN];                                          
		apr_ctime(time_str, apr_time_now());
		len += apr_snprintf(errstr + len, MAX_LOG_LEN - len,
				"[%s]%s(%d): ",time_str,file, line);
	}

	len += apr_vsnprintf(errstr+len, MAX_LOG_LEN-len, fmt, args);

	if (logfile) {
		/* Truncate for the terminator (as apr_snprintf does) */
		if (len > MAX_LOG_LEN - sizeof(APR_EOL_STR)) {
			len = MAX_LOG_LEN - sizeof(APR_EOL_STR);
		}
		strcpy(errstr + len, APR_EOL_STR);
		apr_file_puts(errstr, logfile);
		apr_file_flush(logfile);
	}
}

ZEVENT_DECLARE(void) zevent_log_error(const char *file, int line, apr_pool_t *p,
                               const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_error_core(file, line, p, fmt, args);
    va_end(args);
}

ZEVENT_DECLARE(void) zevent_log_close()
{
	if(logfile)
		apr_file_close(logfile);
}

