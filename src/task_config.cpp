#include <stdio.h>
#include <stdlib.h>
#include <cstring.h>
#include <errno.h>
#include <stdint.h>

#include "cjson/cJSON.h"


/*

json格式如下：
{
	“maxTasks”：1000，
	“queryInterval”：2,
	“tasks”：[
		{“name”:”taskA”, “path”:” /usr/sbin/xxx”, “param”:” -a xx –b xx”, “heartbeat”:10},
		{“name”:”taskB”, “path”:” /usr/sbin/xxx”, “param”:” -a xx –b xx”, “heartbeat”:10},
	]
}
*/


static taskConfig_t s_global_config;

static int parse_config_file(char *buf)
{
    
}


int task_config_init(const char *filename)
{
    memset(&s_global_config, 0, sizeof(s_global_config));

    
}

