# opengl_player
Linux下的opengl播放器

## 特点：
1. 多线程解码与渲染
2. 采用内存池管理原始视频帧与音频帧
3. 音视频同步采用视频同步到音频的方式
4. 音频渲染采用Linux下的ALSA框架
5. 视频渲染采用Opengl

## BUG:
1. 播放rmvb文件时会出现时间戳获取错误的问题，导致视频播放加快。
