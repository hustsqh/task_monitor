#ifndef __TASK_MSG_H__
#define __TASK_MSG_H__

#include "list.h"
#include "ring_buf.h"

enum taskActionType_e{
    TASK_ACTION_NONE,
    TASK_ACTION_START,
    TASK_ACTION_STOP,
    TASK_ACTION_RESTART,
    TASK_ACTION_MAX
};

enum event_type_e{
    EVENT_TYPE_HEARTBEAT,
    EVENT_TYPE_QUERY_ALL,
    EVENT_TYPE_QUERY_TASK,
    EVENT_TYPE_TASK_ACTION,
    EVENT_TYPE_MAX
};


enum event_err_code_e{
    EVENT_SUCC,
    EVENT_ERROR_FAIL,
    EVENT_ERROR_TIMEOUT,
    EVENT_ERROR_UNKNOWN_TYPE,
    EVENT_ERROR_SYSTEM,
    EVENT_ERROR_PARSE,
    EVENT_ERROR_MEM,
    EVENT_ERROR_UNPROCESS,
    EVENT_ERROR_NOT_FOUND,
    EVENT_ERROR_MAX,
};

typedef event_err_code_e (*processEventCb)(void *data, void **reply, size_t *replySize, void *usrData);

struct heartbeatReq_t{
    uint32_t pid;
    double heartbeat;
};

struct heartbeatRsp_t{
    uint32_t pid;
    double heatbeat;
};

struct taskSimpleInfo_t{
    char name[32];
    uint32_t pid;
    char status[32];
};

struct taskAllRsp_t{
    uint32_t count;
    taskSimpleInfo_t taskList[0];
};

/**
 * 优先使用pid查找任务，如果pid为空，则使用name查询
 */
struct taskSingleReq_t{
    uint32_t pid;
    char name[32];
};

struct taskDetailInfo_t{
    uint32_t pid;
    char name[32];
    char param[128];
    char status[32];
    double heartbeat;
};

/**
 * 优先使用pid查找任务，如果pid为空，则使用name查询
 */
struct taskActoinReq_t{
    uint32_t pid;
    char name[32];
    taskActionType_e action;
};


struct msgRequest_t{
    struct list_head node;
    uint32_t reqId;
    event_type_e type;
    void (*cb)(int ret, void *data);
    double timeStart;
    double timeout;
    ev_timer_id timer;
    MsgClient *client;
};


class MsgServer{
    private:
        ring_buf_t *mRb;
        int mClientFd;
        processEventCb mEventCb[EVENT_TYPE_MAX];
        void *mUserData[EVENT_TYPE_MAX];

        int createFailMsg(uint32_t errcode, char **replyData, uint32_t *replySize);
        event_err_code_e processHeartbeatReq(uint32_t reqId, cJSON *data, char **replyData, uint32_t *replySize);
        event_err_code_e processQueryAllReq(uint32_t reqId, cJSON *data, char **replyData, uint32_t *replySize);
        event_err_code_e processQuerySingleTaskReq(uint32_t reqId, cJSON *data, char **replyData, uint32_t *replySize);
        event_err_code_e processTaskActionReq(uint32_t reqId, cJSON *data, char **replyData, uint32_t *replySize);
        int parseServerData(uint32_t reqId, char *data, uint32_t len);
        int procData();        

    public:
        MsgServer(int fd);
        virtual ~MsgServer();
        int registerEventCb(event_type_e type, processEventCb cb);
        int recvData(char *data, size_t len); 
};


class MsgClient{
    private:
        ev_inst_st mInst;
        int mClientFd;
        ring_buf_t *mRb;
        list_head mRequestList;
        uint32_t mCurReqId;
        static const double mDefaultReqTimeout = 3;

        uint32_t getNextReqId();
        msgRequest_t * reqNodeCreate(uint32_t reqId, double timeout);
        void destroyReqNode(msgRequest_t *req);
        int sendRequest(char *data, uint32_t len, void (*cb)(int ret, void *data), double timeout);
        int processHeartbeatRsp(msgRequest_t * req, int ret, cJSON *data);
        int processQueryAllRsp(msgRequest_t * req, int ret, cJSON *data);
        int processQueryTaskRsp(msgRequest_t * req, int ret, cJSON *data);
        int processTaskActionRsp(msgRequest_t * req, int ret, cJSON *data);
        int processRspData(msgRequest_t * req, int ret, cJSON *data);
        int parseClientData(uint32_t reqId, char *data, uint32_t len);
        int procData();

    public:
        MsgClient(ev_inst_st inst, int fd);
        virtual ~MsgClient();
        int recvData(char *data, size_t len); 
        int sendReqHearbeat(pid_t pid, double heartbeat, void (*cb)(int ret, heartbeatRsp_t *rsp));
        int sendReqQueryAllTask(void (*cb)(int ret, taskAllRsp_t *rsp));
        int sendReqQuerySingleTask(int pid, char *name, void (*cb)(int ret, taskDetailInfo_t *rsp));
        int sendReqTaskAction(taskActionType_e action, void (*cb)(int ret, void *data));
};


#endif