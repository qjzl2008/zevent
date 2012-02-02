extern "C"{
#include "os.h"
#include "os_common.h"
#include "http.h"
#include "utility.h"
#include "error.h"
};
#include "dlmanager.h"
#include "thread_mutex.h"
#include <process.h>

dlmanager::dlmanager()
{
	m_phThreads = NULL;
	req_queue = NULL;
	m_nThreadCount = 0;
	shutdown = 0;
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
	rv = init_conn_pool();
	if(rv < 0)
		return -1;

	queue_create(&req_queue,1000);

	m_nThreadCount = cfg.nkeep;
	m_phThreads = new HANDLE[m_nThreadCount];
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
	return 0;
}

int dlmanager::conv_ucs2_to_utf8(const wchar_t *in,
							 size_t *inwords,
							 char *out,
							 size_t *outbytes)
{
	long newch,require;
	size_t need;
	char *invout;
	int ch;

	while(*inwords)
	{
		ch = (unsigned short)(*in++);
		if(ch < 0x80)
		{
			--*inwords;
			--*outbytes;
			*(out++) = (unsigned char ) ch;
		}
		else
		{
			if((ch & 0xFC00) == 0xDC00) {
				return -1;
			}
			if((ch & 0xFC00) == 0xD800) {
				if(*inwords < 2) {
					return -1;
				}

				if(((unsigned short)(*in) & 0xFC00) != 0xDC00) {
					return -1;
				}
				newch = (ch & 0x03FF) << 10 | ((unsigned short)(*in++) & 0x03FF);
				newch += 0x10000;
			}
			else {
				newch = ch;
			}

			require = newch >> 11;
			need = 1;
			while (require)
				require >>=5, ++need;
			if(need >= *outbytes)
				break;
			*inwords -= (need > 2) + 1;
			*outbytes -= need + 1;

			ch = 0200;
			out += need + 1;
			invout = out;
			while(need--) {
				ch |= ch >> 1;
				*(--invout) = (unsigned char)(0200 | (newch & 0077));
				newch >>= 6;
			}
			*(--invout) = (unsigned char)(ch | newch);
		}
	}
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
	if(item->method == 0)
	{
		HANDLE hFile;
		hFile = CreateFile(TEXT(item->fpath),GENERIC_WRITE,0,NULL,
			CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if(hFile == INVALID_HANDLE_VALUE)
		{
			return -1;
		}
		DWORD written = 0;
		BOOL rv = WriteFile(hFile,data,item->size,&written,NULL);
		if(!rv || written != item->size)
		{
			CloseHandle(hFile);
			return -1;
		}
		CloseHandle(hFile);
	}
	return 0;
}

int dlmanager::dlonefile(dlitem *item)
{
	int ret;
	char enc_uri[2048] = {0};
	char host[MAX_HOST_LEN];
	short int port;
	char resource[MAX_RESOURCE_LEN];
	HTTP_GetMessage * gm;
	char *data;

	if((ret = Parse_Url(item->url, host, resource, &port)) != OK) {
		return ret;
	}

	ret = http_uri_encode(resource,enc_uri);

	if((ret = ZNet_Generate_Http_Get(host, enc_uri, port, &gm)) != OK) {
		return ret;
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
		//filelist.set_dlitem_finish(item);
		delete item;
	}

	(void)ZNet_Destroy_Http_Get(&gm);
	return ret;
}

int dlmanager::get_from_dllist(dlitem *&item)
{
	int rv = filelist.get_next_dlitem(item);
	return rv;
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
			rv = pdlmanger->dlonefile(item);
		else
			return 0;
		Sleep(50);
	}
	return 0;
}

int dlmanager::fini(void)
{
	shutdown = 1;
	WaitForMultipleObjects(m_nThreadCount,m_phThreads,TRUE,INFINITE);
	if(req_queue)
	queue_destroy(req_queue);
	conn_pool_fini(&cfg);
	filelist.fini();
	return 0;
}

dlmanager::~dlmanager(void)
{
}
