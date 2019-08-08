// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include "process-manager.h"
#include "notifications.h"

#include <random>

namespace rs2
{
    class viewer_model;
    class subdevice_model;

    class on_chip_calib_manager : public process_manager
    {
    public:
        on_chip_calib_manager(viewer_model& viewer, std::shared_ptr<subdevice_model> sub,
            device_model& model, device dev)
            : process_manager("On-Chip Calibration", model), 
             _dev(dev), _sub(sub), _viewer(viewer)
        {
            generator.seed(int(glfwGetTime() * 1000) % 1000);
        }

        static float get_score(float h) { return 1.f - std::max(0.f, std::min(1.f, h * 30.f)); }
        float get_health() const { return _health; }

        void set_speed(int speed) { _speed = speed; }

    private:
        void process_flow(std::function<void()> cleanup) override;

        float _health = 0.f;
        int _speed = 4;
        device _dev;

        std::default_random_engine generator;
        std::uniform_real_distribution<float> distribution{ 0.01f, 0.99f };

        viewer_model& _viewer;
        std::shared_ptr<subdevice_model> _sub;
    };


    struct autocalib_notification_model : public process_notification_model
    {
        autocalib_notification_model(std::string name,
            std::shared_ptr<on_chip_calib_manager> manager, bool expaned);

        on_chip_calib_manager& get_manager() { 
            return *std::dynamic_pointer_cast<on_chip_calib_manager>(update_manager); 
        }

        void set_color_scheme(float t) const override;
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        void draw_expanded(ux_window& win, std::string& error_message) override;
        int calc_height() override;

        float health_check_result = 0.f;

        float old_score = 0.5f;
        float new_score = 0.5f;
        double score_update_time = 0.f;
    };
}