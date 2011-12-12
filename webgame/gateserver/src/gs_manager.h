#ifndef GS_MANAGER_H
#define GS_MANAGER_H

#ifdef __cplusplus
extern "C"{
#endif

int gm_start();
int gm_send2gs(char *buf);
int gm_stop();

};

#ifdef __cplusplus
}
#endif

#endif

