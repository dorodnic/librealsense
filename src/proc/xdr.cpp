// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"

#include "proc/synthetic-stream.h"
#include "context.h"
#include "environment.h"
#include "option.h"
#include "xdr.h"
#include "image.h"

namespace librealsense
{
    xdr::xdr() : generic_processing_block("Extended Dynamic Range")
    {
        _depth_last = {};
        _depth_prev = {};
        _ir_last = {};
        _ir_prev = {};
    }

    rs2::frame xdr::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        if (f.get_profile().stream_type() == RS2_STREAM_INFRARED)
        {
            if (!_ir_prev) _ir_prev = f;
            if (!_ir_last) _ir_last = f;
            _ir_prev = _ir_last;
            _ir_last = f;
        }
        if (f.is<rs2::depth_frame>())
        {
            if (!_depth_prev) _depth_prev = f;
            if (!_depth_last) _depth_last = f;
            _depth_prev = _depth_last;
            _depth_last = f;
        }

        if (_ir_prev && _ir_last && _depth_prev && _depth_last)
        {
            auto vf = _depth_last.as<rs2::depth_frame>();
            auto width = vf.get_width();
            auto height = vf.get_height();
            auto new_f = source.allocate_video_frame(f.get_profile(), f,
            vf.get_bytes_per_pixel(), width, height, vf.get_stride_in_bytes(), RS2_EXTENSION_DEPTH_FRAME);

            if (new_f && (fabs(_depth_last.get_timestamp() - _ir_last.get_timestamp()) < 
                fabs(_depth_last.get_timestamp() - _ir_prev.get_timestamp())))
            {
                auto ptr = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)new_f.get());
                auto orig = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)f.get());

                auto d1 = (uint16_t*)_depth_last.get_data();
                auto d0 = (uint16_t*)_depth_prev.get_data();
                auto i1 = (uint8_t*)_ir_last.get_data();
                auto i0 = (uint8_t*)_ir_prev.get_data();

                auto new_data = (uint16_t*)ptr->get_frame_data();

                ptr->set_sensor(orig->get_sensor());

                memset(new_data, 0, width * height * sizeof(uint16_t));
                for (int i = 0; i < width * height; i++)
                {
                    if (i1[i] > 0x0f && i1[i] < 0xf0)
                        new_data[i] = d1[i];
                    else
                        new_data[i] = d0[i];
                }

                return new_f;
            }
            else
            {
                return f;
            }
        }

        return f;
    }
}
