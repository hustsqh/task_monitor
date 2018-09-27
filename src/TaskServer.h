#ifndef __TASK_SERVER_H__
#define __TASK_SERVER_H__

#include "ev_api_comm.h"

class TaskServer{
    ev_inst_st mInst;
    std::string mUnixPath;
    int mServerFd;

    uint32_t mClientCount;

    ev_io_id mServerIo;
    
    public:
        TaskServer(ev_inst_st inst, std::string unixPath);
        virtual ~TaskServer();
        int startServer();
        int stopServer();
        int restartServer();
};


#endif