// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_PROCESSING_GL_HPP
#define LIBREALSENSE_RS2_PROCESSING_GL_HPP

#include <librealsense2/rs.hpp>
#include "rs_processing_gl.h"

#include <memory>

namespace rs2
{
    namespace gl
    {
        class pointcloud;
        class yuy_to_rgb;

        inline void shutdown_rendering()
        {
            rs2_error* e = nullptr;
            rs2_gl_shutdown_rendering(RS2_API_VERSION, &e);
            error::handle(e);
        }

#ifdef _glfw3_h_
        inline void init_rendering(bool use_glsl = true)
        {
            rs2_error* e = nullptr;

            glfw_binding binding{
                nullptr,
                &glfwWindowHint,
                &glfwCreateWindow,
                &glfwDestroyWindow,
                &glfwMakeContextCurrent,
                &glfwGetCurrentContext,
                &glfwSwapInterval,
                &glfwGetProcAddress
            };

            rs2_gl_init_rendering_glfw(RS2_API_VERSION, binding, use_glsl ? 1 : 0, &e);
            error::handle(e);
        }
#else
        inline void init_rendering(bool use_glsl = true)
        {
            rs2_error* e = nullptr;
            rs2_gl_init_rendering(RS2_API_VERSION, use_glsl ? 1 : 0, &e);
            error::handle(e);
        }
#endif

        inline void init_processing(bool use_glsl = true)
        {
            rs2_error* e = nullptr;
            rs2_gl_init_processing(RS2_API_VERSION, use_glsl ? 1 : 0, &e);
            error::handle(e);
        }

        inline void shutdown_processing()
        {
            rs2_error* e = nullptr;
            rs2_gl_shutdown_processing(RS2_API_VERSION, &e);
            error::handle(e);
        }

#ifdef _glfw3_h_
        inline void init_processing(GLFWwindow* share_with, bool use_glsl = true)
        {
            rs2_error* e = nullptr;

            glfw_binding binding{
                    nullptr,
                    &glfwWindowHint,
                    &glfwCreateWindow,
                    &glfwDestroyWindow,
                    &glfwMakeContextCurrent,
                    &glfwGetCurrentContext,
                    &glfwSwapInterval,
                    &glfwGetProcAddress
                };

            rs2_gl_init_processing_glfw(RS2_API_VERSION, share_with, binding, use_glsl ? 1 : 0, &e);
            error::handle(e);
        }
#endif

        class gpu_frame : public frame
        {
        public:
            /**
            * Inherit video_frame class with additional depth related attributs/functions
            * \param[in] frame - existing frame instance
            */
            gpu_frame(const frame& f)
                : frame(f)
            {
                rs2_error* e = nullptr;
                if (!f || (rs2_gl_is_frame_extendable_to(f.get(), RS2_GL_EXTENSION_VIDEO_FRAME, &e) == 0 && !e))
                {
                    reset();
                }
                error::handle(e);
            }

            uint32_t get_texture_id(unsigned int id = 0) const
            {
                rs2_error * e = nullptr;
                auto r = rs2_gl_frame_get_texture_id(get(), id, &e);
                error::handle(e);
                return r;
            }
        };

        class yuy_decoder : public rs2::yuy_decoder
        {
        public:
            /**
            * 
            */
            yuy_decoder() : rs2::yuy_decoder(init()) { }

        private:
            std::shared_ptr<rs2_processing_block> init()
            {
                rs2_error* e = nullptr;
                auto block = std::shared_ptr<rs2_processing_block>(
                    rs2_gl_create_yuy_decoder(RS2_API_VERSION, &e),
                    rs2_delete_processing_block);
                error::handle(e);

                // Redirect options API to the processing block
                //options::operator=(pb);

                return block;
            }
        };

        /**
        * Generating the 3D point cloud base on depth frame also create the mapped texture.
        */
        class pointcloud : public rs2::pointcloud
        {
        public:
            /**
            * create pointcloud instance
            */
            pointcloud() : rs2::pointcloud(init()) {}

            pointcloud(rs2_stream stream, int index = 0) 
                : rs2::pointcloud(init())
            {
                set_option(RS2_OPTION_STREAM_FILTER, float(stream));
                set_option(RS2_OPTION_STREAM_INDEX_FILTER, float(index));
            }

        private:
            friend class context;

            std::shared_ptr<rs2_processing_block> init()
            {
                rs2_error* e = nullptr;

                auto block = std::shared_ptr<rs2_processing_block>(
                    rs2_gl_create_pointcloud(RS2_API_VERSION, &e),
                    rs2_delete_processing_block);

                error::handle(e);

                // Redirect options API to the processing block
                //options::operator=(pb);
                return block;
            }
        };

        class camera_renderer : public rs2::filter
        {
        public:
            camera_renderer() : rs2::filter(init()) {}

            void set_matrix(rs2_gl_matrix_type type, float* m4x4)
            {
                rs2_error* e = nullptr;
                rs2_gl_set_matrix(get(), type, m4x4, &e);
                error::handle(e);
            }

        private:
            friend class context;

            std::shared_ptr<rs2_processing_block> init()
            {
                rs2_error* e = nullptr;

                auto block = std::shared_ptr<rs2_processing_block>(
                    rs2_gl_create_camera_renderer(RS2_API_VERSION, &e),
                    rs2_delete_processing_block);

                error::handle(e);
                return block;
            }
        };

        class pointcloud_renderer : public rs2::filter
        {
        public:
            pointcloud_renderer() : rs2::filter(init()) {}

            void set_matrix(rs2_gl_matrix_type type, float* m4x4)
            {
                rs2_error* e = nullptr;
                rs2_gl_set_matrix(get(), type, m4x4, &e);
                error::handle(e);
            }

            static const auto OPTION_FILLED = rs2_option(RS2_OPTION_COUNT + 1);
        private:
            friend class context;

            std::shared_ptr<rs2_processing_block> init()
            {
                rs2_error* e = nullptr;

                auto block = std::shared_ptr<rs2_processing_block>(
                    rs2_gl_create_pointcloud_renderer(RS2_API_VERSION, &e),
                    rs2_delete_processing_block);

                error::handle(e);
                return block;
            }
        };
    }
}
#endif // LIBREALSENSE_RS2_PROCESSING_GL_HPP
