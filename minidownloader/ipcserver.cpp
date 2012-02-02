#include <process.h>
#include "ipcserver.h"

ipc_server::ipc_server()
{
	m_shutdown = 0;
	m_hRecvThread = INVALID_HANDLE_VALUE;
}

DWORD ipc_server::thread_entry(LPVOID pParam) 
{
	return 0;
}

int ipc_server::start(void)
{
    unsigned int dwThreadID = 0;
	m_hRecvThread = (HANDLE)_beginthreadex(NULL,
		0,
		(unsigned int (__stdcall *)(void *))ipc_server::thread_entry,
		this,
		0,
		&dwThreadID);
	return 0;
}

int ipc_server::stop(void)
{
	m_shutdown = 0;
	if(m_hRecvThread != INVALID_HANDLE_VALUE)
		WaitForSingleObject(m_hRecvThread,INFINITE);
	CloseHandle(m_hRecvThread);
	return 0;
}