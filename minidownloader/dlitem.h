#ifndef DL_ITEM_H
#define DL_ITEM_H
#include <windows.h>
#include "utility.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

struct dlitem{
	dlitem()
	{
		memset(url,0,sizeof(url));
		memset(fpath,0,sizeof(fpath));
		memset(pack_path,0,sizeof(pack_path));
		memset(md5,0,sizeof(md5));
		size = 0;
		node = NULL;
		method = -1;
	}
	xmlNodePtr node;
	char url[MAX_URL_LEN];
	char fpath[MAX_PATH];
	char pack_path[MAX_PATH];
	char md5[64];
	UINT32 size;
	int method;
};
#endif