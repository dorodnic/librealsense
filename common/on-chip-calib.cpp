// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <glad/glad.h>
#include "on-chip-calib.h"

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>
#include <model-views.h>
#include <viewer.h>

namespace rs2
{
    enum auto_calib_ui_state
    {
        RS2_CALIB_STATE_INITIAL_PROMPT,
        RS2_CALIB_STATE_FAILED,
        RS2_CALIB_STATE_COMPLETE,
        RS2_CALIB_STATE_HEALTH_CHECK_IN_PROGRESS,
        RS2_CALIB_STATE_HEALTH_CHECK_DONE,
        RS2_CALIB_STATE_CALIB_IN_PROCESS,
        RS2_CALIB_STATE_CALIB_COMPLETE,
    };

    enum rs2_dsc_status : uint16_t
    {
        RS2_DSC_STATUS_SUCCESS = 0, /**< Self calibration succeeded*/
        RS2_DSC_STATUS_RESULT_NOT_READY = 1, /**< Self calibration result is not ready yet*/
        RS2_DSC_STATUS_FILL_FACTOR_TOO_LOW = 2, /**< There are too little textures in the scene*/
        RS2_DSC_STATUS_EDGE_TOO_CLOSE = 3, /**< Self calibration range is too small*/
        RS2_DSC_STATUS_NOT_CONVERGE = 4, /**< For tare calibration only*/
        RS2_DSC_STATUS_BURN_SUCCESS = 5,
        RS2_DSC_STATUS_BURN_ERROR = 6,
        RS2_DSC_STATUS_NO_DEPTH_AVERAGE = 7,
    };

#pragma pack(push, 1)
#pragma pack(1) 

#define MAX_STEP_COUNT 256

    struct DirectSearchCalibrationResult
    {
        uint32_t opcode;
        uint16_t status;      // DscStatus
        uint16_t stepCount;
        uint16_t stepSize; // 1/1000 of a pixel
        uint32_t pixelCountThreshold; // minimum number of pixels in
                                      // selected bin
        uint16_t minDepth;  // Depth range for FWHM
        uint16_t maxDepth;
        uint32_t rightPy;   // 1/1000000 of normalized unit
        float healthCheck;
        float rightRotation[9]; // Right rotation
        uint16_t results[0]; // 1/100 of a percent
    };
#pragma pack(pop)

