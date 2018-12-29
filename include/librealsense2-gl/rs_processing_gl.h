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

typedef enum rs2_gl_matrix_type
{
    RS2_GL_MATRIX_TRANSFORMATION,
    RS2_GL_MATRIX_PROJECTION,
    RS2_GL_MATRIX_CAMERA,
    RS2_GL_MATRIX_COUNT
} rs2_gl_matrix_type;
const char* rs2_gl_matrix_type_to_string(rs2_gl_matrix_type type);

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
rs2_processing_block* rs2_gl_create_yuy_decoder(int api_version, rs2_error** error);

void rs2_gl_set_matrix(rs2_processing_block* block, rs2_gl_matrix_type type, float* m4x4, rs2_error** error);

int rs2_gl_is_frame_extendable_to(const rs2_frame* f, rs2_gl_extension extension_type, rs2_error** error);

unsigned int rs2_gl_frame_get_texture_id(const rs2_frame* f, unsigned int id, rs2_error** error);

/**
 * Camera renderer is a rendering block (meaning it has to be called within the main OpenGL rendering context)
 * that will render the camera model of the frame provided to it
 */
rs2_processing_block* rs2_gl_create_camera_renderer(int api_version, rs2_error** error);

/**
 * Pointcloud renderer will render texture pointcloud as either points
 * or connected polygons
 */
rs2_processing_block* rs2_gl_create_pointcloud_renderer(int api_version, rs2_error** error);

/**
* Creates Point-Cloud processing block. This block accepts depth frames and outputs Points frames
* In addition, given non-depth frame, the block will align texture coordinate to the non-depth stream
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_processing_block* rs2_gl_create_pointcloud(int api_version, rs2_error** error);

/** 
 * Initialize rendering pipeline. This function must be called before executing
 * any of the rendering blocks.
 * Rendering blocks do not handle threading, and assume all calls (including init / shutdown)
 * Until initialized, rendering blocks will do nothing (function as bypass filters)
 * are serialized and coming from a single rendering thread
 * \param[in] use_glsl  On modern GPUs you can get slightly better performance using GLSL
 *                      However, this assumes the current rendering context is v3+
 *                      Setting use_glsl to false will use legacy OpenGL calls
 *                      This in turn assumes the rendering context is either version < 3, or is a compatibility context
 */
void rs2_gl_init_rendering(int api_version, int use_glsl, rs2_error** error);

/**
 * Initialize processing pipeline. This function allows GL processing blocks to 
 * run on the GPU. Until initialized, all GL processing blocks will fall back
 * to their CPU versions. 
 * When initializing using this method, texture sharing is not available.
 */
void rs2_gl_init_processing(int api_version, int use_glsl, rs2_error** error);

void rs2_gl_init_processing_glfw(int api_version, GLFWwindow* share_with, 
                                 glfw_binding bindings, int use_glsl, rs2_error** error);

void rs2_gl_shutdown_rendering(int api_version, rs2_error** error);

void rs2_gl_shutdown_processing(int api_version, rs2_error** error);

#ifdef __cplusplus
}
#endif
#endif
