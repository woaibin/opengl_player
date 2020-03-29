#include "../render/video/opengl/opengl_render.hpp"
#include "../codec/ffmpeg/ffmpeg_codec.hpp"
#include "../render/audio/Linux/ALSARender.hpp"
#include <math.h>
#define AV_NOSYNC_THRSHOLD 10.0
#define AV_SYNC_THRESHOLD 0.01
class XKPlayer
{
private:
    FFmpegCodec codec;
    OpenGLRender video_render;
    ALSARender audio_render;
private:
    FileLog* logger;
    std::thread *audio_thread;//render audio 
    std::thread *video_thread;//deal with avsync and update render texture.
private:
    int width = 0,height=0;
    std::queue<MediaFrame*> decoded_video_frames;
    std::queue<MediaFrame*> decoded_audio_frames;
    MediaFrame* current_video_frame;
    MediaFrame* current_audio_frame;
    double delay_video2audio;
public:
    bool InitPlayer();
    bool OpenMediaFile(const char* path);
    void Play();
private:
    bool audio_thread_func();
    double get_master_clock() {return audio_render.GetClock();}
    bool video_thread_func();
};
bool XKPlayer::InitPlayer()
{
    logger = new FileLog("XKPlayer_log");
    //init codec:
    FFmpegCodec::CODEC_OPT *opt = new FFmpegCodec::CODEC_OPT;
    opt->decorenc=true;
    opt->isaudio=true;
    opt->isvideo=true;
    FFMPEG_RET_ERR_HANDLE(this->codec.initcodec(opt),logger,"init codec failed!");
    return true;
}
bool XKPlayer::OpenMediaFile(const char* path)
{
    FFMPEG_RET_ERR_HANDLE(this->codec.openinput(path),logger,"open media file failed!");
    //init video renderer:
    this->codec.getsize(&this->width,&this->height);
    OPENGL_RET_ERROR_HANDLE(this->video_render.opengl_init(this->width,this->height),logger,"init video render failed!");
    //init audio renderer:
    ALSARender::PALSA_OPT p_opt = new ALSARender::ALSA_OPT();
    AVCodecContext *AudioCodecContext = codec.GetAudioCodecContext();
    p_opt->accesstype = SND_PCM_ACCESS_RW_INTERLEAVED;
    p_opt->format = SND_PCM_FORMAT_S16_LE;
    p_opt->num_channels = AudioCodecContext->channels;
    p_opt->num_period = 2;
    p_opt->pcm_name = "plughw:1,0";
    p_opt->periodsize = 1024;//1024 frames
    p_opt->sample_rate = AudioCodecContext->sample_rate;
    ALSA_RET_ERROR_HANDLE(this->audio_render.InitRender(p_opt),logger,"init audio render failed");
    return true;
}
void XKPlayer::Play()
{
    this->codec.start_decode(&decoded_video_frames,&decoded_audio_frames);
    audio_thread = new std::thread(&XKPlayer::audio_thread_func,this);
    video_thread = new std::thread(&XKPlayer::video_thread_func,this);
    //render video in main thread:
    video_render.renderloop();
}
bool XKPlayer::audio_thread_func()
{
    while(1)
    {
        //get one frame:
        MediaFrame* tempFrame = this->codec.getframe(&decoded_audio_frames,false);
        //render:
        audio_render.Render(tempFrame->getdata(),tempFrame->getnbsamples());
        //release:
        delete tempFrame;
    }
    return true;
}
bool XKPlayer::video_thread_func()
{
    while(1)
    {
        double temp_master_clock = 0;
        double startTime,overTime;//these two variables is to calculate the waste of our avsync process,and delay will cut this part of waste.
        startTime = overTime = av_gettime()/1000000.0;
        //get one frame:
        MediaFrame* tempFrame = this->codec.getframe(&decoded_video_frames,true);
        //update clock:
        double current_video_clock = tempFrame->getclock();
        delay_video2audio = tempFrame->getDuration();
        temp_master_clock = get_master_clock();
        double diff_av = current_video_clock-temp_master_clock;
        //check if the delay is less than AV_SYNC_THRSHOLD.if is,that's too small for it,set the sync_threshold to AV_SYNC_THRESHOLD,otherwise set it to a frame duration.
        double sync_threshold = delay_video2audio<AV_SYNC_THRESHOLD?AV_SYNC_THRESHOLD:delay_video2audio;
        //check if diff is in the sync scope:
        if(abs(diff_av)<AV_NOSYNC_THRSHOLD)
        {
            if(diff_av<-sync_threshold)
            {
                //video is behind of audio at a frame duration time,let video chase audio,which is called "jump frame"
                delay_video2audio = 0;
            }
            else if(diff_av>=sync_threshold)
            {
                //video is ahead of audio at a frame duration,let video wait for audio:
                delay_video2audio*=2;
            }
        }
        else
            logger->logtofile("diff error occurred!",LOG_LEVEL_ERROR);
        overTime = av_gettime()/1000000.0;
        delay_video2audio-=(overTime-startTime);
        //start delaying:
        std::this_thread::sleep_for(std::chrono::duration<double>(delay_video2audio));
        //update video frame:
        video_render.UpdateRenderFrame(tempFrame);
    }
    return true;
}