    void on_chip_calib_manager::process_flow(std::function<void()> cleanup)
    {
        log(to_string() << "Starting calibration at speed " << _speed);

        bool is_3d = _viewer.is_3d_view;
        _viewer.is_3d_view = true;

        auto was_streaming = _sub->streaming;

        if (_sub->streaming) _sub->stop(_viewer);

        auto ui = _sub->ui;
        _sub->ui.selected_format_id.clear();
        _sub->ui.selected_format_id[RS2_STREAM_DEPTH] = 0;

        for (int i = 0; i < _sub->shared_fps_values.size(); i++)
        {
            if (_sub->shared_fps_values[i] == 90)
                _sub->ui.selected_shared_fps_id = i;
        }

        for (int i = 0; i < _sub->res_values.size(); i++)
        {
            auto kvp = _sub->res_values[i];
            if (kvp.first == 256 && kvp.second == 144)
                _sub->ui.selected_res_id = i;
        }

        auto sync = _viewer.synchronization_enable.load();

        _viewer.synchronization_enable = false;
        auto profiles = _sub->get_selected_profiles();

        if (!_model.dev_syncer)
            _model.dev_syncer = _viewer.syncer->create_syncer();

        _sub->play(profiles, _viewer, _model.dev_syncer);
        for (auto&& profile : profiles)
        {
            _viewer.begin_stream(_sub, profile);
        }

        bool frame_arrived = false;
        while (!frame_arrived)
        {
            for (auto&& stream : _viewer.streams)
            {
                if (std::find(profiles.begin(), profiles.end(), 
                        stream.second.original_profile) != profiles.end())
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    if (now - stream.second.last_frame < std::chrono::milliseconds(100))
                        frame_arrived = true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        for (int i = 0; i < 50 / _speed; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
        }

        uint8_t speed = 4 - _speed;

        std::vector<uint8_t> cmd =
        {
            0x14, 0x00, 0xab, 0xcd,
            0x80, 0x00, 0x00, 0x00,
            0x08, 0x00, 0x00, 0x00,
            speed, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };

        debug_protocol dp = _dev;

        auto res = dp.send_and_receive_raw_data(cmd);

        DirectSearchCalibrationResult result;
        memset(&result, 0, sizeof(DirectSearchCalibrationResult));

        int count = 0;
        bool done = false;
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            cmd =
            {
                0x14, 0x00, 0xab, 0xcd,
                0x80, 0x00, 0x00, 0x00,
                0x03, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
            };

            res = dp.send_and_receive_raw_data(cmd);
            int32_t code = *((int32_t*)res.data());

            if (res.size() >= sizeof(DirectSearchCalibrationResult))
            {
                result = *reinterpret_cast<DirectSearchCalibrationResult*>(res.data());
                done = result.status != RS2_DSC_STATUS_RESULT_NOT_READY;
            }

            _progress = count * (2 * _speed);

        } while (count++ < 200 && !done);

        if (!done)
        {
            throw std::runtime_error("Timeout!");
        }

        if (result.status != RS2_DSC_STATUS_SUCCESS)
        {
            throw std::runtime_error(to_string() << "Status = " << result.status);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        _health = abs(result.healthCheck);

        log(to_string() << "Calibration completed, health factor = " << _health);

        _viewer.is_3d_view = is_3d;

        _viewer.synchronization_enable = sync;

        _sub->stop(_viewer);

        while (frame_arrived && _viewer.streams.size())
        {
            for (auto&& stream : _viewer.streams)
            {
                if (std::find(profiles.begin(), profiles.end(),
                    stream.second.original_profile) != profiles.end())
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    if (now - stream.second.last_frame > std::chrono::milliseconds(200))
                        frame_arrived = false;
                }
                else frame_arrived = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        _sub->ui = ui;

        if (was_streaming)
        {
            auto profiles = _sub->get_selected_profiles();
            _sub->play(profiles, _viewer, _model.dev_syncer);
            for (auto&& profile : profiles)
            {
                _viewer.begin_stream(_sub, profile);
            }
        }

        _progress = 100;

        _done = true;
    }

    void autocalib_notification_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        using namespace std;
        using namespace chrono;

        const auto bar_width = width - 115;

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
        { float(x + width), float(y + 25) }, ImColor(shadow));

        if (update_state != RS2_CALIB_STATE_COMPLETE)
        {
            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT ||
                update_state == RS2_CALIB_STATE_HEALTH_CHECK_IN_PROGRESS ||
                update_state == RS2_CALIB_STATE_HEALTH_CHECK_DONE)
                ImGui::Text("Calibration Health-Check");

            if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS ||
                update_state == RS2_CALIB_STATE_CALIB_COMPLETE)
                ImGui::Text("On-Chip Calibration");

            ImGui::SetCursorScreenPos({ float(x + 9), float(y + 27) });

            ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1. - t));

            auto t = smoothstep(glfwGetTime() - score_update_time, 0.f, 1.f);
            float score = old_score * (1.f - t) + new_score * t;

            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
            {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

                ImGui::Text("The following device offers On-Chip Calibration:");
                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 47) });

                ImGui::PushStyleColor(ImGuiCol_Text, white);
                ImGui::Text(message.c_str());
                ImGui::PopStyleColor();

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 65) });
                ImGui::Text("Run quick calibration Health-Check? (~10 sec)");
            }
            else if (update_state == RS2_CALIB_STATE_HEALTH_CHECK_IN_PROGRESS)
            {
                enable_dismiss = false;
                ImGui::Text("Health-Check is underway, please wait...\nKeep the camera stationary pointing at a wall");
            }
            else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
            {
                enable_dismiss = false;
                ImGui::Text("Camera is being calibrated...\nKeep the camera stationary pointing at a wall");
            }
            else if (update_state == RS2_CALIB_STATE_HEALTH_CHECK_DONE ||
                     update_state == RS2_CALIB_STATE_CALIB_COMPLETE)
            {
                if (update_state == RS2_CALIB_STATE_HEALTH_CHECK_DONE)
                    ImGui::Text("Health-Check results:");
                else
                    ImGui::Text("Calibration completed:");

                std::vector<ImVec2> points, cover_points, redpoints, yellowpoints, greenpoints, arrow;
                const int SEGMENTS = 60;
                const float R = 230.f;
                for (int i = 0; i < SEGMENTS; i++)
                {
                    auto t = i / (float)SEGMENTS;
                    auto angle = -M_PI / 2.f - (M_PI / 6.f) * (1. - t) + (M_PI / 6.f) * t;
                    float x1 = cosf(angle) * R + x + width / 2;
                    float y1 = sinf(angle) * R + y + R + 60;
                    points.push_back({ x1, y1 });

                    if (i > 0 && i < SEGMENTS - 1)
                        cover_points.push_back({ x1, y1 + 22 - (sinf(angle) + 1.f) * R * 0.15f });

                    if (i <= SEGMENTS / 3 - 1)
                        redpoints.push_back({ x1 + 4, y1 + 2 });

                    if (i >= SEGMENTS / 3 + 1 && i <= 2 * (SEGMENTS / 3) - 1)
                        yellowpoints.push_back({ x1, y1 + 2 });

                    if (i >= 2 * (SEGMENTS / 3) + 1)
                        greenpoints.push_back({ x1 - 4, y1 + 2 });
                }
                auto last = points[points.size() - 1];
                points.push_back({ last.x - 5, last.y + 15 });
                auto first = points.front();
                points.insert(points.begin(), { first.x + 5, first.y + 15 });

                last = redpoints[redpoints.size() - 1];
                redpoints.push_back({ last.x + 5, last.y + 35 });
                first = redpoints.front();
                redpoints.push_back({ first.x + 5, first.y + 15 });

                last = yellowpoints[yellowpoints.size() - 1];
                yellowpoints.push_back({ last.x - 4, last.y + 35 });
                first = yellowpoints.front();
                yellowpoints.push_back({ first.x + 4, first.y + 35 });

                last = greenpoints[greenpoints.size() - 1];
                greenpoints.push_back({ last.x - 5, last.y + 15 });
                first = greenpoints.front();
                greenpoints.push_back({ first.x - 5, first.y + 35 });

                ImGui::GetWindowDrawList()->AddConvexPolyFilled(points.data(),
                    points.size(), ImColor(dark_window_background), true);

                ImGui::GetWindowDrawList()->AddConvexPolyFilled(redpoints.data(),
                    redpoints.size(), ImColor(alpha(redish, (1.f - smoothstep(score, 0.2f, 0.4f)) * 0.2f + 0.15f)), true);

                ImGui::GetWindowDrawList()->AddConvexPolyFilled(yellowpoints.data(),
                    yellowpoints.size(), ImColor(alpha(yellowish, smoothstep(score, 0.2f, 0.4f) * (1.f - smoothstep(score, 0.6f, 0.8f)) * 0.2f + 0.15f)), true);

                ImGui::GetWindowDrawList()->AddConvexPolyFilled(greenpoints.data(),
                    greenpoints.size(), ImColor(alpha(green, smoothstep(score, 0.6f, 0.8f) * 0.2f + 0.25f)), true);

                ImGui::GetWindowDrawList()->AddPolyline(points.data(),
                    points.size(), ImColor(alpha(light_blue, 0.9f)), false, 2.f, true);


                auto angle = -M_PI / 2.f - (M_PI / 7.f) * (1. - score) + (M_PI / 7.2f) * score;
                float x1 = cosf(angle) * R + x + width / 2;
                float y1 = sinf(angle) * R + y + R + 60;
                arrow.push_back({ x1, y1 + 8 });
                arrow.push_back({ x1 - 5 + 7 * (1.f - score) - 7 * score, y1 + 22 });
                arrow.push_back({ x1 + 5 + 7 * (1.f - score) - 7 * score, y1 + 22 });

                ImGui::GetWindowDrawList()->AddConvexPolyFilled(arrow.data(),
                    arrow.size(), ImColor(regular_blue), true);
                ImGui::GetWindowDrawList()->AddPolyline(arrow.data(),
                    arrow.size(), ImColor(light_grey), true, 3.f, true);

                ImGui::GetWindowDrawList()->AddConvexPolyFilled(cover_points.data(),
                    cover_points.size(), ImColor(sensor_bg), true);

                ImGui::GetWindowDrawList()->AddPolyline(cover_points.data(),
                    cover_points.size(), ImColor(alpha(light_blue, 0.9f)), false, 1.5f, true);

                for (int i = 15; i >= 0; i -= 2)
                {
                    auto copy = arrow;
                    copy[0].y += i; copy[1].y -= i; copy[2].y -= i;
                    ImGui::GetWindowDrawList()->AddPolyline(copy.data(),
                        copy.size(), ImColor(alpha(light_blue, 0.02f)), true, i, true);
                }

                if (update_state == RS2_CALIB_STATE_HEALTH_CHECK_DONE)
                {
                    if (score < 0.5f)
                    {
                        ImGui::SetCursorScreenPos({ float(x + 77), float(y + 102) });
                        ImGui::PushStyleColor(ImGuiCol_Text, white);
                        ImGui::Text("Calibration Recommended");
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::SetCursorScreenPos({ float(x + 107), float(y + 102) });
                        ImGui::Text("Calibration is OK");
                    }
                }
                else
                {
                    auto health = get_manager().get_health();
                    int diff = 100 * (abs(health - health_check_result) / health_check_result);

                    if (health <= health_check_result)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, green);
                        ImGui::SetCursorScreenPos({ float(x + 110), float(y + 102) });
                        std::string label = to_string() << "Improved by " << diff << "%%";
                        ImGui::Text(label.c_str());
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, redish);
                        ImGui::SetCursorScreenPos({ float(x + 100), float(y + 102) });
                        std::string label = to_string() << "Deteriorated by " << diff << "%%";
                        ImGui::Text(label.c_str());
                    }

                    ImGui::PopStyleColor();
                }

                if (update_state == RS2_CALIB_STATE_HEALTH_CHECK_DONE)
                {
                    auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

                    if (new_score < 0.5f)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                    }
                    
                    std::string button_name = to_string() << "Calibrate" << "##calibrate" << index;

                    ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
                    if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
                    {
                        update_manager->reset();
                        update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                        last_progress_time = system_clock::now();
                        enable_dismiss = false;
                        get_manager().set_speed(1);
                        update_manager->start(shared_from_this());
                    }

                    if (new_score < 0.5f) ImGui::PopStyleColor(2);

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", "Point the camera towards an object or a wall");
                    }

                    ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

                    if (new_score < 0.5f)
                    {
                        enable_dismiss = false;
                        string id = to_string() << "Ignore" << "##" << index;
                        if (ImGui::Button(id.c_str(), { 100, 20 }))
                        {
                            update_manager->reset();
                            update_state = RS2_CALIB_STATE_INITIAL_PROMPT;
                            enable_dismiss = true;
                        }
                    }
                    else enable_dismiss = true;
                }
                else
                {
                    auto health = get_manager().get_health();
                    int diff = 100 * (abs(health - health_check_result) / health_check_result);

                    auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

                    if (health <= health_check_result)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                    }

                    std::string button_name = to_string() << "Keep" << "##keep" << index;

                    ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
                    if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
                    {
                        update_state = RS2_CALIB_STATE_COMPLETE;
                        pinned = false;
                        last_progress_time = last_interacted = system_clock::now();
                    }

                    if (health <= health_check_result) ImGui::PopStyleColor(2);

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", "New calibration values will be saved in device memory");
                    }

                    ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

                    if (health > health_check_result)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                    }

                    string id = to_string() << "Discard" << "##" << index;
                    if (ImGui::Button(id.c_str(), { 100, 20 }))
                    {
                        update_manager->reset();
                        update_state = RS2_CALIB_STATE_HEALTH_CHECK_DONE;
                        old_score = new_score;
                        new_score = on_chip_calib_manager::get_score(health_check_result);
                        score_update_time = glfwGetTime();
                    }

                    if (health > health_check_result) ImGui::PopStyleColor(2);

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", "New calibration values will be discarded");
                    }

                }
            }

            ImGui::PopStyleColor();
        }
        else
        {
            //ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_blue, 1.f - t));
            ImGui::Text("Calibration Complete");
            //ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos({ float(x + 10), float(y + 35) });
            ImGui::PushFont(win.get_large_font());
            std::string txt = to_string() << textual_icons::throphy;
            ImGui::Text("%s", txt.c_str());
            ImGui::PopFont();

            ImGui::SetCursorScreenPos({ float(x + 40), float(y + 35) });
            ImGui::Text("Camera Calibration Applied Successfully");
        }

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

        if (update_manager)
        {
            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
            {
                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                std::string button_name = to_string() << "Health-Check" << "##health_check" << index;

                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }) || update_manager->started())
                {
                    if (!update_manager->started()) update_manager->start(shared_from_this());

                    update_state = RS2_CALIB_STATE_HEALTH_CHECK_IN_PROGRESS;
                    enable_dismiss = false;
                    last_progress_time = system_clock::now();
                }
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", "Keep the camera pointing at an object or a wall");
                }
            }
            else if (update_state == RS2_CALIB_STATE_HEALTH_CHECK_IN_PROGRESS ||
                     update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
            {
                if (update_manager->done())
                {
                    if (update_state == RS2_CALIB_STATE_HEALTH_CHECK_IN_PROGRESS)
                        health_check_result = get_manager().get_health();

                    update_state++;
                    old_score = new_score;
                    new_score = on_chip_calib_manager::get_score(get_manager().get_health());
                    score_update_time = glfwGetTime();
                    //pinned = false;
                    //last_progress_time = last_interacted = system_clock::now();
                }

                if (!expanded)
                {
                    if (update_manager->failed())
                    {
                        update_manager->check_error(error_message);
                        update_state = RS2_CALIB_STATE_FAILED;
                        pinned = false;
                        dismissed = true;
                    }

                    draw_progress_bar(win, bar_width);

                    ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

                    string id = to_string() << "Expand" << "##" << index;
                    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    if (ImGui::Button(id.c_str(), { 100, 20 }))
                    {
                        expanded = true;
                    }

                    ImGui::PopStyleColor();
                }
            }
        }
    }

    void autocalib_notification_model::draw_expanded(ux_window& win, std::string& error_message)
    {
        if (update_manager->started() && update_state == RS2_CALIB_STATE_INITIAL_PROMPT) 
            update_state = RS2_CALIB_STATE_HEALTH_CHECK_IN_PROGRESS;

        auto flags = ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, 0 });
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(500, 100));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

        std::string title = "On-Chip Calibration";
        if (update_manager->failed()) title += " Failed";

        ImGui::OpenPopup(title.c_str());
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, flags))
        {
            ImGui::SetCursorPosX(200);
            std::string progress_str = to_string() << "Progress: " << update_manager->get_progress() << "%";
            ImGui::Text("%s", progress_str.c_str());

            ImGui::SetCursorPosX(5);

            draw_progress_bar(win, 490);

            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
            auto s = update_manager->get_log();
            ImGui::InputTextMultiline("##autocalib_log", const_cast<char*>(s.c_str()),
                s.size() + 1, { 490,100 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(190);
            if (visible || update_manager->done() || update_manager->failed())
            {
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    if (update_manager->failed())
                    {
                        update_state = RS2_CALIB_STATE_FAILED;
                        pinned = false;
                        dismissed = true;
                    }
                    expanded = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
                ImGui::PushStyleColor(ImGuiCol_Text, transparent);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, transparent);
                ImGui::Button("OK", ImVec2(120, 0));
                ImGui::PopStyleColor(5);
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);

        error_message = "";
    }

    int autocalib_notification_model::calc_height()
    {
        if (update_state == RS2_CALIB_STATE_COMPLETE) return 65;
        else if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT) return 120;
        else if (update_state == RS2_CALIB_STATE_HEALTH_CHECK_DONE
            || update_state == RS2_CALIB_STATE_CALIB_COMPLETE) return 160;
        else return 100;
    }

    void autocalib_notification_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);

        ImGui::PopStyleColor(1);

        ImVec4 c;

        if (update_state == RS2_CALIB_STATE_COMPLETE)
        {
            c = alpha(saturate(light_blue, 0.7f), 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
        else
        {
            c = alpha(sensor_bg, 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
    }

    autocalib_notification_model::autocalib_notification_model(std::string name,
        std::shared_ptr<on_chip_calib_manager> manager, bool exp)
        : process_notification_model(manager)
    {
        enable_expand = false;
        enable_dismiss = true;
        expanded = exp;
        if (expanded) visible = false;

        message = name;
        this->severity = RS2_LOG_SEVERITY_INFO;
        this->category = RS2_NOTIFICATION_CATEGORY_HARDWARE_EVENT;

        pinned = true;
    }
}
