#ifndef __TASK_DEBUG_H__
#define __TASK_DEBUG_H__


enum{
    VERBOSE_LEVEL,
    DEBUG_LEVEL,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    FATAL_LEVEL,
};

#include <string.h>
#include <time.h>
static inline const char *getBaseName(const char *str){
    const char *p;
    if(str){
        p = strstr(str, "/");
        if(p)return p+1;
        else return str;
    }else return str;    
}
static inline long long getCurMs(){
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return (long long)time.tv_sec * 1000 + (long long)time.tv_nsec/1000000;
}

#define log(level, ...) \
    do{\
        fprintf(stdout, "[%-10s:%-3d-(%-8lld)]", getBaseName(__FILE__), __LINE__, getCurMs()); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    }while(0)

#define logv(...) log(VERBOSE_LEVEL, __VA_ARGS__)
#define logd(...) log(DEBUG_LEVEL, __VA_ARGS__)
#define logi(...) log(INFO_LEVEL, __VA_ARGS__)
#define logn(...) log(WARN_LEVEL, __VA_ARGS__)
#define logw(...) log(WARN_LEVEL, __VA_ARGS__)
#define loge(...) log(ERROR_LEVEL, __VA_ARGS__)
#define logf(...) log(FATAL_LEVEL, __VA_ARGS__)


#endif
