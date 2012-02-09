#ifndef LOG_H
#define LOG_H
#include <windows.h>

class simple_log
{
public:
	simple_log(void);
	~simple_log(void);
	int init_log(const char *fname);
	void log_common(const char *fmt, va_list ap);
private:
	HANDLE m_hFile;
	CRITICAL_SECTION m_csLog;
};
int init_log(const char *fname);
void log(const char *fmt, ...);
#endif