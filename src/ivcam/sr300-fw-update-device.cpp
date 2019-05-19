// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#include "sr300-fw-update-device.h"
#include "ivcam-private.h"

namespace librealsense
{
    sr300_fw_update_device::sr300_fw_update_device(std::shared_ptr<context> ctx, bool register_device_notifications, std::shared_ptr<platform::usb_device> usb_device)
        : fw_update_device(ctx, register_device_notifications, usb_device), _name("Intel RealSense SR300 Recovery")
    {
        auto messenger = usb_device->open();
        auto info = usb_device->get_info();
    }

    void sr300_fw_update_device::finishing_task() const
    {
        auto messenger = _usb_device->open();
        // TODO_MK
    }
}
