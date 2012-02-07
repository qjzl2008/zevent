extern "C"{
#include "os.h"
#include "os_common.h"
#include "http.h"
#include "utility.h"
#include "error.h"
#include "md5.h"
#include "encode.h"
};
#include <windows.h>
#include "dlmanager.h"
#include "thread_mutex.h"
#include <process.h>

dlmanager * dlmanager::pInstance = NULL;

dlmanager::dlmanager()
{
	m_phThreads = NULL;
	req_queue = NULL;
	m_nThreadCount = 0;
	shutdown = 0;
	filenums = 0;
	filennums_done = 0;
	m_dwRateDwn = 0;
	m_dwDwn = 0;

	pInstance = this;
}

int dlmanager::init_conn_pool()
{
	char lpModuleFileName[MAX_PATH] = {0};
	char lpIniFileName[MAX_PATH] = {0};
	TCHAR *file;
	GetModuleFileName(NULL,lpModuleFileName,MAX_PATH);
	GetFullPathName(lpModuleFileName,MAX_PATH,lpModuleFileName,&file);
	*file = 0;
	strcpy_s(lpIniFileName,sizeof(lpIniFileName),lpModuleFileName);
	strcat_s(lpIniFileName,sizeof(lpIniFileName),"dl_cfg.ini");
	char szServerIP[64] = {0};
	GetPrivateProfileString("network","server","",szServerIP,sizeof(szServerIP),lpIniFileName);
	strcpy_s(cfg.host,sizeof(cfg.host),szServerIP);
	cfg.port = GetPrivateProfileInt("network","port",80,lpIniFileName);
	cfg.nmin = GetPrivateProfileInt("network","min_conn",2,lpIniFileName);
	cfg.nmax = GetPrivateProfileInt("network","max_conn",10,lpIniFileName);
	cfg.nkeep = GetPrivateProfileInt("network","keep_conn",5,lpIniFileName);
    cfg.exptime = GetPrivateProfileInt("network","exptime",0,lpIniFileName);
	cfg.timeout = GetPrivateProfileInt("network","timeout",1000,lpIniFileName);
	int rv = conn_pool_init(&cfg);
	if(rv != 0)
		return -1;
	return 0;
}

int dlmanager::init(void)
{
	int rv = filelist.init("dllist.xml");
	if(rv != 0)
		return -1;
	rv = init_timer_socket();
	if(rv < 0)
		return -1;
	rv = init_conn_pool();
	if(rv < 0)
		return -1;

	filenums = filelist.get_file_nums();

	queue_create(&req_queue,1000);

	m_nThreadCount = cfg.nkeep;
	m_phThreads = new HANDLE[m_nThreadCount];
	for(int i = 0; i< m_nThreadCount; ++i)
	{
		m_phThreads[i] = INVALID_HANDLE_VALUE;
	}
	m_pdwThreaIDs = new DWORD[m_nThreadCount];

	for(int i = 0;i < m_nThreadCount; i++)
	{
		DWORD dwThreadID = 0;
		m_phThreads[i] = (HANDLE)_beginthreadex(NULL,
			0,
			(unsigned int (__stdcall *)(void *))dlmanager::dlthread_entry,
			this,
			0,
			(unsigned int *)&m_pdwThreaIDs[i]);
	}
	m_hTickThread = (HANDLE)_beginthreadex(NULL,
		0,
		(unsigned int (__stdcall *)(void *))dlmanager::tick_thread_entry,
		this,
		0,
		NULL);
	return 0;
}

int dlmanager::http_uri_encode(const char  *utf8_uri,char *enc_uri)
{
	char buf[64];
	unsigned char c;
	int i;

	for(i = 0; i < strlen(utf8_uri); i++)
	{
		c = utf8_uri[i];
		if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c<= '9') || c== '.' || c == '-' ||
			c == '_' || c== '/')
		{
			*enc_uri++ = c;
			continue;
		}

		sprintf_s(buf,"%02x",c);
		*enc_uri++ = '%';
		*enc_uri++ = buf[0];
		*enc_uri++ = buf[1];
	}

	//free(utf8_uri);
	return 0;
}

