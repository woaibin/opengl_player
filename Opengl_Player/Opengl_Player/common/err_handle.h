#include "filelog.h"
#ifndef ERR_HANDLE_H
#define ERR_HANDLE_H
#define RETURN_FALSE_IF_FAILED(condition,logger,errinfo) \
if(condition)\
{\
    logger->logtofile(errinfo,LOG_LEVEL_ERROR); \
    return false;\
}
#endif // !ERR_HANDLE_H
