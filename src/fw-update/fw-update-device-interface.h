// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#pragma once

#include "types.h"
#include "core/streaming.h"

namespace librealsense
{
    class fw_update_device_interface : public device_interface
    {
    public:
        virtual void update_fw(const void* fw_image, int fw_image_size, fw_update_progress_callback_ptr = nullptr) const = 0;

    protected:
        virtual const std::string& get_name() const = 0;
        virtual const std::string& get_serial_number() const = 0;
        virtual bool wait_for_device(int mask, uint32_t timeout) const = 0;
    };

    MAP_EXTENSION(RS2_EXTENSION_FW_UPDATE_DEVICE, fw_update_device_interface);
}
