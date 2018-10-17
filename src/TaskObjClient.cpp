#include <stdio.h>
#include <stdlib.h>
#include <cstring.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "TaskObjClient.h"
#include "task_core.h"

static void taskReadIoCb(ev_io_id io, void *data)
{
    int ret = 0;
    TaskObjClient *task = (TaskObjClient *)data;
#define BUF_LEN 1024
    char buf[BUF_LEN] = {0};
    int fd = libev_api_watchio_get_fd(io);

    if(!task){
        return;
    }

    logw("task %s read data", task->mName);

    while(1){
        ret = read(fd, buf, BUF_LEN);
        if(ret == 0){
            logw("client close!");
            task->stopTaskClient();
            break;
        }else if(ret > 0){
            task->recvData(buf, ret);
            break;
        }else if(ret < 0 && errno == EINTR){
            continue;
        }else{
            loge("read fd failed! %d-%s", errno, strerror(errno));
            task->stopTaskClient();
            break;
        }
    }

    return;
}


int TaskObjClient::startTaskClient(int fd)
{
    int ret = 0;
    
    stopTaskClient();

    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);
    ret = libev_api_watchio_create(mInst, fd, LIBEV_IO_READ, taskReadIoCb, this);
    if(ret){
        loge("create watch io failed!");
        return ret;
    }
    msgServer = new MsgServer(fd);
    if(!msgServer){
        stopTaskClient();
        return -1;
    }
    msgServer.registerEventCb(EVENT_TYPE_HEARTBEAT, processHeartbeat, this);
    msgServer.registerEventCb(EVENT_TYPE_QUERY_ALL, processGetAllTask, this);
    msgServer.registerEventCb(EVENT_TYPE_QUERY_TASK, processGetSingleTask, this);
    msgServer.registerEventCb(EVENT_TYPE_TASK_ACTION, processTaskAction, this);

    isAvailable = 1;

    return 0;
}

int TaskObjClient::stopTaskClient()
{
    if(msgServer){
        delete msgServer;
        msgServer = NULL;
    }

    if(mReadIo){
        libev_api_watchio_destroy(&mReadIo);
    }
    
    if(mClientFd > 0){
        close(mClientFd);
        mClientFd = -1;
    }

    isAvailable = 0;

    return 0;
}

int TaskObjClient::recvData(char *buf, int len)
{
    int ret = 0;
    
    if(msgServer){
        ret = msgServer->recvData(buf, len);
        if(ret){
            stopTaskClient();
        }
    }

    return 0;
}

TaskObjClient::TaskObjClient(ev_inst_st inst, int fd, int pid)
{
    int ret = 0;
    
    mInst = inst;
    mClientFd = fd;
    mPid = pid;

    ret = startTaskClient(fd);
    if(ret){
        loge("startTaskClient failed! ");
        throw Exception("startTaskClient failed");
    }
}

TaskObjClient::~TaskObjClient()
{
    stopTaskClient();
}

int TaskObjClient::isClientAvailable()
{
    return isAvailable;
}


