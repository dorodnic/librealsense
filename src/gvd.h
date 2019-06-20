// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#pragma once

#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>

namespace librealsense
{
    static std::string get_hex_string(const std::vector<uint8_t>& buff, size_t index, size_t length)
    {
        std::stringstream formattedBuffer;
        for (auto i = 0; i < length; i++)
            formattedBuffer << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(buff[index + i]);

        return formattedBuffer.str();
    }

    static std::string create_fw_string(const std::vector<uint8_t>& buff, size_t index, size_t length)
    {
        std::stringstream formattedBuffer;
        std::string s = "";
        for (auto i = 1; i <= length; i++)
        {
            formattedBuffer << s << static_cast<int>(buff[index + (length - i)]);
            s = ".";
        }

        return formattedBuffer.str();
    }
}
