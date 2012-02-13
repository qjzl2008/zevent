#include "dlmanager.h"
#include "ipcserver.h"
#include "downloader.h"
#include "log.h"

static ipc_server *ns;
static dlmanager *dl_manager;
	
downloader::downloader(void)
{
	ns = new ipc_server;
	dl_manager = new dlmanager;
}

downloader::~downloader(void)
{
	delete ns;
	delete dl_manager;
}

int downloader::start(void)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2),&wsaData);

	int rv = init_log("minilog.txt");
	rv = ns->start();
	if(rv != 0)
	{
		log("start ipc server failed!");
		return -1;
	}
    rv = dl_manager->init();
	if(rv != 0)
	{
		log("start down load manager failed!");
		rv = ns->stop();
		return -1;
	}
	log("start loader ok!");
	return 0;
}

int downloader::rate(DWORD down)
{
	dl_manager->rate(down);
	return 0;
}

int downloader ::stop(void)
{
	ns->stop();
	dl_manager->fini();
	log("stop loader ok!");
	return 0;
}

int downloader::state(struct dlstat *dl_state)
{
	dl_state->files_got = dl_manager->filennums_done;
	dl_state->files_total = dl_manager->filenums;
	dl_state->rate_down = dl_manager->m_dwRateDwn;
	return 0;
}