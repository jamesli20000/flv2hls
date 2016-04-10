
/*
 * Copyright (C) Roman Arutyunyan
 */


#ifndef _NGX_RTMP_MPEGTS_H_INCLUDED_
#define _NGX_RTMP_MPEGTS_H_INCLUDED_

#include "FlvDecoder.h"


typedef struct {
    int    fd;
    unsigned    size:4;
} flv_mpegts_file_t;


typedef struct {
    u_int64_t    pts;
    u_int64_t    dts;
    u_int32_t  pid;
    u_int32_t  sid;
    u_int32_t  cc;
    unsigned    key:1;
} flv_mpegts_frame_t;


int flv_mpegts_open_file(flv_mpegts_file_t *file, char *path);
int flv_mpegts_close_file(flv_mpegts_file_t *file);
int flv_mpegts_write_frame(flv_mpegts_file_t *file, flv_mpegts_frame_t *f, str_buf_t *b);


#endif /* _NGX_RTMP_MPEGTS_H_INCLUDED_ */
