// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#include "pc-shader.h"
#include "synthetic-stream-gl.h"
#include <glad/glad.h>

#include "option.h"

static const char* vertex_shader_text =
"#version 130\n"
"\n"
"attribute vec3 position;\n"
"attribute vec2 textureCoords;\n"
"\n"
"out float valid;\n"
"out vec2 sampledUvs;\n"
"out vec4 outPos;\n"
"\n"
"uniform mat4 transformationMatrix;\n"
"uniform mat4 projectionMatrix;\n"
"uniform mat4 cameraMatrix;\n"
"\n"
"uniform sampler2D uvsSampler;\n"
"uniform sampler2D positionsSampler;\n"
"\n"
"uniform float imageWidth;\n"
"uniform float imageHeight;\n"
"uniform float minDeltaZ;\n"
"\n"
"void main(void) {\n"
"    float pixelWidth = 1.0 / imageWidth;\n"
"    float pixelHeight = 1.0 / imageHeight;\n"
"    vec2 tex = vec2(textureCoords.x, textureCoords.y);\n"
"    vec4 pos = texture2D(positionsSampler, tex);\n"
"    vec4 uvs = texture2D(uvsSampler, tex);\n"
"\n"
"    vec2 tex_left = vec2(max(textureCoords.x - pixelWidth, 0.0), textureCoords.y);\n"
"    vec2 tex_right = vec2(min(textureCoords.x + pixelWidth, 1.0), textureCoords.y);\n"
"    vec2 tex_top = vec2(textureCoords.x, max(textureCoords.y - pixelHeight, 0.0));\n"
"    vec2 tex_buttom = vec2(textureCoords.x, min(textureCoords.y + pixelHeight, 1.0));\n"
"\n"
"    vec4 pos_left = texture2D(positionsSampler, tex_left);\n"
"    vec4 pos_right = texture2D(positionsSampler, tex_right);\n"
"    vec4 pos_top = texture2D(positionsSampler, tex_top);\n"
"    vec4 pos_buttom = texture2D(positionsSampler, tex_buttom);\n"
"\n"
"    valid = 0.0;\n"
"    if (uvs.x < 0.0) valid = 1.0;\n"
"    if (uvs.y < 0.0) valid = 1.0;\n"
"    if (uvs.x >= 1.0) valid = 1.0;\n"
"    if (uvs.y >= 1.0) valid = 1.0;\n"
"    if (abs(pos_left.z - pos.z) > minDeltaZ) valid = 1.0;\n"
"    if (abs(pos_right.z - pos.z) > minDeltaZ) valid = 1.0;\n"
"    if (abs(pos_top.z - pos.z) > minDeltaZ) valid = 1.0;\n"
"    if (abs(pos_buttom.z - pos.z) > minDeltaZ) valid = 1.0;\n"
"    if (abs(pos.z) < 0.01) valid = 1.0;\n"
"    if (valid > 0.0) pos = vec4(1.0, 1.0, 1.0, 0.0);\n"
"    else pos = vec4(pos.xyz, 1.0);\n"
"    vec4 worldPosition = transformationMatrix * pos;\n"
"    gl_Position = projectionMatrix * cameraMatrix * worldPosition;\n"
"\n"
"    sampledUvs = uvs.xy;\n"
"    outPos = pos;\n"
"}\n";

static const char* fragment_shader_text =
"#version 130\n"
"\n"
"in float valid;\n"
"in vec4  outPos;\n"
"in vec2 sampledUvs;\n"
"out vec4 output_rgb;\n"
"out vec4 output_xyz;\n"
"\n"
"uniform sampler2D textureSampler;\n"
"uniform vec2 mouseXY;\n"
"uniform float pickedID;\n"
"\n"
"void main(void) {\n"
"    if (valid > 0.0) discard;\n"
"    vec4 color = texture2D(textureSampler, sampledUvs);\n"
"    float dist = length(mouseXY - gl_FragCoord.xy);\n"
"    float t = 0.4 + smoothstep(0.0, 5.0, dist) * 0.6;\n" 
"\n"
"    output_rgb = t * vec4(color.xyz, 1.0) + (1.0 - t) * vec4(1.0);\n"
"    output_xyz = outPos;\n"
"}\n";

