#include <stdlib.h>    
#include <string.h>
#include "allocator.h"

/*
 * Magic numbers
 */

#define MIN_ALLOC 256//8192
#define MAX_INDEX   80//20

#define BOUNDARY_INDEX 7//12
#define BOUNDARY_SIZE (1 << BOUNDARY_INDEX)

/*
 * Allocator
 *
 * @note The max_free_index and current_free_index fields are not really
 * indices, but quantities of BOUNDARY_SIZE big memory blocks.
 */

struct allocator_t {
    /** largest used index into free[], always < MAX_INDEX */
    unsigned int        max_index;
    /** Total size (in BOUNDARY_SIZE multiples) of unused memory before
     * blocks are given back. @see allocator_max_free_set().
     * @note Initialized to ALLOCATOR_MAX_FREE_UNLIMITED,
     * which means to never give back blocks.
     */
    unsigned int        max_free_index;
    /**
     * Memory size (in BOUNDARY_SIZE multiples) that currently must be freed
     * before blocks are given back. Range: 0..max_free_index
     */
    unsigned int        current_free_index;
#if HAS_THREADS
    thread_mutex_t *mutex;
#endif /* HAS_THREADS */

    /**
     * Lists of free nodes. Slot 0 is used for oversized nodes,
     * and the slots 1..MAX_INDEX-1 contain nodes of sizes
     * (i+1) * BOUNDARY_SIZE. Example for BOUNDARY_INDEX == 12:
     * slot  0: nodes larger than 81920
     * slot  1: size  8192
     * slot  2: size 12288
     * ...
     * slot 19: size 81920
     */
    memnode_t      *free[MAX_INDEX];
};

#define SIZEOF_ALLOCATOR_T  ALIGN_DEFAULT(sizeof(allocator_t))


/*
 * Allocator
 */

int allocator_create(allocator_t **allocator)
{
    allocator_t *new_allocator;

    *allocator = NULL;

    if ((new_allocator = malloc(SIZEOF_ALLOCATOR_T)) == NULL)
        return -1;

    memset(new_allocator, 0, SIZEOF_ALLOCATOR_T);
    new_allocator->max_free_index = ALLOCATOR_MAX_FREE_UNLIMITED;

    *allocator = new_allocator;

    return 0;
}

void allocator_destroy(allocator_t *allocator)
{
    unsigned int index;
    memnode_t *node, **ref;

    for (index = 0; index < MAX_INDEX; index++) {
        ref = &allocator->free[index];
        while ((node = *ref) != NULL) {
            *ref = node->next;
            free(node);
        }
    }

    free(allocator);
}

#if HAS_THREADS
void allocator_mutex_set(allocator_t *allocator,
                                          thread_mutex_t *mutex)
{
    allocator->mutex = mutex;
}

thread_mutex_t * allocator_mutex_get(
                                      allocator_t *allocator)
{
    return allocator->mutex;
}
#endif /* HAS_THREADS */

void allocator_max_free_set(allocator_t *allocator,
                                             size_t in_size)
{
    unsigned int max_free_index;
    unsigned int size = (unsigned int)in_size;

#if HAS_THREADS
    thread_mutex_t *mutex;

    mutex = allocator_mutex_get(allocator);
    if (mutex != NULL)
        thread_mutex_lock(mutex);
#endif /*HAS_THREADS */

    max_free_index = ALIGN(size, BOUNDARY_SIZE) >> BOUNDARY_INDEX;
    allocator->current_free_index += max_free_index;
    allocator->current_free_index -= allocator->max_free_index;
    allocator->max_free_index = max_free_index;
    if (allocator->current_free_index > max_free_index)
        allocator->current_free_index = max_free_index;

#if HAS_THREADS
    if (mutex != NULL)
        thread_mutex_unlock(mutex);
#endif
}

