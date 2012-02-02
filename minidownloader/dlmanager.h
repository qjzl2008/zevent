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

	dlmanager(void);
	~dlmanager(void);
	int init(void);
	int fini(void);
	int get_from_dllist(dlitem *&item);
	int put_to_dllist(dlitem *item);

	int shutdown;

private:
	static DWORD dlthread_entry(LPVOID pParam);

	int dlonefile(dlitem *item);

	int conv_ucs2_to_utf8(const wchar_t *in,
		size_t *inwords,
		char *out,
		size_t *outbytes);

	int conv_utf8_to_ucs2(const char *in, 
		size_t *inbytes,
		wchar_t *out, 
		size_t *outwords);

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