// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <thread>
#include <algorithm>
#include <regex>
#include <cmath>

#include <opengl3.h>

#include "notifications.h"
#include <imgui_internal.h>

#include "os.h"

namespace rs2
{
    notification_data::notification_data(std::string description,
        double timestamp,
        rs2_log_severity severity,
        rs2_notification_category category)
        : _description(description),
        _timestamp(timestamp),
        _severity(severity),
        _category(category) {}


    rs2_notification_category notification_data::get_category() const
    {
        return _category;
    }

    std::string notification_data::get_description() const
    {
        return _description;
    }


    double notification_data::get_timestamp() const
    {
        return _timestamp;
    }


    rs2_log_severity notification_data::get_severity() const
    {
        return _severity;
    }

    notification_model::notification_model()
    {
        message = "";
    }

    notification_model::notification_model(const notification_data& n)
    {
        message = n.get_description();
        timestamp = n.get_timestamp();
        severity = n.get_severity();
        created_time = std::chrono::high_resolution_clock::now();
        last_interacted = std::chrono::high_resolution_clock::now() - std::chrono::milliseconds(500);
        category = n._category;
    }

    double notification_model::get_age_in_ms() const
    {
        using namespace std;
        using namespace chrono;
        auto interacted = duration<double, milli>(last_interacted - created_time).count();
        return duration<double, milli>(high_resolution_clock::now() - created_time).count() - interacted;
    }

    bool notification_model::interacted() const
    {
        using namespace std;
        using namespace chrono;
        return duration<double, milli>(high_resolution_clock::now() - last_interacted).count() < 100;
    }

    // Pops the 6 colors that were pushed in set_color_scheme
    void notification_model::unset_color_scheme() const
    {
        ImGui::PopStyleColor(6);
    }

