#include "dllist.h"
#include <iostream>
using namespace std;

dllist::dllist(void)
{
	doc = NULL;
	cur = NULL;
	memset(listname,0,sizeof(listname));
	thread_mutex_create(&mutex,THREAD_MUTEX_DEFAULT);
}

dllist::~dllist(void)
{
	thread_mutex_destroy(mutex);
}

int dllist::init(const char *listfile)
{
	char* name=NULL;
	char* value=NULL;
	thread_mutex_lock(mutex);
	xmlKeepBlanksDefault (0); 
	doc=xmlParseFile(listfile);//创建Dom树
	if(doc==NULL)
	{
		thread_mutex_unlock(mutex);
		return -1;
	}
	cur=xmlDocGetRootElement(doc);//获取根节点
	if(cur==NULL)
	{
		thread_mutex_unlock(mutex);
		xmlFreeDoc(doc); 
		return -1;
	}

	cur=cur->xmlChildrenNode;
	strcpy_s(listname,sizeof(listname),listfile);
	thread_mutex_unlock(mutex);
	return 0;
}

int dllist::fini(void)
{
	thread_mutex_lock(mutex);
	xmlFreeDoc(doc);//释放xml解析库所用资源
	xmlCleanupParser();
	cur = NULL;
	thread_mutex_unlock(mutex);
	return 0;
}

int dllist::get_next_dlitem(dlitem *item)
{
	thread_mutex_lock(mutex);

	char *name,*value;
	int find = 0;
	while(cur)
	{
		item->node = cur;
		name=(char*)(cur->name); 
		//value=(char *)xmlNodeGetContent(cur);
		value = (char *)xmlGetProp(cur, (const xmlChar *)"download_path");
		if(value)
		{
			strcpy_s(item->url,sizeof(item->url),value);
			//cout<<"name is: "<<name<<", value is: "<<value<<endl;
			xmlFree(value); 
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"pack_path");
		if(value)
		{
			strcpy_s(item->pack_path,sizeof(item->pack_path),value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"file_path");
		if(value)
		{
			strcpy_s(item->fpath,sizeof(item->fpath),value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"check_md5");
		if(value)
		{
			strcpy_s(item->md5,sizeof(item->md5),value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"file_size");
		if(value)
		{
			item->size = atol(value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"method");
		if(value)
		{
			item->method = atoi(value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"finish");
		int finish = 0;
		if(value)
		{
			finish = atoi(value);
			xmlFree(value);
		}

		if(finish)
		{
			cur=cur->next;
			continue;
		}
		else
		{
			cur=cur->next;
			find = 1;
			break;
		}
	}
	thread_mutex_unlock(mutex);
	if(find)
		return 0;
	else
		return -1;
}

int dllist::set_dlitem_finish(dlitem *item)
{
	int rv = 0;
	thread_mutex_lock(mutex);
	xmlNewProp(item->node,(const xmlChar *)"finish",(const xmlChar *)"1");
	rv = xmlSaveFormatFileEnc(listname,doc,"utf-8",1);
	thread_mutex_unlock(mutex);
	return 0;
}
