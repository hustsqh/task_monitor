#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <arpa/inet.h>

#include "cjson/cJSON.h"


/*
 * 消息格式
 * --------------------------------
 * |reqid|len|   data              |
 * --------------------------------
 *  4     4    ...
*/


#define SOCKET_BUF_SIZE (10 * 1024)
#define DATA_HEAD_LEN 8

static int writeSocket(int fd, char *data, size_t len)
{
    int ret = 0;
    size_t write_n = 0;

    do{
        ret = write(fd, data + write_n, len - write_n);
        if(ret < 0 && (errno == EAGAIN || errno == EINTR)){
            continue;
        }else if(ret < 0){
            loge("write data to socket failed!! %s", strerror(errno));
            return -1;
        }else {
            write_n += ret;
            if(write_n == len){
                break;
            }
        }
    }while(1);

    return 0;
}


static int getAvailablePackage(ring_buf_t *rb, uint32_t *reqIdOut, uint32_t *lenOut, char **dataOut)
{
    size_t ret = 0;
    size_t dataLen = 0;
    char buf[DATA_HEAD_LEN] = {0};
    uint32_t reqId = 0;
    uint32_t len = 0;
    char *data = NULL;
    
    dataLen = get_data_size(rb);
    if(dataLen <= DATA_HEAD_LEN){
        return NULL;
    }

    ring_buf_read_data_only(rb, buf, DATA_HEAD_LEN);
    reqId = ntohl(buf);
    len = ntohl(buf + 4);

    if(dataLen < len + DATA_HEAD_LEN){
        return 0;
    }

    data = malloc(len + DATA_HEAD_LEN);
    if(!data){
        loge("malloc size %u failed!", len + DATA_HEAD_LEN);
        return 0;
    }

    memset(data, 0, len + DATA_HEAD_LEN);
    ret = ring_buf_read_data(rb, data, len + DATA_HEAD_LEN);
    if(ret != len + DATA_HEAD_LEN){
        loge("read data failed!!!");
        free(data);
        return -1;
    }

    *reqIdOut = reqId;
    *lenOut = len;
    **dataOut = data;

    return 1;
}


int MsgServer::createFailMsg(uint32_t errcode, char **replyData, uint32_t *replySize)
{
    cJSON *root = NULL;
    char *data = NULL;

    root = cJSON_CreateObject();
    if(!root){
        loge("create json object failed!");
        return -1;
    }

    cJSON_AddNumberToObject(root, "ret", errcode);

    data = cJSON_PrintUnformatted(root);
    if(data == NULL){
        cJSON_Delete(root);
        loge("print json failed!");
        return -1;
    }

    **replyData = data;
    *replySize = strlen(data) + 1;
    cJSON_Delete(root);

    return 0;
}



event_err_code_e MsgServer::processHeartbeatReq(uint32_t reqId, cJSON *data, char **replyData, uint32_t *replySize)
{
    cJSON *pid = NULL;
    cJSON *heartbeat = NULL;
    cJSON *root = NULL;
    cJSON *dataJsonReply = NULL;
    char *rspJson = NULL;
    int ret = 0;
    heartbeatReq_t hbreq;
    heartbeatRsp_t *rsp = NULL;
    uint32_t rspLen = 0;
    
    if(!data || data->type != cJSON_Object){
        loge("parse data failed!");
        return EVENT_ERROR_PARSE;
    }

    pid = cJSON_GetObjectItem(data, "pid");
    if(!pid || pid->type != cJSON_Number){
        loge("parse pid failed!");
        return EVENT_ERROR_PARSE;
    }

    heartbeat = cJSON_GetObjectItem(data, "hearbeat");
    if(!heartbeat || heartbeat->type != cJSON_Number){
        loge("parse heartbeat failed!");
        return EVENT_ERROR_PARSE;
    }

    memset(&hbreq, 0, sizeof(hbreq));
    hbreq.pid = pid->valueint;
    hbreq.heartbeat = heartbeat->valuedouble;

    if(!mEventCb[EVENT_TYPE_HEARTBEAT]){
        return EVENT_ERROR_UNPROCESS;
    }

    ret = mEventCb[EVENT_TYPE_HEARTBEAT](&hbreq, &rsp, &rspLen);
    if(ret != EVENT_SUCC){
        loge("event cb heartbeat return %d", ret);
        return ret;
    }

    if(!rsp){
        loge("heartbeat return data invalid!");
        return EVENT_ERROR_MEM;
    }

    root = cJSON_CreateObject();
    if(!root){
        return EVENT_ERROR_MEM;
    }
    dataJsonReply = cJSON_CreateObject();
    if(!dataJsonReply){
        cJSON_Delete(root);
        return EVENT_ERROR_MEM;
    }

    cJSON_AddNumberToObject(root, "ret", 0);
    cJSON_AddItemToObject(root, "data", dataJsonReply);
    cJSON_AddNumberToObject(dataJsonReply, "pid", hbreq.pid);
    cJSON_AddNumberToObject(dataJsonReply, "heartbeat", hbreq.heartbeat);

    rspJson = cJSON_PrintUnformatted(root);
    if(!rspJson){
        cJSON_Delete(root);
        return EVENT_ERROR_MEM;
    }

    **replyData = rspJson;
    *replySize = strlen(rspJson) + 1;
    cJSON_Delete(root);

    return 0;
}


