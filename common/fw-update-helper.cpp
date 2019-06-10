// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.
#include "fw-update-helper.h"

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>
#include <model-views.h>

#ifdef INTERNAL_FW
#include "common/fw/D4XX_FW_Image.h"
#include "common/fw/SR3XX_FW_Image.h"
#else
#define FW_D4XX_FW_IMAGE_VERSION ""
#define FW_SR3XX_FW_IMAGE_VERSION ""
#endif // INTERNAL_FW

namespace rs2
{
    static std::map<int, std::string> product_line_to_fw =
    {
        {RS2_PRODUCT_LINE_D400, FW_D4XX_FW_IMAGE_VERSION},
        {RS2_PRODUCT_LINE_D400_RECOVERY, FW_D4XX_FW_IMAGE_VERSION},
        {RS2_PRODUCT_LINE_SR300, FW_SR3XX_FW_IMAGE_VERSION},
        {RS2_PRODUCT_LINE_SR300_RECOVERY, FW_SR3XX_FW_IMAGE_VERSION}
    };

    std::string get_available_firmware_version(int product_line)
    {
        auto it = product_line_to_fw.find(product_line);
        if (it != product_line_to_fw.end())
            return it->second;
        return "";
    }

    std::map<int, std::vector<uint8_t>> create_default_fw_table()
    {
        std::map<int, std::vector<uint8_t>> rv;

        if ("" != FW_D4XX_FW_IMAGE_VERSION)
        {
            int size = 0;
            auto hex = fw_get_D4XX_FW_Image(size);
            auto vec = std::vector<uint8_t>(hex, hex + size);
            rv[RS2_PRODUCT_LINE_D400] = vec;
            rv[RS2_PRODUCT_LINE_D400_RECOVERY] = vec;
        }

        if ("" != FW_SR3XX_FW_IMAGE_VERSION)
        {
            int size = 0;
            auto hex = fw_get_SR3XX_FW_Image(size);
            auto vec = std::vector<uint8_t>(hex, hex + size);
            rv[RS2_PRODUCT_LINE_SR300] = vec;
            rv[RS2_PRODUCT_LINE_SR300_RECOVERY] = vec;
        }

        return rv;
    }

    std::vector<int> parse_fw_version(const std::string& fw)
    {
        std::vector<int> rv;
        size_t pos = 0;
        std::string delimiter = ".";
        auto str = fw + delimiter;
        while ((pos = str.find(delimiter)) != std::string::npos) {
            auto s = str.substr(0, pos);
            int val = std::stoi(s);
            rv.push_back(val);
            str.erase(0, pos + delimiter.length());
        }
        return rv;
    }

    bool is_upgradeable(const std::string& curr, const std::string& available)
    {
        if (curr == "" || available == "") return false;

        size_t fw_string_size = 4;
        auto c = parse_fw_version(curr);
        auto a = parse_fw_version(available);
        if (a.size() != fw_string_size || c.size() != fw_string_size)
            return false;

        for (int i = 0; i < fw_string_size; i++) {
            if (c[i] > a[i])
                return false;
            if (c[i] < a[i])
                return true;
        }
        return false; //equle
    }

    void firmware_update_manager::log(std::string line)
    {
        std::lock_guard<std::mutex> lock(_log_lock);
        _log += line + "\n";
    }

    void firmware_update_manager::start()
    {
        auto cleanup = _model.cleanup;
        _model.cleanup = [] {};

        log("Started update process");

        std::thread t([this, cleanup]{
            try
            {
                context ctx;

                std::mutex m;
                std::condition_variable cv;
                bool dfu_connected = false;
                fw_update_device dfu{ };

                std::string serial = _dev.query_sensors().front().get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);

                _progress = 10;

                ctx.set_devices_changed_callback(
                    [this, &cv, &dfu_connected, serial, &dfu](event_information& info)
                {
                    auto devs = info.get_new_devices();
                    for (auto d : devs)
                    {
                        if (d.is<fw_update_device>())
                        {
                            if (d.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                            {
                                //if (serial == d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER))
                                // TODO: Use serial number
                                {
                                    dfu = d;
                                    dfu_connected = true;
                                    cv.notify_all();
                                }
                            }
                        }
                    }
                    if (info.was_removed(_dev))
                    {
                        log("Device successfully disconnected");
                    }
                });

                log("Requesting to switch to recovery mode");
                _dev.enter_to_fw_update_mode();

                {
                    std::unique_lock<std::mutex> lk(m);
                    cv.wait(lk, [&] { return dfu_connected; });
                }

                log("Recovery device connected");

                dfu.hardware_reset();

                std::this_thread::sleep_for(std::chrono::seconds(5));

                _progress = 100;

                _done = true;
            }
            catch (std::exception ex)
            {
                _last_error = ex.what();
                _failed = true;
                cleanup();
            }
            catch (...)
            {
                _last_error = "Unknown error during update.\nPlease reconnect the camera to exit recovery mode";
                _failed = true;
                cleanup();
            }
        });
        t.detach();
    }
}