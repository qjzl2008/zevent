#include "../p2p_punch.h"
#include <winsock2.h>

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

int main(int argc,char *argv[])
{
	p2p_edge_t edge;
	punch_arg_t args;
	char *peer_ip = "127.0.0.1";
	int port = 61176;
	char msg[256];
	memset(msg,0,sizeof(msg));

	strcpy(args.sn,"127.0.0.1:7654");
	strcpy(args.community_name,"zhou");
	args.local_port = 7788;
	args.traceLevel = 4;
	start_punching_daemon(&args,&edge);
	punching_hole(&edge,"192.168.3.215",61097);
	Sleep(10000);
	stop_punching_daemon(&edge);
	return 0;
}