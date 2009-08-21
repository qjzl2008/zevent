#include <stdio.h>
#include "RT_Svr.h"
#include <string.h>
#include <time.h>

int main()
{
int fd;
//STORE store;
//strcpy(store.key,"test");
/*strcpy(store.data.describe,"this is a test");
store.data.count = 100;
store.data.type = 3098;
store.data.curTime = time(NULL);*/
fd = connect_server("172.24.149.61",10101);
/*if(set_c(fd,&store) != 0)
	printf("set_c fails!\n");
if(refer_c(fd,"test") != 0)
	printf("refer fails!\n");*/
//if(get_c(fd,"test",&store) != 0)
//	printf("get_c fails\n");
//LOGSTORE logstore;
//strcpy(logstore.key,"1");
//logstore.data.count = 123456;
//strcpy(logstore.data.uid,"b0001");
//fd = connect_server("192.168.24.173",10101);
/*if(set_log_c(fd,&logstore) != 0)
	printf("set_log_c fails!\n");*/
//if(get_log_c(fd,"test",&logstore) != 0)
//	printf("get_log_c fails\n");
//if(refer_log_c(fd,&logstore)!=0)
//	printf("refer_log_c fails!\n");


return 0;
}
