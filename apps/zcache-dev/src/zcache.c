#include "zcache.h"
#include <assert.h>
#include <stdlib.h>



/*
* static unsigned int hash
*
* DESCRIPTION:
*
* Hash a variable-length key into a 32-bit value.  Every bit of the
* key affects every bit of the return value.  Every 1-bit and 2-bit
* delta achieves avalanche.  About (6 * len + 35) instructions.  The
* best hash table sizes are powers of 2.  There is no need to use mod
* (sooo slow!).  If you need less than 32 bits, use a bitmask.  For
* example, if you need only 10 bits, do h = (h & hashmask(10)); In
* which case, the hash table should have hashsize(10) elements.
*
* By Bob Jenkins, 1996.  bob_jenkins@compuserve.com.  You may use
* this code any way you wish, private, educational, or commercial.
* It's free.  See
* http://ourworld.compuserve.com/homepages/bob_jenkins/evahash.htm
* Use for hash table lookup, or anything where one collision in 2^^32
* is acceptable.  Do NOT use for cryptographic purposes.
*
* RETURNS:
*
* Returns a 32-bit hash value.
*
* ARGUMENTS:
*
* key - Key (the unaligned variable-length array of bytes) that we
* are hashing.
*
* length - Length of the key in bytes.
*
* init_val - Initialization value of the hash if you need to hash a
* number of strings together.  For instance, if you are hashing N
* strings (unsigned char **)keys, do it like this:
*
* for (i=0, h=0; i<N; ++i) h = hash( keys[i], len[i], h);
*/
unsigned int hash(const unsigned char *key,
						 const unsigned int length,
						 const unsigned int init_val)
{
	const unsigned char *key_p = key;
	unsigned int a, b, c, len;

	/* set up the internal state */
	a = 0x9e3779b9;             /* the golden ratio; an arbitrary value */
	b = 0x9e3779b9;
	c = init_val;               /* the previous hash value */

	/* handle most of the key */
	for (len = length; len >= 12; len -= 12) {
		a += (key_p[0]
		+ ((unsigned long) key_p[1] << 8)
			+ ((unsigned long) key_p[2] << 16)
			+ ((unsigned long) key_p[3] << 24));
		b += (key_p[4]
		+ ((unsigned long) key_p[5] << 8)
			+ ((unsigned long) key_p[6] << 16)
			+ ((unsigned long) key_p[7] << 24));
		c += (key_p[8]
		+ ((unsigned long) key_p[9] << 8)
			+ ((unsigned long) key_p[10] << 16)
			+ ((unsigned long) key_p[11] << 24));
		HASH_MIX(a, b, c);
		key_p += 12;
	}

	c += length;

	/* all the case statements fall through to the next */
	switch (len) {
	case 11:
		c += ((unsigned long) key_p[10] << 24);
	case 10:
		c += ((unsigned long) key_p[9] << 16);
	case 9:
		c += ((unsigned long) key_p[8] << 8);
		/* the first byte of c is reserved for the length */
	case 8:
		b += ((unsigned long) key_p[7] << 24);
	case 7:
		b += ((unsigned long) key_p[6] << 16);
	case 6:
		b += ((unsigned long) key_p[5] << 8);
	case 5:
		b += key_p[4];
	case 4:
		a += ((unsigned long) key_p[3] << 24);
	case 3:
		a += ((unsigned long) key_p[2] << 16);
	case 2:
		a += ((unsigned long) key_p[1] << 8);
	case 1:
		a += key_p[0];
		/* case 0: nothing left to add */
	}
	HASH_MIX(a, b, c);

	return c;
}

void zcache_die()
{
	exit(0);
}



/*  _________________________________________________________________
**
**  Session Cache: Common Abstraction Layer
**  _________________________________________________________________
*/

void zcache_init(MCConfigRecord *mc,apr_pool_t *p)
{
	/*
	* Warn the user that he should use the storage cache.
	* But we can operate without it, of course.
	*/
	if (mc->nStorageMode == ZCACHE_SCMODE_UNSET) {
	/*	ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
			"Init: Session Cache is not configured "
			"[hint: LUASessionCache]");*/
		mc->nStorageMode = ZCACHE_SCMODE_NONE;
		return;
	}

	if (mc->nStorageMode == ZCACHE_SCMODE_SHMCB) 
	{
		zcache_shmcb_init(mc,p);
	}
}

void zcache_attach(MCConfigRecord *mc,apr_pool_t *p)
{
/*
	* Warn the user that he should use the storage cache.
	* But we can operate without it, of course.
	*/
	if (mc->nStorageMode == ZCACHE_SCMODE_UNSET) {
	/*	ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
			"Init: Session Cache is not configured "
			"[hint: LUASessionCache]");*/
		mc->nStorageMode = ZCACHE_SCMODE_NONE;
		return;
	}

	if (mc->nStorageMode == ZCACHE_SCMODE_SHMCB) 
	{
		zcache_shmcb_attach(mc,p);
	}
}
void zcache_kill(MCConfigRecord *mc)
{
	if (mc->nStorageMode == ZCACHE_SCMODE_SHMCB)
		zcache_shmcb_kill(mc);
	return;
}

BOOL zcache_store(MCConfigRecord *mc,UCHAR *id, int idlen, time_t expiry, void * pdata, int ndata)
{
	BOOL rv = FALSE;

	if (mc->nStorageMode == ZCACHE_SCMODE_SHMCB)
		rv = zcache_shmcb_store(mc,id, idlen, expiry, pdata, ndata);
	return rv;
}

void *zcache_retrieve(MCConfigRecord *mc,UCHAR *id, int idlen, int* ndata)
{
	void *pdata = NULL;

	if (mc->nStorageMode == ZCACHE_SCMODE_SHMCB)
		pdata = zcache_shmcb_retrieve(mc,id, idlen,ndata);
	return pdata;
}

void zcache_remove(MCConfigRecord *mc,UCHAR *id, int idlen)
{
	if (mc->nStorageMode == ZCACHE_SCMODE_SHMCB)
		zcache_shmcb_remove(mc,id, idlen);
	return;
}

void zcache_status(MCConfigRecord *mc,apr_pool_t *p, void (*func)(char *, void *), void *arg)
{
	if (mc->nStorageMode == ZCACHE_SCMODE_SHMCB)
		zcache_shmcb_status(mc,p, func, arg);

	return;
}

void zcache_expire(MCConfigRecord *mc)
{
	if (mc->nStorageMode == ZCACHE_SCMODE_SHMCB)
		zcache_shmcb_expire(mc);
	return;
}

