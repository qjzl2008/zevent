#include <conio.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include "../btpd/bt.h"

void 
print_percent(long long part, long long whole)
{
	printf("%5.1f%% ", floor(1000.0 * part / whole) / 10);
}

void
print_rate(long long rate)
{
	if(rate >= 999.995 * (1<< 10))
		printf("%6.2fMB/s ", (double)rate / (1 << 20));
	else
		printf("%6.2fKB/s ", (double)rate / (1 << 10));
}

void 
print_size(long long size)
{
	if (size >= 999.995 * (1<<20))
		printf("%6.2fG ", (double)size / (1 <<30));
	else
		printf("%6.2fM ", (double)size / (1 << 20));
}

void 
print_ratio(long long part, long long whole)
{

	printf("%7.2f ", (double)part / whole);
}

static void
print_stat(struct btstat *st)
{
	print_percent(st->content_got,st->content_size);
	print_size(st->downloaded);
	print_rate(st->rate_down);
	print_size(st->uploaded);
	print_rate(st->rate_up);
	print_ratio(st->tot_up,st->content_size);
	printf("%4u ",st->peers);
	printf("\n");
}

int main(void)
{
	bt_arg_t bt_arg;
	bt_t *bt = NULL;

	int seeding = 1;
	struct btstat tstat[10];
	char *torrents[10];
	int key,rv;
	int ntorrents = 1,i = 0;
	int tids[32] = {0};
	char id[32] = {0};

	torrents[0] = "./torrents/test.torrent";

	bt_arg.net_port = 0;
	bt_arg.empty_start = 1;
	bt_arg.use_upnp = 1;


	if(bt_start_daemon(&bt_arg,&bt) != 0)
		return -1;
	//rv = bt_add("download",torrents[0],&bt);
	//bt_del(ntorrents,torrents,bt);
	rv = bt_add_url("downloads",torrents[0],"http://127.0.0.1/test.torrent",bt);
	if(rv == 1)
	{
		bt_start(ntorrents,torrents,bt);
	}
	else if(rv < 0)
	{
		bt_stop_daemon(bt);
		return 0;
	}

	for(i = 0; i < 10; ++i)
	rv = bt_add_p2sp(torrents[0],"http://down1.chinaunix.net/distfiles/",bt);

	//rv = bt_add_p2sp("test.torrent","http://127.0.0.1",bt);
	//bt_rate(30*1024,30*1024,bt);

	for(i = 0; i < ntorrents;++i)
	{
		bt_stat(torrents[i],bt,&tstat[i]);
		tids[i] = tstat[i].num;
	}

	while(1)
	{
		for(i = 0; i < ntorrents; ++i)
		{
			_snprintf(id,sizeof(id),"%d",tids[i]);
			bt_stat(id,bt,&tstat[i]);
			printf("%-20s ",strrchr(torrents[i],'/') + 1);
			print_stat(&tstat[i]);
		}
		if(_kbhit())
		{
			if((key =_getch()) == 115/*s key*/)
				break;
		}
		Sleep(1000);
	}

	bt_stop_daemon(bt);
	return 0;
}