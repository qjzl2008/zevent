#ifndef __Rio_H_
#define __Rio_H_

#include <unistd.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t rio_readn(int fd,void *usrbuf,size_t n);
ssize_t rio_wirten(int fd,void *usrbuf,size_t n);

/*³ɹ¦·µ»ØֽÚý·µ»Ø£¨ֻ¶Ôio_readn4˵£©£¬³öµ»أ­1¡£*/

#ifdef __cplusplus
}
#endif
#endif
