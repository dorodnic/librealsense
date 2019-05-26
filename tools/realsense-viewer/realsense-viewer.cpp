// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp>
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

#include <imgui_internal.h>

// We use NOC file helper function for cross-platform file dialogs
#include <noc_file_dialog.h>

#ifdef INTERNAL_FW
#include "common/fw/D4XX_FW_Image.h"
#include "common/fw/SR3XX_FW_Image.h"
#else
#define FW_D4XX_FW_IMAGE_VERSION ""
#define FW_SR3XX_FW_IMAGE_VERSION ""
#endif // INTERNAL_FW

using namespace rs2;
using namespace rs400;

static std::map<int, std::string> product_line_to_fw = 
{
    {RS2_PRODUCT_LINE_D400, FW_D4XX_FW_IMAGE_VERSION},
    {RS2_PRODUCT_LINE_D400_RECOVERY, FW_D4XX_FW_IMAGE_VERSION},
    {RS2_PRODUCT_LINE_SR300, FW_SR3XX_FW_IMAGE_VERSION},
    {RS2_PRODUCT_LINE_SR300_RECOVERY, FW_SR3XX_FW_IMAGE_VERSION}
};

static std::map<int, std::vector<uint8_t>> create_default_fw_table()
{
    std::map<int, std::vector<uint8_t>> rv;

    if ("" != FW_D4XX_FW_IMAGE_VERSION)
    {
        int size = 0;
        auto hex = fw_get_D4XX_FW_Image(size);
        auto vec = std::vector<uint8_t>(hex, hex + size);
        rv[RS2_PRODUCT_LINE_D400] = vec;
        rv[RS2_PRODUCT_LINE_D400_RECOVERY] = vec;
    }

    if ("" != FW_SR3XX_FW_IMAGE_VERSION)
    {
        int size = 0;
        auto hex = fw_get_SR3XX_FW_Image(size);
        auto vec = std::vector<uint8_t>(hex, hex + size);
        rv[RS2_PRODUCT_LINE_SR300] = vec;
        rv[RS2_PRODUCT_LINE_SR300_RECOVERY] = vec;
    }

    return rv;
}

static std::vector<int> parse_fw_version(const std::string& fw)
{
    std::vector<int> rv;
    size_t pos = 0;
    std::string delimiter = ".";
    auto str = fw + delimiter;
    while ((pos = str.find(delimiter)) != std::string::npos) {
        auto s = str.substr(0, pos);
        int val = std::stoi(s);
        rv.push_back(val);
        str.erase(0, pos + delimiter.length());
    }
    return rv;
}

static bool is_upgradeable(const std::string& curr, const std::string& available)
{
    size_t fw_string_size = 4;
    auto c = parse_fw_version(curr);
    auto a = parse_fw_version(available);
    if (a.size() != fw_string_size || c.size() != fw_string_size)
        return false;

    for (int i = 0; i < fw_string_size; i++) {
        if (c[i] > a[i])
            return false;
        if (c[i] < a[i])
            return true;
    }
    return false; //equle
}

static void add_device(rs2::context ctx, int product_line, std::map<std::string, fw_update_device_info>& device_map)
{
    static auto default_fw_table = create_default_fw_table();
    auto recommended_fw_version = product_line_to_fw.at(product_line);
    auto fw = default_fw_table.at(product_line);

    auto devices = ctx.query_devices(product_line);
    if (product_line & RS2_PRODUCT_LINE_RECOVERY)
    {
        for (auto&& d : devices)
        {
            fw_update_device_info fudi = { d, product_line, true, "", "", recommended_fw_version, "", fw};
            device_map["recovery"] = fudi;
        }
    }
    else {
        for (auto&& d : devices)
        {
            if (!d.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                continue;
            if (!d.supports(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION))
                continue;
            if (!d.supports(RS2_CAMERA_INFO_FIRMWARE_VERSION))
                continue;
            auto serial = d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
            auto fw_version = d.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION);
            auto minimal_fw_version = d.get_info(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION);
            bool upgradeable = is_upgradeable(fw_version, recommended_fw_version);
            fw_update_device_info fudi = { d, product_line, upgradeable, serial, fw_version, recommended_fw_version, minimal_fw_version, fw};
            device_map[serial] = fudi;
        }
    }
}

