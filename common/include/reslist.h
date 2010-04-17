#ifndef RESLIST_H
#define RESLIST_H

#include "allocator.h"

/** 
 * @file reslist.h
 * @brief Resource List Routines
 */

#if HAS_THREADS

/**
 * @defgroup  Resource List Routines
 * @ingroup
 * @{
 * @warning
 * <strong><em>Resource list data types and routines are only available when
 * threads are enabled (i.e. HAS_THREADS is not zero).</em></strong>
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Opaque resource list object */
typedef struct reslist_t reslist_t;

/* Generic constructor called by resource list when it needs to create a
 * resource.
 * @param resource opaque resource
 * @param param flags
 */
typedef int (*reslist_constructor)(void **resource, void *params
                                                /*,allocator_t *allocator*/);

/* Generic destructor called by resource list when it needs to destroy a
 * resource.
 * @param resource opaque resource
 * @param param flags
 */
typedef int (*reslist_destructor)(void *resource, void *params
                                               /*,allocator_t *allocator*/);

/**
 * Create a new resource list with the following parameters:
 * @param reslist An address where the pointer to the new resource
 *                list will be stored.
 * @param min Allowed minimum number of available resources. Zero
 *            creates new resources only when needed.
 * @param smax Resources will be destroyed to meet this maximum
 *             restriction as they expire.
 * @param hmax Absolute maximum limit on the number of total resources.
 * @param ttl If non-zero, sets the maximum amount of time in microseconds a
 *            resource may be available while exceeding the soft limit.
 * @param con Constructor routine that is called to create a new resource.
 * @param de Destructor routine that is called to destroy an expired resource.
 * @param params Passed to constructor and deconstructor
 */
int reslist_create(reslist_t **reslist,
                                             int min, int smax, int hmax,
                                             long long ttl,
                                             reslist_constructor con,
                                             reslist_destructor de,
                                             void *params,
                                             allocator_t *allocator);

/**
 * Destroy the given resource list and all resources controlled by
 * this list.
 * FIXME: Should this block until all resources become available,
 *        or maybe just destroy all the free ones, or maybe destroy
 *        them even though they might be in use by something else?
 *        Currently it will abort if there are resources that haven't
 *        been released, so there is an assumption that all resources
 *        have been released to the list before calling this function.
 * @param reslist The reslist to destroy
 */
int reslist_destroy(reslist_t *reslist);

/**
 * Retrieve a resource from the list, creating a new one if necessary.
 * If we have met our maximum number of resources, we will block
 * until one becomes available.
 */
int reslist_acquire(reslist_t *reslist,
                                              void **resource);

/**
 * Return a resource back to the list of available resources.
 */
int reslist_release(reslist_t *reslist,
                                              void *resource);

/**
 * Set the timeout the acquire will wait for a free resource
 * when the maximum number of resources is exceeded.
 * @param reslist The resource list.
 * @param timeout Timeout to wait. The zero waits forever.
 */
void reslist_timeout_set(reslist_t *reslist,
                                          long long timeout);

/**
 * Return the number of outstanding resources.
 * @param reslist The resource list.
 */
unsigned int reslist_acquired_count(reslist_t *reslist);

/**
 * Invalidate a resource in the pool - e.g. a database connection
 * that returns a "lost connection" error and can't be restored.
 * Use this instead of reslist_release if the resource is bad.
 */
int reslist_invalidate(reslist_t *reslist,
                                                 void *resource);


#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* HAS_THREADS */

#endif  /* ! RESLIST_H */
