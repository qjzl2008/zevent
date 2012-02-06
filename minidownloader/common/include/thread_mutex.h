#ifndef THREAD_MUTEX_H
#define THREAD_MUTEX_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct thread_mutex_t thread_mutex_t;

#define THREAD_MUTEX_DEFAULT  0x0   
#define THREAD_MUTEX_NESTED   0x1   
#define THREAD_MUTEX_UNNESTED 0x2  

int thread_mutex_create(thread_mutex_t **mutex,
                                                  unsigned int flags);

int thread_mutex_lock(thread_mutex_t *mutex);


int thread_mutex_trylock(thread_mutex_t *mutex);


int thread_mutex_unlock(thread_mutex_t *mutex);


int thread_mutex_destroy(thread_mutex_t *mutex);


#ifdef __cplusplus
}
#endif
#endif

