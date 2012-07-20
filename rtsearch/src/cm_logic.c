#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cm_logic.h"
#include "json_helper.h"
#include "redis_util.h"
#include "response.h"
#include "segword.h"

/**
 * redis_c->set("data",'{"type":"user","id":10,"term":"周四海是一个伟大的领导人物，不一般啊","score":100,"data":"url://"}')
 **/
int cm_logic_set(struct request *req)
{
    if(req->argc < 2)
    {
        return -1;
    }
    char *data = req->argv[2];
    if(!data)
    {
        return -1;
    }
    if(strlen(data) > MAX_RES_LEN)
    {
        return -1;
    }
    json_object *jmsg = json_tokener_parse(data);
    if(!jobject_ptr_isvalid(jmsg))
    {
        return -1;
    }

    json_object *jtype = json_util_get(jmsg,"type");
    if(!jtype)
    {
        json_object_put(jmsg);
        return -1;
    }
    const char *type = json_object_get_string(jtype);
 
    json_object *jid = json_util_get(jmsg,"id");
    if(!jid)
    {
        json_object_put(jmsg);
        return -1;
    }
    int id = json_object_get_int(jid);

    json_object *jscore = json_util_get(jmsg,"score");
    if(!jscore)
    {
        json_object_put(jmsg);
        return -1;
    }
    int score = json_object_get_int(jscore);

    json_object *jterm = json_util_get(jmsg,"term");
    if(!jterm)
    {
        json_object_put(jmsg);
        return -1;
    }
    const char *term = json_object_get_string(jterm);
    printf("term:%s\n",term);

    int value;
    int rv = getint_redis_cmd(&value,"HSET data:%s %d %b",type,id,data,strlen(data));
    if(rv !=0 )
    {
        json_object_put(jmsg);
        return -1;
    }
    printf("value:%d\n",value);

    scws_send_text(s, term, strlen(term));
    scws_res_t res, cur;
    char word[MAX_WORD_LEN + 1]={0};
    while (res = cur = scws_get_result(s))
    {   
        while (cur != NULL)
        {   
            printf("len:%d,Word: %.*s/%s (IDF = %4.2f)\n", cur->len,cur->len, term+cur->off, 
                    cur->attr, cur->idf);

            if(cur->len > MAX_WORD_LEN)
            {
                cur = cur->next;
                continue;
            }

            memset(word,0,sizeof(word));
            memcpy(word,term+cur->off,cur->len);
            rv = getint_redis_cmd(&value,"ZADD index:%s:%s %d %d",type,word,score,id);
            if(rv != 0)
            {
                scws_free_result(res);
                json_object_put(jmsg);
                return -1;
            }

            rv = getint_redis_cmd(&value,"SADD index:%s %s",type,word);
            if(rv != 0)
            {
                scws_free_result(res);
                json_object_put(jmsg);
                return -1;
            }

            cur = cur->next;
        }   
        scws_free_result(res);
    }   
    json_object_put(jmsg);
    return 0;
}

/**
 * redis_c->delete('{"type":"user","id":10,"term":"周四海是一个伟大的领导人物，不一般啊"')
 **/
int cm_logic_del(struct request *req)
{
    if(req->argc < 1)
    {
        return -1;
    }
    char *data = req->argv[1];
    if(!data)
    {
        return -1;
    }
    json_object *jmsg = json_tokener_parse(data);
    if(!jobject_ptr_isvalid(jmsg))
    {
        return -1;
    }

    json_object *jtype = json_util_get(jmsg,"type");
    if(!jtype)
    {
        json_object_put(jmsg);
        return -1;
    }
    const char *type = json_object_get_string(jtype);
 
    json_object *jid = json_util_get(jmsg,"id");
    if(!jid)
    {
        json_object_put(jmsg);
        return -1;
    }
    int id = json_object_get_int(jid);

    json_object *jterm = json_util_get(jmsg,"term");
    if(!jterm)
    {
        json_object_put(jmsg);
        return -1;
    }
    const char *term = json_object_get_string(jterm);
    printf("term:%s\n",term);

    int value;
    int rv = getint_redis_cmd(&value,"HDEL data:%s %d",type,id);
    if(rv !=0 )
    {
        json_object_put(jmsg);
        return -1;
    }
    printf("value:%d\n",value);

    scws_send_text(s, term, strlen(term));
    scws_res_t res, cur;
    char word[MAX_WORD_LEN + 1]={0};
    while (res = cur = scws_get_result(s))
    {   
        while (cur != NULL)
        {   
            printf("len:%d,Word: %.*s/%s (IDF = %4.2f)\n", cur->len,cur->len, term+cur->off, 
                    cur->attr, cur->idf);

            if(cur->len > MAX_WORD_LEN)
            {
                cur = cur->next;
                continue;
            }

            memset(word,0,sizeof(word));
            memcpy(word,term+cur->off,cur->len);
            rv = getint_redis_cmd(&value,"ZREM index:%s:%s %d",type,word,id);
            if(rv != 0)
            {
                scws_free_result(res);
                json_object_put(jmsg);
                return -1;
            }

            rv = getint_redis_cmd(&value,"SREM index:%s %s",type,word);
            if(rv != 0)
            {
                scws_free_result(res);
                json_object_put(jmsg);
                return -1;
            }

            cur = cur->next;
        }   
        scws_free_result(res);
    }   
    json_object_put(jmsg);
    return 0;
}
/**
 * {"keywords":"hello world","types":[{"type":"user","sort":-1},{"type":"tags","sort":-1}]}
 **/
