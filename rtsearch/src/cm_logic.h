#ifndef CM_LOGIC_H
#define CM_LOGIC_H

#include <stdint.h>
#include "json/json.h"
#include "request.h"

#ifdef __cplusplus
extern "C"{
#endif

int cm_logic_set(struct request *req);
int cm_logic_del(struct request *req);
int cm_logic_get(struct request *req,char **qres);

#ifdef __cplusplus
}
#endif


#endif
