// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

void save_frame_depth_data(const std::string& filename,
                           rs2::frame frame,
                           float depth_units)
{
    if (auto image = frame.as<rs2::video_frame>())
    {
        std::ofstream myfile;
        std::stringstream fullname;
        fullname << filename << "_" << frame.get_frame_number() << ".csv";
        myfile.open(fullname.str());
        myfile << std::setprecision(2);
        auto pixels = (uint16_t*)image.get_data();
        
        for (auto y = 0; y < image.get_height(); y++)
        {
            for (auto x = 0; x < image.get_width(); x++)
            {
                myfile << pixels[image.get_width() * y + x] * depth_units << ", ";
            }
            myfile << "\n";
        }
        
        myfile.close();
    }
}

void save_frame_color_data(const std::string& filename,
                           rs2::frame frame)
{
    if (auto image = frame.as<rs2::video_frame>())
    {
        std::ofstream myfile;
        std::stringstream fullname;
        fullname << filename << "_" << frame.get_frame_number() << ".csv";
        myfile.open(fullname.str());
        myfile << std::hex << std::setw(2) << std::setfill('0');
        auto pixels = (uint8_t*)image.get_data();
        
        for (auto y = 0; y < image.get_height(); y++)
        {
            for (auto x = 0; x < image.get_width(); x++)
            {
                myfile << "0x";
                for (auto c = 0; c < 3; c++)
                {
                    myfile << (int)pixels[3*(image.get_width() * y + x) + c];
                }
                myfile << ",";
            }
            myfile << "\n";
        }
        
        myfile.close();
    }
}

// Capture Example demonstrates how to
// capture depth and color video streams and render them to the screen
int main(int argc, char * argv[]) try
{
    rs2::colorizer color_map;
    
    auto depth_units = rs2::context().query_devices().front()
        .query_sensors().front().get_option(RS2_OPTION_DEPTH_UNITS);
    
    rs2::pipeline pipe;
    pipe.start();

    while(true) // Application still alive?
    {
        rs2::frameset data = pipe.wait_for_frames(); // Wait for next set of frames from the camera

        rs2::frame depth = data.get_depth_frame(); // Find and colorize the depth data
        rs2::frame color = data.get_color_frame();            // Find the color data

        save_frame_depth_data("depth", depth, depth_units);
        save_frame_color_data("color", color);
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



