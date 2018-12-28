// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include "core/processing.h"
#include "image.h"
#include "source.h"
#include "../include/librealsense2/hpp/rs_frame.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"
#include "../include/librealsense2-gl/rs_processing_gl.hpp"
#include "opengl3.h"

#include "concurrency.h"
#include <functional>
#include <thread>
#include <deque>
#include <unordered_set>


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

        class gpu_object;
        class gpu_processing_object;
        class gpu_rendering_object;

        class context : public std::enable_shared_from_this<context>
        {
        public:
            context(GLFWwindow* share_with, glfw_binding binding);

            std::shared_ptr<void> begin_session();

            ~context();

        private:
            GLFWwindow* _ctx;
            glfw_binding _binding;
            std::mutex _lock;
        };

        struct lane
        {
            std::unordered_set<gpu_object*> objs;
            std::mutex mutex;
            std::atomic_bool active { false };
            bool use_glsl = false;

            void register_gpu_object(gpu_object* obj)
            {
                std::lock_guard<std::mutex> lock(mutex);
                objs.insert(obj);
            }

            void unregister_gpu_object(gpu_object* obj)
            {
                std::lock_guard<std::mutex> lock(mutex);
                auto it = objs.find(obj);
                objs.erase(it);
            }
        };

        // This represents a persistent object that holds context and other GL stuff
        // The context within it might change, but the lane will remain
        // This is done to simplify GL objects lifetime management - 
        // Once processing blocks are created and in use, and frames have been distributed
        // to various queues and variables, it can be very hard to properly refresh 
        // them if context changes (windows closes, resolution change or any other reason)

        // Processing vs Rendering objects require slightly different handling

        class rendering_lane
        {
        public:
            void register_gpu_object(gpu_rendering_object* obj);

            void unregister_gpu_object(gpu_rendering_object* obj);

            void init(bool use_glsl);

            void shutdown();

            static rendering_lane& instance();

            bool is_active() const { return _data.active; }
            bool glsl_enabled() const { return _data.use_glsl; }
        protected:
            lane _data;
        };

        class matrix_container
        {
        public:
            matrix_container()
            {
                for (auto i = 0; i < RS2_GL_MATRIX_COUNT; i++)
                    m[i] = rs2::matrix4::identity();
            }
            const rs2::matrix4& get_matrix(rs2_gl_matrix_type type) const { return m[type]; }
            void set_matrix(rs2_gl_matrix_type type, const rs2::matrix4& val) { m[type] = val; }
            virtual ~matrix_container() {}

        private:
            rs2::matrix4 m[RS2_GL_MATRIX_COUNT];
        };

        class processing_lane
        {
        public:
            static processing_lane& instance();

            void register_gpu_object(gpu_processing_object* obj);

            void unregister_gpu_object(gpu_processing_object* obj);

            void init(GLFWwindow* share_with, glfw_binding binding, bool use_glsl);

            void shutdown();

            bool is_active() const { return _data.active; }
            std::shared_ptr<context> get_context() const { return _ctx; }
            bool glsl_enabled() const { return _data.use_glsl; }
        private:
            lane _data;
            std::shared_ptr<context> _ctx;
        };

        class gpu_object
        {
        private:
            friend class processing_lane;
            friend class rendering_lane;

            void update_gpu_resources(bool use_glsl)
            {
                _use_glsl = use_glsl;
                if (_needs_cleanup.fetch_xor(1))
                    cleanup_gpu_resources();
                else
                    create_gpu_resources();
            }
        protected:
            virtual void cleanup_gpu_resources() = 0;
            virtual void create_gpu_resources() = 0;

            bool glsl_enabled() const { return _use_glsl; }

            bool need_cleanup() { _needs_cleanup = 1; }
            void use_glsl(bool val) { _use_glsl = val; }

        private:
            std::atomic_int _needs_cleanup { 0 };
            bool _use_glsl = false;
        };

        class gpu_rendering_object : public gpu_object
        {
        public:
            gpu_rendering_object()
            {
                rendering_lane::instance().register_gpu_object(this);
            }
            virtual ~gpu_rendering_object()
            {
                rendering_lane::instance().unregister_gpu_object(this);
            }

        protected:
            void initialize() {
                use_glsl(rendering_lane::instance().glsl_enabled());
                if (rendering_lane::instance().is_active())
                    create_gpu_resources();
                need_cleanup();
            }

            template<class T>
            void perform_gl_action(T action)
            {
                if (rendering_lane::instance().is_active())
                    action();
            }
        };

        class gpu_processing_object : public gpu_object
        {
        public:
            gpu_processing_object()
            {
                processing_lane::instance().register_gpu_object(this);
            }
            virtual ~gpu_processing_object()
            {
                processing_lane::instance().unregister_gpu_object(this);
            }

            void set_context(std::weak_ptr<context> ctx) { _ctx = ctx; }
        protected:
            void initialize() {
                if (processing_lane::instance().is_active())
                {
                    _ctx = processing_lane::instance().get_context();
                    use_glsl(processing_lane::instance().glsl_enabled());
                    perform_gl_action([this](){
                        create_gpu_resources();
                    }, []{});
                    need_cleanup();
                }
            }

            template<class T, class S>
            void perform_gl_action(T action, S fallback)
            {
                auto ctx = _ctx.lock();
                if (ctx)
                {
                    auto session = ctx->begin_session();
                    if (processing_lane::instance().is_active())
                        action();
                    else
                        fallback();
                }
                else fallback();
            }

        private:
            std::weak_ptr<context> _ctx;
        };

        class gpu_section : public gpu_processing_object
        {
        public:
            gpu_section();
            ~gpu_section();

            void on_publish();
            void on_unpublish();
            void fetch_frame(void* to);

            bool input_texture(int id, uint32_t* tex);
            void output_texture(int id, uint32_t* tex, texture_type type);

            void set_size(uint32_t width, uint32_t height);

            void cleanup_gpu_resources() override;
            void create_gpu_resources() override;

            int get_frame_size() const;

            bool on_gpu() const { return !backup.get(); }

            operator bool();

        private:
            uint32_t textures[MAX_TEXTURES];
            texture_type types[MAX_TEXTURES];
            bool loaded[MAX_TEXTURES];
            uint32_t width, height;
            bool backup_content = true;
            std::unique_ptr<uint8_t[]> backup;
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

        class gpu_video_frame : public gpu_addon<video_frame> {};
        class gpu_points_frame : public gpu_addon<points> {};
   }
}
