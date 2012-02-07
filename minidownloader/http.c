#define _CRT_SECURE_NO_DEPRECATE 1

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "http.h"
#include "os.h"
#include "utility.h"
#include "error.h"

#define HTTP_HOST               "%s:%d"
#define HTTP_HOST_LEN_WO_VARS   1

#define HTTP_GET                "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n"
#define HTTP_GET_LEN_WO_VARS    26

#define HTTP_GET_KL                "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n"
#define HTTP_GET_LEN_WO_VARS_KL    50

#define HTTP_POST                "POST /%s HTTP/1.1\r\nHost: %s\r\n%s%s\r\n%s"
#define HTTP_POST_LEN_WO_VARS    27

#define HTTP_CONTENT_LEN_TAG     "CONTENT-LENGTH:"
#define HTTP_CONTENT_LEN_SEARCH  "%d"

#define HTTP_REQUEST_FIELD          "%s: %s\r\n"
#define HTTP_REQUEST_LEN_WO_VARS    4

#define HTTP_ENTITY_FIELD           "%s: %s\r\n"
#define HTTP_ENTITY_LEN_WO_VARS     4

#define HTTP_HEADER_TERM           "\r\n\r\n"

#define MAX_HTTP_HEADER_LEN        4096

#define MAX_HTTP_BODY_LEN          100*1024*1024/*65536*/

#define HTTP_CONNECT_TIMEOUT       2
#define HTTP_RECEIVE_TIMEOUT       10


static int ZNet_Http_Request(const char * resource, const char * host, short int port, const char * message, char ** response);
static int ZNet_Http_Request_KL(OsSocket *s,const char * message,char ** response);
static int Send_Http_Request(OsSocket * s, const char * message);
static int Get_Http_Response(OsSocket * s, char ** http_response);
static int Get_Http_Content(OsSocket * s, int content_length, char ** http_content);
static int Get_Http_Content_Length(char * http_header, int * content_length);
static int Get_Http_Header(OsSocket * s, char ** http_response);
static int Http_Header_Terminator_Reached(char * http_response, int size_recv_sofar);
static int Create_Get_Request(HTTP_GetMessage * gm, char ** http_get_request);
static int Create_Get_Request_KL(HTTP_GetMessage * gm, char ** http_get_request);
static int Create_Post_Request(HTTP_PostMessage * pm, char ** http_post_request);
static int Create_Request_Header_String(HTTP_PostMessage * pm, char ** request_header);
static int Create_Entity_Header_String(HTTP_PostMessage * pm, char ** entity_header);
static int Create_Http_Header_Host_String(const char * host, short int port, char ** request_host);


struct HTTP_GetMessage
{
  char * resource;
  char * host;
  short int port;
};

typedef struct RequestHeader
{
  char * token;
  char * value;
  struct RequestHeader * next;
} RequestHeader;

typedef struct EntityHeader
{
  char * token;
  char * value;
  struct EntityHeader * next;
} EntityHeader;

/* a post message */
struct HTTP_PostMessage
{
  char * resource;
  char * host;
  short int port;
  char * body;

  int num_request_headers;  
  int num_entity_headers;  

  RequestHeader * rh_head; 
  EntityHeader * eh_head; 
};

