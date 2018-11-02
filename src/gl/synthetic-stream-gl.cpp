// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "core/video.h"
#include "synthetic-stream-gl.h"
#include "option.h"

#define NOMINMAX

#include <glad/glad.h>

#include <iostream>

namespace librealsense
{
    namespace gl
    {
        void gpu_section::on_publish()
        {
            texture1 = 0;
            texture2 = 0;
        }

        void gpu_section::on_unpublish()
        {
            if (texture1)
            {
                glDeleteTextures(1, &texture1);
                //std::cout << "Deallocating " << texture1 << std::endl;
            }
        }
    }
}
