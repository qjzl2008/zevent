#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "redis_util.h"
#include "client_manager.h"
#include "json_helper.h"
#include "cm_logic.h"

extern log_t *log_;
int redis_init(void)
{
    json_object *jfile = json_object_from_file("config.json");
    if(!jobject_ptr_isvalid(jfile))
    {
        printf("open config.json failed!\n");
        return -1;
    }

    json_object *jredis_host = json_util_get(jfile,"CONFIG.redis.host");    
    if(!jredis_host)    return -1;    
    const char *redis_host = json_object_get_string(jredis_host);    

    json_object *jredis_port = json_util_get(jfile,"CONFIG.redis.port");    
    if(!jredis_port)    return -1;    
    int redis_port = json_object_get_int(jredis_port);    

    memset(&rcfg,0,sizeof(rcfg));
    strcpy(rcfg.host,redis_host);
    rcfg.port = redis_port;
    rcfg.nmin = 2;
    rcfg.nkeep = 2*rcfg.nmin;
    rcfg.nmax = 4*rcfg.nmin;

    redis_pool_init(&rcfg);
    return 0;
}

int redis_fini(void)
{
    redis_pool_fini(&rcfg);
    return 0;
}

int exec_redis_cmd(const char *format, ...)
{
    redisContext *redis_c = (redisContext *)redis_pool_acquire(&rcfg);

    int rv = 0;
    if(redis_c)
    {
        va_list ap;
        va_start(ap,format);
        redisReply *reply = (redisReply *)redisvCommand(redis_c,format,ap);
        va_end(ap);

        char *cmd;
        va_start(ap,format);
        redisvFormatCommand(&cmd,format,ap);
        va_end(ap);

        if(!reply)
        {
            redis_pool_remove(&rcfg,redis_c);

            rv = -1;
            log_error(log_,"[redis]Exec failed. cmd:\n[%s]",cmd);
            free(cmd);
            return rv;
        }

        if (!(reply->type == REDIS_REPLY_STATUS && strcasecmp(reply->str,"OK") == 0)) {
            log_error(log_,"[redis]Exec failed. cmd:\n[%s],info:%s.",cmd,reply->str);
            free(cmd);

            freeReplyObject(reply);
            rv = -1;

            redis_pool_release(&rcfg,redis_c);
            return rv;
        }

        log_info(log_,"[redis]Exec OK. cmd:\n[%s].",cmd);
        free(cmd);
        freeReplyObject(reply);
        rv = 0;

        redis_pool_release(&rcfg,redis_c);
        return rv;
    }
    else
    {
        va_list ap;
        va_start(ap,format);

        char *cmd;
        redisvFormatCommand(&cmd,format,ap);
        va_end(ap);

        rv = -1;

        log_error(log_,"[redis]Connect failed.cmd:\n[%s]",cmd);
        free(cmd);
        return rv;
    }
}

int getint_redis_cmd(int *rv,const char *format, ...)
{
        redisContext *redis_c = (redisContext *)redis_pool_acquire(&rcfg);
        if(redis_c)
        {
            va_list ap;
            va_start(ap,format);
            redisReply *reply = (redisReply *)redisvCommand(redis_c,format,ap);
            va_end(ap);

            char *cmd;

            va_start(ap,format);
            redisvFormatCommand(&cmd,format,ap);
            va_end(ap);

            if(!reply)
            {
                redis_pool_remove(&rcfg,redis_c);
				log_error(log_,"[redis]Exec failed. cmd:\n[%s]",cmd);
                free(cmd);
                return -1;
            }

            if (!(reply->type == REDIS_REPLY_INTEGER)) {

                log_error(log_,"[redis]Exec failed. cmd:\n[%s],info:%s.",cmd,reply->str);
                free(cmd);

                freeReplyObject(reply);
                redis_pool_release(&rcfg,redis_c);
 
                return -1;
            }

            log_info(log_,"[redis]Exec OK. cmd:\n[%s].",cmd);
            free(cmd);

            *rv = reply->integer;
            freeReplyObject(reply);

           redis_pool_release(&rcfg,redis_c);
        }
        else
        {
            va_list ap;
            va_start(ap,format);

            char *cmd;
            redisvFormatCommand(&cmd,format,ap);

            va_end(ap);

			log_error(log_,"[redis]Connect failed.cmd:\n[%s]",cmd);
            free(cmd);
            return -1;
        }
        return 0;
}


