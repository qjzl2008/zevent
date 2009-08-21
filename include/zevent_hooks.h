#ifndef ZEVENT_HOOKS_H
#define ZEVENT_HOOKS_H

#include "zevent_config.h"
#include "zevent.h"

#ifdef __cplusplus
extern "C" {
#endif
	ZEVENT_DECLARE_HOOK(void,zevent_init,(apr_pool_t *pzevent))
	ZEVENT_DECLARE_HOOK(void,zevent_fini,(apr_pool_t *pzevent))
	ZEVENT_DECLARE_HOOK(void,child_init,(apr_pool_t *pchild))
	ZEVENT_DECLARE_HOOK(void,child_fini,(apr_pool_t *pchild))
	ZEVENT_DECLARE_HOOK(int,process_connection,(conn_state_t *cs))
#ifdef __cplusplus
}
#endif
#endif
