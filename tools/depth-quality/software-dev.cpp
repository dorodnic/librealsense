#include "software-dev.hpp"
#include <iostream>
#include <chrono>

#include <pybind11/numpy.h>
#include <fstream>
#include <streambuf>

using namespace rs2;

std::string load_script(const std::string& fname)
{
    std::ifstream t(fname);
    if (!t.good()) return "CANT LOAD";
    std::string str((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
    return str;
}

class software_camera
{
public:
    software_camera(native_platform*);
    virtual ~software_camera() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void update() = 0;

    uint16_t* get_pixels(int stream_id) { return _server.get_pixels(stream_id); }
private:
    native_platform& _server;
};

///////////////////////////////////

software_camera::software_camera(native_platform* server) : _server(*server) {}

namespace py = pybind11;

class py_software_camera final : public software_camera
{
    using software_camera::software_camera;

    void update() override
    {
        PYBIND11_OVERLOAD_PURE(
            void,              // return type
            software_camera,  // super class
            update            // function name
        );
    }

    void start() override
    {
        PYBIND11_OVERLOAD_PURE(
            void,              // return type
            software_camera,  // super class
            start
        );
    }

    void stop() override
    {
        PYBIND11_OVERLOAD_PURE(
            void,              // return type
            software_camera,  // super class
            stop
        );
    }
};

PYBIND11_PLUGIN()
{
    py::module m("device_framework", "Python integration into Viewer");

    py::class_<native_platform>(m, "native_platform");

    py::class_<software_camera, py_software_camera>(m, "software_camera")
        .def(py::init<native_platform*>())
        .def("start", &software_camera::start)
        .def("stop", &software_camera::stop)
        .def("update", &software_camera::update)
        .def("upload_z", [](py_software_camera& self, py::array_t<uint16_t> x, int stream_id) {
        //std::cout << "bla" << std::endl;
        auto r = x.mutable_unchecked<2>(); // Will throw if ndim != 3 or flags.writeable is false

        auto pixels = self.get_pixels(stream_id);

        for (ssize_t i = 0; i < r.shape(0); i++)
            for (ssize_t j = 0; j < r.shape(1); j++)
            {
                *pixels = r(i, j);
                pixels++;
            }
    }, py::arg().noconvert(), py::arg().none())
        .def("upload_y", [](py_software_camera& self, py::array_t<uint8_t> x, int stream_id) {
        //std::cout << "bla" << std::endl;
        auto r = x.mutable_unchecked<2>(); // Will throw if ndim != 3 or flags.writeable is false

        auto pixels = (uint8_t*)self.get_pixels(stream_id);

        for (ssize_t i = 0; i < r.shape(0); i++)
            for (ssize_t j = 0; j < r.shape(1); j++)
            {
                *pixels = r(i, j);
                pixels++;
            }
    }, py::arg().noconvert(), py::arg().none());

    return m.ptr();
}

py::object import(const std::string& module, const std::string& path, py::object& globals)
{
    py::dict locals;
    locals["module_name"] = py::cast(module);
    locals["path"] = py::cast(path);

    py::eval<py::eval_statements>(
        "import imp\n"
        "new_module = imp.load_module(module_name, open(path), path, ('py', 'U', imp.PY_SOURCE))\n",
        globals,
        locals);

    return locals["new_module"];
}

software_device python_device::get_device() const
{
    return _dev;
}

void python_device::start()
{
    py::object main = py::module::import("__main__");
    py::object globals = main.attr("__dict__");
    py::object module = import("script", "script.py", globals);
    py::object Strategy = module.attr("depth_from_stereo");
    _instance = Strategy(&_server);
}

void python_device::stop()
{
    //pybind11::detail::clear_instance(module);
    //Py_Finalize();
}

python_device::python_device()
{
    _alive = true;

    rs2_intrinsics depth_intrinsics = _server.create_depth_intrinsics();

    //==================================================//
    //           Declare Software-Only Device           //
    //==================================================//


    _sensor = _dev.add_sensor("Stereo Module"); // Define single sensor

    _depth = _sensor.add_video_stream({ RS2_STREAM_DEPTH, 0, 2,
        W, H, 30, BPP,
        RS2_FORMAT_Z16, depth_intrinsics });

    _left = _sensor.add_video_stream({ RS2_STREAM_INFRARED, 0, 0,
        W, H, 30, 1,
        RS2_FORMAT_Y8, depth_intrinsics });

    _right = _sensor.add_video_stream({ RS2_STREAM_INFRARED, 1, 1,
        W, H, 30, 1,
        RS2_FORMAT_Y8, depth_intrinsics });

    _sensor.add_read_only_option(RS2_OPTION_DEPTH_UNITS, 0.001f);

    _dev.create_matcher(RS2_MATCHER_DLR);

    _t = std::make_shared<std::thread>([=]() {
        int frame_number = 0;

        Py_Initialize();
        pybind11_init();

        _script = load_script("script.py");

        while (_alive) // Application still alive?
        {
            try
            {
                auto refresh = false;
                auto new_script = load_script("script.py");

                if (new_script != _script)
                {
                    _script = new_script;
                    refresh = true;
                    std::cout << "Reloading Script!" << std::endl;
                }

                if (_streaming && (!_dev.query_sensors().front().is_streaming() || refresh))
                {
                    stop();
                    _streaming = false;
                    _instance.attr("stop")();
                }
                if (!_streaming && (_dev.query_sensors().front().is_streaming() || refresh))
                {
                    start();
                    _streaming = true;
                    _instance.attr("start")();
                }

                if (_streaming)
                {
                    _instance.attr("update")();
                }

                auto pixels = _server.get_pixels(2);
                _sensor.on_video_frame({ pixels, // Frame pixels from capture API
                    [](void*) {}, // Custom deleter (if required)
                    W * BPP, BPP, // Stride and Bytes-per-pixel
                    (rs2_time_t)frame_number * 16, RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, frame_number, // Timestamp, Frame# for potential sync services
                    _depth });

                pixels = _server.get_pixels(0);
                _sensor.on_video_frame({ pixels, // Frame pixels from capture API
                    [](void*) {}, // Custom deleter (if required)
                    W * 1, 1, // Stride and Bytes-per-pixel
                    (rs2_time_t)frame_number * 16, RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, frame_number, // Timestamp, Frame# for potential sync services
                    _left });

                pixels = _server.get_pixels(1);
                _sensor.on_video_frame({ pixels, // Frame pixels from capture API
                    [](void*) {}, // Custom deleter (if required)
                    W * 1, 1, // Stride and Bytes-per-pixel
                    (rs2_time_t)frame_number * 16, RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, frame_number, // Timestamp, Frame# for potential sync services
                    _right });

                ++frame_number;
            }
            catch (const std::exception& ex)
            {
                std::cout << ex.what() << std::endl;
                _streaming = false;

                while (_alive && load_script("script.py") == _script)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (...)
            {
                std::cout << "Unknown error!" << std::endl;
                _streaming = false;

                while (_alive && load_script("script.py") == _script)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    });

}

python_device::~python_device()
{
    _alive = false;
    _t->join();
    _t.reset();
}