#ifndef HTTP_UTILITY_H
#define HTTP_UTILITY_H

#define NULL_TERM_LEN                 1
#define NULL_TERM                     '\0'

#define MAX_PORT_SIZE                 16383
#define MAX_PORT_STRING_LEN           5

#define HTTP_OK                       "200 OK"
#define HTTP_PROTOCOL_STRING          "http://"
#define DEFAULT_HTTP_PORT             80

#define MAX_URL_LEN                   512
#define MAX_HOST_LEN                  512
#define MAX_RESOURCE_LEN              512


int ZNet_Str_To_Upper(const char * str, char * dest);

#endif