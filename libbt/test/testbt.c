#include <conio.h>
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
	int seeding = 1;
	struct btstat tstat[10];
	char *torrents[10];
	int key,rv;
	int ntorrents = 1,i = 0;
	torrents[0] = "test.torrent";
	torrents[1] = "wow3.torrent";

	bt_arg.ipc_port = 9988;
	bt_arg.pipe_port = 7777;
	bt_arg.net_port = 7881;
	bt_arg.empty_start = 1;

	if(bt_start_daemon(&bt_arg) != 0)
		return -1;
	//rv = bt_add("download",torrents[0],&bt_arg);
	rv = bt_add_url("downloads","test.torrent","http://192.168.1.106/test.torrent",&bt_arg);
	if(rv == 1)
	{
		bt_start(ntorrents,torrents,&bt_arg);
	}
	else if(rv < 0)
	{
		bt_stop_daemon(&bt_arg);
		return 0;
	}

	//rv = bt_add_p2sp("kongfu.torrent",
	//	"http://download.firefox.com.cn/releases/webins2.0/official/zh-CN/",&bt_arg);
	for(i = 0; i < 10; ++i)
	rv = bt_add_p2sp("test.torrent","http://down1.chinaunix.net/distfiles/",&bt_arg);

	//rv = bt_add_p2sp("test.torrent","http://127.0.0.1",&bt_arg);
	for(i = 0; i < ntorrents;++i)
	{
		bt_stat(torrents[i],&bt_arg,&tstat[i]);
	}

	while(1)
	{
		for(i = 0; i < ntorrents; ++i)
		{
			bt_stat(torrents[i],&bt_arg,&tstat[i]);
			printf("%-20s ",torrents[i]);
			print_stat(&tstat[i]);
		}
		if(_kbhit())
		{
			if((key =_getch()) == 115/*s key*/)
				break;
		}
		Sleep(1000);
	}

	bt_stop_daemon(&bt_arg);
	return 0;
}