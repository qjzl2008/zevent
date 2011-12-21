#ifndef GS_MANAGER_H
#define GS_MANAGER_H

#include <stdint.h>
#include "gs.h"

#ifdef __cplusplus
extern "C"{
#endif

int gm_start();
int gm_send2gs(uint64_t peerid,void *buf,uint32_t len);
int gm_reggs(uint64_t peerid,gs_t *gs);
int gm_unreggs(uint64_t peerid);
int gm_getpidbyid(int gsid,uint64_t *peerid);
int gm_getgs(uint64_t *peerid);
void* gm_malloc(uint32_t size);
int gm_free(void *memory);
int gm_stop();
int gm_destroy();

#ifdef __cplusplus
}
#endif

#endif