int cm_logic_get(struct request *req,char **qres)
{
    if(req->argc < 1)
    {
        return -1;
    }
    char *data = req->argv[1];
    if(!data)
        return -1;
    json_object *jmsg = NULL;
    jmsg = json_tokener_parse(data);
    if(!jobject_ptr_isvalid(jmsg))
    {
        return -1;
    }

    json_object *jkeywords = json_util_get(jmsg,"keywords");
    if(!jkeywords)
    {
        json_object_put(jmsg);
        return -1;
    }
    const char *keywords = json_object_get_string(jkeywords);

    json_object *jtypes = json_util_get(jmsg,"types");
    if(!jtypes)
    {
        json_object_put(jmsg);
        return -1;
    }

    scws_send_text(s, keywords, strlen(keywords));
    scws_res_t res, cur;
    one_word_t **words = (one_word_t **)calloc(MAX_WORDS_NUM,sizeof(one_word_t *));

    //combine cachekey
    char *cachekey = (char *)calloc(MAX_WORD_LEN * MAX_WORDS_NUM,sizeof(char));
    strcpy(cachekey,"cache:%s:");
    int off = strlen(cachekey);
    int i = 0,idx = 0;
    while(res = cur = scws_get_result(s))
    {   
        if(i >= MAX_WORDS_NUM)
        {
            break;
        }
        while (cur != NULL)
        {   
            printf("len:%d,Word: %.*s/%s (IDF = %4.2f)\n", cur->len,cur->len, keywords+cur->off, 
                    cur->attr, cur->idf);

            if(cur->len > MAX_WORD_LEN)
            {
                cur = cur->next;
                continue;
            }

            one_word_t *word = (one_word_t *)calloc(1,sizeof(one_word_t));
            word->data = (char *)calloc(cur->len + 1,sizeof(char));
            memcpy(word->data,keywords+cur->off,cur->len);
            words[i++] = word;

            sprintf(cachekey + off,"|%.*s",cur->len,keywords+cur->off);
            off += cur->len + 1;

            cur = cur->next;

            if(i == MAX_WORDS_NUM)
            {
                break;
            }
        }   
        scws_free_result(res);
    }   

    int num_words = i;

    int cmd_len = 2*MAX_WORD_LEN * MAX_WORDS_NUM + 256;
    char *cmd = (char *)calloc(cmd_len,sizeof(char));

    *qres = (char *)calloc(MAX_RES_LEN,sizeof(char));
    strcpy(*qres,"[");

    //check is exist
    int rv = 0,value = 0; 
    int ntypes = 0,ntotal_bytes = 0;

    for(idx=0; idx < json_object_array_length(jtypes); idx++)
    {
        json_object *obj = json_object_array_get_idx(jtypes, idx);
        if(obj)
        {
            json_object *jtype = json_util_get(obj,"type");
            if(!jtype)
            {
                rv = -1;
                break;
            }
            const char *type = json_object_get_string(jtype);

            json_object *jstart = json_util_get(obj,"start");
            if(!jstart)
            {
                rv = -1;
                break;
            }
            int start = json_object_get_int(jstart);

            json_object *jnum = json_util_get(obj,"num");
            if(!jnum)
            {
                rv = -1;
                break;
            }
            int pagenum = json_object_get_int(jnum);

            json_object *jsort = json_util_get(obj,"sort");
            if(!jsort)
            {
                rv = -1;
                break;
            }
            int sort = json_object_get_int(jsort);

            //check cache exist
            //memset(cmd,0,cmd_len);
            strcpy(cmd,"EXISTS ");
            off = strlen(cmd);
            sprintf(cmd + off,cachekey,type);
            rv = getint_redis_strcmd(&value,cmd);
            if(rv != 0)
            {
                rv = -1;
                break;
            }

            if(value == 0)
            {
                //cacheing
                //memset(cmd,0,cmd_len);
                strcpy(cmd,"ZINTERSTORE ");
                off = strlen(cmd);

                //memset(cmd + off,0,cmd_len - off);
                snprintf(cmd + off,cmd_len,cachekey,type);
                off = strlen(cmd);

                sprintf(cmd + off," %d",num_words);
                off = strlen(cmd);

                int count =0;
                for(count = 0; count < num_words; ++count)
                {
                    sprintf(cmd + off," index:%s:%s",type,words[count]->data);
                    off = strlen(cmd);
                }

                rv = getint_redis_cmd(&value,cmd);
                if(rv != 0)
                {
                    rv = -1;
                    break;
                }

                if(value == 1)
                {
                    //expire 10 minutes
                    //memset(cmd,0,cmd_len);
                    strcpy(cmd,"EXPIRE ");
                    off = strlen(cmd);
                    sprintf(cmd + off,cachekey,type);
                    off = strlen(cmd);
                    sprintf(cmd + off," %d",10*60);
                    printf("expire!!!\n");
                    rv = getint_redis_strcmd(&value,cmd);
                    if(rv != 0)
                    {
                        rv = -1;
                        break;
                    }
                }
            }

            //get item ids
            reply_data_t *ids = NULL;
            int num = 0;
            //memset(cmd,0,cmd_len);
            if(sort == 1)
                strcpy(cmd,"ZRANGE ");
            else
                strcpy(cmd,"ZREVRANGE ");

            off = strlen(cmd);
            sprintf(cmd + off,cachekey,type);
            off = strlen(cmd);
            sprintf(cmd + off," %d %d",start,start + pagenum - 1);
            rv = getstrings_redis_strcmd(&ids,&num,cmd);
            if(rv == 0)
            {
                if(num > 0)
                {
                    //memset(cmd,0,cmd_len);
                    sprintf(cmd,"HMGET data:%s",type);
                    off = strlen(cmd);
                    for(i = 0; i< num; ++i)
                    {
                        sprintf(cmd + off," %.*s", ids[i].len,ids[i].data);
                        off = strlen(cmd);
                    }
                    //free memory
                    for(i = 0; i < num; ++i)
                    {
                        if(ids[i].data)
                            free(ids[i].data);
                    }
                    free(ids);

                    int nitems = 0;
                    reply_data_t *items = NULL;
                    rv = getstrings_redis_strcmd(&items,&nitems,cmd);
                    if(rv == 0)
                    {
                        if(nitems > 0)
                        {
                            if(ntypes > 0)
                                sprintf(*qres + strlen(*qres),"%s",",");

                            sprintf(*qres+strlen(*qres),"{\"type\":\"%s\"",type);
                            sprintf(*qres+strlen(*qres),"%s",",\"items\":[");
                            for(i = 0; i < nitems; ++i)
                            {
                                if(ntotal_bytes > (MAX_RES_LEN - 1024))
                                    break;

                                if(i > 0)
                                    sprintf(*qres+strlen(*qres),"%s",",");
                                sprintf(*qres+strlen(*qres),"%.*s",items[i].len,items[i].data);
                                ntotal_bytes += items[i].len;
                            }
                            sprintf(*qres+strlen(*qres),"%s","]");
                            sprintf(*qres+strlen(*qres),"%s","}");
                            ++ntypes;

                            for(i = 0; i < nitems; ++i)
                            {
                                free(items[i].data);
                            }
                            free(items);
                        }

                    }
                    else
                    {
                        rv = -1;
                        break;
                    }
                }
            }

        }
    }
    sprintf(*qres+strlen(*qres),"%s","]");
    //free memory
    for(i = 0; i< num_words; ++i)
    {
        free(words[i]->data);
        free(words[i]);
    }
    free(words);
    free(cachekey);
    free(cmd);

    json_object_put(jmsg);
    if(rv < 0)
    {
        free(*qres);
    }
    return rv;
}
