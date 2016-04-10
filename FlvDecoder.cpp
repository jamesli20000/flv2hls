#include "FlvDecoder.h"


FlvDecoder::FlvDecoder()
{
    _fs = NULL;
    tag_stream = new FlvStream();
}

FlvDecoder::~FlvDecoder()
{
    flv_freep(tag_stream);
}

int FlvDecoder::initialize(FlvFileReader* fs)
{
    int ret = ERROR_SUCCESS;
    
    
    if (!fs->is_open()) {
        ret = ERROR_KERNEL_FLV_STREAM_CLOSED;
        printf("stream is not open for decoder. ret=%d", ret);
        return ret;
    }
    
    _fs = fs;
    
    return ret;
}

int FlvDecoder::read_header(char *header)
{
    int ret = ERROR_SUCCESS;
    
    if ((ret = _fs->read(header, 9, NULL)) != ERROR_SUCCESS) {
        return ret;
    }
    
    char* h = header;
    if (h[0] != 'F' || h[1] != 'L' || h[2] != 'V') {
        ret = ERROR_KERNEL_FLV_HEADER;
        printf("flv header must start with FLV. ret=%d\n", ret);
        return ret;
    }
    
    return ret;
}

int FlvDecoder::getPosition()
{
    return _fs->tellg();
}

void FlvDecoder::seekPosition(int pos)
{
    _fs->lseek(pos);
}

int FlvDecoder::read_tag_header(char* ptype, u_int32_t *pdata_size, u_int32_t* ptime)
{
    int ret = ERROR_SUCCESS;


    u_char th[11]; // tag header
    
    // read tag header
    if ((ret = _fs->read(th, 11, NULL)) != ERROR_SUCCESS) {
        if (ret != ERROR_SYSTEM_FILE_EOF) {
            ERROR("error: FlvDecoder::read_tag_header failed. ret=%d\n", ret);
        }
        return ret;
    }
    
    // Reserved UB [2]
    // Filter UB [1]
    // TagType UB [5]
    *ptype = (th[0] & 0x1F);
    
    // DataSize UI24
    char* pp = (char*)pdata_size;
    pp[2] = th[1];
    pp[1] = th[2];
    pp[0] = th[3];
    
    // Timestamp UI24
    pp = (char*)ptime;
    pp[2] = th[4];
    pp[1] = th[5];
    pp[0] = th[6];
    
    // TimestampExtended UI8
    pp[3] = th[7];

    return ret;
}

int FlvDecoder::read_tag_data(char** data, u_int32_t size, char ptype)
{
    int ret = ERROR_SUCCESS;

    
    if ((ret = _fs->read(*data, size, NULL)) != ERROR_SUCCESS) {
        if (ret != ERROR_SYSTEM_FILE_EOF) {
            printf("read flv tag header failed. ret=%d", ret);
        }
        return ret;
    }
    
    if(ptype == 18)
    {
        //ResetDuration4Live((unsigned char*)data, size);
        printf("meta tag found\n");
    }
    return ret;

}

int FlvDecoder::read_previous_tag_size(char previous_tag_size[4])
{
    int ret = ERROR_SUCCESS;

    // ignore 4bytes tag size.
    if ((ret = _fs->read(previous_tag_size, 4, NULL)) != ERROR_SUCCESS) {
        if (ret != ERROR_SYSTEM_FILE_EOF) {
            printf("read flv previous tag size failed. ret=%d", ret);
        }
        return ret;
    }
    
    return ret;
}


FlvCodec::FlvCodec()
{
}

FlvCodec::~FlvCodec()
{
}


FlvStream::FlvStream()
{
    p = _bytes = NULL;
    _size = 0;
    
   
}

FlvStream::~FlvStream()
{
}

