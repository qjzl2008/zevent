#include <stdio.h>
#include "log.h"
int main(void)
{
    open_log("|/usr/bin/cronolog logs/%Y-%m-%d.%H.log");
    log_error(LOG_MARK,"testlog:%d",1);
    log_close();
    sleep(111111);
    return 0;
}
