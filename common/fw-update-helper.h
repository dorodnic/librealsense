// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>

#include <map>
#include <vector>
#include <string>

#ifdef INTERNAL_FW
#include "common/fw/D4XX_FW_Image.h"
#include "common/fw/SR3XX_FW_Image.h"
#else
#define FW_D4XX_FW_IMAGE_VERSION ""
#define FW_SR3XX_FW_IMAGE_VERSION ""
#endif // INTERNAL_FW

namespace rs2
{
    static std::map<std::string, std::string> product_line_to_fw =
    {
        {"D400", FW_D4XX_FW_IMAGE_VERSION},
        {"D400_RECOVERY", FW_D4XX_FW_IMAGE_VERSION},
        {"SR300", FW_SR3XX_FW_IMAGE_VERSION},
        {"SR300_RECOVERY", FW_SR3XX_FW_IMAGE_VERSION}
    };

    static std::map<std::string, std::vector<uint8_t>> create_default_fw_table()
    {
        std::map<std::string, std::vector<uint8_t>> rv;

        if ("" != FW_D4XX_FW_IMAGE_VERSION)
        {
            int size = 0;
            auto hex = fw_get_D4XX_FW_Image(size);
            auto vec = std::vector<uint8_t>(hex, hex + size);
            rv["D400"] = vec;
            rv["D400_RECOVERY"] = vec;
        }

        if ("" != FW_SR3XX_FW_IMAGE_VERSION)
        {
            int size = 0;
            auto hex = fw_get_SR3XX_FW_Image(size);
            auto vec = std::vector<uint8_t>(hex, hex + size);
            rv["SR300"] = vec;
            rv["SR300_RECOVERY"] = vec;
        }

        return rv;
    }

    static std::vector<int> parse_fw_version(const std::string& fw)
    {
        std::vector<int> rv;
        try {
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
        catch (...)
        {
            return rv;
        }
    }

    static bool is_upgradeable(const std::string& curr, const std::string& available)
    {
        size_t fw_string_size = 4;
        if (curr == "" || available == "")
            return false;
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

    class fw_update_helper
    {
    public:
        fw_update_helper(rs2::device dev)
            : _device(dev), _update_progress(-1)
        {
            _recommended_fw_version = get_recommended_fw_version(dev);
            _serial_number = _device.supports(RS2_CAMERA_INFO_SERIAL_NUMBER) ? _device.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER) : "";
            _curr_fw_version = _device.supports(RS2_CAMERA_INFO_FIRMWARE_VERSION) ? _device.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION) : "";
            _minimal_fw_version = _device.supports(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION) ? _device.get_info(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION) : "";
            _upgrade_recommended = is_upgradeable(_curr_fw_version, _recommended_fw_version);
        }

        ~fw_update_helper()
        {
            if (_update_thread.joinable())
                _update_thread.join();
        }

        void update(const std::vector<uint8_t>& fw_image)
        {
            _update_thread = std::thread(&fw_update_helper::update_fw, this, std::move(fw_image), [](float p) {});
        }

        void update(const std::vector<uint8_t>& fw_image, std::function<void(float)> on_progress)
        {
            _update_thread = std::thread(&fw_update_helper::update_fw, this, std::move(fw_image), on_progress);
        }

        bool is_upgrade_recommended() { return _upgrade_recommended; }
        bool updating() { return _update_progress >= 0; }
        float get_progress() { return _update_progress; }

        const std::string get_serial_number() { return _serial_number; }
        const std::string get_curr_fw_version() { return _curr_fw_version; }
        const std::string get_recommended_fw_version() { return _recommended_fw_version; }
        const std::string get_minimal_fw_version() { return _minimal_fw_version; }

    private:
        rs2::device _device;
        bool _upgrade_recommended;
        std::string _serial_number;
        std::string _curr_fw_version;
        std::string _recommended_fw_version;
        std::string _minimal_fw_version;

        float _update_progress;
        std::thread _update_thread;

        void update_fw(const std::vector<uint8_t>& fw_image, std::function<void(float)> on_progress)
        {
            _update_progress = 0;
            auto fwu_dev = _device.as<rs2::fw_update_device>();

            fwu_dev.update_fw(fw_image, [&](const float progress)
            {
                _update_progress = progress;
                on_progress(progress);
            });
            _update_progress = -1;
        }

        std::string get_recommended_fw_version(const rs2::device& dev)
        {
            if (!dev.supports(RS2_CAMERA_INFO_PRODUCT_LINE))
                return "";
            auto pl = dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE);
            if (product_line_to_fw.find(pl) == product_line_to_fw.end())
                return "";
            return product_line_to_fw.at(dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE));
        }
    };
}