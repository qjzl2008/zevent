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

int dlmanager::conv_utf8_to_ucs2(const char *in, size_t *inbytes,
					  wchar_t *out, 
					  size_t *outwords)
{
	long newch, mask;
	size_t expect, eating;
	int ch;

	while (*inbytes && *outwords) 
	{
		ch = (unsigned char)(*in++);
		if (!(ch & 0200)) {
			/* US-ASCII-7 plain text
			*/
			--*inbytes;
			--*outwords;
			*(out++) = ch;
		}
		else
		{
			if ((ch & 0300) != 0300) { 
				/* Multibyte Continuation is out of place
				*/
				return -1;
			}
			else
			{
				/* Multibyte Sequence Lead Character
				*
				* Compute the expected bytes while adjusting
				* or lead byte and leading zeros mask.
				*/
				mask = 0340;
				expect = 1;
				while ((ch & mask) == mask) {
					mask |= mask >> 1;
					if (++expect > 3) /* (truly 5 for ucs-4) */
						return -1;
				}
				newch = ch & ~mask;
				eating = expect + 1;
				if (*inbytes <= expect)
					return -1;
		
				if (expect == 1) {
					if (!(newch & 0036))
						return -1;
				}
				else {
					if (!newch && !((unsigned char)*in & 0077 & (mask << 1)))
						return -1;
					if (expect == 2) {
						if (newch == 0015 && ((unsigned char)*in & 0040))
							return -1;
					}
					else if (expect == 3) {
						if (newch > 4)
							return -1;
						if (newch == 4 && ((unsigned char)*in & 0060))
							return -1;
					}
				}
				if (*outwords < (size_t)(expect > 2) + 1) 
					break; /* buffer full */
				while (expect--)
				{
					if (((ch = (unsigned char)*(in++)) & 0300) != 0200)
						return -1;
					newch <<= 6;
					newch |= (ch & 0077);
				}
				*inbytes -= eating;
				if (newch < 0x10000) 
				{
					--*outwords;
					*(out++) = (wchar_t) newch;
				}
				else 
				{
					*outwords -= 2;
					newch -= 0x10000;
					*(out++) = (wchar_t) (0xD800 | (newch >> 10));
					*(out++) = (wchar_t) (0xDC00 | (newch & 0x03FF));                    
				}
			}
		}
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
			return -1;
		}
		CloseHandle(hFile);
	}
	delete []c_path;
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

	sprintf_s(url,sizeof(url),"http://%s:%d%s",
		cfg.host,cfg.port,item->resource);
	if((ret = Parse_Url(url, host, resource, &port)) != OK) {
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
