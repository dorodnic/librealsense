// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include "model-views.h"
#include "ux-window.h"

#include "software-dev.hpp"

#include <cstdarg>
#include <thread>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <array>
#include <mutex>
#include <set>

#include <imgui_internal.h>

// We use NOC file helper function for cross-platform file dialogs
#include <noc_file_dialog.h>

#include <pybind11/pybind11.h>
#include <pybind11/eval.h>

using namespace rs2;
using namespace rs400;

#include <librealsense2/hpp/rs_internal.hpp>

#include <stb_image_write.h>
namespace bla
{
    #include <res/int-rs-splash.hpp>
}

#include <stb_image.h>

const int W = 640;
const int H = 480;
const int BPP = 2;

class StrategyInstance;

class StrategyServer
{
public:
    void sendOrder(StrategyInstance&, const std::string& symbol);
private:
    int _next_order_id = 0;
};

class StrategyInstance
{
public:
    StrategyInstance(StrategyServer*);
    virtual ~StrategyInstance() = default;
    
    virtual void eval() = 0;
    virtual void onOrder(const std::string& str) = 0;
    
    void sendOrder(const std::string& str);
    
private:
    StrategyServer& _server;
};

void StrategyServer::sendOrder(StrategyInstance& instance, const std::string& symbol)
{
    // simulate sending an order, receiving an acknowledgement and calling back to the strategy instance
    
    std::cout << "sending order to market\n";
    
    instance.onOrder(symbol);
}

///////////////////////////////////

StrategyInstance::StrategyInstance(StrategyServer* server) : _server(*server)
{
}

void StrategyInstance::sendOrder(const std::string& symbol)
{
    _server.sendOrder(*this, symbol);
}

namespace py = pybind11;

class PyStrategyInstance final : public StrategyInstance
{
    using StrategyInstance::StrategyInstance;
    
    void eval() override
    {
        PYBIND11_OVERLOAD_PURE(
                               void,              // return type
                               StrategyInstance,  // super class
                               eval               // function name
                               );
    }
    
    void onOrder(const std::string& order) override
    {
        PYBIND11_OVERLOAD_PURE_NAME(
                                    void,              // return type
                                    StrategyInstance,  // super class
                                    "on_order",        // python function name
                                    onOrder,           // function name
                                    order              // args
                                    );
    }
};

PYBIND11_PLUGIN(StrategyFramework)
{
    py::module m("StrategyFramework", "Example strategy framework");
    
    py::class_<StrategyServer>(m, "StrategyServer");
    
    py::class_<StrategyInstance, PyStrategyInstance>(m, "StrategyInstance")
    .def(py::init<StrategyServer*>())
    .def("send_order", &StrategyInstance::sendOrder);
    
    return m.ptr();
}

py::object import(const std::string& module, const std::string& path, py::object& globals)
{
    py::dict locals;
    locals["module_name"] = py::cast(module);
    locals["path"]        = py::cast(path);
    
    py::eval<py::eval_statements>(
                                  "import imp\n"
                                  "new_module = imp.load_module(module_name, open(path), path, ('py', 'U', imp.PY_SOURCE))\n",
                                  globals,
                                  locals);
    
    return locals["new_module"];
}

struct synthetic_frame
{
    int x, y, bpp;
    std::vector<uint8_t> frame;
};

