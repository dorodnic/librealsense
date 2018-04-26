#pragma once

#include <librealsense2/hpp/rs_internal.hpp>
#include <librealsense2/rs.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/eval.h>

#include <memory>
#include <iostream>

#include <stb_image_write.h>
namespace bla
{
#include <res/int-rs-splash.hpp>
}

#include <stb_image.h>
#include <pybind11/numpy.h>

#include <map>
#include <vector>

const int W = 1280;
const int H = 720;

class software_camera;

class native_platform
{
public:
    uint16_t* get_pixels(int stream_id)
    {
        auto it = _frames.find(stream_id);
        if (it == _frames.end())
        {
            _frames[stream_id].resize(W * H);
            it = _frames.find(stream_id);
        }
        return it->second.data();
    }

    rs2_intrinsics create_depth_intrinsics() const
    {
        //rs2::pipeline p;
        //auto r = p.start();
        //auto intr = r.get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>().get_intrinsics();
        //std::cout << intr.width << "," << intr.height << "," << std::endl;
        //std::cout << intr.ppx << "," << intr.ppy << "," << std::endl;
        //std::cout << intr.fx << "," << intr.fy << "," << std::endl;
        rs2_intrinsics intrinsics = { 1280,720,
            633.445,368.654,
            944.49,944.49 ,
            RS2_DISTORTION_BROWN_CONRADY ,{ 0,0,0,0,0 } };

        return intrinsics;
    }

private:

    std::map<int, std::vector<uint16_t>> _frames;
};

class python_device
{
public:
    python_device();

    void start();
    void stop();

    rs2::software_device get_device() const;

    ~python_device();

private:
    std::shared_ptr<std::thread> _t;
    rs2::software_device _dev;
    rs2::software_sensor _sensor;
    rs2::stream_profile _left, _right, _depth, _depth_orig;

    std::atomic<bool> _alive;
    bool _streaming = false;

    pybind11::object _instance;
    native_platform _server;
    std::string _script;
};