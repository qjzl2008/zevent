#ifndef DL_MANAGER_H
#define DL_MANAGER_H
#include <windows.h>
#include "queue.h"
#include <list>
#include "dlitem.h"
#include "conn_pool.h"
#include "dllist.h"
extern "C"{
#include "http.h"
}

enum METHOD{
	STANDALONE = 0,
	PACK       = 1
};

class dlmanager
{
public:
	dlmanager(void);
	~dlmanager(void);
	static dlmanager * Instance()
	{
		return pInstance;
	}
	int init(void);
	int fini(void);
	int rate(DWORD down);
	int get_from_dllist(dlitem *&item);
	int put_to_dllist(dlitem *item);
	int return_to_dllist(dlitem *item);
	int remove_from_runlist(dlitem *item);

	int notify_clients(dlitem *item);

	int shutdown;

	int filenums;
	int filennums_done;
	DWORD m_dwRateDwn;

private:
	//dl thread
	static DWORD dlthread_entry(LPVOID pParam);
	//tick thread
	static DWORD tick_thread_entry(LPVOID pParam);
	int net_tick(void);

	int init_timer_socket(void);

	int http_request_get(OsSocket *s,HTTP_GetMessage * gm,
		char ** response);

	int dlonefile(dlitem *item);

	int http_uri_encode(const char  *utf8_uri,char *enc_uri);

	int init_conn_pool(void);

	int process_file(dlitem *item,char *data);

	int gen_md5(char md5code[],
		int size,
		char *data,unsigned len);

private:
	conn_svr_cfg cfg;

	int m_nThreadCount;
	HANDLE *m_phThreads;
	DWORD *m_pdwThreaIDs;
	queue_t *req_queue;

	dllist filelist;

	//定时器
	SOCKET m_TimerSocket;
	HANDLE m_hTickThread;

	//限速和速率计算
	DWORD m_bytes_in;
	DWORD m_bw_bytes_in;
	DWORD net_bw_limit_in;
	CRITICAL_SECTION m_bwMutex;
private:
	static dlmanager *pInstance;
};

#endif