class custom_frame_source
{
public:
    custom_frame_source()
    {
        depth_frame.x = W;
        depth_frame.y = H;
        depth_frame.bpp = BPP;
        
        last = std::chrono::high_resolution_clock::now();
        
        std::vector<uint8_t> pixels_depth(depth_frame.x * depth_frame.y * depth_frame.bpp, 0);
        depth_frame.frame = std::move(pixels_depth);
        
        auto realsense_logo = stbi_load_from_memory(bla::splash, (int)bla::splash_size, &color_frame.x, &color_frame.y, &color_frame.bpp, false);
        
        std::vector<uint8_t> pixels_color(color_frame.x * color_frame.y * color_frame.bpp, 0);
        
        memcpy(pixels_color.data(), realsense_logo, color_frame.x*color_frame.y * 4);
        
        for (auto i = 0; i< color_frame.y; i++)
            for (auto j = 0; j < color_frame.x * 4; j += 4)
            {
                if (pixels_color.data()[i*color_frame.x * 4 + j] == 0)
                {
                    pixels_color.data()[i*color_frame.x * 4 + j] = 22;
                    pixels_color.data()[i*color_frame.x * 4 + j + 1] = 115;
                    pixels_color.data()[i*color_frame.x * 4 + j + 2] = 185;
                }
            }
        color_frame.frame = std::move(pixels_color);
    }
    
    synthetic_frame& get_synthetic_texture()
    {
        return color_frame;
    }
    
    synthetic_frame& get_synthetic_depth()
    {
        //draw_text(50, 50, "This point-cloud is generated from a synthetic device:");
        
        auto now = std::chrono::high_resolution_clock::now();
        if (now - last > std::chrono::milliseconds(1))
        {
            static float yaw = 0;
            yaw -= 1;
            wave_base += 0.1f;
            last = now;
            
            for (int i = 0; i < depth_frame.y; i++)
            {
                for (int j = 0; j < depth_frame.x; j++)
                {
                    auto d = 2 + 0.1 * (1 + sin(wave_base + j / 50.f));
                    ((uint16_t*)depth_frame.frame.data())[i*depth_frame.x + j] = (int)(d * 0xff);
                }
            }
        }
        return depth_frame;
    }
    
    rs2_intrinsics create_texture_intrinsics()
    {
        rs2_intrinsics intrinsics = { color_frame.x, color_frame.y,
            (float)color_frame.x / 2, (float)color_frame.y / 2,
            (float)color_frame.x / 2, (float)color_frame.y / 2,
            RS2_DISTORTION_BROWN_CONRADY ,{ 0,0,0,0,0 } };
        
        return intrinsics;
    }
    
    rs2_intrinsics create_depth_intrinsics()
    {
        rs2_intrinsics intrinsics = { depth_frame.x, depth_frame.y,
            (float)depth_frame.x / 2, (float)depth_frame.y / 2,
            (float)depth_frame.x , (float)depth_frame.y ,
            RS2_DISTORTION_BROWN_CONRADY ,{ 0,0,0,0,0 } };
        
        return intrinsics;
    }
    
private:
    synthetic_frame depth_frame;
    synthetic_frame color_frame;
    
    std::chrono::high_resolution_clock::time_point last;
    float wave_base = 0.f;
};
    
    
void add_playback_device(context& ctx, std::vector<device_model>& device_models, std::string& error_message, viewer_model& viewer_model, const std::string& file)
{
    bool was_loaded = false;
    bool failed = false;
    try
    {
        auto dev = ctx.load_device(file);
        was_loaded = true;
        device_models.emplace_back(dev, error_message, viewer_model); //Will cause the new device to appear in the left panel
        if (auto p = dev.as<playback>())
        {
            auto filename = p.file_name();
            p.set_status_changed_callback([&viewer_model, &device_models, filename](rs2_playback_status status)
            {
                if (status == RS2_PLAYBACK_STATUS_STOPPED)
                {
                    auto it = std::find_if(device_models.begin(), device_models.end(),
                        [&](const device_model& dm) {
                        if (auto p = dm.dev.as<playback>())
                            return p.file_name() == filename;
                        return false;
                    });
                    if (it != device_models.end())
                    {
                        auto subs = it->subdevices;
                        if (it->_playback_repeat)
                        {
                            //Calling from different since playback callback is from reading thread
                            std::thread{ [subs, &viewer_model]()
                            {
                                for (auto&& sub : subs)
                                {
                                    if (sub->streaming)
                                    {
                                        auto profiles = sub->get_selected_profiles();
                                        sub->play(profiles, viewer_model);
                                    }
                                }
                            } }.detach();
                        }
                        else
                        {
                            for (auto&& sub : subs)
                            {
                                if (sub->streaming)
                                {
                                    sub->stop(viewer_model);
                                }
                            }
                        }
                    }
                }
            });
        }
    }
    catch (const error& e)
    {
        error_message = to_string() << "Failed to load file " << file << ". Reason: " << error_to_string(e);
        failed = true;
    }
    catch (const std::exception& e)
    {
        error_message = to_string() << "Failed to load file " << file << ". Reason: " << e.what();
        failed = true;
    }
    if (failed && was_loaded)
    {
        try { ctx.unload_device(file); }
        catch (...) {}
    }
}

