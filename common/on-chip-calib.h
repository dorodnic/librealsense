// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include "process-manager.h"
#include "notifications.h"

namespace rs2
{
    class on_chip_calib_manager : public process_manager
    {
    public:
        on_chip_calib_manager(device_model& model, device dev)
            : process_manager("On-Chip Calibration", model), _dev(dev) {}

    private:
        void process_flow(std::function<void()> cleanup) override;

        device _dev;
    };

    struct autocalib_notification_model : public process_notification_model
    {
        autocalib_notification_model(std::string name,
            std::shared_ptr<on_chip_calib_manager> manager, bool expaned);

        void set_color_scheme(float t) const override;
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        void draw_expanded(ux_window& win, std::string& error_message) override;
        int calc_height() override;
    };
}