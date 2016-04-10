#ifndef FLV_DECODER_H
#define FLV_DECODER_H
#include "common.h"



class FlvBuffer
{
private:
    std::vector<char> data;
public:
    FlvBuffer();
    virtual ~FlvBuffer();
public:
    /**
    * get the length of buffer. empty if zero.
    * @remark assert length() is not negative.
    */
    virtual int length();
    /**
    * get the buffer bytes.
    * @return the bytes, NULL if empty.
    */
    virtual char* bytes();
    /**
    * erase size of bytes from begin.
    * @param size to erase size of bytes. 
    *       clear if size greater than or equals to length()
    * @remark ignore size is not positive.
    */
    virtual void erase(int size);
    /**
    * append specified bytes to buffer.
    * @param size the size of bytes
    * @remark assert size is positive.
    */
    virtual void append(const char* bytes, int size);
public:

};


class FlvStream
{
private:
    char* p;
    char* pp;
    char* _bytes;
    int _size;
public:
    FlvStream();
    virtual ~FlvStream();
public:
    /**
    * initialize the stream from bytes.
    * @bytes, the bytes to convert from/to basic types.
    * @size, the size of bytes.
    * @remark, stream never free the bytes, user must free it.
    * @remark, return error when bytes NULL.
    * @remark, return error when size is not positive.
    */
    virtual int initialize(char* bytes, int size);
// get the status of stream
public:
    /**
    * get data of stream, set by initialize.
    * current bytes = data() + pos()
    */
    virtual char* data();
    /**
    * the total stream size, set by initialize.
    * left bytes = size() - pos().
    */
    virtual int size();
    /**
    * tell the current pos.
    */
    virtual int pos();
    /**
    * whether stream is empty.
    * if empty, user should never read or write.
    */
    virtual bool empty();
    /**
    * whether required size is ok.
    * @return true if stream can read/write specified required_size bytes.
    * @remark assert required_size positive.
    */
    virtual bool require(int required_size);
// to change stream.
public:
    /**
    * to skip some size.
    * @param size can be any value. positive to forward; nagetive to backward.
    * @remark to skip(pos()) to reset stream.
    * @remark assert initialized, the data() not NULL.
    */
    virtual void skip(int size);
public:
    /**
    * get 1bytes char from stream.
    */
    virtual int8_t read_1bytes();
    /**
    * get 2bytes int from stream.
    */
    virtual int16_t read_2bytes();
    /**
    * get 3bytes int from stream.
    */
    virtual int32_t read_3bytes();
    /**
    * get 4bytes int from stream.
    */
    virtual int32_t read_4bytes();
    /**
    * get 8bytes int from stream.
    */
    virtual int64_t read_8bytes();
    /**
    * get string from stream, length specifies by param len.
    */
    virtual std::string read_string(int len);
    /**
    * get bytes from stream, length specifies by param len.
    */
    virtual void read_bytes(char* data, int size);
public:
    /**
    * write 1bytes char to stream.
    */
    virtual void write_1bytes(int8_t value);
    /**
    * write 2bytes int to stream.
    */
    virtual void write_2bytes(int16_t value);
    /**
    * write 4bytes int to stream.
    */
    virtual void write_4bytes(int32_t value);
    /**
    * write 3bytes int to stream.
    */
    virtual void write_3bytes(int32_t value);
    /**
    * write 8bytes int to stream.
    */
    virtual void write_8bytes(int64_t value);
    /**
    * write string to stream
    */
    virtual void write_string(std::string value);
    /**
    * write bytes to stream
    */
    virtual void write_bytes(char* data, int size);
};

class FlvFileReader
{
private:
    std::string _file;
    FILE  *file;
public:
    FlvFileReader();
    virtual ~FlvFileReader();
public:
    /**
    * open file reader, can open then close then open...
    */
    virtual int open(std::string file);
    virtual void close();
public:
    virtual bool is_open();
    virtual int64_t tellg();
    virtual void skip(int64_t size);
    virtual int64_t lseek(int64_t offset);
    virtual int64_t filesize();
public:
    /**
    * read from file. 
    * @param pnread the output nb_read, NULL to ignore.
    */
    virtual int read(void* buf, size_t count, ssize_t* pnread);
};