event_err_code_e MsgServer::processQueryAllReq(uint32_t reqId, cJSON *data, char **replyData, uint32_t *replySize)
{
    int ret = 0;
    cJSON *root = NULL;
    cJSON *dataJsonReply = NULL;
    char *rspJson = NULL;
    taskAllRsp_t *rspData = NULL;
    uint32_t rspLen = 0;
    int i = 0;
    taskSimpleInfo_t *simpleInfo = NULL;

    if(!mEventCb[EVENT_TYPE_QUERY_ALL]){
        return EVENT_ERROR_UNPROCESS;
    }

    ret = mEventCb[EVENT_TYPE_QUERY_ALL](NULL, &rspData, &rspLen);
    if(ret != EVENT_SUCC){
        loge("event cb heartbeat return %d", ret);
        return ret;
    }

    if(!rspData){
        loge("heartbeat return data invalid!");
        return EVENT_ERROR_MEM;
    }

    root = cJSON_CreateObject();
    if(!root){
        return EVENT_ERROR_MEM;
    }
    dataJsonReply = cJSON_CreateArray();
    if(!dataJsonReply){
        cJSON_Delete(root);
        return EVENT_ERROR_MEM;
    }

    cJSON_AddNumberToObject(root, "ret", 0);
    cJSON_AddItemToObject(root, "data", dataJsonReply);

    for(i = 0; i < rspData->count; i++){
        cJSON *item = cJSON_CreateObject();
        if(!item){
            cJSON_Delete(root);
            return EVENT_ERROR_MEM;
        }
        simpleInfo = rspData->taskList[i];
        cJSON_AddStringToObject(item, "name", simpleInfo->name);
        cJSON_AddNumberToObject(item, "pid", simpleInfo->pid);
        cJSON_AddStringToObject(item, "status", simpleInfo->status);
        cJSON_AddItemToArray(dataJsonReply, item);
    }

    rspJson = cJSON_PrintUnformatted(root);
    if(!rspJson){
        cJSON_Delete(root);
        return EVENT_ERROR_MEM;
    }

    **replyData = rspJson;
    *replySize = strlen(rspJson) + 1;
    cJSON_Delete(root);

    return ret;
}

