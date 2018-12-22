// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "core/video.h"
#include "synthetic-stream-gl.h"
#include "option.h"
#include "fbo.h"
#include "texture-2d-shader.h"

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
        gpu_section::gpu_section()
        {
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                textures[i] = 0;
                loaded[i] = false;
                contexts[i] = nullptr;
            }
        }

        gpu_section::~gpu_section()
        {
            // bool need_to_delete = false;
            // for (int i = 0; i < MAX_TEXTURES; i++)
            //     if (textures[i]) need_to_delete = true;

            // if (need_to_delete)
            //     main_thread_dispatcher::instance().invoke([&]()
            //     {
            //         glDeleteTextures(MAX_TEXTURES, textures);
            //     });
            for (int i = 0; i < MAX_TEXTURES; i++)
            {
                if (textures[i])
                {
                    auto session = contexts[i]->begin_session();
                    glDeleteTextures(1, &textures[i]);
                }
            }
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
                // if (textures[i])
                // {
                //     auto session = contexts[i]->begin_session();
                //     glDeleteTextures(1, &textures[i]);
                //     contexts[i] = nullptr;
                //     textures[i] = 0;
                // }

                loaded[i] = false;
            }

            _delayed_actions.clear();
        }

        void gpu_section::output_texture(int id, uint32_t* tex, texture_type type, std::shared_ptr<gl::context> ctx)
        {
            auto existing_tex = textures[id];
            if (existing_tex)
                *tex = existing_tex;
            else
            {
                contexts[id] = ctx;
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

        void gpu_section::delay(std::function<void()> action)
        {
            _delayed_actions.push_back(action);
        }

        void gpu_section::catch_up()
        {
            while (_delayed_actions.size())
            {
                auto act = _delayed_actions.front();
                _delayed_actions.pop_front();
                act();
            }
        }

        bool gpu_section::input_texture(int id, uint32_t* tex)
        {
            catch_up();
            if (loaded[id]) 
            {
                *tex = textures[id];
                return true;
            }
            return false;
        }

        void gpu_section::fetch_frame(void* to)
        {
            bool need_to_fetch = false;
            for (int i = 0; i < MAX_TEXTURES; i++)
                if (loaded[i]) need_to_fetch = true;

            if (need_to_fetch)
            {
                // main_thread_dispatcher::instance().invoke([&]()
                // {
                //     catch_up();
                    
                    auto ptr = (uint8_t*)to;
                    for (int i = 0; i < MAX_TEXTURES; i++)
                        if (textures[i] && loaded[i])
                        {
                            auto session = contexts[i]->begin_session();

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
                //});
            }
        }

        main_thread_dispatcher::main_thread_dispatcher()
            : _actions(10), _active(true)
        {
            
        }

        main_thread_dispatcher& main_thread_dispatcher::instance()
        {
            static main_thread_dispatcher inst;
            return inst;
        }

        void main_thread_dispatcher::stop()
        {
            _active = false;
            update();
        }

        bool main_thread_dispatcher::require_dispatch() const
        {
            auto myid = std::this_thread::get_id();
            return _main_thread != myid;
        }

        void main_thread_dispatcher::invoke(std::function<void()> action)
        {
            auto myid = std::this_thread::get_id();
            if (_main_thread == myid) action();
            else
            {
                std::promise<bool> prom;
                auto future = prom.get_future();

                std::function<void()> new_action = [action, &prom, this](){
                    try
                    {
                        if (_active) action();
                        prom.set_value(0);
                    }
                    catch(...)
                    {
                        prom.set_value(-1);
                    }
                };


                if (_active)
                {
                    _actions.enqueue(std::move(new_action));

                    std::future_status status;
                    do
                    {
                        status = future.wait_for(std::chrono::milliseconds(10));
                    }
                    while (status != std::future_status::ready && _active);
                    if (_active && future.get())
                        throw std::runtime_error("Async operation failed!");
                }
            }
        }

        void main_thread_dispatcher::update()
        {
            _main_thread = std::this_thread::get_id();
            std::function<void()> act;
            while (_actions.try_dequeue(&act)) act();
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
