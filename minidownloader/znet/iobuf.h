#ifndef BTPD_IOBUF_H
#define BTPD_IOBUF_H

#include "allocator.h"
#include "pstdint.h"

struct iobuf {
    uint8_t *buf;
    size_t size;
    size_t off;
    size_t skip;
    int error;
};

struct iobuf iobuf_init(allocator_t *allocator,size_t size);
void iobuf_free(allocator_t *allocator,struct iobuf *iob);
void iobuf_reset(struct iobuf *iob);
int iobuf_accommodate(allocator_t *allocator,struct iobuf *iob, size_t size);
int iobuf_write(allocator_t *allocator,struct iobuf *iob, const void *data, size_t size);
int iobuf_print(allocator_t *allocator,struct iobuf *iob, const char *fmt, ...);
void *iobuf_find(struct iobuf *iob, const void *p, size_t plen);
void iobuf_consumed(struct iobuf *iob, size_t count);

#define iobuf_swrite(iob, s) iobuf_write(iob, s, sizeof(s) - 1)

#endif