event_err_code_e MsgServer::processQuerySingleTaskReq(uint32_t reqId, cJSON *data, char **replyData, uint32_t *replySize)
{
    cJSON *pid = NULL;
    cJSON *name = NULL;
    int ret = 0;
    taskSingleReq_t tsReq;
    taskDetailInfo_t *rspData = NULL;
    uint32_t rspLen = 0;
    cJSON *root = NULL;
    cJSON *dataJsonReply = NULL;
    char *rspJson = NULL;
    
    if(!data || data->type != cJSON_Object){
        loge("parse data failed!");
        return EVENT_ERROR_PARSE;
    }

    pid = cJSON_GetObjectItem(data, "pid");
    if(pid){
        if(pid->type != cJSON_Number){
            loge("parse pid failed! so get name");
            return EVENT_ERROR_PARSE;
        }
    }else{
        name = cJSON_GetObjectItem(data, "name");
        if(!name || name->type != cJSON_String){
            loge("parse name failed!");
            return EVENT_ERROR_PARSE;
        }
    }

    memset(&tsReq, 0, sizeof(tsReq));
    if(pid){
        tsReq.pid = pid->valueint;
    }
    if(name){
        strncpy(tsReq.name, name->valuestring, sizeof(tsReq.name) - 1);
    }

    if(!mEventCb[EVENT_TYPE_QUERY_TASK]){
        return EVENT_ERROR_UNPROCESS;
    }

    ret = mEventCb[EVENT_TYPE_QUERY_TASK](&tsReq, &rspData, &rspLen);
    if(ret != EVENT_SUCC){
        loge("event cb EVENT_TYPE_QUERY_TASK return %d", ret);
        return ret;
    }

    if(!rspData){
        loge("heartbeat return data invalid!");
        return EVENT_ERROR_MEM;
    }

    root = cJSON_CreateObject();
    if(!root){
        return EVENT_ERROR_MEM;
    }
    dataJsonReply = cJSON_CreateObject();
    if(!dataJsonReply){
        cJSON_Delete(root);
        return EVENT_ERROR_MEM;
    }

    cJSON_AddNumberToObject(root, "ret", 0);
    cJSON_AddItemToObject(root, "data", dataJsonReply);

    cJSON_AddNumberToObject(dataJsonReply, "pid", rspData->pid);
    cJSON_AddStringToObject(dataJsonReply, "name", rspData->name);
    cJSON_AddStringToObject(dataJsonReply, "param", rspData->param);
    cJSON_AddStringToObject(dataJsonReply, "status", rspData->status);
    cJSON_AddNumberToObject(dataJsonReply, "heartbeat", rspData->heartbeat);

    rspJson = cJSON_PrintUnformatted(root);
    if(!rspJson){
        cJSON_Delete(root);
        return EVENT_ERROR_MEM;
    }

    **replyData = rspJson;
    *replySize = strlen(rspJson) + 1;
    cJSON_Delete(root);

    return 0;
}

event_err_code_e MsgServer::processTaskActionReq(uint32_t reqId, cJSON *data, char **replyData, uint32_t *replySize)
{
    cJSON *pid = NULL;
    cJSON *action = NULL;
    int ret = 0;
    cJSON *root = NULL;
    taskActoinReq_t taReq;
    char *rspJson = NULL;

    if(!data || data->type != cJSON_Object){
        loge("parse data failed!");
        return EVENT_ERROR_PARSE;
    }

    pid = cJSON_GetObjectItem(data, "pid");
    if(!pid || pid->type != cJSON_Number){
        loge("parse pid failed!");
        return EVENT_ERROR_PARSE;
    }

    action = cJSON_GetObjectItem(data, "hearbeat");
    if(!action || action->type != cJSON_Number){
        loge("parse heartbeat failed!");
        return EVENT_ERROR_PARSE;
    }

    memset(&taReq, 0, sizeof(taReq));
    taReq.pid = pid->valueint;
    taReq.action = action->valueint;

    if(!mEventCb[EVENT_TYPE_TASK_ACTION]){
        return EVENT_ERROR_UNPROCESS;
    }

    ret = mEventCb[EVENT_TYPE_TASK_ACTION](&taReq, NULL, NULL);
    if(ret != EVENT_SUCC){
        loge("event cb TaskActionReq return %d", ret);
        return ret;
    }

    root = cJSON_CreateObject();
    if(!root){
        return EVENT_ERROR_MEM;
    }
    cJSON_AddNumberToObject(root, "ret", 0);

    rspJson = cJSON_PrintUnformatted(root);
    if(!rspJson){
        cJSON_Delete(root);
        return EVENT_ERROR_MEM;
    }

    **replyData = rspJson;
    *replySize = strlen(rspJson) + 1;
    cJSON_Delete(root);

    return 0;
}


