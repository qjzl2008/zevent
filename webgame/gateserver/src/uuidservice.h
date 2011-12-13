#ifndef UUID_SERVICE_H
#define UUID_SERVICE_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <strings.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

#define UUID_BUFSIZE (2048)
#define TIMEOUT_PERSERVER (500000)//200ms

#define RET_TM_ERROR 1
#define RET_FUDGE_OVERFLOW 2
#define RET_IDTYPE_ERROR 3
#define MAX_ERROR_CODE (100)

#define UUID_TYPE_CONNECTION (0)
#define UUID_TYPE_ACCOUNT (1)
#define UUID_TYPE_CHARACTER (2)

#define MAX_PER_SECOND (20000)
#define UUID_TYPE_MAX (60000)

typedef struct uuid_t{
       	int time;
        uint32_t fudge;
        unsigned char svrid;
        uint16_t type;
}uuid_t;

int gen_uuid(const char *svrlist,int uuid_type,uint64_t *uuid);

int uuid_init();
int uuid_fini();
int gen_uuid_local(int uuid_type,uint64_t *uuid);

int uuid2hex(uint64_t uuid,unsigned char *buf,size_t bufsize);

int hex2uuid(unsigned char *buf,uint64_t *uuid);

int parse_uuid(uint64_t uuid, uuid_t *uuid_data);

#ifdef __cplusplus
}
#endif


#endif