int getint_redis_strcmd(int *rv,const char *cmd)
{
    redisContext *redis_c = (redisContext *)redis_pool_acquire(&rcfg);
    if(redis_c)
    {
        redisReply *reply = (redisReply *)redisCommand(redis_c,cmd);
        if(!reply)
        {
            redis_pool_remove(&rcfg,redis_c);

            log_error(log_,"[redis]Exec failed. cmd:\n[%s]",cmd);
            return -1;
        }

        if (!(reply->type == REDIS_REPLY_INTEGER)) {

            log_error(log_,"[redis]Exec failed. cmd:\n[%s],info:%s.",cmd,reply->str);

            freeReplyObject(reply);

            redis_pool_release(&rcfg,redis_c);

            return -1;
        }

        log_info(log_,"[redis]Exec OK. cmd:\n[%s].",cmd);

        *rv = reply->integer;
        freeReplyObject(reply);

        redis_pool_release(&rcfg,redis_c);
        return 0;
    }
    else
    {
        log_error(log_,"[redis]Connect failed.cmd:\n[%s]",cmd);
        return -1;
    }
    return 0;
}

int getstr_redis_cmd(reply_data_t *reply_data,const char *format, ...)
{
        redisContext *redis_c = (redisContext *)redis_pool_acquire(&rcfg);
        if(redis_c)
        {
            va_list ap;
            va_start(ap,format);
            redisReply *reply = (redisReply *)redisvCommand(redis_c,format,ap);
            va_end(ap);

            char *cmd;
            va_start(ap,format);
            redisvFormatCommand(&cmd,format,ap);
            va_end(ap);

            if(!reply)
            {
                redis_pool_remove(&rcfg,redis_c);

				log_error(log_,"[redis]Exec failed. cmd:\n[%s]",cmd);
                free(cmd);
                return -1;
            }

            if (!(reply->type == REDIS_REPLY_STRING)) {

                log_error(log_,"[redis]Exec failed. cmd:\n[%s],info:%s.",cmd,reply->str);
                free(cmd);
                freeReplyObject(reply);

                redis_pool_release(&rcfg,redis_c);
                return -1;
            }

            log_info(log_,"[redis]Exec OK. cmd:\n[%s].",cmd);
            free(cmd);

            reply_data->data = (char *)malloc(reply->len);
            memcpy(reply_data->data,reply->str,reply->len);
            reply_data->len = reply->len;
            freeReplyObject(reply);

            redis_pool_release(&rcfg,redis_c);
            return 0;
        }
        else
        {
            va_list ap;
            va_start(ap,format);
            char *cmd;
            redisvFormatCommand(&cmd,format,ap);
            va_end(ap);
 
			log_error(log_,"[redis]Connect failed.cmd:\n[%s]",cmd);
            free(cmd);
            return -1;
        }

        return 0;
}

