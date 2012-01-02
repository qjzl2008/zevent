#include <windows.h>
#include <iphlpapi.h>
#include "upnp_nat.h"
#include "libnat.h"
#pragma comment(lib, "iphlpapi.lib")

static int port_map(upnp_param_t *param,UINT32 uip)
{
	struct in_addr addr;
	const char *ip = NULL;
	int rv = 0;
	char public_ip[64]={0};
	char *local_ip = NULL;
	UpnpController *c = NULL;

	addr.S_un.S_addr = uip;
	ip = inet_ntoa(addr);

	rv = LNat_Upnp_Discover(&c,ip);
	if(rv != 0)
	{
		return rv;
	}
	rv = LNat_Upnp_Get_Public_Ip(c,public_ip,sizeof(public_ip));
	if(rv != 0 || (inet_addr(public_ip) == 0))
	{
		LNat_Upnp_Controller_Free(&c);

		return rv;
	}
	
	local_ip = param->ip;
	if(inet_addr(param->ip) == 0)
	{
		local_ip = NULL;
	}
	rv = LNat_Upnp_Set_Port_Mapping(c,local_ip,param->port,
		param->protocol,param->desc);
	if(rv != 0)
	{
		LNat_Upnp_Controller_Free(&c);
		return rv;
	}
	LNat_Upnp_Controller_Free(&c);
	return 0;
}

static int port_unmap(upnp_param_t *param,UINT32 uip)
{
	struct in_addr addr;
	const char *ip = NULL;
	int rv = 0;
	char public_ip[64]={0};
	UpnpController *c = NULL;

	addr.S_un.S_addr = uip;
	ip = inet_ntoa(addr);

	rv = LNat_Upnp_Discover(&c,ip);
	if(rv != 0)
	{
		return rv;
	}
	rv = LNat_Upnp_Get_Public_Ip(c,public_ip,sizeof(public_ip));
	if(rv != 0 || (inet_addr(public_ip) == 0))
	{
		return rv;
	}
	rv = LNat_Upnp_Remove_Port_Mapping(c,param->port,param->protocol);
	if(rv != 0)
	{
		LNat_Upnp_Controller_Free(&c);
		return rv;
	}
	LNat_Upnp_Controller_Free(&c);
	return 0;
}

static BOOL upnp_port_map(upnp_param_t *param)
{
	PIP_ADAPTER_INFO pAdapterInfo = NULL, pCurAdapterInfo;
	ULONG ulOutBufLen = 0;
	DWORD dwRetVal = 0;
	UINT32 uip;
	BOOL bRet;
	int rv;
	char public_ip[64]={0};
	UpnpController *c = NULL;

	bRet = FALSE;

	pAdapterInfo = (IP_ADAPTER_INFO *) malloc( sizeof(IP_ADAPTER_INFO) );
	ulOutBufLen = sizeof(IP_ADAPTER_INFO);

	dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);

	if (ERROR_BUFFER_OVERFLOW == dwRetVal)
	{
		free(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *) malloc (ulOutBufLen);
		dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	}

	if (ERROR_SUCCESS == dwRetVal)
	{
		pCurAdapterInfo = pAdapterInfo;
		do {
			if(pCurAdapterInfo->Type != MIB_IF_TYPE_ETHERNET && 
				pCurAdapterInfo->Type != MIB_IF_TYPE_PPP)
				continue;

			uip = inet_addr(pCurAdapterInfo->IpAddressList.IpAddress.String);
			if(uip == 0)
				continue;
			rv = port_map(param,uip);
			if(rv == 0)
			{
				bRet = TRUE;
			}
		} while((pCurAdapterInfo = pCurAdapterInfo->Next) != NULL);
	}

	free(pAdapterInfo);
	pAdapterInfo = NULL;

	return bRet;
}

static BOOL upnp_port_unmap(upnp_param_t *param)
{
	PIP_ADAPTER_INFO pAdapterInfo = NULL, pCurAdapterInfo;
	ULONG ulOutBufLen = 0;
	DWORD dwRetVal = 0;
	UINT32 uip;
	BOOL bRet;
	int rv;
	char public_ip[64]={0};
	UpnpController *c = NULL;

	bRet = FALSE;

	pAdapterInfo = (IP_ADAPTER_INFO *) malloc( sizeof(IP_ADAPTER_INFO) );
	ulOutBufLen = sizeof(IP_ADAPTER_INFO);

	dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);

	if (ERROR_BUFFER_OVERFLOW == dwRetVal)
	{
		free(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *) malloc (ulOutBufLen);
		dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	}

	if (ERROR_SUCCESS == dwRetVal)
	{
		pCurAdapterInfo = pAdapterInfo;
		do {
			if(pCurAdapterInfo->Type != MIB_IF_TYPE_ETHERNET && 
				pCurAdapterInfo->Type != MIB_IF_TYPE_PPP)
				continue;

			uip = inet_addr(pCurAdapterInfo->IpAddressList.IpAddress.String);
			if(uip == 0)
				continue;
			rv = port_unmap(param,uip);
			if(rv == 0)
			{
				bRet = TRUE;
			}
		} while((pCurAdapterInfo = pCurAdapterInfo->Next) != NULL);
	}

	free(pAdapterInfo);
	pAdapterInfo = NULL;

	return bRet;
}

#define UPNP_TIME_SLICE (1000)
#define DELAY_FOR_SUCESS (5*60)
#define DELAY_FOR_FAILED (60)
static DWORD WINAPI upnp_td(void *arg)
{
	upnp_nat_t *upnp_nat = (upnp_nat_t *)arg;
	upnp_param_t *param = &upnp_nat->param;
	BOOL rv = FALSE;
	UINT32 delay = UINT_MAX;
	while(!upnp_nat->stop){
		if(!rv && delay > DELAY_FOR_FAILED)
		{
			rv = upnp_port_map(param);
			delay = 0;
		}
		if(rv && delay > DELAY_FOR_SUCESS)
		{
			rv = upnp_port_map(param);
			delay = 0;
		}
		if(rv)
			upnp_nat->flag = PMP_SUC;
		else
			upnp_nat->flag = PMP_FAILED;
		Sleep(UPNP_TIME_SLICE);
		delay += 1;
	}
	return 0;
}

int upnp_nat_start(upnp_param_t *param,upnp_nat_t **upnp_nat)
{
	upnp_nat_t *upnp = NULL;
	upnp = (upnp_nat_t *)malloc(sizeof(upnp_nat_t));
	memset(upnp,0,sizeof(upnp_nat_t));
	upnp->param = *param;
	upnp->th_upnp = CreateThread(NULL,0,upnp_td,upnp,0,NULL);
	*upnp_nat = upnp;
	return 0;
}


int upnp_nat_stop(upnp_param_t *param,upnp_nat_t **upnp_nat)
{
	(*upnp_nat)->stop = 1;
	WaitForSingleObject((*upnp_nat)->th_upnp,5000);
	if((*upnp_nat)->flag == PMP_SUC)
		upnp_port_unmap(param);
	return 0;
}