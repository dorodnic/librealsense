// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "core/video.h"
#include "synthetic-stream-gl.h"
#include "option.h"
#include "opengl3.h"

#include <GLFW/glfw3.h>

#define NOMINMAX

#include <glad/glad.h>

#include <iostream>
#include <future>

#ifdef BUILD_EASYLOGGINGPP
INITIALIZE_EASYLOGGINGPP
#endif

namespace librealsense
{
    namespace gl
    {

        void rendering_lane::register_gpu_object(gpu_rendering_object* obj)
        {
            _data.register_gpu_object(obj);
        }

        void rendering_lane::unregister_gpu_object(gpu_rendering_object* obj)
        {
            _data.unregister_gpu_object(obj);
        }

        void rendering_lane::init(bool use_glsl)
        {
            std::lock_guard<std::mutex> lock(_data.mutex);
            for (auto&& obj : _data.objs)
            {
                obj->update_gpu_resources(use_glsl);
            }
            _data.active = true;
        }

        void rendering_lane::shutdown()
        {
            std::lock_guard<std::mutex> lock(_data.mutex);
            for (auto&& obj : _data.objs)
            {
                obj->update_gpu_resources(false);
            }
            _data.active = false;
        }

        rendering_lane& rendering_lane::instance()
        {
            static rendering_lane instance;
            return instance;
        }

        processing_lane& processing_lane::instance()
        {
            static processing_lane instance;
            return instance;
        }

        void processing_lane::register_gpu_object(gpu_processing_object* obj)
        {
            _data.register_gpu_object(obj);
        }

        void processing_lane::unregister_gpu_object(gpu_processing_object* obj)
        {
            _data.unregister_gpu_object(obj);
        }

        void processing_lane::init(GLFWwindow* share_with, glfw_binding binding, bool use_glsl)
        {
            std::lock_guard<std::mutex> lock(_data.mutex);

            _data.active = true;

            _ctx = std::make_shared<context>(share_with, binding);
            auto session = _ctx->begin_session();

            for (auto&& obj : _data.objs)
            {
                ((gpu_processing_object*)obj)->set_context(_ctx);
                obj->update_gpu_resources(use_glsl);
            }
        }

        void processing_lane::shutdown()
        {
            std::lock_guard<std::mutex> lock(_data.mutex);

            _data.active = false;
            auto session = _ctx->begin_session();

            for (auto&& obj : _data.objs)
            {
                ((gpu_processing_object*)obj)->set_context({});
                obj->update_gpu_resources(false);
            }
            
            _ctx.reset();
        }

        void gpu_section::cleanup_gpu_resources()
        {
            if (backup_content)
            {
                backup = std::unique_ptr<uint8_t[]>(new uint8_t[get_frame_size()]);
                fetch_frame(backup.get());
            }
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                if (textures[i])
                {
                    glDeleteTextures(1, &textures[i]);
                }
            }
        }

        gpu_section::~gpu_section()
        {
            backup_content = false;
            perform_gl_action([&](){
                cleanup_gpu_resources();
            }, []{});
        }