int getstrings_redis_cmd(reply_data_t **reply_data,int *num,const char *format, ...)
{
    redisContext *redis_c = (redisContext *)redis_pool_acquire(&rcfg);
    if(redis_c)
    {
        va_list ap;
        va_start(ap,format);
        redisReply *reply = (redisReply *)redisvCommand(redis_c,format,ap);
        va_end(ap);

        char *cmd;

        va_start(ap,format);
        redisvFormatCommand(&cmd,format,ap);
        va_end(ap);
        if(!reply)
        {
            redis_pool_remove(&rcfg,redis_c);

			log_error(log_,"[redis]Exec failed. cmd:\n[%s]",cmd);
            free(cmd);
            return -1;
        }

        if (!(reply->type == REDIS_REPLY_ARRAY)) {

            log_error(log_,"[redis]Exec failed. cmd:\n[%s],info:%s.",cmd,reply->str);
            free(cmd);
            freeReplyObject(reply);

            redis_pool_release(&rcfg,redis_c);
            return -1;
        }

        log_info(log_,"[redis]Exec OK. cmd:\n[%s].",cmd);
        free(cmd);
        //_return.retcode = 0;
        if(reply->elements == 0)
        {
            log_error(log_,"[redis]Exec OK. cmd:\n[%s],reply->elements:%d,info:%s.",cmd,reply->elements,reply->str);
            freeReplyObject(reply);

            *num = reply->elements;
            redis_pool_release(&rcfg,redis_c);
            return 0;
        }

        *reply_data = (reply_data_t *)malloc(sizeof(reply_data_t *) * (reply->elements));
        int i = 0;
        for (i = 0; i < reply->elements; ++i) {
            redisReply* childReply = reply->element[i];

            if (childReply->type == REDIS_REPLY_STRING)
            {
                (*reply_data)[i].data = (char *)malloc(childReply->len);
                memcpy((*reply_data)[i].data,childReply->str,childReply->len);
                (*reply_data)[i].len = childReply->len;
            }
            else
            {
                (*reply_data)[i].data = NULL;
                (*reply_data)[i].len = 0;
            }
        }
        *num = reply->elements;

        freeReplyObject(reply);

        redis_pool_release(&rcfg,redis_c);
    }
    else
    {
        va_list ap;
        va_start(ap,format);
        char *cmd;
        redisvFormatCommand(&cmd,format,ap);
        va_end(ap);
 
		log_error(log_,"[redis]Connect failed.cmd:\n[%s]",cmd);
        free(cmd);
        return -1;
    }
    return 0;
}

int getstrings_redis_strcmd(reply_data_t **reply_data,int *num,const char *cmd)
{
    redisContext *redis_c = (redisContext *)redis_pool_acquire(&rcfg);
    if(redis_c)
    {
        redisReply *reply = (redisReply *)redisCommand(redis_c,cmd);
        if(!reply)
        {
            redis_pool_remove(&rcfg,redis_c);

			log_error(log_,"[redis]Exec failed. cmd:\n[%s]",cmd);
            return -1;
        }

        if (!(reply->type == REDIS_REPLY_ARRAY)) {

            log_error(log_,"[redis]Exec failed. cmd:\n[%s],info:%s.",cmd,reply->str);
            freeReplyObject(reply);

            redis_pool_release(&rcfg,redis_c);
            return -1;
        }

        log_info(log_,"[redis]Exec OK. cmd:\n[%s].",cmd);
        //_return.retcode = 0;
        if(reply->elements == 0)
        {
            log_error(log_,"[redis]Exec OK. cmd:\n[%s],reply->elements:%d,info:%s.",cmd,reply->elements,reply->str);
            freeReplyObject(reply);

            *num = reply->elements;
            redis_pool_release(&rcfg,redis_c);
            return 0;
        }

        *reply_data = (reply_data_t *)malloc(sizeof(reply_data_t) * (reply->elements));
        memset(*reply_data,0,sizeof(reply_data_t) * (reply->elements));
        int i = 0;
        for (i = 0; i < reply->elements; ++i) {
            redisReply* childReply = reply->element[i];

            if (childReply->type == REDIS_REPLY_STRING)
            {
                (*reply_data)[i].data = (char *)malloc(childReply->len);
                memcpy((*reply_data)[i].data,childReply->str,childReply->len);
                (*reply_data)[i].len = childReply->len;
            }
            else
            {
                (*reply_data)[i].data = NULL;
                (*reply_data)[i].len = 0;
            }
        }
        *num = reply->elements;

        freeReplyObject(reply);

        redis_pool_release(&rcfg,redis_c);
    }
    else
    {
		log_error(log_,"[redis]Connect failed.cmd:\n[%s]",cmd);
        return -1;
    }
    return 0;
}
