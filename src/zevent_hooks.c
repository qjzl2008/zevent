#include "zevent_hooks.h"

APR_HOOK_STRUCT(
		APR_HOOK_LINK(zevent_init)
		APR_HOOK_LINK(zevent_fini)
		APR_HOOK_LINK(child_init)
		APR_HOOK_LINK(child_fini)
		APR_HOOK_LINK(process_connection)
	       )
ZEVENT_IMPLEMENT_HOOK_VOID(zevent_init,(apr_pool_t *pzevent),(pzevent))
ZEVENT_IMPLEMENT_HOOK_VOID(zevent_fini,(apr_pool_t *pzevent),(pzevent))
ZEVENT_IMPLEMENT_HOOK_VOID(child_init,(apr_pool_t *pchild),(pchild))
ZEVENT_IMPLEMENT_HOOK_VOID(child_fini,(apr_pool_t *pchild),(pchild))
ZEVENT_IMPLEMENT_HOOK_RUN_FIRST(int,process_connection,(conn_state_t *cs),(cs),DECLINED)

