// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once
#include "proc/pointcloud.h"

namespace librealsense
{
    class pointcloud_gl : public pointcloud
    {
    public:
        pointcloud_gl();
    private:
        const float3 * depth_to_points(uint8_t* image, 
            const rs2_intrinsics &depth_intrinsics, 
            const uint16_t * depth_image, 
            float depth_scale) override;
        void get_texture_map(const float3* points,
            const unsigned int width,
            const unsigned int height,
            const rs2_intrinsics &other_intrinsics,
            const rs2_extrinsics& extr,
            float2* tex_ptr,
            float2* pixels_ptr) override;
    };
}
