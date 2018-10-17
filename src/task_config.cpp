#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <errno.h>
#include <stdint.h>

#include "cjson/cJSON.h"

#include "task_config.h"

/*

json格式如下：
{
	“maxTasks”：1000，
	“queryInterval”：2,
	“tasks”：[
		{“name”:”taskA”, “path”:” /usr/sbin/xxx”, “param”:” -a xx –b xx”, “heartbeat”:10},
		{“name”:”taskB”, “path”:” /usr/sbin/xxx”, “param”:” -a xx –b xx”, “heartbeat”:10},
	]
}
*/


static taskConfig_t s_global_config;

static void set_global_config(uint32_t maxTasks, double intvl, struct list_head *taskList)
{
    taskParm_t *pos = NULL, *next = NULL;

    list_for_each_entry_safe(pos, next, &(s_global_config.list), node){
        free(pos);
    }

    s_global_config.maxTasks = maxTasks;
    s_global_config.queryIntvl = intvl;

    list_for_each_entry_safe(pos, next, &taskList, node){
        list_del(&pos->node);
        list_add(&pos->node, &s_global_config.list);
    }    
}

static int parse_config_file(const char *buf)
{
    cJSON *root = NULL;
    cJSON *item = NULL;
    cJSON *list = NULL;
    cJSON *it = NULL;
    int array_size = 0;
    uint32_t maxTasks =  0;
    double queryIntvl = 0;
    struct list_head tmpList;
    taskParm_t *pos = NULL, *next = NULL, *taskNew = NULL;

    INIT_LIST_HEAD(&tmpList);

    root = cJSON_Parse(buf);
    if(!root){
        loge("parse config file failed!");
        return -1;
    }

    item = cJSON_GetObjectItem(root, "maxTasks");
    if(!item || item->type != cJSON_Number){
        loge("parse config file failed!");
        goto ERROR;
    }
    maxTasks = item->valueint;

    item = cJSON_GetObjectItem(root, "queryInterval");
    if(!item || item->type != cJSON_Number){
        loge("parse config file failed!");
        goto ERROR;
    }
    queryIntvl = item->valuedouble;

    list = cJSON_GetObjectItem(root, "tasks");
    if(!list ){
        loge("parse config file failed!");
        goto ERROR;
    }

    array_size = cJSON_GetArraySize(list);
    if(array_size == 0){
        cJSON_Delete(root);
        set_global_config(maxTasks, queryIntvl, &tmpList);
        return 0;
    }

    for(int i = 0; i < array_size; i++){
        taskNew = malloc(sizeof(taskParm_t));
        if(!taskNew){
            loge("parse config file failed! malloc");
            goto ERROR;
        }
        memset(taskNew, 0, sizeof(taskParm_t));
        list_add(&taskNew->node, &tmpList);
    
        item = cJSON_GetArrayItem(list, i);
        if(!item){
            loge("parse config file failed! array");
            goto ERROR;
        }
        it = cJSON_GetObjectItem(item, "name");
        if(!it || it->type != cJSON_String){
            loge("parse config file failed! array");
            goto ERROR;
        }
        strncmp(taskNew->name, it->string, sizeof(taskNew->name) - 1);

        it = cJSON_GetObjectItem(item, "path");
        if(!it || it->type != cJSON_String){
            loge("parse config file failed! array");
            goto ERROR;
        }
        strncmp(taskNew->path, it->string, sizeof(taskNew->path) - 1);

        it = cJSON_GetObjectItem(item, "param");
        if(!it || it->type != cJSON_String){
            loge("parse config file failed! array");
            goto ERROR;
        }
        strncmp(taskNew->param, it->string, sizeof(taskNew->param) - 1);

        it = cJSON_GetObjectItem(item, "heartbeat");
        if(!it || it->type != cJSON_Number){
            loge("parse config file failed! array");
            goto ERROR;
        }
        taskNew->heartbeat = it->double;
    }
    set_global_config(maxTasks, queryIntvl, &tmpList);
    cJSON_Delete(root);
    return 0;

ERROR:
    list_for_each_entry_safe(pos, next, &tmpList, node){
        free(pos);
    }

    cJSON_Delete(root);
    return -1;
}

void foreach_task(void (*cb)(taskParm_t *, void *), void *data)
{
    taskParm_t *pos = NULL;
        
    list_for_each_entry(pos, &s_global_config, node){
        if(cb) cb(pos, data);
    }
}

int task_config_init(const char *filename)
{
    char *buf = NULL;
    FILE *fp = NULL;
    uint32_t filesize = 0;
    int ret = 0;
    uint32_t read_len = 0;
    
    memset(&s_global_config, 0, sizeof(s_global_config));
    INIT_LIST_HEAD(&s_global_config.list);

    fp = fopen(filename, "r");
    if(!fp){
        loge("open config file failed! %d-%s", errno, strerror(errno));
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    filesize = ftell(fp);

    buf = malloc(filesize + 1);
    if(!buf){
        return -1;
    }

    memset(buf, 0, filesize + 1);
    fseek(fp, 0L, SEEK_SET);

    while(!((read_len == filesize) || feof(fp))){
        ret = fread(buf + read_len, 1, filesize - read_len, fp);
        if(ret > 0){
            read_len += ret;
        }else{
            int err = ferror(fp);
            loge("read file %s failed %d, %s", filename, err, strerror(err));
            free(buf);
            fclose(fp);
            return -1;
        }
    }

    close(fp);
    fp = NULL;   

    ret = parse_config_file(buf);
    free(buf)

    return ret;
}

