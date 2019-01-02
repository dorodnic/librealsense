// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"
#include "../include/librealsense2-gl/rs_processing_gl.hpp"

#include "proc/synthetic-stream.h"
#include "upload.h"
#include "option.h"
#include "context.h"

#define NOMINMAX

#include <glad/glad.h>

#include <iostream>

#include <chrono>
#include <strstream>

#include "synthetic-stream-gl.h"

namespace librealsense
{
    namespace gl
    {
        upload::upload()
        {
            _source.add_extension<gl::gpu_video_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);
            initialize();
        }

        upload::~upload()
        {
            perform_gl_action([&]()
            {
                cleanup_gpu_resources();
            }, [] {});
        }

        void upload::cleanup_gpu_resources()
        {

        }
        void upload::create_gpu_resources()
        {

        }

        rs2::frame upload::process_frame(const rs2::frame_source& source, const rs2::frame& f)
        {
            auto res = f;

            if (f.get_profile().format() == RS2_FORMAT_YUYV)
            {
                auto vf = f.as<rs2::video_frame>();
                auto width = vf.get_width();
                auto height = vf.get_height();
                auto new_f = source.allocate_video_frame(f.get_profile(), f,
                    vf.get_bits_per_pixel(), width, height, vf.get_stride_in_bytes(), RS2_EXTENSION_VIDEO_FRAME_GL);

                perform_gl_action([&]()
                {
                    auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)new_f.get());

                    uint32_t output_yuv;
                    gf->get_gpu_section().output_texture(0, &output_yuv, texture_type::UINT16);
                    glBindTexture(GL_TEXTURE_2D, output_yuv);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, f.get_data());
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                    gf->get_gpu_section().set_size(width, height);

                    res = new_f;
                }, []() {
                    
                });
            }

            return res;
        }
    }
}