static const char* blit_vertex_shader_text =
"#version 130\n"
"in vec3 position;\n"
"in vec2 textureCoords;\n"
"out vec2 textCoords[9];\n"
"uniform vec2 elementPosition;\n"
"uniform vec2 elementScale;\n"
"uniform vec2 imageDims;\n"
"void main(void)\n"
"{\n"
"    vec2 tex = vec2(textureCoords.x, 1.0 - textureCoords.y);\n"
"    vec2 pixelSize = 1.0 / imageDims;\n"
"    for (int i = -1; i <= 1; i++)\n"
"       for (int j = -1; j <= 1; j++)\n"
"       {\n"
"           textCoords[(i + 1) * 3 + j + 1] = clamp(tex + pixelSize * vec2(i, j), vec2(0.0), vec2(1.0));\n"
"       }\n"
"    gl_Position = vec4(position * vec3(elementScale, 1.0) + vec3(elementPosition, 0.0), 1.0);\n"
"}";

static const char* blit_frag_shader_text =
"#version 130\n"
"in vec2 textCoords[9];\n"
"uniform sampler2D textureSampler;\n"
"uniform sampler2D depthSampler;\n"
"uniform float opacity;\n"
"uniform float is_selected;\n"
"void main(void) {\n"
"    vec4 color = texture2D(textureSampler, textCoords[4]);\n"
"    float maxAlpha = 0.0;\n"
"    for (int i = 0; i < 9; i++)\n"
"    {\n"
"        float a = texture2D(textureSampler, textCoords[i]).w;\n"
"        maxAlpha = max(a, maxAlpha);\n"
"    }\n"
"    float alpha = texture2D(textureSampler, textCoords[4]).w;\n"
"    float selected = 0.0;"
"    if (alpha < maxAlpha) selected = is_selected;\n"
"    gl_FragColor = color * (1.0 - selected) + selected * vec4(0.0, 0.68, 0.93, 0.8);\n"
"    if (alpha > 0) gl_FragDepth = texture2D(depthSampler, textCoords[4]).x;"
"    else if (selected > 0) gl_FragDepth = 0.0;"
"    else gl_FragDepth = 65000.0;"
"}";

using namespace rs2;

namespace librealsense
{
    namespace gl
    {
        pointcloud_shader::pointcloud_shader(std::unique_ptr<shader_program> shader)
            : _shader(std::move(shader))
        {
            init();
        }

        pointcloud_shader::pointcloud_shader()
        {
            _shader = shader_program::load(
                vertex_shader_text,
                fragment_shader_text,
                "position", "textureCoords",
                "output_rgb", "output_pos");

            init();
        }

        void pointcloud_shader::init()
        {
            _transformation_matrix_location = _shader->get_uniform_location("transformationMatrix");
            _projection_matrix_location = _shader->get_uniform_location("projectionMatrix");
            _camera_matrix_location = _shader->get_uniform_location("cameraMatrix");

            _width_location = _shader->get_uniform_location("imageWidth");
            _height_location = _shader->get_uniform_location("imageHeight");
            _min_delta_z_location = _shader->get_uniform_location("minDeltaZ");
            _mouse_xy_location = _shader->get_uniform_location("mouseXY");
            _picked_id_location = _shader->get_uniform_location("pickedID");

            auto texture0_sampler_location = _shader->get_uniform_location("textureSampler");
            auto texture1_sampler_location = _shader->get_uniform_location("positionsSampler");
            auto texture2_sampler_location = _shader->get_uniform_location("uvsSampler");

            _shader->begin();
            _shader->load_uniform(_min_delta_z_location, 0.2f);
            _shader->load_uniform(texture0_sampler_location, texture_slot());
            _shader->load_uniform(texture1_sampler_location, geometry_slot());
            _shader->load_uniform(texture2_sampler_location, uvs_slot());
            _shader->end();
        }

        void pointcloud_shader::begin() { _shader->begin(); }
        void pointcloud_shader::end() { _shader->end(); }

        void pointcloud_shader::set_mvp(const matrix4& model,
            const matrix4& view,
            const matrix4& projection)
        {
            _shader->load_uniform(_transformation_matrix_location, model);
            _shader->load_uniform(_camera_matrix_location, view);
            _shader->load_uniform(_projection_matrix_location, projection);
        }

