#ifndef _REQUEST_H
#define _REQUEST_H

#define BUF_SIZE (1024*10)

enum{
	STATE_CONTINUE,
	STATE_FAIL
};

typedef enum{CMD_PING,CMD_GET,CMD_SET,CMD_DEL,CMD_UNKNOW}CMD;
struct request{
	char *querybuf;
	int argc;
	char **argv;
	size_t pos;
	CMD cmd;
};

struct request *request_new(char *querybuf);
int  request_parse(struct request *request);
void request_free(struct request *request);
void request_dump(struct request *request);
int req_state_len(struct request *req,char *sb);
#endif
