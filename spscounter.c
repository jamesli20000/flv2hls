#include <stdio.h>
#include "FlvDecoder.h"
#include "flv_mpegts.h"
#include "common.h"


#define NGX_RTMP_MSG_AUDIO              8
#define NGX_RTMP_MSG_VIDEO              9
#define NGX_RTMP_MSG_AMF3_META          15
#define NGX_RTMP_MSG_AMF3_SHARED        16
#define NGX_RTMP_MSG_AMF3_CMD           17
#define NGX_RTMP_MSG_AMF_META           18
#define NGX_RTMP_MSG_AMF_SHARED         19

#define FLV_HLS_BUFSIZE            (1024*1024)
#define FLV_HLS_DIR_ACCESS         0744
int g_winfrages = 6;
int g_fraglen = 3000;
int g_max_fraglen = 5000;

typedef struct {
    u_int64_t                            id;
    u_int64_t                            key_id;
    double                              duration;
    unsigned                            active:1;
    unsigned                            discont:1; /* before */
} hls_frag_t;

typedef struct {
    unsigned                            opened:1;

    flv_mpegts_file_t                   file;

    std::string                         playlist;
    std::string                         playlist_bak;

    char                                stream[1024];
    int                                   stream_len;


    u_int64_t                            frag;
    u_int64_t                            frag_ts;
    u_int32_t                          nfrags;
    hls_frag_t                          *frags; /* circular 2 * winfrags + 1 */
    u_int32_t                          winfrags;
    u_int32_t                          max_fraglen;
    u_int32_t                          fraglen;

    u_int32_t                          audio_cc;
    u_int32_t                          video_cc;
    u_int32_t                          key_frags;

    u_int64_t                            aframe_base;
    u_int64_t                            aframe_num;

    str_buf_t                            *aframe;
    u_int64_t                            aframe_pts;
    int                                bStart;
    
    int                          max_audio_delay;
    u_int32_t                       sync;
} hls_ctx_t;



typedef struct Flv2hlsContext
{
    FlvFileReader flvreader;
    FlvDecoder flvdec;
    hls_ctx_t   hls_ctx;
    av_codec_ctx_t  codec;
}Flv2hlsContext_t;

static hls_frag_t *
hls_get_frag(hls_ctx_t  *ctx, int n)
{
    return &ctx->frags[(ctx->frag + n) % (ctx->winfrags * 2 + 1)];
}


Flv2hlsContext* flv_open_read(const char* file)
{
    Flv2hlsContext* flv = new Flv2hlsContext();
    
    if (flv->flvreader.open(file) != SUCCESS) {
        flv_freep(flv);
        return NULL;
    }
    
    if (flv->flvdec.initialize(&flv->flvreader) != SUCCESS) {
        flv_freep(flv);
        return NULL;
    }
    
    return flv;
}

void flv_close(Flv2hlsContext* flv)
{
    Flv2hlsContext* context = flv;
    flv_freep(context);
}

int flv_read_header(Flv2hlsContext* flv, char (*header)[9], u_int32_t*pos)
{
    int ret = SUCCESS;
    
    Flv2hlsContext* context = (Flv2hlsContext*)flv;

    if (!context->flvreader.is_open()) {
        return ERROR_SYSTEM_IO_INVALID;
    }
    
    if ((ret = context->flvdec.read_header(*header)) != SUCCESS) {
        return ret;
    }
    
    char ts[4]; // tag size
    if ((ret = context->flvdec.read_previous_tag_size(ts)) != SUCCESS) {
        return ret;
    }
    *pos = context->flvdec.getPosition();
    return ret;
}

int flv_read_tag_header(Flv2hlsContext* context, char* ptype, u_int32_t *pdata_size, u_int32_t* ptime)
{
    int ret = SUCCESS;
    

    if (!context->flvreader.is_open()) {
        return ERROR_SYSTEM_IO_INVALID;
    }
    
    if ((ret = context->flvdec.read_tag_header(ptype, pdata_size, ptime)) != SUCCESS) {
        return ret;
    }
    
    return ret;
}

