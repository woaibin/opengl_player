#ifndef INTERFACES_H_PLUS
#define INTERFACES_H_PLUS
#include  "../codec/ffmpeg/ffmpeg_codec_header.h"
#include  "../codec/ffmpeg/ffmpeg_err_handle.h"
#include "stdio.h"
//a class used to store raw video and audio frames.
class MediaFramePool
{
private:
    //memory pool:
    AVBufferPool *AudioPool = NULL;
    AVBufferPool *VideoPool = NULL;
    int size_raw_video_frame;
    int size_raw_audio_frame;
public:
    bool InitPoolThroupAVCodecContext(AVCodecContext *audio_codecContext,AVCodecContext *video_codecContext);
    AVBufferPool* getVideoPool()const{return this->VideoPool;}
    AVBufferPool* getAudioPool()const{return this->AudioPool;}
    int getVideoFrameSize()const{return this->size_raw_video_frame;}
    int getAudioFrameSize()const{return this->size_raw_audio_frame;}
};
bool MediaFramePool::InitPoolThroupAVCodecContext(AVCodecContext *audio_codecContext,AVCodecContext *video_codecContext)
{
    if(audio_codecContext)
    {
        size_raw_video_frame = av_image_get_buffer_size(AV_PIX_FMT_RGB24,video_codecContext->width,video_codecContext->height,1);
        VideoPool = av_buffer_pool_init(size_raw_video_frame,NULL);
    }
    if(video_codecContext)
    {
        int linesize_dest;
        size_raw_audio_frame = av_samples_get_buffer_size(
            &linesize_dest,
            av_get_channel_layout_nb_channels(audio_codecContext->channel_layout),
            audio_codecContext->frame_size,AV_SAMPLE_FMT_S16,0);//only support stero s16le
        AudioPool = av_buffer_pool_init(size_raw_audio_frame,NULL);
    }
    return true;
}
class MediaFrame
{
private:
    //common members:
    AVBufferRef* buffer = NULL;
    int data_size = 0;
    double clock = 0;
private:
    //video members:
    int width = 0;
    int height = 0;
    double duration = 0;
    int testindex = 0;
private:
    //audio memebers:
    int nb_samples = 0;
    int sample_rate = 0;
    int nb_channels = 0;
public:
    MediaFrame(){logger = new FileLog("MediaFrame_codec_log");}
    ~MediaFrame()
    {
        av_buffer_unref(&buffer);
    }
public:
    bool InitMediaFrameFromAVFrame(AVFrame* frame,double time_base,bool videoOraudio,MediaFramePool* framepool);//trans avframe to media frame
    double getDuration()const{return this->duration;}
    unsigned char*getdata()const{return this->buffer->data;}
    double getclock()const{return this->clock;}
    int getDataSize()const{return this->data_size;}
    int getnbsamples()const{return this->nb_samples;}
public:
    int getwidth()const{return this->width;}
    int getheight()const{return this->height;}
private:
    bool YUV2RGB(AVFrame* frame,AVBufferPool* videoPool);
    bool FLTP2S16LE(AVFrame* frame,AVBufferPool* audioPool);
private:
    FileLog* logger = NULL;
};
bool MediaFrame::InitMediaFrameFromAVFrame(AVFrame* frame,double time_base,bool videoOraudio,MediaFramePool* framepool)
{
    //common fields:
    this->duration = frame->pkt_duration*time_base;
    this->clock = frame->pts*time_base;    
    if(videoOraudio) 
    {
        FFMPEG_RET_ERR_HANDLE(YUV2RGB(frame,framepool->getVideoPool()),logger,FFMPEG_MAKE_ERR_LOG_STRING("InitMediaFrameFromAVFrame","YUV to RGB failed"))
        //video fields:
        this->width = frame->width;this->height = frame->height;
        this->data_size = framepool->getVideoFrameSize();
    }
    else
    {
        FFMPEG_RET_ERR_HANDLE(FLTP2S16LE(frame,framepool->getAudioPool()),logger,FFMPEG_MAKE_ERR_LOG_STRING("InitMediaFrameFromAVFrame","FLTP to S16LE failed"))
        //audio fields:
        this->nb_samples = frame->nb_samples;
        this->sample_rate = frame->sample_rate;
        this->nb_channels = av_get_channel_layout_nb_channels(frame->channel_layout);
        this->data_size = framepool->getAudioFrameSize();
    }
    return true;
}
bool MediaFrame::YUV2RGB(AVFrame* frame,AVBufferPool* videoPool)
{
    AVFrame* frame_RGB = av_frame_alloc();
    buffer = av_buffer_pool_get(videoPool);
    //set the allocated buffer to the frame and calculate linesizes according to the pixel format,width,height,and CPU alignment
    FFMPEG_RET_ERR_HANDLE(
        av_image_fill_arrays(frame_RGB->data,frame_RGB->linesize,buffer->data,AV_PIX_FMT_RGB24,frame->width,frame->height,1),
        logger,FFMPEG_MAKE_ERR_LOG_STRING("YUV2RGB","av_image_fill_arrarys failed")
    );
    struct SwsContext* imgCtx = NULL;
    FFMPEG_OBJ_ERR_HANDLE(
        (imgCtx = sws_getContext(frame->width,frame->height,(AVPixelFormat)frame->format,frame->width,frame->height,AV_PIX_FMT_RGB24,SWS_BILINEAR,NULL,NULL,NULL)),
        logger,FFMPEG_MAKE_ERR_LOG_STRING("YUV2RGB","sws get context failed")
    );
    //yuv planar format should be y,u,v order(not y,v,u instead),so here we should make some code to deal with the yuv order...
    FFMPEG_RET_ERR_HANDLE(
        sws_scale(imgCtx,frame->data,frame->linesize,0,frame->height,frame_RGB->data,frame_RGB->linesize),
        logger,FFMPEG_MAKE_ERR_LOG_STRING("YUV2RGB","sws_scale failed")
    );
    //clear:
    av_frame_free(&frame_RGB);
    sws_freeContext(imgCtx);
    return true;
}

bool MediaFrame::FLTP2S16LE(AVFrame* frame,AVBufferPool* audioPool)
{
    //allocate resample context:
    SwrContext* context = swr_alloc();
    FFMPEG_OBJ_ERR_HANDLE(context,logger,"failed allocating context!");
    //set input options:
    av_opt_set_int(context,"in_channel_layout",frame->channel_layout,0);
    av_opt_set_int(context,"in_sample_rate",frame->sample_rate,0);
    av_opt_set_sample_fmt(context,"in_sample_fmt",(AVSampleFormat)frame->format,0);

    //set output options:
    av_opt_set_int(context,"out_channel_layout",frame->channel_layout,0);
    av_opt_set_int(context,"out_sample_rate",frame->sample_rate,0);
    av_opt_set_sample_fmt(context,"out_sample_fmt",AV_SAMPLE_FMT_S16,0);

    //init the resampling context:
    FFMPEG_RET_ERR_HANDLE(swr_init(context),logger,"failed to init swr context!");
    
    buffer = av_buffer_pool_get(audioPool);
    FFMPEG_RET_ERR_HANDLE(swr_convert(context,&buffer->data,frame->nb_samples,(const uint8_t**)frame->data,frame->nb_samples),logger,"swr convert failed!");
    //clear:
    swr_free(&context);
    return true;
}

#endif // !INTERFACES_H_PLUS

