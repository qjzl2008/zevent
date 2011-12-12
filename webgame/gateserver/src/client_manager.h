#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#ifdef __cplusplus
extern "C"{
#endif

int cm_start();
int cm_send2clients(char *msg);
int cm_send2player(char *buf);
int cm_stop();

#ifdef __cplusplus
}
#endif

#endif

