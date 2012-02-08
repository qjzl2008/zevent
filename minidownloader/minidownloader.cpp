// minidownloader.cpp : 定义控制台应用程序的入口点。
//
#include <conio.h>
#include <windows.h>
#include "stdafx.h"
#include <string.h>
#include "stdlib.h"
#include "downloader.h"

#ifdef WIN32
#pragma comment(lib,"ws2_32.lib")
#endif

int _tmain(int argc, _TCHAR* argv[])
{
	int key;
	downloader downloader_;
	downloader_.rate(200000);
	downloader_.start();
	struct dlstat state;
	while(1)
	{
		downloader_.state(&state);
		printf("tot:%d,done:%d,rate_dwn:%d\n",state.files_total,
			state.files_got,state.rate_down);
		if(_kbhit())
		{
			if((key =_getch()) == 115/*s key*/)
				break;
		}
		Sleep(1000);
	}
	downloader_.stop();
	return 0;
}

