#ifndef GS_MANAGER_H
#define GS_MANAGER_H

#ifdef __cplusplus
extern "C"{
#endif

int sc_start();
int sc_send2store(char *buf);
int sc_stop();

};

#ifdef __cplusplus
}
#endif

#endif

