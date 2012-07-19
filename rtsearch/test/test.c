#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "hiredis.h"

int main(void)
{
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    redisContext *c = redisConnectWithTimeout("127.0.0.1",6888 , timeout);
    if (c->err) {
        redisFree(c);
        printf("connect redis failed!\n");
        return -1;
    }

    printf("connect redis ok!\n");

    int count = 0;
    while(1)
    {
        char cmd[256]={0};
        const char *query = "{\"keywords\":\"周四海是一个伟大的\",\"types\":[{\"type\":\"user\",\"start\":0,\"num\":10,\"sort\":-1},{\"type\":\"tags\",\"start\":0,\"num\":10,\"sort\":-1}]}";

        snprintf(cmd,sizeof(cmd),"GET %s",query);
        redisReply *reply = redisCommand(c,cmd);
        if(!reply)
        {
            printf("redis fatal error!\n");
            return;
        }

        /*
           if (!(reply->type == REDIS_REPLY_STATUS && strcasecmp(reply->str,"OK") == 0))
           {
           printf("redisCommand error!\n");
           }
           */
        freeReplyObject(reply);
        if((count > 1000) && ((count % 1000) == 0))
        {
            printf("count:%d,tm:%u\n",count,time(NULL));
        }
        ++count;
    }


    return 0;
}
