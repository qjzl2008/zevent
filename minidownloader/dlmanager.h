#ifndef DL_MANAGER_H
#define DL_MANAGER_H
#include <windows.h>
#include "queue.h"
#include <list>
#include "dlitem.h"
#include "conn_pool.h"

class dlmanager
{
public:
	typedef std::list<dlitem *> dllist_t;
	typedef std::list<dlitem *>::iterator dllist_iter;

	dlmanager(void);
	~dlmanager(void);
	int init(const char *dllist);
	int fini(void);
	int push_req(dlitem *);

	int shutdown;

private:
	static DWORD dlthread_entry(LPVOID pParam);
	int dlonefile(dlitem *item);
	int conv_ucs2_to_utf8(const wchar_t *in,
		size_t *inwords,
		char *out,
		size_t *outbytes);
	int http_uri_encode(const wchar_t *uri,size_t inwords,char *enc_uri);

	int init_conn_pool(void);

private:
	conn_svr_cfg cfg;

	int m_nThreadCount;
	HANDLE *m_phThreads;
	DWORD *m_pdwThreaIDs;
	queue_t *req_queue;

	thread_mutex_t *list_mutex;
};

#endif