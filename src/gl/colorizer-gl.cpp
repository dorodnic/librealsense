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


static const char* fragment_shader_text =
"#version 110\n"
"varying vec2 textCoords;\n"
"uniform sampler2D textureSampler;\n"
"uniform sampler2D cmSampler;\n"
"uniform float opacity;\n"
"uniform float depth_units;\n"
"uniform float min_depth;\n"
"uniform float max_depth;\n"
"uniform float colors;\n"
"void main(void) {\n"
"    vec2 tex = vec2(textCoords.x, 1.0 - textCoords.y);\n"
"    vec4 depth = texture2D(textureSampler, tex);\n"
"    float d = (depth.x + depth.y * 256.0) * 256.0;\n"
"    if (d > 0.0){\n"
"        float f = (d * depth_units - min_depth) / (max_depth - min_depth);"
"        f = clamp(f, 0.0, 0.99);\n"
"        vec4 color = texture2D(cmSampler, vec2(f, 0.0));\n"
"        gl_FragColor = vec4(color.x / 256.0, color.y / 256.0, color.z / 256.0, opacity);\n"
"    } else {\n"
"        gl_FragColor = vec4(0.0, 0.0, 0.0, opacity);\n"
"    }\n"
"}";

using namespace rs2;
using namespace librealsense::gl;

class colorize_shader : public texture_2d_shader
{
public:
    colorize_shader()
        : texture_2d_shader(shader_program::load(
            texture_2d_shader::default_vertex_shader(), 
            fragment_shader_text))
    {
        _depth_units_location = _shader->get_uniform_location("depth_units");
        _min_depth_location = _shader->get_uniform_location("min_depth");
        _max_depth_location = _shader->get_uniform_location("max_depth");
        _colors_location = _shader->get_uniform_location("colors");

        auto texture0_sampler_location = _shader->get_uniform_location("textureSampler");
        auto texture1_sampler_location = _shader->get_uniform_location("cmSampler");

        _shader->begin();
        _shader->load_uniform(texture0_sampler_location, texture_slot());
        _shader->load_uniform(texture1_sampler_location, color_map_slot());
        _shader->end();
    }

    int texture_slot() const { return 0; }
    int color_map_slot() const { return 1; }

    void set_params(float units, float min, float max, int colors)
    {
        _shader->load_uniform(_depth_units_location, units);
        _shader->load_uniform(_min_depth_location, min);
        _shader->load_uniform(_max_depth_location, max);
        _shader->load_uniform(_colors_location, (float)colors);
    }

private:
    uint32_t _depth_units_location;
    uint32_t _min_depth_location;
    uint32_t _max_depth_location;
    uint32_t _colors_location;
};

using namespace rs2;

namespace librealsense
{
    namespace gl
    {
        void colorizer::cleanup_gpu_resources()
        {
            _viz.reset();
            _fbo.reset();

            if (_cm_texture) glDeleteTextures(1, &_cm_texture);

            _enabled = 0;
        }

        void colorizer::create_gpu_resources()
        {
            _viz = std::make_shared<visualizer_2d>(std::make_shared<colorize_shader>());
            _fbo = std::make_shared<fbo>(_width, _height);

            glGenTextures(1, &_cm_texture);
            auto& curr_map = _maps[_map_index]->get_cache();
            _last_selected_cm = _map_index;
            glBindTexture(GL_TEXTURE_2D, _cm_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, curr_map.size(), 1, 0, GL_RGB, GL_FLOAT, curr_map.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            _enabled = glsl_enabled() ? 1 : 0;
        }

        colorizer::colorizer() : _cm_texture(0)
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
            scoped_timer c("colorizer body");

            if (f.get_profile().get() != _source_stream_profile.get())
            {
                scoped_timer c("prefix");
                _source_stream_profile = f.get_profile();
                _target_stream_profile = _source_stream_profile.clone(
                                            _source_stream_profile.stream_type(), 
                                            _source_stream_profile.stream_index(), 
                                            RS2_FORMAT_RGB8);
                auto vp = _source_stream_profile.as<rs2::video_stream_profile>();
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
                scoped_timer c("colorizer");
                auto& curr_map = _maps[_map_index]->get_cache();

                if (_last_selected_cm != _map_index)
                {
                    glBindTexture(GL_TEXTURE_2D, _cm_texture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, curr_map.size(), 1, 0, GL_RGB, GL_FLOAT, curr_map.data());
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                    _last_selected_cm = _map_index;
                }

                res = src.allocate_video_frame(_target_stream_profile, f, 3, _width, _height, _width * 3, RS2_EXTENSION_VIDEO_FRAME_GL);
                
                if (!res) return;

                auto fi = (frame_interface*)f.get();
                auto df = dynamic_cast<librealsense::depth_frame*>(fi);
                auto depth_units = df->get_units();

                auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)res.get());

                uint32_t depth_texture;
                if (auto input_frame = f.as<rs2::gl::gpu_frame>())
                {
                    depth_texture = input_frame.get_texture_id(0);
                }
                else
                {
                    glGenTextures(1, &depth_texture);
                    glBindTexture(GL_TEXTURE_2D, depth_texture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, _width, _height, 0, GL_RG, GL_UNSIGNED_BYTE, f.get_data());
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                }
                
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

                {
                    scoped_timer c("colorizer shader");

                    auto& shader = (colorize_shader&)_viz->get_shader();
                    shader.begin();
                    shader.set_params(depth_units, _min, _max, curr_map.size());
                    shader.end();

                    glActiveTexture(GL_TEXTURE0 + shader.texture_slot());
                    glBindTexture(GL_TEXTURE_2D, depth_texture);

                    glActiveTexture(GL_TEXTURE0 + shader.color_map_slot());
                    glBindTexture(GL_TEXTURE_2D, _cm_texture);

                    _viz->draw_texture(depth_texture);

                    glActiveTexture(GL_TEXTURE0 + shader.texture_slot());

                    _fbo->unbind();
                }

                glBindTexture(GL_TEXTURE_2D, 0);

                if (!f.is<rs2::gl::gpu_frame>())
                {
                    glDeleteTextures(1, &depth_texture);
                }
            }, 
            [this]{
                _enabled = false;
            }); 

            return res;
        }
    }
}