int flv_read_tag_data(Flv2hlsContext* context, char**data, u_int32_t size, char type)
{
    int ret = SUCCESS;
    

    if (!context->flvreader.is_open()) {
        return ERROR_SYSTEM_IO_INVALID;
    }
    
    if ((ret = context->flvdec.read_tag_data(data, size, type)) != SUCCESS) {
        return ret;
    }
    
    char ts[4]; // tag size
    if ((ret = context->flvdec.read_previous_tag_size(ts)) != SUCCESS) {
        return ret;
    }
    
    return ret;
}

int hls_init_playlist(hls_ctx_t*ctx, char*hls_path)
{
    int ret = SUCCESS;
    ctx->playlist = std::string(hls_path) + ".m3u8";
    ctx->playlist_bak = ctx->playlist + ".bak";
    return ret;
}


int flv_hls_copy(void *dst, u_int8_t**src, int n)
{
    u_char  *last;
    size_t   pn;

    if (src == NULL || dst == NULL) {
        return -1;
    }

    memcpy(dst, *src, n);
    *src += n;
    return SUCCESS;
}


static int
hls_append_aud(str_buf_t * out)
{
    static u_char   aud_nal[] = { 0x00, 0x00, 0x00, 0x01, 0x09, 0xf0 };

    if (out->last + sizeof(aud_nal) > out->end) {
        return ERROR_NORMAL;
    }

    memcpy(out->last, aud_nal, sizeof(aud_nal));
    out->last += sizeof(aud_nal);
    return SUCCESS;
}

static int
hls_append_sps_pps(av_codec_ctx_t*codec_ctx, str_buf_t*out)
{
    u_int8_t                        *p;
    u_int8_t                        *in;
    hls_ctx_t             *ctx;
    int8_t                          nnals;
    u_int16_t                        len, rlen;
    int                       n;
    u_int8_t                        dummy[FLV_HLS_BUFSIZE];
    in = codec_ctx->avc_header;

    p = in;

    /*
     * Skip bytes:
     * - flv fmt
     * - H264 CONF/PICT (0x00)
     * - 0
     * - 0
     * - 0
     * - version
     * - profile
     * - compatibility
     * - level
     * - nal bytes
     */
    DEBUG("enter hls_append_sps_pps, buf size:%d\n", out->end-out->last);
    if (flv_hls_copy(dummy, &p, 10) != SUCCESS) {
        return SUCCESS;
    }

    /* number of SPS NALs */
    if (flv_hls_copy(&nnals, &p, 1) != SUCCESS) {
        return SUCCESS;
    }

    nnals &= 0x1f; /* 5lsb */

    DEBUG("hls: SPS number: %u\n", nnals);

    /* SPS */
    for (n = 0; ; ++n) {
        for (; nnals; --nnals) {

            /* NAL length */
            if (flv_hls_copy(&rlen, &p, 2) != SUCCESS) {
                return SUCCESS;
            }

            flv_rmemcpy(&len, &rlen, 2);

            printf("hls: header NAL length: %u, buf size:%d\n", (size_t) len, out->end-out->last);

            /* AnnexB prefix */
            if (out->end - out->last < 4) {
                printf( "hls: too small buffer for header NAL size\n");
                return ERROR_NORMAL;
            }

            *out->last++ = 0;
            *out->last++ = 0;
            *out->last++ = 0;
            *out->last++ = 1;

            /* NAL body */
            if (out->end - out->last < len) {
                printf("hls: too small buffer for header NAL\n");
                return ERROR_NORMAL;
            }
            
            if (flv_hls_copy(out->last, &p, len) != SUCCESS) {
                return SUCCESS;
            }
            out->last += len;

        }

        if (n == 1) {
            break;
        }

        /* number of PPS NALs */
        if (flv_hls_copy(&nnals, &p, 1) != SUCCESS) {
            return SUCCESS;
        }
        DEBUG("PPS nnals:%d\n", nnals);
    }

    return SUCCESS;
}

static void
hls_next_frag(hls_ctx_t *ctx)
{

    if (ctx->nfrags == ctx->winfrags) {
        ctx->frag++;
    } else {
        ctx->nfrags++;
    }
}

static int
hls_rename_file(const char *src, const char *dst)
{
    /* rename file with overwrite */


    rename(src, dst);
   return SUCCESS;
}


