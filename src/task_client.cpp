#include <stdio.h>
#include <stdlib.h>
#include <cstring.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>

#include "task_client.h"
#include "task_comm.h"
#include "task_debug.h"

#include "ev_api_comm.h"

static pthread_t s_pthread_id;
static ev_inst_st s_inst = NULL;
static ev_io_id s_client_io = NULL;
static ev_timer_id s_heartbeat_timer = NULL;
static int is_connected = 0;
static int s_client_fd = -1;
static MsgClient *s_msg_client = NULL;

static double s_heartbeat_timeout = 0;

#define GLOBAL_BUF_LEN 1024
static char s_global_buf[GLOBAL_BUF_LEN] = {0};


static void client_readio_cb(ev_io_id io, void *data)
{
    MsgClient *client = (MsgClient *)data;
    ssize_t len = 0;
    int ret = 0;
    int fd = libev_api_watchio_get_fd(io);

    if(!data){
        return;
    }

    memset(s_global_buf, 0, GLOBAL_BUF_LEN);
    len = readSocket(fd, s_global_buf, GLOBAL_BUF_LEN);
    if(len < 0){
        loge("read socket error! %s", strerror(errno));
        libev_api_send_event_delay(s_inst, connect_server_event_cb, NULL, NULL, 1);
    }else if(len == 0){
        loge("socket closed by server!");
        libev_api_send_event_delay(s_inst, connect_server_event_cb, NULL, NULL, 1);
    }else{
        ret = client->recvData(s_global_buf, len);
        if(ret){
            loge("process data failed!");
            libev_api_send_event_delay(s_inst, connect_server_event_cb, NULL, NULL, 1);
        }
    }
}


static void heartbeat_req_cb(int ret, heartbeatRsp_t * rsp)
{
    logd("heartbeat_req_cb %d, %p", ret, rsp);

    if(ret == EVENT_SUCC){
        logd("get new heartbeat %u, %lf", rsp->pid, rsp->heatbeat);

        if(rsp->heatbeat > 1 && rsp->heatbeat != s_heartbeat_timeout){
            libev_api_restart_timer_with_param(s_heartbeat_timer, rsp->heatbeat, rsp->heatbeat);
            s_heartbeat_timeout = rsp->heatbeat;
        }
    }
}

static void clear_global_data()
{
    if(s_client_fd > 0){
        close(s_client_fd);
        s_client_fd = -1;
    }

    if(s_client_io){
        libev_api_watchio_destroy(&s_client_io);
    }
    if(s_msg_client){
        delete s_msg_client;
        s_msg_client = NULL;
    }

    libev_api_stop_timer(s_heartbeat_timer);
}

static void set_global_data(int fd, ev_io_id io, MsgClient *client)
{
    s_client_fd = fd;
    s_client_io = io;
    s_msg_client = client;
}

static int connect_to_server(ev_inst_st inst, const char *path)
{
    int fd = -1;
    int len = 0;
    int ret = 0;
    struct sockaddr_un sun;
    MsgClient *client = NULL;
    ev_io_id io = NULL;

    clear_global_data();

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0){
        loge("create socket failed! %s", strerror(errno));
        return -1;
    }

    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, path, sizeof(sun.sun_path) - 1);

    len = sizeof(struct sockaddr_un);

    ret = connect(fd, (struct sockaddr *)&sun,sizeof(sun));
    if(ret){
        loge("connect to server %s failed! %s", path, strerror(errno));
        close(fd);
        return ret;
    }

    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);

    client = new MsgClient(inst, fd);
    if(client == NULL){
        loge("new s_msg_client failed!");
        close(fd);
        return -1;
    }
    
    io = libev_api_watchio_create(inst, fd, LIBEV_IO_READ, client_readio_cb, client);
    if(io == NULL){
        loge("create s_client_io failed!");
        delete client;
        client = NULL;
        close(fd);
        return -1;
    }

    set_global_data(fd, io, client);
    client.sendReqHearbeat(getpid(), s_heartbeat_timeout, heartbeat_req_cb);
    libev_api_restart_timer(s_heartbeat_timer);

    return fd;
}


static void connect_server_event_cb(ev_inst_st inst, void *data)
{
    int fd = connect_to_server(inst, TASK_UNIX_PATH);

    if(fd < 0){
        libev_api_send_event_delay(inst, connect_server_event_cb, NULL, NULL, 1);
    }else{
        
    }
}

static void heartbeat_timer_cb(ev_timer_id timer, void *data)
{
    // send heartbeat

    if(s_msg_client == NULL){
        loge("client is null!");
        return;
    }

    s_msg_client.sendReqHearbeat(getpid(), s_heartbeat_timeout, heartbeat_req_cb);
}


static void inst_init_cb(ev_inst_st inst, void *data)
{
    connect_server_event_cb(inst, NULL);
}


static void *thread_start_cb(void *data)
{
    s_inst = libev_api_inst_create(inst_init_cb, NULL);
    if(s_inst == NULL){
        loge("create ev inst failed!");
        return NULL;
    }

    s_heartbeat_timer = libev_api_create_timer(s_inst, heartbeat_timer_cb, NULL, 
                getRandom(DEFAULT_HEARTBEAT_TIMEOUT), DEFAULT_HEARTBEAT_TIMEOUT);
    if(s_heartbeat_timer == NULL){
        loge("create send timer failed!");
        libev_api_inst_destroy(&s_inst);
        return NULL;
    }

    s_heartbeat_timeout = DEFAULT_HEARTBEAT_TIMEOUT;
    libev_api_inst_start(&s_inst, 0);

    return NULL;
}


int task_client_init()
{
    int ret = 0;
    
    int ret = pthread_create(&s_pthread_id, NULL, thread_start_cb, NULL);
    if(ret){
        loge("create pthread failed! %s", strerror(errno));
        return ret;
    }

    return 0;
}

