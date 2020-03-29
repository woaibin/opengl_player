#include <alsa/asoundlib.h>
#include <string.h>
#include "../../../common/filelog.h"
#include "ALSA_err_handle.h"
#include <mutex>

class ALSARender
{
public:
    typedef struct
    {
        unsigned int sample_rate;
        int num_period;
        snd_pcm_uframes_t periodsize;//in frames
        char* pcm_name;
        snd_pcm_access_t accesstype;
        snd_pcm_format_t format;
        int num_channels;
    }ALSA_OPT,*PALSA_OPT;
private:
    snd_pcm_t *pcmhandle = NULL;
    snd_pcm_stream_t pcmstream;
    snd_pcm_hw_params_t *hwparams = NULL;
    FileLog* logger = NULL;
    PALSA_OPT opt = NULL;
private:
    std::mutex clock_access;
    double clock = 0;//in seconds.
public:
    ALSARender(){logger = new FileLog("ALSARender_log");}
public:
    bool InitRender(ALSARender::PALSA_OPT p_alsa_opt);
    bool Render(unsigned char* data,int nb_frames);
    double GetClock();
    int Framesize2Bytes(int framesize,snd_pcm_format_t format);
    int Samplerate2Bitrate(int samplerate,int n_channel,snd_pcm_format_t format);
};

bool ALSARender::InitRender(ALSARender::PALSA_OPT p_alsa_opt)
{
    this->pcmstream = SND_PCM_STREAM_PLAYBACK;
    snd_pcm_hw_params_alloca(&this->hwparams);
    ALSA_RET_ERROR_HANDLE(snd_pcm_open(&this->pcmhandle,p_alsa_opt->pcm_name,pcmstream,0),logger,"opening pcm device failed!");
    ALSA_RET_ERROR_HANDLE(snd_pcm_hw_params_any(this->pcmhandle,this->hwparams),logger,"can not configure this pcm device!");
    ALSA_RET_ERROR_HANDLE(snd_pcm_hw_params_set_access(this->pcmhandle,this->hwparams,p_alsa_opt->accesstype),logger,"setting access type failed!");
    ALSA_RET_ERROR_HANDLE(snd_pcm_hw_params_set_format(this->pcmhandle,this->hwparams,p_alsa_opt->format),logger,"setting format failed!");
    ALSA_RET_ERROR_HANDLE(snd_pcm_hw_params_set_rate_near(this->pcmhandle,this->hwparams,&p_alsa_opt->sample_rate,0),logger,"setting sample rate failed!");
    ALSA_RET_ERROR_HANDLE(snd_pcm_hw_params_set_channels(this->pcmhandle,this->hwparams,p_alsa_opt->num_channels),logger,"setting channel failed!");
    ALSA_RET_ERROR_HANDLE(snd_pcm_hw_params_set_periods(this->pcmhandle,this->hwparams,2,0),logger,"setting the number of periods failed!");
    if(p_alsa_opt->format == SND_PCM_FORMAT_S16_LE)//only support this format now
        ALSA_RET_ERROR_HANDLE(snd_pcm_hw_params_set_buffer_size(this->pcmhandle,this->hwparams,2*p_alsa_opt->periodsize),logger,"setting the buffersize failed!")
    else
    {
        logger->logtofile("format not supported!",LOG_LEVEL_ERROR);
    }
    ALSA_RET_ERROR_HANDLE(snd_pcm_hw_params(this->pcmhandle,this->hwparams),logger,"setting hwparams to pcmhandle failed!");
    this->opt = p_alsa_opt;
    return true;
}

bool ALSARender::Render(unsigned char* data,int nb_frames)
{
    int ret;
    if(nb_frames<=this->opt->periodsize)
    {
        //smaller than periodsize,just write once:
        while ((ret = snd_pcm_writei(this->pcmhandle, data, nb_frames)) < 0) 
        {
            snd_pcm_prepare(this->pcmhandle);
            printf("<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n");
            logger->logtofile("<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n",LOG_LEVEL_ERROR);
        }
        //update clock:
        clock_access.lock();
        clock+=((double)nb_frames/this->opt->sample_rate);
        clock_access.unlock();
    }
    else
    {
        //larger than periodsize, write multiple times:
        int times = nb_frames/this->opt->periodsize;
        int remain = nb_frames % this->opt->periodsize;
        for(int i=0;i<times;i++)
        {
            while ((ret = snd_pcm_writei(this->pcmhandle, data, this->opt->periodsize)) < 0) 
            {
                snd_pcm_prepare(this->pcmhandle);
                printf("<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n");
                logger->logtofile("<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n",LOG_LEVEL_ERROR);
            }
            data+=snd_pcm_frames_to_bytes(this->pcmhandle,this->opt->periodsize);
            //update clock:
            clock_access.lock();
            clock+=((double)this->opt->periodsize/this->opt->sample_rate);
            clock_access.unlock();
        }
        if(remain)
        {
            while ((ret = snd_pcm_writei(this->pcmhandle, data, snd_pcm_bytes_to_frames(this->pcmhandle,remain))) < 0) 
            {
                snd_pcm_prepare(this->pcmhandle);
                printf("<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n");
                logger->logtofile("<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n",LOG_LEVEL_ERROR);
            }
            //update clock:
            clock_access.lock();
            clock+=((double)remain/this->opt->sample_rate);
            clock_access.unlock();
        }
    }
    return true;
}

int ALSARender::Samplerate2Bitrate(int samplerate,int n_channel,snd_pcm_format_t format)
{
    if(format == SND_PCM_FORMAT_S16_LE)
        return samplerate*n_channel*2;
    else
        return 0;
}

int ALSARender::Framesize2Bytes(int framesize,snd_pcm_format_t format)
{
    if(format == SND_PCM_FORMAT_S16_LE)
        return framesize*2;
    else
        return 0;
}

double ALSARender::GetClock()
{
    clock_access.lock();
    double temp_clock = this->clock;
    clock_access.unlock();
    return temp_clock;
}