    /* Sets color scheme for notifications, must be used with unset_color_scheme to pop all colors in the end
       Parameter t indicates the transparency of the nofication interface */
    void notification_model::set_color_scheme(float t) const
    {
        ImGui::PushStyleColor(ImGuiCol_CloseButton, { 0, 0, 0, 0 });
        ImGui::PushStyleColor(ImGuiCol_CloseButtonActive, { 0, 0, 0, 0 });
        if (category == RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED)
        {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, { 33/255.f, 40/255.f, 46/255.f, 1 - t });
            ImGui::PushStyleColor(ImGuiCol_TitleBg, { 62 / 255.f, 77 / 255.f, 89 / 255.f, 1 - t });
            ImGui::PushStyleColor(ImGuiCol_TitleBgActive, { 62 / 255.f, 77 / 255.f, 89 / 255.f, 1 - t });
            ImGui::PushStyleColor(ImGuiCol_CloseButtonHovered, { 62 / 255.f + 0.1f, 77 / 255.f + 0.1f, 89 / 255.f + 0.1f, 1 - t });
        }
        else
        {
            if (severity == RS2_LOG_SEVERITY_ERROR ||
                severity == RS2_LOG_SEVERITY_WARN || interacted())
            {
                ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0.3f, 0.f, 0.f, 1 - t });
                ImGui::PushStyleColor(ImGuiCol_TitleBg, { 0.5f, 0.2f, 0.2f, 1 - t });
                ImGui::PushStyleColor(ImGuiCol_TitleBgActive, { 0.6f, 0.2f, 0.2f, 1 - t });
                ImGui::PushStyleColor(ImGuiCol_CloseButtonHovered, { 0.5f + 0.1f, 0.2f + 0.1f, 0.2f + 0.1f, 1 - t });
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0.3f, 0.3f, 0.3f, 1 - t });
                ImGui::PushStyleColor(ImGuiCol_TitleBg, { 0.4f, 0.4f, 0.4f, 1 - t });
                ImGui::PushStyleColor(ImGuiCol_TitleBgActive, { 0.6f, 0.6f, 0.6f, 1 - t });
                ImGui::PushStyleColor(ImGuiCol_CloseButtonHovered, { 0.6f + 0.1f, 0.6f + 0.1f, 0.6f + 0.1f, 1 - t });
            }
        }
    }

    const int notification_model::get_max_lifetime_ms() const
    {
        if (category == RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED)
        {
            return 30000;
        }
        return 10000;
    }

    void notification_model::draw(ux_window& win, int w, int y, notification_model& selected)
    {
        auto ms = get_age_in_ms() / get_max_lifetime_ms();
        auto t = smoothstep(static_cast<float>(ms), 0.7f, 1.0f);

        set_color_scheme(t);
        ImGui::PushStyleColor(ImGuiCol_Text, { 0.8, 0.8, 0.8, 1 - t });

        auto lines = static_cast<int>(std::count(message.begin(), message.end(), '\n') + 1);
        height = lines * 30 + 20;

        auto c = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        c.w = smoothstep(static_cast<float>(get_age_in_ms() / 1000.f), 0.0f, 1.0f);
        c.w = std::min(c.w, 1.f - t);

        ImGui::GetWindowDrawList()->AddRectFilled({ float(w - 330), float(y) },
            { float(w - 5), float(y + height) }, ImColor(c));
        ImGui::SetCursorScreenPos({ float(w - 330), float(y) });

        bool opened = true;
        std::string label;

        if (category == RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED)
        {
            label = to_string() << "Firmware update recommended" << "##" << index;
        }
        else
        {
            label = to_string() << "Hardware Notification #" << index;
        }

        //ImGui::Begin(label.c_str(), &opened, flags);

        // if (ImGui::IsWindowHovered())
        // {
        //     last_interacted = std::chrono::system_clock::now();
        //     ImGui::FocusWindow(ImGui::GetCurrentWindow());
        // }

        if (!opened)
            to_close = true;

        if (category == RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED)
        {
            std::regex version_regex("([0-9]+.[0-9]+.[0-9]+.[0-9]+\n)");
            std::smatch sm;
            std::regex_search(message, sm, version_regex);
            std::string message_prefix = sm.prefix();
            std::string curr_version = sm.str();
            std::string message_suffix = sm.suffix();
            ImGui::Text("%s", message_prefix.c_str());
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, { (float)255 / 255, (float)46 / 255, (float)54 / 255, 1 - t });
            ImGui::Text("%s", curr_version.c_str());
            ImGui::PopStyleColor();
            ImGui::Text("%s", message_suffix.c_str());

            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 1, 1, 1, 1 - t });
            ImGui::PushStyleColor(ImGuiCol_Button, { 62 / 255.f, 77 / 255.f, 89 / 255.f, 1 - t });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 62 / 255.f + 0.1f, 77 / 255.f + 0.1f, 89 / 255.f + 0.1f, 1 - t });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 62 / 255.f - 0.1f, 77 / 255.f - 0.1f, 89 / 255.f - 0.1f, 1 - t });
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2);
            std::string button_name = to_string() << "Download update" << "##" << index;
            ImGui::Indent(80);
            if (ImGui::Button(button_name.c_str(), { 130, 30 }))
            {
                open_url(recommended_fw_url);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", "Internet connection required");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
        else
        {
            ImGui::Text("%s", message.c_str());
        }

        if (lines == 1)
            ImGui::SameLine();

        if (category != RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED)
        {
            //ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
            ImGui::PushFont(win.get_large_font());
            
            ImGui::PushStyleColor(ImGuiCol_Button, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            if (ImGui::Button(textual_icons::dotdotdot))
            {
                selected = *this;
            }

            if (ImGui::IsItemHovered())
                win.link_hovered();

            if (interacted())
            {
                ImGui::SameLine();
                if (ImGui::Button(textual_icons::see_less))
                {

                }

                if (ImGui::IsItemHovered())
                    win.link_hovered();
            }

            ImGui::PopStyleColor(5);

            ImGui::PopFont();

            //ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor(1);
        unset_color_scheme();
    }

    void notifications_model::add_notification(const notification_data& n)
    {
        {
            using namespace std;
            using namespace chrono;
            lock_guard<mutex> lock(m); // need to protect the pending_notifications queue because the insertion of notifications
                                        // done from the notifications callback and proccesing and removing of old notifications done from the main thread

            notification_model m(n);
            m.index = index++;
            m.timestamp = duration<double, milli>(system_clock::now().time_since_epoch()).count();
            pending_notifications.push_back(m);

            if (pending_notifications.size() > (size_t)MAX_SIZE)
            {
                pending_notifications.erase(pending_notifications.begin());

                for (int i = pending_notifications.size() - 2; i >= 0; i--)
                {
                    if (pending_notifications[i].interacted())
                        std::swap(pending_notifications[i], pending_notifications[i+1]);
                }
            }
        }

        add_log(n.get_description());
    }

    void notifications_model::draw(ux_window& win, int w, int h)
    {
        ImGui::PushFont(win.get_font());
        std::lock_guard<std::mutex> lock(m);
        if (pending_notifications.size() > 0)
        {
            auto int_id = 0;
            for (int i = 0; i < pending_notifications.size(); i++)
            {
                if (pending_notifications[i].interacted())
                {
                    int_id = i;
                    break;
                }
            }

            auto old_size = pending_notifications.size();
            // loop over all notifications, remove "old" ones
            pending_notifications.erase(std::remove_if(std::begin(pending_notifications),
                std::end(pending_notifications),
                [&](notification_model& n)
            {
                return (n.get_age_in_ms() > n.get_max_lifetime_ms() || n.to_close);
            }), end(pending_notifications));

            for (int i = 0; i < pending_notifications.size(); i++)
            {
                if (pending_notifications[i].interacted())
                {
                    std::swap(pending_notifications[int_id], pending_notifications[i]);
                    break;
                }
            }

            int idx = 0;
            auto height = 55;
            for (auto& noti : pending_notifications)
            {
                noti.draw(win, w, height, selected);
                height += noti.height + 4;
                idx++;
            }
        }

        auto flags = ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, 0 });
        //ImGui::Begin("Notification parent window", nullptr, flags);

        selected.set_color_scheme(0.f);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

        if (selected.message != "")
            ImGui::OpenPopup("Notification from Hardware");
        if (ImGui::BeginPopupModal("Notification from Hardware", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Received the following notification:");
            std::stringstream ss;
            ss << "Timestamp: "
                << std::fixed << selected.timestamp
                << "\nSeverity: " << selected.severity
                << "\nDescription: " << selected.message;
            auto s = ss.str();
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
            ImGui::InputTextMultiline("notification", const_cast<char*>(s.c_str()),
                s.size() + 1, { 500,100 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();

            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                selected.message = "";
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        selected.unset_color_scheme();
        //ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
}
