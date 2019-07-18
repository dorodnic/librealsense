// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#include "sr300-fw-update-device.h"
#include "sr300.h"
#include "ivcam-private.h"
#include "context.h"
#include <chrono>
#include <thread>

namespace librealsense
{
    sr300_update_device::sr300_update_device(std::shared_ptr<context> ctx, bool register_device_notifications, std::shared_ptr<platform::usb_device> usb_device)
        : update_device(ctx, register_device_notifications, usb_device), 
          _name("Intel RealSense SR300 Recovery"), _product_line("SR300"), _ctx(ctx)
    {

    }

    void sr300_update_device::update(const void* fw_image, int fw_image_size, update_progress_callback_ptr callback) const
    {
        update_device::update(fw_image, fw_image_size, callback);

        bool found = false;
        bool timeout = false;
        auto started = std::chrono::system_clock::now();
        while (!found && !timeout)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            auto elapsed = std::chrono::system_clock::now() - started;
            if (elapsed > std::chrono::seconds(60)) timeout = true;

            auto devs = _ctx->query_devices(RS2_PRODUCT_LINE_SR300);
            for (auto&& info : devs)
            {
                try
                {
                    auto dev = info->create_device();
                    if (dev->get_sensor(0).supports_info(RS2_CAMERA_INFO_ASIC_SERIAL_NUMBER))
                    {
                        auto asic_sn = dev->get_sensor(0).get_info(RS2_CAMERA_INFO_ASIC_SERIAL_NUMBER);
                        if (asic_sn == _asic_serial_number)
                        {
                            auto sr300 = std::dynamic_pointer_cast<sr300_camera>(dev);
                            if (sr300)
                            {
                                sr300->rgb_firmware_burn();
                                sr300->hardware_reset();
                                found = true;
                            }
                        }
                    }
                }
                catch (...)
                {
                }
            }
        }
        if (timeout && !found)
        {
            throw std::runtime_error("Wasn't able to complete the update process");
        }
    }
}
