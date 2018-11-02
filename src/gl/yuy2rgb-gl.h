// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include <map>
#include <vector>

#include "proc/synthetic-stream.h"

#include <librealsense2/rs.hpp>
#include "fbo.h"

#include <memory>

namespace rs2
{
    class stream_profile;
    class visualizer_2d;
}

namespace librealsense 
{
    namespace gl
    {
        class yuy2rgb : public stream_filter_processing_block
        {
        public:
            yuy2rgb();

            rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;

            void init();

            ~yuy2rgb();
        private:
            void on_frame(frame f, frame_source& src);

            rs2::stream_profile _input_profile;
            rs2::stream_profile _output_profile;

            int _width, _height;

            uint32_t _yuy_texture;
            uint32_t _output_rgb;
            //std::unique_ptr<rs2::fbo> _fbo;
            std::unique_ptr<rs2::visualizer_2d> _viz;
            bool _was_init = false;
        };
    }
}
