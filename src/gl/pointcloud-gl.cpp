// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/rs.hpp"
#include "../include/librealsense2/rsutil.h"

#include "synthetic-stream-gl.h"
#include "environment.h"
#include "proc/occlusion-filter.h"
#include "pointcloud-gl.h"
#include "option.h"
#include "environment.h"
#include "context.h"

#include <iostream>
#include <chrono>

#include "fbo.h"

using namespace rs2;
using namespace librealsense;
using namespace librealsense::gl;

static const char* project_fragment_text =
"#version 130\n"
"in vec2 textCoords;\n"
"out vec4 output;\n"
"uniform sampler2D textureSampler;\n"
"uniform float opacity;\n"
"uniform vec2 focal;\n"
"uniform vec2 principal;\n"
"uniform float is_bc;\n"
"uniform float coeffs[5];\n"
"uniform float depth_scale;\n"
"uniform float width;\n"
"uniform float height;\n"
"\n"
"void main(void) {\n"
"    float px = textCoords.x * width;\n"
"    float py = (1.0 - textCoords.y) * height;\n"
"    float x = (px - principal.x) / focal.x;\n"
"    float y = (py - principal.y) / focal.y;\n"
"    if(is_bc > 0.0)\n"
"    {\n"
"        float r2  = x*x + y*y;\n"
"        float f = 1.0 + coeffs[0]*r2 + coeffs[1]*r2*r2 + coeffs[4]*r2*r2*r2;\n"
"        float ux = x*f + 2.0*coeffs[2]*x*y + coeffs[3]*(r2 + 2.0*x*x);\n"
"        float uy = y*f + 2.0*coeffs[3]*x*y + coeffs[2]*(r2 + 2.0*y*y);\n"
"        x = ux;\n"
"        y = uy;\n"
"    }\n"
"    vec2 tex = vec2(textCoords.x, 1.0 - textCoords.y);\n"
"    float d = texture(textureSampler, tex).x;\n"
"    float depth = depth_scale * d * 65535;\n"
"    output = vec4(x * depth, y * depth, depth, opacity);\n"
"}";


static const char* uv_fragment_text =
"#version 130\n"
"in vec2 textCoords;\n"
"out vec4 output;\n"
"uniform sampler2D textureSampler;\n"
"uniform float opacity;\n"
"uniform mat4 extrinsics;\n"
"uniform vec2 focal;\n"
"uniform vec2 principal;\n"
"uniform float is_bc;\n"
"uniform float coeffs[5];\n"
"uniform float width;\n"
"uniform float height;\n"
"\n"
"void main(void) {\n"
"    vec2 tex = vec2(textCoords.x, 1.0 - textCoords.y);\n"
"    vec4 xyz = texture(textureSampler, tex);\n"
"    vec4 trans = extrinsics * xyz;\n"
"    float x = trans.x / trans.z;\n"
"    float y = trans.y / trans.z;\n"
"\n"
"    if(is_bc > 0.0)\n"
"    {\n"
"        float r2  = x*x + y*y;\n"
"        float f = 1.0 + coeffs[0]*r2 + coeffs[1]*r2*r2 + coeffs[4]*r2*r2*r2;\n"
"        x *= f;\n"
"        y *= f;\n"
"        float dx = x + 2.0*coeffs[2]*x*y + coeffs[3]*(r2 + 2.0*x*x);\n"
"        float dy = y + 2.0*coeffs[3]*x*y + coeffs[2]*(r2 + 2.0*y*y);\n"
"        x = dx;\n"
"        y = dy;\n"
"    }\n"
"    // TODO: Enable F-Thetha\n"
"    //if (intrin->model == RS2_DISTORTION_FTHETA)\n"
"    //{\n"
"    //    float r = sqrtf(x*x + y*y);\n"
"    //    float rd = (float)(1.0f / intrin->coeffs[0] * atan(2 * r* tan(intrin->coeffs[0] / 2.0f)));\n"
"    //    x *= rd / r;\n"
"    //    y *= rd / r;\n"
"    //}\n"
"\n"
"    float u = (x * focal.x + principal.x) / width;\n"
"    float v = (y * focal.y + principal.y) / height;\n"
"    output = vec4(u, v, 0.0, 1.0);\n"
"}";

class project_shader : public texture_2d_shader
{
public:
    project_shader()
        : texture_2d_shader(project_fragment_text)
    {
        _focal_location = _shader->get_uniform_location("focal");
        _principal_location = _shader->get_uniform_location("principal");
        _is_bc_location = _shader->get_uniform_location("is_bc");
        _coeffs_location = _shader->get_uniform_location("coeffs");
        _depth_scale_location = _shader->get_uniform_location("depth_scale");
        _width_location = _shader->get_uniform_location("width");
        _height_location = _shader->get_uniform_location("height");
    }

