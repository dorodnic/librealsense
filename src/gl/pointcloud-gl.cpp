// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/rs.hpp"
#include "../include/librealsense2/rsutil.h"

#include "proc/synthetic-stream.h"
#include "environment.h"
#include "proc/occlusion-filter.h"
#include "pointcloud-gl.h"
#include "option.h"
#include "environment.h"
#include "context.h"

#include <iostream>

namespace librealsense
{
    pointcloud_gl::pointcloud_gl() : pointcloud() {}

    const float3* pointcloud_gl::depth_to_points(uint8_t* points, 
            const rs2_intrinsics &depth_intrinsics, 
            const uint16_t * depth_image, 
            float depth_scale)
    {
        
    }

    void pointcloud_gl::get_texture_map(const float3* points,
        const unsigned int width,
        const unsigned int height,
        const rs2_intrinsics &other_intrinsics,
        const rs2_extrinsics& extr,
        float2* tex_ptr,
        float2* pixels_ptr)
    {
        
    }
}
