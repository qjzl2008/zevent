#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

int cm_start();
int cm_send2clients(char *msg);
int cm_send2client(uint64_t peerid,void *buf,uint32_t len);
int cm_stop();
int cm_destroy();

#ifdef __cplusplus
}
#endif

#endif