    void set_size(int w, int h)
    {
        _shader->load_uniform(_width_location, (float)w);
        _shader->load_uniform(_height_location, (float)h);
    }

    void set_intrinsics(const rs2_intrinsics& intr)
    {
        rs2::float2 focal{ intr.fx, intr.fy };
        rs2::float2 principal{ intr.ppx, intr.ppy };
        float is_bc = (intr.model == RS2_DISTORTION_INVERSE_BROWN_CONRADY ? 1.f : 0.f);
        _shader->load_uniform(_focal_location, focal);
        _shader->load_uniform(_principal_location, principal);
        _shader->load_uniform(_is_bc_location, is_bc);
        glUniform1fv(_shader->get_id(), 5, intr.coeffs);
    }

    void set_depth_scale(float depth_scale)
    {
        _shader->load_uniform(_depth_scale_location, depth_scale);
    }
private:
    uint32_t _focal_location;
    uint32_t _principal_location;
    uint32_t _is_bc_location;
    uint32_t _coeffs_location;
    uint32_t _depth_scale_location;

    uint32_t _width_location;
    uint32_t _height_location;
};

class uvmap_shader : public texture_2d_shader
{
public:
    uvmap_shader()
        : texture_2d_shader(uv_fragment_text)
    {
        _focal_location = _shader->get_uniform_location("focal");
        _principal_location = _shader->get_uniform_location("principal");
        _is_bc_location = _shader->get_uniform_location("is_bc");
        _coeffs_location = _shader->get_uniform_location("coeffs");
        _width_location = _shader->get_uniform_location("width");
        _height_location = _shader->get_uniform_location("height");
        _extrinsics_location = _shader->get_uniform_location("extrinsics");
    }

    void set_size(int w, int h)
    {
        _shader->load_uniform(_width_location, (float)w);
        _shader->load_uniform(_height_location, (float)h);
    }

    void set_intrinsics(const rs2_intrinsics& intr)
    {
        rs2::float2 focal{ intr.fx, intr.fy };
        rs2::float2 principal{ intr.ppx, intr.ppy };
        float is_bc = (intr.model == RS2_DISTORTION_INVERSE_BROWN_CONRADY ? 1.f : 0.f);
        _shader->load_uniform(_focal_location, focal);
        _shader->load_uniform(_principal_location, principal);
        _shader->load_uniform(_is_bc_location, is_bc);
        glUniform1fv(_shader->get_id(), 5, intr.coeffs);
    }

    void set_extrinsics(const rs2_extrinsics& extr)
    {
        rs2::matrix4 m;
        for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            if (i < 3 && j < 3) m.mat[i][j] = extr.rotation[i * 3 + j];
            else if (i == 3 && j < 3) m.mat[i][j] = extr.translation[j];
            else if (i < 3 && j == 3) m.mat[i][j] = 0.f;
            else m.mat[i][j] = 1.f;
        }
        _shader->load_uniform(_extrinsics_location, m);
    }

private:
    uint32_t _focal_location;
    uint32_t _principal_location;
    uint32_t _is_bc_location;
    uint32_t _coeffs_location;

    uint32_t _width_location;
    uint32_t _height_location;

    uint32_t _extrinsics_location;
};

pointcloud_gl::pointcloud_gl(std::shared_ptr<librealsense::gl::context> ctx)
    : pointcloud(), _ctx(ctx),
      _projection_renderer(std::make_shared<lazy<visualizer_2d>>([](){ 
          return visualizer_2d(std::make_shared<project_shader>()); })),
      _uvmap_renderer(std::make_shared<lazy<visualizer_2d>>([](){ 
          return visualizer_2d(std::make_shared<uvmap_shader>()); }))
{
    _source.add_extension<gl::gpu_points_frame>(RS2_EXTENSION_VIDEO_FRAME_GL);
}

