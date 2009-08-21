#include "httpd.h"
#include "http_log.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>        /* netbd.h is needed for struct hostent  */
#include <time.h>
#include <string.h>
#include "Protocol.h"
#include "Rio.h"
#include "RT_Svr.h"

//#define MAXDATASIZE 4096
#define CMD_OFFSET 4
#define INFO_OFFSET 8

int connect_server(const char *ip,int port)
{
    if(!ip)
	    return -1;
    int fd=-1, numbytes;   /* files descriptors */
    struct sockaddr_in server;  /* server's address information */
    struct in_addr addr;

	if(inet_aton(ip,&addr)  == 0) {
        printf("ip errer!\n");
        return -1;
    }
    if ((fd=socket(AF_INET, SOCK_STREAM, 0))==-1){  /* calls socket() */
        printf("socket() error\n");
        return -1;
    }

    bzero(&server,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port); /* htons() is needed again */
    server.sin_addr = addr;

    if(connect(fd, (struct sockaddr *)&server,sizeof(struct sockaddr))==-1){ /* calls connect() */
        printf("connect() error\n");
        return -1;
    }
	return fd;
}

int connect_close(int fd)
{
	return close(fd);
}

static char* unserialize(PSTORE pstore,char *buffer,int buflen)
{
	char *p = buffer;

	memcpy(&pstore->data.value[0],p,strlen(p) + 1);
	p += strlen(pstore->data.value) + 1;

	memcpy(&(pstore->data.curTime),p,sizeof(pstore->data.curTime));
	p += sizeof(pstore->data.curTime);

	memcpy(pstore->dbname,p,strlen(pstore->dbname) + 1);
	p += strlen(p) + 1;
	memcpy(pstore->key,p,strlen(pstore->key) + 1);
	p += strlen(p) + 1;


	return p;
}

static char* data_serialize(PDATA pdata,char *buffer,int buflen)
{
	char *p = buffer;
	int size = 0;
	
	memset(buffer,0,buflen);
	size = strlen(pdata->value) + 1 + sizeof(pdata->curTime);
	if(buflen < size)
		return NULL;
	else {
		memcpy(p,pdata->value,strlen(pdata->value) + 1);
		p += strlen(pdata->value) + 1;

		memcpy(p,&(pdata->curTime),sizeof(pdata->curTime));
		p += sizeof(pdata->curTime);
	}
	return p;
}

static char* serialize(PSTORE pstore,char *buffer,int buflen)
{

	char *p = buffer;
	int size = 0;
	
	memset(buffer,0,buflen);
	size = strlen(pstore->data.value) + 1  + sizeof(time_t)
		+ strlen(pstore->dbname) + 1 + strlen(pstore->key) + 1;
	if(buflen < size)
		return NULL;
	else {
		p = data_serialize(&(pstore->data),p,buflen);

	        memcpy(p,pstore->dbname,strlen(pstore->dbname) + 1);	
		p += strlen(pstore->dbname) + 1;

		memcpy(p,pstore->key,strlen(pstore->key) + 1);
		p += strlen(pstore->key) + 1;

	}
	return p;
}
static size_t decode_len(unsigned char *code)
{
	int i;
	size_t len = 0;
	for(i = 0; i < 4; i++) {
		len |= (size_t)code[i] << (8 * i);
	}
	return len;
}

static void code_len(size_t len,unsigned char *code)
{
	int i;
	for(i = 0; i < 4; i++) {
		{

			code[i] = (len >> (8 * i)) & 0xff;
		}
	}
}

static int send_command(int fd,unsigned int cmd,unsigned char *info,int info_size)
{
	int len = info_size + sizeof(cmd);
	unsigned char len_buf[4];
	memset(len_buf,0,4);

	code_len(len,len_buf);
	memcpy(info,&len_buf,4);

	memcpy(info+4,&cmd,sizeof(cmd));
	len = info_size + INFO_OFFSET;
	
	if(rio_wirten(fd,info,len) != len)
		return -1;

	return 0;
}

static int dispose(char* msg,int len,PSTORE store)
{
	int ret = 0;
	TDATA tdata;
	memset(&tdata,0,sizeof(TDATA));
		 
	memcpy(&tdata.command,msg,sizeof(tdata.command));
	tdata.data = msg + 2*sizeof(tdata.command);
	unsigned int *result = (unsigned int*)(msg + sizeof(tdata.command));
		 
	switch(tdata.command)
	{
	case CMD_DATA_GET:
		if(*result == CMD_SUCCESSFUL)
		{
			unserialize(store,(char*)tdata.data,len);

		}
		else
		{
			ret = -1;
		}
		break;
	case CMD_DATA_SET:
		if(*result != CMD_SUCCESSFUL)
			ret = -1;
		break;
	default:
		ret = -2;
	}
	return ret;
}

int exec_c(int fd,PSTORE pstore,int cmd)
{
	unsigned char buffer[2*MAX_VALUE_LEN];
	char *p = buffer + INFO_OFFSET;
	int info_len = 0;
	
	memset(buffer,0,2*MAX_VALUE_LEN);

	char *pRet = serialize(pstore,p,2*MAX_VALUE_LEN - INFO_OFFSET);
	if(pRet == NULL)
		return -1;
	info_len = pRet - p;
	
	if(send_command(fd,cmd,buffer,info_len) != 0)
		return -2;
	memset(buffer,0,2*MAX_VALUE_LEN);
	if(rio_readn(fd,buffer,CMD_OFFSET) != CMD_OFFSET)
		return -2;

	info_len = decode_len(buffer);

	if(rio_readn(fd,buffer+CMD_OFFSET,info_len) != info_len)
		return -2;
	int ret = dispose(buffer+CMD_OFFSET,2*MAX_VALUE_LEN - CMD_OFFSET,pstore);
       	return ret;
}

