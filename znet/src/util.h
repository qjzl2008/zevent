#ifndef UTIL_H
#define UTIL_H

int id_eq(const void *k1, const void *k2); 
uint32_t id_hash(const void *k);     
void * memfind(const void *sub, size_t sublen, const void *mem, size_t memlen);


int set_nonblocking(int fd);
int set_blocking(int fd);

#endif
