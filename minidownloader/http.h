#ifndef _HTTP_H_
#define _HTTP_H_

#ifdef __cplusplus
extern "C"{
#endif
#include "os.h"
typedef struct HTTP_GetMessage HTTP_GetMessage;
typedef struct HTTP_PostMessage HTTP_PostMessage;

int Parse_Url(const char * url, char * host, 
			  char * resource, short int * port);

int ZNet_Generate_Http_Get(const char * host,
                           const char * resource,
                           short int port,
                           HTTP_GetMessage ** gm);


int ZNet_Destroy_Http_Get(HTTP_GetMessage ** gm);


int ZNet_Generate_Http_Post(const char * host,
                            const char * resource,
                            short int port,
                            const char * body,
                            HTTP_PostMessage ** pm);

int ZNet_Http_Post_Add_Request_Header(HTTP_PostMessage * pm,
                                      const char * token,
                                      const char * value);

int ZNet_Http_Post_Add_Entity_Header(HTTP_PostMessage * pm,
                                     const char * token,
                                     const char * value);

int ZNet_Destroy_Http_Post(HTTP_PostMessage ** pm);


int ZNet_Http_Request_Get(HTTP_GetMessage * gm,
                          char ** response);

int ZNet_Http_Request_Get_KL(OsSocket *s,HTTP_GetMessage * gm,
							 char ** response);


int ZNet_Http_Request_Post(HTTP_PostMessage * pm,
                           char ** response);
#ifdef __cplusplus
}
#endif
#endif
