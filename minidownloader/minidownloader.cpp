// minidownloader.cpp : 定义控制台应用程序的入口点。
//
#include <conio.h>
#include <windows.h>
#include "stdafx.h"
#include <string.h>
#include "stdlib.h"
#include "dlmanager.h"
#include "znet.h"
#include "ipcserver.h"

#ifdef WIN32
#pragma comment(lib,"ws2_32.lib")
#endif

int _tmain(int argc, _TCHAR* argv[])
{
	int key;
#ifdef WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2),&wsaData);
#endif
	ipc_server ns;
	int rv = ns.start();
	if(rv != 0)
		return -1;
	dlmanager dl_manager;
	dl_manager.init();
	while(1)
	{
		if(_kbhit())
		{
			if((key =_getch()) == 115/*s key*/)
				break;
		}
		Sleep(1000);
	}
	ns.stop();
	dl_manager.fini();
	return 0;
}

