#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mysql_pool.h"
#include "log.h"

int main(void)
{
    svr_cfg dbcfg;
    strcpy(dbcfg.host,"127.0.0.1");
    strcpy(dbcfg.user,"root");
    strcpy(dbcfg.password,"123456");
    strcpy(dbcfg.db,"user");
    dbcfg.port = DB_DEFAULT_PORT;
    dbcfg.nmin = DB_DEFAULT_NMIN;
    dbcfg.nkeep = DB_DEFAULT_NKEEP;
    dbcfg.nmax = DB_DEFAULT_NMAX;
    dbcfg.exptime = DB_DEFAULT_EXPTIME;
    dbcfg.timeout = DB_DEFAULT_TIMEOUT;
    strcpy(dbcfg.charset,DB_DEFAULT_CHARSET);

    int rv = mysql_pool_init(&dbcfg);

    char *sql = "select uid,uname from game_user where uid = 1";
    MYSQL_RES *rs = NULL;                                                                 
    int ret = mysql_pool_query(&dbcfg,sql,strlen(sql),&rs); 
    if(ret < 0)                                                                           
    {                                                                                     
	return -1;
    }                                                                                     
    int rows = mysql_num_rows(rs); 
    if(rows > 0)                                                                          
    {                                                                                     
	MYSQL_ROW row = mysql_fetch_row(rs);
	printf("uname:%s\n",row[1]);
	mysql_free_result(rs);
	return 0;
    }
    else
    {
	mysql_free_result(rs);
	return 0;
    }

    mysql_pool_fini(&dbcfg);
    return 0;
}
