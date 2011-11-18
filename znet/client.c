#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include "znet.h"

int main(void)
{
    net_client_t *nc;
    nc_arg_t cinfo;
    cinfo.func = NULL;
    strcpy(cinfo.ip,"127.0.0.1");
    cinfo.port = 8899;
    nc_connect(&nc,&cinfo);

    return 0;
}
