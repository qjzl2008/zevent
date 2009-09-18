#include "unixd_mutex.h"
#include "apr_thread_proc.h"
#include "apr_strings.h"
#include "apr_portable.h"
#include <sys/sem.h>
#include <sys/prctl.h>
#include <unistd.h>

unixd_config_rec unixd_config;

/* XXX move to APR and externalize (but implement differently :) ) */
static apr_lockmech_e proc_mutex_mech(apr_proc_mutex_t *pmutex)
{
	const char *mechname = apr_proc_mutex_name(pmutex);

	if (!strcmp(mechname, "sysvsem")) {
		return APR_LOCK_SYSVSEM;
	}
	else if (!strcmp(mechname, "flock")) {
		return APR_LOCK_FLOCK;
	}
	return APR_LOCK_DEFAULT;
}

apr_status_t unixd_set_proc_mutex_perms(apr_proc_mutex_t *pmutex)
{
	if (!geteuid()) {
		apr_lockmech_e mech = proc_mutex_mech(pmutex);

		switch(mech) {
#if APR_HAS_SYSVSEM_SERIALIZE
			case APR_LOCK_SYSVSEM:
				{
					apr_os_proc_mutex_t ospmutex;
#if !APR_HAVE_UNION_SEMUN
					union semun {
						long val;
						struct semid_ds *buf;
						unsigned short *array;
					};
#endif
					union semun ick;
					struct semid_ds buf;

					apr_os_proc_mutex_get(&ospmutex, pmutex);
					buf.sem_perm.uid = unixd_config.user_id;
					buf.sem_perm.gid = unixd_config.group_id;
					buf.sem_perm.mode = 0600;
					ick.buf = &buf;
					if (semctl(ospmutex.crossproc, 0, IPC_SET, ick) < 0) {
						return errno;
					}
				}
				break;
#endif
#if APR_HAS_FLOCK_SERIALIZE
			case APR_LOCK_FLOCK:
				{
					const char *lockfile = apr_proc_mutex_lockfile(pmutex);

					if (lockfile) {
						if (chown(lockfile, unixd_config.user_id,
									-1 /* no gid change */) < 0) {
							return errno;
						}
					}
				}
				break;
#endif
			default:
				/* do nothing */
				break;
		}
	}
	return APR_SUCCESS;
}

apr_status_t unixd_set_global_mutex_perms(apr_global_mutex_t *gmutex)  
{                         
#if !APR_PROC_MUTEX_IS_GLOBAL      
	apr_os_global_mutex_t osgmutex; 
	apr_os_global_mutex_get(&osgmutex, gmutex);      
	return unixd_set_proc_mutex_perms(osgmutex.proc_mutex);      
#else  /* APR_PROC_MUTEX_IS_GLOBAL */        
	/* In this case, apr_proc_mutex_t and apr_global_mutex_t are the same. */      
	return unixd_set_proc_mutex_perms(gmutex);                             
#endif /* APR_PROC_MUTEX_IS_GLOBAL */                                       
}           

