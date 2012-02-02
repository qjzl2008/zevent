#include "dllist.h"
#include <iostream>
using namespace std;

dllist::dllist(void)
{
	doc = NULL;
	memset(listname,0,sizeof(listname));
	thread_mutex_create(&list_mutex,THREAD_MUTEX_DEFAULT);
	thread_mutex_create(&doc_mutex,THREAD_MUTEX_DEFAULT);
}

dllist::~dllist(void)
{
	thread_mutex_destroy(list_mutex);
	thread_mutex_destroy(doc_mutex);
}

int dllist::init(const char *listfile)
{
	char* name=NULL;
	char* value=NULL;
	xmlKeepBlanksDefault (0); 
	doc=xmlParseFile(listfile);//创建Dom树
	if(doc==NULL)
	{
		return -1;
	}
	xmlNodePtr  cur;
	cur=xmlDocGetRootElement(doc);//获取根节点
	if(cur==NULL)
	{
		xmlFreeDoc(doc); 
		return -1;
	}

	cur=cur->xmlChildrenNode;
	strcpy_s(listname,sizeof(listname),listfile);

	while(cur)
	{
		dlitem *item = new dlitem;
		item->node = cur;
		name=(char*)(cur->name); 
		//value=(char *)xmlNodeGetContent(cur);
		value = (char *)xmlGetProp(cur, (const xmlChar *)"download_path");
		if(value)
		{
			strcpy_s(item->resource,sizeof(item->resource),value);
			//cout<<"name is: "<<name<<", value is: "<<value<<endl;
			xmlFree(value); 
		}


		value = (char *)xmlGetProp(cur, (const xmlChar *)"path");
		if(value)
		{
			strcpy_s(item->path,sizeof(item->path),value);
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

		if(!finish)
		{
			filelist.push_back(item);
		}
		cur=cur->next;
	}
	return 0;
}

int dllist::fini(void)
{
	xmlFreeDoc(doc);//释放xml解析库所用资源
	xmlCleanupParser();

	dlitem *item = NULL;
	filelist_iter iter = filelist.begin();
	for(; iter != filelist.end(); iter++)
	{
		item = reinterpret_cast<dlitem *>(*iter);
		delete item;
	}
	filelist.clear();
	return 0;
}

int dllist::put_to_dllist(dlitem *item)
{
	thread_mutex_lock(list_mutex);
	filelist.push_back(item);
	thread_mutex_unlock(list_mutex);
	return 0;
}

int dllist::get_next_dlitem(dlitem *&item)
{
	thread_mutex_lock(list_mutex);
	if(filelist.empty())
	{
		thread_mutex_unlock(list_mutex);
		return -1;
	}
	item = filelist.front();
	filelist.pop_front();
	thread_mutex_unlock(list_mutex);
	return 0;
}

int dllist::set_dlitem_finish(dlitem *item)
{
	int rv = 0;
	thread_mutex_lock(doc_mutex);
	xmlNewProp(item->node,(const xmlChar *)"finish",(const xmlChar *)"1");
	rv = xmlSaveFormatFileEnc(listname,doc,"utf-8",1);
	thread_mutex_unlock(doc_mutex);
	return 0;
}
