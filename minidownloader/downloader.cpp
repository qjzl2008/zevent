#include "dlmanager.h"
#include "ipcserver.h"
#include "downloader.h"

static ipc_server *ns;
static dlmanager *dl_manager;

int downloader::start(void)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2),&wsaData);

	ns = new ipc_server;
	int rv = ns->start();
	if(rv != 0)
		return -1;
	dl_manager = new dlmanager;
    dl_manager->init();
	return 0;
}

int downloader ::stop(void)
{
	ns->stop();
	delete ns;
	dl_manager->fini();
	delete dl_manager;
	return 0;
}

int downloader::state(struct dlstat *dl_state)
{
	dl_state->files_got = dl_manager->filennums_done;
	dl_state->files_total = dl_manager->filenums;
	dl_state->rate_down = dl_manager->m_dwRateDwn;
	return 0;
}