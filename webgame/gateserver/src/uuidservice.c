#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "uuidservice.h"
#include "shmop.h"

static shm_t shm;
typedef struct uuid_data_t{  
    uint64_t time_last;
    uint64_t fudge;  
}uuid_data_t;   

static uuid_data_t *uuid_data;
#define SHM_FILE ("store.dat")
#define SHM_SIZE (uint32_t)(sizeof(uuid_data_t))  
/*get uuid from one server*/
static int get_uuid(int sockfd,char *ip,int nport,int uuid_type,uint64_t *uuid)
{
	int rv;
	/*if(fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0)
	{
		return -1;
	}*/

	struct sockaddr_in servaddr; 
	bzero(&servaddr, sizeof(servaddr)); 

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(nport);
	if(inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0)
	{
		return -1;
	}

	uint64_t type = (uint64_t)uuid_type;

	do {
		rv = sendto(sockfd, &type,sizeof(type), 0,
				(const struct sockaddr*)&servaddr,
				sizeof(struct sockaddr));
	} while (rv == -1 && errno == EINTR);

	if(rv <= 0)
	{
		return -1;
	}

	struct timeval tmout;
	fd_set readfds;
	FD_ZERO(&readfds);

	tmout.tv_sec = 0;
	tmout.tv_usec = TIMEOUT_PERSERVER;
        FD_SET(sockfd, &readfds);
	rv = select(sockfd+1, &readfds, NULL, NULL, &tmout);
	if (rv == -1){
		return -1;
	}
	else if(rv)
	{
		do {
			rv = recvfrom(sockfd, uuid, sizeof(uint64_t), 0,(struct sockaddr *) 0, 
					(socklen_t *) 0);
		} while(rv == -1 && errno == EINTR);

		if(rv <= 0)
			return -1;
		return 0;
	}
	else
		return -1;
}


int uuid_init()
{
    int rv = shm_attach(&shm,SHM_FILE);
    if(rv != 0)
    {
	rv = shm_create(&shm,SHM_SIZE,SHM_FILE);
	if( rv != 0)
	{
	    printf("shm creat errno:%d\n",rv);
	    return -1;
	}
	uuid_data = shm.base;
	uuid_data->time_last = time(NULL);  
	uuid_data->fudge = 0;
    }
    else
    {
	uuid_data = shm.base;
    }
    return 0;
}

int uuid_fini()
{
    shm_detach(&shm);
    int rv = shm_remove(SHM_FILE);
    if(rv < 0)
	printf("remvoe shm failed!\n");
    return 0;
}

int gen_uuid_local(int uuid_type,uint64_t *uuid)
{
    int svrid = 1;
    uint64_t time_now;

    time_now = time(NULL);
    if (uuid_data->time_last > time_now)
	return RET_TM_ERROR;

    if (uuid_data->time_last != time_now) {
	uuid_data->fudge = 0;
	uuid_data->time_last = time_now;  
    }
    else {
	if(uuid_data->fudge >= MAX_PER_SECOND - 1)
	    return RET_FUDGE_OVERFLOW;
	++uuid_data->fudge; 
    }                                 
    if(uuid_type > UUID_TYPE_MAX)
	return RET_IDTYPE_ERROR;

    *uuid = (time_now << 32) | (uuid_data->fudge << 18) | svrid << 30 | uuid_type;
    return 0;
}

/*svrlist:"192.168.1.1:9991,192.168.1.2:9992"*/
int gen_uuid(const char *svrlist,int uuid_type,uint64_t *uuid)
{	
	int rv;
	const char *token = ",";

	char *state = NULL;
	char *strret = NULL;

	char buf[UUID_BUFSIZE];
	memset(buf,0,sizeof(buf));
	strcpy(buf,svrlist);
	char *str = buf;

	char *ip = NULL;
	char *port = NULL;
	const char *token2 = ":";
	char *state2 = NULL;
	int nport;

	if(uuid_type < 0)
		return -1;

	while((strret = strtok_r(str,token,&state)))
	{

		ip = strtok_r(strret,token2,&state2);
		if(!ip)
			return -1;
		port = strtok_r(NULL,token2,&state2);
		if(!port)
			return -1;
		nport = atoi(port);

		int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		rv = get_uuid(sockfd,ip,nport,uuid_type,uuid);
		close(sockfd);
		if(rv == 0)
		{
			if(*uuid < MAX_ERROR_CODE)
				rv = *uuid;
			break;
		}
		str = NULL;
	}
	return rv;
}



int uuid2hex(uint64_t uuid,unsigned char *buf,size_t bufsize)
{

	uint64_t mask = 0x00000000000000FF;
	unsigned char a[8];
	int i=0;
	for(;i<8;++i)
	{
		a[i]= (uuid & (mask << 8*i)) >> (8*i);
	}
	memset(buf,0,bufsize);
	snprintf((char *)buf,(size_t)bufsize,"%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
			a[7],a[6],a[5],a[4],a[3],a[2],a[1],a[0]);
	return 0;

}

int hex2uuid(unsigned char *buf,uint64_t *uuid)
{
	char *endptr;
	*uuid=strtoull((const char *)buf,&endptr,16);
	return 0;
}

int parse_uuid(uint64_t uuid, uuid_t *uuid_data)
{
	uuid_data->time = (int)(uuid >> 32);
	uint64_t mask = 0x00000000FF000000;
	uuid_data->fudge = (uuid & mask) >> 24;
	mask = 0x0000000000FF0000;
	uuid_data->svrid = (uuid & mask) >> 23;
	mask = 0x00000000000000FF;
	uuid_data->type = (uuid &mask);
	return 0;
}
