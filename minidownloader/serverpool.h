#ifndef DL_SERVER_POOL_H
#define DL_SERVER_POOL_H
#include <windows.h>
#include "conn_pool.h"

struct conn_info_t
{
	conn_info_t(void)
	{
		memset(host,0,sizeof(host));
		port = 0;
		memset(webroot,0,sizeof(webroot));
		conn = NULL;
		idx = 0;
	}
	char host[512];
	int port;
	char webroot[MAX_PATH];
	int idx;
	void *conn;
};

struct webroot_t{
	webroot_t()
	{
		memset(webroot,0,sizeof(webroot));
	}
	char webroot[MAX_PATH];
};

class serverpool
{
public:
	serverpool(void);
	~serverpool(void);

	static serverpool * Instance()
	{
		return pInstance;
	}

	int init_server_pool(const char *cfg);
	int fini_server_pool();

	int acquire_conn(conn_info_t *conn);
	int release_conn(conn_info_t *conn);
	int remove_conn(conn_info_t *conn);

private:
	conn_svr_cfg *connpool;
	webroot_t *webroots;
	int m_nServerNum;

private:
	static serverpool *pInstance;
};
#endif