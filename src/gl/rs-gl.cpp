// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#include "api.h"
#include "synthetic-stream-gl.h"
#include "yuy2rgb-gl.h"
#include "pointcloud-gl.h"
#include "../include/librealsense2/h/rs_types.h"
#include "../include/librealsense2-gl/rs_processing_gl.h"
#include <assert.h>

////////////////////////
// API implementation //
////////////////////////

using namespace librealsense;

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

rs2_processing_block* rs2_gl_create_yuy_to_rgb(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::gl::yuy2rgb>();

    auto res = new rs2_processing_block{ block };

    auto res2 = (rs2_options*)res;

    return res;
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

unsigned int rs2_gl_frame_get_texture_id(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    
    auto gpu = dynamic_cast<gl::gpu_addon_interface*>((frame_interface*)frame_ref);
    if (!gpu) throw std::runtime_error("Expected GPU frame!");

    return gpu->get_gpu_section().texture;
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

rs2_processing_block* rs2_gl_create_pointcloud(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::pointcloud_gl>();

    return new rs2_processing_block { block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)