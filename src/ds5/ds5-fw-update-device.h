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
        virtual void finishing_task() const override;
        virtual ~ds_fw_update_device() = default;

    protected:
        virtual const std::string get_name() const override { return _name; }

    private:
        std::string _name;
    };
}
