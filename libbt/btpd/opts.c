#include "btpd.h"

const char *btpd_dir;
uint32_t btpd_logmask =  BTPD_L_BTPD | BTPD_L_ERROR 
                  | BTPD_L_TR/*| BTPD_L_MSG*/ | BTPD_L_CONN | BTPD_L_POL/*|BTPD_L_ALL*/;
int net_max_uploads = -2;
unsigned net_max_peers;
int safe_max_fds = 800;
unsigned net_bw_limit_in;
unsigned net_bw_limit_out;
int net_port = 6881;
fpos_t cm_alloc_size = 0;//2048 * 1024;
int ipcprot = 0600;
int empty_start = 0;
const char *tr_ip_arg;
int net_ipv4 = 1;
int net_ipv6 = 0;
int webseed_timeout = 3;
const char *ipc_port_path = "ipc";
int choose_method = 0;
