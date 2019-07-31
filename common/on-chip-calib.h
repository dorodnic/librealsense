// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include "process-manager.h"

namespace rs2
{
    class on_chip_calib_manager : public process_manager
    {
    public:
        on_chip_calib_manager(device_model& model, device dev)
            : process_manager("On-Chip Calibration", model), _dev(dev) {}

    private:
        void process_flow(std::function<void()> cleanup) override;
        bool check_for(
            std::function<bool()> action, std::function<void()> cleanup,
            std::chrono::system_clock::duration delta);

        device _dev;
    };
}