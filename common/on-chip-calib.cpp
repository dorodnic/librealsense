// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.
#include "on-chip-calib.h"

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>
#include <model-views.h>

namespace rs2
{
    void on_chip_calib_manager::process_flow(std::function<void()> cleanup)
    {


        log("Device reconnected succesfully!");

        _progress = 100;

        _done = true;
    }
}
