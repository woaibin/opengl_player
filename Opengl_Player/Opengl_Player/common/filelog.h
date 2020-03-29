#include <stdio.h>
#include <string.h>
#ifndef FILELOG_H
#define FILELOG_H
#define LOG_TYPE int
#define LOG_LEVEL_INFO 0
#define LOG_LEVEL_DEBUG 1 
#define LOG_LEVEL_ERROR 2
class FileLog
{
private:
    FILE *file;
public:
    FileLog(const char* filename);
    bool logtofile(const char* log,LOG_TYPE log_type);
};
FileLog::FileLog(const char* filename)
{
    this->file = fopen(filename,"wt");
}
bool FileLog::logtofile(const char* log,LOG_TYPE log_type)
{
    char log_level_string[255]={0};
    switch (log_type)
    {
    case LOG_LEVEL_INFO:
        strcpy(log_level_string,"\tINFO");
        break;
    case LOG_LEVEL_DEBUG:
        strcpy(log_level_string,"\tDEBUG");
        break;
    case LOG_LEVEL_ERROR:
        strcpy(log_level_string,"\tERROR");
        break;
    default:
        return false;
    }
    fprintf(this->file,"%s%s\n",log,log_level_string);
}
#endif // !FILELOG_H