        void gpu_section::create_gpu_resources()
        {
            auto ptr = backup.get();
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                if (textures[i])
                {
                    glGenTextures(1, &textures[i]);
                    glBindTexture(GL_TEXTURE_2D, textures[i]);

                    if (types[i] == texture_type::RGB)
                    {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, ptr);
                        ptr += width * height * 3;
                    }
                    else if (types[i] == texture_type::XYZ)
                    {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, ptr);
                        ptr += width * height * 12;
                    }
                    else if (types[i] == texture_type::UV)
                    {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, width, height, 0, GL_RG, GL_FLOAT, ptr);
                        ptr += width * height * 8;
                    }

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                }
            }
            backup.reset();
        }

        gpu_section::operator bool()
        {
            bool res = false;
            for (int i = 0; i < MAX_TEXTURES; i++)
                if (loaded[i]) res = true;
            return res;
        }

        gpu_section::gpu_section()
        {
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                textures[i] = 0;
                loaded[i] = false;
            }
            initialize();
        }

        void gpu_section::on_publish()
        {
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                loaded[i] = false;
            }
        }

        void gpu_section::on_unpublish()
        {
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                loaded[i] = false;
            }
        }

        void gpu_section::output_texture(int id, uint32_t* tex, texture_type type)
        {
            auto existing_tex = textures[id];
            if (existing_tex)
                *tex = existing_tex;
            else
            {
                glGenTextures(1, tex);
                textures[id] = *tex;
            }
            loaded[id] = true;
            types[id] = type;
        }

        void gpu_section::set_size(uint32_t width, uint32_t height)
        {
            this->width = width; this->height = height;
        }

        bool gpu_section::input_texture(int id, uint32_t* tex)
        {
            if (loaded[id]) 
            {
                *tex = textures[id];
                return true;
            }
            return false;
        }

        int gpu_section::get_frame_size() const
        {
            int res = 0;
            for (int i = 0; i < MAX_TEXTURES; i++)
                if (textures[i] && loaded[i])
                {
                    if (types[i] == texture_type::RGB)
                    {
                        res += width * height * 3;
                    }
                    else if (types[i] == texture_type::XYZ)
                    {
                        res += width * height * 12;
                    }
                    else if (types[i] == texture_type::UV)
                    {
                        res += width * height * 8;
                    }
                }
            return res;
        }

        void gpu_section::fetch_frame(void* to)
        {
            bool need_to_fetch = false;
            for (int i = 0; i < MAX_TEXTURES; i++)
                if (loaded[i]) need_to_fetch = true;

            if (need_to_fetch)
            {
                perform_gl_action([&]{
                    auto ptr = (uint8_t*)to;

                    for (int i = 0; i < MAX_TEXTURES; i++)
                    if (textures[i] && loaded[i])
                    {
                        rs2::visualizer_2d vis;
                        rs2::fbo fbo(width, height);
                        uint32_t res;
                        glGenTextures(1, &res);
                        fbo.createTextureAttachment(res);

                        fbo.bind();
                        glViewport(0, 0, width, height);
                        glClearColor(0, 0, 0, 1);
                        glClear(GL_COLOR_BUFFER_BIT);
                        vis.draw_texture(textures[i]);
                        glReadBuffer(GL_COLOR_ATTACHMENT0);

                        if (types[i] == texture_type::RGB)
                        {
                            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, ptr);
                            ptr += width * height * 3;
                        }
                        else if (types[i] == texture_type::XYZ)
                        {
                            glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, ptr);
                            ptr += width * height * 12;
                        }
                        else if (types[i] == texture_type::UV)
                        {
                            glReadPixels(0, 0, width, height, GL_RG, GL_FLOAT, ptr);
                            ptr += width * height * 8;
                        }

                        glDeleteTextures(1, &res);
                        
                        fbo.unbind();
                    }
                }, [&]{
                    memcpy(to, backup.get(), get_frame_size());
                });
            }
        }

        context::context(GLFWwindow* share_with, glfw_binding binding) : _binding(binding)
        {
            if (binding.glfwInit) binding.glfwInit();

            binding.glfwWindowHint(GLFW_VISIBLE, 0);
            _ctx = binding.glfwCreateWindow(640, 480, "Offscreen Context", NULL, share_with);
            if (!_ctx)
            {
                throw std::runtime_error("Could not initialize offscreen context!");
            }

            auto curr = binding.glfwGetCurrentContext();
            binding.glfwMakeContextCurrent(_ctx);

            if (glShaderSource == nullptr)
            {
                gladLoadGLLoader((GLADloadproc)binding.glfwGetProcAddress);
            }

            binding.glfwSwapInterval(0);

            binding.glfwMakeContextCurrent(curr);
        }

        std::shared_ptr<void> context::begin_session()
        {
            auto curr = _binding.glfwGetCurrentContext();
            _lock.lock();
            _binding.glfwMakeContextCurrent(_ctx);
            auto me = shared_from_this();
            return std::shared_ptr<void>(nullptr, [curr, me](void*){
                me->_binding.glfwMakeContextCurrent(curr);
                me->_lock.unlock();
            });
        }

        context::~context()
        {
            _binding.glfwDestroyWindow(_ctx);
        }
    }
}
