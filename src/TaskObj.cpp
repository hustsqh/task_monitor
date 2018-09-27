#include <stdio.h>
#include <stdlib.h>
#include <cstring.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>


#include "task_debug.h"



static void TaskTimeoutCb(ev_timer_id timer, void *data)
{
    TaskObj *task = (TaskObj *)data;

    if(!task){
        return;
    }
    
    loge("task %s timeout!!!, so restart it!", task->mName);

    task->restartTask();
}

static void taskReadIoCb(ev_io_id io, void *data)
{
    TaskObj *task = (TaskObj *)data;

    if(!task){
        return;
    }

    logw("task %s read data", task->mName);

    // TODO:

    return;
}



void TaskObj::clearTask()
{
    if(mStatus != TASK_STATUS_U){
        stopTask();
    }

    if(mTimer != NULL){
        libev_api_destroy_timer(&mTimer);
    }

    if(mReadIo){
        libev_api_watchio_destroy(&mReadIo);
    }

    if(mClientFd > 0){
        close(mClientFd);
        mClientFd = -1;
    }

    mInst = NULL;
    mName = "";
    mPath = "";
    mParam = "";
    mHeartbeat = 0;
}





TaskObj::TaskObj(ev_inst_st *inst, std::string name, std::string path, std::string param, uint32_t heartbeat)
{
    mInst = inst;
    mName = name;
    mPath = path;
    mParam = param;
    mHeartbeat = heartbeat;

    logd("create task %s", name.c_str());

    mTimer = libev_api_create_timer(mInst, TaskTimeoutCb, this, getRandom(), mHeartbeat);
    if(mTimer == NULL){
        goto ERROR;
    }
    mReadIo = NULL;
    
    return;
ERROR:
    clearTask();
    throw exception("create task failed!");           
}

TaskObj::~TaskObj()
{
    clearTask();
}

int TaskObj::startTask()
{
    pid_t pid = 0;
    
    if(mStatus != TASK_STATUS_U){
        logw("task is running!");
        return -1;
    }

    pid = fork();
    if(pid == 0){
        char *argp[] = {(char *)"sh", (char *)"-c", NULL, NULL};
        argp[2] = (mPath + " " + mParam).c_str();
        execv("/bin/sh", argp);
        _exit(127);
    }else{
        mPid = pid;
        mStatus = TASK_STATUS_R;
        libev_api_start_timer(mTimer);
    }

    return 0;
}

int TaskObj::stopTask()
{
    int ret = 0;
    
    if(mStatus == TASK_STATUS_U){
        loge("timer is already stopped!");
        return -1;
    }

    ret = kill(mPid, SIGKILL);
    if(ret){
        loge("kill %d failed, %s", mPid, strerror(errno));
    }

    if(mReadIo){
        libev_api_watchio_destroy(&mReadIo);
        if(mClientFd > 0){
            close(mClientFd);
            mClientFd = -1;
        }
    }

    if(mTimer){
        libev_api_stop_timer(mTimer);
    }

    ret = waitpid(mPid);
    logw("wait for pid %d return %d", mPid, ret);
    mStatus = TASK_STATUS_U;

    return 0;
}

int TaskObj::restartTask()
{
    stopTask();
    return startTask();
}


int TaskObj::startTaskClient(int fd)
{
    int ret = 0;
    
    stopTaskClient();

    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);
    ret = libev_api_watchio_create(mInst, fd, LIBEV_IO_READ, taskReadIoCb, this);
    if(ret){
        loge("create watch io failed!");
        return ret;
    }
    mClientFd = fd;

    return 0;
}

int TaskObj::stopTaskClient()
{
    if(mReadIo){
        libev_api_watchio_destroy(mReadIo);
    }
    
    if(mClientFd > 0){
        close(mClientFd);
        mClientFd = -1;
    }

    return 0;
}