/**
* decode flv file.
*/
class FlvDecoder
{
private:
    FlvFileReader* _fs;
private:
    FlvStream* tag_stream;
public:
    FlvDecoder();
    virtual ~FlvDecoder();
public:
    /**
    * initialize the underlayer file stream
    * @remark user can initialize multiple times to decode multiple flv files.
    * @remark, user must free the fs, flv decoder never close/free it.
    */
    virtual int initialize(FlvFileReader* fs);
public:
    /**
    * read the flv header, donot including the 4bytes previous tag size.
    * @remark assert header not NULL.
    */
    virtual int read_header(char header[9]);
    /**
    * read the tag header infos.
    * @remark assert ptype/pdata_size/ptime not NULL.
    */
    virtual int read_tag_header(char* ptype, u_int32_t* pdata_size, u_int32_t* ptime);
    /**
    * read the tag data.
    * @remark assert data not NULL.
    */
	virtual int read_tag_data(char** data, u_int32_t size, char ptype);

    /**
    * read the 4bytes previous tag size.
    * @remark assert previous_tag_size not NULL.
    */
    virtual int read_previous_tag_size(char previous_tag_size[4]);
	    
    /**
    * get current flv file handler position
    */
    virtual int getPosition();

    /**
    * seek flv file handler to position pos
    */
    virtual void seekPosition(int pos);

};

class FlvCodec
{
public:
    FlvCodec();
    virtual ~FlvCodec();
// the following function used to finger out the flv/rtmp packet detail.
public:


};



#define ERROR_SUCCESS 0
#define SUCCESS 0
#define ERROR_NORMAL 1
    
#define ERROR_SOCKET 100
#define ERROR_OPEN_SOCKET 101
#define ERROR_CONNECT 102
#define ERROR_SEND 103
#define ERROR_READ 104
#define ERROR_CLOSE 105
#define ERROR_DNS_RESOLVE 106
    
#define ERROR_URL_INVALID 200
#define ERROR_HTTP_RESPONSE 201
#define ERROR_HLS_INVALID 202
    
#define ERROR_NOT_SUPPORT 300
    
#define ERROR_ST_INITIALIZE 400
#define ERROR_ST_THREAD_CREATE 401
    
#define ERROR_HP_PARSE_URL 500
#define ERROR_HP_EP_CHNAGED 501
#define ERROR_HP_PARSE_RESPONSE 502
    
#define ERROR_RTMP_URL 600
#define ERROR_RTMP_OVERFLOW 601
#define ERROR_RTMP_MSG_TOO_BIG 602
#define ERROR_RTMP_INVALID_RESPONSE 603
#define ERROR_RTMP_OPEN_FLV 604

#define ERROR_SOCKET_CREATE                 1000
#define ERROR_SOCKET_SETREUSE               1001
#define ERROR_SOCKET_BIND                   1002
#define ERROR_SOCKET_LISTEN                 1003
#define ERROR_SOCKET_CLOSED                 1004
#define ERROR_SOCKET_GET_PEER_NAME          1005
#define ERROR_SOCKET_GET_PEER_IP            1006
#define ERROR_SOCKET_READ                   1007
#define ERROR_SOCKET_READ_FULLY             1008
#define ERROR_SOCKET_WRITE                  1009
#define ERROR_SOCKET_WAIT                   1010
#define ERROR_SOCKET_TIMEOUT                1011
#define ERROR_SOCKET_CONNECT                1012
#define ERROR_ST_SET_EPOLL                  1013

