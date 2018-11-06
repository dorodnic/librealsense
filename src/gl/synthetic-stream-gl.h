// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include "core/processing.h"
#include "image.h"
#include "source.h"
#include "../include/librealsense2/hpp/rs_frame.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"
#include "../include/librealsense2-gl/rs_processing_gl.hpp"

#include "concurrency.h"
#include <functional>
#include <thread>
#include <deque>

#define RS2_EXTENSION_VIDEO_FRAME_GL (rs2_extension)(RS2_EXTENSION_COUNT + RS2_GL_EXTENSION_VIDEO_FRAME)
#define MAX_TEXTURES 2

namespace librealsense
{ 
    namespace gl
    {
        enum class texture_type
        {
            RGB,
            XYZ,
            UV
        };


        class context : public std::enable_shared_from_this<context>
        {
        public:
            context(GLFWwindow* share_with);

            std::shared_ptr<void> begin_session();

            ~context();

        private:
            GLFWwindow* _ctx;
            std::mutex _lock;
        };

        class gpu_section
        {
        public:
            gpu_section();
            ~gpu_section();

            void on_publish();
            void on_unpublish();
            void fetch_frame(void* to);

            void catch_up();
            void delay(std::function<void()> action);

            bool input_texture(int id, uint32_t* tex);
            void output_texture(int id, uint32_t* tex, texture_type type, std::shared_ptr<gl::context> ctx);

            void set_size(uint32_t width, uint32_t height);

        private:
            uint32_t textures[MAX_TEXTURES];
            texture_type types[MAX_TEXTURES];
            bool loaded[MAX_TEXTURES];
            std::shared_ptr<gl::context> contexts[MAX_TEXTURES];
            uint32_t width, height;
            std::deque<std::function<void()>> _delayed_actions;
        };

        class gpu_addon_interface
        {
        public:
            virtual gpu_section& get_gpu_section() = 0;
            virtual ~gpu_addon_interface() = default;
        };

        template<class T>
        class gpu_addon : public T, public gpu_addon_interface
        {
        public:
            virtual gpu_section& get_gpu_section() override { return _section; }
            frame_interface* publish(std::shared_ptr<archive_interface> new_owner) override
            {
                _section.on_publish();
                return T::publish(new_owner);
            }
            void unpublish() override
            {
                _section.on_unpublish();
                T::unpublish();
            }
            const byte* get_frame_data() const override
            {
                auto res = T::get_frame_data();
                _section.fetch_frame((void*)res);
                return res;
            }
            gpu_addon() : T(), _section() {}
            gpu_addon(gpu_addon&& other)
                :T((T&&)std::move(other))
            {
            }
            gpu_addon& operator=(gpu_addon&& other)
            {
                return (gpu_addon&)T::operator=((T&&)std::move(other));
            }
        private:
            mutable gpu_section _section;
        };

        class main_thread_dispatcher
        {
        public:
            main_thread_dispatcher();
            void update();
            void invoke(std::function<void()> action);
            void stop();
            bool require_dispatch() const;

            static main_thread_dispatcher& instance();

        private:
            single_consumer_queue<std::function<void()>> _actions;
            std::thread::id _main_thread;
            std::atomic<bool> _active;
        };

        class gpu_video_frame : public gpu_addon<video_frame> {};
        class gpu_points_frame : public gpu_addon<points> {};
   }
}
