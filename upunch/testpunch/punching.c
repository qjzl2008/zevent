#include "../p2p_punch.h"
#include <winsock2.h>
#include "../win32/getopt.h"

int sendudp(int fd, unsigned char *buf, int len, const char *ip,int port)
{
	int sent;
	struct sockaddr_in peer_addr;
	peer_addr.sin_family = AF_INET;
	peer_addr.sin_port = htons(port);
	peer_addr.sin_addr.s_addr = inet_addr(ip);

	sent = sendto(fd, buf, len, 0,(struct sockaddr *)&peer_addr,sizeof(struct sockaddr_in));
	if(sent == SOCKET_ERROR)
	{
		printf("sendto failed (%d)!\n",WSAGetLastError());
	}
	else
	{
		printf("sendto send:%d to %s\n",(signed int)sent,ip);
	}
	return 0;
}

static void readfrompeer(p2p_edge_t *eee)
{
	uint8_t		udp_buf[P2P_PKT_BUF_SIZE];
	ssize_t		recvlen;
	struct sockaddr_in sender_sock;
	p2p_sock_t	sender;
	p2p_sock_t	*orig_sender = NULL;
	time_t		now = 0;

	size_t		i;

	i = sizeof(sender_sock);
	recvlen = recvfrom(eee->udp_sock,udp_buf,P2P_PKT_BUF_SIZE,0,
		(struct sockaddr *)&sender_sock,(socklen_t*)&i);

	if(recvlen == SOCKET_ERROR)
	{
		printf("recvfrom failed(%d)!\n",WSAGetLastError());
		return;
	}

	sender.family = AF_INET;
	sender.port = ntohs(sender_sock.sin_port);
	memcpy(&(sender.addr.v4),&(sender_sock.sin_addr.s_addr),IPV4_SIZE);
	orig_sender = &sender;

	printf("### Rx P2P UDP (%d)\n",recvlen);
}

/* *********************************************** */
/** Help message to print if the command line arguments are not valid. */
static void exit_help(int argc, char * const argv[])
{
	fprintf( stderr, "%s usage\n", argv[0] );
	fprintf( stderr, "-l <local port> \tSet local port\n");
	fprintf( stderr, "-s <ip:port>\tSet server node ip and port\n" );
	fprintf( stderr, "-e <ip>\tSet peer node ip\n" );
	fprintf( stderr, "-p <port>\tSet peer node port\n" );
	fprintf( stderr, "-v        \tIncrease verbosity. Can be used multiple times.\n" );
	fprintf( stderr, "-h        \tThis help message.\n" );
	fprintf( stderr, "\n" );
	exit(1);
}

static const struct option long_options[] = {
	{ "local_port",      required_argument, NULL, 'l' },
	{ "sn_ip:port",      required_argument, NULL, 's' },
	{ "peer_ip",         required_argument, NULL, 'e' },
	{ "peer_port",       required_argument, NULL, 'p' },
	{ "help"   ,         no_argument,       NULL, 'h' },
	{ "verbose",         no_argument,       NULL, 'v' },
	{ NULL,              0,                 NULL,  0  }
};


int main(int argc,char * const argv[] )
{
	p2p_edge_t edge;
	punch_arg_t args;
	char *peer_ip = "127.0.0.1";
	int peer_port = 7789;
	char msg[256];

	int opt;

	memset(msg,0,sizeof(msg));

	strcpy(args.sn,"127.0.0.1:7654");
	strcpy(args.community_name,"zhou");
	args.local_port = 7788;
	args.traceLevel = 4;
	//////////////////////////////////////////////////////////////////////////


	while((opt = getopt_long(argc, argv, "l:s:h:p:", long_options, NULL)) != -1) 
	{
		switch (opt) 
		{
		case 'l': /*local port*/
			args.local_port = atoi(optarg);
			break;
		case 's': /* sn ip:port */
			strcpy(args.sn,optarg);
			break;
		case 'e': /* peer ip */
			peer_ip = optarg;
			break;
		case 'p': /* help */
			peer_port = atoi(optarg);
			break;
		case 'h':
			exit_help(argc, argv);
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	start_punching_daemon(&args,&edge);
	Sleep(10000);
	punching_hole(&edge,peer_ip,peer_port);
	Sleep(10000);
	stop_punching_daemon(&edge);
	return 0;
}