#define ERROR_ST_OPEN_SOCKET                1015
#define ERROR_ST_CREATE_LISTEN_THREAD       1016
#define ERROR_ST_CREATE_CYCLE_THREAD        1017
#define ERROR_ST_CONNECT                    1018
#define ERROR_SYSTEM_PACKET_INVALID         1019
#define ERROR_SYSTEM_CLIENT_INVALID         1020
#define ERROR_SYSTEM_ASSERT_FAILED          1021
#define ERROR_SYSTEM_SIZE_NEGATIVE          1022
#define ERROR_SYSTEM_CONFIG_INVALID         1023
#define ERROR_SYSTEM_CONFIG_DIRECTIVE       1024
#define ERROR_SYSTEM_CONFIG_BLOCK_START     1025
#define ERROR_SYSTEM_CONFIG_BLOCK_END       1026
#define ERROR_SYSTEM_CONFIG_EOF             1027
#define ERROR_SYSTEM_STREAM_BUSY            1028
#define ERROR_SYSTEM_IP_INVALID             1029
#define ERROR_SYSTEM_FORWARD_LOOP           1030
#define ERROR_SYSTEM_WAITPID                1031
#define ERROR_SYSTEM_BANDWIDTH_KEY          1032
#define ERROR_SYSTEM_BANDWIDTH_DENIED       1033
#define ERROR_SYSTEM_PID_ACQUIRE            1034
#define ERROR_SYSTEM_PID_ALREADY_RUNNING    1035
#define ERROR_SYSTEM_PID_LOCK               1036
#define ERROR_SYSTEM_PID_TRUNCATE_FILE      1037
#define ERROR_SYSTEM_PID_WRITE_FILE         1038
#define ERROR_SYSTEM_PID_GET_FILE_INFO      1039
#define ERROR_SYSTEM_PID_SET_FILE_INFO      1040
#define ERROR_SYSTEM_FILE_ALREADY_OPENED    1041
#define ERROR_SYSTEM_FILE_OPENE             1042
#define ERROR_SYSTEM_FILE_CLOSE             1043
#define ERROR_SYSTEM_FILE_READ              1044
#define ERROR_SYSTEM_FILE_WRITE             1045
#define ERROR_SYSTEM_FILE_EOF               1046
#define ERROR_SYSTEM_FILE_RENAME            1047
#define ERROR_SYSTEM_CREATE_PIPE            1048
#define ERROR_SYSTEM_FILE_SEEK              1049
#define ERROR_SYSTEM_IO_INVALID             1050
#define ERROR_ST_EXCEED_THREADS             1051

