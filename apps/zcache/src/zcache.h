#ifndef __ZCACHE_H__
#define __ZCACHE_H__

#include "zcache_config.h"

#ifndef ZCACHE_CACHE_TIMEOUT
#define ZCACHE_CACHE_TIMEOUT  300
#endif

/*
 * The shared-memory segment header can be cast to and from the
 * SHMCBHeader type, all other structures need to be initialised by
 * utility functions.
 *
 * The "header" looks like this;
 *
 * data applying to the overall structure:
 * - division_offset (unsigned int):
 *   how far into the shared memory segment the first division is.
 * - division_size (unsigned int):
 *   how many bytes each division occupies.
 *   (NB: This includes the queue and the cache)
 * - division_mask (unsigned char):
 *   the "mask" in the next line. Add one to this,
 *   and that's the number of divisions.
 *
 * data applying to within each division:
 * - queue_size (unsigned int):
 *   how big each "queue" is. NB: The queue is the first block in each
 *   division and is followed immediately by the cache itself so so
 *   there's no cache_offset value.
 *
 * data applying to within each queue:
 * - index_num (unsigned char):
 *   how many indexes in each cache's queue
 * - index_offset (unsigned char):
 *   how far into the queue the first index is.
 * - index_size:
 *   how big each index is.
 *
 * data applying to within each cache:
 * - cache_data_offset (unsigned int):
 *   how far into the cache the session-data array is stored.
 * - cache_data_size (unsigned int):
 *   how big each cache's data block is.
 *
 * statistics data (this will eventually be per-division but right now
 * there's only one mutex):
 * - stores (unsigned long):
 *   how many stores have been performed in the cache.
 * - expiries (unsigned long):
 *   how many session have been expired from the cache.
 * - scrolled (unsigned long):
 *   how many sessions have been scrolled out of full cache during a
 *   "store" operation. This is different to the "removes" stats as
 *   they are requested by mod_ssl/Apache, these are done because of
 *   cache logistics. (NB: Also, this value should be deducible from
 *   the others if my code has no bugs, but I count it anyway - plus
 *   it helps debugging :-).
 * - retrieves_hit (unsigned long):
 *   how many session-retrieves have succeeded.
 * - retrieves_miss (unsigned long):
 *   how many session-retrieves have failed.
 * - removes_hit (unsigned long):
 * - removes_miss (unsigned long):
 * 
 * Following immediately after the header is an array of "divisions".
 * Each division is simply a "queue" immediately followed by its
 * corresponding "cache". Each division handles some pre-defined band
 * of sessions by using the "division_mask" in the header. Eg. if
 * division_mask=0x1f then there are 32 divisions, the first of which
 * will store sessions whose least-significant 5 bits are 0, the second
 * stores session whose LS 5 bits equal 1, etc. A queue is an indexing
 * structure referring to its corresponding cache.
 *
 * A "queue" looks like this;
 *
 * - first_pos (unsigned int):
 *   the location within the array of indexes where the virtual
 *   "left-hand-edge" of the cyclic buffer is.
 * - pos_count (unsigned int):
 *   the number of indexes occupied from first_pos onwards.
 *
 * ...followed by an array of indexes, each of which can be
 * memcpy'd to and from an SHMCBIndex, and look like this;
 *
 * - expires (time_t):
 *   the time() value at which this session expires.
 * - offset (unsigned int):
 *   the offset within the cache data block where the corresponding
 *   session is stored.
 * - s_id2 (unsigned char):
 *   the second byte of the session_id, stored as an optimisation to
 *   reduce the number of d2i_SSL_SESSION calls that are made when doing
 *   a lookup.
 * - removed (unsigned char):
 *   a byte used to indicate whether a session has been "passively"
 *   removed. Ie. it is still in the cache but is to be disregarded by
 *   any "retrieve" operation.
 *
 * A "cache" looks like this;
 *
 * - first_pos (unsigned int):
 *   the location within the data block where the virtual
 *   "left-hand-edge" of the cyclic buffer is.
 * - pos_count (unsigned int):
 *   the number of bytes used in the data block from first_pos onwards.
 *
 * ...followed by the data block in which actual DER-encoded SSL
 * sessions are stored.
 */

/* 
 * Header - can be memcpy'd to and from the front of the shared
 * memory segment. NB: The first copy (commented out) has the
 * elements in a meaningful order, but due to data-alignment
 * braindeadness, the second (uncommented) copy has the types grouped
 * so as to decrease "struct-bloat". sigh.
 */
typedef struct {
    unsigned long num_stores;
    unsigned long num_expiries;
    unsigned long num_scrolled;
    unsigned long num_retrieves_hit;
    unsigned long num_retrieves_miss;
    unsigned long num_removes_hit;
    unsigned long num_removes_miss;
    unsigned int division_offset;
    unsigned int division_size;
    unsigned int queue_size;
    unsigned int cache_data_offset;
    unsigned int cache_data_size;
    unsigned int division_mask;
    unsigned int index_num;
    unsigned int index_offset;
    unsigned int index_size;
} SHMCBHeader;

/* 
 * Index - can be memcpy'd to and from an index inside each
 * queue's index array.
 */
typedef struct {
    time_t expires;
    unsigned int offset;
	unsigned int length;
    unsigned int key;
    unsigned char removed;
} SHMCBIndex;

/* 
 * Queue - must be populated by a call to shmcb_get_division
 * and the structure's pointers are used for updating (ie.
 * the structure doesn't need any "set" to update values).
 */
