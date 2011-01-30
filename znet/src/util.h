#ifndef UTIL_H
#define UTIL_H

void enc_be32(void *buf, uint32_t num);
uint32_t dec_be32(const void *buf);

int id_eq(const void *k1, const void *k2); 
uint32_t id_hash(const void *k);     
void * memfind(const void *sub, size_t sublen, const void *mem, size_t memlen);


int set_nonblocking(int fd);
int set_blocking(int fd);

#endif
