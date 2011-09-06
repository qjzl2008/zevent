#ifndef ZNET_LOG_H
#define ZNET_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_MARK  __FILE__,__LINE__
#define MAX_LOG_LEN (8192)

int open_log(const char *filename);

void log_error(const char *file, 
		             int line,
                             const char *fmt, ...)
			    __attribute__((format(printf,3,4)));

void log_close();
#ifdef __cplusplus
}
#endif

#endif
