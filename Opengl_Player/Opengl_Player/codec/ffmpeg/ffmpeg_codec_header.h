//compile the following using C
extern "C"{
    #ifndef FFMPEG_CODEC_HEADER_H
    #define FFMPEG_CODEC_HEADER_H
    #include <libavcodec/avcodec.h>
    #include <libavutil/log.h>
    #include <libavutil/time.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #endif // !FFMPEG_CODEC_HEADER_H
}
