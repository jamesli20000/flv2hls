
/*
 * Copyright (C) Roman Arutyunyan
 */


#include "flv_mpegts.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static u_char flv_mpegts_header[] = {

    /* TS */
    0x47, 0x40, 0x00, 0x10, 0x00,
    /* PSI */
    0x00, 0xb0, 0x0d, 0x00, 0x01, 0xc1, 0x00, 0x00,
    /* PAT */
    0x00, 0x01, 0xf0, 0x01,
    /* CRC */
    0x2e, 0x70, 0x19, 0x05,
    /* stuffing 167 bytes */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

    /* TS */
    0x47, 0x50, 0x01, 0x10, 0x00,
    /* PSI */
    0x02, 0xb0, 0x17, 0x00, 0x01, 0xc1, 0x00, 0x00,
    /* PMT */
    0xe1, 0x00,
    0xf0, 0x00,
    0x1b, 0xe1, 0x00, 0xf0, 0x00, /* h264 */
    0x0f, 0xe1, 0x01, 0xf0, 0x00, /* aac */
    /*0x03, 0xe1, 0x01, 0xf0, 0x00,*/ /* mp3 */
    /* CRC */
    0x2f, 0x44, 0xb9, 0x9b, /* crc for aac */
    /*0x4e, 0x59, 0x3d, 0x1e,*/ /* crc for mp3 */
    /* stuffing 157 bytes */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};


/* 700 ms PCR delay */
#define FLV_HLS_DELAY  63000
#define flv2hls_memmove(dst, src, n)   (void) memmove(dst, src, n)
#define flv2hls_movemem(dst, src, n)   (((u_char *) memmove(dst, src, n)) + (n))

static int
flv_mpegts_write_file(flv_mpegts_file_t *file, u_char *in,
    size_t in_size)
{

    ssize_t   rc;

    //printf("mpegts: write %uz bytes", in_size);

    rc = write(file->fd, in, in_size);
    if (rc < 0) {
        return ERROR_NORMAL;
    }

    return SUCCESS;
}


static int32_t
flv_mpegts_write_header(flv_mpegts_file_t *file)
{
    return flv_mpegts_write_file(file, flv_mpegts_header,
                                      sizeof(flv_mpegts_header));
}


static u_char *
flv_mpegts_write_pcr(u_char *p, u_int64_t pcr)
{
    *p++ = (u_char) (pcr >> 25);
    *p++ = (u_char) (pcr >> 17);
    *p++ = (u_char) (pcr >> 9);
    *p++ = (u_char) (pcr >> 1);
    *p++ = (u_char) (pcr << 7 | 0x7e);
    *p++ = 0;

    return p;
}


static u_char *
flv_mpegts_write_pts(u_char *p, u_int32_t fb, u_int64_t pts)
{
    u_int32_t val;
    DEBUG("flv_mpegts_write_pts:%lu\n", pts);
    val = fb << 4 | (((pts >> 30) & 0x07) << 1) | 1;
    *p++ = (u_char) val;

    val = (((pts >> 15) & 0x7fff) << 1) | 1;
    *p++ = (u_char) (val >> 8);
    *p++ = (u_char) val;

    val = (((pts) & 0x7fff) << 1) | 1;
    *p++ = (u_char) (val >> 8);
    *p++ = (u_char) val;

    return p;
}

int
flv_mpegts_write_frame(flv_mpegts_file_t *file,
    flv_mpegts_frame_t *f, str_buf_t *b)
{
    u_int32_t  pes_size, header_size, body_size, in_size, stuff_size, flags;
    u_char      packet[188], *p, *base;
    int   first, rc;


    first = 1;

    while (b->pos < b->last) {
        p = packet;

        f->cc++;

        *p++ = 0x47;
        *p++ = (u_char) (f->pid >> 8);

        if (first) {
            p[-1] |= 0x40;
        }

        *p++ = (u_char) f->pid;
        *p++ = 0x10 | (f->cc & 0x0f); /* payload */

        if (first) {

            if (f->key) {
                DEBUG("*******************flv_mpegts_write_frame, write key frame\n");
                packet[3] |= 0x20; /* adaptation */

                *p++ = 7;    /* size */
                *p++ = 0x50; /* random access + PCR */

                p = flv_mpegts_write_pcr(p, f->dts - FLV_HLS_DELAY);
            }

            /* PES header */

            *p++ = 0x00;
            *p++ = 0x00;
            *p++ = 0x01;
            *p++ = (u_char) f->sid;

            header_size = 5;
            flags = 0x80; /* PTS */

            if (f->dts != f->pts) {
                header_size += 5;
                flags |= 0x40; /* DTS */
            }

            pes_size = (b->last - b->pos) + header_size + 3;
            if (pes_size > 0xffff) {
                pes_size = 0;
            }

            *p++ = (u_char) (pes_size >> 8);
            *p++ = (u_char) pes_size;
            *p++ = 0x80; /* H222 */
            *p++ = (u_char) flags;
            *p++ = (u_char) header_size;

            p = flv_mpegts_write_pts(p, flags >> 6, f->pts +
                                                         FLV_HLS_DELAY);

            if (f->dts != f->pts) {
                p = flv_mpegts_write_pts(p, 1, f->dts +
                                                    FLV_HLS_DELAY);
            }

            first = 0;
        }

        body_size = (u_int32_t) (packet + sizeof(packet) - p);
        in_size = (u_int32_t) (b->last - b->pos);

        if (body_size <= in_size) {
            memcpy(p, b->pos, body_size);
            b->pos += body_size;

        } else {
            stuff_size = (body_size - in_size);

            if (packet[3] & 0x20) {

                /* has adaptation */

                base = &packet[5] + packet[4];
                p = (u_int8_t*)memmove((void*)(base + stuff_size), (void*)base, p - base);
                memset(base, 0xff, stuff_size);
                packet[4] += (u_char) stuff_size;

            } else {

                /* no adaptation */

                packet[3] |= 0x20;
                p = (u_int8_t*)memmove((void*)(&packet[4] + stuff_size), (void*)&packet[4],
                                p - &packet[4]);

                packet[4] = (u_char) (stuff_size - 1);
                if (stuff_size >= 2) {
                    packet[5] = 0;
                    memset(&packet[6], 0xff, stuff_size - 2);
                }
            }

            memcpy(p, b->pos, in_size);
            b->pos = b->last;
        }

        rc = flv_mpegts_write_file(file, packet, sizeof(packet));
        if (rc != SUCCESS) {
            return rc;
        }
    }

    return SUCCESS;
}


int32_t
flv_mpegts_open_file(flv_mpegts_file_t *file, char *path)
{
    file->fd = open(path, O_WRONLY|O_CREAT|O_TRUNC);

    if (file->fd == -1) {
        printf("hls: error creating fragment file\n");
        return ERROR_NORMAL;
    }

    file->size = 0;

    if (flv_mpegts_write_header(file) != SUCCESS) {
        printf("hls: error writing fragment header\n");
        close(file->fd);
        return ERROR_NORMAL;
    }
    fchmod(file->fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    
    return SUCCESS;
}


int
flv_mpegts_close_file(flv_mpegts_file_t *file)
{
    u_int8_t   buf[16];
    ssize_t  rc;


    close(file->fd);

    return SUCCESS;
}
