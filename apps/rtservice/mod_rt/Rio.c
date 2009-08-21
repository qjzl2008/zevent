#include "Rio.h"
#include <errno.h>

extern int errno;

ssize_t rio_readn(int fd,void *usrbuf,size_t n)
{
	size_t nleft = n;
	ssize_t nread;
	char *bufp = usrbuf;

	while(nleft > 0) {
		if((nread = read(fd,bufp,nleft)) < 0) {
			if(errno == EINTR)
				nread = 0;
			else
				return -1;
		}
		else if(nread == 0)
			break;
		nleft -= nread;
		bufp += nread;
	}
	return (n - nleft);
}

ssize_t rio_wirten(int fd,void *usrbuf,size_t n)
{
	size_t nleft = n;
	ssize_t nwriten;
	char *bufp = usrbuf;

	while(nleft > 0) {
		if((nwriten = write(fd,bufp,nleft)) <= 0) {
			if(errno == EINTR)
				nwriten = 0;
			else
				return -1;
		}
		else if(nwriten == 0)
			break;
		nleft -= nwriten;
		bufp += nwriten;
	}
	return n;
}
