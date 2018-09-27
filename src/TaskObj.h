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

        int mClientFd;
        pid_t mPid;
        ev_io_id mReadIo;
        ev_timer_id mTimer;
        task_status_e mStatus;

        ev_inst_st *mInst;

        void clearTask();

    public:
        enum task_status_e{
            TASK_STATUS_U = 0, // UNKNOWN
            TASK_STATUS_R,      // RUNNING
            TASK_STATUS_S,      // SLEEPING
            TASK_STATUS_D,      // UNINTERRUPT RUNNING
            TASK_STATUS_T,      // TRACED OR STOPPED
            TASK_STATUS_Z,      // ZOMBIE
            TASK_STATUS_MAX
        };

        TaskObj(std::string path, std::string param, uint32_t heartbeat);
        virtual ~TaskObj();
        int startTask();
        int stopTask();
        int restartTask();
        int startTaskClient(int fd);
        int stopTaskClient();
        int refreshTimer();
};



#endif
