// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include <map>
#include <string>
#include <sstream>

namespace rs2
{
    class config_file
    {
    public:
        config_file();
        config_file(std::string filename);

        bool operator==(const config_file& other) const;

        config_file& operator=(const config_file& other);

        void set(const char* key, const char* value);
        std::string get(const char* key, const char* def = "") const;

        template<class T>
        T get(const char* key, T def) const
        {
            if (!contains(key)) return def;
            try
            {
                std::stringstream ss;
                ss.str(get(key));
                T res;
                ss >> res;
                return res;
            }
            catch(...)
            {
                return def;
            }
        }

        template<class T>
        void set(const char* key, T val)
        {
            std::stringstream ss;
            ss << val;
            set(key, ss.str().c_str());
        }

        bool contains(const char* key) const;
        
        void save(const char* filename);

        void reset();

        static config_file& instance();

    private:
        void save();

        std::map<std::string, std::string> _values;
        std::string _filename;
    };
}