#include <stdio.h>
#include <stdlib.h>
#include <cstring.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <list>


#include "TaskServer.h"
#include "ev_api_comm.h"
#include "task_comm.h"
#include "task_config.h"
#include "task_msg.h"

static TaskServer *taskServer = NULL;

static list<TaskObj> taskList;
static uint32_t s_taskCount = 0;

static list<TaskObjClient> cliList;
static ev_timer_id s_clilist_timer;

event_err_code_e processHeartbeat(void *data, void **reply, size_t *replySize, void *usrData)
{
    heartbeatReq_t *req = (heartbeatReq_t)data;
    TaskObj *obj = (TaskObj)usrData;
    heartbeatRsp_t *rsp = NULL;

    rsp = malloc(sizeof(heartbeatRsp_t));
    if(!rsp){
        return EVENT_ERROR_MEM;
    }

    rsp->heatbeat = obj->getTaskHeartbeat();
    rsp->pid = obj->getTaskPid();

    *reply = rsp;
    *replySize = sizeof(heartbeatRsp_t);

    return EVENT_SUCC;
}


static char *getStatusStr(task_status_e status)
{
    switch(status){
        case TASK_STATUS_U:
            return "unknown";
        case TASK_STATUS_R:
            return "running";
        case TASK_STATUS_S:
            return "sleeping";
        case TASK_STATUS_D
            return "uninterrupt";
        case TASK_STATUS_T:
            return "traced";
        case TASK_STATUS_Z:
            return "zombie";
        default:
            return "unknown";
    }
}

event_err_code_e processGetAllTask(void *data, void **reply, size_t *replySize, void *usrData)
{
    taskAllRsp_t *rsp = NULL;
    int i = 0;
    size_t data_len = 0;

    data_len = sizeof(taskAllRsp_t) + s_taskCount * sizeof(taskSimpleInfo_t);
    rsp = malloc(data_len);
    if(!rsp){
        return EVENT_ERROR_MEM;
    }

    memset(rsp, 0, data_len);
    rsp->count = s_taskCount;

    for(list<TaskObj>::const_iterator iter = taskList.begin(); iter != taskList.end(); iter++){
        TaskObj *obj = *iter;
        strncpy(rsp->taskList[i].name, obj->getTaskName().c_str(), sizeof(rsp->taskList[i].name) - 1);
        rsp->taskList[i].pid = obj->getTaskPid();
        strncpy(rsp->taskList[i].status, getStatusStr(obj->getTaskStatus()), sizeof(rsp->taskList[i].status) - 1);        
        i++;

        if(i == rsp->count){
            break;
        }
    }

    *reply = rsp;
    *replySize = data_len;

    return EVENT_SUCC;
}

event_err_code_e processGetSingleTask(void *data, void **reply, size_t *replySize, void *usrData)
{
    taskDetailInfo_t *rsp = NULL;
    taskSingleReq_t *req = (taskSingleReq_t *)data;
    TaskObj *obj = NULL, *task = NULL;

    for(list<TaskObj>::const_iterator iter = taskList.begin(); iter != taskList.end(); iter++){
        obj = *iter;
        if(req->pid != 0 ){
            if(obj->getTaskPid() == req->pid){
                task = obj;
                break;
            }
        }else{
            if(strcmp(obj->getTaskName().c_str(), req->name) == 0){
                task = obj;
                break;
            }
        }
    }

    if(!task){
        return EVENT_ERROR_NOT_FOUND;
    }

    rsp = malloc(sizeof(taskDetailInfo_t));
    if(!rsp){
        return EVENT_ERROR_MEM;
    }
    memset(rsp , 0, sizeof(taskDetailInfo_t));
    rsp->pid = task->getTaskPid();
    strncpy(rsp->name, task->getTaskName().c_str(), sizeof(rsp->name) - 1);
    strncpy(rsp->param, task->getTaskParam().c_str(), sizeof(rsp->param) - 1);
    strncpy(rsp->status, getStatusStr(task->getTaskStatus()), sizeof(rsp->status) - 1);

    *reply = rsp;
    *replySize = sizeof(taskDetailInfo_t);

    return EVENT_SUCC;
}

event_err_code_e processTaskAction(void *data, void **reply, size_t *replySize, void *usrData)
{
    taskActoinReq_t *req = (taskActoinReq_t *)data;

    TaskObj *obj = NULL, *task = NULL;

    for(list<TaskObj>::const_iterator iter = taskList.begin(); iter != taskList.end(); iter++){
        obj = *iter;
        if(req->pid != 0 ){
            if(obj->getTaskPid() == req->pid){
                task = obj;
                break;
            }
        }else{
            if(strcmp(obj->getTaskName().c_str(), req->name) == 0){
                task = obj;
                break;
            }
        }
    }

    if(!task){
        return EVENT_ERROR_NOT_FOUND;        
    }

    switch(req->action){
        case TASK_ACTION_START:
            task->startTask();
            break;
        case TASK_ACTION_STOP:
            task->stopTask();
            break;
        case TASK_ACTION_RESTART:
            task->restartTask();
            break;
    }

    return EVENT_SUCC;
}


int recv_client_connect(ev_inst_st inst, pid_t pid, int fd)
{
    for(list<TaskObj>::const_iterator iter = taskList.begin(); iter != taskList.end(); iter++){
        TaskObj *obj = *iter;
        if(obj->getTaskPid() == pid){
            logd("find task obj!!");
            obj->startTaskClient(fd);
            return 0;
        }
    }

    TaskObjClient *client = new TaskObjClient(inst, fd, pid);
    if(!client){
        const char *str = "no memory!\n";
        loge("create client failed!");
        write(fd, str, strlen(str) + 1);
        close(fd);
        return -1;
    }
    cliList.push_back(client);

    return 0;
}

static void task_param_cb(taskParm_t *param, void *data)
{
    ev_inst_st inst = (ev_inst_st)data;
    
    TaskObj *obj = new TaskObj(inst, param->name, param->path, param->param, param->heartbeat);
    if(!obj){
        loge("create task %s failed!", param->name);
        return;
    }

    obj.startTask();

    taskList.push_back(obj);
    s_taskCount++;
}


static void cli_list_clear_timer_cb(ev_timer_id  timer, void * data)
{
    for(list<TaskObjClient>::const_iterator iter = cliList.begin(); iter != cliList.end(); iter++){
        TaskObjClient *obj = *iter;
        if(!obj->isClientAvailable()){
            delete obj;
        }
    }
}


int task_core_init(ev_inst_st inst)
{
    taskServer = new TaskServer(inst, TASK_UNIX_PATH);
    if(!taskServer){
        loge("task server create failed!");
        return -1;
    }

    s_clilist_timer = libev_api_create_timer(inst, cli_list_clear_timer_cb, NULL, 10, 10);
    if(!s_clilist_timer){
        delete taskServer;
        loge("s_clilist_clear_timer create failed!");
        return -1;
    }
    libev_api_start_timer(s_clilist_timer);

    foreach_task(task_param_cb, inst);

    return 0;   
}
