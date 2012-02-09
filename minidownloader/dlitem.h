#ifndef DL_ITEM_H
#define DL_ITEM_H
#include <windows.h>
#include "http/utility.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

struct dlitem{
	dlitem()
	{
		memset(resource,0,sizeof(resource));
		memset(path,0,sizeof(path));
		memset(md5,0,sizeof(md5));
		size = 0;
		node = NULL;
		method = -1;
	}
	xmlNodePtr node;
	char resource[MAX_RESOURCE_LEN];
	char path[MAX_PATH];
	char md5[33];
	UINT32 size;
	int method;
};
#endif