extern "C"{
#include "os.h"
#include "os_common.h"
#include "utility.h"
#include "error.h"
#include "md5.h"
#include "encode.h"
};
#include <windows.h>
#include "dlmanager.h"
#include "thread_mutex.h"
#include "protocol.h"
#include "ipcserver.h"
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
	m_bytes_in = 0;
	m_bw_bytes_in = 0;
	net_bw_limit_in = 0;

	pInstance = this;
	InitializeCriticalSectionAndSpinCount(&m_bwMutex,512);
	memset(m_IniFileName,0,sizeof(m_IniFileName));
	memset(m_szWebRoot,0,sizeof(m_szWebRoot));
}

int dlmanager::init_conn_pool()
{
	char szServerIP[64] = {0};
	GetPrivateProfileString("network","server","",szServerIP,sizeof(szServerIP),m_IniFileName);
	strcpy_s(cfg.host,sizeof(cfg.host),szServerIP);
	cfg.port = GetPrivateProfileInt("network","port",80,m_IniFileName);
	cfg.nmin = GetPrivateProfileInt("network","min_conn",2,m_IniFileName);
	cfg.nmax = GetPrivateProfileInt("network","max_conn",10,m_IniFileName);
	cfg.nkeep = GetPrivateProfileInt("network","keep_conn",5,m_IniFileName);
    cfg.exptime = GetPrivateProfileInt("network","exptime",0,m_IniFileName);
	cfg.timeout = GetPrivateProfileInt("network","timeout",1000,m_IniFileName);
	int rv = conn_pool_init(&cfg);
	if(rv != 0)
		return -1;
	return 0;
}

