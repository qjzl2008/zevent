#include <stdio.h>
#include "segword.h"
#include "json_helper.h"

int segword_init()
{
    json_object *jfile = json_object_from_file("config.json");
    if(!jobject_ptr_isvalid(jfile))
    {
        return -1;
    }
 
    json_object *jencode = json_util_get(jfile,"CONFIG.scws.encode");
    if(!jencode)
        return -1;
    const char *encode = json_object_get_string(jencode);

    json_object *jdic = json_util_get(jfile,"CONFIG.scws.dic");
    if(!jdic)
        return -1;
    const char *dic = json_object_get_string(jdic);

    if (!(s = scws_new())) {
        printf("error, can't init the scws_t!\n");
        json_object_put(jfile);
        return -1;
    }
    scws_set_charset(s, encode);
    int rv = scws_set_dict(s, dic, SCWS_XDICT_MEM);
    if(rv < 0)
    {
        json_object_put(jfile);
        return -1;
    }
   // scws_set_rule(s, "/usr/local/scws/etc/rules.ini");
    scws_set_ignore(s,1);

    json_object_put(jfile);
    return 0;
}

int segword_fini(void)
{
    scws_free(s);
    return 0;
}


