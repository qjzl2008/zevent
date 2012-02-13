#include <time.h>
#include <stdio.h>
#include "log.h"

simple_log g_log;

simple_log::simple_log()
{
	m_hFile = INVALID_HANDLE_VALUE;
	InitializeCriticalSectionAndSpinCount(&m_csLog,512);
}

simple_log::~simple_log()
{
	if(m_hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hFile);
	}
	DeleteCriticalSection(&m_csLog);
}

int simple_log::init_log(const char *fname)
{
	m_hFile = CreateFile(fname,GENERIC_WRITE,FILE_SHARE_READ|
		FILE_SHARE_WRITE,NULL,OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,NULL);
	if(INVALID_HANDLE_VALUE == m_hFile)
	{
		m_hFile = CreateFile(fname,GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if(INVALID_HANDLE_VALUE == m_hFile)
			return -1;
	}
	return 0;
}

void simple_log::log_common(const char *fmt, va_list ap)
{
	if(m_hFile == INVALID_HANDLE_VALUE)
		return;
	EnterCriticalSection(&m_csLog);
	char log[4096]={0};
	size_t off = 0;
	time_t tp = time(NULL);
	struct tm newtime;
	int err = localtime_s(&newtime,&tp);
	if(err)
	{
		LeaveCriticalSection(&m_csLog);
		return;
	}
	strftime(log,sizeof(log), "[%Y %b %d %H:%M:%S]",&newtime);
	off = strlen(log);
	vsprintf_s(log+off,sizeof(log) - off,fmt,ap);
	strcpy(log + strlen(log),"\r\n");
	SetFilePointer(m_hFile,0,NULL,FILE_END);
	DWORD writen = 0;
	WriteFile(m_hFile,log,(DWORD)strlen(log),&writen,NULL);
	LeaveCriticalSection(&m_csLog);
	return;
}

int init_log(const char *fname)
{
	return g_log.init_log(fname);
}

void log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	g_log.log_common(fmt, ap);
	va_end(ap);
}