#define ERROR_HLS_METADATA                  3000
#define ERROR_HLS_DECODE_ERROR              3001
#define ERROR_HLS_CREATE_DIR                3002
#define ERROR_HLS_OPEN_FAILED               3003
#define ERROR_HLS_WRITE_FAILED              3004
#define ERROR_HLS_AAC_FRAME_LENGTH          3005
#define ERROR_HLS_AVC_SAMPLE_SIZE           3006
#define ERROR_HTTP_PARSE_URI                3007
#define ERROR_HTTP_DATA_INVLIAD             3008
#define ERROR_HTTP_PARSE_HEADER             3009
#define ERROR_HTTP_HANDLER_MATCH_URL        3010
#define ERROR_HTTP_HANDLER_INVALID          3011
#define ERROR_HTTP_API_LOGS                 3012
#define ERROR_HTTP_FLV_SEQUENCE_HEADER      3013
#define ERROR_HTTP_FLV_OFFSET_OVERFLOW      3014
#define ERROR_ENCODER_VCODEC                3015
#define ERROR_ENCODER_OUTPUT                3016
#define ERROR_ENCODER_ACHANNELS             3017
#define ERROR_ENCODER_ASAMPLE_RATE          3018
#define ERROR_ENCODER_ABITRATE              3019
#define ERROR_ENCODER_ACODEC                3020
#define ERROR_ENCODER_VPRESET               3021
#define ERROR_ENCODER_VPROFILE              3022
#define ERROR_ENCODER_VTHREADS              3023
#define ERROR_ENCODER_VHEIGHT               3024
#define ERROR_ENCODER_VWIDTH                3025
#define ERROR_ENCODER_VFPS                  3026
#define ERROR_ENCODER_VBITRATE              3027
#define ERROR_ENCODER_FORK                  3028
#define ERROR_ENCODER_LOOP                  3029
#define ERROR_ENCODER_OPEN                  3030
#define ERROR_ENCODER_DUP2                  3031
#define ERROR_ENCODER_PARSE                 3032
#define ERROR_ENCODER_NO_INPUT              3033
#define ERROR_ENCODER_NO_OUTPUT             3034
#define ERROR_ENCODER_INPUT_TYPE            3035
#define ERROR_KERNEL_FLV_HEADER             3036
#define ERROR_KERNEL_FLV_STREAM_CLOSED      3037
#define ERROR_KERNEL_STREAM_INIT            3038
#define ERROR_EDGE_VHOST_REMOVED            3039
#define ERROR_HLS_AVC_TRY_OTHERS            3040
#define ERROR_H264_API_NO_PREFIXED          3041
#define ERROR_FLV_INVALID_VIDEO_TAG         3042
#define ERROR_H264_DROP_BEFORE_SPS_PPS      3043
#define ERROR_H264_DUPLICATED_SPS           3044
#define ERROR_H264_DUPLICATED_PPS           3045
#define ERROR_AAC_REQUIRED_ADTS             3046
#define ERROR_AAC_ADTS_HEADER               3047
#define ERROR_AAC_DATA_INVALID              3048



typedef struct {
    u_int32_t                  width;
    u_int32_t                  height;
    u_int32_t                  duration;
    u_int32_t                  frame_rate;
    u_int32_t                  video_data_rate;
    u_int32_t                  video_codec_id;
    u_int32_t                  audio_data_rate;
    u_int32_t                  audio_codec_id;
    u_int32_t                  aac_profile;
    u_int32_t                  aac_chan_conf;
    u_int32_t                  aac_sbr;
    u_int32_t                  aac_ps;
    u_int32_t                  avc_profile;
    u_int32_t                  avc_compat;
    u_int32_t                  avc_level;
    u_int32_t                  avc_nal_bytes;
    u_int32_t                  avc_ref_frames;
    u_int32_t                  sample_rate;    /* 5512, 11025, 22050, 44100 */
    u_int32_t                  sample_size;    /* 1=8bit, 2=16bit */
    u_int32_t                  audio_channels; /* 1, 2 */
    u_int8_t                   profile[32];
    u_int8_t                   level[32];

    u_int8_t                   *avc_header;
    u_int8_t                   *aac_header;

    u_int8_t                   *meta;
    u_int32_t                  meta_version;
} av_codec_ctx_t;



#define flv_freep(p) \
    if (p) { \
        delete p; \
        p = NULL; \
    } \
    (void)0
    
typedef struct {
    u_int8_t    *pos;
    u_int8_t    *last;
    u_int32_t   offs;
    u_int32_t   err;
} stream_bit_reader_t;

void stream_bit_init_reader(stream_bit_reader_t *br, u_int8_t *pos, u_int8_t *last);
u_int64_t stream_bit_read(stream_bit_reader_t *br, u_int32_t n);
u_int64_t stream_bit_read_golomb(stream_bit_reader_t *br);

#define stream_bit_read_err(br) ((br)->err)

#define stream_bit_read_eof(br) ((br)->pos == (br)->last)

#define stream_bit_read_8(br)                                               \
    ((u_int8_t) stream_bit_read(br, 8))

#define stream_bit_read_16(br)                                              \
    ((u_int16_t) stream_bit_read(br, 16))

#define stream_bit_read_32(br)                                              \
    ((u_int32_t) stream_bit_read(br, 32))


/* Bit reverse: we need big-endians in many places  */
void * flv_rmemcpy(void *dst, const void* src, size_t n);



#endif
