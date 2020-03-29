#ifndef OPENGL_SHADER_H_PLUS
#define OPENGL_SHADER_H_PLUS
#include "opengl_header.h"
#include "opengl_err_handle.h"
#include "../../../common/filelog.h"
#include <string>
class GLShader
{
private:
    std::string VertexShader;
    std::string FragmentShader;
    unsigned int id_VertexShader = -1;
    unsigned int id_FragmentShader = -1;
    unsigned int id_ShaderProgram = -1;
private:
    FileLog* logger;
public:
    GLShader(){logger = new FileLog("GLShader_log");}
    void use(){glUseProgram(id_ShaderProgram);}
    void setBool(const char* name,bool value)const{glUniform1i(glGetUniformLocation(id_ShaderProgram,name),value);}
    void setInt(const char* name,int value)const{glUniform1i(glGetUniformLocation(id_ShaderProgram,name),value);}
    void setFloat(const char* name,float value)const{glUniform1f(glGetUniformLocation(id_ShaderProgram,name),value);}
public:
    bool CompileShaders(const char* path_vs,const char* path_fs);
};
bool GLShader::CompileShaders(const char* path_vs,const char* path_fs)
{
    FILE* file_vs = NULL;
    FILE* file_fs = NULL;
    int n_read = 0;
    char tempstr1[1024]={0};
    char tempstr2[1024]={0};
    VertexShader = "";
    FragmentShader = "";
    //read vertext shader from file
    file_vs = fopen(path_vs,"rt");
    file_fs = fopen(path_fs,"rt");
    while(!feof(file_vs))
    {
        n_read = fread(tempstr1,1,1024,file_vs);
        if(n_read) VertexShader+=tempstr1;
    }
    while(!feof(file_fs))
    {
        n_read = fread(tempstr2,1,1024,file_fs);
        if(n_read) FragmentShader+=tempstr2;
    }
    //compile shaders:
    const char* vs = this->VertexShader.c_str();
    const char* fs = this->FragmentShader.c_str();
    id_VertexShader = glCreateShader(GL_VERTEX_SHADER);
    id_FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(id_VertexShader,1,&vs,NULL);
    glShaderSource(id_FragmentShader,1,&fs,NULL);
    glCompileShader(id_VertexShader);
    //handle err:
    {
        int success;
        char infolog[512]={0};
        glGetShaderiv(id_VertexShader,GL_COMPILE_STATUS,&success);
        if(!success)
        {
            glGetShaderInfoLog(id_VertexShader,512,NULL,infolog);
            logger->logtofile(OPENGL_MAKE_ERR_LOG_STRING2("CompileShaders",infolog),LOG_LEVEL_ERROR);
            return false;
        }
    }
    glCompileShader(id_FragmentShader);
    //handle err:
    {
        int success;
        char infolog[512]={0};
        glGetShaderiv(id_FragmentShader,GL_COMPILE_STATUS,&success);
        if(!success)
        {
            glGetShaderInfoLog(id_FragmentShader,512,NULL,infolog);
            logger->logtofile(OPENGL_MAKE_ERR_LOG_STRING2("CompileShaders",infolog),LOG_LEVEL_ERROR);
            return false;
        }
    }
    //link shaders:
    id_ShaderProgram = glCreateProgram();
    glAttachShader(id_ShaderProgram,id_VertexShader);
    glAttachShader(id_ShaderProgram,id_FragmentShader);
    glLinkProgram(id_ShaderProgram);
    //handle err:
    {
        int success;
        char infolog[512];
        glGetProgramiv(id_ShaderProgram,GL_COMPILE_STATUS,&success);
        if(!success)
        {
            glGetShaderInfoLog(id_ShaderProgram,512,NULL,infolog);
            logger->logtofile(OPENGL_MAKE_ERR_LOG_STRING2("CompileShaders",infolog),LOG_LEVEL_ERROR);
            return false;
        }
    }
    //delete shaders, because they are already link to a shader program:
    glDeleteShader(id_VertexShader);
    glDeleteShader(id_FragmentShader);

    return true;
}
#endif // !OPENGL_SHADER_H_PLUS