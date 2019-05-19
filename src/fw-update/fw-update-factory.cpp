// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#include "fw-update-factory.h"
#include "fw-update-device.h"
#include "usb/usb-enumerator.h"
#include "ds5/ds5-private.h"
#include "ds5/ds5-fw-update-device.h"
#include "ivcam/sr300.h"
#include "ivcam/sr300-fw-update-device.h"

namespace librealsense
{
    bool is_recovery_pid(uint16_t pid)
    {
        return (
            std::find(ds::recovery_pid.begin(), ds::recovery_pid.end(), pid) != ds::recovery_pid.end()) ||
            SR300_RECOVERY == pid;
    }

    std::vector<std::shared_ptr<device_info>> fw_update_info::pick_recovery_devices(
        std::shared_ptr<context> ctx,
        const std::vector<platform::usb_device_info>& usb_devices)
    {
        std::vector<std::shared_ptr<device_info>> list;
        for (auto&& usb : usb_devices)
        {
            if (is_recovery_pid(usb.pid))
            {
                list.push_back(std::make_shared<fw_update_info>(ctx, usb));
            }
        }
        return list;
    }

    std::shared_ptr<device_interface> fw_update_info::create(std::shared_ptr<context> ctx, bool register_device_notifications) const
    {
        try
        {
            for (auto&& info : platform::usb_enumerator::query_devices_info())
            {
                if (info.unique_id == _dfu.unique_id && info.cls == platform::RS2_USB_CLASS_UNSPECIFIED)
                {
                    auto usb = platform::usb_enumerator::create_usb_device(info);
                    if (!usb)
                        return nullptr;
                    if (std::find(ds::recovery_pid.begin(), ds::recovery_pid.end(), info.pid) != ds::recovery_pid.end())
                        return std::make_shared<ds_fw_update_device>(ctx, register_device_notifications, usb);
                    if (SR300_RECOVERY == info.pid)
                        return std::make_shared<sr300_fw_update_device>(ctx, register_device_notifications, usb);
                }
            }
            return nullptr;
        }
        catch (std::exception e)
        {
            LOG_ERROR(e.what());
            return nullptr;
        }
    }
}
