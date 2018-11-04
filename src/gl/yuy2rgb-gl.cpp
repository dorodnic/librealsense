// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"
#include "../include/librealsense2-gl/rs_processing_gl.hpp"

#include "proc/synthetic-stream.h"
#include "yuy2rgb-gl.h"

#define NOMINMAX

#include <glad/glad.h>

#include <iostream>

#include <chrono>
#include <strstream>

#include "texture-2d-shader.h"
#include "synthetic-stream-gl.h"

static const char* fragment_shader_text =
"#version 110\n"
"varying vec2 textCoords;\n"
"uniform sampler2D textureSampler;\n"
"uniform float opacity;\n"
"uniform float width;\n"
"uniform float height;\n"
"void main(void) {\n"
"    float pixel_width = 1.0 / width;\n"
"    float pixel_height = 1.0 / height;\n"
"    float y = 0.0;\n"
"    float u = 0.0;\n"
"    float v = 0.0;\n"
"    float tex_y = 1.0 - textCoords.y;\n"
"    if (mod(floor(gl_FragCoord.x), 2.0) == 0.0){\n"
"        vec2 tx1 = vec2(textCoords.x, tex_y);\n"
"        vec4 px1 = texture2D(textureSampler, tx1);\n"
"        vec2 tx2 = vec2(textCoords.x + pixel_width, tex_y);\n"
"        vec4 px2 = texture2D(textureSampler, tx2);\n"
"        y = px1.x; u = px1.y; v = px2.y;\n"
"    }\n"
"    else\n"
"    {\n"
"        vec2 tx1 = vec2(textCoords.x - pixel_width, tex_y);\n"
"        vec4 px1 = texture2D(textureSampler, tx1);\n"
"        vec2 tx2 = vec2(textCoords.x, tex_y);\n"
"        vec4 px2 = texture2D(textureSampler, tx2);\n"
"        y = px2.x; u = px1.y; v = px2.y;\n"
"    }\n"
"    //y *= 256.0; u *= 256.0; v *= 256.0;\n"
"    float c = y - (16.0 / 256.0);\n"
"    float d = u - 0.5;\n"
"    float e = v - 0.5;\n"
"    vec3 color = vec3(0.0);\n"
"    //color.x = clamp(((298.0 / 256.0) * c + (409.0 / 256.0) * e + 0.5), 0.0, 1.0);\n"
"    //color.y = clamp(((298.0 / 256.0) * c - (100.0 / 256.0) * d - (208.0/256.0) * e + 0.5), 0.0, 1.0);\n"
"    //color.z = clamp(((298.0 / 256.0) * c + (516.0 / 256.0) * d + 0.5), 0.0, 1.0);\n"
"    color.x = clamp((y + 1.40200 * (v - 0.5)), 0.0, 1.0);\n"
"    color.y = clamp((y - 0.34414 * (u - 0.5) - 0.71414 * (v - 0.5)), 0.0, 1.0);\n"
"    color.z = clamp((y + 1.77200 * (u - 0.5)), 0.0, 1.0);\n"
"    gl_FragColor = vec4(color.xyz, opacity);\n"
"}";

using namespace rs2;
using namespace librealsense::gl;

class yuy2rgb_shader : public texture_2d_shader
{
public:
    yuy2rgb_shader()
        : texture_2d_shader(fragment_shader_text)
    {
        _width_location = _shader->get_uniform_location("width");
        _height_location = _shader->get_uniform_location("height");
    }

    void set_size(int w, int h)
    {
        _shader->load_uniform(_width_location, (float)w);
        _shader->load_uniform(_height_location, (float)h);
    }

private:
    uint32_t _width_location;
    uint32_t _height_location;
};

yuy2rgb::yuy2rgb(std::shared_ptr<gl::context> ctx)
    : _viz([](){ return visualizer_2d(std::make_shared<yuy2rgb_shader>()); }), _ctx(ctx)
{
    _source.add_extension<gpu_video_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);
}

rs2::frame yuy2rgb::process_frame(const rs2::frame_source& src, const rs2::frame& f)
{
    if (f.get_profile().get() != _input_profile.get())
    {
        _input_profile = f.get_profile();
        _output_profile = _input_profile.clone(_input_profile.stream_type(), 
                                            _input_profile.stream_index(), 
                                            RS2_FORMAT_RGB8);
        auto vp = _input_profile.as<rs2::video_stream_profile>();
        _width = vp.width(); _height = vp.height();
    }

    auto start = std::chrono::high_resolution_clock::now();

    auto res = src.allocate_video_frame(_output_profile, f, 3, _width, _height, _width * 3, RS2_EXTENSION_VIDEO_FRAME_GL);

    auto session = _ctx->begin_session();

    // main_thread_dispatcher::instance().invoke([&]()
    // {
        auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)res.get());
        
        uint32_t yuy_texture;
        glGenTextures(1, &yuy_texture);
        glBindTexture(GL_TEXTURE_2D, yuy_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, _width, _height, 0, GL_RG, GL_UNSIGNED_BYTE, f.get_data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        uint32_t output_rgb;
        gf->get_gpu_section().output_texture(0, &output_rgb, texture_type::RGB, _ctx);
        glBindTexture(GL_TEXTURE_2D, output_rgb);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        gf->get_gpu_section().set_size(_width, _height);

        fbo fbo(_width, _height);
        glBindTexture(GL_TEXTURE_2D, output_rgb);
        fbo.createTextureAttachment(output_rgb);

        fbo.bind();
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        auto& shader = (yuy2rgb_shader&)_viz->get_shader();
        shader.begin();
        shader.set_size(_width, _height);
        shader.end();
        _viz->draw_texture(yuy_texture);

        fbo.unbind();

        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &yuy_texture);
    //});

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    //std::cout << ms << std::endl;

    return res;
}