// This function is called every frame
// If between the frames there was an asyncronous connect/disconnect event
// the function will pick up on this and add the device to the viewer
void refresh_devices(std::mutex& m,
    context& ctx,
    device_changes& devices_connection_changes,
    std::vector<device>& current_connected_devices,
    std::vector<std::pair<std::string, std::string>>& device_names,
    std::vector<device_model>& device_models,
    viewer_model& viewer_model,
    std::string& error_message)
{
    event_information info({}, {});
    if (devices_connection_changes.try_get_next_changes(info))
    {
        try
        {
            auto prev_size = current_connected_devices.size();

            //Remove disconnected
            auto dev_itr = begin(current_connected_devices);
            while (dev_itr != end(current_connected_devices))
            {
                auto dev = *dev_itr;
                if (info.was_removed(dev))
                {
                    //Notify change
                    viewer_model.not_model.add_notification({ get_device_name(dev).first + " Disconnected\n",
                        0, RS2_LOG_SEVERITY_INFO, RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });

                    viewer_model.ppf.depth_stream_active = false;

                    //Remove from devices
                    auto dev_model_itr = std::find_if(begin(device_models), end(device_models),
                        [&](const device_model& other) { return get_device_name(other.dev) == get_device_name(dev); });

                    if (dev_model_itr != end(device_models))
                    {
                        for (auto&& s : dev_model_itr->subdevices)
                            s->streaming = false;

                        device_models.erase(dev_model_itr);
                    }
                    auto dev_name_itr = std::find(begin(device_names), end(device_names), get_device_name(dev));
                    if (dev_name_itr != end(device_names))
                        device_names.erase(dev_name_itr);

                    dev_itr = current_connected_devices.erase(dev_itr);
                    continue;
                }
                ++dev_itr;
            }

            //Add connected
            static bool initial_refresh = true;
            for (auto dev : info.get_new_devices())
            {
                auto dev_descriptor = get_device_name(dev);
                device_names.push_back(dev_descriptor);
                if (!initial_refresh)
                    viewer_model.not_model.add_notification({ dev_descriptor.first + " Connected\n",
                        0, RS2_LOG_SEVERITY_INFO, RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });

                current_connected_devices.push_back(dev);
                for (auto&& s : dev.query_sensors())
                {
                    s.set_notifications_callback([&, dev_descriptor](const notification& n)
                    {
                        if (n.get_category() == RS2_NOTIFICATION_CATEGORY_HARDWARE_EVENT)
                        {
                            auto data = n.get_serialized_data();
                            if (!data.empty())
                            {
                                auto dev_model_itr = std::find_if(begin(device_models), end(device_models),
                                    [&](const device_model& other) { return get_device_name(other.dev) == dev_descriptor; });

                                if (dev_model_itr == end(device_models))
                                    return;

                                dev_model_itr->handle_harware_events(data);
                            }
                        }
                        viewer_model.not_model.add_notification({ n.get_description(), n.get_timestamp(), n.get_severity(), n.get_category() });
                    });
                }

                if (device_models.size() == 0 &&
                    dev.supports(RS2_CAMERA_INFO_NAME) && std::string(dev.get_info(RS2_CAMERA_INFO_NAME)) != "Platform Camera")
                {
                    device_models.emplace_back(dev, error_message, viewer_model);
                    viewer_model.not_model.add_log(to_string() << device_models.rbegin()->dev.get_info(RS2_CAMERA_INFO_NAME) << " was selected as a default device");
                }
            }
            initial_refresh = false;
        }
        catch (const error& e)
        {
            error_message = error_to_string(e);
        }
        catch (const std::exception& e)
        {
            error_message = e.what();
        }
        catch (...)
        {
            error_message = "Unknown error";
        }
    }
}

