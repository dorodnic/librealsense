// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <regex>

#include "viewer.h"
#include "os.h"

#include "udev-rules.h"

#include <opengl3.h>

#include <imgui_internal.h>
#include <librealsense2/rsutil.h>

#define ARCBALL_CAMERA_IMPLEMENTATION
#include <arcball_camera.h>

#include <ImGuizmo/ImGuizmo.h>

#include "tiny-profiler.h"

namespace rs2
{
    void viewer_model::render_pose(rs2::rect stream_rect, float buttons_heights)
    {
        int num_of_pose_buttons = 2; // trajectory, info

        // Draw selection buttons on the pose header, the buttons are global to all the streaming devices
        ImGui::SetCursorPos({ stream_rect.w - 32 * num_of_pose_buttons - 5, buttons_heights });

        bool color_icon = pose_info_object_button.is_pressed(); //draw trajectory is on - color the icon
        if (color_icon)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
        }

        // Draw info object button (is not synchronized with the info buttons in the 2D view)
        if (ImGui::Button(pose_info_object_button.get_icon().c_str(), { 24, buttons_heights }))
        {
            show_pose_info_3d = !show_pose_info_3d;
            pose_info_object_button.toggle_button();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", pose_info_object_button.get_tooltip().c_str());
        }
        if (color_icon)
        {
            ImGui::PopStyleColor(2);
        }

        // Draw grid object button
        ImGui::SameLine();