int FlvStream::initialize(char* bytes, int size)
{
    int ret = ERROR_SUCCESS;
    
    if (!bytes) {
        ret = ERROR_KERNEL_STREAM_INIT;
        printf("stream param bytes must not be NULL. ret=%d", ret);
        return ret;
    }
    
    if (size <= 0) {
        ret = ERROR_KERNEL_STREAM_INIT;
        printf("stream param size must be positive. ret=%d", ret);
        return ret;
    }

    _size = size;
    p = _bytes = bytes;
    printf("init stream ok, size=%d", size);

    return ret;
}

char* FlvStream::data()
{
    return _bytes;
}

int FlvStream::size()
{
    return _size;
}

int FlvStream::pos()
{
    return p - _bytes;
}

bool FlvStream::empty()
{
    return !_bytes || (p >= _bytes + _size);
}

bool FlvStream::require(int required_size)
{
    
    return required_size <= _size - (p - _bytes);
}

void FlvStream::skip(int size)
{
    
    p += size;
}

int8_t FlvStream::read_1bytes()
{
    
    return (int8_t)*p++;
}

int16_t FlvStream::read_2bytes()
{
    
    int16_t value;
    pp = (char*)&value;
    pp[1] = *p++;
    pp[0] = *p++;
    
    return value;
}

int32_t FlvStream::read_3bytes()
{
    
    int32_t value = 0x00;
    pp = (char*)&value;
    pp[2] = *p++;
    pp[1] = *p++;
    pp[0] = *p++;
    
    return value;
}

int32_t FlvStream::read_4bytes()
{
    
    int32_t value;
    pp = (char*)&value;
    pp[3] = *p++;
    pp[2] = *p++;
    pp[1] = *p++;
    pp[0] = *p++;
    
    return value;
}

int64_t FlvStream::read_8bytes()
{
    
    int64_t value;
    pp = (char*)&value;
    pp[7] = *p++;
    pp[6] = *p++;
    pp[5] = *p++;
    pp[4] = *p++;
    pp[3] = *p++;
    pp[2] = *p++;
    pp[1] = *p++;
    pp[0] = *p++;
    
    return value;
}

std::string FlvStream::read_string(int len)
{
    
    std::string value;
    value.append(p, len);
    
    p += len;
    
    return value;
}

void FlvStream::read_bytes(char* data, int size)
{
    
    memcpy(data, p, size);
    
    p += size;
}

void FlvStream::write_1bytes(int8_t value)
{
    
    *p++ = value;
}

void FlvStream::write_2bytes(int16_t value)
{
    
    pp = (char*)&value;
    *p++ = pp[1];
    *p++ = pp[0];
}

