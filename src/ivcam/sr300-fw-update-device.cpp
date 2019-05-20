// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#include "sr300-fw-update-device.h"
#include "ivcam-private.h"

namespace librealsense
{
    std::vector<uint8_t> create_hw_reset_buffer()
    {
        uint16_t header_size = 4;
        uint16_t data_size = 20;
        std::vector<uint8_t> rv(header_size + data_size);

        uint32_t HWReset = librealsense::ivcam::fw_cmd::HWReset;

        uint16_t IVCAM_MONITOR_MAGIC_NUMBER = 0xcdab;

        memcpy(rv.data(), &data_size, sizeof(uint16_t));
        memcpy(rv.data() + 2, &IVCAM_MONITOR_MAGIC_NUMBER, sizeof(uint16_t));
        memcpy(rv.data() + 4, &HWReset, sizeof(uint32_t));

        return rv;
    }


    sr300_fw_update_device::sr300_fw_update_device(std::shared_ptr<context> ctx, bool register_device_notifications, std::shared_ptr<platform::usb_device> usb_device)
        : fw_update_device(ctx, register_device_notifications, usb_device), _name("Intel RealSense SR300 Recovery")
    {
        auto messenger = usb_device->open();
        auto info = usb_device->get_info();
    }

    void sr300_fw_update_device::finishing_task() const
    {
        auto messenger = _usb_device->open();
        auto data = create_hw_reset_buffer();
        auto intfs = _usb_device->get_interfaces();
        auto it = std::find_if(intfs.begin(), intfs.end(),
            [](const platform::rs_usb_interface& i) { return i->get_class() == platform::RS2_USB_CLASS_VENDOR_SPECIFIC; });
        if (it == intfs.end())
            throw std::runtime_error("can't find HWM interface of device: " + _usb_device->get_info().id);

        auto hwm = *it;
        uint32_t transfered_count = 0;
        uint32_t timeout_ms = 10;
        auto sts = messenger->bulk_transfer(hwm->first_endpoint(platform::RS2_USB_ENDPOINT_DIRECTION_WRITE), const_cast<uint8_t*>(data.data()), data.size(), transfered_count, timeout_ms);

        if (sts != platform::RS2_USB_STATUS_SUCCESS)
            throw std::runtime_error("command transfer failed to execute bulk transfer, error: " + platform::usb_status_to_string.at(sts));

    }
}
