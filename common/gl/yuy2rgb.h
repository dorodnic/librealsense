#pragma once

#include <librealsense2/rs.hpp>
#include "fbo.h"

#include <memory>

namespace rs2
{
    class visualizer_2d;

    class yuy2rgb : public processing_block
    {
    public:
        yuy2rgb();

        void init();

        ~yuy2rgb();
    private:
        void on_frame(frame f, frame_source& src);

        stream_profile _input_profile;
        stream_profile _output_profile;

        int _width, _height;

        uint32_t _yuy_texture;
        uint32_t _output_rgb;
        std::unique_ptr<fbo> _fbo;
        std::unique_ptr<visualizer_2d> _viz;
        bool _was_init = false;
    };
}