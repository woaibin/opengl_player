#ifndef OPENGL_ERR_HANDLE_H
#define OPENGL_ERR_HANDLE_H
#include "../../../common/err_handle.h"
#define OPENGL_RET_ERROR_HANDLE(ret,logger,errinfo) RETURN_FALSE_IF_FAILED((ret<0),logger,errinfo)
#define OPENGL_MAKE_ERR_LOG_STRING(funcname,reason) (funcname "function failed:" reason "!(opengl_render err)")
#define OPENGL_MAKE_ERR_LOG_STRING2(funcname,reason) std::string(std::string(funcname)+std::string("function failed:")+reason+std::string("!(opengl_render err)")).c_str()
#endif // !OPENGL_ERR_HANDLE_H