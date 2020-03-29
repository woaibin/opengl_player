#ifndef FFMPEG_ERR_H
#define FFMPEG_ERR_H
#include "../../common/err_handle.h"
#define FFMPEG_RET_ERR_HANDLE(ret,logger,errinfo) RETURN_FALSE_IF_FAILED((ret<0),logger,errinfo)
#define FFMPEG_OBJ_ERR_HANDLE(obj,logger,errinfo) RETURN_FALSE_IF_FAILED((!obj),logger,errinfo)
#define FFMPEG_MAKE_ERR_LOG_STRING(funcname,reason) (funcname "function failed:" reason "!(ffmpeg_codec err)")
#define FFMPEG_MAKE_ERR_LOG_STRING2(funcname,reason) std::string(std::string(funcname)+std::string("function failed:")+reason+std::string("!(ffmpeg_codec err)")).c_str()
#endif // !FFMPEG_ERR_H
