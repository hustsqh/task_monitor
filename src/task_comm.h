#ifndef __TASK_COMM_H__
#define __TASK_COMM_H__

#define TASK_UNIX_PATH "task_unix_path"

#define TAKS_CONFIG_PATH "/etc/task_config.json"

#define DEFAULT_HEARTBEAT_TIMEOUT 5 // unit:s

enum task_status_e{
    TASK_STATUS_U = 0, // UNKNOWN
    TASK_STATUS_R,      // RUNNING
    TASK_STATUS_S,      // SLEEPING
    TASK_STATUS_D,      // UNINTERRUPT RUNNING
    TASK_STATUS_T,      // TRACED OR STOPPED
    TASK_STATUS_Z,      // ZOMBIE
    TASK_STATUS_MAX
};

#endif