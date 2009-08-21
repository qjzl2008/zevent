#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

#define BUFFER_SIZE (2*8*1024)
#define CMD_SUCCESSFUL	0x11111111
#define CMD_FAIL	0x00000000


#define	CMD_CTRL_PAUSE	0x00010001
#define	CMD_CTRL_STOP	0x00010002
#define	CMD_CTRL_START	0x00010004

#define CMD_DB_ADD      0x00020000
#define CMD_DATA_GET	0x00020001
#define CMD_DATA_SET	0x00020002
typedef struct tagTData
{
	unsigned int command;
	void *data;
}TDATA,*PTDATA;

#endif