static int
hls_write_playlist(hls_ctx_t *ctx)
{
    static u_char                   buffer[1024];
    int                                 fd;
    u_char                         *p, *end;
    ssize_t                         n;

    hls_frag_t            *f;
    u_int32_t                      i, max_frag;


    fd = open(ctx->playlist_bak.c_str(), O_WRONLY|O_CREAT|O_TRUNC);

    if (fd == -1) {
        printf("hls: open file failed: '%s'\n",
                      ctx->playlist_bak.c_str());
        return ERROR_NORMAL;
    }
    fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    max_frag = ctx->fraglen / 1000;

    for (i = 0; i < ctx->nfrags; i++) {
        f = hls_get_frag(ctx, i);
        if (f->duration > max_frag) {
            max_frag = (u_int32_t) (f->duration + .5);
        }
    }

    p = buffer;
    end = p + sizeof(buffer);

    p += sprintf((char*)p, "#EXTM3U\n"
                     "#EXT-X-VERSION:3\n"
                     "#EXT-X-MEDIA-SEQUENCE:%u\n"
                     "#EXT-X-TARGETDURATION:%u\n",
                     ctx->frag, max_frag);

    printf("will write buf:%s, size:%d", buffer, p-buffer);
    n = write(fd, buffer, p - buffer);
    if (n <= 0) {
        printf("hls:write failed: '%s'\n",
                      ctx->playlist_bak.c_str());
        close(fd);
        return ERROR_NORMAL;
    }


    for (i = 0; i < ctx->nfrags; i++) {
        f = hls_get_frag(ctx, i);

        p = buffer;
        end = p + sizeof(buffer);

        if (f->discont) {
            p += sprintf((char*)p, "#EXT-X-DISCONTINUITY\n");
        }


        p += sprintf((char*)p, "#EXTINF:%.3f,\n"
                         "%u.ts\n",
                         f->duration, f->id);


        n = write(fd, buffer, p - buffer);
        if (n <= 0) {
            printf("hls: write failed '%V'",
                          ctx->playlist_bak.c_str());
            close(fd);
            return ERROR_NORMAL;
        }
    }

    close(fd);

    hls_rename_file(ctx->playlist_bak.c_str(), ctx->playlist.c_str());

    return SUCCESS;
}


static int
hls_close_fragment(hls_ctx_t *ctx, u_int32_t ts)
{
    DEBUG("hls_close_fragment frag:%d, nfrags:%d\n", ctx->frag, ctx->nfrags);
    if( ctx->frag != 0 || ctx->nfrags != 0 )
    {
        close(ctx->file.fd);
        ctx->opened = 0;
    }else{
    /*
        if( ctx->aframe )
        {
            DEBUG("*********clean aframe***************\n");
            memset(ctx->aframe->buf, 0, sizeof(MAX_FRAME_SIZE));
            ctx->aframe->last = ctx->aframe->pos = ctx->aframe->start = ctx->aframe->buf;
            ctx->aframe_pts = 0;
        }
        */
        ctx->bStart = 1;
    }
    hls_next_frag(ctx);

    if( ctx->frag != 0 || ctx->nfrags != 1)
    {
        hls_write_playlist(ctx);
    }
    return SUCCESS;
}

static u_int64_t
flv_hls_get_fragment_id(hls_ctx_t*ctx)
{
    return ctx->frag + ctx->nfrags;    
}








static int
hls_parse_aac_header(av_codec_ctx_t   *codec_ctx, u_int32_t *objtype,
    u_int32_t *srindex, u_int32_t *chconf)
{
    u_int8_t            *cl;
    u_char             p[2];
    u_char              b0, b1;
    DEBUG("enter hls_parse_aac_header\n");
    cl = codec_ctx->aac_header;

    if (flv_hls_copy(p, &cl, 2) != SUCCESS) {
        return ERROR_NORMAL;
    }

    if (flv_hls_copy(&b0, &cl, 1) != SUCCESS) {
        return ERROR_NORMAL;
    }

    if (flv_hls_copy(&b1, &cl, 1) != SUCCESS) {
        return ERROR_NORMAL;
    }

    *objtype = b0 >> 3;
    if (*objtype == 0 || *objtype == 0x1f) {
        ERROR( "error: hls_parse_aac_header unsupported adts object type:%u\n", *objtype);
        return ERROR_NORMAL;
    }

    if (*objtype > 4) {

        /*
         * Mark all extended profiles as LC
         * to make Android as happy as possible.
         */

        *objtype = 2;
    }

    *srindex = ((b0 << 1) & 0x0f) | ((b1 & 0x80) >> 7);
    if (*srindex == 0x0f) {
        ERROR( "error: hls_parse_aac_header unsupported adts sample rate:%u\n", *srindex);
        return ERROR_NORMAL;
    }

    *chconf = (b1 >> 3) & 0x0f;

    DEBUG( "hls: aac object_type:%u, sample_rate_index:%u, "
                   "channel_config:%u\n", *objtype, *srindex, *chconf);

    return SUCCESS;
}


