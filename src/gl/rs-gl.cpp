// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#include "api.h"
#include "synthetic-stream-gl.h"
#include "yuy2rgb-gl.h"
#include "pointcloud-gl.h"
#include "../include/librealsense2/h/rs_types.h"
#include "../include/librealsense2-gl/rs_processing_gl.h"
#include <assert.h>

#include <GLFW/glfw3.h>


////////////////////////
// API implementation //
////////////////////////

using namespace librealsense;

struct rs2_gl_context
{
    std::shared_ptr<gl::context> ctx;
};

namespace librealsense
{
    RS2_ENUM_HELPERS(rs2_gl_extension, GL_EXTENSION)

    const char* get_string(rs2_gl_extension value)
    {
        switch (value)
        {
        case RS2_GL_EXTENSION_VIDEO_FRAME: return "GL Video Frame";
        default: assert(!is_valid(value)); return UNKNOWN_VALUE;
        }
    }
}

const char* rs2_frame_metadata_to_string(rs2_gl_extension ex) { return librealsense::get_string(ex); }

rs2_processing_block* rs2_gl_create_yuy_to_rgb(rs2_gl_context* ctx, rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::gl::yuy2rgb>(ctx->ctx);

    auto res = new rs2_processing_block{ block };

    auto res2 = (rs2_options*)res;

    return res;
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

unsigned int rs2_gl_frame_get_texture_id(const rs2_frame* frame_ref, unsigned int id, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    VALIDATE_RANGE(id, 0, MAX_TEXTURES - 1);
    
    auto gpu = dynamic_cast<gl::gpu_addon_interface*>((frame_interface*)frame_ref);
    if (!gpu) throw std::runtime_error("Expected GPU frame!");

    uint32_t res;
    if (!gpu->get_gpu_section().input_texture(id, &res)) 
        throw std::runtime_error("Texture not ready!");

    return res;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

int rs2_gl_is_frame_extendable_to(const rs2_frame* f, rs2_gl_extension extension_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(f);
    VALIDATE_ENUM(extension_type);

    switch (extension_type)
    {
    case RS2_GL_EXTENSION_VIDEO_FRAME: return dynamic_cast<gl::gpu_addon_interface*>((frame_interface*)f) ? 1 : 0;
    default: return 0;
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(0, f, extension_type)

rs2_processing_block* rs2_gl_create_pointcloud(rs2_gl_context* ctx, rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::gl::pointcloud_gl>(ctx->ctx);

    return new rs2_processing_block { block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

void rs2_gl_update_all(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);

    gl::main_thread_dispatcher::instance().update();
}
NOEXCEPT_RETURN(, api_version)

void rs2_gl_stop_all(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);

    gl::main_thread_dispatcher::instance().stop();
}
NOEXCEPT_RETURN(, api_version)

rs2_gl_context* rs2_gl_create_context(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    glfw_binding binding{
        &glfwInit,
        &glfwWindowHint,
        &glfwCreateWindow,
        &glfwDestroyWindow,
        &glfwMakeContextCurrent,
        &glfwGetCurrentContext,
        &glfwSwapInterval,
        &glfwGetProcAddress
    };
    return new rs2_gl_context { std::make_shared<gl::context>(nullptr, binding) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version)

rs2_gl_context* rs2_gl_create_shared_context(int api_version, GLFWwindow* share_with, glfw_binding binding, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);
    return new rs2_gl_context{ std::make_shared<gl::context>(share_with, binding) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version, share_with)

void rs2_gl_delete_context(rs2_gl_context* context) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(context);
    delete context;
}
NOEXCEPT_RETURN(, context)