int main(int argv, const char** argc) try
{
    rs2::log_to_console(RS2_LOG_SEVERITY_WARN);
    
    Py_Initialize();
    pybind11_init();
    
    StrategyServer server;
    
    py::object main     = py::module::import("__main__");
    py::object globals  = main.attr("__dict__");
    py::object module   = import("strategy", "strategy.py", globals);
    py::object Strategy = module.attr("Strategy");
    py::object strategy = Strategy(&server);
    
    strategy.attr("eval")();
    

    ux_window window("Intel RealSense Viewer");
    
    custom_frame_source app_data;
    
    auto texture = app_data.get_synthetic_texture();
    
    rs2_intrinsics color_intrinsics = app_data.create_texture_intrinsics();
    rs2_intrinsics depth_intrinsics = app_data.create_depth_intrinsics();
    
    //==================================================//
    //           Declare Software-Only Device           //
    //==================================================//
    
    rs2::software_device dev; // Create software-only device
    
    auto depth_sensor = dev.add_sensor("Depth"); // Define single sensor
    auto color_sensor = dev.add_sensor("Color"); // Define single sensor
    
    auto depth_stream = depth_sensor.add_video_stream({  RS2_STREAM_DEPTH, 0, 0,
        W, H, 60, BPP,
        RS2_FORMAT_Z16, depth_intrinsics });
    
    depth_sensor.add_read_only_option(RS2_OPTION_DEPTH_UNITS, 0.001f);
    
    
    auto color_stream = color_sensor.add_video_stream({  RS2_STREAM_COLOR, 0, 1, texture.x,
        texture.y, 60, texture.bpp,
        RS2_FORMAT_RGBA8, color_intrinsics });
    
    dev.create_matcher(RS2_MATCHER_DLR_C);
    depth_stream.register_extrinsics_to(color_stream, { { 1,0,0,0,1,0,0,0,1 },{ 0,0,0 } });
    
    std::thread t([&](){
        int frame_number = 0;
        while (true) // Application still alive?
        {
            synthetic_frame& depth_frame = app_data.get_synthetic_depth();
            
            depth_sensor.on_video_frame({ depth_frame.frame.data(), // Frame pixels from capture API
                [](void*) {}, // Custom deleter (if required)
                depth_frame.x*depth_frame.bpp, depth_frame.bpp, // Stride and Bytes-per-pixel
                (rs2_time_t)frame_number * 16, RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, frame_number, // Timestamp, Frame# for potential sync services
                depth_stream });
            
            
            color_sensor.on_video_frame({ texture.frame.data(), // Frame pixels from capture API
                [](void*) {}, // Custom deleter (if required)
                texture.x*texture.bpp, texture.bpp, // Stride and Bytes-per-pixel
                (rs2_time_t)frame_number * 16, RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, frame_number, // Timestamp, Frame# for potential sync services
                color_stream });
            
            ++frame_number;
        }
    });
    

    // Create RealSense Context
    context ctx;

    dev.inject_to(ctx);
    
    device_changes devices_connection_changes(ctx);
    std::vector<std::pair<std::string, std::string>> device_names;

    std::string error_message{ "" };
    std::string label{ "" };

    std::vector<device_model> device_models;
    device_model* device_to_remove = nullptr;

    viewer_model viewer_model;
    std::vector<device> connected_devs;
    std::mutex m;

    periodic_timer update_readonly_options_timer(std::chrono::seconds(6));

    window.on_file_drop = [&](std::string filename)
    {
        std::string error_message{};
        add_playback_device(ctx, device_models, error_message, viewer_model, filename);
        if (!error_message.empty())
        {
            viewer_model.not_model.add_notification({ error_message,
                0, RS2_LOG_SEVERITY_ERROR, RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });
        }
    };

    for (int i = 1; i < argv; i++)
    {
        try
        {
            const char* arg = argc[i];
            std::ifstream file(arg);
            if (!file.good())
                continue;

            add_playback_device(ctx, device_models, error_message, viewer_model, arg);
        }
        catch (const rs2::error& e)
        {
            error_message = error_to_string(e);
        }
        catch (const std::exception& e)
        {
            error_message = e.what();
        }
    }

    window.on_load = [&]()
    {
        refresh_devices(m, ctx, devices_connection_changes, connected_devs, device_names, device_models, viewer_model, error_message);
        return true;
    };

    // Closing the window
    while (window)
    {
        refresh_devices(m, ctx, devices_connection_changes, connected_devs, device_names, device_models, viewer_model, error_message);

        bool update_read_only_options = update_readonly_options_timer;

        auto output_height = viewer_model.get_output_height();

        rect viewer_rect = { viewer_model.panel_width,
                             viewer_model.panel_y, window.width() -
                             viewer_model.panel_width,
                             window.height() - viewer_model.panel_y - output_height };

        // Flags for pop-up window - no window resize, move or collaps
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::SetNextWindowPos({ 0, 0 });
        ImGui::SetNextWindowSize({ viewer_model.panel_width, viewer_model.panel_y });

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Add Device Panel", nullptr, flags);

        ImGui::PushFont(window.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_PopupBg, from_rgba(230, 230, 230, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, from_rgba(0, 0xae, 0xff, 255));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, from_rgba(255, 255, 255, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::SetNextWindowPos({ 0, viewer_model.panel_y });

        std::string add_source_button_text = to_string() << " " << textual_icons::plus_circle << "  Add Source\t\t\t\t\t\t\t\t\t\t\t";
        if (ImGui::Button(add_source_button_text.c_str(), { viewer_model.panel_width - 1, viewer_model.panel_y }))
            ImGui::OpenPopup("select");

        auto new_devices_count = device_names.size() + 1;
        for (auto&& dev_model : device_models)
        {
            auto connected_devs_itr = std::find_if(begin(connected_devs), end(connected_devs),
                [&](const device& d) { return get_device_name(d) == get_device_name(dev_model.dev); });

            if (connected_devs_itr != end(connected_devs) || dev_model.dev.is<playback>())
                new_devices_count--;
        }


        ImGui::PushFont(window.get_font());
        ImGui::SetNextWindowSize({ viewer_model.panel_width, 20.f * new_devices_count + 8 });
        if (ImGui::BeginPopup("select"))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, dark_grey);
            ImGui::Columns(2, "DevicesList", false);
            for (size_t i = 0; i < device_names.size(); i++)
            {
                bool skip = false;
                for (auto&& dev_model : device_models)
                    if (get_device_name(dev_model.dev) == device_names[i]) skip = true;
                if (skip) continue;

                if (ImGui::Selectable(device_names[i].first.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)/* || switch_to_newly_loaded_device*/)
                {
                    try
                    {
                        auto dev = connected_devs[i];
                        device_models.emplace_back(dev, error_message, viewer_model);
                    }
                    catch (const error& e)
                    {
                        error_message = error_to_string(e);
                    }
                    catch (const std::exception& e)
                    {
                        error_message = e.what();
                    }
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, from_rgba(255, 255, 255, 255));
                    ImGui::NextColumn();
                    ImGui::Text("S/N: %s", device_names[i].second.c_str());
                    ImGui::NextColumn();
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::NextColumn();
                    ImGui::Text("S/N: %s", device_names[i].second.c_str());
                    ImGui::NextColumn();
                }

            }

            if (new_devices_count > 1) ImGui::Separator();

            if (ImGui::Selectable("Load Recorded Sequence", false, ImGuiSelectableFlags_SpanAllColumns))
            {
                if (auto ret = file_dialog_open(open_file, "ROS-bag\0*.bag\0", NULL, NULL))
                {
                    add_playback_device(ctx, device_models, error_message, viewer_model, ret);
                }
            }
            ImGui::NextColumn();
            ImGui::Text("%s", "");
            ImGui::NextColumn();

            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::End();
        ImGui::PopStyleVar();


        viewer_model.show_top_bar(window, viewer_rect);

        viewer_model.show_event_log(window.get_font(), viewer_model.panel_width,
            window.height() - (viewer_model.is_output_collapsed ? viewer_model.default_log_h : 20),
            window.width() - viewer_model.panel_width, viewer_model.default_log_h);

        // Set window position and size
        ImGui::SetNextWindowPos({ 0, viewer_model.panel_y });
        ImGui::SetNextWindowSize({ viewer_model.panel_width, window.height() - viewer_model.panel_y });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, sensor_bg);

        // *********************
        // Creating window menus
        // *********************
        ImGui::Begin("Control Panel", nullptr, flags | ImGuiWindowFlags_AlwaysVerticalScrollbar);

        if (device_models.size() > 0)
        {
            std::vector<std::function<void()>> draw_later;
            auto windows_width = ImGui::GetContentRegionMax().x;

            for (auto&& dev_model : device_models)
            {
                dev_model.draw_controls(viewer_model.panel_width, viewer_model.panel_y,
                    window,
                    error_message, device_to_remove, viewer_model, windows_width,
                    update_read_only_options,
                    draw_later);
            }

            if (device_to_remove)
            {
                if (auto p = device_to_remove->dev.as<playback>())
                {
                    ctx.unload_device(p.file_name());
                }

                device_models.erase(std::find_if(begin(device_models), end(device_models),
                    [&](const device_model& other) { return get_device_name(other.dev) == get_device_name(device_to_remove->dev); }));
                device_to_remove = nullptr;
            }

            ImGui::SetContentRegionWidth(windows_width);

            auto pos = ImGui::GetCursorScreenPos();
            auto h = ImGui::GetWindowHeight();
            if (h > pos.y - viewer_model.panel_y)
            {
                ImGui::GetWindowDrawList()->AddLine({ pos.x,pos.y }, { pos.x + viewer_model.panel_width,pos.y }, ImColor(from_rgba(0, 0, 0, 0xff)));
                ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + ImGui::GetContentRegionAvail().y));
                ImGui::GetWindowDrawList()->AddRectFilled(bb.GetTL(), bb.GetBR(), ImColor(dark_window_background));
            }

            for (auto&& lambda : draw_later)
            {
                try
                {
                    lambda();
                }
                catch (const error& e)
                {
                    error_message = error_to_string(e);
                }
                catch (const std::exception& e)
                {
                    error_message = e.what();
                }
            }
        }
        else
        {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + ImGui::GetContentRegionAvail().y));
            ImGui::GetWindowDrawList()->AddRectFilled(bb.GetTL(), bb.GetBR(), ImColor(dark_window_background));

            viewer_model.show_no_device_overlay(window.get_large_font(), 50, static_cast<int>(viewer_model.panel_y + 50));
        }

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // Fetch and process frames from queue
        viewer_model.handle_ready_frames(viewer_rect, window, static_cast<int>(device_models.size()), error_message);
    }

    // Stop calculating 3D model
    viewer_model.ppf.stop();

    // Stop all subdevices
    for (auto&& device_model : device_models)
        for (auto&& sub : device_model.subdevices)
        {
            if (sub->streaming)
                sub->stop(viewer_model);
        }

    return EXIT_SUCCESS;
}
catch (const error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
