// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"

#include "proc/synthetic-stream.h"
#include "context.h"
#include "environment.h"
#include "option.h"
#include "xdr.h"
#include "log.h"
#include "image.h"

namespace librealsense
{
    xdr::xdr() : generic_processing_block("Extended Dynamic Range")
    {
        _s.start(_queue);
    }

    rs2::frame xdr::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        std::vector<rs2::frame> frames;

        if (auto fs = f.as<rs2::frameset>())
        {
            if (fs.size() == 2)
            {
                _frames[fs.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE)] = fs;
            }
        }
        else
            frames.push_back(f);

        for (auto&& q : frames) _s.invoke(q);

        rs2::frame fr;
        while (_queue.poll_for_frame(&fr))
        {
            if (auto fs = fr.as<rs2::frameset>())
            {
                if (fs.size() == 2)
                {
                    _frames[fs.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE)] = fs;
                }
            }
        }

        std::vector<rs2::frameset> fss;
        for (auto&& kvp : _frames) fss.push_back(kvp.second);
        std::sort(fss.begin(), fss.end(),
            [](const rs2::frame& a, const rs2::frame& b) -> bool
        {
            return a.get_timestamp() > b.get_timestamp();
        });
        //fss.resize(2);
        if (fss.size() > 1) fss.resize(2);

        using namespace std::chrono;
        static system_clock::time_point last_time = system_clock::now();
        if (system_clock::now() - last_time > milliseconds(1000))
        {
            last_time = system_clock::now();

            for (auto&& f : fss)
            {
                LOG(WARNING) << f.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE) << " - " << std::fixed << f.get_timestamp();

            }
            LOG(WARNING) << "";
        }

        if (fss.size() > 1)
        {
            auto latest = fss[0];
            auto last = fss[1];

            auto latest_depth = latest.get_depth_frame();
            auto last_depth = last.get_depth_frame();
            auto latest_ir = latest.get_infrared_frame();
            auto last_ir = last.get_infrared_frame();

            auto vf = latest_depth.as<rs2::depth_frame>();
            auto width = vf.get_width();
            auto height = vf.get_height();
            auto new_f = source.allocate_video_frame(latest_depth.get_profile(), latest_depth,
                vf.get_bytes_per_pixel(), width, height, vf.get_stride_in_bytes(), RS2_EXTENSION_DEPTH_FRAME);

            if (new_f)
            {
                auto ptr = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)new_f.get());
                auto orig = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)latest_depth.get());

                auto d1 = (uint16_t*)latest_depth.get_data();
                auto d0 = (uint16_t*)last_depth.get_data();
                auto i1 = (uint8_t*)latest_ir.get_data();
                auto i0 = (uint8_t*)last_ir.get_data();

                auto new_data = (uint16_t*)ptr->get_frame_data();

                ptr->set_sensor(orig->get_sensor());

                memset(new_data, 0, width * height * sizeof(uint16_t));
                for (int i = 0; i < width * height; i++)
                {
                    if (i1[i] > 0x08 && i1[i] < 0xf8 && d1[i])
                        new_data[i] = d1[i];
                    else if (i0[i] > 0x08 && i0[i] < 0xf8 && d0[i])
                        new_data[i] = d0[i];
                    else if (d1[i] && d0[i]) 
                        new_data[i] = std::min(d0[i], d1[i]);
                    else if (d1[i])
                        new_data[i] = d1[i];
                    else if (d0[i])
                        new_data[i] = d0[i];
                    if (new_data[i] == 0xffff) new_data[i] = 0;
                }

                return new_f;
            }
        }

        /*for (auto&& q : frames)
        {
            if (q.get_profile().stream_type() == RS2_STREAM_INFRARED)
            {
                _ir[q.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE)] = q;
            }
            if (q.is<rs2::depth_frame>())
            {
                _depth[q.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE)] = q;
            }
        }

        if (_depth.size() > 1 && _ir.size() > 1 && _depth.size() == _ir.size())
        {
            for (int i = 0; i < _ir.size(); i++)
            {
                if (abs(_ir[i].get_timestamp() - _depth[i].get_timestamp()) > 1.f)
                    return f;
            }

            auto min_exp = 10000000;
            auto max_exp = 0;
            for (auto&& kvp : _depth)
            {
                min_exp = std::min(min_exp, kvp.first);
                max_exp = std::max(max_exp, kvp.first);
            }

            auto vf = _depth[max_exp].as<rs2::depth_frame>();
            auto width = vf.get_width();
            auto height = vf.get_height();
            auto new_f = source.allocate_video_frame(_depth[max_exp].get_profile(), _depth[max_exp],
            vf.get_bytes_per_pixel(), width, height, vf.get_stride_in_bytes(), RS2_EXTENSION_DEPTH_FRAME);

            if (new_f)
            {
                auto ptr = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)new_f.get());
                auto orig = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)_depth[max_exp].get());

                auto d1 = (uint16_t*)_depth[max_exp].get_data();
                auto d0 = (uint16_t*)_depth[min_exp].get_data();
                auto i1 = (uint8_t*)_ir[max_exp].get_data();
                auto i0 = (uint8_t*)_ir[min_exp].get_data();

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
        }*/

        return f;
    }
}
