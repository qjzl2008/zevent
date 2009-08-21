#include <string.h>
#include "store.h"

char* data_serialize(DATA *pdata,char *buffer,size_t buflen)
{
	char *p = buffer;
	int size = 0;
	
	memset(buffer,0,buflen);
	size = sizeof(pdata->curTime) + strlen(pdata->value) + 1;
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
	
char* data_unserialize(DATA *pdata,char *buffer,size_t buflen)
{
	char *p = buffer;
	memcpy(pdata->value,p,strlen(p) + 1);
	p += strlen(p) + 1;
	memcpy(&pdata->curTime,p,sizeof(pdata->curTime));
	p += sizeof(pdata->curTime);

	return p;
}


char* store_serialize(STORE *pstore,char *buffer,size_t buflen)
{
	char *p = buffer;
	int size = 0;
	
	memset(buffer,0,buflen);
	size = strlen(pstore->dbname) + 1 + strlen(pstore->key) + 1 + sizeof(pstore->data.curTime) + strlen(pstore->data.value) + 1;

	if(buflen < size)
		return NULL;
	else {
		p = data_serialize(&pstore->data,p,buflen);

		memcpy(p,pstore->dbname,strlen(pstore->dbname) + 1);
		p += strlen(pstore->dbname) + 1;
		memcpy(p,pstore->key,strlen(pstore->key) + 1);
		p += strlen(pstore->key) + 1;
		
	}
	return p;
}

char* store_unserialize(STORE *pstore,char *buffer,size_t buflen)
{
	char *p = buffer;
	p = data_unserialize(&pstore->data,p,buflen);

	memcpy(pstore->dbname,p,strlen(p)+1);
	p += strlen(p) + 1;

	memcpy(pstore->key,p,strlen(p)+1);
	p += strlen(p) + 1;

	return p;
}

