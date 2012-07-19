#ifndef REDIS_UTIL_H
#define REDIS_UTIL_H
#include "hiredis.h"
#include "redis_pool.h"

redis_svr_cfg rcfg;

typedef struct{
    int len;
    char *data;
}reply_data_t;

int redis_init(void);
int redis_fini(void);
int exec_redis_cmd(const char *format, ...);
int getint_redis_cmd(int *rv,const char *format, ...);
int getint_redis_strcmd(int *rv,const char *cmd);
int getstr_redis_cmd(reply_data_t *reply_data,const char *format, ...);
int getstrings_redis_cmd(reply_data_t **reply_data,int *num,const char *format, ...);
int getstrings_redis_strcmd(reply_data_t **reply_data,int *num,const char *cmd);
#endif
