#include <stdlib.h>
#include <string.h>
#include "hash.h"

struct hash_entry_t{
	hash_entry_t *next;
	unsigned int hash;
	const void *key;
	int klen;
	const void *val;
};

struct hash_index_t{
	hash_t *ht;
	hash_entry_t *this,*next;
	unsigned int index;
};

struct hash_t{
	hash_entry_t **array;
	hash_index_t iterator;
	unsigned int count,max;
	hashfunc_t hash_func;
	freefunc_t free_func;
	hash_entry_t *free;
};

#define INITIAL_MAX (15) //2^n-1;

static hash_entry_t **alloc_array(hash_t *ht,unsigned int max)
{
	hash_entry_t **array = malloc(sizeof(*ht->array) * (max + 1));
	memset(array,0,sizeof(*ht->array) * (max + 1));
	return array;
}

hash_t *hash_make(void)
{
	hash_t *ht;
	ht = malloc(sizeof(hash_t));
	ht->free = NULL;
	ht->count = 0;
	ht->max = INITIAL_MAX;
	ht->array = alloc_array(ht,ht->max);
	ht->hash_func = hashfunc_default;
	ht->free_func = freefunc_default;

	return ht;
}

hash_t *hash_make_custom(hashfunc_t hashfunc,freefunc_t freefunc)
{
	hash_t *ht;
	ht = hash_make();

	if(hashfunc)
		ht->hash_func = hashfunc;
	if(freefunc)
		ht->free_func = freefunc;

	return ht;
}

/*
 * hash iteration functions
 */

hash_index_t* hash_next(hash_index_t *hi)
{
	hi->this = hi->next;
	while(!hi->this){
		if(hi->index > hi->ht->max)
			return NULL;
		hi->this = hi->ht->array[hi->index++];
	}

	hi->next = hi->this->next;
	return hi;
}

hash_index_t *hash_first(hash_t *ht)
{
	hash_index_t *hi;
	hi = &ht->iterator;
	hi->ht = ht;
	hi->index = 0;
	hi->next = NULL;
	hi->this = NULL;
	return hash_next(hi);
}

void hash_this(hash_index_t *hi,const void **key,int *klen,void **val)
{
	if(key) *key = hi->this->key;
	if(klen) *klen = hi->this->klen;
	if(val) *val = (void *)hi->this->val;
}

static void expand_array(hash_t *ht)
{
	hash_index_t *hi;
	hash_entry_t **new_array;
	unsigned int new_max;

	new_max = ht->max*2 + 1;
	new_array = alloc_array(ht,new_max);

	for(hi = hash_first(ht); hi ; hi = hash_next(hi)){
		unsigned int i = hi->this->hash & new_max;
		hi->this->next = new_array[i];
		new_array[i] = hi->this;
	}

	free(ht->array);
	ht->array = new_array;
	ht->max = new_max;
}

void freefunc_default(void *memory)
{
	free(memory);
}

unsigned int hashfunc_default(const char *char_key,int *klen)
{
	unsigned int hash = 0;
	const unsigned char *key = (const unsigned char *)char_key;
	const unsigned char *p;
	int i;

	if(*klen == HASH_KEY_STRING){
		for(p = key; *p ; p ++)
		{
			hash = hash * 33 + *p;
		}
		*klen = (int)(p - key);
	}
	else{
		for (p = key,i = *klen;i;i--,p++){
			hash = hash * 33 + *p;
		}
	}

	return hash;
}

static hash_entry_t **find_entry(hash_t *ht,
		                     const void *key,
				     int klen,
				     const void *val)
{
	hash_entry_t **hep,*he;
	unsigned int hash;

	hash = ht->hash_func(key,&klen);

	for (hep = &ht->array[hash & ht->max],he = *hep;
			he; hep = &he->next,he = *hep){
		if(he->hash == hash
				&& he->klen == klen
				&& memcmp(he->key,key,klen) == 0)
			break;
	}

	if(he || !val)
		return hep;
	if((he = ht->free) != NULL)
		ht->free = he->next;
	else
		he = malloc(sizeof(*he));
	he->next = NULL;
	he->hash = hash;
	he->key = key;
	he->klen = klen;
	he->val = val;
	*hep = he;
	ht->count++;
	return hep;
}

void * hash_get(hash_t *ht,const void *key,int klen)
{
	hash_entry_t *he;
	he = *find_entry(ht,key,klen,NULL);
	if(he)
		return (void *)he->val;
	else
		return NULL;
}

void hash_set(hash_t *ht,const void *key,int klen,const void *val)
{
	hash_entry_t **hep;
	hep = find_entry(ht,key,klen,val);
	if(*hep){
		if(!val){
			hash_entry_t *old = *hep;
			*hep = (*hep)->next;
			old->next = ht->free;
			ht->free = old;
			--ht->count;
		}
		else {
			/* replace entry */
			(*hep)->val = val;
			/* check that the collision rate isn't too high*/
			if(ht->count > ht->max){
				expand_array(ht);
			}
		}
	}

	/*else key not present and val == NULL */
}

unsigned int hash_count(hash_t *ht)
{
	return ht->count;
}

void hash_clear(hash_t *ht)
{
	hash_index_t *hi;
	for (hi = hash_first(ht); hi ;hi = hash_next(hi))
		hash_set(ht,hi->this->key,hi->this->klen,NULL);
}

static void free_he(hash_t *ht,hash_entry_t *he)
{
	ht->free_func((void *)he->key);
	ht->free_func((void *)he->val);
	free(he);
}

void hash_destroy(hash_t *ht)
{
	hash_index_t *hi;
	hash_entry_t *he = ht->free,*old;
	for(hi = hash_first(ht);hi;hi = hash_next(hi))
	{
		free_he(ht,hi->this);
	}

	while(he)
	{
		old = he->next;
		free_he(ht,he);
		he = old;
	}

	free(ht->array);
	free(ht);
}

int hash_do(hash_do_callback_fn_t *comp,void *rec,const hash_t *ht)
{
	hash_index_t hix;
	hash_index_t *hi;
	int rv,dorv = 1;

	hix.ht = (hash_t *)ht;
	hix.index = 0;
	hix.this = NULL;
	hix.next = NULL;

	if ((hi = hash_next(&hix))) {
		/* scan table*/
		do{
			rv = (*comp)(rec,hi->this->key,hi->this->klen,hi->this->val);
		}while(rv >= 0 && (hi = hash_next(hi)));

		if(rv == 0){
			dorv = 0;
		}
	}

	return dorv;
}
