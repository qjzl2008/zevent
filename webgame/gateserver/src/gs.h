#ifndef GS_H
#define GS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

#define MAX_SCENES_PERGS (1000)
typedef struct gs_t gs_t;
struct gs_t{
    int gsid;
    int scenes[MAX_SCENES_PERGS];
    int nscenes;
};

#ifdef __cplusplus
}
#endif

#endif

