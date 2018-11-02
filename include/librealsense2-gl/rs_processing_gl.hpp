// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_PROCESSING_GL_HPP
#define LIBREALSENSE_RS2_PROCESSING_GL_HPP

#include <librealsense2/rs.hpp>
#include "rs_processing_gl.h"

namespace rs2
{
    namespace gl
    {
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

            uint32_t get_texture_id() const
            {
                rs2_error * e = nullptr;
                auto r = rs2_gl_frame_get_texture_id(get(), &e);
                error::handle(e);
                return r;
            }
        };

        class yuy_to_rgb : public processing_block
        {
        public:
            /**
            * 
            */
            yuy_to_rgb() : processing_block(init(), 1) { }

        private:
            std::shared_ptr<rs2_processing_block> init()
            {
                rs2_error* e = nullptr;
                auto block = std::shared_ptr<rs2_processing_block>(
                    rs2_gl_create_yuy_to_rgb(&e),
                    rs2_delete_processing_block);
                error::handle(e);

                // Redirect options API to the processing block
                //options::operator=(pb);

                return block;
            }
        };
    }
}
#endif // LIBREALSENSE_RS2_PROCESSING_GL_HPP
