#ifndef __STORE_H_
#define __STORE_H_

#include <time.h>

enum {MAX_VALUE_LEN = 8192};
typedef struct data_t
{
	char value[MAX_VALUE_LEN];
	time_t curTime;
}DATA,*PDATA;

enum {key_len = 256};
typedef struct store_t
{
	char dbname[key_len];
	char key[key_len];
	DATA data;
	
		
}STORE,*PSTORE;

char* data_serialize(DATA *pdata,char *buffer,size_t buflen);
char* data_unserialize(DATA *pdata,char *buffer,size_t buflen);

char* store_serialize(STORE *pstore,char *buffer,size_t buflen);
char * store_unserialize(STORE *pstore,char *buffer,size_t buflen);
#endif
