#ifndef DL_LIST_H
#define DL_LIST_H

#include <list>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "thread_mutex.h"
#include "dlitem.h"

struct _equal_list_
{
	bool operator()(const dlitem* item1, const dlitem* item2)
	{
		if(!strcmp(item1->md5,item2->md5))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
};

class dllist
{
public:
	typedef std::list<dlitem *> filelist_t;
	typedef std::list<dlitem *>::iterator filelist_iter;

	dllist(void);
	~dllist(void);

	int init(const char *listfile);
	int fini(void);
	int get_next_dlitem(dlitem *&item);
	int put_to_dllist(dlitem *item);
	int return_to_dllist(dlitem *item);
	int remove_from_runlist(dlitem *item);

	int set_dlitem_finish(dlitem *item);
	int save(void);

private:
	char listname[MAX_PATH];

	thread_mutex_t *list_mutex;
	filelist_t filelist;
	filelist_t running_list;

	thread_mutex_t *doc_mutex;
	xmlDocPtr doc;
};
#endif