static void
av_codec_parse_aac_header(Flv2hlsContext*context, u_int8_t*data, int data_len)
{
    u_int32_t               idx;
    av_codec_ctx_t          *ctx = &context->codec;
    stream_bit_reader_t     br;
    static u_int32_t    aac_sample_rates[] =
        { 96000, 88200, 64000, 48000,
          44100, 32000, 24000, 22050,
          16000, 12000, 11025,  8000,
           7350,     0,     0,     0 };

    stream_bit_init_reader(&br, data, data+data_len);

    stream_bit_read(&br, 16);

    ctx->aac_profile = (u_int32_t) stream_bit_read(&br, 5);
    if (ctx->aac_profile == 31) {
        ctx->aac_profile = (u_int32_t) stream_bit_read(&br, 6) + 32;
    }

    idx = (u_int32_t) stream_bit_read(&br, 4);
    if (idx == 15) {
        ctx->sample_rate = (u_int32_t) stream_bit_read(&br, 24);
    } else {
        ctx->sample_rate = aac_sample_rates[idx];
    }

    ctx->aac_chan_conf = (u_int32_t) stream_bit_read(&br, 4);

    if (ctx->aac_profile == 5 || ctx->aac_profile == 29) {
        
        if (ctx->aac_profile == 29) {
            ctx->aac_ps = 1;
        }

        ctx->aac_sbr = 1;

        idx = (u_int32_t) stream_bit_read(&br, 4);
        if (idx == 15) {
            ctx->sample_rate = (u_int32_t) stream_bit_read(&br, 24);
        } else {
            ctx->sample_rate = aac_sample_rates[idx];
        }

        ctx->aac_profile = (u_int32_t) stream_bit_read(&br, 5);
        if (ctx->aac_profile == 31) {
            ctx->aac_profile = (u_int32_t) stream_bit_read(&br, 6) + 32;
        }
    }

    /* MPEG-4 Audio Specific Config

       5 bits: object type
       if (object type == 31)
         6 bits + 32: object type
       4 bits: frequency index
       if (frequency index == 15)
         24 bits: frequency
       4 bits: channel configuration

       if (object_type == 5)
           4 bits: frequency index
           if (frequency index == 15)
             24 bits: frequency
           5 bits: object type
           if (object type == 31)
             6 bits + 32: object type
             
       var bits: AOT Specific Config
     */

    DEBUG("codec: aac header profile=%u, "
           "sample_rate=%u, chan_conf=%u\n",
           ctx->aac_profile, ctx->sample_rate, ctx->aac_chan_conf);
}