int Parse_Url(const char * url, char * host, 
					 char * resource, short int * port)
{
	char * loc_of_semicolon;
	char * loc_of_slash;
	char * end_of_host_loc;
	char * loc_of_resource;
	char * loc_of_port;
	char url_wo_http[MAX_HOST_LEN];

	if(sscanf(url, "http://%255s", url_wo_http) != 1) {
		if(sscanf(url, "%255s", url_wo_http) != 1) {
			return HTTP_INVALID_URL;
		}
	}

	loc_of_semicolon = strchr(url_wo_http, ':');
	loc_of_slash = strchr(url_wo_http, '/');

	if(NULL == loc_of_slash) {
		return HTTP_INVALID_URL;
	} else {
		loc_of_resource = loc_of_slash+1;
	}

	if(NULL == loc_of_semicolon || loc_of_semicolon > loc_of_slash) {
		loc_of_semicolon = NULL;
		if(NULL != port) {
			*port = DEFAULT_HTTP_PORT;
		}
		end_of_host_loc = loc_of_slash;
	} else {
		loc_of_port = loc_of_semicolon + 1;
		if((loc_of_slash - loc_of_port) > MAX_PORT_STRING_LEN) {
			return HTTP_INVALID_URL;
		}
		end_of_host_loc = loc_of_semicolon;
	}

	if(NULL != host) {
		strncpy(host, url_wo_http, end_of_host_loc - url_wo_http);
		host[end_of_host_loc - url_wo_http] = NULL_TERM;
	}
	if(NULL != loc_of_semicolon) {
		if(sscanf(loc_of_port, "%hd", port) != 1) {
			*port = DEFAULT_HTTP_PORT;
		}
		if(*port < 0 || *port > MAX_PORT_SIZE) {
			return HTTP_INVALID_URL;
		}
	}
	if(NULL != resource) {
		strcpy(resource, loc_of_resource);
	}

	return OK;
}

int ZNet_Generate_Http_Get(const char * host,
                           const char * resource,
                           short int port,
                           HTTP_GetMessage ** gm)
{
  if(NULL == host || NULL == resource || port < 0 || NULL == gm) {
    return BAD_PARAMS;
  }

  if(NULL == (*gm = (HTTP_GetMessage *)malloc(sizeof(HTTP_GetMessage)))) {
    return BAD_MALLOC;
  }

  if(NULL == ((*gm)->resource = (char *)malloc(strlen(resource)+1))) {
    free(*gm);
    return BAD_MALLOC;
  }
  (void)strcpy((*gm)->resource, resource);

  if(NULL == ((*gm)->host = (char *)malloc(strlen(host)+1))) {
    free((*gm)->resource);
    free(*gm);
    return BAD_MALLOC;
  }
  (void)strcpy((*gm)->host, host);

  (*gm)->port = port;
  return OK;
}



int ZNet_Destroy_Http_Get(HTTP_GetMessage ** gm)
{
  if(NULL == gm || NULL == *gm) {
    return BAD_PARAMS;
  }

  if(NULL != (*gm)->resource) {
    free((*gm)->resource);
  }

  if(NULL != (*gm)->host) {
    free((*gm)->host);
  }

  free(*gm);
  return OK;
}


int ZNet_Generate_Http_Post(const char * host,
                            const char * resource,
                            short int port,
                            const char * body,
                            HTTP_PostMessage ** pm)
{
  if(NULL == host || NULL == resource || port < 0 || NULL == body || NULL == pm) {
    return BAD_PARAMS;
  }

  if(NULL == (*pm = (HTTP_PostMessage *)malloc(sizeof(HTTP_PostMessage)))) {
    return BAD_MALLOC;
  }

  if(NULL == ((*pm)->resource = (char *)malloc(strlen(resource)+1))) {
    free(*pm);
    return BAD_MALLOC;
  }
  (void)strcpy((*pm)->resource, resource);

  if(NULL == ((*pm)->host = (char *)malloc(strlen(host)+1))) {
    free((*pm)->resource);
    free(*pm);
    return BAD_MALLOC;
  }
  (void)strcpy((*pm)->host, host);

  (*pm)->port = port;

  if(NULL == ((*pm)->body = (char *)malloc(strlen(body)+1))) {
    free((*pm)->resource);
    free((*pm)->host);
    free(*pm);
    return BAD_MALLOC;
  }
  (void)strcpy((*pm)->body, body);

  (*pm)->num_request_headers = 0;
  (*pm)->num_entity_headers = 0;
  (*pm)->rh_head = NULL;
  (*pm)->eh_head = NULL;
  return OK;
}



