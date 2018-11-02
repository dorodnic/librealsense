// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/rs.hpp"
#include "../include/librealsense2/rsutil.h"

#include "synthetic-stream-gl.h"
#include "environment.h"
#include "proc/occlusion-filter.h"
#include "pointcloud-gl.h"
#include "option.h"
#include "environment.h"
#include "context.h"

#include <iostream>

namespace librealsense
{
    pointcloud_gl::pointcloud_gl() : pointcloud() 
    {
        _source.add_extension<gl::gpu_points_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);
    }

    const float3* pointcloud_gl::depth_to_points(
            rs2::points output,
            uint8_t* points, 
            const rs2_intrinsics &depth_intrinsics, 
            const uint16_t * depth_image, 
            float depth_scale)
    {
        
    }

    void pointcloud_gl::get_texture_map(
        rs2::points output,
        const float3* points,
        const unsigned int width,
        const unsigned int height,
        const rs2_intrinsics &other_intrinsics,
        const rs2_extrinsics& extr,
        float2* tex_ptr,
        float2* pixels_ptr)
    {
        
    }

    rs2::points pointcloud_gl::allocate_points(
        const rs2::frame_source& source, 
        const rs2::frame& f)
    {
        auto prof = std::dynamic_pointer_cast<librealsense::stream_profile_interface>(
            _output_stream.get()->profile->shared_from_this());
        auto frame_ref = _source_wrapper.allocate_points(prof, (frame_interface*)f.get(),
            RS2_EXTENSION_VIDEO_FRAME_GL);
        rs2::frame res { (rs2_frame*)frame_ref };
        return res.as<rs2::points>();
    }
}
