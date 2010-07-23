#ifndef HASH_H
#define HASH_H

#ifdef __cplusplus
extern "C"{
#endif

#define HASH_KEY_STRING (-1)

typedef struct hash_t hash_t;

typedef struct hash_entry_t hash_entry_t;

typedef struct hash_index_t hash_index_t;

typedef unsigned int (*hashfunc_t)(const char *key,int *klen);

typedef void (*freefunc_t)(void *memory);

void freefunc_default(void *memory);

unsigned int hashfunc_default(const char *key,int *klen);

hash_ti* hash_make(void);

hash_t* hash_make_custom(hashfunc_t hashfunc,freefunc_t freefunc);

void hash_set(hash_t *ht,const void *key,int klen,const void *val);

void *hash_get(hash_t *ht,const void *key,int klen);

hash_index_t* hash_first(hash_t *ht);

hash_index_t* hash_next(hash_index_t *hi);

void hash_this(hash_index_t *hi,const void **key,int *klen,void **val);

unsigned int hash_count(hash_t *ht);

void hash_clear(hash_t *ht);

void hash_destroy(hash_t *ht);

typedef int (hash_do_callback_fn_t)(void *rec,const void *key,
		int klen,const void *value);

int hash_do(hash_do_callback_fn_t *comp,
	        void *rec,const hash_t *ht);

#ifdef __cplusplus
}
#endif
#endif

