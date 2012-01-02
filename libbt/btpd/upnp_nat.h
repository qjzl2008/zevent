#ifndef UPNP_NAT_H
#define UPNP_NAT_H

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct {
	char ip[32];
	short port;
	char protocol[16];
	char desc[64];
}upnp_param_t;

enum{
	PMP_SUC = 0,
	PMP_FAILED = 1
};
typedef struct {
	HANDLE th_upnp; //upnp线程句柄
	upnp_param_t param;
	int stop;
	int flag;//映射是否成功
}upnp_nat_t;

int upnp_nat_start(upnp_param_t *param,upnp_nat_t **upnp_nat);
int upnp_port_map(upnp_param_t *param);
int upnp_nat_stop(upnp_param_t *param,upnp_nat_t **upnp_nat);

#ifdef __cplusplus
}
#endif
#endif