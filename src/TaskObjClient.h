#ifndef __TASK_OBJ_CLIENT_H__
#define __TASK_OBJ_CLIENT_H__

#include <stdint.h>
#include <unistd.h>
#include <string>

#include "ev_api_comm.h"


class TaskObjClient{
    private:
        int mClientFd;
        pid_t mPid;
        ev_io_id mReadIo;

        ev_inst_st *mInst;
        MsgServer *msgServer;

        int isAvailable;

        int startTaskClient(int fd);
        int stopTaskClient();
        int recvData(char *buf, int len);

    public:
        TaskObjClient(ev_inst_st inst, int fd, int pid);
        virtual ~TaskObjClient();
        int isClientAvailable();
};

#endif
