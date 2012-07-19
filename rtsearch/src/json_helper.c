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

int jobject_ptr_isvalid(json_object* ptr)
{
    if (ptr == NULL)
    {
        return 0;
    }

    int errorValue = - (int)ptr;

    if ( (errorValue == json_tokener_continue) ||
            (errorValue == json_tokener_error_depth) ||
            (errorValue == json_tokener_error_parse_eof) ||
            (errorValue == json_tokener_error_parse_unexpected) ||
            (errorValue == json_tokener_error_parse_null) ||
            (errorValue == json_tokener_error_parse_boolean) ||
            (errorValue == json_tokener_error_parse_number) ||
            (errorValue == json_tokener_error_parse_array) ||
            (errorValue == json_tokener_error_parse_object_key_name) ||
            (errorValue == json_tokener_error_parse_object_key_sep) ||
            (errorValue == json_tokener_error_parse_object_value_sep) ||
            (errorValue == json_tokener_error_parse_string) ||
            (errorValue == json_tokener_error_parse_comment) )
    {
        return 0;
    }

    return 1;

}
