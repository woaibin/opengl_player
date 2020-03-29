#ifndef ALSA_ERR_HANDLE_H
#define ALSA_ERR_HANDLE_H
#include "../../../common/err_handle.h"
#define ALSA_RET_ERROR_HANDLE(ret,logger,errinfo) RETURN_FALSE_IF_FAILED((ret<0),logger,errinfo)
#endif // !ALSA_ERR_HANDLE_H