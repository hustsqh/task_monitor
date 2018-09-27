#ifndef __TASK_CONFIG_H__
#define __TASK_CONFIG_H__

#include <stdint.h>

#include "list.h"

struct taskParm_t{
    struct list_head node;
    char name[32];
    char path[64];
    char param[128];
    double heartbeat;
};

struct taskConfig_t{
    uint32_t maxTasks;
    double queryIntvl;
    struct list_head list;
};


#endif