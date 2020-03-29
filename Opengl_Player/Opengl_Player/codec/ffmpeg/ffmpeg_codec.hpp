#ifndef FFMPEG_CODEC_H_PLUS
#define FFMPEG_CODEC_H_PLUS
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include "ffmpeg_codec_header.h"
#include "../../common/interfaces.hpp"
#include "../../common/filelog.h"
#include "ffmpeg_err_handle.h"
class FFmpegCodec
{
private:
    AVCodec* audio_codec = NULL;
    AVCodec* video_codec = NULL;
    AVCodecContext* audio_codecContext = NULL;
    AVCodecContext* video_codecContext = NULL;
    AVFormatContext* formatcontext = NULL;
    int video_stream_index = -1;
    int audio_stream_index = -1;
    int video_packet_num = 0;
    int audio_packet_num = 0;
    int decoded_video_packet_num = 0;
    int decoded_audio_packet_num = 0;
    AVStream* video_stream = NULL;
    AVStream* audio_stream = NULL;
private:
    std::thread *decode_thread;
    std::thread *query_decoded_audio_thread;
    std::thread *query_decoded_video_thread;
    std::mutex decoded_queue_access;
    std::mutex *decoded_audio_queue_access;
    std::mutex *decoded_video_queue_access;
    std::queue <AVPacket*> need_to_decode_video_queue;
    std::queue <AVPacket*> need_to_decode_audio_queue;
    std::mutex *need_to_decode_audio_queue_access;
    std::mutex *need_to_decode_video_queue_access;
    FileLog* logger = NULL;
    MediaFramePool* framepool;
    enum decode_state
    {
        decode_state_not_start = 0,
        decode_state_started,
        decode_state_packet_read_completely,
        decode_state_stop,
        decode_state_finish
    };
    decode_state DecVideoState = decode_state_not_start;
    decode_state DecAudioState = decode_state_not_start;
public:
    //codec options
    typedef struct codec_opt
    {
    //common members:
        int     video_codecid;
        int     audio_codecid;
        bool    isvideo;//decode video option
        bool    isaudio;//decode audio option
        bool    decorenc;//true for decode,false for encode
    //video options:
    //audio options:
    }CODEC_OPT;
private:
    struct codec_opt *options;
public:
    bool openinput(const char* path,const char* format = NULL);
    AVCodecContext* GetAudioCodecContext()const{return this->audio_codecContext;}
    AVCodecContext* GetVideoCodecContext()const{return this->video_codecContext;}
    bool initcodec(CODEC_OPT *opt);
    bool start_decode(std::queue<MediaFrame*>* decoded_video_frames,std::queue<MediaFrame*>* decoded_audio_frames);
    MediaFrame* getframe(std::queue<MediaFrame*>* decoded_frames,bool VideoOrAudio);//get decoded frame in a thread-save way
    AVPacket* getpacket(std::queue<AVPacket*> *need_to_decode_packet,bool VideoOrAudio);//get packets needed to decode in a thread-save way
    void getsize(int *width,int *height){*width = video_codecContext->width;*height = video_codecContext->height; }//this function can only be called after openinput is done.
private:
    bool thread_decode(std::queue<MediaFrame*>* decoded_video_frames,std::queue<MediaFrame*>* decoded_audio_frames);
    bool getcodec(AVCodec* codec,AVCodecContext** codecContext,AVStream** stream,AVMediaType mediatype,bool decorenc);
    bool query_decoded_audio_threadfunc(AVCodecContext* video_codec_context,std::queue<MediaFrame*>* frames);
    bool query_decoded_video_threadfunc(AVCodecContext* audio_codec_context,std::queue<MediaFrame*>* frames);
};
bool FFmpegCodec::initcodec(FFmpegCodec::CODEC_OPT *opt)
{
    logger = new FileLog("ffmpeg_codec_log");//allocate looger
    av_register_all();//register ffmpeg codecs,demuxers and muxers
    this->options = opt;
    framepool = new MediaFramePool();
    decoded_audio_queue_access = new std::mutex();
    decoded_video_queue_access = new std::mutex();
    need_to_decode_audio_queue_access = new std::mutex();
    need_to_decode_video_queue_access = new std::mutex();
    return false;
}

