// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>

#include <map>
#include <vector>
#include <string>

namespace rs2
{
    std::string get_available_firmware_version(int product_line);
    std::map<int, std::vector<uint8_t>> create_default_fw_table();
    std::vector<int> parse_fw_version(const std::string& fw);
    bool is_upgradeable(const std::string& curr, const std::string& available);

    class firmware_update_manager
    {
    public:
        firmware_update_manager(device dev, std::vector<uint8_t> fw) 
            : _dev(dev), _fw(fw) {}

        void start();
        int get_progress() const { return _progress; }
        bool is_done() const { return _done; }
        const std::string& get_log() const { return _log; }

    private:
        std::string _log;
        bool _started = false;
        bool _done = false;
        int _progress = 0;
        device _dev;
        std::vector<uint8_t> _fw;
    };
}