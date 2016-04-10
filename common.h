#ifndef FLV_2HLS_COMMON_H
#define FLV_2HLS_COMMON_H

#include <vector>
#include <sys/types.h>
#include <string>
#include <stdio.h>
#include <string>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>



#define verbose 1

#define DEBUG(msg,...) if(verbose) fprintf(stderr, msg, ##__VA_ARGS__);
#define ERROR(msg,...) fprintf(stderr, msg, ##__VA_ARGS__);

//typedef unsigned long long u_int64_t;
//typedef long long int64_t;
typedef unsigned int u_int32_t;
typedef int int32_t;
typedef unsigned char u_int8_t;
//typedef char int8_t;
typedef unsigned short u_int16_t;
typedef short int16_t;
//typedef int64_t ssize_t;
typedef u_int8_t u_char;

#define MAX_FRAME_SIZE 1024*1024



typedef struct{
    u_int8_t* pos;
    u_int8_t* end;
    u_int8_t* last;
    u_int8_t* start;
    u_int8_t buf[MAX_FRAME_SIZE];
}str_buf_t;


#endif
