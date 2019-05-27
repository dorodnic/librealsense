// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>
#include "context.h"
#include "ux-window.h"
#include "model-views.h"

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
    static std::map<int, std::string> product_line_to_fw =
    {
        {RS2_PRODUCT_LINE_D400, FW_D4XX_FW_IMAGE_VERSION},
        {RS2_PRODUCT_LINE_D400_RECOVERY, FW_D4XX_FW_IMAGE_VERSION},
        {RS2_PRODUCT_LINE_SR300, FW_SR3XX_FW_IMAGE_VERSION},
        {RS2_PRODUCT_LINE_SR300_RECOVERY, FW_SR3XX_FW_IMAGE_VERSION}
    };

    static std::map<int, std::vector<uint8_t>> create_default_fw_table()
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

    static std::vector<int> parse_fw_version(const std::string& fw)
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

    static bool is_upgradeable(const std::string& curr, const std::string& available)
    {
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

    static void add_device(rs2::context ctx, int product_line, std::map<std::string, fw_update_device_info>& device_map)
    {
        static auto default_fw_table = create_default_fw_table();
        auto recommended_fw_version = product_line_to_fw.at(product_line);
        auto fw = default_fw_table.at(product_line);

        auto devices = ctx.query_devices(product_line);
        if (product_line & RS2_PRODUCT_LINE_RECOVERY)
        {
            for (auto&& d : devices)
            {
                fw_update_device_info fudi = { d, product_line, true, "", "", recommended_fw_version, "", fw };
                device_map["recovery"] = fudi;
            }
        }
        else {
            for (auto&& d : devices)
            {
                auto serial = d.supports(RS2_CAMERA_INFO_SERIAL_NUMBER) ? d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER) : "";
                auto fw_version = d.supports(RS2_CAMERA_INFO_FIRMWARE_VERSION) ? d.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION) : "";
                auto minimal_fw_version = d.supports(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION) ? d.get_info(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION) : "";
                bool upgradeable = is_upgradeable(fw_version, recommended_fw_version);
                fw_update_device_info fudi = { d, product_line, upgradeable, serial, fw_version, recommended_fw_version, minimal_fw_version, fw };
                device_map[serial] = fudi;
            }
        }
    }

    static std::map<std::string, fw_update_device_info> create_upgradeable_device_table(rs2::context ctx)
    {
        std::map<std::string, fw_update_device_info> rv;

        add_device(ctx, RS2_PRODUCT_LINE_D400, rv);
        add_device(ctx, RS2_PRODUCT_LINE_SR300, rv);
        add_device(ctx, RS2_PRODUCT_LINE_D400_RECOVERY, rv);
        add_device(ctx, RS2_PRODUCT_LINE_SR300_RECOVERY, rv);

        return rv;
    }

    class fw_update_helper
    {
    public:
        fw_update_helper(rs2::context ctx, const ux_window& window, viewer_model& vm)
            : _context(ctx), _window(window), _viewer_model(vm), _update_progress(-1) {}

        ~fw_update_helper()
        {
            if (_update_thread.joinable())
                _update_thread.join();
        }

        void set_update_request(std::vector<uint8_t> image) { _fw_image = image; }

        bool has_update_request() { return !_fw_image.empty(); }

        bool is_update_in_progress() { return _update_progress >= 0; }

        float get_progress() { return _update_progress; }

        void refresh()
        {
            try
            {
                _upgradeable_devices = create_upgradeable_device_table(_context);
                if (_update_thread.joinable())
                    _update_thread.join();
                if (_fw_image.size() > 0)
                    _update_thread = std::thread(&fw_update_helper::recover_devices, this, std::move(_fw_image));
            }
            catch (...)
            {
                //TODO
            }

        }

        void validate_fw_update_requests(device_model& dev_model)
        {
            // check for fw update recomendation for any of the connected devices
            for (auto&& d : _upgradeable_devices)
            {
                if (!d.second.upgrade_recommended)
                    continue;
                if (!dev_model.dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                    continue;
                auto sn = dev_model.dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                bool update = false;
                if (d.second.serial_number == sn)
                    _viewer_model.popup_if_fw_update_required(_window, d.second, update);
                if (update)
                    _fw_image = d.second.fw_image;
            }

            // check if there was a user request for FW update on a device in recovery mode
            check_for_update_request(dev_model, "recovery");

            // check if there was a user request for FW update on a standard device
            if (!dev_model.dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                return;
            auto sn = dev_model.dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
            check_for_update_request(dev_model, sn);
        }

    private:
        float _update_progress;
        rs2::context _context;
        const ux_window& _window;
        viewer_model& _viewer_model;
        std::vector<uint8_t> _fw_image;
        std::thread _update_thread;
        std::map<std::string, fw_update_device_info> _upgradeable_devices;

        void check_for_update_request(device_model& dev_model, const std::string& id)
        {
            if (dev_model.fw_update_requested)
            {
                std::vector<uint8_t> fw;
                bool canceled = false;
                fw_update_device_info fudi = {};

                auto itr = _upgradeable_devices.find(id);

                if (itr != _upgradeable_devices.end())
                    fudi = (*itr).second;

                if (!fudi.dev)
                    return;

                _viewer_model.popup_fw_file_select(_window, fudi, fw, canceled);
                if (canceled || fw.size() > 0)
                    dev_model.fw_update_requested = false;
                if (fw.size() > 0)
                {
                    _fw_image = fw;//this is the way to signal that there is a pending fw update request.
                }
            }
        }

        void recover_devices(std::vector<uint8_t> fw_image)
        {
            auto fwu_devs = _context.query_devices(RS2_PRODUCT_LINE_RECOVERY);
            for (auto&& d : fwu_devs)
            {
                _update_progress = 0;
                auto fwu_dev = d.as<rs2::fw_update_device>();

                if (!fwu_dev)
                    continue;
                _viewer_model.not_model.add_log(to_string() << "firmware update started\n");
                fwu_dev.update_fw(fw_image, [&](const float progress)
                {
                    _update_progress = progress;
                });
                _viewer_model.not_model.add_log(to_string() << "firmware update done\n");
                _update_progress = -1;
            }
        }
    };
}