int ZNet_Http_Post_Add_Request_Header(HTTP_PostMessage * pm,
                                      const char * token,
                                      const char * value)
{
  RequestHeader * new_rheader = (RequestHeader *)malloc(sizeof(RequestHeader));
  if(NULL == new_rheader) {
    return BAD_MALLOC;
  }

  new_rheader->token = (char *)malloc(strlen(token)+1);
  if(NULL == new_rheader->token) {
    free(new_rheader);
    return BAD_MALLOC;
  }
  new_rheader->value = (char *)malloc(strlen(value)+1);
  if(NULL == new_rheader->value) {
    free(new_rheader->token);
    free(new_rheader);
    return BAD_MALLOC;
  }

  new_rheader->next = pm->rh_head;
  pm->rh_head = new_rheader;
  pm->num_request_headers++;
  return OK;
}



int ZNet_Http_Post_Add_Entity_Header(HTTP_PostMessage * pm,
                                     const char * token,
                                     const char * value)
{
  EntityHeader * new_eheader = (EntityHeader *)malloc(sizeof(EntityHeader));
  if(NULL == new_eheader) {
    return BAD_MALLOC;
  }

  new_eheader->token = (char *)malloc(strlen(token)+1);
  if(NULL == new_eheader->token) {
    free(new_eheader);
    return BAD_MALLOC;
  }
  strcpy(new_eheader->token, token);

  new_eheader->value = (char *)malloc(strlen(value)+1);
  if(NULL == new_eheader->value) {
    free(new_eheader->token);
    free(new_eheader);
    return BAD_MALLOC;
  }
  strcpy(new_eheader->value , value);

  new_eheader->next = pm->eh_head;
  pm->eh_head = new_eheader;
  pm->num_entity_headers++;
  return OK;
}


int ZNet_Destroy_Http_Post(HTTP_PostMessage ** pm)
{
  int i;
  RequestHeader * next_rheader;
  EntityHeader * next_eheader;

  if(NULL == pm || NULL == *pm) {
    return BAD_PARAMS;
  }

  if(NULL != (*pm)->resource) {
    free((*pm)->resource);
  }

  if(NULL != (*pm)->host) {
    free((*pm)->host);
  }

  if(NULL != (*pm)->body) {
    free((*pm)->body);
  }

  for(i=0; i<(*pm)->num_request_headers; i++) {
    next_rheader = (*pm)->rh_head->next;
    free((*pm)->rh_head->value);
    free((*pm)->rh_head->token);
    free((*pm)->rh_head);
    (*pm)->rh_head = next_rheader;
  }

  for(i=0; i<(*pm)->num_entity_headers; i++) {
    next_eheader = (*pm)->eh_head->next;
    free((*pm)->eh_head->value);
    free((*pm)->eh_head->token);
    free((*pm)->eh_head);
    (*pm)->eh_head = next_eheader;
  }

  free(*pm);
  return OK;
}


int ZNet_Http_Request_Get(HTTP_GetMessage * gm,
                          char ** response)
{
  int ret;
  char * http_get_request;

  if((ret = Create_Get_Request(gm, &http_get_request)) != OK) {
    return ret;
  }

  if((ret = ZNet_Http_Request(gm->resource, gm->host, gm->port,
                            http_get_request, response)) != OK) {
    free(http_get_request);
    return ret;
  }

  free(http_get_request);
  return OK;
}

static int ZNet_Http_Request_KL(OsSocket *s,
							 const char * message,
							 char ** response)
{
	int ret;

	/* send an HTTP request */
	if((ret = Send_Http_Request(s, message)) != OK) {
		return ret;
	}

	/* get an HTTP response */
	if((ret = Get_Http_Response(s, response)) != OK) {
		return ret;
	}

	return OK;
}
/*
用已经存在的socket发送请求 keep-alive
*/
int ZNet_Http_Request_Get_KL(OsSocket *s,HTTP_GetMessage * gm,
						  char ** response)
{
	int ret;
	char * http_get_request;

	if((ret = Create_Get_Request_KL(gm, &http_get_request)) != OK) {
		return ret;
	}

	if((ret = ZNet_Http_Request_KL(s,http_get_request, response)) != OK) {
			free(http_get_request);
			return ret;
	}

	free(http_get_request);
	return OK;
}


