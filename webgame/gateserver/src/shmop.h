#ifndef SHMOP_H
#define SHMOP_H

#ifdef __cplusplus
extern "C"{
#endif

typedef struct shm_t shm_t;
struct shm_t {
	void *base;          
	void *usable;       
	int reqsize; 
	const char *filename;
	int shmid;
};

int shm_create(shm_t *new_m, 
		uint32_t reqsize, 
		const char *filename);

int shm_attach(shm_t *new_m,const char *filename);
int shm_detach(shm_t *m);
int shm_remove(const char *filename);
#ifdef __cplusplus
}
#endif

#endif

