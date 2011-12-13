#ifndef STORE_CLIENT_H
#define STORE_CLIENT_H

#ifdef __cplusplus
extern "C"{
#endif

int sc_start();
int sc_send2store(void *buf,int len);
int sc_stop();
int sc_destroy();

#ifdef __cplusplus
}
#endif

#endif

