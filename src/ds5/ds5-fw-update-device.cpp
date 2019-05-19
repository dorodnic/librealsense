// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#include "ds5-fw-update-device.h"
#include "ds5-private.h"

namespace librealsense
{
    ds_fw_update_device::ds_fw_update_device(std::shared_ptr<context> ctx, bool register_device_notifications, std::shared_ptr<platform::usb_device> usb_device)
        : fw_update_device(ctx, register_device_notifications, usb_device)
    {
        auto messenger = usb_device->open();
        auto info = usb_device->get_info();
        _name = ds::rs400_sku_names.find(info.pid) != ds::rs400_sku_names.end() ? ds::rs400_sku_names.at(info.pid) : "unknown";
    }

    void ds_fw_update_device::finishing_task() const
    {
        auto messenger = _usb_device->open();
        // WaitForDFU state sends several DFU_GETSTATUS requests, until we hit
        // either RS2_DFU_STATE_DFU_MANIFEST_WAIT_RESET or RS2_DFU_STATE_DFU_ERROR status.
        // This command also reset the device
        if (!wait_for_state(messenger, RS2_DFU_STATE_DFU_MANIFEST_WAIT_RESET))
            throw std::runtime_error("firmware manifest failed");
    }
}