static 
memnode_t *in_allocator_alloc(allocator_t *allocator, size_t in_size)
{
    memnode_t *node, **ref;
    unsigned int max_index;
    size_t size, i, index;

    /* Round up the block size to the next boundary, but always
     * allocate at least a certain size (MIN_ALLOC).
     */
    size = ALIGN(in_size + MEMNODE_T_SIZE, BOUNDARY_SIZE);
    if (size < in_size) {
        return NULL;
    }
    if (size < MIN_ALLOC)
        size = MIN_ALLOC;

    /* Find the index for this node size by
     * dividing its size by the boundary size
     */
    index = (size >> BOUNDARY_INDEX) - 1;
    
    if (index > (0xffffffffU)) {
        return NULL;
    }

    /* First see if there are any nodes in the area we know
     * our node will fit into.
     */
    if (index <= allocator->max_index) {
#if HAS_THREADS
        if (allocator->mutex)
            thread_mutex_lock(allocator->mutex);
#endif /* HAS_THREADS */

        /* Walk the free list to see if there are
         * any nodes on it of the requested size
         *
         * NOTE: an optimization would be to check
         * allocator->free[index] first and if no
         * node is present, directly use
         * allocator->free[max_index].  This seems
         * like overkill though and could cause
         * memory waste.
         */
        max_index = allocator->max_index;
        ref = &allocator->free[index];
        i = index;
        while (*ref == NULL && i < max_index) {
           ref++;
           i++;
        }

        if ((node = *ref) != NULL) {
            /* If we have found a node and it doesn't have any
             * nodes waiting in line behind it _and_ we are on
             * the highest available index, find the new highest
             * available index
             */
            if ((*ref = node->next) == NULL && i >= max_index) {
                do {
                    ref--;
                    max_index--;
                }
                while (*ref == NULL && max_index > 0);

                allocator->max_index = max_index;
            }

            allocator->current_free_index += node->index;
            if (allocator->current_free_index > allocator->max_free_index)
                allocator->current_free_index = allocator->max_free_index;

#if HAS_THREADS
            if (allocator->mutex)
                thread_mutex_unlock(allocator->mutex);
#endif /* HAS_THREADS */

            node->next = NULL;
            node->first_avail = (char *)node + MEMNODE_T_SIZE;

            return node;
        }

#if HAS_THREADS
        if (allocator->mutex)
            thread_mutex_unlock(allocator->mutex);
#endif /* HAS_THREADS */
    }

    /* If we found nothing, seek the sink (at index 0), if
     * it is not empty.
     */
    else if (allocator->free[0]) {
#if HAS_THREADS
        if (allocator->mutex)
            thread_mutex_lock(allocator->mutex);
#endif /* HAS_THREADS */

        /* Walk the free list to see if there are
         * any nodes on it of the requested size
         */
        ref = &allocator->free[0];
        while ((node = *ref) != NULL && index > node->index)
            ref = &node->next;

        if (node) {
            *ref = node->next;

            allocator->current_free_index += node->index;
            if (allocator->current_free_index > allocator->max_free_index)
                allocator->current_free_index = allocator->max_free_index;

#if HAS_THREADS
            if (allocator->mutex)
                thread_mutex_unlock(allocator->mutex);
#endif /* HAS_THREADS */

            node->next = NULL;
            node->first_avail = (char *)node + MEMNODE_T_SIZE;

            return node;
        }

#if HAS_THREADS
        if (allocator->mutex)
            thread_mutex_unlock(allocator->mutex);
#endif /* HAS_THREADS */
    }

    /* If we haven't got a suitable node, malloc a new one
     * and initialize it.
     */
    if ((node = malloc(size)) == NULL)
        return NULL;

    node->next = NULL;
    node->index = (unsigned int)index;
    node->first_avail = (char *)node + MEMNODE_T_SIZE;
    node->endp = (char *)node + size;

    return node;
}

static 
void in_allocator_free(allocator_t *allocator, memnode_t *node)
{
    memnode_t *next, *freelist = NULL;
    unsigned int index, max_index;
    unsigned int max_free_index, current_free_index;

#if HAS_THREADS
    if (allocator->mutex)
        thread_mutex_lock(allocator->mutex);
#endif /* HAS_THREADS */

    max_index = allocator->max_index;
    max_free_index = allocator->max_free_index;
    current_free_index = allocator->current_free_index;

    /* Walk the list of submitted nodes and free them one by one,
     * shoving them in the right 'size' buckets as we go.
     */
    do {
        next = node->next;
        index = node->index;

        if (max_free_index != ALLOCATOR_MAX_FREE_UNLIMITED
            && index > current_free_index) {
            node->next = freelist;
            freelist = node;
        }
        else if (index < MAX_INDEX) {
            /* Add the node to the appropiate 'size' bucket.  Adjust
             * the max_index when appropiate.
             */
            if ((node->next = allocator->free[index]) == NULL
                && index > max_index) {
                max_index = index;
            }
            allocator->free[index] = node;
            if (current_free_index >= index)
                current_free_index -= index;
            else
                current_free_index = 0;
        }
        else {
            /* This node is too large to keep in a specific size bucket,
             * just add it to the sink (at index 0).
             */
            node->next = allocator->free[0];
            allocator->free[0] = node;
            if (current_free_index >= index)
                current_free_index -= index;
            else
                current_free_index = 0;
        }
    } while ((node = next) != NULL);

    allocator->max_index = max_index;
    allocator->current_free_index = current_free_index;

#if HAS_THREADS
    if (allocator->mutex)
        thread_mutex_unlock(allocator->mutex);
#endif /* HAS_THREADS */

    while (freelist != NULL) {
        node = freelist;
        freelist = node->next;
        free(node);
    }
}

memnode_t * allocator_alloc(allocator_t *allocator,
                                                 size_t size)
{
    return in_allocator_alloc(allocator, size);
}

void allocator_free(allocator_t *allocator,
                                     memnode_t *node)
{
    in_allocator_free(allocator, node);
}

void *mmalloc(allocator_t *allocator,uint32_t size)
{
	memnode_t *pNode = allocator_alloc(allocator,size);
	if(pNode)
		return pNode->first_avail;
	else
		return NULL;
}

void mfree(allocator_t *allocator,void *memory)
{
	memnode_t *pNode = NULL;
	pNode = (memnode_t *)((char *)memory - MEMNODE_T_SIZE);
	allocator_free(allocator,pNode);
}
