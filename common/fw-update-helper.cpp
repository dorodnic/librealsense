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
        {RS2_PRODUCT_LINE_SR300, FW_SR3XX_FW_IMAGE_VERSION},
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
        }

        if ("" != FW_SR3XX_FW_IMAGE_VERSION)
        {
            int size = 0;
            auto hex = fw_get_SR3XX_FW_Image(size);
            auto vec = std::vector<uint8_t>(hex, hex + size);
            rv[RS2_PRODUCT_LINE_SR300] = vec;
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

    void firmware_update_manager::fail(std::string error)
    {
        _last_error = error;
        _progress = 0;
        log("\nERROR: " + error);
        log("\nFirmware Update process is safe.\nSimply reconnect the device to get it working again");
        _failed = true;
    }

    void firmware_update_manager::start()
    {
        auto cleanup = _model.cleanup;
        _model.cleanup = [] {};

        log("Started update process");

        auto me = shared_from_this();
        std::weak_ptr<firmware_update_manager> ptr(me);

        _ctx = context();
        _dfu_connected = false;
        _dev_reconnected = false;
        
        std::thread t([ptr, cleanup]() {
            auto self = ptr.lock();
            if (!self) return;

            try
            {
                std::string serial = "";
                if (self->_dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                    serial = self->_dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                else
                    serial = self->_dev.query_sensors().front().get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);

                self->_progress = 5;

                self->_ctx.set_devices_changed_callback(
                    [ptr, serial, cleanup](event_information& info)
                {
                    auto self = ptr.lock();
                    if (!self) return;

                    try
                    {
                        auto devs = info.get_new_devices();
                        if (devs.size() > 0)
                            self->log("New device connected, checking if recovery...");
                            
                        for (auto d : devs)
                        {
                            if (d.query_sensors().size() && d.query_sensors().front().supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                            {
                                auto s = d.query_sensors().front().get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                                //if (s == serial)
                                {
                                    self->log("Discovered connection of the original device");
                                    self->_dev_reconnected = true;
                                    self->_cv.notify_all();
                                }
                            }

                            if (d.is<fw_update_device>())
                            {
                                if (d.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                                {
                                    //if (serial == d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER))
                                    // TODO: Use serial number
                                    {
                                        self->_dfu = d;
                                        self->_dfu_connected = true;
                                        self->_cv.notify_all();
                                    }
                                }
                            }
                        }
                        if (info.was_removed(self->_dev))
                        {
                            self->log("Device successfully disconnected");
                        }
                    }
                    catch (const error& e)
                    {
                        self->fail(error_to_string(e));
                        cleanup();
                    }
                    catch (const std::exception& ex)
                    {
                        self->fail(ex.what());
                        cleanup();
                    }
                    catch (...)
                    {
                        self->fail("Unknown error during update.\nPlease reconnect the camera to exit recovery mode");
                        cleanup();
                    }
                });


                if (!self->_dev.is<fw_update_device>())
                {
                    self->log("Requesting to switch to recovery mode");
                    self->_dev.enter_to_fw_update_mode();

                    {
                        std::unique_lock<std::mutex> lk(self->_m);
                        if (!self->_cv.wait_for(lk, std::chrono::seconds(10),
                            [&] { return self->_dfu_connected || self->_dev_reconnected; }))
                        {
                            self->fail("Device did not reconnect in time!");
                            return;
                        }
                    }

                    if (self->_dev_reconnected)
                    {
                        self->fail("Device reconnected before update started!");
                        return;
                    }
                }
                else
                {
                    self->_dfu = self->_dev.as<fw_update_device>();
                }

                self->_progress = 10;

                self->log("Recovery device connected, starting update");

                self->_dfu.update_fw(self->_fw, [&](const float progress)
                {
                    self->_progress = (ceil(progress*10)/10 * 70) + 10;
                });

                self->log("Update completed, waiting for device to reconnect");

                {
                    std::unique_lock<std::mutex> lk(self->_m);
                    if (!self->_cv.wait_for(lk, std::chrono::seconds(60),
                        [&] { return self->_dev_reconnected; }))
                    {
                        self->fail("Device did not reconnect in time!");
                        return;
                    }
                }

                self->log("Device reconnected succesfully!");

                self->_progress = 100;

                self->_done = true;
            }
            catch (const error& e)
            {
                self->fail(error_to_string(e));
                cleanup();
            }
            catch (const std::exception& ex)
            {
                self->fail(ex.what());
                cleanup();
            }
            catch (...)
            {
                self->fail("Unknown error during update.\nPlease reconnect the camera to exit recovery mode");
                cleanup();
            }
        });
        t.detach();

        _started = true;
    }
}