        void pointcloud_shader::set_image_size(int width, int height)
        {
            _shader->load_uniform(_width_location, (float)width);
            _shader->load_uniform(_height_location, (float)height);
        }

        void pointcloud_shader::set_mouse_xy(float x, float y)
        {
            rs2::float2 xy{ x, y };
            _shader->load_uniform(_mouse_xy_location, xy);
        }

        void pointcloud_shader::set_picked_id(float pid)
        {
            _shader->load_uniform(_picked_id_location, pid);
        }

        void pointcloud_shader::set_min_delta_z(float min_delta_z)
        {
            _shader->load_uniform(_min_delta_z_location, min_delta_z);
        }

        void blit_shader::set_selected(bool selected)
        {
            _shader->load_uniform(_is_selected_location, selected ? 1.f : 0.f);
        }

        void blit_shader::set_image_size(int width, int height)
        {
            rs2::float2 xy{ width, height };
            _shader->load_uniform(_image_dims_location, xy);
        }

        blit_shader::blit_shader()
            : texture_2d_shader(shader_program::load(blit_vertex_shader_text, blit_frag_shader_text))
        {
            auto texture1_sampler_location = _shader->get_uniform_location("depthSampler");

            _image_dims_location = _shader->get_uniform_location("imageDims");
            _is_selected_location = _shader->get_uniform_location("is_selected");
            
            _shader->begin();
            _shader->load_uniform(texture1_sampler_location, 1);
            _shader->end();
        }

        void pointcloud_renderer::cleanup_gpu_resources()
        {
            glDeleteTextures(1, &color_tex);
            glDeleteTextures(1, &depth_tex);
            glDeleteTextures(1, &xyz_tex);

            _shader.reset();
            _model.reset();
            _vertex_texture.reset();
            _uvs_texture.reset();
            _viz.reset();
            _blit.reset();
            _fbo.reset();
        }

        pointcloud_renderer::~pointcloud_renderer()
        {
            perform_gl_action([&]()
            {
                cleanup_gpu_resources();
            });
        }

        void pointcloud_renderer::create_gpu_resources()
        {
            if (glsl_enabled())
            {
                clear_gl_errors();
                _shader = std::make_shared<pointcloud_shader>();

                _vertex_texture = std::make_shared<rs2::texture_buffer>();
                _uvs_texture = std::make_shared<rs2::texture_buffer>();

                obj_mesh mesh = make_grid(_width, _height);
                _model = vao::create(mesh);

                _fbo = std::make_shared<fbo>(1, 1);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                _viz = std::make_shared<rs2::texture_visualizer>();
                _blit = std::make_shared<blit_shader>();

                glGenTextures(1, &color_tex);
                glGenTextures(1, &depth_tex);
                glGenTextures(1, &xyz_tex);
            }
        }