const librealsense::float3* pointcloud_gl::depth_to_points(
        rs2::points output,
        uint8_t* points, 
        const rs2_intrinsics &depth_intrinsics, 
        const uint16_t * depth_image, 
        float depth_scale)
{
    auto session = _ctx->begin_session();

    auto start = std::chrono::high_resolution_clock::now();

    auto width = depth_intrinsics.width;
    auto height = depth_intrinsics.height;

    // std::shared_ptr<uint16_t> depth_data;

    // if (main_thread_dispatcher::instance().require_dispatch())
    // {
    //     depth_data = std::shared_ptr<uint16_t>(new uint16_t[width*height], 
    //         [](uint16_t* ptr) { delete[] ptr; });
    //     memcpy(depth_data.get(), depth_image, width * height * sizeof(uint16_t));
    //     depth_image = depth_data.get();
    // }

    auto viz = _projection_renderer;
    auto frame_ref = output.get();

    // auto action = [depth_data, frame_ref, width, height,
    //     depth_intrinsics, depth_scale, depth_image, viz]()
    // {
        //auto start_gl = std::chrono::high_resolution_clock::now();

        auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)frame_ref);

        uint32_t depth_texture;
        glGenTextures(1, &depth_texture);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, width, height, 0, GL_RED, GL_UNSIGNED_SHORT, depth_image);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        fbo fbo(width, height);
        uint32_t output_xyz;
        gf->get_gpu_section().output_texture(0, &output_xyz, texture_type::XYZ, _ctx);
        glBindTexture(GL_TEXTURE_2D, output_xyz);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        gf->get_gpu_section().set_size(width, height);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_xyz, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        fbo.bind();
        glViewport(0, 0, width, height);
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        auto& shader = (project_shader&)(*viz)->get_shader();
        shader.begin();
        shader.set_depth_scale(depth_scale);
        shader.set_intrinsics(depth_intrinsics);
        shader.set_size(width, height);
        shader.end();
        (*viz)->draw_texture(depth_texture);

        fbo.unbind();

        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &depth_texture);

        // auto end_gl = std::chrono::high_resolution_clock::now();
        // auto ms_gl = std::chrono::duration_cast<std::chrono::microseconds>(end_gl - start_gl).count();
        // std::cout << "GL " << ms_gl << std::endl;
    //};

    // if (main_thread_dispatcher::instance().require_dispatch())
    // {
    //     auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)frame_ref);
    //     gf->get_gpu_section().delay(action);
    // }
    // else
    // {
    //     action();
    // }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << ms << std::endl;

    return nullptr;
}

void pointcloud_gl::get_texture_map(
    rs2::points output,
    const librealsense::float3* points,
    const unsigned int width,
    const unsigned int height,
    const rs2_intrinsics &other_intrinsics,
    const rs2_extrinsics& extr,
    librealsense::float2* tex_ptr,
    librealsense::float2* pixels_ptr)
{
    auto viz = _uvmap_renderer;
    auto frame_ref = output.get();

    auto session = _ctx->begin_session();

    // auto action = [frame_ref, width, height,
    //     other_intrinsics, extr, viz]()
    // {
        auto start_gl = std::chrono::high_resolution_clock::now();

        auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)frame_ref);

        uint32_t input_xyz;
        gf->get_gpu_section().input_texture(0, &input_xyz);

        fbo fbo(width, height);
        uint32_t output_uv;
        gf->get_gpu_section().output_texture(1, &output_uv, texture_type::UV, _ctx);
        glBindTexture(GL_TEXTURE_2D, output_uv);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, width, height, 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_uv, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        fbo.bind();
        glViewport(0, 0, width, height);
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        auto& shader = (uvmap_shader&)(*viz)->get_shader();
        shader.begin();
        shader.set_extrinsics(extr);
        shader.set_intrinsics(other_intrinsics);
        shader.set_size(width, height);
        shader.end();
        (*viz)->draw_texture(input_xyz);

        fbo.unbind();

        glBindTexture(GL_TEXTURE_2D, 0);

        auto end_gl = std::chrono::high_resolution_clock::now();
        auto ms_gl = std::chrono::duration_cast<std::chrono::microseconds>(end_gl - start_gl).count();
        std::cout << "TM GL " << ms_gl << std::endl;
    //};

    // if (main_thread_dispatcher::instance().require_dispatch())
    // {
    //     auto gf = dynamic_cast<gpu_addon_interface*>((frame_interface*)frame_ref);
    //     gf->get_gpu_section().delay(action);
    // }
    // else
    // {
    //     action();
    // }
}

rs2::points pointcloud_gl::allocate_points(
    const rs2::frame_source& source, 
    const rs2::frame& f)
{
    auto prof = std::dynamic_pointer_cast<librealsense::stream_profile_interface>(
        _output_stream.get()->profile->shared_from_this());
    auto frame_ref = _source_wrapper.allocate_points(prof, (frame_interface*)f.get(),
        RS2_EXTENSION_VIDEO_FRAME_GL);
    rs2::frame res { (rs2_frame*)frame_ref };
    return res.as<rs2::points>();
}
