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
        rs2_log_severity severity,
        rs2_notification_category category)
        : _description(description),
          _severity(severity),
          _category(category) 
    {
        _timestamp = (double)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }

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
        custom_action = []{};
        last_x = 500000;
        last_y = 200;
        message = "";
        last_moved = std::chrono::system_clock::now();
    }

    notification_model::notification_model(const notification_data& n)
        : notification_model()
    {
        message = n.get_description();
        timestamp = n.get_timestamp();
        severity = n.get_severity();
        created_time = std::chrono::system_clock::now();
        last_interacted = std::chrono::system_clock::now() - std::chrono::milliseconds(500);
        category = n.get_category();
    }

    double notification_model::get_age_in_ms(bool total) const
    {
        using namespace std;
        using namespace chrono;
        auto interacted = duration<double, milli>(last_interacted - created_time).count();
        return duration<double, milli>(system_clock::now() - created_time).count() - (total ? 0.0 : interacted);
    }

    bool notification_model::interacted() const
    {
        using namespace std;
        using namespace chrono;
        return duration<double, milli>(system_clock::now() - last_interacted).count() < 100;
    }

    // Pops the N colors that were pushed in set_color_scheme
    void notification_model::unset_color_scheme() const
    {
        ImGui::PopStyleColor(6);
    }

    ImVec4 saturate(const ImVec4& a, float f)
    {
        return { f * a.x, f * a.y, f * a.z, a.w };
    }

    ImVec4 alpha(const ImVec4& v, float a)
    {
        return { v.x, v.y, v.z, a };
    }

    /* Sets color scheme for notifications, must be used with unset_color_scheme to pop all colors in the end
       Parameter t indicates the transparency of the nofication interface */
    void notification_model::set_color_scheme(float t) const
    {
        ImVec4 c;

        if (category == RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED)
        {
            c = alpha(sensor_bg, 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
        else
        {
            if (severity == RS2_LOG_SEVERITY_ERROR ||
                severity == RS2_LOG_SEVERITY_WARN)
            {
                c = alpha(dark_red, 1 - t);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
            }
            else
            {
                c = alpha(saturate(grey, 0.7f), 1 - t);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
            }
        }

        ImGui::PushStyleColor(ImGuiCol_Button, saturate(c, 1.3));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, saturate(c, 0.9));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(c, 1.5));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        c = alpha(white, 1 - t);
        ImGui::PushStyleColor(ImGuiCol_Text, c);
    }

    const int notification_model::get_max_lifetime_ms() const
    {
        return 10000;
    }

    std::function<void()> notification_model::draw(ux_window& win, int w, int y, notification_model& selected)
    {
        std::function<void()> follow_up = []{};

        auto stack = std::min(count, max_stack);
        auto x = w - width - 10;

        if (dismissed)
        {
            x = w + width;
        }

        using namespace std;
        using namespace chrono;

        if (!animating && (fabs(x - last_x) > 1.f || fabs(y - last_y) > 1.f))
        {
            if (last_x > 100000)
            {
                last_x = x + 500;
                last_y = y;
            }
            last_moved = system_clock::now();
            animating = true;
        }

        auto elapsed = duration<double, milli>(system_clock::now() - last_moved).count();
        auto s = smoothstep(static_cast<float>(elapsed / 250.f), 0.0f, 1.0f);

        if (s < 1.f)
        {
            x = s * x + (1 - s) * last_x;
            y = s * y + (1 - s) * last_y;
        }
        else
        {
            last_x = x; last_y = y;
            animating = false;
            if (dismissed) to_close = true;
        }

        auto ms = get_age_in_ms() / get_max_lifetime_ms();
        auto t = smoothstep(static_cast<float>(ms), 0.8f, 1.f);
        if (pinned) t = 0.f;

        set_color_scheme(t);

        auto lines = static_cast<int>(std::count(message.begin(), message.end(), '\n') + 1);
        height = lines * 30 + 5;

        auto c = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        c.w = smoothstep(static_cast<float>(get_age_in_ms(true) / 200.f), 0.0f, 1.0f);
        c.w = std::min(c.w, 1.f - t);

        for (int i = stack - 1; i >= 0; i--)
        {
            auto ccopy = alpha(c, (0.9f * c.w) / (i + 1));

            ImVec4 shadow { 0.1f, 0.1f, 0.1f, 0.1f };

            ImGui::GetWindowDrawList()->AddRectFilled({ float(x+2 + i * stack_offset), float(y+2 + i * stack_offset) },
                { float(x+2 + width + i * stack_offset), float(y+2 + height + i * stack_offset) }, ImColor(shadow));

            ImGui::GetWindowDrawList()->AddRectFilled({ float(x + i  * stack_offset), float(y + i * stack_offset) },
                { float(x + width + i * stack_offset), float(y + height + i * stack_offset) }, ImColor(ccopy));

            ImGui::GetWindowDrawList()->AddRect({ float(x + i * stack_offset), float(y + i * stack_offset) },
                { float(x + width + i * stack_offset), float(y + height + i * stack_offset) }, ImColor(saturate(ccopy, 0.6f)));
        }
        
        ImGui::SetCursorScreenPos({ float(x), float(y) });

        if (enable_click)
        {
            std::string button_name = to_string() << "##" << index;

            ImGui::PushStyleColor(ImGuiCol_Button, transparent);

            if (ImGui::Button(button_name.c_str(), { (float)width, (float)height }))
            {
                follow_up = custom_action;
                dismissed = true;
            }
            if (ImGui::IsItemHovered())
                win.link_hovered();

            ImGui::PopStyleColor();
        }

        if (count > 1)
        {
            std::string count_str = to_string() << "x " << count;
            ImGui::SetCursorScreenPos({ float(x + width - 22 - count_str.size() * 5), float(y + 5) });
            ImGui::Text("%s", count_str.c_str());
        }

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + 5) });

        if (category == RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED)
        {
            ImVec4 shadow { 1.f, 1.f, 1.f, 0.1f };
            ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
                { float(x + width), float(y + 25) }, ImColor(shadow));

            ImGui::Text("Firmware Update Recommended!");

            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);

            ImGui::SetCursorScreenPos({ float(x + 5), float(y + 27) });

            std::regex version_regex("([0-9]+.[0-9]+.[0-9]+.[0-9]+\n)");
            std::smatch sm;
            std::regex_search(message, sm, version_regex);
            std::string message_prefix = sm.prefix();
            std::string curr_version = sm.str();
            std::string message_suffix = sm.suffix();
            ImGui::Text("%s", message_prefix.c_str());
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_red, 1 - t));
            ImGui::Text("%s", curr_version.c_str());
            ImGui::PopStyleColor();
            
            ImGui::SetCursorScreenPos({ float(x + 5), float(y + 47) });
            ImGui::Text("%s", message_suffix.c_str());

            ImGui::PopStyleColor();

            std::string button_name = to_string() << "Learn More..." << "##" << index;

            ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
            if (ImGui::Button(button_name.c_str(), { 130, 20 }))
            {
                open_url(recommended_fw_url);
            }
            if (ImGui::IsItemHovered())
            {
                win.link_hovered();
                ImGui::SetTooltip("%s", "Internet connection required");
            }
        }
        else
        {
            ImGui::PushTextWrapPos(x + width - 100);
            ImGui::Text("%s", message.c_str());
            ImGui::PopTextWrapPos();

            if (ImGui::IsItemHovered())
            {
                last_interacted = system_clock::now();
            }
        }

        if (enable_expand)
        {
            ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

            ImGui::PushFont(win.get_large_font());
            
            ImGui::PushStyleColor(ImGuiCol_Button, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
            string id = to_string() << textual_icons::dotdotdot << "##" << index;
            if (ImGui::Button(id.c_str()))
            {
                selected = *this;
            }

            if (ImGui::IsItemHovered())
                win.link_hovered();

            ImGui::PopStyleColor(3);
            ImGui::PopFont();
        }

        if (enable_dismiss)
        {
            ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

            string id = to_string() << "Dismiss" << "##" << index;
            if (ImGui::Button(id.c_str(), { 100, 20 }))
            {
                dismissed = true;
            }
        }

        unset_color_scheme();

        return follow_up;
    }

    int notifications_model::add_notification(const notification_data& n)
    {
        return add_notification(n, []{}, false);
    }

    int notifications_model::add_notification(const notification_data& n,
        std::function<void()> custom_action, bool use_custom_action)
    {
        int result = 0;

        {
            using namespace std;
            using namespace chrono;
            lock_guard<mutex> lock(m); // need to protect the pending_notifications queue because the insertion of notifications
                                       // done from the notifications callback and proccesing and removing of old notifications done from the main thread

            for (auto&& nm : pending_notifications)
            {
                if (nm.category == n.get_category() && nm.message == n.get_description())
                {
                    nm.last_interacted = std::chrono::system_clock::now();
                    nm.count++;
                    return nm.index;
                }
            }

            notification_model m(n);
            m.index = index++;
            result = m.index;
            m.timestamp = duration<double, milli>(system_clock::now().time_since_epoch()).count();

            if (n.get_category() == RS2_NOTIFICATION_CATEGORY_FIRMWARE_UPDATE_RECOMMENDED)
            {
                m.pinned = true;
                m.enable_expand = false;
            }

            if (use_custom_action)
            {
                m.custom_action = custom_action;
                m.enable_click = true;
                m.enable_expand = false;
                m.enable_dismiss = false;
            }
            
            pending_notifications.push_back(m);

            if (pending_notifications.size() > (size_t)MAX_SIZE)
            {
                auto it = pending_notifications.begin();
                while (it != pending_notifications.end() && it->pinned) it++;

                if (it != pending_notifications.end())
                    pending_notifications.erase(it);
            }
        }

        add_log(n.get_description());
        return result;
    }

    void notifications_model::draw(ux_window& win, int w, int h)
    {
        ImGui::PushFont(win.get_font());

        std::vector<std::function<void()>> follow_up;

        {
            std::lock_guard<std::mutex> lock(m);
            if (pending_notifications.size() > 0)
            {
                // loop over all notifications, remove "old" ones
                pending_notifications.erase(std::remove_if(std::begin(pending_notifications),
                    std::end(pending_notifications),
                    [&](notification_model& n)
                {
                    return ((n.get_age_in_ms() > n.get_max_lifetime_ms() && !n.pinned) || n.to_close);
                }), end(pending_notifications));

                int idx = 0;
                auto height = 60;
                for (auto& noti : pending_notifications)
                {
                    follow_up.push_back(noti.draw(win, w, height, selected));
                    height += noti.height + 4 + 
                        std::min(noti.count, noti.max_stack) * noti.stack_offset;
                    idx++;
                }
            }

            auto flags = ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoTitleBar;

            ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, 0 });
            //ImGui::Begin("Notification parent window", nullptr, flags);

            //selected.set_color_scheme(0.f);
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
            //selected.unset_color_scheme();
            //ImGui::End();

            ImGui::PopStyleColor();
        }

        for (auto& action : follow_up)
            try
            {
                action();
            }
            catch(...) {}
        
        ImGui::PopFont();
    }
}