int MsgServer::parseServerData(uint32_t reqId, char *data, uint32_t len)
{
    cJSON *root = NULL;
    msgRequest_t *pos = NULL, *next = NULL;
    int ret = 0;
    char *replyData = NULL;
    uint32_t replySize = 0;
    event_err_code_e errcode = EVENT_SUCC;

    logw("get data:%s", data);

    root = cJSON_Parse(data);
    if(!root){
        loge("parse data failed! %s", data);
        return -1;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if(!type || type->type != cJSON_Number){
        loge("parse type failed! %s", data);
        return -1;
    }

    cJSON *value = cJSON_GetObjectItem(root, "data");

    switch(type->valueint){
        case EVENT_TYPE_HEARTBEAT:
            errcode = processHeartbeatReq(reqId, data, &replyData, &replySize);
            break;
        case EVENT_TYPE_QUERY_ALL:
            errcode = processQueryAllReq(reqId, data, &replyData, &replySize);
            break;
        case EVENT_TYPE_QUERY_TASK:
            errcode = processQuerySingleTaskReq(reqId, data, &replyData, &replySize);
            break;
        case EVENT_TYPE_TASK_ACTION:
            errcode = processTaskActionReq(reqId, data, &replyData, &replySize);
            break;
        default:
            errcode = EVENT_ERROR_UNKNOWN_TYPE;
            break;
    }

    if(errcode != EVENT_SUCC){
        createFailMsg(errcode, &replyData, &replySize);
    }

    if(replyData){
        logd("reply data to client:%s", replyData);
        ret += writeSocket(htonl(reqId));
        ret += writeSocket(htonl(replySize));
        ret + = writeSocket(mClientFd, replyData, replySize);
        free(replyData);
    }
    
    cJSON_Delete(root);
    return ret;
}


int MsgServer::procData()
{
    uint32_t reqId = 0;
    uint32_t len = 0;
    char *data = NULL;
    int ret = 0;

    ret = getAvailablePackage(mRb, &reqId, &len, &data);
    if(ret <= 0){
        return ret;
    }

    ret = parseServerData(reqId, data + DATA_HEAD_LEN, len);
    free(data);

    return ret;
}



MsgServer::MsgServer(int fd)
{
    mClientFd = fd;
    mRb = ring_buf_create(SOCKET_BUF_SIZE);
    if(!mRb){
        loge("create ringbuf failed!");
        goto ERROR;
    }

    memset(mEventCb, 0, sizeof(mEventCb));

    return;

ERROR:
    throw exception("create MsgServer failed!");
}


MsgServer::~MsgServer()
{
    ring_buf_destory(mRb);
    mRb = NULL;
    memset(mEventCb, 0, sizeof(mEventCb));
}

int MsgServer::registerEventCb(event_type_e type, processEventCb cb)
{
    if(type >= EVENT_TYPE_MAX){
        return -1;
    }
    if(mEventCb[type]){
        return -1;
    }

    mEventCb[type] = cb;

    return 0;
}

int MsgServer::recvData(char * data, size_t len)
{
    size_t ret = 0;

    ret = ring_buf_write_data(mRb, data, len);
    if(ret == 0){
        loge("write ringbuf len failed!");
        return -1;
    }

    return procData();
}

static void msgClientReqTimerCb(ev_timer_id timer, void *data)
{
    msgRequest_t *req = (msgRequest_t *)data;
    msgRequest_t *pos = NULL, *next = NULL;

    if(!req){
        return;
    }
    if(req->cb){
        req->cb(EVENT_ERROR_TIMEOUT, NULL);
    }

    list_del(&(req->node));
    libev_api_destroy_timer(&(req->timer));
    free(req);
}

uint32_t MsgClient::getNextReqId()
{
    return ++mCurReqId;
}

msgRequest_t * MsgClient::reqNodeCreate(uint32_t reqId, event_type_e type, double timeout)
{
    msgRequest_t *req = NULL;

    req = malloc(sizeof(msgRequest_t));
    if(!req){
        loge("malloc %u failed!", sizeof(msgRequest_t));
        return NULL;
    }

    memset(req, 0, sizeof(msgRequest_t));
    INIT_LIST_HEAD(&req->node);
    req->reqId = reqId;
    req->type = type;
    req->cb = cb;
    req->timeStart = ev_now();
    req->timeout = timeout;
    req->client = this;
    req->timer = libev_api_create_timer(mInst, msgClientReqTimerCb, req, timeout, 0);
    if(!(req->timer)){
        loge("create req timer failed!");
        free(req);
        return NULL;
    }

    return req;
}

void MsgClient::destroyReqNode(msgRequest_t *req)
{
    if(req){
        if(req->timer){
            libev_api_destroy_timer(&req->timer);
        }
        free(req);
    }
}


int MsgClient::sendRequest(event_type_e reqType, char *data, uint32_t len, void (*cb)(int ret, void *data), double timeout)
{
    msgRequest_t * req = NULL;
    int ret = 0;
    uint32_t reqId = getNextReqId();

    if(timeout == 0){
        ret += writeSocket(htonl(reqId));
        ret += writeSocket(htonl(len));
        ret += writeSocket(data, len);
        return ret;
    }

    ret += writeSocket(htonl(reqId));
    ret += writeSocket(htonl(len));
    ret += writeSocket(data, len);
    if(ret){
        return ret;
    }

    req = reqNodeCreate(reqId, reqType, timeout);
    if(req){
        libev_api_start_timer(req->timer);
        list_add(&req->node, mRequestList);
        return 0;
    }

    return -1;
}


int MsgClient::processHeartbeatRsp(msgRequest_t * req, int ret, cJSON *data)
{
    heartbeatRsp_t rsp;
    cJSON *pidJson = NULL, heartBeatJson = NULL;

    if(data == NULL){
        loge("data is null!");
        return -1;
    }

    if(data->type != cJSON_Object){
        loge("type error %d", data->type);
        return -1;
    }

    memset(&rsp, 0, sizeof(heartbeatRsp_t));
    pidJson = cJSON_GetObjectItem(data, "pid");
    if(!pidJson){
        loge("parse pid failed!");
        return -1;
    }

    heartBeatJson = cJSON_GetObjectItem(data, "heatbeat");
    if(!heartBeatJson){
        loge("parse heatbeat failed!");
        return -1;
    }

    rsp.pid = pidJson->valueint;
    rsp.heatbeat = pidJson->valuedouble;

    req->cb(ret, &rsp);

    return 0;
}

int MsgClient::processQueryAllRsp(msgRequest_t * req, int ret, cJSON *data)
{
    taskAllRsp_t *pAllTask;
    int size = 0, i = 0;
    
    if(data == NULL){
        loge("data is null!");
        return -1;
    }

    if(data->type != cJSON_Raw){
        loge("type error %d", data->type);
        return -1;
    }

    size = cJSON_GetArraySize(data);
    
    pAllTask = malloc(sizeof(taskAllRsp_t) + size * sizeof(taskSimpleInfo_t));
    memset(pAllTask, 0, sizeof(taskAllRsp_t) + size * sizeof(taskSimpleInfo_t));
    pAllTask->count = size;
    
    for (i = 0 i < size; i++){
        cJSON * name = NULL, *pid = NULL, *status = NULL;
        cJSON * item = cJSON_GetArrayItem(data, i);
        if(item == NULL || item->type != cJSON_Object){
            loge("parse item failed %d", i);
            goto ERROR;
        }
        
        name = cJSON_GetObjectItem(item, "name");
        if(name == NULL || name.type != cJSON_String){
            loge("parse name failed!");
            goto ERROR;
        }
        pid = cJSON_GetObjectItem(item, "pid");
        if(pid == NULL || pid->type != cJSON_Number){
            loge("parse pid failed!");
            goto ERROR;
        }
        status = cJSON_GetObjectItem(item, "status");
        if(status == NULL || status.type != cJSON_String){
            loge("parse status failed!");
            goto ERROR;
        }
        
        taskSimpleInfo_t *info = pAllTask->taskList + i;
        strncpy(info->name, name->valuestring, sizeof(info->name) - 1);
        info->pid = pid->valueint;
        strncpy(info->status, name->valuestring, sizeof(info->status) - 1);
    }

    req->cb(ret, pAllTask);
    free(pAllTask);

    return 0;
ERROR:
    if(pAllTask){
        free(pAllTask);
    }
    return -1;
}


int MsgClient::processQueryTaskRsp(msgRequest_t * req, int ret, cJSON *data)
{
    taskDetailInfo_t taskInfo;
    cJSON *pid = NULL, *name = NULL, *param = NULL, *status = NULL, *heartBeat = NULL;

    if(data == NULL){
        loge("data is null!");
        return -1;
    }

    if(data->type != cJSON_Object){
        loge("type error %d", data->type);
        return -1;
    }

    memset(&taskInfo, 0, sizeof(taskInfo));
    
    pid = cJSON_GetObjectItem(data, "pid");
    if(!pid || pid->type != cJSON_Number){
        loge("parse pid failed!");
        return -1;
    }

    name = cJSON_GetObjectItem(data, "name");
    if(!name || name->type != cJSON_String){
        loge("parse name failed!");
        return -1;
    }

    param = cJSON_GetObjectItem(data, "param");
    if(!param || param->type != cJSON_String){
        loge("parse name failed!");
        return -1;
    }

    status = cJSON_GetObjectItem(data, "status");
    if(!status || status->type != cJSON_String){
        loge("parse status failed!");
        return -1;
    }

    heartBeat = cJSON_GetObjectItem(data, "heartBeat");
    if(!heartBeat || heartBeat->type != cJSON_Number){
        loge("parse heartBeat failed!");
        return -1;
    }

    taskInfo.pid = pid->valueint;
    strncpy(taskInfo.name, name->valuestring, sizeof(taskInfo.name) - 1);
    strncpy(taskInfo.param, param->valuestring, sizeof(taskInfo.param) - 1);
    strncpy(taskInfo.status, status->valuestring, sizeof(taskInfo.status) - 1);
    taskInfo.heartbeat = heartBeat->valuedouble;

    req->cb(ret, &taskInfo);

    return 0;
}

int MsgClient::processTaskActionRsp(msgRequest_t * req, int ret, cJSON *data)
{
    req->cb(ret);

    return 0;
}



int MsgClient::processRspData(msgRequest_t * req, int ret, cJSON *data)
{
    if(!(req->cb)){
        return 0;
    }
    if(ret != 0){
        req->cb(ret, NULL);
        return 0;
    }
    
    switch(req->type){
        case EVENT_TYPE_HEARTBEAT:
            return processHeartbeatRsp(req, ret, data);
        case EVENT_TYPE_QUERY_ALL:
            return processQueryAllRsp(req, ret, data);
        case EVENT_TYPE_QUERY_TASK:
            return processQueryTaskRsp(req, ret, data);
        case EVENT_TYPE_TASK_ACTION:
            return processTaskActionRsp(req, ret, data);
        default:
            return -1;
    }
}


int MsgClient::parseClientData(uint32_t reqId, char *data, uint32_t len)
{
    cJSON *root = NULL;
    msgRequest_t *pos = NULL, *next = NULL;
    int ret = 0;

    logw("get data:%s", data);

    root = cJSON_Parse(data);
    if(!root){
        loge("parse data failed! %s", data);
        return -1;
    }

    cJSON *retval = cJSON_GetObjectItem(root, "ret");
    if(!retval){
        loge("parse type failed! %s", data);
        return -1;
    }

    cJSON *value = cJSON_GetObjectItem(root, "data");

    list_for_each_entry_safe(pos, next, &mRequestList, node){
        if(pos->reqId == reqId){
            ret = processRspData(pos, retval->valueint, value);
            list_del(pos);
            destroyReqNode(pos);
            break;
        }
    }
    cJSON_Delete(root);
    return ret;
}


int MsgClient::procData()
{
    uint32_t reqId = 0;
    uint32_t len = 0;
    char *data = NULL;
    int ret = 0;

    ret = getAvailablePackage(mRb, &reqId, &len, &data);
    if(ret <= 0){
        return ret;
    }

    ret = parseClientData(reqId, data + DATA_HEAD_LEN, len);
    free(data);

    return ret;
}

int MsgClient::recvData(char *data, size_t len)
{
    size_t ret = 0;

    ret = ring_buf_write_data(mRb, data, len);
    if(ret == 0){
        loge("write ringbuf len failed!");
        return -1;
    }

    return procData();
}


MsgClient::MsgClient(ev_inst_st inst, int fd)
{
    mInst = inst;
    mClientFd = fd;
    INIT_LIST_HEAD(&mRequestList);
    mRb = ring_buf_create(SOCKET_BUF_SIZE);
    if(!mRb){
        throw exception("create ring buf failed!");
    }
}

MsgClient::~MsgClient()
{
    msgRequest_t *pos = NULL, *next = NULL;
    
    list_for_each_entry_safe(pos, next, &mRequestList, node){
        list_del(pos);
        destroyReqNode(pos);
    }

    ring_buf_destory(mRb);
}


int MsgClient::sendReqHearbeat(pid_t pid, double heartbeat, void (*cb)(int ret, heartbeatRsp_t *rsp))
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    char *json = NULL;
    int ret = 0;

    root = cJSON_CreateObject();
    if(!root){
        loge("root create failed!");
        return -1;
    }

    data = cJSON_CreateObject();
    if(!data){
        loge("create data failed!");
        cJSON_Delete(root);
        return -1;
    }

    cJSON_AddNumberToObject(root, "type", EVENT_TYPE_HEARTBEAT);
    cJSON_AddItemToObject(root, "data", data);
    
    cJSON_AddNumberToObject(data, "pid", pid);
    cJSON_AddNumberToObject(data, "heartbeat", heartbeat);
    
    json = cJSON_PrintUnformatted(root);
    if(!json){
        loge("print json failed!");
        cJSON_Delete(root);
        return -1;
    }

    logd("create heartbeat req json:%s", json);

    ret = sendRequest(json, strlen(json) + 1, cb, mDefaultReqTimeout);
    free(json);
    cJSON_Delete(root);

    return ret;
}

