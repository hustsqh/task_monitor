#ifndef __TASK_OBJ_H__
#define __TASK_OBJ_H__


#include <stdint.h>
#include <unistd.h>
#include <string>

#include "ev_api_comm.h"

class TaskObj{
    private:
        std::string mName;
        std::string mPath;
        std::string mParam;
        uint32_t mHeartbeat; // unit:s

        pid_t mPid;
        ev_timer_id mTimer;
        task_status_e mStatus;

        ev_inst_st *mInst;
        TaskObjClient *mClient;

        void clearTask();
        void refreshHeartBeat();

    public:
        TaskObj(ev_inst_st inst, std::string name, std::string path, std::string param, uint32_t heartbeat);
        virtual ~TaskObj();
        int startTask();
        int stopTask();
        int restartTask();
        int startTaskClient(int fd);
        int stopTaskClient();
        int refreshTimer();
        std::string getTaskName();
        pid_t getTaskPid();
        task_status_e getTaskStatus();
};



#endif
