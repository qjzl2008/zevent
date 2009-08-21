#ifndef ZEVENT_LOG_H
#define ZEVENT_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apr_thread_proc.h"
#include "zevent_config.h"

#define APLOG_MARK	__FILE__,__LINE__
#define MAX_LOG_LEN (8192)

ZEVENT_DECLARE(apr_status_t) zevent_open_log(apr_pool_t *p,const char *filename);

ZEVENT_DECLARE(apr_status_t) zevent_open_stderr_log(apr_pool_t *p);

ZEVENT_DECLARE(apr_status_t) zevent_replace_stderr_log(apr_pool_t *p,
                                               const char *filename);
ZEVENT_DECLARE(void) zevent_log_error(const char *file, 
		             int line,
                             apr_pool_t *p, 
                             const char *fmt, ...)
			    __attribute__((format(printf,4,5)));


ZEVENT_DECLARE(void) zevent_log_close();
#ifdef __cplusplus
}
#endif

#endif
