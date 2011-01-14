#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

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

void
enc_be64(void *buf, uint64_t num)
{
    uint8_t *p = buf;
    *p = (num >> 56) & 0xff;
    *(p + 1) = (num >> 48) & 0xff;
    *(p + 2) = (num >> 40) & 0xff;
    *(p + 3) = (num >> 32) & 0xff;
    *(p + 4) = (num >> 24) & 0xff;
    *(p + 5) = (num >> 16) & 0xff;
    *(p + 6) = (num >> 8) & 0xff;
    *(p + 7) = num & 0xff;
}

uint64_t
dec_be64(const void *buf)
{
    const uint8_t *p = buf;
    return (uint64_t)*p << 56 | (uint64_t)*(p + 1) << 48
        | (uint64_t)*(p + 2) << 40 | (uint64_t)*(p + 3) << 32
        | (uint64_t)*(p + 4) << 24 | (uint64_t)*(p + 5) << 16
        | (uint64_t)*(p + 6) << 8 | (uint64_t)*(p + 7);
}

void
set_bit(uint8_t *bits, unsigned long index)
{
    bits[index / 8] |= (1 << (7 - index % 8));
}

void
clear_bit(uint8_t *bits, unsigned long index)
{
    bits[index / 8] &= ~(1 << (7 - index % 8));
}

int
has_bit(const uint8_t *bits, unsigned long index)
{
    return bits[index / 8] & (1 << (7 - index % 8));
}

uint8_t
hex2i(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return 10 + c - 'a';
    else
        abort();
}

int
ishex(char *str)
{
    while (*str != '\0') {
        if (!((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f')))
            return 0;
        str++;
    }
    return 1;
}

uint8_t *
hex2bin(const char *hex, uint8_t *bin, size_t bsize)
{
    size_t i = 0;
    for (i = 0; i < bsize; i++)
        bin[i] = hex2i(hex[i * 2]) << 4 | hex2i(hex[i * 2 + 1]);
    return bin;
}

char *
bin2hex(const uint8_t *bin, char *hex, size_t bsize)
{
    size_t i;
    const char *hexc = "0123456789abcdef";
    for (i = 0; i < bsize; i++) {
        hex[i * 2] = hexc[(bin[i] >> 4) & 0xf];
        hex[i * 2 + 1] = hexc[bin[i] &0xf];
    }
    hex[i * 2] = '\0';
    return hex;
}

int id_eq(const void *k1, const void *k2)
{
	return *(uint32_t *)k1 == *(uint32_t *)k2;
}

uint32_t
id_hash(const void *k)                                                                    
{
	return *(uint32_t *)k;
}

int
set_nonblocking(int fd)
{
    int oflags;
    if ((oflags = fcntl(fd, F_GETFL, 0)) == -1)
        return errno;
    if (fcntl(fd, F_SETFL, oflags | O_NONBLOCK) == -1)
        return errno;
    return 0;
}

int
set_blocking(int fd)
{
    int oflags;
    if ((oflags = fcntl(fd, F_GETFL, 0)) == -1)
        return errno;
    if (fcntl(fd, F_SETFL, oflags & ~O_NONBLOCK) == -1)
        return errno;
    return 0;
}


