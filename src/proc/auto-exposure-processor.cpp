// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#include "auto-exposure-processor.h"

librealsense::auto_exposure_processor::auto_exposure_processor(rs2_stream stream, 
    enable_auto_exposure_option& enable_ae_option_low,
    enable_auto_exposure_option& enable_ae_option_high)
    : auto_exposure_processor("Auto Exposure Processor", stream, enable_ae_option_low, enable_ae_option_high) {}

librealsense::auto_exposure_processor::auto_exposure_processor(const char * name, rs2_stream stream, 
    enable_auto_exposure_option& enable_ae_option_low, 
    enable_auto_exposure_option& enable_ae_option_high)
    : generic_processing_block(name),
      _stream(stream),
      _enable_ae_option_low(enable_ae_option_low), 
      _enable_ae_option_high(enable_ae_option_high) {}

bool librealsense::auto_exposure_processor::should_process(const rs2::frame & frame)
{
    return true;
}

rs2::frame librealsense::auto_exposure_processor::process_frame(const rs2::frame_source & source, const rs2::frame & f)
{
    if (f && f.supports_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE))
    {
        _frames[f.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE)] = f;
    }

    std::vector<rs2::frame> fss;
    for (auto&& kvp : _frames) fss.push_back(kvp.second);
    std::sort(fss.begin(), fss.end(),
        [](const rs2::frame& a, const rs2::frame& b) -> bool
    {
        return a.get_timestamp() > b.get_timestamp();
    });
    //fss.resize(2);
    if (fss.size() > 1) fss.resize(2);

    using namespace std::chrono;
    static system_clock::time_point last_time = system_clock::now();
    if (system_clock::now() - last_time > milliseconds(1000))
    {
        last_time = system_clock::now();

        for (auto&& f : fss)
        {
            LOG(WARNING) << f.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE) << " - " << std::fixed << f.get_timestamp();

        }
        LOG(WARNING) << "";
    }

    if (fss.size() == 2)
    {
        auto low = fss[0];
        auto high = fss[1];

        _frames.clear();
        _frames[high.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE)] = high;
        _frames[low.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE)] = low;

        if ( low.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE) > 
             high.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE) )
        {
            std::swap(low, high);
        }

        if (auto auto_exposure = _enable_ae_option_low.get_auto_exposure())
        {
            // We dont actually modify the frame, only calculate and process the exposure values.
            auto&& fi = (frame_interface*)low.get();
            ((librealsense::frame*)fi)->additional_data.fisheye_ae_mode = true;

            //fi->acquire();
            //auto_exposure->add_frame(fi);
        }

        if (auto auto_exposure = _enable_ae_option_high.get_auto_exposure())
        {
            // We dont actually modify the frame, only calculate and process the exposure values.
            auto&& fi = (frame_interface*)high.get();
            ((librealsense::frame*)fi)->additional_data.fisheye_ae_mode = true;

            //fi->acquire();
            //auto_exposure->add_frame(fi);
        }
    }

    return f;
}
