#ifndef OPENGL_RENDER_H_PLUS
#define OPNEGL_RENDER_H_PLUS
#include "opengl_header.h"
#include "opengl_err_handle.h"
#include "../../../common/filelog.h"
#include "../../../common/interfaces.hpp"
#include "opengl_shader.hpp"
#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600
class OpenGLRender
{
private:
    float vertices[20] = {
        /*--pos--*/             /*--texcoord--*/
        -1.0f,1.0f,0.0f,        0.0f,1.0f,//left top
        1.0f,1.0f,0.0f,         1.0f,1.0f,//right top
        -1.0f,-1.0f,0.0f,       0.0f,0.0f,//left down
        1.0f,-1.0f,0.0f,        1.0f,0.0f//right down
    };
    unsigned int indices[6] = {
        0,2,3,
        0,1,3
    };
    unsigned int id_VBO = -1;
    unsigned int id_VAO = -1;
    unsigned int id_EBO = -1;
private:
    MediaFrame* renderframe = NULL;
    std::mutex frame_access;
    GLFWwindow* window;
    GLShader shader;
    int width=0,height=0;
private:
    FileLog* logger = NULL;
public:
    OpenGLRender(){logger = new FileLog("OpenGLRender_log");}
public:
    bool opengl_init(int width,int height);
    void UpdateRenderFrame(MediaFrame* frame);//before rendering,you should call it once to init the texture data.
    void renderloop();
private:
    bool init_window();
    bool init_resources();
};
bool OpenGLRender::opengl_init(int width,int height)
{
    this->width = width;
    this->height =height;
    init_window();
    init_resources();
    shader.CompileShaders("../render/video/opengl/shaders/vertexShader.vs","../render/video/opengl/shaders/fragmentShader.fs");//change temporarily
    return true;
}
bool OpenGLRender::init_window()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

    // glfw window creation
    // --------------------
    window = glfwCreateWindow(this->width, this->height, "OpenGLPlayer", NULL, NULL);
    if (window == NULL)
    {
        logger->logtofile(OPENGL_MAKE_ERR_LOG_STRING("init_window","Failed to create GLFW window"),LOG_LEVEL_ERROR);
        return false;
    }
    glfwMakeContextCurrent(window);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        logger->logtofile(OPENGL_MAKE_ERR_LOG_STRING("init_window","Failed to initialize GLAD"),LOG_LEVEL_ERROR);
        return false;
    }
    return true;
}
bool OpenGLRender::init_resources()
{
    glGenBuffers(1,&id_VBO);
    glGenBuffers(1,&id_EBO);
    glGenVertexArrays(1,&id_VAO);
    glBindVertexArray(id_VAO);
    glBindBuffer(GL_ARRAY_BUFFER,id_VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,id_EBO);
    //Update data:
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    return true;
}
void OpenGLRender::renderloop()
{
    while(!glfwWindowShouldClose(this->window))
    {
        //check if frame is NULL:
        if(!renderframe) continue;
        //render:
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        unsigned int id_Texture;glGenTextures(1,&id_Texture);
        glBindTexture(GL_TEXTURE_2D,id_Texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        frame_access.lock();
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,renderframe->getwidth(),renderframe->getheight(),0,GL_RGB,GL_UNSIGNED_BYTE,renderframe->getdata());
        frame_access.unlock();
        shader.use();shader.setInt("tex",0);
        glActiveTexture(GL_TEXTURE0);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
        glDeleteTextures(1,&id_Texture);
        glfwSwapBuffers(window);
        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
}
void OpenGLRender::UpdateRenderFrame(MediaFrame* frame)
{
    //free current frame:
    frame_access.lock();
    if(this->renderframe)
        delete renderframe;
    this->renderframe = frame;
    frame_access.unlock();
}
#endif // !OPENGL_RENDER_H_PLUS