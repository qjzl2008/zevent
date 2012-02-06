#ifndef RESLIST_H
#define RESLIST_H


#if HAS_THREADS


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct reslist_t reslist_t;


typedef int (*reslist_constructor)(void **resource, void *params
                                                /*,allocator_t *allocator*/);

typedef int (*reslist_destructor)(void *resource, void *params
                                               /*,allocator_t *allocator*/);

int reslist_create(reslist_t **reslist,
                                             int min, int smax, int hmax,
                                             long long ttl,
                                             reslist_constructor con,
                                             reslist_destructor de,
                                             void *params
                                             /*,allocator_t *allocator*/);


int reslist_destroy(reslist_t *reslist);


int reslist_acquire(reslist_t *reslist,
                                              void **resource);

int reslist_release(reslist_t *reslist,
                                              void *resource);


void reslist_timeout_set(reslist_t *reslist,
                                          long long timeout);


unsigned int reslist_acquired_count(reslist_t *reslist);


int reslist_invalidate(reslist_t *reslist,
                                                 void *resource);


#ifdef __cplusplus
}
#endif


#endif  /* HAS_THREADS */

#endif  
