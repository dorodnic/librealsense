// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include <map>
#include <vector>

#include "proc/synthetic-stream.h"
#include "synthetic-stream-gl.h"

#include <librealsense2/rs.hpp>
#include "opengl3.h"

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
            yuy2rgb(std::shared_ptr<librealsense::gl::context> ctx);

            rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;
        private:
            void on_frame(frame f, frame_source& src);

            rs2::stream_profile _input_profile;
            rs2::stream_profile _output_profile;

            int _width, _height;

            lazy<rs2::visualizer_2d> _viz;

            std::shared_ptr<gl::context> _ctx;
        };
    }
}
