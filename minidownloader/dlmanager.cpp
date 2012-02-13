extern "C"{
#include "http/os.h"
#include "http/os_common.h"
#include "http/utility.h"
#include "http/error.h"
#include "md5.h"
#include "encode.h"
};
#include <windows.h>
#include "dlmanager.h"
#include "thread_mutex.h"
#include "protocol.h"
#include "ipcserver.h"
#include <process.h>
#include "log.h"

dlmanager * dlmanager::pInstance = NULL;

dlmanager::dlmanager()
{
	m_phThreads = NULL;
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
}

int dlmanager::init_conn_pool(const char *svrlist)
{
	int rv = m_serverpool.init_server_pool(svrlist);
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
	strcat_s(m_IniFileName,sizeof(m_IniFileName),"minicfg.ini");

	char szList[MAX_PATH]={0},szServerList[MAX_PATH]={0};
	GetPrivateProfileString("misc","serverlist","",szServerList,sizeof(szServerList),m_IniFileName);
	GetPrivateProfileString("misc","minilist","",szList,sizeof(szList),m_IniFileName);
	m_nThreadCount = GetPrivateProfileInt("misc","threads",3,m_IniFileName);

	int rv = filelist.init(szList);
	if(rv != 0)
	{
		log("load download file list:%s failed!",szList);
		return -1;
	}
	log("load download file list:%s ok!",szList);
	rv = init_timer_socket();
	if(rv < 0)
		return -1;
	rv = init_conn_pool(szServerList);
	if(rv < 0)
	{
		log("init server pool:%s failed!",szServerList);
		return -1;
	}
	log("init server pool:%s ok!",szServerList);

	filenums = filelist.get_file_nums();

	m_phThreads = new HANDLE[m_nThreadCount];
	for(int i = 0; i< m_nThreadCount; ++i)
	{
		m_phThreads[i] = INVALID_HANDLE_VALUE;
	}
	m_pdwThreaIDs = new DWORD[m_nThreadCount];

	for(int i = 0;i < m_nThreadCount; i++)
	{
		m_phThreads[i] = (HANDLE)_beginthreadex(NULL,
			0,
			(unsigned int (__stdcall *)(void *))dlmanager::dlthread_entry,
			this,
			0,
			(unsigned int *)&m_pdwThreaIDs[i]);

		log("start loader work thread idx:%d,ID:%d.",i,m_pdwThreaIDs[i]);
	}
	DWORD dwThreadID = 0;
	m_hTickThread = (HANDLE)_beginthreadex(NULL,
		0,
		(unsigned int (__stdcall *)(void *))dlmanager::tick_thread_entry,
		this,
		0,
		(unsigned int *)&dwThreadID);

	log("start timer thread ID:%d.",dwThreadID);
	return 0;
}

int dlmanager::rate(DWORD down)
{
	EnterCriticalSection(&m_bwMutex);
    if(down > 0)
	{
		net_bw_limit_in = down;
		m_bw_bytes_in = down;
		log("limit down rate:%uB/s.",down);
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
	wchar_t w_path[MAX_PATH] = {0};
	size_t inbytes = strlen(item->path);
	size_t outbytes = sizeof(w_path);
	int rv = conv_utf8_to_ucs2(item->path,&inbytes,w_path,&outbytes);
	DWORD dwNum = WideCharToMultiByte(CP_ACP,NULL,w_path,-1,NULL,0,NULL,FALSE);
	char *c_path;
	c_path = new char[dwNum];
	rv = WideCharToMultiByte (CP_OEMCP,NULL,w_path,-1,c_path,dwNum,NULL,FALSE);

	//验证MD5
	char md5[64]={0};
	gen_md5(md5,sizeof(md5),data,item->size);
	if(strcmp(item->md5,md5))
	{
		log("check md5 for file:%s failed!",c_path);
		delete[]c_path;
		return -1;
	}

	if(item->method == STANDALONE)
	{
		HANDLE hFile;
		hFile = CreateFile(c_path,GENERIC_WRITE,0,NULL,
			CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if(hFile == INVALID_HANDLE_VALUE)
		{
			delete []c_path;
			log("create file:%s failed!",c_path);
			return -1;
		}
		DWORD written = 0;
		BOOL rv = WriteFile(hFile,data,item->size,&written,NULL);
		if(!rv || written != item->size)
		{
			delete []c_path;
			CloseHandle(hFile);
			DeleteFile(c_path);
			log("write file:%s failed!",c_path);
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

#define MAX_BYTES_PERRECV (100*1024)
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
		log("send http request:%s failed!",http_get_request);
		free(http_get_request);
		return -1;
	}

	if((ret = Get_Http_Header(s, &http_header)) != OK) {
		log("get http header for request:%s failed!",http_get_request);
		free(http_get_request);
		return -1;
	}

	/* get http data size from the header */
	if((ret = Get_Http_Content_Length(http_header, &content_length)) != OK) {
		free(http_header);
		log("get http content length for request:%s failed!",http_get_request);
		free(http_get_request);
		return -1;
	}
	free(http_header);

	if(content_length != item->size)
	{
		log("content length:%u <> filesize:%u! for request:%s!",http_get_request);
		free(http_get_request);
		return -1;
	}
	free(http_get_request);
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
			want_read = remain;
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
		else if(want_read > MAX_BYTES_PERRECV)
		{
			want_read = MAX_BYTES_PERRECV;
		}

		if((ret = ZNet_Os_Socket_Recv(s, http_content + received, want_read, 
			&recv, HTTP_RECEIVE_TIMEOUT)) != OK) {
				free(http_content);
				log("ZNet_Os_Socket_Recv failed,wsacode:%d!\n",WSAGetLastError());
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
	delete []c_path;
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
				free(pFileData);
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

	conn_info_t conn_info;
	int rv = serverpool::Instance()->acquire_conn(&conn_info);
	if(rv != 0)
	{
		log("loader acquire connection failed!");
		return -1;
	}

	if(strcmp(conn_info.webroot,""))
	{
		sprintf_s(url,sizeof(url),"http://%s:%d/%s/%s",
			conn_info.host,conn_info.port,conn_info.webroot,item->resource);
	}
	else
	{
			sprintf_s(url,sizeof(url),"http://%s:%d/%s",
				conn_info.host,conn_info.port,item->resource);
	}
	if((ret = Parse_Url(url, host, resource, &port)) != OK) {
		log("loader parse url:%s failed!",url);
		return -1;
	}

	ret = http_uri_encode(resource,enc_uri);

	if((ret = ZNet_Generate_Http_Get(host, enc_uri, port, &gm)) != OK) {
		return -1;
	}
	OsSocket s;
	s.sock= *(SOCKET *)conn_info.conn;
	if((ret = http_request_get(item,&s,gm, &data)) != 0) {
		serverpool::Instance()->remove_conn(&conn_info);
		log("loader http request file:%s failed!",url);
	}
	else
	{
		serverpool::Instance()->release_conn(&conn_info);
		ret = process_file(item,data);
		free(data);
		if(ret != 0)
		{
			log("process file url:%s failed!",url);
		}
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
				InterlockedIncrement((long *)(&pdlmanger->filennums_done));
				delete item;
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
	delete []m_pdwThreaIDs;
	delete []m_phThreads;
	closesocket(m_TimerSocket);
	WaitForSingleObject(m_hTickThread,INFINITE);
	CloseHandle(m_hTickThread);
	serverpool::Instance()->fini_server_pool();
	filelist.fini();
	return 0;
}

dlmanager::~dlmanager(void)
{
	DeleteCriticalSection(&m_bwMutex);
}
