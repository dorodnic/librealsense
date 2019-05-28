// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#pragma once

#include "fw-update/fw-update-device.h"

namespace librealsense
{
    class ds_fw_update_device : public fw_update_device
    {
    public:
        ds_fw_update_device(std::shared_ptr<context> ctx, bool register_device_notifications, std::shared_ptr<platform::usb_device> usb_device);
        virtual ~ds_fw_update_device() = default;

        virtual void update_fw(const void* fw_image, int fw_image_size, fw_update_progress_callback_ptr = nullptr) const override;

    protected:
        virtual const std::string& get_name() const override { return _name; }
        virtual const std::string& get_product_line() const override { return "D400_RECOVERY"; }
    private:
        std::string _name;
    };
}
