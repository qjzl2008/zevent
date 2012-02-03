#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define MSG_REQUEST    0
#define MSG_RESPONSE   1

#pragma pack(4)
struct nb_ipcmsg_t{
	nb_ipcmsg_t()
	{
		nb_type = -1;
		memset(path,0,sizeof(path));
		method = 0;
		memset(check_code,0,sizeof(check_code));
		file_size = 0;
	}

	int nb_type;
	char path[MAX_PATH];
	unsigned char method;
	char check_code[33];
	int file_size;
};
#pragma pack()
#endif