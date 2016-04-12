# flv2hls
a tool that could covert flv into http live streaming

This tool could be used to convert flv formart file into apple http live streaming format/mpeg2ts  file.

compiLe:

g++ flv2hls.c FlvDecoder.cpp flv_mpegts.c -o flv2hls

usage:
./flv2hls -s (your flv file) 

or

./flv2hls -s (your flv file) -w (item number in one m3u8 file) -f (segment length) -m (max segment length)

more detail could visit:

 http://www.jamesli20000.com/wordpress/how-to-convert-a-live-http-flv-stream-into-a-http-live-streammpeg2ts/
 
 
