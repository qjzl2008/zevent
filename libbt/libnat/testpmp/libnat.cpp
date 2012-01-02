// libnat.cpp : 定义控制台应用程序的入口点。
//

#include "libnat.h"
#include <windows.h>
#include "IpHlpApi.h"

BOOL GetDefaultGateway(UINT32 &ip)
{
	PIP_ADAPTER_INFO pAdapterInfo = NULL, pCurAdapterInfo;
	ULONG ulOutBufLen = 0;
	DWORD dwRetVal = 0;
	BOOL bRet;

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
			ip = inet_addr(pCurAdapterInfo->GatewayList.IpAddress.String);
		} while(ip == 0 && (pCurAdapterInfo = pCurAdapterInfo->Next) != NULL);
		bRet = TRUE;
	}

	free(pAdapterInfo);
	pAdapterInfo = NULL;

	return bRet;
}
int main(void)
{
	UpnpController *c = NULL;
	char public_ip[64]={0};
	UINT32 ip=0;
	GetDefaultGateway(ip);
	int rv = LNat_Upnp_Discover(&c);
	rv = LNat_Upnp_Get_Public_Ip(c,public_ip,sizeof(public_ip));
	rv = LNat_Upnp_Set_Port_Mapping(c,NULL,8899,"TCP");
	rv = LNat_Upnp_Remove_Port_Mapping(c,8899,"TCP");
	return 0;
}

