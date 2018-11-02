// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "core/video.h"
#include "synthetic-stream-gl.h"
#include "option.h"
#include "fbo.h"
#include "texture-2d-shader.h"

#define NOMINMAX

#include <glad/glad.h>

#include <iostream>

namespace librealsense
{
    namespace gl
    {
        void gpu_section::on_publish()
        {
            texture = 0;
        }

        void gpu_section::on_unpublish()
        {
            if (texture)
            {
                glDeleteTextures(1, &texture);
            }
        }

        void gpu_section::fetch_frame(void* to) const
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
            vis.draw_texture(texture);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, to);

            glDeleteTextures(1, &res);
            
            fbo.unbind();
        }
    }
}
