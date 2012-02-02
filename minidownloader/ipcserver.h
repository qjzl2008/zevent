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
    int start(void);
	int stop(void);

private:
	static DWORD thread_entry(LPVOID pParam); 

private:
    HANDLE m_hRecvThread;
    int m_shutdown;
};
#endif 