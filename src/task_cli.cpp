#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>


#include "task_msg.h"

static ev_inst_st s_inst;

struct msg_data{
    int is_list;
    int pid;
    char name[32];
    taskActionType_e action;
};

struct msg_data s_data;


static void usage(void *program)
{
    fprintf(stdout, "%s: [param]\n", program);
    fprintf(stdout, "\t-l: list tasks\n");
    fprintf(stdout, "\t-p: task pid\n");
    fprintf(stdout, "\t-n: task name\n");
    fprintf(stdout, "\t-a: action, [start|stop|restart]\n");
    exit(1);
}

static const char *getErrMsgByCode(int code)
{
    switch(code){
        case EVENT_ERROR_FAIL:
            return "error";
        case EVENT_ERROR_TIMEOUT:
            return "timeout";
        case EVENT_ERROR_UNKNOWN_TYPE:
            return "unknown";
        case EVENT_ERROR_SYSTEM:
            return "system";
        case EVENT_ERROR_PARSE:
            return "parse error";    
        case EVENT_ERROR_MEM:
            return "memory error";
        case EVENT_ERROR_UNPROCESS:
            return "unsupport";
        case EVENT_ERROR_NOT_FOUND:
            return "not found";
        default:
            return "unknown";    
    }
}

static void allTaskCb(int ret, taskAllRsp_t *rsp)
{
    int i = 0;
    
    if(ret != EVENT_SUCC){
        fprintf(stdout, "command error! %s !\n", getErrMsgByCode(ret));
        exit(1);
    }

    fprintf(stdout, "%s\t\t%s\t\t%s\n", "name", "
pid", "status");
    for(i = 0; i < rsp->count; i++){
        fprintf(stdout, "%s\t\t%u\t\t%s\n", rsp->taskList[i].name, rsp->taskList[i].pid, rsp->taskList[i].status);
    }
    exit(0);
}

static void singleTaskCb(int ret, taskDetailInfo_t *rsp)
{
    if(ret != EVENT_SUCC){
        fprintf(stdout, "command error! %s !\n", getErrMsgByCode(ret));
        exit(1);
    }

    fprintf(stdout, "
%s\t:%u\n", "pid", rsp->pid);
    fprintf(stdout, "
%s\t:%s\n", "name", rsp->name);
    fprintf(stdout, "
%s\t:%s\n", "param", rsp->param);
    fprintf(stdout, "
%s\t:%s\n", "status", rsp->status);
    fprintf(stdout, "
%s\t:%lf\n", "heartbeat", rsp->heartbeat);

    exit(0);
}

static void taskActionCb(int ret, void *data)
{
    if(ret != EVENT_SUCC){
        fprintf(stdout, "command error! %s !\n", getErrMsgByCode(ret));
        exit(1);
    }
    exit(0);
}

static void cli_readio_cb(ev_io_id io, void *data)
{
    MsgClient *client = (MsgClient *)data;
    ssize_t len = 0;
    int ret = 0;
    int fd = libev_api_watchio_get_fd(io);
    char buf[1024] = {0};

    if(!data){
        return;
    }

    memset(buf, 0, 1024);
    while(1){
        len = readSocket(fd, s_global_buf, GLOBAL_BUF_LEN);
        if(len > 0){
            ret = client->recvData(s_global_buf, len);
            break;
        }else if(len == 0){
            loge("socket closed by server!");
            exit(1);
            break;
        }else if(len < 0 && errno == EINTR){
            continue;
        }else{
            loge("read socket error! %s", strerror(errno));
            exit(1);
        }
    }
}


static MsgClient * connect_server(ev_inst_st inst, const char *path)
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
        return NULL;
    }

    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, path, sizeof(sun.sun_path) - 1);

    len = sizeof(struct sockaddr_un);

    ret = connect(fd, (struct sockaddr *)&sun,sizeof(sun));
    if(ret){
        loge("connect to server %s failed! %s", path, strerror(errno));
        close(fd);
        return NULL;
    }

    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);

    client = new MsgClient(inst, fd);
    if(client == NULL){
        loge("new s_msg_client failed!");
        close(fd);
        return NULL;
    }
    
    io = libev_api_watchio_create(inst, fd, LIBEV_IO_READ, cli_readio_cb, client);
    if(io == NULL){
        loge("create s_client_io failed!");
        delete client;
        client = NULL;
        close(fd);
        return NULL;
    }

    return client;
}


static void inst_init_cb(ev_inst_st inst, void *data)
{
    MsgClient *client = NULL;
    struct msg_data *msgData = (struct msg_data *)data;

    client = connect_server(inst, TASK_UNIX_PATH);
    if(client == NULL){
        loge("connect to server failed!");
        exit(1);
    }

    if(msgData->is_list){
        if(msgData->pid || strlen(msgData->name)){
            client->sendReqQuerySingleTask(msgData->pid, msgData->name, singleTaskCb);
        }else{
            client->sendReqQueryAllTask(allTaskCb);
        }
    }else{
        client->sendReqTaskAction(msgData->action, msgData->action);
    }
}



int main(int argc, char **argv)
{
    MsgClient *client = NULL;
    int ch;
    int list = 0;
    int pid = 0;
    char name[32] = {0};
    int action = 0;
    char *program = argv[0];
    

    while((ch = getopt(argc, argv, "lp:n:a:"))){
        switch(ch){
            case 'l':
                list = 1;
                break;
            case 'p':
                pid = atoi(optarg);
                if(pid == 0){
                    usage(program);
                }
                break;
            case 'n':
                strncpy(name, optarg, sizeof(name) - 1);
                break;
            case 'a':
                if(strcmp(optarg, "start") == 0){
                    action = TASK_ACTION_START;
                }else if(strcmp(optarg, "stop") == 0){
                    action = TASK_ACTION_STOP;
                }else if(strcmp(optarg, "restart") == 0){
                    action = TASK_ACTION_RESTART;
                }else{
                    usage();
                }
                
                break;
        }
    }

    memset(&s_data, 0, sizeof(s_data));
    s_data.is_list = list;
    s_data.pid = pid;
    strncpy(s_data.name, name, sizeof(s_data.name) - 1);
    s_data.action = action;
    
    s_inst = libev_api_inst_create(inst_init_cb, &s_data);
    if(s_inst == NULL){
        loge("create ev inst failed!");
        return NULL;
    }

    libev_api_inst_start(&s_inst, 0);

    return 0;
}
