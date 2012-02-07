#include <process.h>
#include "ipcserver.h"
#include "dlmanager.h"
#include "protocol.h"

ipc_server * ipc_server::pInstance = NULL;

ipc_server::ipc_server()
{
	m_shutdown = 0;
	m_hRecvThread = INVALID_HANDLE_VALUE;
	ns = NULL;
	memset(&ns_arg,0,sizeof(ns_arg));
	ns_arg.func = NULL;
    strcpy_s(ns_arg.ip,sizeof(ns_arg.ip),"127.0.0.1");
	ns_arg.max_peers = 1000;
	ns_arg.port = 7799;
	pInstance = this;
}

DWORD ipc_server::thread_entry(LPVOID pParam) 
{
	ipc_server *server = (ipc_server *)pParam;
	void *msg;uint32_t len;
	uint64_t peer_id;
	int rv;
	nb_ipcmsg_t ipcmsg;
	while(!server->m_shutdown)
	{
		rv = ns_recvmsg(server->ns,&msg,&len,&peer_id,1000000);
		if(rv == 0)
		{
			memcpy(&ipcmsg,(char *)msg+4,len-4);
			dlitem *item = new dlitem;
			strcpy_s(item->path,sizeof(item->path),ipcmsg.path);
			strcpy_s(item->resource,sizeof(item->path),ipcmsg.path);
			strcpy_s(item->md5,sizeof(item->md5),ipcmsg.check_code);
		    item->method = ipcmsg.method;
			item->size = ipcmsg.file_size;
			dlmanager::Instance()->put_to_dllist(item);
			ns_free(server->ns,msg);
		}
		else if(rv == 1)
		{
			printf("peer id:%llu disconnect!\n",peer_id);
		}
	}
	return 0;
}

int ipc_server::start(void)
{
	int rv = ns_start_daemon(&ns,&ns_arg);
	if(rv < 0)
		return -1;

    unsigned int dwThreadID = 0;
	m_hRecvThread = (HANDLE)_beginthreadex(NULL,
		0,
		(unsigned int (__stdcall *)(void *))ipc_server::thread_entry,
		this,
		0,
		&dwThreadID);
	return 0;
}

int ipc_server::ipc_port(int *port)
{
	*port = ns_arg.port;
	return 0;
}

int ipc_server::stop(void)
{
	m_shutdown = 1;
	if(m_hRecvThread != INVALID_HANDLE_VALUE)
		WaitForSingleObject(m_hRecvThread,INFINITE);
	CloseHandle(m_hRecvThread);
	ns_stop_daemon(ns);
	return 0;
}

int ipc_server::broadcastmsg(void *msg,uint32_t len)
{
	if(ns)
		ns_broadcastmsg(ns,msg,len);
	return 0;
}