        pointcloud_renderer::pointcloud_renderer() 
            : stream_filter_processing_block("Pointcloud Renderer")
        {
            register_option(OPTION_FILLED, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 0, 1 }));
            register_option(OPTION_MOUSE_X, std::make_shared<librealsense::float_option>(option_range{ 0, 10000, 1, 0 }));
            register_option(OPTION_MOUSE_Y, std::make_shared<librealsense::float_option>(option_range{ 0, 10000, 1, 0 }));
            register_option(OPTION_MOUSE_PICK, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 1, 0 }));   

            register_option(OPTION_PICKED_X, std::make_shared<librealsense::float_option>(option_range{ -1000, 1000, 0, 0 }));
            register_option(OPTION_PICKED_Y, std::make_shared<librealsense::float_option>(option_range{ -1000, 1000, 0, 0 }));
            register_option(OPTION_PICKED_Z, std::make_shared<librealsense::float_option>(option_range{ -1000, 1000, 0, 0 }));
            register_option(OPTION_PICKED_ID, std::make_shared<librealsense::float_option>(option_range{ 0, 32, 1, 0 }));

            register_option(OPTION_SELECTED, std::make_shared<librealsense::float_option>(option_range{ 0, 1, 0, 1 }));

            _filled_opt = &get_option(OPTION_FILLED);
            _mouse_x_opt = &get_option(OPTION_MOUSE_X);
            _mouse_y_opt = &get_option(OPTION_MOUSE_Y);
            _mouse_pick_opt = &get_option(OPTION_MOUSE_PICK);
            _picked_x_opt = &get_option(OPTION_PICKED_X);
            _picked_y_opt = &get_option(OPTION_PICKED_Y);
            _picked_z_opt = &get_option(OPTION_PICKED_Z);
            _picked_id_opt = &get_option(OPTION_PICKED_ID);
            _selected_opt = &get_option(OPTION_SELECTED);

            initialize();
        }

        rs2::frame pointcloud_renderer::process_frame(const rs2::frame_source& src, const rs2::frame& f)
        {
            scoped_timer t("pointcloud_renderer");
            if (auto points = f.as<rs2::points>())
            {
                perform_gl_action([&]()
                {
                    //scoped_timer t("pointcloud_renderer.gl");

                    GLint curr_tex;
                    glGetIntegerv(GL_TEXTURE_BINDING_2D, &curr_tex);

                    clear_gl_errors();

                    auto vf_profile = f.get_profile().as<video_stream_profile>();
                    int width = vf_profile.width();
                    int height = vf_profile.height();
                    
                    if (glsl_enabled())
                    {
                        if (_width != width || _height != height)
                        {
                            obj_mesh mesh = make_grid(width, height);
                            _model = vao::create(mesh);

                            _width = width;
                            _height = height;
                        }

                        auto points_f = (frame_interface*)points.get();

                        uint32_t vertex_tex_id = 0;
                        uint32_t uv_tex_id = 0;

                        bool error = false;
                        
                        if (auto g = dynamic_cast<gpu_points_frame*>(points_f))
                        {
                            if (!g->get_gpu_section().input_texture(0, &vertex_tex_id) ||
                                !g->get_gpu_section().input_texture(1, &uv_tex_id)
                                ) error = true;
                        }
                        else
                        {
                            _vertex_texture->upload(points, RS2_FORMAT_XYZ32F);
                            vertex_tex_id = _vertex_texture->get_gl_handle();

                            _uvs_texture->upload(points, RS2_FORMAT_Y16);
                            uv_tex_id = _uvs_texture->get_gl_handle();
                        }

                        if (!error)
                        {
                            int32_t vp[4];
                            glGetIntegerv(GL_VIEWPORT, vp);
                            check_gl_error();

                            _fbo->set_dims(vp[2], vp[3]);

                            glBindFramebuffer(GL_FRAMEBUFFER, _fbo->get());
                            glDrawBuffer(GL_COLOR_ATTACHMENT0);

                            _fbo->createTextureAttachment(color_tex);
                            _fbo->createDepthTextureAttachment(depth_tex);

                            glBindTexture(GL_TEXTURE_2D, xyz_tex);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, vp[2], vp[3], 0, GL_RGB, GL_FLOAT, nullptr);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, xyz_tex, 0);
                            glBindTexture(GL_TEXTURE_2D, 0);

                            _fbo->bind();

                            GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
                            glDrawBuffers(2, attachments);

                            glClearColor(0, 0, 0, 0);
                            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                            _shader->begin();
                            _shader->set_mvp(get_matrix(
                                RS2_GL_MATRIX_TRANSFORMATION),
                                get_matrix(RS2_GL_MATRIX_CAMERA),
                                get_matrix(RS2_GL_MATRIX_PROJECTION)
                            );
                            _shader->set_image_size(vf_profile.width(), vf_profile.height());

                            _shader->set_picked_id(_picked_id_opt->query());

                            if (_mouse_pick_opt->query() > 0.f)
                            {
                                auto x = _mouse_x_opt->query() - vp[0];
                                auto y = vp[3] + vp[1] - _mouse_y_opt->query();
                                _shader->set_mouse_xy(x, y);
                            }
                            else _shader->set_mouse_xy(-1, -1);

                            glActiveTexture(GL_TEXTURE0 + _shader->texture_slot());
                            glBindTexture(GL_TEXTURE_2D, curr_tex);

                            glActiveTexture(GL_TEXTURE0 + _shader->geometry_slot());
                            glBindTexture(GL_TEXTURE_2D, vertex_tex_id);

                            glActiveTexture(GL_TEXTURE0 + _shader->uvs_slot());
                            glBindTexture(GL_TEXTURE_2D, uv_tex_id);

                            if (_filled_opt->query() > 0.f) _model->draw();
                            else _model->draw_points();

                            glActiveTexture(GL_TEXTURE0 + _shader->texture_slot());

                            _shader->end();

                            _fbo->unbind();

                            _picked_id_opt->set(0.f);

                            if (_mouse_pick_opt->query() > 0.f)
                            {
                                auto x = _mouse_x_opt->query() - vp[0];
                                auto y = vp[3] + vp[1] - _mouse_y_opt->query();

                                glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo->get());
                                glReadBuffer(GL_COLOR_ATTACHMENT1);

                                float3 pos;
                                glReadPixels(x, y, 1, 1, GL_RGB, GL_FLOAT, &pos);

                                glReadBuffer(GL_COLOR_ATTACHMENT0);

                                uint8_t rgba[4];
                                glReadPixels(x, y, 1, 1, GL_RGBA, GL_BYTE, &rgba);

                                if (rgba[3] > 0)
                                {
                                    _picked_id_opt->set(1.f);
                                    _picked_x_opt->set(pos.x);
                                    _picked_y_opt->set(pos.y);
                                    _picked_z_opt->set(pos.z);
                                }

                                glReadBuffer(GL_NONE);
                                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                                _mouse_pick_opt->set(0.f);
                            }

                            //glDisable(GL_DEPTH_TEST);

                            glActiveTexture(GL_TEXTURE1);
                            glBindTexture(GL_TEXTURE_2D, depth_tex);
                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, color_tex);

                            _blit->begin();
                            _blit->set_image_size(vp[2], vp[3]);
                            _blit->set_selected(_selected_opt->query() > 0.f);
                            _blit->end();

                            _viz->draw(*_blit, color_tex);

                            glActiveTexture(GL_TEXTURE0 + _shader->texture_slot());

                            //glEnable(GL_DEPTH_TEST);
                        }
                    }
                    else
                    {
                        glMatrixMode(GL_MODELVIEW);
                        glPushMatrix();

                        auto t = get_matrix(RS2_GL_MATRIX_TRANSFORMATION);
                        auto v = get_matrix(RS2_GL_MATRIX_CAMERA);

                        glLoadMatrixf(v * t);

                        auto vertices = points.get_vertices();
                        auto tex_coords = points.get_texture_coordinates();

                        glBindTexture(GL_TEXTURE_2D, curr_tex);

                        if (_filled_opt->query() > 0.f)
                        {
                            glBegin(GL_QUADS);

                            const auto threshold = 0.05f;
                            for (int x = 0; x < width - 1; ++x) {
                                for (int y = 0; y < height - 1; ++y) {
                                    auto a = y * width + x, b = y * width + x + 1, c = (y + 1)*width + x, d = (y + 1)*width + x + 1;
                                    if (vertices[a].z && vertices[b].z && vertices[c].z && vertices[d].z
                                        && abs(vertices[a].z - vertices[b].z) < threshold && abs(vertices[a].z - vertices[c].z) < threshold
                                        && abs(vertices[b].z - vertices[d].z) < threshold && abs(vertices[c].z - vertices[d].z) < threshold) {
                                        glVertex3fv(vertices[a]); glTexCoord2fv(tex_coords[a]);
                                        glVertex3fv(vertices[b]); glTexCoord2fv(tex_coords[b]);
                                        glVertex3fv(vertices[d]); glTexCoord2fv(tex_coords[d]);
                                        glVertex3fv(vertices[c]); glTexCoord2fv(tex_coords[c]);
                                    }
                                }
                            }
                            glEnd();
                        }
                        else
                        {
                            glBegin(GL_POINTS);
                            for (int i = 0; i < points.size(); i++)
                            {
                                if (vertices[i].z)
                                {
                                    glVertex3fv(vertices[i]);
                                    glTexCoord2fv(tex_coords[i + 1]);
                                }
                            }
                            glEnd();
                        }

                        glPopMatrix();
                    }
                }); 
            }

            return f;
        }
    }
}