int dlmanager::process_file(dlitem *item,char *data)
{
	//验证MD5
	char md5[64]={0};
	file_md5(md5,sizeof(md5),data,item->size);
	if(strcmp(item->md5,md5))
	{
		return -1;
	}
	wchar_t w_path[MAX_PATH] = {0};
	size_t inbytes = strlen(item->path);
	size_t outbytes = sizeof(w_path);
	int rv = conv_utf8_to_ucs2(item->path,&inbytes,w_path,&outbytes);
	DWORD dwNum = WideCharToMultiByte(CP_ACP,NULL,w_path,-1,NULL,0,NULL,FALSE);
	char *c_path;
	c_path = new char[dwNum];
	rv = WideCharToMultiByte (CP_OEMCP,NULL,w_path,-1,c_path,dwNum,NULL,FALSE);
	
	if(item->method == STANDALONE)
	{
		HANDLE hFile;
		hFile = CreateFile(c_path,GENERIC_WRITE,0,NULL,
			CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if(hFile == INVALID_HANDLE_VALUE)
		{
			delete []c_path;
			return -1;
		}
		DWORD written = 0;
		BOOL rv = WriteFile(hFile,data,item->size,&written,NULL);
		if(!rv || written != item->size)
		{
			delete []c_path;
			CloseHandle(hFile);
			DeleteFile(c_path);
			return -1;
		}
		CloseHandle(hFile);
	}
	delete []c_path;
	return 0;
}

int dlmanager::file_md5(char md5code[],
			 int size,
			 char *data,unsigned len)
{
	unsigned char ss[16];
	struct MD5Context md5c;
	MD5Init( &md5c );
	MD5Update( &md5c, (unsigned char *)data, len );
	MD5Final( ss, &md5c );
	int off = 0;
	for(int i=0; i<16; i++ )
	{
		sprintf_s(md5code+off,size-off,"%02x", ss[i] );
		off += 2;
	}
	return 0;
}

int dlmanager::dlonefile(dlitem *item)
{
	int ret;
	char enc_uri[2048] = {0};
	char host[MAX_HOST_LEN];
	char url[MAX_URL_LEN] = {0};
	short int port;
	char resource[MAX_RESOURCE_LEN];
	HTTP_GetMessage * gm;
	char *data;

	sprintf_s(url,sizeof(url),"http://%s:%d/%s",
		cfg.host,cfg.port,item->resource);
	if((ret = Parse_Url(url, host, resource, &port)) != OK) {
		return -1;
	}

	ret = http_uri_encode(resource,enc_uri);

	if((ret = ZNet_Generate_Http_Get(host, enc_uri, port, &gm)) != OK) {
		return -1;
	}
	OsSocket s;
	void *res = conn_pool_acquire(&cfg);
	if(!res)
	{
		return -1;
	}
	s.sock= *(SOCKET *)res;
	if((ret = ZNet_Http_Request_Get_KL(&s,gm, &data)) != OK) {
		conn_pool_remove(&cfg,res);
	}
	else
	{
		conn_pool_release(&cfg,res);
		process_file(item,data);
		free(data);
		InterlockedExchangeAdd((LONG volatile *)&m_dwDwn,item->size);
		//filelist.set_dlitem_finish(item);
	}
	(void)ZNet_Destroy_Http_Get(&gm);
	return 0;
}

int dlmanager::get_from_dllist(dlitem *&item)
{
	int rv = filelist.get_next_dlitem(item);
	return rv;
}

int dlmanager::put_to_dllist(dlitem *item)
{
	int rv = filelist.put_to_dllist(item);
	return rv;
}

int dlmanager::return_to_dllist(dlitem *item)
{
	int rv = filelist.return_to_dllist(item);
	return rv;
}

int dlmanager::remove_from_runlist(dlitem *item)
{
	int rv = filelist.remove_from_runlist(item);
	return rv;
}

int dlmanager::init_timer_socket(void)
{
	m_TimerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(m_TimerSocket == SOCKET_ERROR)
		return -1;
	return 0;
}

int dlmanager::net_tick(void)
{
	m_dwRateDwn = m_dwDwn;
	InterlockedExchange((long *)&m_dwDwn,0);
	return 0;
}

DWORD dlmanager::tick_thread_entry(LPVOID pParam)
{
	dlmanager *pdlmanger = (dlmanager *)pParam;
	fd_set readset;
	int rev;
	DWORD wsacode;

	struct timeval timeout;
	timeout.tv_sec = 1;//1s
	timeout.tv_usec = 0;
	while(!pdlmanger->shutdown)
	{
		FD_ZERO(&readset);
		FD_SET(pdlmanger->m_TimerSocket,&readset);
		if((rev = select(0,&readset,NULL,NULL,&timeout)) == SOCKET_ERROR) {
			wsacode = WSAGetLastError();
			break;
		}
		if(rev == 0)
		{
			//超时
			pdlmanger->net_tick();
		}
	}
	return 0;
}

DWORD dlmanager::dlthread_entry(LPVOID pParam)
{
	int rv;
	dlmanager *pdlmanger = (dlmanager *)pParam;
	while(!pdlmanger->shutdown)
	{
		dlitem *item;
		rv = pdlmanger->get_from_dllist(item);
		if(rv == 0)
		{
			rv = pdlmanger->dlonefile(item);
			if(rv != 0)
			{
				pdlmanger->return_to_dllist(item);
			}
			else
			{
				//从下载列表中移除
				pdlmanger->remove_from_runlist(item);
				delete item;
				InterlockedIncrement((long *)(&pdlmanger->filennums_done));
			}
		}
		Sleep(50);
	}
	return 0;
}

int dlmanager::fini(void)
{
	shutdown = 1;
	WaitForMultipleObjects(m_nThreadCount,m_phThreads,TRUE,INFINITE);
	for(int i = 0; i < m_nThreadCount; ++i)
	{
		if(m_phThreads[i] != INVALID_HANDLE_VALUE)
			CloseHandle(m_phThreads[i]);
	}
	closesocket(m_TimerSocket);
	WaitForSingleObject(m_hTickThread,INFINITE);
	if(req_queue)
		queue_destroy(req_queue);
	conn_pool_fini(&cfg);
	filelist.fini();
	return 0;
}

dlmanager::~dlmanager(void)
{
}
