#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>


#include "ev_api_comm.h"
#include "task_debug.h"
#include "task_comm.h"


static ev_inst_st s_inst;


static void inst_init_cb(ev_inst_st inst, void * data)
{
    task_core_init(inst);
}


int main(int argc, char **argv)
{
    int ret = 0;

    ret = task_config_init(TAKS_CONFIG_PATH);
    if(ret){
        return;
    }

    s_inst = libev_api_inst_create(inst_init_cb, NULL);
    if(!s_inst){
        loge("create inst failed!");
        return -1;
    }

    libev_api_inst_start(&s_inst, 0);

    return 0;
}

