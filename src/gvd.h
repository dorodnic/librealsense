// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#pragma once

#include <stdint.h>
#include <string>

#define TO_HEX(__str, __field) {__str += "0x"; __str += user_space_frameworks::ToHex(__field);}
#define TO_INT(__str, __field) {__str += std::to_string(__field);}
#define FW_APPEND_TO_STRING_POINT(_str, _field, _TO_STR) {_TO_STR(_str, _field); _str += ".";}
#define FW_APPEND_TO_STRING(_str, _field, _TO_STR) {_TO_STR(_str, _field);}

namespace librealsense
{
    struct change_set_version
    {
        uint8_t        version_revision;
        uint8_t        version_number;
        uint8_t        version_minor;
        uint8_t        version_major;
        std::string to_string() const
        {
            std::string str = "";
            FW_APPEND_TO_STRING_POINT(str, version_major, TO_INT);
            FW_APPEND_TO_STRING_POINT(str, version_minor, TO_INT);
            FW_APPEND_TO_STRING_POINT(str, version_number, TO_INT);
            FW_APPEND_TO_STRING(str, version_revision, TO_INT);
            return str;
        }
    };

    struct change_set_version_padded :public change_set_version
    {
        uint8_t        version_spare[4];
    };

    template<typename T>
    struct major_minor_version
    {
        T        version_minor;
        T        version_major;
        std::string ToString() const
        {
            std::string str = "";
            FW_APPEND_TO_STRING_POINT(str, version_major, TO_INT);
            FW_APPEND_TO_STRING(str, version_minor, TO_INT);
            return str;
        }
    };

    template<typename T, size_t PADDING>
    struct major_minor_version_padding : public major_minor_version<T>
    {
        uint8_t    padding[PADDING];
    };

    template<typename T>
    union endian_convert_helper
    {
        uint8_t data[sizeof(T)];
        T value;
    };

    template<typename T>
    inline std::string to_hex(T value)
    {
        std::stringstream strStream;
        strStream << std::hex << (int)value;
        return strStream.str();
    }

    template<typename T>
    struct number
    {
        T value;
        std::string to_string() const
        {
            return std::to_string(reverse(value));
        }
        std::string to_hex_string() const
        {
            return to_hex(reverse(value));
        }
        std::string to_reversed_hex_string() const
        {
            return to_hex(value);
        }
        std::string to_bool_string() const
        {
            return value == 0 ? "FALSE" : "TRUE";
        }

        T reverse(T v) const {
            endian_convert_helper<T> t;
            endian_convert_helper<T> r;
            t.value = v;
            r.value = 0;
            for (auto i = 0; i < sizeof(T); i++) {
                r.data[i] = t.data[sizeof(T) - 1 - i];
            }
            return r.value;
        }
    };

    template<size_t LENGTH, size_t ACTUAL = LENGTH>
    struct serial
    {
        uint8_t data[LENGTH];

        std::string to_string() const
        {
            std::stringstream formattedBuffer;
            for (auto i = 0; i < sizeof(data); i++)
                formattedBuffer << data[i];

            return formattedBuffer.str();
        }

        std::string to_hex_string() const
        {
            std::stringstream formattedBuffer;
            for (auto i = 0; i < sizeof(data); i++)
                formattedBuffer << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(data[i]);

            return formattedBuffer.str();
        }

        std::string to_ascii_string() const
        {
            std::string str = "";
            for (auto c : data) {
                str += static_cast<char>(c);
            }
            return str;
        }
    };
}
