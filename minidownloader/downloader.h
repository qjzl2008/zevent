#ifndef DOWNLOADER_IF_H
#define DOWNLOADER_IF_H

#if !defined(WIN32)
#define DL_DECLARE(type)            type
#define DL_DECLARE_NONSTD(type)     type
#define DL_DECLARE_DATA
#else
#define DL_DECLARE(type)            __declspec(dllexport) type __stdcall
#define DL_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define DL_DECLARE_DATA             __declspec(dllexport)
#endif

enum state {
	DL_STATE_LEECH = 0,   //正在下载
	DL_STATE_COMPLETE = 1    //完成
};

struct dlstat{
	//任务状态
	enum state st;
	/*
	* peers:连接数
	*/
	int peers;
	/*
	* files_got:已经下载的文件数
	* files_total: 总文件数
	* rate_down: 下载速率
	*/
	int files_got,files_total,rate_down;
};

class DL_DECLARE_DATA downloader
{
public:
	static int start(void);
	static int stop(void);
	static int state(struct dlstat *dl_state);
};

#endif