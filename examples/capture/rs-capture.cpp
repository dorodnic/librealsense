// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>
#include <librealsense/rs.hpp>
#include "example.hpp"

int main(int argc, char * argv[]) try
{
    rs2_intrinsics color_intrinsics;
    rs2_intrinsics depth_intrinsics;
    rs2_extrinsics depth_to_color;

    rs2::software_device software_dev;
    auto depth_sensor = software_dev.add_sensor("Depth");
    auto color_sensor = software_dev.add_sensor("Color"); 
    rs2::syncer sync;

    rs::context ctx;
    rs::device& dev = *ctx.get_device(0);
    dev.enable_stream(rs::stream::color, 640, 480, rs::format::rgb8, 30);
    dev.enable_stream(rs::stream::depth, 640, 480, rs::format::z16, 30);

    rs_intrinsics depth_intr = dev.get_stream_intrinsics(rs::stream::depth);
    rs_intrinsics color_intr = dev.get_stream_intrinsics(rs::stream::color);
    color_intrinsics = *((rs2_intrinsics*)&color_intr);
    depth_intrinsics = *((rs2_intrinsics*)&depth_intr);
    rs_extrinsics extr = dev.get_extrinsics(rs::stream::depth, rs::stream::color);
    depth_to_color = *((rs2_extrinsics*)&extr);

    auto depth_stream = depth_sensor.add_video_stream({ RS2_STREAM_DEPTH, 0, 0,
        depth_intr.width, depth_intr.height, 30, 2,
        RS2_FORMAT_Z16, depth_intrinsics });

    auto color_stream = color_sensor.add_video_stream({ RS2_STREAM_COLOR, 0, 0,
        color_intr.width, color_intr.height, 30, 3,
        RS2_FORMAT_RGB8, color_intrinsics });

    depth_stream.register_extrinsics_to(color_stream, depth_to_color);

    depth_sensor.add_read_only_option(RS2_OPTION_DEPTH_UNITS, 0.001f);
    software_dev.create_matcher(RS2_MATCHER_DLR_C);

    dev.start();

    rs2::recorder rec("1.bag", software_dev);
    for (auto&& sensor : rec.query_sensors())
    {
        sensor.open(sensor.get_stream_profiles().front());
        sensor.start(sync);
    }

    for (int i = 0; i < 200; i++)
    {
        dev.wait_for_frames();
        
        depth_sensor.on_video_frame({ (void*)dev.get_frame_data(rs::stream::depth),
            [](void*) {}, // Custom deleter (if required)
            depth_intr.width * 2, 2, // Stride and Bytes-per-pixel
            (rs2_time_t)dev.get_frame_timestamp(rs::stream::depth), RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, (int)dev.get_frame_number(rs::stream::depth),
            depth_stream });

        color_sensor.on_video_frame({ (void*)dev.get_frame_data(rs::stream::color),
            [](void*) {}, // Custom deleter (if required)
            color_intr.width * 3, 3, // Stride and Bytes-per-pixel
            (rs2_time_t)dev.get_frame_timestamp(rs::stream::color), RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, (int)dev.get_frame_number(rs::stream::color),
            color_stream });
    }

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}



