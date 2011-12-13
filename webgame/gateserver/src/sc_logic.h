#ifndef SC_LOGIC_H
#define SC_LOGIC_H
#include "json/json.h"

#ifdef __cplusplus
extern "C"{
#endif

int sc_logic_createaccount(json_object *jmsg);
int sc_logic_login(json_object *jmsg);

#ifdef __cplusplus
}
#endif

#endif

