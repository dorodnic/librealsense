// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#pragma once
#include "model-views.h"

namespace rs2
{
    constexpr const char* recommended_fw_url = "https://downloadcenter.intel.com/download/27522/Latest-Firmware-for-Intel-RealSense-D400-Product-Family?v=t";
    constexpr const char* store_url = "https://store.intelrealsense.com/";

    struct notification_data
    {
        notification_data(std::string description,
                            double timestamp,
                            rs2_log_severity severity,
                            rs2_notification_category category);
        rs2_notification_category get_category() const;
        std::string get_description() const;
        double get_timestamp() const;
        rs2_log_severity get_severity() const;

        std::string _description;
        double _timestamp;
        rs2_log_severity _severity;
        rs2_notification_category _category;
    };

    struct notification_model
    {
        notification_model();
        notification_model(const notification_data& n);
        double get_age_in_ms() const;
        bool interacted() const;
        void draw(ux_window& win, int w, int y, notification_model& selected);
        void set_color_scheme(float t) const;
        void unset_color_scheme() const;
        const int get_max_lifetime_ms() const;

        int height = 40;
        int index = 0;
        std::string message;
        double timestamp = 0.0;
        rs2_log_severity severity = RS2_LOG_SEVERITY_NONE;
        std::chrono::high_resolution_clock::time_point created_time;
        rs2_notification_category category;
        bool to_close = false; // true when user clicks on close notification
        // TODO: Add more info

        std::chrono::high_resolution_clock::time_point last_interacted;
    };

    struct notifications_model
    {
        void add_notification(const notification_data& n);
        void draw(ux_window& win, int w, int h);

        void foreach_log(std::function<void(const std::string& line)> action)
        {
            std::lock_guard<std::mutex> lock(m);
            for (auto&& l : log)
            {
                action(l);
            }

            auto rc = ImGui::GetCursorPos();
            ImGui::SetCursorPos({ rc.x, rc.y + 5 });

            if (new_log)
            {
                ImGui::SetScrollPosHere();
                new_log = false;
            }
        }

        void add_log(std::string line)
        {
            std::lock_guard<std::mutex> lock(m);
            if (!line.size()) return;
            if (line[line.size() - 1] != '\n') line += "\n";
            log.push_back(line);
            new_log = true;
        }

    private:
        std::vector<notification_model> pending_notifications;
        int index = 1;
        const int MAX_SIZE = 6;
        std::mutex m;
        bool new_log = false;

        std::vector<std::string> log;
        notification_model selected;
    };
}
