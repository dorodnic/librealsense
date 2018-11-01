#include "yuy2rgb.h"

#define NOMINMAX

#include <glad/glad.h>

#include <iostream>

#include <chrono>
#include <strstream>

#include "texture-2d-shader.h"

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

// 3rd party header for writing png files
#include "stb_image_write.h"

using namespace rs2;

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

yuy2rgb::yuy2rgb()
    : processing_block([this](frame f, frame_source& src){
        on_frame(f, src);
    }), _yuy_texture(0), _output_rgb(0)
{
    start(_queue);
}

void yuy2rgb::init()
{
    if (!_yuy_texture)
    {
        glDeleteTextures(1, &_yuy_texture);
        glDeleteTextures(1, &_output_rgb);
    }

    _viz = std::unique_ptr<visualizer_2d>(new visualizer_2d(
        std::make_shared<yuy2rgb_shader>()
    ));

    glGenTextures(1, &_yuy_texture);
    glGenTextures(1, &_output_rgb);
}

yuy2rgb::~yuy2rgb()
{
    glDeleteTextures(1, &_yuy_texture);
    glDeleteTextures(1, &_output_rgb);
}

void yuy2rgb::on_frame(frame f, frame_source& src)
{
    auto start = std::chrono::high_resolution_clock::now();

    if (!_was_init)
    {
        init();
        _was_init = true;
    }

    if (f.get_profile().get() != _input_profile.get())
    {
        _input_profile = f.get_profile();
        _output_profile = _input_profile.clone(_input_profile.stream_type(), 
                                               _input_profile.stream_index(), 
                                               RS2_FORMAT_RGB8);
        auto vp = _input_profile.as<rs2::video_stream_profile>();
        _width = vp.width(); _height = vp.height();
        _fbo = std::unique_ptr<fbo>(new fbo(_width, _height));
        _fbo->createTextureAttachment(_output_rgb);
    }

    glBindTexture(GL_TEXTURE_2D, _yuy_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, _width, _height, 0, GL_RG, GL_UNSIGNED_BYTE, f.get_data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    _fbo->bind();
    glViewport(0, 0, _width, _height);
    glClearColor(1, 0, 0, 1);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto& shader = (yuy2rgb_shader&)_viz->get_shader();
    shader.begin();
    shader.set_size(_width, _height);
    shader.end();
    _viz->draw_texture(_yuy_texture);

    auto res = src.allocate_video_frame(_output_profile, f, 3, _width, _height, _width * 3);

    glReadBuffer(GL_COLOR_ATTACHMENT0);

    glReadPixels(0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, (void*)res.get_data());
    
    _fbo->unbind();

    //glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_2D, 0);
    src.frame_ready(res);

    //auto vf = res.as<rs2::video_frame>();

    //auto fw = vf.get_width();
    //auto fh = vf.get_height();
    //auto fbpp = vf.get_bytes_per_pixel();
    //auto fstride = vf.get_stride_in_bytes();

    //stbi_write_png("1.png", fw, fh,
    //    fbpp, vf.get_data(), fstride);

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::stringstream ss; ss << ms << std::endl;
    OutputDebugStringA(ss.str().c_str());
}