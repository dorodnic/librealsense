// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>

#include "model-views.h"
#include "os.h"
#include "ux-window.h"

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
#include <regex>

#include <imgui_internal.h>

// We use NOC file helper function for cross-platform file dialogs
#include <noc_file_dialog.h>

using namespace rs2;
using namespace rs400;

#include "../rosbag-inspector/rosbag_content.h"
 
void add_playback_device(context& ctx, std::shared_ptr<std::vector<device_model>> device_models, 
    std::string& error_message, viewer_model& viewer_model, const std::string& file)
{
    bool was_loaded = false;
    bool failed = false;
    try
    {
        auto dev = ctx.load_device(file);
        was_loaded = true;
        device_models->emplace_back(dev, error_message, viewer_model); //Will cause the new device to appear in the left panel
        if (auto p = dev.as<playback>())
        {
            auto filename = p.file_name();
            p.set_status_changed_callback([&viewer_model, device_models, filename](rs2_playback_status status)
            {
                if (status == RS2_PLAYBACK_STATUS_STOPPED)
                {
                    auto it = std::find_if(device_models->begin(), device_models->end(),
                        [&](const device_model& dm) {
                        if (auto p = dm.dev.as<playback>())
                            return p.file_name() == filename;
                        return false;
                    });
                    if (it != device_models->end())
                    {
                        auto subs = it->subdevices;
                        if (it->_playback_repeat)
                        {
                            //Calling from different since playback callback is from reading thread
                            std::thread{ [subs, &viewer_model, it]()
                            {
                                if(!it->dev_syncer)
                                    it->dev_syncer = viewer_model.syncer->create_syncer();

                                for (auto&& sub : subs)
                                {
                                    if (sub->streaming)
                                    {
                                        auto profiles = sub->get_selected_profiles();

                                        sub->play(profiles, viewer_model, it->dev_syncer);
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
    std::shared_ptr<std::vector<device_model>> device_models,
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

                    //Remove from devices
                    auto dev_model_itr = std::find_if(begin(*device_models), end(*device_models),
                        [&](const device_model& other) { return get_device_name(other.dev) == get_device_name(dev); });

                    if (dev_model_itr != end(*device_models))
                    {
                        for (auto&& s : dev_model_itr->subdevices)
                            s->streaming = false;

                        dev_model_itr->reset();
                        device_models->erase(dev_model_itr);

                        if(device_models->size() == 0)
                        {
                            viewer_model.ppf.depth_stream_active = false;

                            // Stopping post processing filter rendering thread in case of disconnection
                            viewer_model.ppf.stop();
                        }
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
                                auto dev_model_itr = std::find_if(begin(*device_models), end(*device_models),
                                    [&](const device_model& other) { return get_device_name(other.dev) == dev_descriptor; });

                                if (dev_model_itr == end(*device_models))
                                    return;

                                dev_model_itr->handle_hardware_events(data);
                            }
                        }
                        viewer_model.not_model.add_notification({ n.get_description(), n.get_timestamp(), n.get_severity(), n.get_category() });
                    });
                }

                if (device_models->size() == 0 &&
                    dev.supports(RS2_CAMERA_INFO_NAME) && std::string(dev.get_info(RS2_CAMERA_INFO_NAME)) != "Platform Camera")
                {
                    device_models->emplace_back(dev, error_message, viewer_model);
                    viewer_model.not_model.add_log(to_string() << device_models->rbegin()->dev.get_info(RS2_CAMERA_INFO_NAME) << " was selected as a default device");
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

class bag_device
{
public:
    bag_device(std::string filename)
        : _bag(filename), _t([this] { loop(); })
    {
        _dev.create_matcher(RS2_MATCHER_DLR_C);

        for (auto&& topic_to_message_type : _bag.topics_to_message_types)
        {
            std::string topic = topic_to_message_type.first;
            std::vector<std::string> messages_types = topic_to_message_type.second;

            if (messages_types.front() == "tf2_msgs/TFMessage")
            {
                // ??
            }

            if (messages_types.front() == "sensor_msgs/Image")
            {
                rosbag::View messages(_bag.bag, rosbag::TopicQuery(topic));
                auto m = *messages.begin();

                rs2_intrinsics intr;
                bool found_intr = false;

                auto intr_topic = std::regex_replace(topic, std::regex("/image"), "/camera_info");
                auto it = _bag.topics_to_message_types.find(intr_topic);
                if (it != _bag.topics_to_message_types.end())
                {
                    if (it->second.front() == "sensor_msgs/CameraInfo")
                    {
                        rosbag::View messages(_bag.bag, rosbag::TopicQuery(intr_topic));
                        auto message = *messages.begin();

                        if (auto intr_msg = rosbag_inspector::try_instantiate<sensor_msgs::CameraInfo>(message))
                        {
                            intr = {
                                (int)intr_msg->width, (int)intr_msg->height,
                                (float)intr_msg->K[2], (float)intr_msg->K[5],
                                (float)intr_msg->K[0], (float)intr_msg->K[4],
                                RS2_DISTORTION_NONE,
                                { 0.f, 0.f, 0.f, 0.f, 0.f }
                            };
                            found_intr = true;
                        }
                    }
                }
                if (!found_intr) continue;

                if (auto data = rosbag_inspector::try_instantiate<sensor_msgs::Image>(m))
                {
                    if (data->encoding == "mono16")
                    {
                        auto depth_sensor = _dev.add_sensor(topic);
                        auto ir_stream = depth_sensor.add_video_stream({ RS2_STREAM_INFRARED, 0, uids++,
                            (int)data->width, (int)data->height, 30, 2,
                            RS2_FORMAT_Y16, intr });

                        _profiles[topic] = ir_stream;
                        _sensors[topic] = depth_sensor;
                    }
                    if (data->encoding == "16UC1")
                    {
                        auto depth_sensor = _dev.add_sensor(topic);
                        auto ir_stream = depth_sensor.add_video_stream({ RS2_STREAM_DEPTH, 0, uids++,
                            (int)data->width, (int)data->height, 30, 2,
                            RS2_FORMAT_Z16, intr });

                        depth_sensor.add_read_only_option(RS2_OPTION_DEPTH_UNITS, 0.001f);

                        _profiles[topic] = ir_stream;
                        _sensors[topic] = depth_sensor;
                    }
                    if (data->encoding == "mono8")
                    {
                        auto depth_sensor = _dev.add_sensor(topic);
                        auto ir_stream = depth_sensor.add_video_stream({ RS2_STREAM_INFRARED, 0, uids++,
                            (int)data->width, (int)data->height, 30, 1,
                            RS2_FORMAT_Y8, intr });

                        _profiles[topic] = ir_stream;
                        _sensors[topic] = depth_sensor;
                    }
                }
            }
        }

        rs2_extrinsics extr{
            { 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f },
            { 0.f, 0.f, 0.f }
        };

        for (auto&& kvp : _profiles)
        {
            kvp.second.register_extrinsics_to(_profiles.begin()->second, extr);
        }

        _alive = true;
    }

    ~bag_device()
    {
        _alive = false;
        _t.join();
    }

    void add_to(context& ctx)
    {
        _dev.add_to(ctx);
    }

    device get() { return _dev; }

    void pause() { _paused = !_paused; }
    void next() { _frame_number++; }
    void prev() { _frame_number--; }

private:
    void loop()
    {
        while (!_alive);

        while (_alive) // Application still alive?
        {
            for (auto&& kvp : _profiles)
            {
                rosbag::View messages(_bag.bag, rosbag::TopicQuery(kvp.first));
                int count = 0;
                for (auto&& m : messages)
                {
                    count++;
                    if (count < _frame_number % messages.size()) continue;

                    if (auto data = rosbag_inspector::try_instantiate<sensor_msgs::Image>(m))
                    {
                        auto ptr = new uint8_t[data->data.size()];
                        memcpy(ptr, data->data.data(), data->data.size());

                        auto depth_sensor = _sensors[kvp.first];
                        depth_sensor.on_video_frame({ (void*)ptr,
                            [](void* p) { delete[] p; },
                            (int)data->step, (int)data->step / (int)data->width,
                            (rs2_time_t)_frame_number * 16, RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, _frame_number,
                            kvp.second });
                    }

                    if (count > 1) break;
                }
            }
            if (!_paused) ++_frame_number;
        }
    }

    std::atomic_bool _alive = false;
    std::thread _t;
    rs2::software_device _dev;

    static int uids;

    bool _paused = false;
    int _frame_number = 0;

    std::map<std::string, software_sensor> _sensors;
    std::map<std::string, stream_profile> _profiles;
    std::map<std::string, rosbag::View> _views;
    rosbag_inspector::rosbag_content _bag;
};

int bag_device::uids = 1000;

void add_bag_device(context& ctx, std::shared_ptr<std::vector<device_model>> device_models,
    std::string& error_message, viewer_model& viewer_model, const std::string& file, std::vector<std::shared_ptr<bag_device>>& bag_devices)
{
    bool was_loaded = false;
    bool failed = false;
    try
    {
        auto dev = std::make_shared<bag_device>(file);
        bag_devices.push_back(dev);
        dev->add_to(ctx);
        was_loaded = true;
        device_models->emplace_back(dev->get(), error_message, viewer_model); 
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

int main(int argv, const char** argc) try
{
    rs2::log_to_console(RS2_LOG_SEVERITY_WARN);

    ux_window window("Intel RealSense Viewer");

    std::vector<std::shared_ptr<bag_device>> bag_devices;
    
    // Create RealSense Context
    context ctx;
    
    device_changes devices_connection_changes(ctx);
    std::vector<std::pair<std::string, std::string>> device_names;

    std::string error_message{ "" };
    std::string label{ "" };

    std::shared_ptr<std::vector<device_model>> device_models = std::make_shared<std::vector<device_model>>();
    device_model* device_to_remove = nullptr;

    viewer_model viewer_model;

    std::vector<device> connected_devs;
    std::mutex m;

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
        refresh_devices(m, ctx, devices_connection_changes, connected_devs, 
            device_names, device_models, viewer_model, error_message);
        return true;
    };

    // Closing the window
    while (window)
    {
        if (!window.is_ui_aligned())
        {
            viewer_model.popup_if_ui_not_aligned(window.get_font());
        }
        refresh_devices(m, ctx, devices_connection_changes, connected_devs, 
            device_names, device_models, viewer_model, error_message);

        auto output_height = viewer_model.get_output_height();

        rect viewer_rect = { viewer_model.panel_width,
                             viewer_model.panel_y, window.width() -
                             viewer_model.panel_width,
                             window.height() - viewer_model.panel_y - output_height };

        // Flags for pop-up window - no window resize, move or collaps
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

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
        for (auto&& dev_model : *device_models)
        {
            auto connected_devs_itr = std::find_if(begin(connected_devs), end(connected_devs),
                [&](const device& d) { return get_device_name(d) == get_device_name(dev_model.dev); });

            if (connected_devs_itr != end(connected_devs) || dev_model.dev.is<playback>())
                new_devices_count--;
        }


        ImGui::PushFont(window.get_font());
        ImGui::SetNextWindowSize({ viewer_model.panel_width, 20.f * new_devices_count + 28 });
        if (ImGui::BeginPopup("select"))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, dark_grey);
            ImGui::Columns(2, "DevicesList", false);
            for (size_t i = 0; i < device_names.size(); i++)
            {
                bool skip = false;
                for (auto&& dev_model : *device_models)
                    if (get_device_name(dev_model.dev) == device_names[i]) skip = true;
                if (skip) continue;

                if (ImGui::Selectable(device_names[i].first.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)/* || switch_to_newly_loaded_device*/)
                {
                    try
                    {
                        auto dev = connected_devs[i];
                        device_models->emplace_back(dev, error_message, viewer_model);
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

            if (ImGui::Selectable("Load Generic ROS-bag", false, ImGuiSelectableFlags_SpanAllColumns))
            {
                if (auto ret = file_dialog_open(open_file, "ROS-bag\0*.bag\0", NULL, NULL))
                {
                    add_bag_device(ctx, device_models, error_message, viewer_model, ret, bag_devices);
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


        viewer_model.show_top_bar(window, viewer_rect, *device_models);

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

        if (bag_devices.size())
        {
            ImGui::Text("Bag Control:");
            ImGui::SameLine();
            if (ImGui::Button(textual_icons::step_backward, { 16, 16 }))
            {
                bag_devices[0]->prev();
            }
            ImGui::SameLine();
            if (ImGui::Button(textual_icons::pause, { 16, 16 }))
            {
                bag_devices[0]->pause();
            }
            ImGui::SameLine();
            if (ImGui::Button(textual_icons::step_forward, { 16, 16 }))
            {
                bag_devices[0]->next();
            }
        }

        if (device_models->size() > 0)
        {
            std::vector<std::function<void()>> draw_later;
            auto windows_width = ImGui::GetContentRegionMax().x;

            for (auto&& dev_model : *device_models)
            {
                dev_model.draw_controls(viewer_model.panel_width, viewer_model.panel_y,
                    window, error_message, device_to_remove, viewer_model, windows_width, draw_later);
            }
            if (viewer_model.ppf.is_rendering())
            {
                if (!std::any_of(device_models->begin(), device_models->end(),
                    [](device_model& dm)
                {
                    return dm.is_streaming();
                }))
                {
                    // Stopping post processing filter rendering thread
                    viewer_model.ppf.stop();
                }
            }

            if (device_to_remove)
            {
                if (auto p = device_to_remove->dev.as<playback>())
                {
                    ctx.unload_device(p.file_name());
                }
                viewer_model.syncer->remove_syncer(device_to_remove->dev_syncer);
                device_models->erase(std::find_if(begin(*device_models), end(*device_models),
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
        viewer_model.handle_ready_frames(viewer_rect, window, static_cast<int>(device_models->size()), error_message);
    }

    // Stopping post processing filter rendering thread
    viewer_model.ppf.stop();

    // Stop all subdevices
    for (auto&& device_model : *device_models)
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