bool FFmpegCodec::openinput(const char* path,const char* format)
{
    //check params:
    if(!path)
    {
        logger->logtofile(FFMPEG_MAKE_ERR_LOG_STRING("openinput","path is unavailable"),LOG_LEVEL_ERROR);
    }
    //open input:
    FFMPEG_RET_ERR_HANDLE(avformat_open_input(&this->formatcontext,path,NULL,NULL),logger,FFMPEG_MAKE_ERR_LOG_STRING("openinput","open input failed"));
    //get stream info:
    FFMPEG_RET_ERR_HANDLE(avformat_find_stream_info(formatcontext,NULL),logger,FFMPEG_MAKE_ERR_LOG_STRING("openinput","find stream info failed"));
    //handle audio codec:
    if(this->options->isaudio)
        FFMPEG_RET_ERR_HANDLE(getcodec(audio_codec,&audio_codecContext,&audio_stream,AVMEDIA_TYPE_AUDIO,true),logger,FFMPEG_MAKE_ERR_LOG_STRING("openinput","handle audio codec failed"));
    if(this->options->isvideo)
        FFMPEG_RET_ERR_HANDLE(getcodec(video_codec,&video_codecContext,&video_stream,AVMEDIA_TYPE_VIDEO,true),logger,FFMPEG_MAKE_ERR_LOG_STRING("openinput","handle video codec failed"));
    framepool->InitPoolThroupAVCodecContext(audio_codecContext,video_codecContext);
    return true;
}

bool FFmpegCodec::start_decode(std::queue<MediaFrame*>* decoded_video_frames,std::queue<MediaFrame*>* decoded_audio_frames)
{
    this->decode_thread = new std::thread(&FFmpegCodec::thread_decode,this,decoded_video_frames,decoded_audio_frames);
    DecVideoState = decode_state_started;
    DecAudioState = decode_state_started;
    return true;
}

