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

        /**
        * GL context maps to OpenGL rendering context
        * Providing one to a processing block allows that processing block
        * to run in parallel with other OpenGL calls.
        * includes realsense API version as provided by RS2_API_VERSION macro
        */
        class context
        {
        public:
            context()
            {
                rs2_error* e = nullptr;
                _context = std::shared_ptr<rs2_gl_context>(
                    rs2_gl_create_context(RS2_API_VERSION, &e),
                    rs2_gl_delete_context);
                error::handle(e);
            }

#ifdef _glfw3_h_
            context(GLFWwindow* share_with)
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

                _context = std::shared_ptr<rs2_gl_context>(
                    rs2_gl_create_shared_context(RS2_API_VERSION, share_with, binding, &e),
                    rs2_gl_delete_context);
                error::handle(e);
            }
#endif
        protected:
            friend class rs2::gl::pointcloud;
            friend class rs2::gl::yuy_to_rgb;

            std::shared_ptr<rs2_gl_context> _context;
        };

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

        class yuy_to_rgb : public filter
        {
        public:
            /**
            * 
            */
            yuy_to_rgb(context ctx = context()) : filter(init(ctx), 1) { }

        private:
            std::shared_ptr<rs2_processing_block> init(context ctx)
            {
                rs2_error* e = nullptr;
                auto block = std::shared_ptr<rs2_processing_block>(
                    rs2_gl_create_yuy_to_rgb(ctx._context.get(), &e),
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
            pointcloud(context ctx = context()) : rs2::pointcloud(init(ctx)) {}

            pointcloud(rs2_stream stream, int index = 0, context ctx = context()) 
                : rs2::pointcloud(init(ctx))
            {
                set_option(RS2_OPTION_STREAM_FILTER, float(stream));
                set_option(RS2_OPTION_STREAM_INDEX_FILTER, float(index));
            }

        private:
            friend class context;

            std::shared_ptr<rs2_processing_block> init(context ctx)
            {
                rs2_error* e = nullptr;

                auto block = std::shared_ptr<rs2_processing_block>(
                    rs2_gl_create_pointcloud(ctx._context.get(), &e),
                    rs2_delete_processing_block);

                error::handle(e);

                // Redirect options API to the processing block
                //options::operator=(pb);
                return block;
            }
        };

        inline void update_all()
        {
            rs2_error* e = nullptr;
            rs2_gl_update_all(RS2_API_VERSION, &e);
            error::handle(e);
        }

        inline void stop_all()
        {
            rs2_error* e = nullptr;
            rs2_gl_stop_all(RS2_API_VERSION, &e);
            error::handle(e);
        }
    }
}
#endif // LIBREALSENSE_RS2_PROCESSING_GL_HPP