int ZNet_Http_Request_Post(HTTP_PostMessage * pm,
                           char ** response)
{
  int ret;
  char * http_post_request;

  if((ret = Create_Post_Request(pm, &http_post_request)) != OK) {
    return ret;
  }

  if((ret = ZNet_Http_Request(pm->resource, pm->host, pm->port,
                            http_post_request, response)) != OK) {
    free(http_post_request);
    return ret;
  }

  free(http_post_request);
  return OK;
}



static int ZNet_Http_Request(const char * resource,
                             const char * host,
                             short int port,
                             const char * message,
                             char ** response)
{
  OsSocket * s;
  int ret;

  /* make the connection to the host and port */
  if((ret = ZNet_Os_Socket_Connect(&s, host, port, 
                                    HTTP_CONNECT_TIMEOUT)) != OK ) {
    return ret;
  }

  /* send an HTTP request */
  if((ret = Send_Http_Request(s, message)) != OK) {
    ZNet_Os_Socket_Close(&s);
    return ret;
  }

  /* get an HTTP response */
  if((ret = Get_Http_Response(s, response)) != OK) {
    ZNet_Os_Socket_Close(&s);
    return ret;
  }

  /* close the connection s is connected to */
  if((ret = ZNet_Os_Socket_Close(&s)) != OK) {
    return ret;
  }

  return OK;
}

static int Send_Http_Request(OsSocket * s, 
                             const char * message)
{
  int amt_sent;
  int ret;

  /* now we will send the request */
  if((ret = ZNet_Os_Socket_Send(s, 
      (char *)message, (int)strlen(message), &amt_sent)) != OK) {
    return ret;
  }

  return OK;
}

static int Get_Http_Response(OsSocket * s, 
                             char ** http_response)
{
  int ret = 0;
  int content_length;
  char * http_header;

  if((ret = Get_Http_Header(s, &http_header)) != OK) {
    return ret;
  }

  /* get http data size from the header */
  if((ret = Get_Http_Content_Length(http_header, &content_length)) != OK) {
    free(http_header);
    return ret;
  }
  free(http_header);

  if((ret = Get_Http_Content(s, content_length, http_response)) != OK) {
    return ret;
  }

  return OK;
}


/* get the http content */
static int Get_Http_Content(OsSocket * s, 
                            int content_length, 
                            char ** http_content)
{
  int ret;
  int recv = 0;
  *http_content = (char *)malloc(content_length + NULL_TERM_LEN);
  if(NULL == *http_content) {
    return BAD_MALLOC;
  }

  if((ret = ZNet_Os_Socket_Recv(s, *http_content, content_length, 
                                &recv, HTTP_RECEIVE_TIMEOUT)) != OK) {
    free(*http_content);
    return ret;
  }

  /* null terminate it */
  (*http_content)[recv] = NULL_TERM;
  return OK;
}


static int Get_Http_Content_Length(char * http_header, 
                                   int * content_length)
{
  char * cl_loc;
  char * http_upper_header = (char *)malloc(strlen(http_header)+NULL_TERM_LEN);
  if(NULL == http_upper_header) {
    return -1;
  }
  (void)ZNet_Str_To_Upper(http_header, http_upper_header);

  cl_loc = strstr(http_upper_header, HTTP_CONTENT_LEN_TAG);
  if(NULL == cl_loc) {
    *content_length = MAX_HTTP_BODY_LEN;
    free(http_upper_header);
    return OK;
  }

  /* put the cl_loc in the correct position in the original http_header */
  cl_loc = http_header + (cl_loc - http_upper_header) + 
           strlen(HTTP_CONTENT_LEN_TAG);
  if(sscanf(cl_loc, HTTP_CONTENT_LEN_SEARCH, content_length) != 1) {
    *content_length = MAX_HTTP_BODY_LEN;
    free(http_upper_header);
    return OK;
  }

  if(*content_length > MAX_HTTP_BODY_LEN) {
    *content_length = MAX_HTTP_BODY_LEN;
  }

  free(http_upper_header);
  return OK;
}