static void
av_codec_parse_avc_header(Flv2hlsContext*context, u_int8_t*data, int data_len)
{
    u_int32_t               profile_idc, width, height, crop_left, crop_right,
                            crop_top, crop_bottom, frame_mbs_only, n, cf_idc,
                            num_ref_frames;
    av_codec_ctx_t   *ctx = &context->codec;
    stream_bit_reader_t   br;
    u_int8_t                *p = data;
    u_int8_t                tm = 0;
    stream_bit_init_reader(&br, data, data+data_len);

    stream_bit_read(&br, 48);

    ctx->avc_profile = (u_int32_t) stream_bit_read_8(&br);
    ctx->avc_compat = (u_int32_t) stream_bit_read_8(&br);
    ctx->avc_level = (u_int32_t) stream_bit_read_8(&br);

    /* nal bytes */
    ctx->avc_nal_bytes = (u_int32_t) ((stream_bit_read_8(&br) & 0x03) + 1);

    /* nnals */
    if ((stream_bit_read_8(&br) & 0x1f) == 0) {
        return;
    }

    /* nal size */
    stream_bit_read(&br, 16);

    /* nal type */
    if ( (tm = stream_bit_read_8(&br)) != 0x67 && tm != 0x68) {
        return;
    }
    if(tm == 0x67){
      DEBUG("we have sps\n");
    }else if(tm == 0x68)
    {
      DEBUG("we have pps\n");
      return;
    }
    /* SPS */

    /* profile idc */
    profile_idc = (u_int32_t) stream_bit_read(&br, 8);

    /* flags */
    stream_bit_read(&br, 8);

    /* level idc */
    stream_bit_read(&br, 8);

    /* SPS id */
    stream_bit_read_golomb(&br);

    if (profile_idc == 100 || profile_idc == 110 ||
        profile_idc == 122 || profile_idc == 244 || profile_idc == 44 ||
        profile_idc == 83 || profile_idc == 86 || profile_idc == 118)
    {
        /* chroma format idc */
        cf_idc = (u_int32_t) stream_bit_read_golomb(&br);
        
        if (cf_idc == 3) {

            /* separate color plane */
            stream_bit_read(&br, 1);
        }

        /* bit depth luma - 8 */
        stream_bit_read_golomb(&br);

        /* bit depth chroma - 8 */
        stream_bit_read_golomb(&br);

        /* qpprime y zero transform bypass */
        stream_bit_read(&br, 1);

        /* seq scaling matrix present */
        if (stream_bit_read(&br, 1)) {

            for (n = 0; n < (cf_idc != 3 ? 8u : 12u); n++) {

                /* seq scaling list present */
                if (stream_bit_read(&br, 1)) {

                    /* TODO: scaling_list()
                    if (n < 6) {
                    } else {
                    }
                    */
                }
            }
        }
    }

    /* log2 max frame num */
    stream_bit_read_golomb(&br);

    /* pic order cnt type */
    switch (stream_bit_read_golomb(&br)) {
    case 0:

        /* max pic order cnt */
        stream_bit_read_golomb(&br);
        break;

    case 1:

        /* delta pic order alwys zero */
        stream_bit_read(&br, 1);

        /* offset for non-ref pic */
        stream_bit_read_golomb(&br);

        /* offset for top to bottom field */
        stream_bit_read_golomb(&br);

        /* num ref frames in pic order */
        num_ref_frames = (u_int32_t) stream_bit_read_golomb(&br);

        for (n = 0; n < num_ref_frames; n++) {

            /* offset for ref frame */
            stream_bit_read_golomb(&br);
        }
    }

    /* num ref frames */
    ctx->avc_ref_frames = (u_int32_t) stream_bit_read_golomb(&br);

    /* gaps in frame num allowed */
    stream_bit_read(&br, 1);

    /* pic width in mbs - 1 */
    width = (u_int32_t) stream_bit_read_golomb(&br);

    /* pic height in map units - 1 */
    height = (u_int32_t) stream_bit_read_golomb(&br);

    /* frame mbs only flag */
    frame_mbs_only = (u_int32_t) stream_bit_read(&br, 1);

    if (!frame_mbs_only) {

        /* mbs adaprive frame field */
        stream_bit_read(&br, 1);
    }

    /* direct 8x8 inference flag */
    stream_bit_read(&br, 1);

    /* frame cropping */
    if (stream_bit_read(&br, 1)) {

        crop_left = (u_int32_t) stream_bit_read_golomb(&br);
        crop_right = (u_int32_t) stream_bit_read_golomb(&br);
        crop_top = (u_int32_t) stream_bit_read_golomb(&br);
        crop_bottom = (u_int32_t) stream_bit_read_golomb(&br);

    } else {

        crop_left = 0;
        crop_right = 0;
        crop_top = 0;
        crop_bottom = 0;
    }

    ctx->width = (width + 1) * 16 - (crop_left + crop_right) * 2;
    ctx->height = (2 - frame_mbs_only) * (height + 1) * 16 -
                  (crop_top + crop_bottom) * 2;
    printf("profile:%d,compat:%d,level:%d,profile_idc:%d,num_ref:%d, width:%d,height:%d\n", ctx->avc_profile,ctx->avc_compat,ctx->avc_level,profile_idc,num_ref_frames,  ctx->width,ctx->height);
}

