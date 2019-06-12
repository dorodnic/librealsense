// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#include "ds5-fw-update-device.h"
#include "ds5-private.h"

namespace librealsense
{
    ds_fw_update_device::ds_fw_update_device(std::shared_ptr<context> ctx, bool register_device_notifications, std::shared_ptr<platform::usb_device> usb_device)
        : fw_update_device(ctx, register_device_notifications, usb_device), _product_line("D400")
    {
        auto messenger = usb_device->open();
        auto info = usb_device->get_info();
        _name = ds::rs400_sku_names.find(info.pid) != ds::rs400_sku_names.end() ? ds::rs400_sku_names.at(info.pid) : "unknown";        
    }

    void ds_fw_update_device::update_fw(const void* fw_image, int fw_image_size, fw_update_progress_callback_ptr callback) const
    {
        fw_update_device::update_fw(fw_image, fw_image_size, callback);
    }
}
