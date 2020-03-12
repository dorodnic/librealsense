// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#pragma once

#include "ds5-private.h"

#include "algo.h"
#include "error-handling.h"

namespace librealsense
{
    class emitter_option : public uvc_xu_option<uint8_t>
    {
    public:
        const char* get_value_description(float val) const override;
        explicit emitter_option(uvc_sensor& ep);
    };

    class asic_and_projector_temperature_options : public readonly_option
    {
    public:
        float query() const override;

        option_range get_range() const override;

        bool is_enabled() const override;

        const char* get_description() const override;

        explicit asic_and_projector_temperature_options(uvc_sensor& ep, rs2_option opt);

    private:
        uvc_sensor&                 _ep;
        rs2_option                  _option;
    };

    class motion_module_temperature_option : public readonly_option
    {
    public:
        float query() const override;

        option_range get_range() const override;

        bool is_enabled() const override;

        const char* get_description() const override;

        explicit motion_module_temperature_option(hid_sensor& ep);

    private:
        const std::string custom_sensor_name = "custom";
        const std::string report_name = "data-field-custom-value_2";
        hid_sensor& _ep;
    };

    class enable_motion_correction : public option_base
    {
    public:
        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Enable/Disable Automatic Motion Data Correction";
        }

        enable_motion_correction(sensor_base* mm_ep, const option_range& opt_range);

    private:
        std::atomic<bool>   _is_active;
    };

    class enable_auto_exposure_option : public option_base
    {
    public:
        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Enable/disable auto-exposure";
        }

        auto_exposure_mechanism* get_auto_exposure() { return _auto_exposure.get(); }
        bool to_add_frames() { return _to_add_frames.load(); }

        enable_auto_exposure_option(synthetic_sensor* fisheye_ep,
                                    std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                    std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                    const option_range& opt_range);

    private:
        std::shared_ptr<auto_exposure_state>         _auto_exposure_state;
        std::atomic<bool>                            _to_add_frames;
        std::shared_ptr<auto_exposure_mechanism>     _auto_exposure;
    };

    class auto_exposure_mode_option : public option_base
    {
    public:
        auto_exposure_mode_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                  std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                  const option_range& opt_range,
                                  const std::map<float, std::string>& description_per_value);

        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Auto-Exposure Mode";
        }

        const char* get_value_description(float val) const override;

    private:
        const std::map<float, std::string>          _description_per_value;
        std::shared_ptr<auto_exposure_state>        _auto_exposure_state;
        std::shared_ptr<auto_exposure_mechanism>    _auto_exposure;
    };

    class auto_exposure_step_option : public option_base
    {
    public:
        auto_exposure_step_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                  std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                  const option_range& opt_range);

        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Auto-Exposure converge step";
        }

    private:
        std::shared_ptr<auto_exposure_state>        _auto_exposure_state;
        std::shared_ptr<auto_exposure_mechanism>    _auto_exposure;
    };

    class auto_exposure_antiflicker_rate_option : public option_base
    {
    public:
        auto_exposure_antiflicker_rate_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                              std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                              const option_range& opt_range,
                                              const std::map<float, std::string>& description_per_value);

        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Auto-Exposure anti-flicker";
        }

        const char* get_value_description(float val) const override;

    private:
        const std::map<float, std::string>           _description_per_value;
        std::shared_ptr<auto_exposure_state>         _auto_exposure_state;
        std::shared_ptr<auto_exposure_mechanism>     _auto_exposure;
    };

    class depth_scale_option : public option, public observable_option
    {
    public:
        depth_scale_option(hw_monitor& hwm);
        virtual ~depth_scale_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Number of meters represented by a single depth unit";
        }
        void enable_recording(std::function<void(const option &)> record_action) override
        {
            _record_action = record_action;
        }

    private:
        ds::depth_table_control get_depth_table(ds::advanced_query_mode mode) const;
        std::function<void(const option &)> _record_action = [](const option&) {};
        lazy<option_range> _range;
        hw_monitor& _hwm;
    };

    class external_sync_mode : public option
    {
    public:
        external_sync_mode(hw_monitor& hwm);
        virtual ~external_sync_mode() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Inter-camera synchronization mode: 0:Default, 1:Master, 2:Slave";
        }
        void enable_recording(std::function<void(const option &)> record_action) override
        {
            _record_action = record_action;
        }
    private:
        std::function<void(const option &)> _record_action = [](const option&) {};
        lazy<option_range> _range;
        hw_monitor& _hwm;
    };

    class emitter_on_and_off_option : public option
    {
    public:
        emitter_on_and_off_option(hw_monitor& hwm, sensor_base* depth_ep);
        virtual ~emitter_on_and_off_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override
        {
            return "Emitter On/Off Mode: 0:disabled(default), 1:enabled(emitter toggles between on and off). Can only be set before streaming";
        }
        virtual void enable_recording(std::function<void(const option &)> record_action) override {_record_action = record_action;}

    private:
        std::function<void(const option &)> _record_action = [](const option&) {};
        lazy<option_range> _range;
        hw_monitor& _hwm;
        sensor_base* _sensor;
    };

    class alternating_emitter_option : public option
    {
    public:
        alternating_emitter_option(hw_monitor& hwm, sensor_base* depth_ep);
        virtual ~alternating_emitter_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override { return *_range; }
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override
        {
            return "Alternating Emitter Pattern: 0:disabled(default), 1:enabled( emitter is toggled on/off on per-frame basis)";
        }
        virtual void enable_recording(std::function<void(const option &)> record_action) override { _record_action = record_action; }

    private:
        std::function<void(const option &)> _record_action = [](const option&) {};
        lazy<option_range> _range;
        hw_monitor& _hwm;
        sensor_base* _sensor;
    };

    struct dual_exposure
    {
        float exposure1;
        float gain1;
        float exposure2;
        float gain2;
    };
    using dual_exposure_field = float dual_exposure::*;
    inline bool operator==(const dual_exposure& a, const dual_exposure& b)
    {
        return a.exposure1 == b.exposure1 && a.exposure2 == b.exposure2 && a.gain1 == b.gain1 && a.gain2 == b.gain2;
    }

    class alternating_exposure_gain
    {
    public:
        alternating_exposure_gain(hw_monitor& hwm, 
            sensor_base* depth_ep, dual_exposure val) 
            : _value(val), _hwm(hwm), _sensor(depth_ep) {}

        float read(dual_exposure_field f) const;
        void write(dual_exposure_field f, float value);

    private:
        dual_exposure _value;
        dual_exposure _last;
        hw_monitor& _hwm;
        sensor_base* _sensor;
    };

    class alternating_exposure_gain_option : public option
    {
    public:
        alternating_exposure_gain_option(std::shared_ptr<alternating_exposure_gain> aeg, 
            dual_exposure_field f, option_range range) : _aeg(aeg), _f(f), _range(range) {}
        
        virtual ~alternating_exposure_gain_option() = default;
        virtual void set(float value) override { _aeg->write(_f, value); _record_action(*this); }
        virtual float query() const override { return _aeg->read(_f); }
        virtual option_range get_range() const override { return _range; }
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override { return "Test"; }
        virtual void enable_recording(std::function<void(const option &)> record_action) override { _record_action = record_action; }

    private:
        std::function<void(const option &)> _record_action = [](const option&) {};
        std::shared_ptr<alternating_exposure_gain> _aeg;
        dual_exposure_field _f;
        option_range _range;
    };
}
