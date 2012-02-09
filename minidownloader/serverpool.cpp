#include <time.h>
#include <assert.h>
#include "serverpool.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

serverpool *serverpool::pInstance = NULL;
serverpool::serverpool()
{
	connpool = NULL;
	m_nServerNum = 0;
	pInstance = this;
}

serverpool::~serverpool()
{

}

int serverpool::init_server_pool(const char *cfg)
{
	int i = 0;
	char* name=NULL;
	char* value=NULL;
	xmlKeepBlanksDefault (0); 
	xmlDocPtr doc=xmlParseFile(cfg);//创建Dom树
	if(doc==NULL)
	{
		xmlCleanupParser(); 
		return -1;
	}
	xmlNodePtr  cur,svrlist;
	cur=xmlDocGetRootElement(doc);//获取根节点
	if(cur==NULL)
	{
		xmlFreeDoc(doc); 
		return -1;
	}
	svrlist =cur->xmlChildrenNode;
	cur = svrlist;
	m_nServerNum = 0;
	while(cur)
	{
		++m_nServerNum;
		cur= cur->next;
	}
	connpool = (conn_svr_cfg *)malloc(sizeof(conn_svr_cfg) * m_nServerNum);
	webroots = (webroot_t *)malloc(sizeof(webroot_t) * m_nServerNum);

	for(i = 0; i < m_nServerNum ; ++i)
	{
		connpool[i].exptime = 0;
		connpool[i].nmin = 0;
		connpool[i].nkeep = 2;
		connpool[i].nmax = 10;
		connpool[i].timeout = 1000;
		strcpy_s(webroots[i].webroot,sizeof(webroots[i].webroot),"");
	}

	cur = svrlist;
	i = 0;
	int rv = 0;
	while(cur)
	{
		value = (char *)xmlGetProp(cur, (const xmlChar *)"host");
		if(value)
		{
			strcpy_s(connpool[i].host,sizeof(connpool[i].host),value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"port");
		if(value)
		{
			connpool[i].port = atoi(value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"min_conn");
		if(value)
		{
			connpool[i].nmin= atoi(value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"keep_con");
		if(value)
		{
			connpool[i].nkeep = atoi(value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"max_conn");
		if(value)
		{
			connpool[i].nmax = atoi(value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"exptime");
		if(value)
		{
			connpool[i].exptime = atol(value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"timeout");
		if(value)
		{
			connpool[i].timeout = atoi(value);
			xmlFree(value);
		}

		value = (char *)xmlGetProp(cur, (const xmlChar *)"webroot");
		if(value)
		{
			strcpy_s(webroots[i].webroot,sizeof(webroots[i].webroot),value);
			xmlFree(value);
		}

		rv = conn_pool_init(&connpool[i]);
		cur = cur->next;
		++i;
	}
	xmlFreeDoc(doc); 
	srand((unsigned)time(0));
	return 0;
}

int serverpool::fini_server_pool()
{
	xmlCleanupParser();
	for(int i = 0; i < m_nServerNum; ++i)
	{
		conn_pool_fini(&connpool[i]);
	}
	free(connpool);
	free(webroots);
	m_nServerNum = 0;
	pInstance = NULL;
	return 0;
}

int serverpool::acquire_conn(conn_info_t *conn)
{
	if(m_nServerNum <= 0)
		return -1;
	int idx = rand() % m_nServerNum;
	void *res = conn_pool_acquire(&connpool[idx]);
	if(!res)
	{
		//随机不中只有顺序取下
		for(idx = 0; idx < m_nServerNum; ++idx)
		{
			res = conn_pool_acquire(&connpool[idx]);
		}
	}
	if(!res)
	{
		return -1;
	}
	else
	{
		conn->idx = idx;
		conn->conn = res;
		strcpy_s(conn->host,sizeof(conn->host),connpool[idx].host);
		conn->port = connpool[idx].port;
		strcpy_s(conn->webroot,sizeof(conn->webroot),webroots[idx].webroot);
	}
	return 0;
}

int serverpool::release_conn(conn_info_t *conn)
{
	assert(conn->idx < m_nServerNum);
	conn_pool_release(&connpool[conn->idx],conn->conn);
	return 0;
}

int serverpool::remove_conn(conn_info_t *conn)
{	
	assert(conn->idx < m_nServerNum);
	conn_pool_remove(&connpool[conn->idx],conn);
	return 0;
}
