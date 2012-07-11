#ifndef ZNET_LOG_H
#define ZNET_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_EMERG   0   /* system in unusable */
#define LOG_ALERT   1   /* action must be taken immediately */
#define LOG_CRIT    2   /* critical conditions */
#define LOG_ERR     3   /* error conditions */
#define LOG_WARN    4   /* warning conditions */
#define LOG_NOTICE  5   /* normal but significant condition (default) */
#define LOG_INFO    6   /* informational */
#define LOG_DEBUG   7   /* debug messages */
#define LOG_VERB    8   /* verbose messages */
#define LOG_VVERB   9   /* verbose messages on crack */
#define LOG_VVVERB  10  /* verbose messages on ganga */
#define LOG_PVERB   11  /* periodic verbose messages on crack */

#define LOG_MARK  __FILE__,__LINE__
#define MAX_LOG_LEN (4096)
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))

typedef struct log_t log_t;
/*
 * log_stderr   - log to stderr
 * loga         - log always
 * loga_hexdump - log hexdump always
 * log_error    - error log messages
 * log_warn     - warning log messages
 * log_panic    - log messages followed by a panic
 * ...
 * log_debug    - debug log messages based on a log level
 * log_hexdump  - hexadump -C of a log buffer
 */
#if defined DEBUG_LOG && DEBUG_LOG == 1

#define log_debug(l,_level, ...) do {                                         \
    if (log_loggable((log_t*)l,_level) != 0) {                                        \
        _log((log_t *)l,__FILE__, __LINE__, __VA_ARGS__);                           \
    }                                                                       \
} while (0)

#define log_hexdump(l,_level, _data, _datalen, ...) do {                      \
    if (log_loggable((log_t*)l,_level) != 0) {                                        \
        _log((log_t*)l,__FILE__,__LINE__,  __VA_ARGS__);                            \
        _log_hexdump((log_t*)l,(char *)(_data), (int)(_datalen));                     \
    }                                                                       \
} while (0)

#else

#define log_debug(l,_level, ...)
#define log_hexdump(l,_level, _data, _datalen, ...)

#endif

#define log_stderr(...) do {                                                \
    _log_stderr(__VA_ARGS__);                                               \
} while (0)

#define loga(l,...) do {                                                      \
    _log((log_t*)l,__FILE__, __LINE__, __VA_ARGS__);                               \
} while (0)

#define loga_hexdump(l,_data, _datalen, ...) do {                             \
    _log((log_t*)l,__FILE__,__LINE__, __VA_ARGS__);                                \
    _log_hexdump((log_t*)l,(char *)(_data), (int)(_datalen));                         \
} while (0)                                                                 \


#define log_error(l,...) do {                                                 \
    if (log_loggable((log_t*)l,LOG_ERR) != 0) {                                     \
        _log((log_t*)l,__FILE__, __LINE__, __VA_ARGS__);                           \
    }                                                                       \
} while (0)

#define log_info(l,...) do {                                                 \
    if (log_loggable((log_t*)l,LOG_INFO) != 0) {                                     \
        _log((log_t*)l,__FILE__, __LINE__, __VA_ARGS__);                           \
    }                                                                       \
} while (0)

#define log_warn(l,...) do {                                                  \
    if (log_loggable((log_t*)l,LOG_WARN) != 0) {                                      \
        _log((log_t*)l,__FILE__, __LINE__, __VA_ARGS__);                           \
    }                                                                       \
} while (0)

int log_open(log_t **log,const char *filename);
void log_close(log_t *log);
void log_level_up(log_t *log);
void log_level_down(log_t *log);
void log_level_set(log_t *log,int level);
int log_loggable(log_t *log,int level);
void _log(log_t *log,const char *file, 
		             int line,
                             const char *fmt, ...)__attribute__((format(printf,4,5)));
void _log_stderr(const char *fmt, ...);
void _log_hexdump(log_t *log,char *data, int datalen);

#ifdef __cplusplus
}
#endif

#endif