static std::map<std::string, fw_update_device_info> create_upgradeable_device_table(rs2::context ctx)
{
    std::map<std::string, fw_update_device_info> rv;

    add_device(ctx, RS2_PRODUCT_LINE_D400, rv);
    add_device(ctx, RS2_PRODUCT_LINE_SR300, rv);
    add_device(ctx, RS2_PRODUCT_LINE_D400_RECOVERY, rv);
    add_device(ctx, RS2_PRODUCT_LINE_SR300_RECOVERY, rv);

    return rv;
}

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
bool refresh_devices(std::mutex& m,
    context& ctx,
    device_changes& devices_connection_changes,
    std::vector<device>& current_connected_devices,
    std::vector<std::pair<std::string, std::string>>& device_names,
    std::shared_ptr<std::vector<device_model>> device_models,
    viewer_model& viewer_model,
    std::string& error_message)
{
    event_information info({}, {});
    if (!devices_connection_changes.try_get_next_changes(info))
        return false;
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

                    if (device_models->size() == 0)
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
    return true;
}

int main(int argv, const char** argc) try
{
    rs2::log_to_console(RS2_LOG_SEVERITY_WARN);

    ux_window window("Intel RealSense Viewer");

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
    std::vector<device> fw_update_available;
    std::map<std::string, fw_update_device_info> upgradeable_devices;
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

        upgradeable_devices = create_upgradeable_device_table(ctx);
        return true;
    };

    class fw_update_helper
    {
    public:
        void set_update_request(std::vector<uint8_t> image)
        {
            _fw_image = image;
        }

        bool has_update_request() { return !_fw_image.empty(); };
    private:
        std::vector<uint8_t> _fw_image;
        std::thread update_thread;

        void recover_devices(rs2::context ctx, std::vector<uint8_t> fw_image, const ux_window& window, viewer_model& vm)
        {
            auto fwu_devs = ctx.query_devices(RS2_PRODUCT_LINE_RECOVERY);
            for (auto&& d : fwu_devs)
            {
                auto fwu_dev = d.as<rs2::fw_update_device>();

                if (!fwu_dev)// || !fwu_dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                    continue;
                auto sn = fwu_dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                fwu_dev.update_fw(fw_image, [&](const float progress)
                {
                    vm.popup_firmware_update_progress(window, progress);
                    vm.not_model.add_log(to_string() << "firmware update progress: " << (int)(progress * 100.0) << "[%]\n");
                });
            }
        }
    };

    // Closing the window
    while (window)
    {
        auto device_changed = refresh_devices(m, ctx, devices_connection_changes, connected_devs, 
            device_names, device_models, viewer_model, error_message);

        if (device_changed)
        {
            upgradeable_devices = create_upgradeable_device_table(ctx);

            if(!fw_image.empty())
                recover_devices(ctx, fw_image, window, viewer_model);
            fw_image.clear();
        }

        if (!window.is_ui_aligned())
        {
            viewer_model.popup_if_ui_not_aligned(window);
        }

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
        ImGui::SetNextWindowSize({ viewer_model.panel_width, 20.f * new_devices_count + 8 });
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

        for (auto&& dev_model : *device_models)
        {
            // check for fw update recomendation for any of the connected devices
            for (auto&& d : upgradeable_devices)
            {
                if (!d.second.upgrade_recommended)
                    continue;
                if (!dev_model.dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                    continue;
                auto sn = dev_model.dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                bool update = false;
                if (upgradeable_devices.find(sn) != upgradeable_devices.end())
                    viewer_model.popup_if_fw_update_required(window, d.second, update);
                if (update)
                {
                    fw_image = d.second.fw_image;
                }
            }

            // check if there was a user request for FW update on a standard device
            if (dev_model.fw_update_requested)
            {
                std::vector<uint8_t> fw;
                bool canceled = false;
                fw_update_device_info fudi = {};

                if (!dev_model.dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                    continue;

                auto sn = dev_model.dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                auto sn_itr = upgradeable_devices.find(sn);
                if (sn_itr != upgradeable_devices.end())
                    fudi = (*sn_itr).second;

                viewer_model.popup_fw_file_select(window, fudi, fw, canceled);
                if (canceled || fw.size() > 0)
                    dev_model.fw_update_requested = false;
                if (fw.size() > 0)
                    fw_image = fw; //this is the way to signal that there is a panding fw update request.
            }

            // check if there was a user request for FW update on a device in recovery mode
            if (dev_model.fw_update_requested)
            {
                std::vector<uint8_t> fw;
                bool canceled = false;
                fw_update_device_info fudi = {};

                if (!dev_model.dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                    continue;

                auto sn = dev_model.dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                auto sn_itr = upgradeable_devices.find(sn);
                auto rec_itr = upgradeable_devices.find("recovery");

                if (sn_itr != upgradeable_devices.end())
                    fudi = (*sn_itr).second;
                if (rec_itr != upgradeable_devices.end())
                    fudi = (*rec_itr).second;

                if (!fudi.dev)
                    continue;

                viewer_model.popup_fw_file_select(window, fudi, fw, canceled);
                if (canceled || fw.size() > 0)
                    dev_model.fw_update_requested = false;
                if (fw.size() > 0)
                    fw_image = fw;
            }
        }

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