typedef struct {
    SHMCBHeader *header;
    unsigned int *first_pos;
    unsigned int *pos_count;
    SHMCBIndex *indexes;
} SHMCBQueue;

/* 
 * Cache - same comment as for Queue. 'Queue's are in a 1-1
 * correspondance with 'Cache's and are usually carried round
 * in a pair, they are only seperated for clarity.
 */
typedef struct {
    SHMCBHeader *header;
    unsigned int *first_pos;
    unsigned int *pos_count;
    unsigned char *data;
} SHMCBCache;


/*
* void HASH_MIX
*
* DESCRIPTION:
*
* Mix 3 32-bit values reversibly.  For every delta with one or two bits
* set, and the deltas of all three high bits or all three low bits,
* whether the original value of a,b,c is almost all zero or is
* uniformly distributed.
*
* If HASH_MIX() is run forward or backward, at least 32 bits in a,b,c
* have at least 1/4 probability of changing.  If mix() is run
* forward, every bit of c will change between 1/3 and 2/3 of the
* time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
*
* HASH_MIX() takes 36 machine instructions, but only 18 cycles on a
* superscalar machine (like a Pentium or a Sparc).  No faster mixer
* seems to work, that's the result of my brute-force search.  There
* were about 2^68 hashes to choose from.  I only tested about a
* billion of those.
*/
#define HASH_MIX(a, b, c) \
	do { \
	a -= b; a -= c; a ^= (c >> 13); \
	b -= c; b -= a; b ^= (a << 8); \
	c -= a; c -= b; c ^= (b >> 13); \
	a -= b; a -= c; a ^= (c >> 12); \
	b -= c; b -= a; b ^= (a << 16); \
	c -= a; c -= b; c ^= (b >> 5); \
	a -= b; a -= c; a ^= (c >> 3); \
	b -= c; b -= a; b ^= (a << 10); \
	c -= a; c -= b; c ^= (b >> 15); \
	} while(0)

unsigned int hash(const unsigned char *key,
				  const unsigned int length,
				  const unsigned int init_val);
/*
* Support for MM library
*/
#define ZCACHE_MM_FILE_MODE ( APR_UREAD | APR_UWRITE | APR_GREAD | APR_WREAD )

/*
* Provide useful shorthands
*/
#define strEQ(s1,s2)     (strcmp(s1,s2)        == 0)
#define strNE(s1,s2)     (strcmp(s1,s2)        != 0)
#define strEQn(s1,s2,n)  (strncmp(s1,s2,n)     == 0)
#define strNEn(s1,s2,n)  (strncmp(s1,s2,n)     != 0)

#define strcEQ(s1,s2)    (strcasecmp(s1,s2)    == 0)
#define strcNE(s1,s2)    (strcasecmp(s1,s2)    != 0)
#define strcEQn(s1,s2,n) (strncasecmp(s1,s2,n) == 0)
#define strcNEn(s1,s2,n) (strncasecmp(s1,s2,n) != 0)

#define strIsEmpty(s)    (s == NULL || s[0] == NUL)

/*  Mutex Support  */
int          zcache_mutex_init(MCConfigRecord *mc,apr_pool_t *);
int          zcache_mutex_reinit(MCConfigRecord *mc,apr_pool_t *);
int          zcache_mutex_on(MCConfigRecord *mc);
int          zcache_mutex_off(MCConfigRecord *mc);

void         zcache_die(void);

/*  Session Cache Support  */
void         zcache_init(MCConfigRecord *mc,apr_pool_t *);
void         zcache_attach(MCConfigRecord *mc,apr_pool_t *);
#if 0 /* XXX */
void         zcache_status_register(apr_pool_t *p);
#endif
void         zcache_kill(MCConfigRecord *mc);
BOOL         zcache_store(MCConfigRecord *mc,UCHAR *, int, time_t, void *, int);
void        *zcache_retrieve(MCConfigRecord *mc,UCHAR *, int, int*);
void         zcache_remove(MCConfigRecord *mc,UCHAR *, int);
void         zcache_expire(MCConfigRecord *mc);
void         zcache_status(MCConfigRecord *mc,apr_pool_t *, void (*)(char *, void *), void *);
char        *zcache_id2sz(UCHAR *, int);

void         zcache_shmcb_init(MCConfigRecord *mc,apr_pool_t *);
void         zcache_shmcb_attach(MCConfigRecord *mc,apr_pool_t *);
void         zcache_shmcb_kill(MCConfigRecord *mc);
BOOL         zcache_shmcb_store(MCConfigRecord *mc,UCHAR *, int, time_t, void *, int);
void        *zcache_shmcb_retrieve(MCConfigRecord *mc,UCHAR *, int, int*);
void         zcache_shmcb_remove(MCConfigRecord *mc,UCHAR *, int);
void         zcache_shmcb_expire();
void         zcache_shmcb_status(MCConfigRecord *mc,apr_pool_t *, void (*)(char *, void *), void *);

unsigned int zcache_shmcb_get_safe_uint(unsigned int *i);
time_t zcache_shmcb_get_safe_time(time_t *t);
SHMCBIndex *zcache_shmcb_get_index(const SHMCBQueue *q, unsigned int i);
unsigned int zcache_shmcb_expire_division( SHMCBQueue *q, SHMCBCache *c);
BOOL zcache_shmcb_get_division(SHMCBHeader *h, SHMCBQueue *q, SHMCBCache *c, unsigned int i);
void zcache_shmcb_get_header(void *shm_mem, SHMCBHeader **header);
#endif //__MOD_ZCACHE_H__
