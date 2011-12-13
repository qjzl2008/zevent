#ifndef CM_LOGIC_H
#define CM_LOGIC_H

#include <stdint.h>
#include "json/json.h"

#ifdef __cplusplus
extern "C"{
#endif

int cm_logic_createaccount(uint64_t peerid,json_object *jmsg);
int cm_logic_login(uint64_t peerid,json_object *jmsg);

#ifdef __cplusplus
}
#endif

#endif

