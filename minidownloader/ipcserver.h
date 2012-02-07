#ifndef IPC_SERVER_H
#define IPC_SERVER_H
#include <windows.h>
#include "thread_mutex.h"
#include "znet.h"

class ipc_server
{
public:
	ipc_server(void);
	~ipc_server(void){}

	static ipc_server * Instance()
	{
		return pInstance;
	}

    int start(void);
	int stop(void);
	int ipc_port(int *port);
	int broadcastmsg(void *msg,uint32_t len);

private:
	static DWORD thread_entry(LPVOID pParam); 

private:
	ns_arg_t ns_arg;
	net_server_t *ns;
    HANDLE m_hRecvThread;
    int m_shutdown;

private:
	static ipc_server *pInstance;
};
#endif 