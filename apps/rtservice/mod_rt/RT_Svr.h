#ifndef __RT_SVR_H
#define __RT_SVR_H

#include <time.h>

#define KEY_LEN 256
#define VALUE_LEN 768
#define MAX_VALUE_LEN 8192
#define TMP_BUF_LEN 1024

typedef struct data
{
	char value[MAX_VALUE_LEN];
	time_t curTime;
}DATA,*PDATA;

typedef struct store_t
{
	char dbname[KEY_LEN];
	char key[KEY_LEN];
	DATA data;
}STORE,*PSTORE;

#ifdef __cplusplus
extern "C" {
#endif

int connect_server(const char *ip,int port);
int connect_close(int fd);
int exec_c(int fd,PSTORE pstore,int cmd);
#ifdef __cplusplus
}
#endif
#endif