int dlmanager::init(void)
{
	char lpModuleFileName[MAX_PATH] = {0};
	TCHAR *file;
	GetModuleFileName(NULL,lpModuleFileName,MAX_PATH);
	GetFullPathName(lpModuleFileName,MAX_PATH,lpModuleFileName,&file);
	*file = 0;
	strcpy_s(m_IniFileName,sizeof(m_IniFileName),lpModuleFileName);
	strcat_s(m_IniFileName,sizeof(m_IniFileName),"dl_cfg.ini");

	char szList[MAX_PATH]={0};
	GetPrivateProfileString("misc","web_root","",m_szWebRoot,sizeof(m_szWebRoot),m_IniFileName);
	GetPrivateProfileString("misc","dllist","",szList,sizeof(szList),m_IniFileName);

	int rv = filelist.init(szList);
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

int dlmanager::rate(DWORD down)
{
	EnterCriticalSection(&m_bwMutex);
    if(down > 0)
	{
		net_bw_limit_in = down;
		m_bw_bytes_in = down;
	}
	LeaveCriticalSection(&m_bwMutex);
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
	gen_md5(md5,sizeof(md5),data,item->size);
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

int dlmanager::gen_md5(char md5code[],
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

int dlmanager::http_request_get(dlitem *item,OsSocket *s,HTTP_GetMessage * gm,
					 char ** response)
{
	int ret;
	char * http_get_request;
	int content_length;
	char * http_header;

	if((ret = Create_Get_Request_KL(gm, &http_get_request)) != OK) {
		return -1;
	}
	/* send an HTTP request */
	if((ret = Send_Http_Request(s, http_get_request)) != OK) {
		free(http_get_request);
		return -1;
	}
	free(http_get_request);

	if((ret = Get_Http_Header(s, &http_header)) != OK) {
		return -1;
	}

	/* get http data size from the header */
	if((ret = Get_Http_Content_Length(http_header, &content_length)) != OK) {
		free(http_header);
		return -1;
	}
	free(http_header);

	if(content_length != item->size)
	{
		return -1;
	}

	//Get http content
	char *http_content = (char *)malloc(content_length + NULL_TERM_LEN);
	if(NULL == *http_content) {
		return -1;
	}

	int recv = 0,received = 0,remain = content_length;

	while(remain > 0 && !shutdown)
	{
		int want_read = 0;
		if(net_bw_limit_in == 0)
		{
			//不进行限速
			want_read = content_length;
		}
		else
		{
			EnterCriticalSection(&m_bwMutex);
			if(m_bw_bytes_in >= remain)
			{
				want_read = remain;
				m_bw_bytes_in -= remain;
			}
			else
			{
				//这里我们平均分配下可用余量吧
				want_read = m_bw_bytes_in / m_nThreadCount;
				m_bw_bytes_in -= want_read;
			}
			LeaveCriticalSection(&m_bwMutex);
		}

		if(want_read <= 0)
		{
			Sleep(100);
			continue;
		}

		if((ret = ZNet_Os_Socket_Recv(s, http_content, want_read, 
			&recv, HTTP_RECEIVE_TIMEOUT)) != OK) {
				free(http_content);
				return -1;
		}
		InterlockedExchangeAdd((LONG volatile *)&m_bytes_in,recv);
		received += recv;
		remain -=recv;
	}
	if(remain > 0)
	{
		free(http_content);
		return -1;
	}

	/* null terminate it */
	http_content[received] = NULL_TERM;
	*response = http_content;
	return 0;
}

int dlmanager::check_pfile(dlitem *item)
{
	return 0;
}

int dlmanager::check_standalone(dlitem *item)
{
	wchar_t w_path[MAX_PATH] = {0};
	size_t inbytes = strlen(item->path);
	size_t outbytes = sizeof(w_path);
	int rv = conv_utf8_to_ucs2(item->path,&inbytes,w_path,&outbytes);
	DWORD dwNum = WideCharToMultiByte(CP_ACP,NULL,w_path,-1,NULL,0,NULL,FALSE);
	char *c_path;
	c_path = new char[dwNum];
	rv = WideCharToMultiByte (CP_OEMCP,NULL,w_path,-1,c_path,dwNum,NULL,FALSE);
	HANDLE hFile = CreateFile(c_path,     
		GENERIC_READ,    
		0,                       
		NULL,                 
		OPEN_EXISTING,      
		FILE_ATTRIBUTE_NORMAL,   
		NULL                             
		);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		return -1;
	}
	else
	{
		DWORD dwFileSize = GetFileSize(hFile,NULL);
		if(dwFileSize == INVALID_FILE_SIZE)
		{
			CloseHandle(hFile);
			return -1;
		}
		DWORD dwToReadCharNums = dwFileSize;
		DWORD dwReadBytes = 0;
		char  *pFileData = NULL;
		pFileData = (char *)malloc(dwFileSize + 1);
		memset(pFileData,0,(dwFileSize + 1));
		while(dwReadBytes < dwFileSize)
		{
			DWORD dwRecvBytes = 0;
			BOOL bReadFile = ReadFile(hFile,           
				pFileData ,   
				dwToReadCharNums,
				&dwRecvBytes,
				NULL                  
				);

			if( FALSE == bReadFile ){
				CloseHandle(hFile);
				return -1;
			}
			dwReadBytes += dwRecvBytes;
			dwToReadCharNums = dwFileSize - dwReadBytes;
		}
		CloseHandle(hFile);
		char md5[64]={0};
		gen_md5(md5,sizeof(md5),pFileData,dwReadBytes);
		free(pFileData);
		if(strcmp(item->md5,md5))
		{
			return -1;
		}
	}
	return 0;
}

int dlmanager::check_file(dlitem *item)
{
	if(item->method == PACK)
	{
		return check_pfile(item);
	}
	else
	{
		return check_standalone(item);
	}
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

	sprintf_s(url,sizeof(url),"http://%s:%d/%s/%s",
		cfg.host,cfg.port,m_szWebRoot,item->resource);
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
	if((ret = http_request_get(item,&s,gm, &data)) != 0) {
		conn_pool_remove(&cfg,res);
	}
	else
	{
		conn_pool_release(&cfg,res);
		process_file(item,data);
		free(data);
		//filelist.set_dlitem_finish(item);
	}
	(void)ZNet_Destroy_Http_Get(&gm);
	return ret;
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
	InterlockedExchange((long *)&m_dwRateDwn,m_bytes_in);
	InterlockedExchange((long *)&m_bytes_in,0);
	InterlockedExchange((long *)&m_bw_bytes_in,net_bw_limit_in);
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

int dlmanager::notify_clients(dlitem *item)
{
	char buf[1024];
	memset(buf,0,sizeof(buf));
	nb_ipcmsg_t ipcmsg;
	strcpy_s(ipcmsg.path,sizeof(ipcmsg.path),item->path);
	ipcmsg.file_size = item->size;
	ipcmsg.nb_type = MSG_RESPONSE;
	int len = sizeof(ipcmsg);
	int nlen = htonl(len);
	memcpy(buf,&nlen,sizeof(nlen));
	memcpy(buf+sizeof(len),&ipcmsg,len);
	len+= sizeof(len);
	ipc_server::Instance()->broadcastmsg(buf,len);
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
			if(pdlmanger->check_file(item) == 0)
			{
				pdlmanger->remove_from_runlist(item);
				continue;
			}
			rv = pdlmanger->dlonefile(item);
			if(rv != 0)
			{
				pdlmanger->return_to_dllist(item);
			}
			else
			{
				//从下载列表中移除
				pdlmanger->remove_from_runlist(item);
				InterlockedIncrement((long *)(&pdlmanger->filennums_done));
				pdlmanger->notify_clients(item);
				delete item;
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
	DeleteCriticalSection(&m_bwMutex);
}
