#ifndef ZNET_LOG_H
#define ZNET_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_MARK  __FILE__,__LINE__
#define MAX_LOG_LEN (8192)

typedef struct log_t log_t;

int open_log(log_t **log,const char *filename);

void log_error(log_t *log,const char *file, 
		             int line,
                             const char *fmt, ...)
			    __attribute__((format(printf,4,5)));

void log_close(log_t *log);
#ifdef __cplusplus
}
#endif

#endif
