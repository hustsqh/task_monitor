#include <stdio.h>
#include <stdlib.h>
#include <cstring.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>

#include "task_server.h"
#include "task_debug.h"


static void serverReadIoCb(ev_io_id io, void *data)
{
    TaskServer *s = (TaskServer *)data;    
    struct sockaddr_un sun;
    struct ucred cred;
    int len = 0;
    int fd = -1;
    int ret = 0;
    pid_t pid = 0;
    
    if(!s){
        return;
    }

    len = sizeof(sun);
    fd = accept(s->mServerFd, (struct sockaddr *)&sun, &len);
    if(fd < 0){
        loge("accept get failed! %s", strerror(errno));
        return;
    }

    len = sizeof(struct ucred);
    ret = getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len);
    if(ret){
        loge("getsockopt failed!%s ", strerror(errno));
        close(fd);
        return;
    }

    pid = cred.pid;
    recv_client_connect(libev_api_watchio_get_inst(io), pid, fd);
    return;
}


TaskServer::TaskServer(ev_inst_st inst, std::string unixPath)
{
    mInst = inst;
    mUnixPath = unixPath;
    mServerFd = -1;
    mClientCount = 0;
    mServerIo = NULL;
}

TaskServer::~TaskServer()
{
    stopServer();
    mUnixPath = "";
    mServerFd = -1;
    mClientCount = 0;
}

int TaskServer::startServer()
{
    int fd = -1;
    int size = 0;
    int ret = 0;
    struct sockaddr_un sun;

    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, mUnixPath.c_str(), sizeof(sun.sun_path) - 1);
    
    fd = socket(AF_UNIX, SOCKET_STREAM, 0);
    if(fd < 0){
        loge("create server fd failed! %s", strerror(errno));
        return -1;
    }

    size = offsetof(struct sockaddr_un, sun_path) + strlen(sun.sun_path);

    ret = bind(fd, &sun, size);
    if(ret < 0){
        loge("bind failed! %s", strerror(errno));
        close(fd);
        return -1;
    }

    ret = listen(fd, 5);
    if(ret){
        loge("listen failed! %s", strerror(errno));
        close(fd);
        return -1;
    }

    mServerIo = libev_api_watchio_create(mInst, fd, LIBEV_IO_READ, serverReadIoCb, this);
    if(!mServerIo){
        loge("create watch io failed!");
        close(fd);
        return -1;
    }

    mServerFd = fd;

    return 0;
}

int TaskServer::stopServer()
{
    // TODO: clear all clients
    mClientCount = 0;

    if(mServerIo){
        libev_api_watchio_destroy(&mServerIo);
    }
    if(mServerFd > 0){
        close(mServerFd);
        mServerFd = -1;
    }

    return 0;
}

int TaskServer::restartServer()
{
    stopServer();

    return startServer();
}

