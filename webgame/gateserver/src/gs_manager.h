#ifndef GS_MANAGER_H
#define GS_MANAGER_H

#ifdef __cplusplus
extern "C"{
#endif

int gm_start();
int gm_send2gs(uint64_t peerid,void *buf,uint32_t len);
int gm_reggs(uint64_t peerid,int gsid);
int gm_stop();
int gm_destroy();

#ifdef __cplusplus
}
#endif

#endif

