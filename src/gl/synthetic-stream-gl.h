// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include "core/processing.h"
#include "image.h"
#include "source.h"
#include "../include/librealsense2/hpp/rs_frame.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"

#define RS2_EXTENSION_VIDEO_FRAME_GL (rs2_extension)(RS2_EXTENSION_COUNT + RS2_GL_EXTENSION_VIDEO_FRAME)

namespace librealsense
{ 
    namespace gl
    {
        class gpu_section
        {
        public:
            void on_publish();
            void on_unpublish();

            uint32_t texture1 = 0;
            uint32_t texture2 = 0;
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
        private:
            gpu_section _section;
        };

        class gpu_video_frame : public gpu_addon<video_frame> {};
   }
}