bool FFmpegCodec::thread_decode(std::queue<MediaFrame*>* decoded_video_frames,std::queue<MediaFrame*>* decoded_audio_frames)
{
    //initialization:
    AVPacket *ffmpeg_media_packet;
    ffmpeg_media_packet = av_packet_alloc();
    av_init_packet(ffmpeg_media_packet);
    ffmpeg_media_packet->data = NULL;
    ffmpeg_media_packet->size = 0;
    //read frames
    if(video_codecContext)
    {
        query_decoded_video_thread = new std::thread(&FFmpegCodec::query_decoded_video_threadfunc,this,video_codecContext,decoded_video_frames);
    }
    if(audio_codecContext)
    {
        query_decoded_audio_thread = new std::thread(&FFmpegCodec::query_decoded_audio_threadfunc,this,audio_codecContext,decoded_audio_frames);
    }
    while(!av_read_frame(formatcontext,ffmpeg_media_packet))
    {
        std::string errinfo;
        if(ffmpeg_media_packet->stream_index==video_stream_index)
        {
            need_to_decode_video_queue_access->lock();
            need_to_decode_video_queue.push(ffmpeg_media_packet);
            video_packet_num++;
            need_to_decode_video_queue_access->unlock();
        }
        else if(ffmpeg_media_packet->stream_index==audio_stream_index)
        {
            need_to_decode_audio_queue_access->lock();
            need_to_decode_audio_queue.push(ffmpeg_media_packet);
            audio_packet_num++;
            need_to_decode_audio_queue_access->unlock();
        }
        else
        {
            av_packet_unref(ffmpeg_media_packet);    
            continue;
        }
        //control parse rate:
        if(need_to_decode_video_queue.size()>80)
        {
            while(need_to_decode_video_queue.size()>60)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        //realloc an other packet:
        DecVideoState = decode_state_finish;
        DecAudioState = decode_state_finish;
        ffmpeg_media_packet = av_packet_alloc();
        av_init_packet(ffmpeg_media_packet);
        ffmpeg_media_packet->data = NULL;
        ffmpeg_media_packet->size = 0;
    }
    return true;
}

bool FFmpegCodec::getcodec(AVCodec* codec,AVCodecContext** codecContext,AVStream** stream,AVMediaType mediatype,bool decorenc)
{
    std::string mediatype_id_string;
    std::string err_reason;
    int stream_index;
    //check media type:
    if(mediatype==AVMEDIA_TYPE_AUDIO)
        mediatype_id_string="audio";
    else if(mediatype==AVMEDIA_TYPE_VIDEO)
        mediatype_id_string="video";
    else
    {
        logger->logtofile(FFMPEG_MAKE_ERR_LOG_STRING("getcodec","media type is unavailable"),LOG_LEVEL_ERROR);
        return false;
    }
    //find audio or video stream:
    err_reason = "find "+mediatype_id_string+" stream index failed!";
    FFMPEG_RET_ERR_HANDLE((stream_index = av_find_best_stream(formatcontext,mediatype,-1,-1,NULL,NULL)),logger,FFMPEG_MAKE_ERR_LOG_STRING2("getcodec",err_reason));
    *stream = formatcontext->streams[stream_index];
    //find codec
    if(decorenc)
    {
        err_reason = "find "+mediatype_id_string+" decoder failed";
        FFMPEG_OBJ_ERR_HANDLE((codec = avcodec_find_decoder((*stream)->codecpar->codec_id)),logger,FFMPEG_MAKE_ERR_LOG_STRING2("getcodec",err_reason));
    }
    else
    {
        err_reason = "find "+mediatype_id_string+" encoder failed";
        FFMPEG_OBJ_ERR_HANDLE((codec = avcodec_find_encoder((*stream)->codecpar->codec_id)),logger,FFMPEG_MAKE_ERR_LOG_STRING2("getcodec",err_reason));
    }
    //alloc codec context:
    err_reason = "alloc "+mediatype_id_string+" decoder context failed";
    FFMPEG_OBJ_ERR_HANDLE((*codecContext = avcodec_alloc_context3(codec)),logger,FFMPEG_MAKE_ERR_LOG_STRING2("getcodec",err_reason));
    //copy codec params to the context:
    err_reason = "copy "+mediatype_id_string+" decoder params to context failed";
    FFMPEG_RET_ERR_HANDLE(avcodec_parameters_to_context(*codecContext,(*stream)->codecpar),logger,FFMPEG_MAKE_ERR_LOG_STRING2("getcodec",err_reason));
    //open codec:
    err_reason = "open "+mediatype_id_string+" codec failed";
    FFMPEG_RET_ERR_HANDLE(avcodec_open2(*codecContext,codec,NULL),logger,FFMPEG_MAKE_ERR_LOG_STRING2("getcodec",err_reason));
    //return stream and stream index:
    if(mediatype==AVMEDIA_TYPE_AUDIO)
        this->audio_stream_index = stream_index;
    else
        this->video_stream_index = stream_index;
    
    return true;
}

bool FFmpegCodec::query_decoded_audio_threadfunc(AVCodecContext* audio_codec_context,std::queue<MediaFrame*>* frames)
{
    AVFrame* tempFrame = NULL;
    std::string errinfo;
    tempFrame = av_frame_alloc();
    int ret;
    while(1)
    {
        //get one need-to-decode packet:
        AVPacket* tempPacket = getpacket(&this->need_to_decode_audio_queue,false);
        if(decoded_audio_packet_num==audio_packet_num&&DecAudioState==decode_state_finish)
        {
            //entering drain mode:
            FFMPEG_RET_ERR_HANDLE(avcodec_send_packet(audio_codec_context,NULL),logger,FFMPEG_MAKE_ERR_LOG_STRING("decode","decode audio:entering drain mode failed"));
        }
        else
        {
            errinfo = "err occurred while sending a audio packet";
            FFMPEG_RET_ERR_HANDLE(avcodec_send_packet(audio_codec_context,tempPacket),logger,FFMPEG_MAKE_ERR_LOG_STRING2("decode",errinfo));  
            decoded_audio_packet_num++;
        }
        av_packet_free(&tempPacket);
        //get one decoded frame:
        ret = avcodec_receive_frame(audio_codec_context,tempFrame);
        if(ret == AVERROR(EAGAIN))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        else if(ret == AVERROR_EOF)
            break;
        else if(!ret)
        {
            //output frame is ready:
            MediaFrame* mediaframe = new MediaFrame();
            mediaframe->InitMediaFrameFromAVFrame(tempFrame,av_q2d(audio_stream->time_base),false,framepool);
            decoded_audio_queue_access->lock();
            frames->push(mediaframe);
            decoded_audio_queue_access->unlock();
            //control decode rate:
            if(frames->size()>60)
            {
                while(frames->size()>40)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
        else
        {
            //error occurred:
            logger->logtofile(FFMPEG_MAKE_ERR_LOG_STRING("get_decode_data","decoding video: receive frame error occurred"),LOG_LEVEL_ERROR);   
        }     
    }
    av_frame_free(&tempFrame);
}

bool FFmpegCodec::query_decoded_video_threadfunc(AVCodecContext* video_codec_context,std::queue<MediaFrame*>* frames)
{
    AVFrame* tempFrame = NULL;
    tempFrame = av_frame_alloc();
    std::string errinfo;
    int ret;
    while(1)
    {
        //get one need-to-decode packet:
        AVPacket* tempPacket = getpacket(&this->need_to_decode_video_queue,true);
        if(decoded_video_packet_num == video_packet_num&&DecVideoState==decode_state_finish)
        {
            //entering drain mode:
            FFMPEG_RET_ERR_HANDLE(avcodec_send_packet(video_codec_context,NULL),logger,FFMPEG_MAKE_ERR_LOG_STRING("decode","decode audio:entering drain mode failed"));
        }
        else
        {
            errinfo = "err occurred while sending a audio packet";
            FFMPEG_RET_ERR_HANDLE(avcodec_send_packet(video_codec_context,tempPacket),logger,FFMPEG_MAKE_ERR_LOG_STRING2("decode",errinfo));  
            decoded_video_packet_num++;
        }
        av_packet_free(&tempPacket);
        //get one decoded frame:
        ret = avcodec_receive_frame(video_codec_context,tempFrame);
        if(ret == AVERROR(EAGAIN))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        else if(ret == AVERROR_EOF)
            break;
        else if(!ret)
        {
            //output frame is ready:
            MediaFrame* mediaframe = new MediaFrame();
            mediaframe->InitMediaFrameFromAVFrame(tempFrame,av_q2d(video_stream->time_base),true,framepool);
            decoded_video_queue_access->lock();
            frames->push(mediaframe);
            decoded_video_queue_access->unlock();
            //control decode rate:
            if(frames->size()>60)
            {
                while(frames->size()>40)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
        else
        {
            //error occurred:
            logger->logtofile(FFMPEG_MAKE_ERR_LOG_STRING("get_decode_data","decoding video: receive frame error occurred"),LOG_LEVEL_ERROR);   
        }     
    }
    av_frame_free(&tempFrame);
}
MediaFrame* FFmpegCodec::getframe(std::queue<MediaFrame*>* decoded_frames,bool VideoOrAudio)
{
    std::mutex* temp_access = VideoOrAudio?this->decoded_video_queue_access:this->decoded_audio_queue_access;
    temp_access->lock();
    MediaFrame* tempFrame  = NULL;
    while (!decoded_frames->size())
    {
        //no decoded frames yet,unlock a while:
        temp_access->unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        temp_access->lock();
    }
    tempFrame = decoded_frames->front();
    decoded_frames->pop();
    temp_access->unlock();
    return tempFrame;
}
AVPacket* FFmpegCodec::getpacket(std::queue<AVPacket*> *need_to_decode_packet,bool VideoOrAudio)//get packets needed to decode in a thread-save way
{
    std::mutex* temp_access = VideoOrAudio?this->need_to_decode_audio_queue_access:this->need_to_decode_video_queue_access;
    temp_access->lock();
    AVPacket* tempPacket  = NULL;
    while (!need_to_decode_packet->size())
    {
        //no decoded frames yet,unlock a while:
        temp_access->unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        temp_access->lock();
    }
    tempPacket = need_to_decode_packet->front();
    need_to_decode_packet->pop();
    temp_access->unlock();
    return tempPacket;
}
#endif // !FFMPEG_CODEC_H_PLUS