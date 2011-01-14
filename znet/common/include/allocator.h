#ifndef MEM_ALLOCATOR_H
#define MEM_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

/**
 * @file allocator.h
 * @brief Internal Memory Allocation
 */

#ifdef __cplusplus
extern "C" {
#endif


typedef struct allocator_t allocator_t;

typedef struct memnode_t memnode_t;

struct memnode_t {
    memnode_t *next;            /**< next memnode */
    memnode_t **ref;            /**< reference to self */
    unsigned int   index;           /**< size */
    unsigned int   free_index;      /**< how much free */
    char          *first_avail;     /**< pointer to first free memory */
    char          *endp;            /**< pointer to end of free memory */
};

#define ALIGN(size, boundary) \
    (((size) + ((boundary) - 1)) & ~((boundary) - 1))

#define ALIGN_DEFAULT(size) ALIGN(size, 8)


#define MEMNODE_T_SIZE ALIGN_DEFAULT(sizeof(memnode_t))

#define ALLOCATOR_MAX_FREE_UNLIMITED 0

/**
 * Create a new allocator
 * @param allocator The allocator we have just created.
 *
 */
int allocator_create(allocator_t **allocator);

/**
 * Destroy an allocator
 * @param allocator The allocator to be destroyed
 * @remark Any memnodes not given back to the allocator prior to destroying
 *         will _not_ be free()d.
 */
void allocator_destroy(allocator_t *allocator);

/**
 * Allocate a block of mem from the allocator
 * @param allocator The allocator to allocate from
 * @param size The size of the mem to allocate (excluding the
 *        memnode structure)
 */
memnode_t * allocator_alloc(allocator_t *allocator,
		                  size_t size);

/**
 * Free a list of blocks of mem, giving them back to the allocator.
 * The list is typically terminated by a memnode with its next field
 * set to NULL.
 * @param allocator The allocator to give the mem back to
 * @param memnode The memory node to return
 */
void allocator_free(allocator_t *allocator,
                                     memnode_t *memnode);

void *mmalloc(allocator_t *allocator,uint32_t size);

void mfree(allocator_t *allocator,void *memory);


void allocator_max_free_set(allocator_t *allocator,
                                             size_t size);

#include "thread_mutex.h"

#if HAS_THREADS

void allocator_mutex_set(allocator_t *allocator,
                                          thread_mutex_t *mutex);

thread_mutex_t * allocator_mutex_get(
                                      allocator_t *allocator);

#endif /* HAS_THREADS */


#ifdef __cplusplus
}
#endif

#endif /* !ALLOCATOR_H */
