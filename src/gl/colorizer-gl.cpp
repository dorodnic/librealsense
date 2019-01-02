// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"
#include "../include/librealsense2-gl/rs_processing_gl.hpp"

#include "proc/synthetic-stream.h"
#include "colorizer-gl.h"
#include "option.h"

#include "tiny-profiler.h"

#define NOMINMAX

#include <glad/glad.h>

#include <iostream>

#include <chrono>
#include <strstream>

#include "synthetic-stream-gl.h"


using namespace rs2;

namespace librealsense
{
    namespace gl
    {
        void colorizer::cleanup_gpu_resources()
        {
            _viz.reset();
            _fbo.reset();
            _enabled = 0;
        }

        void colorizer::create_gpu_resources()
        {
            //_viz = std::make_shared<visualizer_2d>(std::make_shared<yuy2rgb_shader>());
            _fbo = std::make_shared<fbo>(_width, _height);

            _enabled = glsl_enabled() ? 1 : 0;
        }

        colorizer::colorizer()
        {
            _source.add_extension<gpu_video_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);

            auto opt = std::make_shared<librealsense::ptr_option<int>>(
                0, 1, 0, 1, &_enabled, "GLSL enabled"); 
            register_option(RS2_OPTION_COUNT, opt);

            initialize();
        }

        colorizer::~colorizer()
        {
            perform_gl_action([&]()
            {
                cleanup_gpu_resources();
            }, []{});
        }

        rs2::frame colorizer::process_frame(const rs2::frame_source& src, const rs2::frame& f)
        {
            if (f.get_profile().get() != _input_profile.get())
            {
                _input_profile = f.get_profile();
                _output_profile = _input_profile.clone(_input_profile.stream_type(), 
                                                    _input_profile.stream_index(), 
                                                    RS2_FORMAT_RGB8);
                auto vp = _input_profile.as<rs2::video_stream_profile>();
                _width = vp.width(); _height = vp.height();

                perform_gl_action([&]()
                {
                    _fbo = std::make_shared<fbo>(_width, _height);
                }, [this] {
                    _enabled = false;
                });
            }

            rs2::frame res = f;

            perform_gl_action([&]()
            {
                res = src.allocate_video_frame(_output_profile, f, 3, _width, _height, _width * 3, RS2_EXTENSION_VIDEO_FRAME_GL);
                
                auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)res.get());
                
                uint32_t yuy_texture;
                glGenTextures(1, &yuy_texture);
                glBindTexture(GL_TEXTURE_2D, yuy_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, _width, _height, 0, GL_RG, GL_UNSIGNED_BYTE, f.get_data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                uint32_t output_rgb;
                gf->get_gpu_section().output_texture(0, &output_rgb, texture_type::RGB);
                glBindTexture(GL_TEXTURE_2D, output_rgb);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

                gf->get_gpu_section().set_size(_width, _height);

                glBindFramebuffer(GL_FRAMEBUFFER, _fbo->get());
                glDrawBuffer(GL_COLOR_ATTACHMENT0);

                glBindTexture(GL_TEXTURE_2D, output_rgb);
                _fbo->createTextureAttachment(output_rgb);

                _fbo->bind();
                glClearColor(1, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);

                // auto& shader = (yuy2rgb_shader&)_viz->get_shader();
                // shader.begin();
                // shader.set_size(_width, _height);
                // shader.end();
                
                _viz->draw_texture(yuy_texture);

                _fbo->unbind();

                glBindTexture(GL_TEXTURE_2D, 0);

                if (!f.is<rs2::gl::gpu_frame>())
                {
                    glDeleteTextures(1, &yuy_texture);
                }
            }, 
            [this]{
                _enabled = false;
            }); 

            return res;
        }
    }
}