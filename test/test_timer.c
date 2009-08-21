#include <stdio.h>
#include "zevent_alarm.h"

/**  @} */

void fun2(unsigned int clientreg, void *clientarg);
void fun1(unsigned int clientreg, void *clientarg)
{

	zevent_alarm_register(10, SA_REPEAT,fun2,NULL);
	fprintf(stderr,"fun:%s clientreg:%d\n",__FUNCTION__,clientreg);
}

void fun2(unsigned int clientreg, void *clientarg)
{

	fprintf(stderr,"fun:%s clientreg:%d\n",__FUNCTION__,clientreg);
}

void fun3(unsigned int clientreg, void *clientarg)
{
	fprintf(stderr,"fun:%s clientreg:%d\n",__FUNCTION__,clientreg);
}

int main()
{
	init_alarm_post_config();
	zevent_alarm_register(5, 0,fun1,NULL);
	zevent_alarm_register(60, SA_REPEAT,fun3,NULL);
	for(;;){
		pause();
	}
	return 0;
}

