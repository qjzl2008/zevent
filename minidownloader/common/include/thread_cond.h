#ifndef THREAD_COND_H
#define THREAD_COND_H


#include "thread_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if HAS_THREADS

typedef enum{
	COND_ETIMEUP = 1
}COND_ERR_TYPE;

typedef struct thread_cond_t thread_cond_t;

int thread_cond_create(thread_cond_t **cond);

int thread_cond_wait(thread_cond_t *cond,
                                               thread_mutex_t *mutex);

int thread_cond_timedwait(thread_cond_t *cond,
                                                    thread_mutex_t *mutex,
                                                    long long timeout);

int thread_cond_signal(thread_cond_t *cond);


int thread_cond_broadcast(thread_cond_t *cond);


int thread_cond_destroy(thread_cond_t *cond);

#endif /* HAS_THREADS */

#ifdef __cplusplus
}
#endif

#endif  /* ! THREAD_COND_H */