static int Get_Http_Header(OsSocket * s, 
                           char ** http_response)
{
  int ret;
  int recv = 0;
  int recv_sofar = 0;
  *http_response = (char *)malloc(MAX_HTTP_HEADER_LEN);
  if(NULL == *http_response) {
    return BAD_MALLOC;
  }

  /* recieve of size 1 till we hit the \r\n\r\n terminator
     of an HTTP header */
  do {
    ret = ZNet_Os_Socket_Recv(s, &((*http_response)[recv_sofar]), 
                              1, &recv, HTTP_RECEIVE_TIMEOUT);
    recv_sofar += recv;
  } while(ret == OK && recv > 0 && recv_sofar < MAX_HTTP_HEADER_LEN &&
          !Http_Header_Terminator_Reached(*http_response, recv_sofar));

  if(ret != OK) {
    free(*http_response);
    return ret;
  }

  if(recv_sofar <= 0) {
    free(*http_response);
    return HTTP_RECV_FAILED;
  }

  if(recv_sofar >= MAX_HTTP_HEADER_LEN) {
    free(*http_response);
    return HTTP_RECV_OVER_MAXSIZE;
  }

  (*http_response)[recv_sofar] = NULL_TERM;
  if(NULL == strstr(*http_response, HTTP_OK)) {
    free(*http_response);
    return HTTP_HEADER_NOT_OK;
  }

  return OK;
}


static int Http_Header_Terminator_Reached(char * http_response, 
                                          int size_recv_sofar)
{
  if(size_recv_sofar < (int)strlen(HTTP_HEADER_TERM)) {
    return 0;
  }

  if(!strncmp(&http_response[size_recv_sofar-strlen(HTTP_HEADER_TERM)], 
              HTTP_HEADER_TERM, strlen(HTTP_HEADER_TERM))) {
    return 1;
  }

  return 0;
}

static int Create_Get_Request(HTTP_GetMessage * gm, 
                              char ** http_get_request)
{
  int ret;
  char * get_request_host;

  if(NULL == gm || NULL == http_get_request) {
    return BAD_PARAMS;
  }

  if((ret = Create_Http_Header_Host_String(gm->host, gm->port, 
                                    &get_request_host)) != OK) {
    return ret;
  }

  *http_get_request = (char *)malloc(HTTP_GET_LEN_WO_VARS +
                                     strlen(gm->resource) +
                                     strlen(get_request_host) +
                                     NULL_TERM_LEN);
  if(NULL == *http_get_request) {
    free(get_request_host);
    return BAD_MALLOC;
  }
  (void)sprintf(*http_get_request, HTTP_GET, gm->resource, get_request_host);

  free(get_request_host);
  return OK;
}

static int Create_Get_Request_KL(HTTP_GetMessage * gm, 
							  char ** http_get_request)
{
	int ret;
	char * get_request_host;

	if(NULL == gm || NULL == http_get_request) {
		return BAD_PARAMS;
	}

	if((ret = Create_Http_Header_Host_String(gm->host, gm->port, 
		&get_request_host)) != OK) {
			return ret;
	}

	*http_get_request = (char *)malloc(HTTP_GET_LEN_WO_VARS_KL +
		strlen(gm->resource) +
		strlen(get_request_host) +
		NULL_TERM_LEN);
	if(NULL == *http_get_request) {
		free(get_request_host);
		return BAD_MALLOC;
	}
	(void)sprintf(*http_get_request, HTTP_GET_KL, gm->resource, get_request_host);

	free(get_request_host);
	return OK;
}


