#ifndef __MOD_RTINFO_H__
#define __MOD_RTINFO_H__
#define DEFAULT_SERVER_PORT 9977
static char *server_ip = NULL;
static int server_port = DEFAULT_SERVER_PORT;

struct con_t{
	int fd;
#if APR_HAS_THREADS
	apr_thread_mutex_t *mutex;
#endif
};
static struct con_t rtinfo_con;

typedef	struct	User	anUser;

struct User {
  char dbname[KEY_LEN];
  char key[KEY_LEN];
  char value[MAX_VALUE_LEN];
  int command;
  int err;
};
enum command {
	adddb = 0,
	set = 1,
	get = 2,
};
enum err_param {
  err_noerr=0,
  err_dbname=-1,
  err_key=-2,
  err_command=-3,
  err_value=-4
};
enum exec_ret{
	EXEC_CMD_FAILED=-1,
	EXEC_CMD_SUC=0,
	EXEC_PARAM_BAD=-2,
	EXEC_CON_FAILED=-3
};
#endif
