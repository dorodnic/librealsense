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
    struct subdevice_ui_selection;

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

        void keep();

        void restore_workspace();

        void apply_calib(bool old);

        std::pair<float, float> get_metric(bool use_new);

    private:
        rs2::depth_frame fetch_depth_frame();

        std::pair<float, float> get_depth_metrics();

        void process_flow(std::function<void()> cleanup) override;

        float _health = 0.f;
        int _speed = 4;
        device _dev;

        bool _was_streaming = false;
        bool _synchronized = false;
        bool _post_processing = false;
        std::shared_ptr<subdevice_ui_selection> _ui { nullptr };
        bool _in_3d_view = false;

        std::default_random_engine generator;
        std::uniform_real_distribution<float> distribution{ 0.01f, 0.99f };

        viewer_model& _viewer;
        std::shared_ptr<subdevice_model> _sub;

        std::vector<uint8_t> _old_calib, _new_calib;
        std::vector<std::pair<float, float>> _metrics;

        void stop_viewer();
        void start_viewer(int w, int h, int fps);
    };


    struct autocalib_notification_model : public process_notification_model
    {
        enum auto_calib_ui_state
        {
            RS2_CALIB_STATE_INITIAL_PROMPT,
            RS2_CALIB_STATE_FAILED,
            RS2_CALIB_STATE_COMPLETE,
            RS2_CALIB_STATE_CALIB_IN_PROCESS,
            RS2_CALIB_STATE_CALIB_COMPLETE,
        };

        autocalib_notification_model(std::string name,
            std::shared_ptr<on_chip_calib_manager> manager, bool expaned);

        on_chip_calib_manager& get_manager() { 
            return *std::dynamic_pointer_cast<on_chip_calib_manager>(update_manager); 
        }

        void set_color_scheme(float t) const override;
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        void draw_expanded(ux_window& win, std::string& error_message) override;
        int calc_height() override;

        bool use_new_calib = true;
    };
}