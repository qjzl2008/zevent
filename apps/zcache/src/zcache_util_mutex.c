#include "zcache.h"

#if !defined(OS2) && !defined(WIN32) && !defined(BEOS) && !defined(NETWARE)
#include "unixd_mutex.h"
#define MOD_ZCACHE_SET_MUTEX_PERMS /* XXX Apache should define something */
#endif

int zcache_mutex_init(MCConfigRecord *mc,apr_pool_t *p)
{
    apr_status_t rv;

    if (mc->nMutexMode == ZCACHE_MUTEXMODE_NONE) 
        return TRUE;

    if ((rv = apr_global_mutex_create(&mc->pMutex, mc->szMutexFile,
                                mc->nMutexMech, p)) != APR_SUCCESS) {
       /* if (mc->szMutexFile)
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                         "Cannot create STORAGEMutex with file `%s'",
                         mc->szMutexFile);
        else
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                         "Cannot create STORAGEMutex");*/
        return FALSE;
    }

#ifdef MOD_ZCACHE_SET_MUTEX_PERMS
    rv = unixd_set_global_mutex_perms(mc->pMutex);
    if (rv != APR_SUCCESS) {
        /*ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                     "Could not set permissions on stroage_mutex; check User "
                     "and Group directives");*/
        return FALSE;
    }
#endif
    return TRUE;
}

int zcache_mutex_reinit(MCConfigRecord *mc,apr_pool_t *p)
{
    apr_status_t rv;

    if (mc->nMutexMode == ZCACHE_MUTEXMODE_NONE)
        return TRUE;

    if ((rv = apr_global_mutex_child_init(&mc->pMutex,
                                    mc->szMutexFile, p)) != APR_SUCCESS) {
     /*   if (mc->szMutexFile)
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                         "Cannot reinit STORAGEMutex with file `%s'",
                         mc->szMutexFile);
        else
            ap_log_error(APLOG_MARK, APLOG_WARNING, rv, s,
                         "Cannot reinit STORAGEMutex");*/
        return FALSE;
    }
    return TRUE;
}

int zcache_mutex_on(MCConfigRecord *mc)
{
    apr_status_t rv;

    if (mc->nMutexMode == ZCACHE_MUTEXMODE_NONE)
        return TRUE;
    if ((rv = apr_global_mutex_lock(mc->pMutex)) != APR_SUCCESS) {
       /* ap_log_error(APLOG_MARK, APLOG_WARNING, rv, s,
                     "Failed to acquire global mutex lock");*/
        return FALSE;
    }
    return TRUE;
}

int zcache_mutex_off(MCConfigRecord *mc)
{
    apr_status_t rv;

    if (mc->nMutexMode == ZCACHE_MUTEXMODE_NONE)
        return TRUE;
    if ((rv = apr_global_mutex_unlock(mc->pMutex)) != APR_SUCCESS) {
/*        ap_log_error(APLOG_MARK, APLOG_WARNING, rv, s,
                     "Failed to release global mutex lock");*/
        return FALSE;
    }
    return TRUE;
}

