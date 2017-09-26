// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

/*! \file rs_device.hpp
    \brief This file contains abstractions for Intel RealSense devices
    Device in the SDK abstracts a collection of sensors that share common power-management
    and physical case. From a device you can access the collection of its sensors or
    perform global operations that might have side-effects across sensors
*/

#ifndef LIBREALSENSE_RS2_DEVICE_HPP
#define LIBREALSENSE_RS2_DEVICE_HPP

#include "rs_types.hpp"
#include "rs_sensor.hpp"

namespace rs2
{
    class device
    {
    public:
        using SensorType = sensor;
        using ProfileType = stream_profile;

        /**
        * returns the list of adjacent devices, sharing the same physical parent composite device
        * \return            the list of adjacent devices
        */
        std::vector<sensor> query_sensors() const
        {
            std::shared_ptr<rs2_sensor_list> list(
                rs2_query_sensors(_dev.get(), handle_error()),
                rs2_delete_sensor_list);

            auto size = rs2_get_sensors_count(list.get(), handle_error());

            std::vector<sensor> results;
            for (auto i = 0; i < size; i++)
            {
                std::shared_ptr<rs2_sensor> dev(
                    rs2_create_sensor(list.get(), i, handle_error()),
                    rs2_delete_sensor);

                sensor rs2_dev(dev);
                results.push_back(rs2_dev);
            }

            return results;
        }

        template<class T>
        T first()
        {
            for (auto&& s : query_sensors())
            {
                if (auto t = s.as<T>()) return t;
            }
            throw rs2::error("Could not find requested sensor type!");
        }

        /**
        * check if specific camera info is supported
        * \param[in] info    the parameter to check for support
        * \return                true if the parameter both exist and well-defined for the specific device
        */
        bool supports(rs2_camera_info info) const
        {
            return rs2_supports_device_info(_dev.get(), info, handle_error()) > 0;
        }

        /**
        * retrieve camera specific information, like versions of various internal components
        * \param[in] info     camera info type to retrieve
        * \return             the requested camera info string, in a format specific to the device model
        */
        const char* get_info(rs2_camera_info info) const
        {
            return rs2_get_device_info(_dev.get(), info, handle_error());
        }

        /**
        * send hardware reset request to the device
        */
        void hardware_reset()
        {
            rs2_hardware_reset(_dev.get(), handle_error());
        }

        device& operator=(const std::shared_ptr<rs2_device> dev)
        {
            _dev.reset();
            _dev = dev;
            return *this;
        }
        device& operator=(const device& dev)
        {
            *this = nullptr;
            _dev = dev._dev;
            return *this;
        }
        device() : _dev(nullptr) {}

        operator bool() const
        {
            return _dev != nullptr;
        }
        const std::shared_ptr<rs2_device>& get() const
        {
            return _dev;
        }

        template<class T>
        bool is() const
        {
            T extension(*this);
            return extension;
        }

        template<class T>
        T as() const
        {
            T extension(*this);
            return extension;
        }
        virtual ~device()
        {
        }
    protected:
        friend context;
        friend device_list;
        friend class pipeline;
        friend class device_hub;

        std::shared_ptr<rs2_device> _dev;
        explicit device(std::shared_ptr<rs2_device> dev) : _dev(dev)
        {
        }
    };

    class debug_protocol : public device
    {
    public:
        debug_protocol(device d)
                : device(d.get())
        {
            if(rs2_is_device_extendable_to(_dev.get(), RS2_EXTENSION_DEBUG, handle_error()) == 0)
            {
                _dev = nullptr;
            }
        }

        std::vector<uint8_t> send_and_receive_raw_data(const std::vector<uint8_t>& input) const
        {
            std::vector<uint8_t> results;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                    rs2_send_and_receive_raw_data(_dev.get(), (void*)input.data(), (uint32_t)input.size(), handle_error()),
                    rs2_delete_raw_data);

            auto size = rs2_get_raw_data_size(list.get(), handle_error());
            auto start = rs2_get_raw_data(list.get(), handle_error());
            results.insert(results.begin(), start, start + size);
            return results;
        }
    };

    class device_list
    {
    public:
        explicit device_list(std::shared_ptr<rs2_device_list> list)
            : _list(move(list)) {}

        device_list()
            : _list(nullptr) {}

        operator std::vector<device>() const
        {
            std::vector<device> res;
            for (auto&& dev : *this) res.push_back(dev);
            return res;
        }

        bool contains(const device& dev) const
        {
            return !!(rs2_device_list_contains(_list.get(), dev.get().get(), handle_error()));
        }

        device_list& operator=(std::shared_ptr<rs2_device_list> list)
        {
            _list = move(list);
            return *this;
        }

        device operator[](uint32_t index) const
        {
            std::shared_ptr<rs2_device> dev(
                rs2_create_device(_list.get(), index, handle_error()),
                rs2_delete_device);
            return device(dev);
        }

        uint32_t size() const
        {
            return rs2_get_device_count(_list.get(), handle_error());
        }

        device front() const { return std::move((*this)[0]); }
        device back() const
        {
            return std::move((*this)[size() - 1]);
        }

        class device_list_iterator
        {
            device_list_iterator(
                const device_list& device_list,
                uint32_t uint32_t)
                : _list(device_list),
                  _index(uint32_t)
            {
            }

        public:
            device operator*() const
            {
                return _list[_index];
            }
            bool operator!=(const device_list_iterator& other) const
            {
                return other._index != _index || &other._list != &_list;
            }
            bool operator==(const device_list_iterator& other) const
            {
                return !(*this != other);
            }
            device_list_iterator& operator++()
            {
                _index++;
                return *this;
            }
        private:
            friend device_list;
            const device_list& _list;
            uint32_t _index;
        };

        device_list_iterator begin() const
        {
            return device_list_iterator(*this, 0);
        }
        device_list_iterator end() const
        {
            return device_list_iterator(*this, size());
        }
        const rs2_device_list* get_list() const
        {
            return _list.get();
        }

    private:
        std::shared_ptr<rs2_device_list> _list;
    };

    template<class T>
    class status_changed_callback : public rs2_playback_status_changed_callback
    {
        T on_status_changed_function;
    public:
        explicit status_changed_callback(T on_status_changed) : on_status_changed_function(on_status_changed) {}

        void on_playback_status_changed(rs2_playback_status status) override
        {
            on_status_changed_function(status);
        }

        void release() override { delete this; }
    };

    class event_information
    {
    public:
        event_information(device_list removed, device_list added)
            :_removed(removed), _added(added){}

        /**
        * check if specific device was disconnected
        * \return            true if device disconnected, false if device connected
        */
        bool was_removed(const rs2::device& dev) const
        {
            if(!dev) return false;
            return rs2_device_list_contains(_removed.get_list(), dev.get().get(), handle_error()) > 0;
        }

        /**
        * returns a list of all newly connected devices
        * \return            the list of all new connected devices
        */
        device_list get_new_devices()  const
        {
            return _added;
        }

    private:
        device_list _removed;
        device_list _added;
    };
}
#endif // LIBREALSENSE_RS2_DEVICE_HPP
