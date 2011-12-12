#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "json_helper.h"

json_object *json_util_get(json_object *obj, const char *path)
{
    char buf[512]={'\0'};
    strcpy(buf,path);
    const char *delim = ".";
    char *saveptr = NULL;
    char *tok = strtok_r(buf,delim,&saveptr);

    json_object *tmp = NULL;
    json_object *jobj = obj;
    while(tok != NULL )
    {
	tmp = jobj;
	jobj = json_object_object_get(tmp, tok);
	if(!jobj)
	    return NULL;
	tok = strtok_r(NULL,delim,&saveptr);
    }
    return jobj;
}