static int Create_Post_Request(HTTP_PostMessage * pm,
                               char ** http_post_request)
{
  int ret;
  char * post_request_host;
  char * request_header;
  char * entity_header;

  if(NULL == pm || NULL == http_post_request) {
    return BAD_PARAMS;
  }

  if((ret = Create_Http_Header_Host_String(pm->host, pm->port, 
                                    &post_request_host)) != OK) {
    return ret;
  } 

  if((ret = Create_Request_Header_String(pm, &request_header)) != OK) {
    free(post_request_host);
    return ret;
  }

  if((ret = Create_Entity_Header_String(pm, &entity_header)) != OK) {
    free(post_request_host);
    free(request_header);
    return ret;
  }

  *http_post_request = (char *)malloc(HTTP_POST_LEN_WO_VARS +
                                      strlen(pm->resource) +
                                      strlen(post_request_host) +
                                      strlen(request_header) +
                                      strlen(entity_header) +
                                      strlen(pm->body) +
                                      NULL_TERM_LEN);
  if(NULL == *http_post_request) {
    free(post_request_host);
    free(request_header);
    free(entity_header);
    return BAD_MALLOC;
  }

  (void)sprintf(*http_post_request, HTTP_POST, pm->resource, post_request_host,
                request_header, entity_header, pm->body);

  free(post_request_host);
  free(request_header);
  free(entity_header);
  return OK;
}

static int Create_Request_Header_String(HTTP_PostMessage * pm,
                                        char ** request_header)
{
  int request_header_fields_size = 0;
  RequestHeader * rh;

  for(rh = pm->rh_head; rh != NULL; rh = rh->next) {
    request_header_fields_size += HTTP_REQUEST_LEN_WO_VARS + 
                                  (int)strlen(rh->token) +
                                  (int)strlen(rh->value) +
                                  NULL_TERM_LEN;
  }
  if(0 == request_header_fields_size) {
    *request_header = (char *)malloc(1);
    (*request_header)[0] = NULL_TERM;
    return OK;
  }

  *request_header = (char *)malloc(request_header_fields_size);
  if(NULL == *request_header) {
    return BAD_MALLOC;
  }
  (*request_header)[0] = NULL_TERM;

  for(rh = pm->rh_head; rh != NULL; rh = rh->next) {
    (void)sprintf(&((*request_header)[strlen(*request_header)]),
                  HTTP_REQUEST_FIELD, rh->token, rh->value);
  }
  return OK;
}


static int Create_Entity_Header_String(HTTP_PostMessage * pm,
                                       char ** entity_header)
{
  int entity_header_fields_size = 0;
  EntityHeader * eh;

  for(eh = pm->eh_head; eh != NULL; eh = eh->next) {
    entity_header_fields_size += HTTP_ENTITY_LEN_WO_VARS +
                                 (int)strlen(eh->token) +
                                 (int)strlen(eh->value) +
                                 NULL_TERM_LEN;
  }
  if(0 == entity_header_fields_size) {
    *entity_header = (char *)malloc(1);
    (*entity_header)[0] = NULL_TERM;
    return OK;
  }

  *entity_header = (char *)malloc(entity_header_fields_size);
  if(NULL == *entity_header) {
    return BAD_MALLOC;
  }
  (*entity_header)[0] = NULL_TERM;

  for(eh = pm->eh_head; eh != NULL; eh = eh->next) {
    (void)sprintf(&((*entity_header)[strlen(*entity_header)]),
                  HTTP_ENTITY_FIELD, eh->token, eh->value);
  }
  return OK;
}

static int Create_Http_Header_Host_String(const char * host,
                                          short int port,
                                          char ** request_host)
{
  if(port > MAX_PORT_SIZE) {
    return HTTP_PORT_OUT_OF_RANGE;
  }


  *request_host = (char *)malloc(HTTP_HOST_LEN_WO_VARS + 
                                 strlen(host) +
                                 MAX_PORT_STRING_LEN +
                                 NULL_TERM_LEN);
  if(NULL == *request_host) {
    return BAD_MALLOC;
  }
  (void)sprintf(*request_host, HTTP_HOST, host, port);

  return OK;
}
