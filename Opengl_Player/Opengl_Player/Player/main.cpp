// #include "../render/video/opengl/opengl_render.hpp"
// #include "../codec/ffmpeg/ffmpeg_codec.hpp"
// #include "../render/audio/Linux/ALSARender.hpp"
// #include <queue>
// int main(int argc,const char* argv[])
// {
//     const char* Path = argv[1];
//     std::queue<MediaFrame*> decoded_video_frames;
//     std::queue<MediaFrame*> decoded_audio_frames;
//     FFmpegCodec codec;
//     FFmpegCodec::CODEC_OPT opt;
//     opt.decorenc=true;
//     opt.isaudio=true;
//     opt.isvideo=true;
//     codec.initcodec(&opt);
//     codec.openinput(Path);
//     codec.start_decode(&decoded_video_frames,&decoded_audio_frames);
//     //OpenGLRender render;
//     ALSARender audio_render;
//     {
//         ALSARender::PALSA_OPT p_opt = new ALSARender::ALSA_OPT();
//         AVCodecContext *AudioCodecContext = codec.GetAudioCodecContext();
//         p_opt->accesstype = SND_PCM_ACCESS_RW_INTERLEAVED;
//         p_opt->format = SND_PCM_FORMAT_S16_LE;
//         p_opt->num_channels = AudioCodecContext->channels;
//         p_opt->num_period = 2;
//         p_opt->pcm_name = "plughw:1,0";
//         p_opt->periodsize = 1024;//1024 frames
//         p_opt->sample_rate = AudioCodecContext->sample_rate;
//         audio_render.InitRender(p_opt);
//     }
//     MediaFrame* tempFrame = codec.getframe(&decoded_video_frames);
//     while(1)
//     {
//         MediaFrame* tFrame = codec.getframe(&decoded_audio_frames);
//         audio_render.Render(tFrame->getdata(),tFrame->getnbsamples());
//     }
//     // render.UpdateRenderFrame(tempFrame);
//     // render.opengl_init();
//     // render.renderloop();
//     return 0;
// }
#include  "XKPlayer.hpp"
int main(int argc,const char* argv[])
{
    XKPlayer xkplayer;
    xkplayer.InitPlayer();
    xkplayer.OpenMediaFile(argv[1]);
    xkplayer.Play();
    return 0;
}