#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <stdint.h>
#include <time.h>


uint32_t getRandom(uint32_t max)
{
    int rand = 0;
    
    srandom(time());

    rand = random();

    rand = (rand * max) / RAND_MAX;

    return rand;    
}


int readSocket(int fd, void *buf, uint32_t len)
{
    int ret = 0;

    ret = read(fd, buf, len);
    if(ret < 0 && (errno == EAGAIN || errno == EINTR)){
        continue;
    }

    return ret;
}