        color_icon = trajectory_button.is_pressed(); //draw trajectory is on - color the icon
        if (color_icon)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
        }
        if (ImGui::Button(trajectory_button.get_icon().c_str(), { 24, buttons_heights }))
        {
            trajectory_button.toggle_button();
            for (auto&& s : streams)
            {
                if (s.second.profile.stream_type() == RS2_STREAM_POSE)
                    streams[s.second.profile.unique_id()].dev->tm2.record_trajectory(trajectory_button.is_pressed());
            }
        }
        if (color_icon)
        {
            ImGui::PopStyleColor(2);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", trajectory_button.get_tooltip().c_str());
        }

        //ImGui::End();
    }

    // void viewer_model::show_3dviewer_header(ImFont* font, rs2::rect stream_rect, bool& paused, std::string& error_message)
    // {
    //     int combo_boxes = 0;
    //     const float combo_box_width = 200;

    //     // Draw pose header if pose stream exists
    //     bool pose_render = false;

    //     for (auto&& s : streams)
    //     {
    //         if (s.second.is_stream_visible() &&
    //             s.second.profile.stream_type() == RS2_STREAM_POSE)
    //         {
    //             pose_render = true;
    //             break;
    //         }
    //     }

    //     // Initialize and prepare depth and texture sources
    //     int selected_depth_source = -1;
    //     std::vector<std::string> depth_sources_str;
    //     std::vector<int> depth_sources;
    //     int i = 0;
    //     for (auto&& s : streams)
    //     {
    //         if (s.second.is_stream_visible() &&
    //             s.second.profile.stream_type() == RS2_STREAM_DEPTH)
    //         {
    //             if (selected_depth_source_uid == -1)
    //             {
    //                 if (streams_origin.find(s.second.profile.unique_id()) != streams_origin.end() &&
    //                     streams.find(streams_origin[s.second.profile.unique_id()]) != streams.end())
    //                 {
    //                     selected_depth_source_uid = streams_origin[s.second.profile.unique_id()];
    //                 }   
    //             }
    //             if (streams_origin.find(s.second.profile.unique_id()) != streams_origin.end() && streams_origin[s.second.profile.unique_id()] == selected_depth_source_uid)
    //             {
    //                 selected_depth_source = i;
    //             }

    //             depth_sources.push_back(s.second.profile.unique_id());

    //             auto dev_name = s.second.dev ? s.second.dev->dev.get_info(RS2_CAMERA_INFO_NAME) : "Unknown";
    //             auto stream_name = rs2_stream_to_string(s.second.profile.stream_type());

    //             depth_sources_str.push_back(to_string() << dev_name << " " << stream_name);

    //             i++;
    //         }
    //     }
    //     if (depth_sources_str.size() > 0 && allow_3d_source_change) combo_boxes++;

    //     auto top_bar_height = 32.f;
    //     const auto buttons_heights = top_bar_height;
    //     const auto num_of_buttons = 5;

    //     if (num_of_buttons * 40 + combo_boxes * (combo_box_width + 100) > stream_rect.w || pose_render)
    //         top_bar_height = 2 * top_bar_height;

    //     ImGui::PushFont(font);
    //     ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
    //     ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);

    //     ImGui::PushStyleColor(ImGuiCol_Button, header_window_bg);
    //     ImGui::PushStyleColor(ImGuiCol_ButtonHovered, header_window_bg);
    //     ImGui::PushStyleColor(ImGuiCol_ButtonActive, header_window_bg);

    //     std::string label = to_string() << "header of 3dviewer";

    //     ImGui::GetWindowDrawList()->AddRectFilled({ stream_rect.x, stream_rect.y },
    //                 { stream_rect.x + stream_rect.w, stream_rect.y + top_bar_height }, ImColor(sensor_bg));

    //     if (depth_sources_str.size() > 0 && allow_3d_source_change)
    //     {
    //         ImGui::SetCursorPos({ 7, 7 });
    //         ImGui::Text("Depth Source:"); ImGui::SameLine();

    //         ImGui::SetCursorPosY(7);
    //         ImGui::PushItemWidth(combo_box_width);
    //         draw_combo_box("##Depth Source", depth_sources_str, selected_depth_source);
    //         i = 0;
    //         for (auto&& s : streams)
    //         {
    //             if (s.second.is_stream_visible() &&
    //                 s.second.texture->get_last_frame() &&
    //                 s.second.profile.stream_type() == RS2_STREAM_DEPTH)
    //             {
    //                 if (i == selected_depth_source)
    //                 {
    //                     selected_depth_source_uid =  streams_origin[s.second.profile.unique_id()];
    //                 }
    //                 i++;
    //             }
    //         }

    //         ImGui::PopItemWidth();

    //         if (buttons_heights == top_bar_height) ImGui::SameLine();
    //     }

    //     if (!allow_3d_source_change) ImGui::SetCursorPos({ 7, 7 });

    //     ImGui::SetCursorPos({ stream_rect.w - 32 * num_of_buttons - 5, 0 });

    //     if (paused)
    //     {
    //         ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
    //         ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
    //         label = to_string() << textual_icons::play << "##Resume 3d";
    //         if (ImGui::Button(label.c_str(), { 24, buttons_heights }))
    //         {
    //             paused = false;
    //             for (auto&& s : streams)
    //             {
    //                 if (s.second.dev) s.second.dev->resume();
    //             }
    //         }
    //         if (ImGui::IsItemHovered())
    //         {
    //             ImGui::SetTooltip("Resume All");
    //         }
    //         ImGui::PopStyleColor(2);
    //     }
    //     else
    //     {
    //         label = to_string() << textual_icons::pause << "##Pause 3d";
    //         if (ImGui::Button(label.c_str(), { 24, buttons_heights }))
    //         {
    //             paused = true;
    //             for (auto&& s : streams)
    //             {
    //                 if (s.second.dev) s.second.dev->pause();
    //             }
    //         }
    //         if (ImGui::IsItemHovered())
    //         {
    //             ImGui::SetTooltip("Pause All");
    //         }
    //     }

    //     ImGui::SameLine();

    //     if (ImGui::Button(textual_icons::floppy, { 24, buttons_heights }))
    //     {
    //         if (auto ret = file_dialog_open(save_file, "Polygon File Format (PLY)\0*.ply\0", NULL, NULL))
    //         {
    //             auto model = ppf.get_points();
                
    //             frame tex;
    //             if (selected_tex_source_uid >= 0 && streams.find(selected_tex_source_uid) != streams.end())
    //             {
    //                 tex = streams[selected_tex_source_uid].texture->get_last_frame(true);
    //                 if (tex) ppf.update_texture(tex);
    //             }

    //             std::string fname(ret);
    //             if (!ends_with(to_lower(fname), ".ply")) fname += ".ply";
    //             export_to_ply(fname.c_str(), not_model, last_points, last_texture->get_last_frame());
    //         }
    //     }
    //     if (ImGui::IsItemHovered())
    //         ImGui::SetTooltip("Export 3D model to PLY format");

    //     ImGui::SameLine();

    //     if (ImGui::Button(textual_icons::refresh, { 24, buttons_heights }))
    //     {
    //         reset_camera();
    //     }
    //     if (ImGui::IsItemHovered())
    //         ImGui::SetTooltip("Reset View");

    //     ImGui::SameLine();

    //     if (render_quads)
    //     {
    //         ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
    //         ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
    //         label = to_string() << textual_icons::cubes << "##Render Quads";
    //         if (ImGui::Button(label.c_str(), { 24, buttons_heights }))
    //         {
    //             render_quads = false;
    //         }
    //         if (ImGui::IsItemHovered())
    //         {
    //             ImGui::SetTooltip("Render Quads");
    //         }
    //         ImGui::PopStyleColor(2);
    //     }
    //     else
    //     {
    //         label = to_string() << textual_icons::cubes << "##Render Points";
    //         if (ImGui::Button(label.c_str(), { 24, buttons_heights }))
    //         {
    //             render_quads = true;
    //         }
    //         if (ImGui::IsItemHovered())
    //         {
    //             ImGui::SetTooltip("Render Points");
    //         }
    //     }
    //     ImGui::SameLine();

    //     if (support_non_syncronized_mode)
    //     {
    //         if (synchronization_enable)
    //         {
    //             ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
    //             ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
    //             if (ImGui::Button(textual_icons::lock, { 24, buttons_heights }))
    //             {
    //                 synchronization_enable = false;
    //             }
    //             if (ImGui::IsItemHovered())
    //                 ImGui::SetTooltip("Disable syncronization between the pointcloud and the texture");
    //             ImGui::PopStyleColor(2);
    //         }
    //         else
    //         {
    //             if (ImGui::Button(textual_icons::unlock, { 24, buttons_heights }))
    //             {
    //                 synchronization_enable = true;
    //             }
    //             if (ImGui::IsItemHovered())
    //                 ImGui::SetTooltip("Keep the pointcloud and the texture sycronized");
    //         }
    //     }


    //     //ImGui::End();

    //     if (pose_render)
    //     {
    //         render_pose(stream_rect, buttons_heights);
    //     }

    //     auto total_top_bar_height = top_bar_height * (1 + pose_render); // may include single bar or additional bar for pose

    //     ImGui::PopStyleColor(5);

    //     ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
    //     ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);

    //     ImGui::PushStyleColor(ImGuiCol_Button, header_window_bg);
    //     ImGui::PushStyleColor(ImGuiCol_ButtonHovered, header_window_bg);
    //     ImGui::PushStyleColor(ImGuiCol_ButtonActive, header_window_bg);

    //     auto old_pos = ImGui::GetCursorScreenPos();
    //     ImVec2 pos { stream_rect.x + stream_rect.w - 215, stream_rect.y + total_top_bar_height + 5 };
    //     ImGui::SetCursorScreenPos(pos);
    //     ImGui::GetWindowDrawList()->AddRectFilled(pos, { pos.x + 260, pos.y + 65 }, ImColor(dark_sensor_bg));

    //     if (_pc_selected)
    //     {
    //         ImGui::SetCursorScreenPos({ pos.x + 5, pos.y + 2 });
    //         ImGui::Text("Move:");
    //         ImGui::SetCursorScreenPos({ pos.x + 5, pos.y + 22 });
    //         ImGui::Text("Rotate:");
    //         ImGui::SetCursorScreenPos({ pos.x + 5, pos.y + 42 });
    //         ImGui::Text("Deselect:");

    //         ImGui::SetCursorScreenPos({ pos.x + 65, pos.y + 2 });
    //         ImGui::Text("Left Mouse");
    //         ImGui::SetCursorScreenPos({ pos.x + 65, pos.y + 22 });
    //         ImGui::Text("Ctrl + (Alt) + Mouse");
    //         ImGui::SetCursorScreenPos({ pos.x + 65, pos.y + 42 });
    //         ImGui::Text("Right Click");
    //     }
    //     else
    //     {
    //         ImGui::SetCursorScreenPos({ pos.x + 5, pos.y + 2 });
    //         ImGui::Text("Rotate:");
    //         ImGui::SetCursorScreenPos({ pos.x + 5, pos.y + 22 });
    //         ImGui::Text("Pan:");
    //         ImGui::SetCursorScreenPos({ pos.x + 5, pos.y + 42 });
    //         ImGui::Text("Zoom:");

    //         ImGui::SetCursorScreenPos({ pos.x + 65, pos.y + 2 });
    //         ImGui::Text("Left Mouse Button");
    //         ImGui::SetCursorScreenPos({ pos.x + 65, pos.y + 22 });
    //         ImGui::Text("Middle Mouse Button");
    //         ImGui::SetCursorScreenPos({ pos.x + 65, pos.y + 42 });
    //         ImGui::Text("Mouse Wheel");
    //     }
        

    //     ImGui::SetCursorScreenPos(old_pos);
    //     ImGui::PopStyleColor(5);


    //     ImGui::PopFont();
    // }

    void viewer_model::check_permissions()
    {
#ifdef __linux__

        if (directory_exists("/etc/udev/rules.d") || directory_exists("/lib/udev/rules.d/"))
        {
            const std::string udev_rules_man("/etc/udev/rules.d/99-realsense-libusb.rules");
            const std::string udev_rules_deb("/lib/udev/rules.d/60-librealsense2-udev-rules.rules");
            std::ifstream f_man(udev_rules_man);
            std::ifstream f_deb(udev_rules_deb);

            std::string message = "UDEV-Rules permissions configuration \n for RealSense devices.`\n"
                        "Missing/outdated UDEV-Rules will cause 'Permissions Denied' errors\nunless the application is running under 'sudo' (not recommended)\n"
                        "In case of Debians use: \n"
                        "sudo apt-get upgrade/install librealsense2-udev-rules\n"
                        "To manually install UDEV-Rules in terminal run:\n"
                        "$ sudo cp ~/.99-realsense-libusb.rules /etc/udev/rules.d/99-realsense-libusb.rules && sudo udevadm control --reload-rules && udevadm trigger\n";

            bool create_file = false;

            if(!(f_man.good() || f_deb.good()))
            {
                message = "RealSense UDEV-Rules are missing!\n" + message;
                not_model.add_notification({ message,
                     RS2_LOG_SEVERITY_WARN,
                     RS2_NOTIFICATION_CATEGORY_COUNT });
                create_file = true;
            }
            else
            {
                std::ifstream f;
                std::string udev_fname;
                if(f_man.good())
                {
                    if (f_deb.good())
                    {
                        std::string duplicates = "Multiple realsense udev-rules were found! :\n1:" + udev_rules_man
                                    + "\n2: " + udev_rules_deb+ "\nMake sure to remove redundancies!";
                        not_model.add_notification({ duplicates,
                             RS2_LOG_SEVERITY_WARN,
                             RS2_NOTIFICATION_CATEGORY_COUNT });
                    }
                    f.swap(f_man);
                    udev_fname = udev_rules_man;
                    create_file = true;
                }
                else
                {
                    f.swap(f_deb);
                    udev_fname = udev_rules_deb;
                }

                std::string str((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());

                std::string udev = realsense_udev_rules;
                udev.erase(udev.find_last_of("\n") + 1);

                if (udev != str)
                {
                    message = "RealSense UDEV-Rules file:\n " + udev_fname +"\n is not up-to date!\n" + message;
                    not_model.add_notification({ 
                        message,
                        RS2_LOG_SEVERITY_WARN,
                        RS2_NOTIFICATION_CATEGORY_COUNT });
                }
            }

            if (create_file)
            {
                std::string tmp_filename = to_string() << get_folder_path(special_folder::app_data) << "/.99-realsense-libusb.rules";

                std::ofstream out(tmp_filename.c_str());
                out << realsense_udev_rules;
                out.close();
            }
        }

#endif
    }

    void viewer_model::update_configuration()
    {
        is_3d_view = config_file::instance().get_or_default(
            configurations::viewer::is_3d_view, false);

        metric_system = config_file::instance().get_or_default(
            configurations::viewer::metric_system, true);

        auto min_severity = (rs2_log_severity)config_file::instance().get_or_default(
            configurations::viewer::log_severity, 2);

        if (config_file::instance().get_or_default(
            configurations::viewer::log_to_console, false))
        {
            rs2::log_to_console(min_severity);
        }
        if (config_file::instance().get_or_default(
            configurations::viewer::log_to_file, false))
        {
            std::string filename = config_file::instance().get(
                configurations::viewer::log_filename);

            rs2::log_to_file(min_severity, filename.c_str());
        }
    }

    viewer_model::viewer_model()
            : ppf(*this), 
              synchronization_enable(true),
              zo_sensors(0)
    {
        syncer = std::make_shared<syncer_model>();
        reset_camera();
        rs2_error* e = nullptr;
        not_model.add_log(to_string() << "librealsense version: " << api_version_to_string(rs2_get_api_version(&e)) << "\n");
    
        update_configuration();

        check_permissions();
    }

    void viewer_model::gc_streams()
    {
        std::lock_guard<std::mutex> lock(streams_mutex);
        std::vector<int> streams_to_remove;
        for (auto&& kvp : streams)
        {
            if (!kvp.second.is_stream_visible() &&
                (!kvp.second.dev || (!kvp.second.dev->is_paused() && !kvp.second.dev->streaming)))
            {
                // if (kvp.first == selected_depth_source_uid)
                // {
                //     ppf.depth_stream_active = false;
                // }
                streams_to_remove.push_back(kvp.first);
            }
        }
        for (auto&& i : streams_to_remove) {
            // if(selected_depth_source_uid == i)
            //     selected_depth_source_uid = -1;
            // if(selected_tex_source_uid == i)
            //     selected_tex_source_uid = -1;
            streams.erase(i);

            if(ppf.frames_queue.find(i) != ppf.frames_queue.end())
            {
                ppf.frames_queue.erase(i);
            }
        }
    }

    void viewer_model::show_event_log(ImFont* font_14, float x, float y, float w, float h)
    {
        ImGui::PushFont(font_14);
        ImGui::SetNextWindowPos({ x, y });
        ImGui::SetNextWindowSize({ w, h });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::SetNextWindowCollapsed(true, ImGuiSetCond_Appearing);

        is_output_collapsed = ImGui::Begin("Output", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_ShowBorders);

        int i = 0;
        not_model.foreach_log([&](const std::string& line) {
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_blue);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);

            auto rc = ImGui::GetCursorPos();
            ImGui::SetCursorPos({ rc.x + 10, rc.y + 4 });

            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::Icon(textual_icons::minus); ImGui::SameLine();
            ImGui::PopStyleColor();

            rc = ImGui::GetCursorPos();
            ImGui::SetCursorPos({ rc.x, rc.y - 4 });

            std::string label = to_string() << "##log_entry" << i++;
            ImGui::InputTextEx(label.c_str(),
                        (char*)line.data(),
                        static_cast<int>(line.size() + 1),
                        ImVec2(-1, ImGui::GetTextLineHeight() * 1.5f * float(std::max(1,(int)std::count(line.begin(),line.end(), '\n')))),
                        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor(2);

            rc = ImGui::GetCursorPos();
            ImGui::SetCursorPos({ rc.x, rc.y - 6 });
        });

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopFont();
    }

    void rs2::viewer_model::popup(const ux_window& window, const std::string& header, const std::string& message, std::function<void()> configure)
    {
        if (popup_triggered)
            return;

        popup_triggered = true;
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysVerticalScrollbar;

        auto font_14 = window.get_font();

        ImGui_ScopePushFont(font_14);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

        ImGui::SetNextWindowSize({520, 180});

        ImGui::OpenPopup(header.c_str());

        if (ImGui::BeginPopupModal(header.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PushStyleColor(ImGuiCol_Button, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);

            configure();

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }

    void viewer_model::popup_if_error(const ux_window& window, std::string& message)
    {
        if (message == "")
            return;

        // The list of errors the user asked not to show again:
        static std::set<std::string> errors_not_to_show;
        static bool dont_show_this_error = false;
        auto simplify_error_message = [](const std::string& s) {
            std::regex e("\\b(0x)([^ ,]*)");
            return std::regex_replace(s, e, "address");
        };

        if (errors_not_to_show.count(simplify_error_message(message)))
        {
            not_model.add_notification({ message,
                RS2_LOG_SEVERITY_ERROR,
                RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR });
            message = "";
            return;
        }

        std::string header = std::string(textual_icons::exclamation_triangle) + " Oops, something went wrong!";

        auto config = [&]()
        {
            ImGui::Text("RealSense error calling:");
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
            ImGui::InputTextMultiline("##error", const_cast<char*>(message.c_str()),
               message.size() + 1, { 500,95 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();

            //ImGui::SetCursorPos({ 10, 130 });
            ImGui::PopStyleColor(5);

            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                if (dont_show_this_error)
                {
                    errors_not_to_show.insert(simplify_error_message(message));
                }
                message = "";
                ImGui::CloseCurrentPopup();
                dont_show_this_error = false;
            }

            ImGui::SameLine();
            ImGui::Checkbox("Don't show this error again", &dont_show_this_error);
        };

        popup(window, header, message, config);
    }

    std::vector<uint8_t> read_fw_file(std::string file_path)
    {
        std::vector<uint8_t> rv;

        std::ifstream file(file_path, std::ios::in | std::ios::binary | std::ios::ate);
        if (file.is_open())
        {
            rv.resize(file.tellg());

            file.seekg(0, std::ios::beg);
            file.read((char*)rv.data(), rv.size());
            file.close();
        }

        return rv;
    }

    void viewer_model::show_icon(ImFont* font_18, const char* label_str, const char* text, int x, int y, int id,
        const ImVec4& text_color, const std::string& tooltip)
    {
        ImGui_ScopePushFont(font_18);
        
        std::string label = to_string() << label_str << id;

        ImGui::SetCursorScreenPos({(float)x, (float)y});
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::Text("%s", text);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered() && tooltip != "")
            ImGui::SetTooltip("%s", tooltip.c_str());

    }
    void viewer_model::show_paused_icon(ImFont* font_18, int x, int y, int id)
    {
        show_icon(font_18, "paused_icon", textual_icons::pause, x, y, id, white);
    }
    void viewer_model::show_recording_icon(ImFont* font_18, int x, int y, int id, float alpha_delta)
    {
        show_icon(font_18, "recording_icon", textual_icons::circle, x, y, id, from_rgba(255, 46, 54, static_cast<uint8_t>(alpha_delta * 255)));
    }

    void viewer_model::show_no_stream_overlay(ImFont* font_18, int min_x, int min_y, int max_x, int max_y)
    {
        ImGui::PushFont(font_18);

        auto pos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos({ (min_x + max_x) / 2.f - 150, (min_y + max_y) / 2.f - 20 });

        ImGui::PushStyleColor(ImGuiCol_Text, sensor_header_light_blue);
        std::string text = to_string() << "Nothing is streaming! Toggle " << textual_icons::toggle_off << " to start";
        ImGui::Text("%s", text.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(pos);

        ImGui::PopFont();
    }

    void viewer_model::show_no_device_overlay(ImFont* font_18, int x, int y)
    {
        auto pos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos({ float(x), float(y) });

        ImGui::PushFont(font_18);
        ImGui::PushStyleColor(ImGuiCol_Text, from_rgba(0x70, 0x8f, 0xa8, 0xff));
        ImGui::Text("Connect a RealSense Camera\nor Add Source");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(pos);
    }

    // Generate streams layout, creates a grid-like layout with factor amount of columns
    std::map<int, rect> generate_layout(const rect& r,
        int top_bar_height, size_t factor,
        const std::set<stream_model*>& active_streams,
        std::map<stream_model*, int>& stream_index
    )
    {
        std::map<int, rect> results;
        if (factor == 0) return results;

        // Calc the number of rows
        auto complement = ceil((float)active_streams.size() / factor);

        auto cell_width = static_cast<float>(r.w / factor);
        auto cell_height = static_cast<float>(r.h / complement);

        auto it = active_streams.begin();
        for (auto x = 0; x < factor; x++)
        {
            for (auto y = 0; y < complement; y++)
            {
                // There might be spare boxes at the end (3 streams in 2x2 array for example)
                if (it == active_streams.end()) break;

                rect rxy = { r.x + x * cell_width, r.y + y * cell_height + top_bar_height,
                    cell_width, cell_height - top_bar_height };
                // Generate box to display the stream in
                results[stream_index[*it]] = rxy.adjust_ratio((*it)->size);
                ++it;
            }
        }

        return results;
    }

    // Return the total display area of the layout
    // The bigger it is, the more details we can see
    float evaluate_layout(const std::map<int, rect>& l)
    {
        float res = 0.f;
        for (auto&& kvp : l) res += kvp.second.area();
        return res;
    }

    std::map<int, rect> viewer_model::calc_layout(const rect& r)
    {
        const int top_bar_height = 32;

        std::set<stream_model*> active_streams;
        std::map<stream_model*, int> stream_index;
        for (auto&& stream : streams)
        {
            if (stream.second.is_stream_visible())
            {
                active_streams.insert(&stream.second);
                stream_index[&stream.second] = stream.first;
            }
        }

        if (fullscreen)
        {
            if (active_streams.count(selected_stream) == 0) fullscreen = false;
        }

        std::map<int, rect> results;

        if (fullscreen)
        {
            results[stream_index[selected_stream]] = { r.x, r.y + top_bar_height,
                                                       r.w, r.h - top_bar_height };
        }
        else
        {
            // Go over all available fx(something) layouts
            for (size_t f = 1; f <= active_streams.size(); f++)
            {
                auto l = generate_layout(r, top_bar_height, f,
                    active_streams, stream_index);

                // Keep the "best" layout in result
                if (evaluate_layout(l) > evaluate_layout(results))
                    results = l;
            }
        }

        return get_interpolated_layout(results);
    }

    rs2::frame viewer_model::handle_ready_frames(const rect& viewer_rect, ux_window& window, int devices, std::string& error_message)
    {
        scoped_timer t("handle_ready_frames");

        frame f{};

        std::map<int, frame> last_frames;
        try
        {
            size_t index = 0;
            while (ppf.resulting_queue.poll_for_frame(&f) && ++index < ppf.resulting_queue_max_size)
            {
                last_frames[f.get_profile().unique_id()] = f;
            }

            for(auto&& frame : last_frames)
            {
                auto f = frame.second;
                frameset frames;
                if ((frames = f.as<frameset>()))
                {
                    for (auto&& frame : frames)
                    {
                        if (frame.is<pose_frame>())  // Aggregate the trajectory in pause mode to make the path consistent
                        {
                            auto dev = streams[frame.get_profile().unique_id()].dev;
                            if (dev)
                            {
                                dev->tm2.update_model_trajectory(frame.as<pose_frame>(), !paused);
                            }
                        }

                        //auto texture = upload_frame(std::move(frame));
                    }
                }
                else 
                {
                    //upload_frame(std::move(f));
                }
            }
        }
        catch (const error& ex)
        {
            error_message = error_to_string(ex);
        }
        catch (const std::exception& ex)
        {
            error_message = ex.what();
        }


        gc_streams();

        window.begin_viewport();

        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove 
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoSavedSettings 
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5, 5 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

        ImGui::SetNextWindowPos({ viewer_rect.x, viewer_rect.y });
        ImGui::SetNextWindowSize({ viewer_rect.w, viewer_rect.h });

        ImGui::Begin("Viewport", nullptr, { viewer_rect.w, viewer_rect.h }, 0.f, flags);

        draw_viewport(viewer_rect, window, devices, error_message);

        not_model.draw(window, 
            static_cast<int>(window.width()), static_cast<int>(window.height()),
            error_message);

        popup_if_error(window, error_message);

        ImGui::End();
        ImGui::PopStyleVar(2);

        popup_triggered = false;
        return f;
    }

    void viewer_model::reset_camera(float3 p)
    {
        target = { 0.0f, 0.0f, 0.0f };
        pos = p;

        // initialize "up" to be tangent to the sphere!
        // up = cross(cross(look, world_up), look)
        {
            float3 look = { target.x - pos.x, target.y - pos.y, target.z - pos.z };
            look = look.normalize();

            float world_up[3] = { 0.0f, 1.0f, 0.0f };

            float across[3] = {
                look.y * world_up[2] - look.z * world_up[1],
                look.z * world_up[0] - look.x * world_up[2],
                look.x * world_up[1] - look.y * world_up[0],
            };

            up.x = across[1] * look.z - across[2] * look.y;
            up.y = across[2] * look.x - across[0] * look.z;
            up.z = across[0] * look.y - across[1] * look.x;

            float up_len = up.length();
            up.x /= -up_len;
            up.y /= -up_len;
            up.z /= -up_len;
        }
    }

    void viewer_model::draw_color_ruler(const mouse_info& mouse,
                                        const stream_model& s_model,
                                        const rect& stream_rect,
                                        std::vector<rgb_per_distance> rgb_per_distance_vec,
                                        float ruler_length,
                                        const std::string& ruler_units)
    {
        if (rgb_per_distance_vec.empty() || (ruler_length <= 0.f))
            return;

        ruler_length = std::ceil(ruler_length);
        std::sort(rgb_per_distance_vec.begin(), rgb_per_distance_vec.end(), [](const rgb_per_distance& a,
            const rgb_per_distance& b) {
            return a.depth_val < b.depth_val;
        });

        const auto stream_height = stream_rect.y + stream_rect.h;
        const auto stream_width = stream_rect.x + stream_rect.w;

        static const auto ruler_distance_offset = 10;
        auto bottom_y_ruler = stream_height - ruler_distance_offset;
        if (s_model.texture->zoom_preview)
        {
            bottom_y_ruler = s_model.texture->curr_preview_rect.y - ruler_distance_offset;
        }

        static const auto top_y_offset = 50;
        auto top_y_ruler = stream_rect.y + top_y_offset;
        if (s_model.show_stream_details)
        {
            top_y_ruler = s_model.curr_info_rect.y + s_model.curr_info_rect.h + ruler_distance_offset;
        }

        static const auto left_x_colored_ruler_offset = 50;
        static const auto colored_ruler_width = 20;
        const auto left_x_colored_ruler = stream_width - left_x_colored_ruler_offset;
        const auto right_x_colored_ruler = stream_width - (left_x_colored_ruler_offset - colored_ruler_width);
        const auto first_rgb = rgb_per_distance_vec.begin()->rgb_val;
        assert((bottom_y_ruler - top_y_ruler) != 0.f);
        const auto ratio = (bottom_y_ruler - top_y_ruler) / ruler_length;

        // Draw numbered ruler
        float y_ruler_val = top_y_ruler;
        static const auto numbered_ruler_width = 20.f;
        const auto numbered_ruler_height = bottom_y_ruler - top_y_ruler;

        const auto right_x_numbered_ruler = right_x_colored_ruler + numbered_ruler_width;
        static const auto hovered_numbered_ruler_opac = 0.8f;
        static const auto unhovered_numbered_ruler_opac = 0.6f;
        float colored_ruler_opac = unhovered_numbered_ruler_opac;
        float numbered_ruler_background_opac = unhovered_numbered_ruler_opac;
        bool is_ruler_hovered = false;
        if (mouse.cursor.x >= left_x_colored_ruler &&
            mouse.cursor.x <= right_x_numbered_ruler &&
            mouse.cursor.y >= top_y_ruler &&
            mouse.cursor.y <= bottom_y_ruler)
            is_ruler_hovered = true;

        if (is_ruler_hovered)
        {
            std::stringstream ss;
            auto relative_mouse_y = ImGui::GetMousePos().y - top_y_ruler;
            auto y = (bottom_y_ruler - top_y_ruler) - relative_mouse_y;
            ss << std::fixed << std::setprecision(2) << (y / ratio) << ruler_units;
            ImGui::SetTooltip("%s", ss.str().c_str());
            colored_ruler_opac = 1.f;
            numbered_ruler_background_opac = hovered_numbered_ruler_opac;
        }

        // Draw a background to the numbered ruler
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0, 0.0, 0.0, numbered_ruler_background_opac);
        glBegin(GL_POLYGON);
        glVertex2f(right_x_colored_ruler, top_y_ruler);
        glVertex2f(right_x_numbered_ruler , top_y_ruler);
        glVertex2f(right_x_numbered_ruler , bottom_y_ruler);
        glVertex2f(right_x_colored_ruler, bottom_y_ruler);
        glEnd();


        const float x_ruler_val = right_x_colored_ruler + 4.0f;
        ImGui::SetCursorScreenPos({ x_ruler_val, y_ruler_val });
        const auto font_size = ImGui::GetFontSize();
        ImGui::TextUnformatted(std::to_string(static_cast<int>(ruler_length)).c_str());
        const auto skip_numbers = ((ruler_length / 10.f) - 1.f);
        auto to_skip = (skip_numbers < 0.f)?0.f: skip_numbers;
        for (int i = static_cast<int>(ruler_length - 1); i > 0; --i)
        {
            y_ruler_val += ((bottom_y_ruler - top_y_ruler) / ruler_length);
            ImGui::SetCursorScreenPos({ x_ruler_val, y_ruler_val - font_size / 2 });
            if (((to_skip--) > 0))
                continue;

            ImGui::TextUnformatted(std::to_string(i).c_str());
            to_skip = skip_numbers;
        }
        y_ruler_val += ((bottom_y_ruler - top_y_ruler) / ruler_length);
        ImGui::SetCursorScreenPos({ x_ruler_val, y_ruler_val - font_size });
        ImGui::Text("0");

        auto total_depth_scale = rgb_per_distance_vec.back().depth_val - rgb_per_distance_vec.front().depth_val;
        static const auto sensitivity_factor = 0.01f;
        auto sensitivity = sensitivity_factor * total_depth_scale;

        // Draw colored ruler
        auto last_y = bottom_y_ruler;
        auto last_depth_value = 0.f;
        size_t last_index = 0;
        for (size_t i = 1; i < rgb_per_distance_vec.size(); ++i)
        {
            auto curr_depth = rgb_per_distance_vec[i].depth_val;
            if ((((curr_depth - last_depth_value) < sensitivity) && (i != rgb_per_distance_vec.size() - 1)))
                continue;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_QUADS);
            glColor4f(rgb_per_distance_vec[last_index].rgb_val.r / 255.f,
                      rgb_per_distance_vec[last_index].rgb_val.g / 255.f,
                      rgb_per_distance_vec[last_index].rgb_val.b / 255.f,
                      colored_ruler_opac);
            glVertex2f(left_x_colored_ruler, last_y);
            glVertex2f(right_x_colored_ruler, last_y);

            last_depth_value = curr_depth;
            last_index = i;

            auto y = bottom_y_ruler - ((rgb_per_distance_vec[i].depth_val) * ratio);
            if ((i == (rgb_per_distance_vec.size() - 1)) || (std::ceil(curr_depth) > ruler_length))
                y = top_y_ruler;

            glColor4f(rgb_per_distance_vec[i].rgb_val.r / 255.f,
                      rgb_per_distance_vec[i].rgb_val.g / 255.f,
                      rgb_per_distance_vec[i].rgb_val.b / 255.f,
                      colored_ruler_opac);

            glVertex2f(right_x_colored_ruler, y);
            glVertex2f(left_x_colored_ruler, y);
            last_y = y;
            glEnd();
        }

        // Draw ruler border
        static const auto top_line_offset = 0.5f;
        static const auto right_line_offset = top_line_offset / 2;
        glColor4f(0.0, 0.0, 0.0, colored_ruler_opac);
        glBegin(GL_LINE_LOOP);
        glVertex2f(left_x_colored_ruler - top_line_offset, top_y_ruler - top_line_offset);
        glVertex2f(right_x_numbered_ruler + right_line_offset / 2, top_y_ruler - top_line_offset);
        glVertex2f(right_x_numbered_ruler + right_line_offset / 2, bottom_y_ruler + top_line_offset);
        glVertex2f(left_x_colored_ruler - top_line_offset, bottom_y_ruler + top_line_offset);
        glEnd();
    }

    float viewer_model::calculate_ruler_max_distance(const std::vector<float>& distances) const
    {
        assert(!distances.empty());

        float mean = std::accumulate(distances.begin(),
            distances.end(), 0.0f) / distances.size();

        float e = 0;
        float inverse = 1.f / distances.size();
        for (auto elem : distances)
        {
            e += pow(elem - mean, 2);
        }

        auto standard_deviation = sqrt(inverse * e);
        static const auto length_jump = 4.f;
        return std::ceil((mean + 1.5f * standard_deviation) / length_jump) * length_jump;
    }

    void viewer_model::render_2d_view(const rect& view_rect,
        ux_window& win, int output_height,
        ImFont *font1, ImFont *font2, size_t dev_model_num,
        const mouse_info &mouse, std::string& error_message)
    {
        static periodic_timer every_sec(std::chrono::seconds(1));
        static bool icon_visible = false;
        if (every_sec) icon_visible = !icon_visible;
        float alpha = icon_visible ? 1.f : 0.2f;

        glViewport(0, 0,
            static_cast<GLsizei>(win.framebuf_width()), static_cast<GLsizei>(win.framebuf_height()));
        glLoadIdentity();
        glOrtho(0, win.width(), win.height(), 0, -1, +1);

        auto layout = calc_layout(view_rect);

        if ((layout.size() == 0) && (dev_model_num > 0))
        {
            show_no_stream_overlay(font2, static_cast<int>(view_rect.x), static_cast<int>(view_rect.y), static_cast<int>(win.width()), static_cast<int>(win.height() - output_height));
        }

        for (auto &&kvp : layout)
        {
            auto&& view_rect = kvp.second;
            auto stream = kvp.first;
            auto&& stream_mv = streams[stream];
            auto&& stream_size = stream_mv.size;
            auto stream_rect = view_rect.adjust_ratio(stream_size).grow(-3);

            stream_mv.show_frame(stream_rect, mouse, error_message);

            auto p = stream_mv.dev->dev.as<playback>();
            float posX = stream_rect.x + 9;
            float posY = stream_rect.y - 26;

            if (!stream_mv.is_stream_alive())
            {
                std::string message = to_string() << textual_icons::exclamation_triangle << " No Frames Received!";
                show_icon(font2, "warning_icon", message.c_str(),
                    static_cast<int>(stream_rect.center().x - 100),
                    static_cast<int>(stream_rect.center().y - 25),
                    stream_mv.profile.unique_id(),
                    blend(dark_red, alpha),
                    "Did not receive frames from the platform within a reasonable time window,\nplease try reducing the FPS or the resolution");
            }

            stream_mv.show_stream_header(font1, stream_rect, *this);
            stream_mv.show_stream_footer(font1, stream_rect, mouse, *this);

            if (stream_mv.dev->_is_being_recorded)
            {
                show_recording_icon(font2, static_cast<int>(posX), static_cast<int>(posY), stream_mv.profile.unique_id(), alpha);
                posX += 23;
            }
            if (stream_mv.dev->is_paused() || (p && p.current_status() == RS2_PLAYBACK_STATUS_PAUSED))
                show_paused_icon(font2, static_cast<int>(posX), static_cast<int>(posY), stream_mv.profile.unique_id());

            auto stream_type = stream_mv.profile.stream_type();

            if (streams[stream].is_stream_visible())
            {
                switch (stream_type)
                {
                    case RS2_STREAM_GYRO: /* Fall Through */
                    case RS2_STREAM_ACCEL:
                    {
                        auto motion = streams[stream].texture->get_last_frame().as<motion_frame>();
                        if (motion.get())
                        {
                            auto axis = motion.get_motion_data();
                            stream_mv.show_stream_imu(font1, stream_rect, axis, mouse);
                        }
                        break;
                    }
                    case RS2_STREAM_POSE:
                    {
                        if (streams[stream].show_stream_details)
                        {
                            auto pose = streams[stream].texture->get_last_frame().as<pose_frame>();
                            if (pose.get())
                            {
                                auto pose_data = pose.get_pose_data();
                                if (stream_rect.contains(win.get_mouse().cursor))
                                    stream_mv.show_stream_pose(font1, 
                                    stream_rect, pose_data, 
                                    stream_type, (*this).fullscreen, 30.0f, *this);
                            }
                        }
  
                        break;
                    }
                }
            }

            glColor3f(header_window_bg.x, header_window_bg.y, header_window_bg.z);
            stream_rect.y -= 32;
            stream_rect.h += 32;
            stream_rect.w += 1;
            draw_rect(stream_rect);

            auto frame = streams[stream].texture->get_last_frame().as<video_frame>();
            auto textured_frame = streams[stream].texture->get_last_frame(true).as<video_frame>();
            if (streams[stream].show_map_ruler && frame && textured_frame &&
                RS2_STREAM_DEPTH == stream_mv.profile.stream_type() &&
                RS2_FORMAT_Z16 == stream_mv.profile.format())
            {
                if(RS2_FORMAT_RGB8 == textured_frame.get_profile().format())
                {
                    static const std::string depth_units = "m";
                    float ruler_length = 0.f;
                    auto depth_vid_profile = stream_mv.profile.as<video_stream_profile>();
                    auto depth_width = depth_vid_profile.width();
                    auto depth_height = depth_vid_profile.height();
                    auto num_of_pixels = depth_width * depth_height;
                    auto depth_data = static_cast<const uint16_t*>(frame.get_data());
                    auto textured_depth_data = static_cast<const uint8_t*>(textured_frame.get_data());
                    static const auto skip_pixels_factor = 30;
                    std::vector<rgb_per_distance> rgb_per_distance_vec;
                    std::vector<float> distances;
                    for (uint64_t i = 0; i < depth_height; i+= skip_pixels_factor)
                    {
                        for (uint64_t j = 0; j < depth_width; j+= skip_pixels_factor)
                        {
                            auto depth_index = i*depth_width + j;
                            auto length = depth_data[depth_index] * stream_mv.dev->depth_units;
                            if (length > 0.f)
                            {
                                auto textured_depth_index = depth_index * 3;
                                auto r = textured_depth_data[textured_depth_index];
                                auto g = textured_depth_data[textured_depth_index + 1];
                                auto b = textured_depth_data[textured_depth_index + 2];
                                rgb_per_distance_vec.push_back({ length, { r, g, b } });
                                distances.push_back(length);
                            }
                        }
                    }

                    if (!distances.empty())
                    {
                        ruler_length = calculate_ruler_max_distance(distances);
                        draw_color_ruler(mouse, streams[stream], stream_rect, rgb_per_distance_vec, ruler_length, depth_units);
                    }
                }
            }
        }

        // Metadata overlay windows shall be drawn after textures to preserve z-buffer functionality
        for (auto &&kvp : layout)
        {
            if (streams[kvp.first].metadata_displayed)
                streams[kvp.first].show_metadata(mouse);
        }
    }

    void viewer_model::try_select_pointcloud(ux_window& win, series_3d& s)
    {
        win.link_hovered();
        if (win.get_mouse().mouse_down && !_pc_selected_down) 
        {
            _pc_selected_down = true;
            _selection_started = win.time();
        }
        if (_pc_selected_down && !win.get_mouse().mouse_down)
        {
            _pc_selected_down = false;
            if (win.time() - _selection_started < 0.5)
            {
                s.selected = !s.selected;

                if (s.selected)
                foreach_series([&](series_3d& s2) {
                    if (s.selected_depth_source_uid != s2.selected_depth_source_uid)
                        s2.selected = false;
                });
            }
        }
    }

    bool viewer_model::render_3d_view(const rect& viewer_rect, ux_window& win, float3* picked_xyz)
    {
        scoped_timer t("render_3d_view");

        bool picked = false;
        auto top_bar_height = 32.f;

        glViewport(static_cast<GLint>(viewer_rect.x), 0,
            static_cast<GLsizei>(viewer_rect.w), static_cast<GLsizei>(win.framebuf_height()));

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        matrix4 perspective_mat = create_perspective_projection_matrix(viewer_rect.w, win.framebuf_height(), 60, 0.001f, 100.f);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadMatrixf((float*)perspective_mat.mat);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadMatrixf(view);

        matrix4 view_mat;
        memcpy(&view_mat, view, sizeof(matrix4));

        glDisable(GL_TEXTURE_2D);

        glEnable(GL_DEPTH_TEST);

        auto r1 = matrix4::identity();
        auto r2 = matrix4::identity();

        if (draw_plane)
        {
            glPushAttrib(GL_LINE_BIT);
            glLineWidth(2);
            glBegin(GL_LINES);

            if (is_valid(roi_rect))
            {
                glColor4f(yellow.x, yellow.y, yellow.z, 1.f);

                auto rects = subdivide(roi_rect);
                for (auto&& r : rects)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        auto j = (i + 1) % 4;
                        glVertex3f(r[i].x, r[i].y, r[i].z);
                        glVertex3f(r[j].x, r[j].y, r[j].z);
                    }
                }
            }

            glEnd();
            glPopAttrib();
        }

        auto x = static_cast<float>(-M_PI / 2);
        float _rx[4][4] = {
            { 1 , 0, 0, 0 },
            { 0, cos(x), -sin(x), 0 },
            { 0, sin(x), cos(x), 0 },
            { 0, 0, 0, 1 }
        };
        auto z = static_cast<float>(M_PI);
        float _rz[4][4] = {
            { cos(z), -sin(z),0, 0 },
            { sin(z), cos(z), 0, 0 },
            { 0 , 0, 1, 0 },
            { 0, 0, 0, 1 }
        };
        rs2::matrix4 rx(_rx);
        rs2::matrix4 rz(_rz);

        int stream_num = 0; // counter to locate the pose info window correctly (currently works for up to 3 streaming devices)
        pose_frame pose = frame{};
        for (auto&& stream : streams)
        {
            if (stream.second.profile.stream_type() == RS2_STREAM_POSE)
            {
                auto f = stream.second.texture->get_last_frame();
                if (!f.is<pose_frame>())
                {
                    continue;
                }

                pose = f;
                rs2_pose pose_data = pose.get_pose_data();
                if (show_pose_info_3d)
                {
                    auto stream_type = stream.second.profile.stream_type();
                    auto stream_rect = viewer_rect;
                    stream_rect.x += 460 * stream_num;
                    stream_rect.y += 2 * top_bar_height + 5;
                    stream.second.show_stream_pose(win.get_font(), stream_rect, pose_data, stream_type, true, 0, *this);
                }

                auto t = tm2_pose_to_world_transformation(pose_data);
                float model[4][4];
                t.to_column_major((float*)model);
                auto m = model;

                r1 = m * rx;
                r2 = rz * m * rx;

                // set the pose transformation as the model matrix to draw the axis
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadMatrixf(view);

                glMultMatrixf((float*)_rx);

                streams[f.get_profile().unique_id()].dev->tm2.draw_trajectory(trajectory_button.is_pressed());

                // remove model matrix from the rest of the render
                glPopMatrix();

                _cam_renderer.set_matrix(RS2_GL_MATRIX_CAMERA, r2 * view_mat);
                _cam_renderer.set_matrix(RS2_GL_MATRIX_PROJECTION, perspective_mat);

                if (f)
                {
                    glDisable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);

                    glBlendFunc(GL_ONE, GL_ONE);

                    // Render camera model (based on source_frame camera type)
                    f.apply_filter(_cam_renderer);

                    glDisable(GL_BLEND);
                    glEnable(GL_DEPTH_TEST);
                }

                stream_num++;
            }
        }

        {
            auto tiles = 12;
            if (!metric_system) tiles *= 1.f / FEET_TO_METER;

            // Render "floor" grid
            glLineWidth(1);
            glBegin(GL_LINES);

            auto T = tiles * 0.5f;

            if (!metric_system) T *= FEET_TO_METER;

            glTranslatef(0, 0, -1);

            for (int i = 0; i <= ceil(tiles); i++)
            {
                float I = i;
                if (!metric_system) I *= FEET_TO_METER;

                if (i == tiles / 2) glColor4f(0.7f, 0.7f, 0.7f, 1.f);
                else glColor4f(0.4f, 0.4f, 0.4f, 1.f);

                glVertex3f(I - T, 1, -T);
                glVertex3f(I - T, 1, T);
                glVertex3f(-T, 1, I - T);
                glVertex3f(T, 1, I - T);
            }
            glEnd();
        }

        auto cursor = win.get_mouse().cursor;

        foreach_series([&](series_3d& s)
        {

            if (!pose && !s.selected)
            {
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadMatrixf(s.model * r1 * view_mat);
                texture_buffer::draw_axes(0.4f, 1);
                glPopMatrix();
            }

            glColor4f(1.f, 1.f, 1.f, 1.f);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadMatrixf(s.model * r1 * view_mat);
            if (draw_frustrum && s.last_points)
            {
                if (s.selected) glColor4f(regular_blue.x, regular_blue.y, regular_blue.z, 0.2f);
                else glColor4f(sensor_bg.x, sensor_bg.y, sensor_bg.z, 0.5f);

                glLineWidth(1.f);
                glBegin(GL_LINES);

                auto intrin = s.last_points.get_profile().as<video_stream_profile>().get_intrinsics();
            
                for (float d = 1; d < 6; d += 2)
                {
                    auto get_point = [&](float x, float y) -> float3
                    {
                        float point[3];
                        float pixel[2]{ x, y };
                        rs2_deproject_pixel_to_point(point, &intrin, pixel, d);
                        glVertex3f(0.f, 0.f, 0.f);
                        glVertex3fv(point);
                        return{ point[0], point[1], point[2] };
                    };

                    auto top_left = get_point(0, 0);
                    auto top_right = get_point(static_cast<float>(intrin.width), 0);
                    auto bottom_right = get_point(static_cast<float>(intrin.width), static_cast<float>(intrin.height));
                    auto bottom_left = get_point(0, static_cast<float>(intrin.height));

                    glVertex3fv(&top_left.x); glVertex3fv(&top_right.x);
                    glVertex3fv(&top_right.x); glVertex3fv(&bottom_right.x);
                    glVertex3fv(&bottom_right.x); glVertex3fv(&bottom_left.x);
                    glVertex3fv(&bottom_left.x); glVertex3fv(&top_left.x);
                }

                glEnd();

                glColor4f(1.f, 1.f, 1.f, 1.f);
            }

            if (s.selected)
            {
                ImGuiIO& io = ImGui::GetIO();
                ImGuizmo::SetRect(viewer_rect.x, 0, viewer_rect.w, win.framebuf_height());

                if (ImGui::IsKeyDown(GLFW_KEY_LEFT_CONTROL))
                {
                    float snap = 45.f;
                    ImGuizmo::Manipulate(view_mat, perspective_mat, 
                        ImGuizmo::ROTATE, 
                        ImGuizmo::LOCAL, s.model, nullptr, ImGui::IsKeyDown(GLFW_KEY_LEFT_ALT) ? &snap : nullptr);
                }
                else
                    ImGuizmo::Manipulate(view_mat, perspective_mat, 
                        ImGuizmo::TRANSLATE, 
                        ImGuizmo::LOCAL, s.model);
            }

            if (s.last_points && s.last_texture)
            {
                auto vf_profile = s.last_points.get_profile().as<video_stream_profile>();
                // Non-linear correspondence customized for non-flat surface exploration
                glPointSize(std::sqrt(viewer_rect.w / vf_profile.width()));

                auto tex = s.last_texture->get_gl_handle();
                glBindTexture(GL_TEXTURE_2D, tex);
                glEnable(GL_TEXTURE_2D);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                s.pc_renderer.set_option(gl::pointcloud_renderer::OPTION_FILLED, s.render_quads ? 1.f : 0.f);

                s.pc_renderer.set_matrix(RS2_GL_MATRIX_CAMERA, s.model * r2 * view_mat);
                s.pc_renderer.set_matrix(RS2_GL_MATRIX_PROJECTION, perspective_mat);

                s.pc_renderer.set_option(gl::pointcloud_renderer::OPTION_SELECTED, s.selected ? 1.f : 0.f);
                
                if (viewer_rect.contains(cursor))
                {
                    if (win.time() - s.last_picked < 0.5)
                    {
                        *picked_xyz = s.last_picked_value;
                        picked = true;
                    }
                    else
                    {
                        s.pc_renderer.set_option(gl::pointcloud_renderer::OPTION_MOUSE_PICK, 1.f);
                        s.pc_renderer.set_option(gl::pointcloud_renderer::OPTION_MOUSE_X, cursor.x);
                        s.pc_renderer.set_option(gl::pointcloud_renderer::OPTION_MOUSE_Y, cursor.y);
                    }
                }

                // Render Point-Cloud
                s.last_points.apply_filter(s.pc_renderer);

                if (s.pc_renderer.get_option(gl::pointcloud_renderer::OPTION_PICKED_ID) > 0.f)
                {
                    float3 p{
                        s.pc_renderer.get_option(gl::pointcloud_renderer::OPTION_PICKED_X),
                        s.pc_renderer.get_option(gl::pointcloud_renderer::OPTION_PICKED_Y),
                        s.pc_renderer.get_option(gl::pointcloud_renderer::OPTION_PICKED_Z),
                    };
                    picked = true;
                    *picked_xyz = p;

                    s.last_picked = win.time();
                    s.last_picked_value = p;

                    //try_select_pointcloud(win);
                }
            }
        });

        foreach_series([&](series_3d& s)
        {
            if (s.last_points && s.last_texture)
            {
                glDisable(GL_TEXTURE_2D);

                _cam_renderer.set_matrix(RS2_GL_MATRIX_CAMERA, s.model * r2 * view_mat);
                _cam_renderer.set_matrix(RS2_GL_MATRIX_PROJECTION, perspective_mat);

                if (streams.find(s.selected_depth_source_uid) != streams.end())
                {
                    auto source_frame = streams[s.selected_depth_source_uid].texture->get_last_frame();
                    if (source_frame)
                    {
                        glDisable(GL_DEPTH_TEST);
                        glEnable(GL_BLEND);

                        glBlendFunc(GL_ONE, GL_ONE);

                        _cam_renderer.set_option(gl::camera_renderer::OPTION_SELECTED, s.selected ? 1.f : 0.f);

                        if (viewer_rect.contains(cursor))
                        {
                            _cam_renderer.set_option(gl::camera_renderer::OPTION_MOUSE_PICK, 1.f);
                            _cam_renderer.set_option(gl::camera_renderer::OPTION_MOUSE_X, cursor.x);
                            _cam_renderer.set_option(gl::camera_renderer::OPTION_MOUSE_Y, cursor.y);
                        }

                        // Render camera model (based on source_frame camera type)
                        source_frame.apply_filter(_cam_renderer);

                        if (_cam_renderer.get_option(gl::camera_renderer::OPTION_WAS_PICKED) > 0.f)
                        {
                            try_select_pointcloud(win, s);
                        }

                        glDisable(GL_BLEND);
                        glEnable(GL_DEPTH_TEST);
                    }
                }
            }

            glPopMatrix();
        });

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopAttrib();

        glDisable(GL_DEPTH_TEST);

        if (ImGui::IsKeyPressed('R') || ImGui::IsKeyPressed('r'))
        {
            reset_camera();
        }

        return picked;
    }

    void viewer_model::show_top_bar(ux_window& window, const rect& viewer_rect, const device_models_list& devices)
    {
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::SetNextWindowPos({ panel_width, 0 });
        ImGui::SetNextWindowSize({ window.width() - panel_width, panel_y });

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, button_color);
        ImGui::Begin("Toolbar Panel", nullptr, flags);

        ImGui::PushFont(window.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_Border, black);

        int buttons = window.is_fullscreen() ? 4 : 3;

        ImGui::SetCursorPosX(window.width() - panel_width - panel_y * (buttons));
        ImGui::PushStyleColor(ImGuiCol_Text, is_3d_view ? light_grey : light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, is_3d_view ? light_grey : light_blue);
        if (ImGui::Button("2D", { panel_y, panel_y })) 
        {
            is_3d_view = false;
            config_file::instance().set(configurations::viewer::is_3d_view, is_3d_view);
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        ImGui::SetCursorPosX(window.width() - panel_width - panel_y * (buttons - 1));
        auto pos1 = ImGui::GetCursorScreenPos();

        ImGui::PushStyleColor(ImGuiCol_Text, !is_3d_view ? light_grey : light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, !is_3d_view ? light_grey : light_blue);
        if (ImGui::Button("3D", { panel_y,panel_y }))
        {
            is_3d_view = true;
            config_file::instance().set(configurations::viewer::is_3d_view, is_3d_view);
            update_3d_camera(window, viewer_rect, true);
        }

        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        ImGui::SetCursorPosX(window.width() - panel_width - panel_y * (buttons - 2));

        static bool settings_open = false;
        ImGui::PushStyleColor(ImGuiCol_Text, !settings_open ? light_grey : light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, !settings_open ? light_grey : light_blue);
        
        if (ImGui::Button(u8"\uf013\uf0d7", { panel_y,panel_y }))
        {
            ImGui::OpenPopup("More Options");
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", "More Options...");
        }

        if (window.is_fullscreen())
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX(window.width() - panel_width - panel_y * (buttons - 3));

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_color);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_red);
            if (ImGui::Button(textual_icons::exit, { panel_y,panel_y }))
            {
                exit(0);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Exit the App");
                window.link_hovered();
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::PopFont();

        ImGui::PushStyleColor(ImGuiCol_Text, black);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));

        ImGui::PushFont(window.get_font());

        auto settings = "Settings";
        auto about = "About";
        bool open_settings_popup = false;
        bool open_about_popup = false;

        ImGui::SetNextWindowPos({ window.width() - 120, panel_y });
        ImGui::SetNextWindowSize({ 120, 90 });

        if (ImGui::BeginPopup("More Options"))
        {
            settings_open = true;

            if (ImGui::Selectable("Report Issue"))
            {
                open_issue(devices);
            }

            if (ImGui::Selectable("RealSense Store"))
            {
                open_url(store_url);
            }

            if (ImGui::Selectable(settings))
            {
                open_settings_popup = true;
            }
            
            ImGui::Separator();

            if (ImGui::Selectable(about))
            {
                open_about_popup = true;
            }

            ImGui::EndPopup();
        }
        else
        {
            settings_open = false;
        }

        static config_file temp_cfg;
        static bool reload_required = false;
        static bool refresh_required = false;

        static int tab = 0;

        if (open_settings_popup) 
        {
            temp_cfg = config_file::instance();
            ImGui::OpenPopup(settings);   
            reload_required = false;    
            refresh_required = false;
            tab = config_file::instance().get_or_default(configurations::viewer::settings_tab, 0);             
        }

        {
            float w  = window.width()  * 0.6f;
            float h  = window.height() * 0.6f;
            float x0 = window.width()  * 0.2f;
            float y0 = window.height() * 0.2f;
            ImGui::SetNextWindowPos({ x0, y0 });
            ImGui::SetNextWindowSize({ w, h });

            flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

            ImGui_ScopePushFont(window.get_font());
            ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

            if (ImGui::BeginPopupModal(settings, nullptr, flags))
            {
                ImGui::SetCursorScreenPos({ (float)(x0 + w / 2 - 220), (float)(y0 + 27) });
                ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
                ImGui::PushFont(window.get_large_font());

                ImGui::PushStyleColor(ImGuiCol_Text, tab != 0 ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tab != 0 ? light_grey : light_blue);
                if (ImGui::Button("Playback & Record", { 170, 30})) 
                {
                    tab = 0;
                    config_file::instance().set(configurations::viewer::settings_tab, tab);
                    temp_cfg.set(configurations::viewer::settings_tab, tab);
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Text, tab != 1 ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tab != 1 ? light_grey : light_blue);
                if (ImGui::Button("Performance", { 150, 30})) 
                {
                    tab = 1;
                    config_file::instance().set(configurations::viewer::settings_tab, tab);
                    temp_cfg.set(configurations::viewer::settings_tab, tab);
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Text, tab != 2 ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tab != 2 ? light_grey : light_blue);
                if (ImGui::Button("General", { 100, 30})) 
                {
                    tab = 2;
                    config_file::instance().set(configurations::viewer::settings_tab, tab);
                    temp_cfg.set(configurations::viewer::settings_tab, tab);
                }
                ImGui::PopStyleColor(2);
                //ImGui::SameLine();

                ImGui::PopFont();
                ImGui::PopStyleColor(2); // button color

                ImGui::SetCursorScreenPos({ (float)(x0 + 15), (float)(y0 + 65) });
                ImGui::Separator();

                if (tab == 0)
                {
                    int recording_setting = temp_cfg.get(configurations::record::file_save_mode);
                    ImGui::Text("When starting a new recording:");
                    if (ImGui::RadioButton("Select filename automatically", recording_setting == 0))
                    {
                        recording_setting = 0;
                        temp_cfg.set(configurations::record::file_save_mode, recording_setting);
                    }
                    if (ImGui::RadioButton("Ask me every time", recording_setting == 1))
                    {
                        recording_setting = 1;
                        temp_cfg.set(configurations::record::file_save_mode, recording_setting);
                    }
                    ImGui::Text("Default recording folder: ");
                    ImGui::SameLine();
                    static char path[256];
                    memset(path, 0, 256);
                    std::string path_str = temp_cfg.get(configurations::record::default_path);
                    memcpy(path, path_str.c_str(), std::min(255, (int)path_str.size()));

                    if (ImGui::InputText("##default_record_path", path, 255))
                    {
                        path_str = path;
                        temp_cfg.set(configurations::record::default_path, path_str);
                    }

                    ImGui::Separator();

                    ImGui::Text("ROS-bag Compression:");
                    int recording_compression = temp_cfg.get(configurations::record::compression_mode);
                    if (ImGui::RadioButton("Always Compress (might cause frame drops)", recording_compression == 0))
                    {
                        recording_compression = 0;
                        temp_cfg.set(configurations::record::compression_mode, recording_compression);
                    }
                    if (ImGui::RadioButton("Never Compress (larger .bag file size)", recording_compression == 1))
                    {
                        recording_compression = 1;
                        temp_cfg.set(configurations::record::compression_mode, recording_compression);
                    }
                    if (ImGui::RadioButton("Use device defaults", recording_compression == 2))
                    {
                        recording_compression = 2;
                        temp_cfg.set(configurations::record::compression_mode, recording_compression);
                    }
                }

                if (tab == 1)
                {
                    int font_samples = temp_cfg.get(configurations::performance::font_oversample);
                    ImGui::Text("Font Samples: "); 
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Increased font samples produce nicer text, but require more GPU memory, sometimes resulting in boxes instead of font characters");
                    ImGui::SameLine();
                    ImGui::PushItemWidth(80);
                    if (ImGui::SliderInt("##font_samples", &font_samples, 1, 8))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::performance::font_oversample, font_samples);
                    }
                    ImGui::PopItemWidth();

#ifndef __APPLE__ // Not available at the moment on Mac
                    bool gpu_rendering = temp_cfg.get(configurations::performance::glsl_for_rendering);
                    if (ImGui::Checkbox("Use GLSL for Rendering", &gpu_rendering))
                    {
                        refresh_required = true;
                        temp_cfg.set(configurations::performance::glsl_for_rendering, gpu_rendering);
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Using OpenGL 3 shaders is a widely supported way to boost rendering speeds on modern GPUs.");

                    bool gpu_processing = temp_cfg.get(configurations::performance::glsl_for_processing);
                    if (ImGui::Checkbox("Use GLSL for Processing", &gpu_processing))
                    {
                        refresh_required = true;
                        temp_cfg.set(configurations::performance::glsl_for_processing, gpu_processing);
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Using OpenGL 3 shaders for depth data processing can reduce CPU utilisation.");

                    if (gpu_processing && !gpu_rendering)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                        ImGui::Text(u8"\uf071 Using GLSL for processing but not for rendering can reduce CPU utilisation, but is likely to hurt overall performance!");
                        ImGui::PopStyleColor();
                    }
#endif
                    bool msaa = temp_cfg.get(configurations::performance::enable_msaa);
                    if (ImGui::Checkbox("Enable Multisample Anti-Aliasing (MSAA)", &msaa))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::performance::enable_msaa, msaa);
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("MSAA will improve the rendering quality of edges at expense of greater GPU memory utilisation.");

                    if (msaa)
                    {
                        int samples = temp_cfg.get(configurations::performance::msaa_samples);
                        ImGui::Text("MSAA Samples: "); ImGui::SameLine();
                        ImGui::PushItemWidth(160);
                        if (ImGui::SliderInt("##samples", &samples, 2, 16))
                        {
                            reload_required = true;
                            temp_cfg.set(configurations::performance::msaa_samples, samples);
                        }
                        ImGui::PopItemWidth();
                    }

                    bool show_fps = temp_cfg.get(configurations::performance::show_fps);
                    if (ImGui::Checkbox("Show Application FPS (rendering FPS)", &show_fps))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::performance::show_fps, show_fps);
                    }

                    bool vsync = temp_cfg.get(configurations::performance::vsync);
                    if (ImGui::Checkbox("Enable VSync", &vsync))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::performance::vsync, vsync);
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Vertical sync will try to synchronize application framerate to the monitor refresh-rate (usually limiting the framerate to 60)");

                    bool fullscreen = temp_cfg.get(configurations::window::is_fullscreen);
                    if (ImGui::Checkbox("Fullscreen (F8)", &fullscreen))
                    {
                        reload_required = true;
                        temp_cfg.set(configurations::window::is_fullscreen, fullscreen);
                    }
                }

                if (tab == 2)
                {
                    ImGui::Text("Units of Measurement: ");
                    ImGui::SameLine();

                    int metric_system = temp_cfg.get(configurations::viewer::metric_system);
                    std::vector<std::string> unit_systems;
                    unit_systems.push_back("Imperial System");
                    unit_systems.push_back("Metric System");

                    ImGui::PushItemWidth(150);
                    if (draw_combo_box("##units_system", unit_systems, metric_system))
                    {
                        temp_cfg.set(configurations::viewer::metric_system, metric_system);
                    }
                    ImGui::PopItemWidth();

                    ImGui::Separator();

                    ImGui::Text("librealsense has built-in logging capabilities.");
                    ImGui::Text("Logs may contain API calls, timing of frames, OS error messages and file-system links, but no actual frame content.");

                    bool log_to_console = temp_cfg.get(configurations::viewer::log_to_console);
                    if (ImGui::Checkbox("Output librealsense log to console", &log_to_console))
                    {
                        temp_cfg.set(configurations::viewer::log_to_console, log_to_console);
                    }
                    bool log_to_file = temp_cfg.get(configurations::viewer::log_to_file);
                    if (ImGui::Checkbox("Output librealsense log to file", &log_to_file))
                    {
                        temp_cfg.set(configurations::viewer::log_to_file, log_to_file);
                    }
                    if (log_to_file)
                    {
                        ImGui::Text("Log file name:");
                        ImGui::SameLine();
                        static char logpath[256];
                        memset(logpath, 0, 256);
                        std::string path_str = temp_cfg.get(configurations::viewer::log_filename);
                        memcpy(logpath, path_str.c_str(), std::min(255, (int)path_str.size()));

                        if (ImGui::InputText("##default_log_path", logpath, 255))
                        {
                            path_str = logpath;
                            temp_cfg.set(configurations::viewer::log_filename, path_str);
                        }
                    }
                    if (log_to_console || log_to_file)
                    {
                        int new_severity = temp_cfg.get(configurations::viewer::log_severity);

                        std::vector<std::string> severities;
                        for (int i = 0; i < RS2_LOG_SEVERITY_COUNT; i++)
                            severities.push_back(rs2_log_severity_to_string((rs2_log_severity)i));

                        ImGui::Text("Minimal log severity:");
                        ImGui::SameLine();

                        ImGui::PushItemWidth(150);
                        if (draw_combo_box("##log_severity", severities, new_severity))
                        {
                            temp_cfg.set(configurations::viewer::log_severity, new_severity);
                        }
                        ImGui::PopItemWidth();
                    }


                    ImGui::Separator();

                    ImGui::Text("RealSense tools settings capture the state of UI, and not of the hardware:");

                    if (ImGui::Button(" Restore Defaults "))
                    {
                        reload_required = true;
                        temp_cfg = config_file();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(" Export Settings "))
                    {
                        auto ret = file_dialog_open(save_file, "JavaScript Object Notation (JSON)\0*.json\0", NULL, NULL);
                        if (ret)
                        {
                            try
                            {
                                std::string filename = ret;
                                filename = to_lower(filename);
                                if (!ends_with(filename, ".json")) filename += ".json";
                                temp_cfg.save(filename.c_str());
                            }
                            catch (...){}
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(" Import Settings "))
                    {
                        auto ret = file_dialog_open(open_file, "JavaScript Object Notation (JSON)\0*.json\0", NULL, NULL);
                        if (ret)
                        {
                            try
                            {
                                config_file file(ret);
                                temp_cfg = file;
                                reload_required = true;
                            }
                            catch (...){}
                        }
                    }
                }

                ImGui::Separator();

                ImGui::GetWindowDrawList()->AddRectFilled({ (float)x0, (float)(y0 + h - 60) },
                    { (float)(x0 + w), (float)(y0 + h) }, ImColor(sensor_bg));

                ImGui::SetCursorScreenPos({ (float)(x0 + 15), (float)(y0 + h - 60) });
                if (reload_required)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    ImGui::Text(u8"\uf071 The application will be restarted in order for new settings to take effect");
                    ImGui::PopStyleColor();
                }

                auto apply = [&](){
                    config_file::instance() = temp_cfg;
                    if (reload_required) window.reload();
                    else if (refresh_required) window.refresh();
                    update_configuration();
                };
        
                ImGui::SetCursorScreenPos({ (float)(x0 + w / 2 - 190), (float)(y0 + h - 30) });
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    apply();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", "Save settings and close");
                }
                ImGui::SameLine();

                auto configs_same = temp_cfg == config_file::instance();
                ImGui::PushStyleColor(ImGuiCol_Text, configs_same ? light_grey : light_blue);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, configs_same ? light_grey : light_blue);
                if (ImGui::Button("Apply", ImVec2(120, 0)))
                {
                    apply();
                }
                ImGui::PopStyleColor(2);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", "Save settings");
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", "Close window without saving any changes to the settings");
                }

                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
        }

        if (open_about_popup) 
        {
            ImGui::OpenPopup(about);                    
        }

        {
            float w = 590.f;
            float h = 300.f;
            float x0 = (window.width() - w) / 2.f;
            float y0 = (window.height() - h) / 2.f;
            ImGui::SetNextWindowPos({ x0, y0 });
            ImGui::SetNextWindowSize({ w, h });

            flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

            ImGui_ScopePushFont(window.get_font());
            ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

            if (ImGui::BeginPopupModal(about, nullptr, flags))
            {
                ImGui::Image((void*)(intptr_t)window.get_splash().get_gl_handle(), 
                             ImVec2(w - 30, 100), {0.20f, 0.38f}, {0.80f, 0.56f});

                auto realsense_pos = ImGui::GetCursorPos();
                ImGui::Text("Intel RealSense is a suite of depth-sensing and motion-tracking technologies.");

                ImGui::Text("librealsense is an open-source cross-platform SDK for working with RealSense devices.");

                ImGui::Text("Full source code is available at"); ImGui::SameLine();
                auto github_pos = ImGui::GetCursorPos();
                ImGui::Text("github.com/IntelRealSense/librealsense.");
                
                ImGui::Text("This software is distributed under the"); ImGui::SameLine();
                auto license_pos = ImGui::GetCursorPos();
                ImGui::Text("Apache License, Version 2.0.");

                ImGui::Text("RealSense is a registered trademark of Intel Corporation.");
                
                ImGui::Text("Copyright 2018 Intel Corporation.");

                ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, sensor_bg);
                ImGui::PushStyleColor(ImGuiCol_Text, light_blue);

                ImGui::SetCursorPos({ realsense_pos.x - 4, realsense_pos.y - 3 });

                hyperlink(window, "Intel RealSense", "https://realsense.intel.com/");

                ImGui::SetCursorPos({ github_pos.x - 4, github_pos.y - 3 });
                hyperlink(window, "github.com/IntelRealSense/librealsense", "https://github.com/IntelRealSense/librealsense/");

                ImGui::SetCursorPos({ license_pos.x - 4, license_pos.y - 3 });

                hyperlink(window, "Apache License, Version 2.0", "https://raw.githubusercontent.com/IntelRealSense/librealsense/master/LICENSE");

                ImGui::PopStyleColor(4);

        
                ImGui::SetCursorScreenPos({ (float)(x0 + w / 2 - 60), (float)(y0 + h - 30) });
                if (ImGui::Button("OK", ImVec2(120, 0))) ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(7);

        ImGui::GetWindowDrawList()->AddLine({ pos1.x, pos1.y + 10 }, { pos1.x,pos1.y + panel_y - 10 }, ImColor(light_grey));


        ImGui::SameLine();
        ImGui::SetCursorPosX(window.width() - panel_width - panel_y);

        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void viewer_model::update_3d_camera(
        ux_window& win,
        const rect& viewer_rect, bool force)
    {
        if (ImGuizmo::IsUsing()) return;

        mouse_info& mouse = win.get_mouse();
        auto now = std::chrono::high_resolution_clock::now();
        static auto view_clock = std::chrono::high_resolution_clock::now();
        auto sec_since_update = std::chrono::duration<float, std::milli>(now - view_clock).count() / 1000;
        view_clock = now;

        if (fixed_up)
            up = { 0.f, -1.f, 0.f };

        auto dir = target - pos;
        auto x_axis = cross(dir, up);
        auto step = sec_since_update * 0.3f;

        if (ImGui::IsKeyPressed('w') || ImGui::IsKeyPressed('W'))
        {
            pos = pos + dir * step;
            target = target + dir * step;
        }
        if (ImGui::IsKeyPressed('s') || ImGui::IsKeyPressed('S'))
        {
            pos = pos - dir * step;
            target = target - dir * step;
        }
        if (ImGui::IsKeyPressed('d') || ImGui::IsKeyPressed('D'))
        {
            pos = pos + x_axis * step;
            target = target + x_axis * step;
        }
        if (ImGui::IsKeyPressed('a') || ImGui::IsKeyPressed('A'))
        {
            pos = pos - x_axis * step;
            target = target - x_axis * step;
        }

        if (viewer_rect.contains(mouse.cursor) || force)
        {
            // Whenever the mouse reaches the end of the window
            // and jump back to the start, it will add to the overflow
            // counter, so by adding the overflow value
            // we essentially emulate an infinite display
            auto cx = mouse.cursor.x + overflow.x;
            auto px = mouse.prev_cursor.x + overflow.x;
            auto cy = mouse.cursor.y + overflow.y;
            auto py = mouse.prev_cursor.y + overflow.y;

            auto dragging = ImGui::IsKeyDown(GLFW_KEY_LEFT_CONTROL);

            // Limit how much user mouse can jump between frames
            // This can work poorly when the app FPS is really terrible (< 10)
            // but overall works resonably well
            const auto MAX_MOUSE_JUMP = 200;
            if (std::abs(cx - px) < MAX_MOUSE_JUMP && 
                std::abs(cy - py) < MAX_MOUSE_JUMP )
                arcball_camera_update(
                (float*)&pos, (float*)&target, (float*)&up, view,
                sec_since_update,
                0.2f, // zoom per tick
                -0.1f, // pan speed
                3.0f, // rotation multiplier
                static_cast<int>(viewer_rect.w), static_cast<int>(viewer_rect.h), // screen (window) size
                static_cast<int>(px), static_cast<int>(cx),
                static_cast<int>(py), static_cast<int>(cy),
                (ImGui::GetIO().MouseDown[2] || 
                (ImGui::GetIO().MouseDown[0] && dragging)) ? 1 : 0,
                (ImGui::GetIO().MouseDown[0] && !dragging) ? 1 : 0,
                mouse.mouse_wheel,
                0);

            // If we are pressing mouse button
            // inside the 3D viewport
            // we should remember that we
            // are in the middle of manipulation
            // and adjust when mouse leaves the area
            if (ImGui::GetIO().MouseDown[0] || 
                ImGui::GetIO().MouseDown[1] || 
                ImGui::GetIO().MouseDown[2])
            {
                manipulating = true;
            }
        }

        auto rect = viewer_rect;
        rect.w -= 10;

        // If we started manipulating the camera
        // and left the viewport
        if (manipulating && !rect.contains(mouse.cursor))
        {
            // If mouse is no longer pressed,
            // abort the manipulation
            if (!ImGui::GetIO().MouseDown[0] && 
                !ImGui::GetIO().MouseDown[1] && 
                !ImGui::GetIO().MouseDown[2])
            {
                manipulating = false;
                overflow = float2{ 0.f, 0.f };
            }
            else
            {
                // Wrap-around the mouse in X direction
                auto startx = (mouse.cursor.x - rect.x);
                if (startx < 0) 
                {
                    overflow.x -= rect.w;
                    startx += rect.w;
                }
                if (startx > rect.w) 
                {
                    overflow.x += rect.w;
                    startx -= rect.w;
                }
                startx += rect.x;

                // Wrap-around the mouse in Y direction
                auto starty = (mouse.cursor.y - rect.y);
                if (starty < 0) 
                {
                    overflow.y -= rect.h;
                    starty += rect.h;
                }
                if (starty > rect.h)
                {
                    overflow.y += rect.h;
                    starty -= rect.h;
                }
                starty += rect.y;

                // Set new cursor position
                glfwSetCursorPos(win, startx, starty);
            }
        }
        else overflow = float2{ 0.f, 0.f };

        mouse.prev_cursor = mouse.cursor;
    }

    void viewer_model::begin_stream(std::shared_ptr<subdevice_model> d, rs2::stream_profile p)
    {
        // Starting post processing filter rendering thread
        ppf.start();
        streams[p.unique_id()].begin_stream(d, p);
        ppf.frames_queue.emplace(p.unique_id(), rs2::frame_queue(5));
        find_or_create_series(p);
    }

    std::vector<frame> rs2::viewer_model::get_frames(frame frame)
    {
        std::vector<rs2::frame> res;

        if (auto set = frame.as<frameset>())
            for (auto&& f : set)
                res.push_back(f);

        else
            res.push_back(frame);

        return res;
    }

    std::shared_ptr<texture_buffer> viewer_model::upload_frame(frame&& f)
    {
        scoped_timer t("upload_frame");


        if (f.get_profile().stream_type() == RS2_STREAM_DEPTH)
            ppf.depth_stream_active = true;

        auto index = f.get_profile().unique_id();

        std::lock_guard<std::mutex> lock(streams_mutex);
        if (streams.find(streams_origin[index]) != streams.end())
            return streams[streams_origin[index]].upload_frame(std::move(f));
        else return nullptr;
    }

    void viewer_model::draw_viewport(const rect& viewer_rect, 
        ux_window& window, int devices, std::string& error_message)
    {
        scoped_timer t("draw_viewport");

        static bool first = true;
        if (first)
        {
            update_3d_camera(window, viewer_rect, true);
            first = false;
        }

        if (!is_3d_view)
        {
            render_2d_view(viewer_rect, window,
                static_cast<int>(get_output_height()), window.get_font(), window.get_large_font(),
                devices, window.get_mouse(), error_message);
        }
        else
        {
            if (paused)
                show_paused_icon(window.get_large_font(), static_cast<int>(panel_width + 15), static_cast<int>(panel_y + 15), 0);

            //show_3dviewer_header(window.get_font(), viewer_rect, paused, error_message);

            update_3d_camera(window, viewer_rect);

            rect window_size{ 0, 0, (float)window.width(), (float)window.height() };
            rect fb_size{ 0, 0, (float)window.framebuf_width(), (float)window.framebuf_height() };
            rect new_rect = viewer_rect.normalize(window_size).unnormalize(fb_size);

            float3 picked_xyz;
            auto picked = render_3d_view(new_rect, window, &picked_xyz);
            if (!picked) _last_no_pick_time = window.time();

            if (picked) _context_menu = true;

            ImGui::PushStyleColor(ImGuiCol_Text, black);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, almost_white_bg);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, light_blue);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
            
            if (_context_menu && ImGui::BeginPopupContextWindow())
            {
                int selected_tex_source = 0;
                std::vector<std::string> tex_sources_str;
                std::vector<int> tex_sources;
                int i = 0;
                for (auto&& s : streams)
                {
                    if (s.second.is_stream_visible() &&
                        (s.second.profile.stream_type() == RS2_STREAM_COLOR ||
                        s.second.profile.stream_type() == RS2_STREAM_INFRARED ||
                        s.second.profile.stream_type() == RS2_STREAM_DEPTH ||
                        s.second.profile.stream_type() == RS2_STREAM_FISHEYE))
                    {
                        // if (selected_tex_source_uid == -1 && selected_depth_source_uid != -1)
                        // {
                        //     if (streams_origin.find(s.second.profile.unique_id()) != streams_origin.end() &&
                        //         streams.find(streams_origin[s.second.profile.unique_id()]) != streams.end())
                        //     {
                        //         selected_tex_source_uid = streams_origin[s.second.profile.unique_id()];
                        //     }                         
                        // }
                        // if ((streams_origin.find(s.second.profile.unique_id()) != streams_origin.end() &&streams_origin[s.second.profile.unique_id()] == selected_tex_source_uid))
                        // {
                        //     selected_tex_source = i;
                        // }

                        // // The texture source shall always refer to the raw (original) streams
                        // tex_sources.push_back(streams_origin[s.second.profile.unique_id()]);

                        // auto dev_name = s.second.dev ? s.second.dev->dev.get_info(RS2_CAMERA_INFO_NAME) : "Unknown";
                        // std::string stream_name = rs2_stream_to_string(s.second.profile.stream_type());
                        // if (s.second.profile.stream_index())
                        //     stream_name += "_" + std::to_string(s.second.profile.stream_index());
                        // tex_sources_str.push_back(to_string() << dev_name << " " << stream_name);

                        // i++;
                    }
                }

                // Only allow to change texture if we have something to put it on:
                if (tex_sources_str.size())
                {
                    if (ImGui::BeginMenu("Texture Source "))
                    {
                        for (int i = 0; i < tex_sources.size(); i++)
                        {
                            // auto selected = selected_tex_source_uid == tex_sources[i];
                            // if (ImGui::MenuItem(tex_sources_str[i].c_str(), nullptr, &selected))
                            // {
                            //     selected_tex_source_uid = tex_sources[i];
                            // }
                        }

                        ImGui::EndMenu();
                    }
                }

                if (ImGui::BeginMenu("Render Style "))
                {
                    // if (ImGui::MenuItem("Triangle Mesh", nullptr, &render_quads))
                    // {
                    //     render_quads = true;
                    // }

                    // auto render_pc = !render_quads;

                    // if (ImGui::MenuItem("Point Cloud", nullptr, &render_pc))
                    // {
                    //     render_quads = false;
                    // }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Export "))
                {
                    if (ImGui::MenuItem("Stanford PLY"))
                    {
                        // if (auto ret = file_dialog_open(save_file, "Polygon File Format (PLY)\0*.ply\0", NULL, NULL))
                        // {
                        //     auto model = ppf.get_points();
                            
                        //     frame tex;
                        //     if (selected_tex_source_uid >= 0 && streams.find(selected_tex_source_uid) != streams.end())
                        //     {
                        //         tex = streams[selected_tex_source_uid].texture->get_last_frame(true);
                        //         if (tex) ppf.update_texture(tex);
                        //     }

                        //     std::string fname(ret);
                        //     if (!ends_with(to_lower(fname), ".ply")) fname += ".ply";
                        //     export_to_ply(fname.c_str(), not_model, last_points, last_texture->get_last_frame());
                        // }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndPopup();
            }
            else 
            {
                if (ImGui::IsMouseClicked(1)) 
                {
                    foreach_series([](series_3d& s) { s.selected = false; });
                }

                _context_menu = false;

                if (window.time() - _last_no_pick_time > 0.5)
                {
                    std::string tt = to_string() << std::fixed << std::setprecision(2) 
                        << picked_xyz.x << ", " << picked_xyz.y << ", " << picked_xyz.z << " meters";
                    ImGui::SetTooltip(tt.c_str());
                }
            }

            ImGui::PopStyleColor(4);
        }

        if (ImGui::IsKeyPressed(' '))
        {
            if (paused)
            {
                for (auto&& s : streams)
                {
                    if (s.second.dev)
                    {
                        s.second.dev->resume();
                        if (auto p = s.second.dev->dev.as<playback>())
                        {
                            p.resume();
                        }
                    }
                }
            }
            else
            {
                for (auto&& s : streams)
                {
                    if (s.second.dev)
                    {
                        s.second.dev->pause();
                        if (s.second.dev->dev.is<playback>())
                        {
                            auto p = s.second.dev->dev.as<playback>();
                            p.pause();
                        }
                    }
                }
            }
            paused = !paused;
        }
    }

    std::map<int, rect> viewer_model::get_interpolated_layout(const std::map<int, rect>& l)
    {
        using namespace std::chrono;
        auto now = high_resolution_clock::now();
        if (l != _layout) // detect layout change
        {
            _transition_start_time = now;
            _old_layout = _layout;
            _layout = l;
        }

        //if (_old_layout.size() == 0 && l.size() == 1) return l;

        auto diff = now - _transition_start_time;
        auto ms = duration_cast<milliseconds>(diff).count();
        auto t = smoothstep(static_cast<float>(ms), 0, 100);

        std::map<int, rect> results;
        for (auto&& kvp : l)
        {
            auto stream = kvp.first;
            if (_old_layout.find(stream) == _old_layout.end())
            {
                _old_layout[stream] = _layout[stream].center();
            }
            results[stream] = _old_layout[stream].lerp(t, _layout[stream]);
        }

        return results;
    }
}
