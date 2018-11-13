/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2017 Intel Corporation. All Rights Reserved. */

/** \file rs_processing_gl.h
* \brief
* Exposes RealSense processing-block functionality for GPU for C compilers
*/


#ifndef LIBREALSENSE_RS2_PROCESSING_GL_H
#define LIBREALSENSE_RS2_PROCESSING_GL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "librealsense2/rs.h"

typedef enum rs2_gl_extension
{
    RS2_GL_EXTENSION_VIDEO_FRAME,
    RS2_GL_EXTENSION_COUNT
} rs2_gl_extension;
const char* rs2_gl_extension_to_string(rs2_extension type);

typedef struct rs2_gl_context rs2_gl_context;
typedef struct GLFWwindow GLFWwindow;
typedef struct GLFWmonitor GLFWmonitor;

typedef int(*glfwInitFun)(void);
typedef void(*glfwWindowHintFun)(int, int);
typedef GLFWwindow*(*glfwCreateWindowFun)(int, int, const char*, GLFWmonitor*, GLFWwindow*);
typedef void(*glfwDestroyWindowFun)(GLFWwindow*);
typedef void(*glfwMakeContextCurrentFun)(GLFWwindow*);
typedef GLFWwindow*(*glfwGetCurrentContextFun)(void);
typedef void(*glfwSwapIntervalFun)(int);
typedef void(*GLFWglproc)(void);
typedef GLFWglproc(*glfwGetProcAddressFun)(const char*);

struct glfw_binding
{
    glfwInitFun glfwInit;
    glfwWindowHintFun glfwWindowHint;
    glfwCreateWindowFun glfwCreateWindow;
    glfwDestroyWindowFun glfwDestroyWindow ;
    glfwMakeContextCurrentFun glfwMakeContextCurrent;
    glfwGetCurrentContextFun glfwGetCurrentContext;
    glfwSwapIntervalFun glfwSwapInterval;
    glfwGetProcAddressFun glfwGetProcAddress;
};

/**
* Creates a processing block that can efficiently convert YUY image format to RGB variants
* This is specifically useful for rendering the RGB frame to the screen (since the output is ready for rendering on the GPU)
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_processing_block* rs2_gl_create_yuy_to_rgb(rs2_gl_context* context, rs2_error** error);


int rs2_gl_is_frame_extendable_to(const rs2_frame* f, rs2_gl_extension extension_type, rs2_error** error);

unsigned int rs2_gl_frame_get_texture_id(const rs2_frame* f, unsigned int id, rs2_error** error);

/**
* Creates Point-Cloud processing block. This block accepts depth frames and outputs Points frames
* In addition, given non-depth frame, the block will align texture coordinate to the non-depth stream
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_processing_block* rs2_gl_create_pointcloud(rs2_gl_context* context, rs2_error** error);

void rs2_gl_update_all(int api_version, rs2_error** error);

void rs2_gl_stop_all(int api_version, rs2_error** error);

rs2_gl_context* rs2_gl_create_context(int api_version, rs2_error** error);

rs2_gl_context* rs2_gl_create_shared_context(int api_version, GLFWwindow* share_with, glfw_binding bindings, rs2_error** error);

void rs2_gl_delete_context(rs2_gl_context* context);

#ifdef __cplusplus
}
#endif
#endif
