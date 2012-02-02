#ifndef DL_LIST_H
#define DL_LIST_H

#include <list>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "thread_mutex.h"
#include "dlitem.h"
class dllist
{
public:
	typedef std::list<dlitem *> filelist_t;

	dllist(void);
	~dllist(void);

	int init(const char *listfile);
	int fini(void);
	int get_next_dlitem(dlitem *&item);
	int set_dlitem_finish(dlitem *item);
	int save(void);

private:
	char listname[MAX_PATH];

	thread_mutex_t *list_mutex;
	filelist_t filelist;

	thread_mutex_t *doc_mutex;
	xmlDocPtr doc;
};
#endif