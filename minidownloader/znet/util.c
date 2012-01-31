#include <fcntl.h>
#include "pstdint.h"
#include <stdlib.h>
#include "util.h"

void
enc_be32(void *buf, uint32_t num)
{
	uint8_t *p = buf;
	*p = (num >> 24) & 0xff;
	*(p + 1) = (num >> 16) & 0xff;
	*(p + 2) = (num >> 8) & 0xff;
	*(p + 3) = num & 0xff;
}

uint32_t
dec_be32(const void *buf)
{
	const uint8_t *p = buf;
	return (uint32_t)*p << 24 | (uint32_t)*(p + 1) << 16
		| (uint16_t)*(p + 2) << 8 | *(p + 3);
}

void *
memfind(const void *sub, size_t sublen, const void *mem, size_t memlen)
{
    size_t i, j;
    const uint8_t *s = sub, *m = mem;
    for (i = 0; i < memlen - sublen + 1; i++) {
        for (j = 0; j < sublen; j++)
            if (m[i+j] != s[j])
                break;
        if (j == sublen)
            return (void *)(m + i);
    }
    return NULL;
}


int id_eq(const void *k1, const void *k2)
{
	return *(uint64_t *)k1 == *(uint64_t *)k2;
}

uint64_t
id_hash(const void *k)                                                                    
{
	return *(uint64_t *)k;
}

int
set_nonblocking(SOCKET fd)
{
	u_long one = 1;
	if(ioctlsocket(fd,FIONBIO,&one) == SOCKET_ERROR) {
		return -1;
	}

	return 0;
}

int
set_blocking(SOCKET fd)
{
	u_long one = 0;
	if(ioctlsocket(fd,FIONBIO,&one) == SOCKET_ERROR) {
		return -1;
	}

	return 0;
}