Flv2hlsContext* init_context(char*flv_meta_path)
{
    Flv2hlsContext* context = new Flv2hlsContext();
    hls_ctx_t   *ctx;
    char hls_path[1024] = {0};
    snprintf(hls_path, 1024,  "hls_%s", flv_meta_path);
    if (context->flvreader.open(flv_meta_path) != SUCCESS) {
        flv_freep(context);
        return NULL;
    }
    
    if (context->flvdec.initialize(&context->flvreader) != SUCCESS) {
        flv_freep(context);
        return NULL;
    }

    if (hls_init_playlist(&context->hls_ctx, hls_path) != SUCCESS) {
        ERROR("error: hls_init_playlist failed.\n");
        return NULL;
    }

    ctx = &context->hls_ctx;
    ctx->winfrags = g_winfrages;
    ctx->nfrags = 0;
    ctx->frag = 0;
    ctx->fraglen = g_fraglen;
    ctx->max_fraglen = g_max_fraglen;
    ctx->bStart = 0;
    
    if (ctx->frags == NULL) {
        ctx->frags = (hls_frag_t*) new hls_frag_t [ctx->winfrags*2+1];   
        memset(ctx->frags, 0, sizeof(hls_frag_t)*(ctx->winfrags*2+1));
    }
    return context;    
}

int main(int argc, char*argv[])
{
    char header[9];
    u_int32_t g_pos4firstpkt = 0;
    int ret = SUCCESS;
    u_int32_t vstartime = 0;
    char *source = "test";
    int c;

    while ((c = getopt(argc, argv, "w:f:m:s:")) != -1) {
        switch (c) {
            case 'c':
                g_winfrages = atoi(optarg);
                printf("g_winfrages:%d\n", g_winfrages);
                break;
            case 'f':
                g_fraglen = atoi(optarg);
                printf("g_fraglen:%d\n", g_fraglen);
                break;
            case 'm':
                g_max_fraglen = atoi(optarg);
                printf("g_max_fraglen:%d\n", g_max_fraglen);
                break;
            case 's':
                source = optarg;
                break;
            default:
                exit(0);
        }
    }

    time_t startid = time(NULL);

    Flv2hlsContext* g_con = init_context(source);  
    if( !g_con )
    {
        ERROR("error: fail to open test.flv\n");
        return 0;
    }

    if (flv_read_header(g_con, &header, &g_pos4firstpkt) != SUCCESS) {
        ERROR("error: read flv header failed.\n");
        return 0;
    }

    DEBUG("startid:%d\n", startid);
    startid = startid - 10;
    while(true){
        char type;
        u_int32_t timestamp=0;
        u_int32_t size = 0;
        
        
        if ((ret = flv_read_tag_header(g_con, &type, &size, &timestamp)) != SUCCESS) {        
            ERROR("error: flv_read_tag_header failed \n"); 
            return 0;
            char tmpname[35] = {0};
            sprintf(tmpname, "test-%d.flv", startid);
            std::string clippath = tmpname;
            g_con->flvreader.close();
            if (g_con->flvreader.open(clippath) != SUCCESS) {
               DEBUG("open clip:%s failed, try 1s later\n", clippath.c_str());
               sleep(1);
               continue;
            }

            if (g_con->flvdec.initialize(&g_con->flvreader) != SUCCESS) {
                ERROR("init new clip failed\n");
                return 0;
            }
            DEBUG("using new clip:%s\n", clippath.c_str());
            startid++;
            if ((ret = flv_read_tag_header(g_con, &type, &size, &timestamp)) != SUCCESS) {        
                ERROR("real error: flv_read_tag_header failed \n"); 
                return 0;
            }
        }
        if( vstartime == 0 )
        {
            vstartime = timestamp;
        }
        char* data = new char[size];
        DEBUG("flv frametype:%d, timestamp:%u\n", type, timestamp);
        if ((ret = flv_read_tag_data(g_con, &data, size, type)) != SUCCESS) {
            ERROR("error: flv_read_tag_data failed\n");
            delete data;
            return 0;
        }
        if( type == NGX_RTMP_MSG_VIDEO )
        {
            av_codec_parse_avc_header(g_con, (u_int8_t*)data, size);
        }
        else if( type == NGX_RTMP_MSG_AUDIO )
        {
            av_codec_parse_aac_header(g_con, (u_int8_t*)data, size);
        }
    }
    
    flv_close(g_con);
    ERROR(" job finished\n"); 
    return 0;
}
