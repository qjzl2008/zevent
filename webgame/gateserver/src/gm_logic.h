#ifndef GM_LOGIC_H
#define GM_LOGIC_H

#include <stdint.h>
#include "json/json.h"

#ifdef __cplusplus
extern "C"{
#endif

int gm_logic_reggs(uint64_t peerid,json_object *jmsg);
int gm_logic_data2clients(uint64_t peerid,json_object *jmsg);
#ifdef __cplusplus
}
#endif

#endif