int MsgClient::sendReqQueryAllTask(void (*cb)(int ret, taskAllRsp_t *rsp))
{
    
    cJSON *root = NULL;
    char *json = NULL;
    int ret = 0;

    root = cJSON_CreateObject();
    if(!root){
        loge("root create failed!");
        return -1;
    }

    cJSON_AddNumberToObject(root, "type", EVENT_TYPE_QUERY_ALL);
    json = cJSON_PrintUnformatted(root);
    if(!json){
        loge("print json failed!");
        cJSON_Delete(root);
        return -1;
    }

    logd("create QueryAllTask req json:%s", json);

    ret = sendRequest(json, strlen(json) + 1, cb, mDefaultReqTimeout);
    free(json);
    cJSON_Delete(root);

    return ret;
}

int MsgClient::sendReqQuerySingleTask(void (*cb)(int ret, taskDetailInfo_t *rsp))
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    char *json = NULL;
    int ret = 0;

    root = cJSON_CreateObject();
    if(!root){
        loge("root create failed!");
        return -1;
    }

    data = cJSON_CreateObject();
    if(!data){
        loge("create data failed!");
        cJSON_Delete(root);
        return -1;
    }

    cJSON_AddNumberToObject(root, "type", EVENT_TYPE_QUERY_TASK);
    cJSON_AddItemToObject(root, "data", data);
    
    cJSON_AddNumberToObject(data, "pid", pid);
    
    json = cJSON_PrintUnformatted(root);
    if(!json){
        loge("print json failed!");
        cJSON_Delete(root);
        return -1;
    }

    logd("create QuerySingleTask req json:%s", json);

    ret = sendRequest(json, strlen(json) + 1, cb, mDefaultReqTimeout);
    free(json);
    cJSON_Delete(root);

    return ret;
}

int MsgClient::sendReqTaskAction(taskActionType_e action, void (*cb)(int ret, void *data))
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    char *json = NULL;
    int ret = 0;

    root = cJSON_CreateObject();
    if(!root){
        loge("root create failed!");
        return -1;
    }

    data = cJSON_CreateObject();
    if(!data){
        loge("create data failed!");
        cJSON_Delete(root);
        return -1;
    }

    cJSON_AddNumberToObject(root, "type", EVENT_TYPE_TASK_ACTION);
    cJSON_AddItemToObject(root, "data", data);
    
    cJSON_AddNumberToObject(data, "pid", pid);
    cJSON_AddNumberToObject(data, "action", action);
    
    json = cJSON_PrintUnformatted(root);
    if(!json){
        loge("print json failed!");
        cJSON_Delete(root);
        return -1;
    }

    logd("create TaskAction req json:%s", json);

    ret = sendRequest(json, strlen(json) + 1, cb, mDefaultReqTimeout);
    free(json);
    cJSON_Delete(root);

    return ret;
}




