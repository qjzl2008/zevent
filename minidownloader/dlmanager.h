#ifndef DL_MANAGER_H
#define DL_MANAGER_H
#include <windows.h>
#include "queue.h"
#include <list>
#include "dlitem.h"
#include "conn_pool.h"
#include "dllist.h"

enum METHOD{
	STANDALONE = 0,
	PACK       = 1
};

class dlmanager
{
public:
	typedef std::list<dlitem *> dllist_t;
	typedef std::list<dlitem *>::iterator dllist_iter;

	dlmanager(void);
	~dlmanager(void);
	int init(void);
	int fini(void);
	int get_from_dllist(dlitem *&item);

	int shutdown;

private:
	static DWORD dlthread_entry(LPVOID pParam);
	int dlonefile(dlitem *item);
	int conv_ucs2_to_utf8(const wchar_t *in,
		size_t *inwords,
		char *out,
		size_t *outbytes);
	//int http_uri_encode(const wchar_t *uri,size_t inwords,char *enc_uri);
	int http_uri_encode(const char  *utf8_uri,char *enc_uri);

	int init_conn_pool(void);

	int process_file(dlitem *item,char *data);

private:
	conn_svr_cfg cfg;

	int m_nThreadCount;
	HANDLE *m_phThreads;
	DWORD *m_pdwThreaIDs;
	queue_t *req_queue;

	dllist filelist;
};

#endif