void FlvStream::write_4bytes(int32_t value)
{
    
    pp = (char*)&value;
    *p++ = pp[3];
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void FlvStream::write_3bytes(int32_t value)
{
    
    pp = (char*)&value;
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void FlvStream::write_8bytes(int64_t value)
{
    
    pp = (char*)&value;
    *p++ = pp[7];
    *p++ = pp[6];
    *p++ = pp[5];
    *p++ = pp[4];
    *p++ = pp[3];
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void FlvStream::write_string(std::string value)
{
    
    memcpy(p, value.data(), value.length());
    p += value.length();
}

void FlvStream::write_bytes(char* data, int size)
{
    
    memcpy(p, data, size);
    p += size;
}


#define SOCKET_READ_SIZE 4096



FlvFileReader::FlvFileReader()
{
    file = NULL;
}

FlvFileReader::~FlvFileReader()
{
    close();
}

int FlvFileReader::open(std::string filename)
{
    int ret = ERROR_SUCCESS;
    
    if (file) {
        ret = ERROR_SYSTEM_FILE_ALREADY_OPENED;
        close();
    }

    if ((file = fopen(filename.c_str(), "rb" )) == NULL) {
        ret = ERROR_SYSTEM_FILE_OPENE;
        printf("open file %s failed. ret=%d", filename.c_str(), ret);
        return ret;
    }
    
    _file = filename;
    DEBUG("open %s  OK\n", filename.c_str());
    return ret;
}

void FlvFileReader::close()
{
    int ret = ERROR_SUCCESS;
    
    if (!file ) {
        return;
    }
    
    if (fclose(file) < 0) {
        ret = ERROR_SYSTEM_FILE_CLOSE;
        printf("close file %s failed. ret=%d", _file.c_str(), ret);
        return;
    }
    file = NULL;
    
    return;
}

bool FlvFileReader::is_open()
{
    return file?1:0;
}

int64_t FlvFileReader::tellg()
{
    return (int64_t)fseek(file, 0, SEEK_CUR);
}

void FlvFileReader::skip(int64_t size)
{
    fseek(file, (off_t)size, SEEK_CUR);
}

int64_t FlvFileReader::lseek(int64_t offset)
{
    return (int64_t)fseek(file, (off_t)offset, SEEK_SET);
}

int64_t FlvFileReader::filesize()
{
    int64_t cur = tellg();
    int64_t size = (int64_t)fseek(file, 0, SEEK_END);
    fseek(file, (off_t)cur, SEEK_SET);
    return size;
}

int FlvFileReader::read(void* buf, size_t count, ssize_t* pnread)
{
    int ret = ERROR_SUCCESS;
    
    ssize_t nread;
    // TODO: FIXME: use st_read.
    if ((nread = fread( buf, count, 1, file)) < 0) {
        ret = ERROR_SYSTEM_FILE_READ;
        printf("read from file %s failed. ret=%d", _file.c_str(), ret);
        return ret;
    }
    
    if (nread == 0) {
        ret = ERROR_SYSTEM_FILE_EOF;
        return ret;
    }
    
    if (pnread != NULL) {
        *pnread = nread;
    }
    
    return ret;
}

FlvBuffer::FlvBuffer()
{
}

FlvBuffer::~FlvBuffer()
{
}

int FlvBuffer::length()
{
    int len = (int)data.size();
    return len;
}

char* FlvBuffer::bytes()
{
    return (length() == 0)? NULL : &data.at(0);
}

void FlvBuffer::erase(int size)
{
    if (size <= 0) {
        return;
    }
    
    if (size >= length()) {
        data.clear();
        return;
    }
    
    data.erase(data.begin(), data.begin() + size);
}

void FlvBuffer::append(const char* bytes, int size)
{
    data.insert(data.end(), bytes, bytes + size);
}



void
stream_bit_init_reader(stream_bit_reader_t *br, u_int8_t *pos, u_int8_t *last)
{
    memset(br, 0, sizeof(stream_bit_reader_t));

    br->pos = pos;
    br->last = last;
}


u_int64_t
stream_bit_read(stream_bit_reader_t *br, u_int32_t n)
{
    u_int64_t    v;
    u_int32_t  d;

    v = 0;

    while (n) {

        if (br->pos >= br->last) {
            br->err = 1;
            return 0;
        }

        d = (br->offs + n > 8 ? (u_int32_t) (8 - br->offs) : n);

        v <<= d;
        v += (*br->pos >> (8 - br->offs - d)) & ((u_int8_t) 0xff >> (8 - d));

        br->offs += d;
        n -= d;

        if (br->offs == 8) {
            br->pos++;
            br->offs = 0;
        }
    }

    return v;
}


u_int64_t
stream_bit_read_golomb(stream_bit_reader_t *br)
{
    u_int32_t  n;

    for (n = 0; stream_bit_read(br, 1) == 0 && !br->err; n++);

    return ((u_int64_t) 1 << n) + stream_bit_read(br, n) - 1;
}

void *
flv_rmemcpy(void *dst, const void* src, size_t n)
{
    u_char     *d, *s;

    d = (u_char*)dst;
    s = (u_char*)src + n - 1;

    while(s >= (u_char*)src) {
        *d++ = *s--;
    }

    return dst;
}

