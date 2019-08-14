// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <glad/glad.h>
#include "on-chip-calib.h"

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>
#include <model-views.h>
#include <viewer.h>

#include "../tools/depth-quality/depth-metrics.h"

namespace rs2
{
#pragma pack(push, 1)
#pragma pack(1)

#define UPDC32(octet, crc) (crc_32_tab[((crc) ^ (octet)) & 0xff] ^ ((crc) >> 8))

    static const uint32_t crc_32_tab[] = { /* CRC polynomial 0xedb88320 */
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    /// Calculate CRC code for arbitrary characters buffer
    uint32_t calc_crc32(const uint8_t *buf, size_t bufsize)
    {
        uint32_t oldcrc32 = 0xFFFFFFFF;
        for (; bufsize; --bufsize, ++buf)
            oldcrc32 = UPDC32(*buf, oldcrc32);
        return ~oldcrc32;
    }

    typedef struct
    {
        float rf[2];
        float rp[2];
        float k[5];
    }
    STCalibrationTableIntrinsics;

    typedef struct
    {
        float rf[2];
        float rp[2];
    }
    STCalibrationTableRectParams;

    typedef struct
    {
        uint16_t width;
        uint16_t height;
        STCalibrationTableRectParams recParams;
    }STCalibResRecParams;

    typedef struct
    {
        STCalibrationTableIntrinsics left;
        STCalibrationTableIntrinsics right;
        float    Rleft[9];
        float    Rright[9];
        float baseline;
        uint32_t useBrownModel;
        uint8_t    reserved1[88];
        //STCalibrationTableRectParams modesLR[CALIBRATION_NUM_OF_LR_MODES];
        //uint8_t    reserved2[96];
    } STCoeff;

    struct table_header
    {
        uint16_t                version;        // major.minor. Big-endian
        uint16_t                table_type;     // ctCalibration
        uint32_t                table_size;     // full size including: TOC header + TOC + actual tables
        uint32_t                param;          // This field content is defined ny table type
        uint32_t                crc32;          // crc of all the actual table data excluding header/CRC
    };

    enum rs2_dsc_status : uint16_t
    {
        RS2_DSC_STATUS_SUCCESS = 0, /**< Self calibration succeeded*/
        RS2_DSC_STATUS_RESULT_NOT_READY = 1, /**< Self calibration result is not ready yet*/
        RS2_DSC_STATUS_FILL_FACTOR_TOO_LOW = 2, /**< There are too little textures in the scene*/
        RS2_DSC_STATUS_EDGE_TOO_CLOSE = 3, /**< Self calibration range is too small*/
        RS2_DSC_STATUS_NOT_CONVERGE = 4, /**< For tare calibration only*/
        RS2_DSC_STATUS_BURN_SUCCESS = 5,
        RS2_DSC_STATUS_BURN_ERROR = 6,
        RS2_DSC_STATUS_NO_DEPTH_AVERAGE = 7,
    };

#define MAX_STEP_COUNT 256

    struct DirectSearchCalibrationResult
    {
        uint32_t opcode;
        uint16_t status;      // DscStatus
        uint16_t stepCount;
        uint16_t stepSize; // 1/1000 of a pixel
        uint32_t pixelCountThreshold; // minimum number of pixels in
                                      // selected bin
        uint16_t minDepth;  // Depth range for FWHM
        uint16_t maxDepth;
        uint32_t rightPy;   // 1/1000000 of normalized unit
        float healthCheck;
        float rightRotation[9]; // Right rotation
        uint16_t results[0]; // 1/100 of a percent
    };

    typedef struct
    {
        uint16_t m_status;
        float    m_healthCheck;
    } DscResultParams;

    typedef struct
    {
        uint16_t m_paramSize;
        DscResultParams m_dscResultParams;
        uint16_t m_tableSize;
    } DscResultBuffer;
#pragma pack(pop)

    void on_chip_calib_manager::stop_viewer()
    {
        auto profiles = _sub->get_selected_profiles();
        _sub->stop(_viewer);

        bool frame_arrived = false;
        while (frame_arrived && _viewer.streams.size())
        {
            for (auto&& stream : _viewer.streams)
            {
                if (std::find(profiles.begin(), profiles.end(),
                    stream.second.original_profile) != profiles.end())
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    if (now - stream.second.last_frame > std::chrono::milliseconds(200))
                        frame_arrived = false;
                }
                else frame_arrived = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    rs2::depth_frame on_chip_calib_manager::fetch_depth_frame()
    {
        auto profiles = _sub->get_selected_profiles();
        bool frame_arrived = false;
        rs2::depth_frame res = rs2::frame{};
        while (!frame_arrived)
        {
            for (auto&& stream : _viewer.streams)
            {
                if (std::find(profiles.begin(), profiles.end(),
                    stream.second.original_profile) != profiles.end())
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    if (now - stream.second.last_frame < std::chrono::milliseconds(100))
                    {
                        if (auto f = stream.second.texture->get_last_frame(false).as<rs2::depth_frame>())
                        {
                            frame_arrived = true;
                            res = f;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return res;
    }

    void on_chip_calib_manager::start_viewer(int w, int h, int fps)
    {
        if (_ui) _sub->ui = *_ui;

        _sub->ui.selected_format_id.clear();
        _sub->ui.selected_format_id[RS2_STREAM_DEPTH] = 0;

        for (int i = 0; i < _sub->shared_fps_values.size(); i++)
        {
            if (_sub->shared_fps_values[i] == fps)
                _sub->ui.selected_shared_fps_id = i;
        }

        for (int i = 0; i < _sub->res_values.size(); i++)
        {
            auto kvp = _sub->res_values[i];
            if (kvp.first == w && kvp.second == h)
                _sub->ui.selected_res_id = i;
        }

        auto profiles = _sub->get_selected_profiles();

        if (!_model.dev_syncer)
            _model.dev_syncer = _viewer.syncer->create_syncer();

        _sub->play(profiles, _viewer, _model.dev_syncer);
        for (auto&& profile : profiles)
        {
            _viewer.begin_stream(_sub, profile);
        }

        bool frame_arrived = false;
        while (!frame_arrived)
        {
            for (auto&& stream : _viewer.streams)
            {
                if (std::find(profiles.begin(), profiles.end(),
                    stream.second.original_profile) != profiles.end())
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    if (now - stream.second.last_frame < std::chrono::milliseconds(100))
                        frame_arrived = true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::pair<float, float> on_chip_calib_manager::get_metric(bool use_new)
    {
        return _metrics[use_new ? 1 : 0];
    }

    std::pair<float, float> on_chip_calib_manager::get_depth_metrics()
    {
        using namespace depth_quality;

        auto f = fetch_depth_frame();
        auto sensor = _sub->s->as<rs2::depth_stereo_sensor>();
        auto intr = f.get_profile().as<rs2::video_stream_profile>().get_intrinsics();
        rs2::region_of_interest roi { f.get_width() * 0.45f, f.get_height()  * 0.45f, 
                                      f.get_width() * 0.55f, f.get_height() * 0.55f };
        std::vector<single_metric_data> v;

        std::vector<float> fill_rates;
        std::vector<float> rmses;

        auto show_plane = _viewer.draw_plane;

        auto on_frame = [sensor, &fill_rates, &rmses, this](
            const std::vector<rs2::float3>& points,
            const plane p,
            const rs2::region_of_interest roi,
            const float baseline_mm,
            const float focal_length_pixels,
            const int ground_thruth_mm,
            const bool plane_fit,
            const float plane_fit_to_ground_truth_mm,
            const float distance_mm,
            bool record,
            std::vector<single_metric_data>& samples)
        {
            float TO_METERS = sensor.get_depth_scale();
            static const float TO_MM = 1000.f;
            static const float TO_PERCENT = 100.f;

            // Calculate fill rate relative to the ROI
            auto fill_rate = points.size() / float((roi.max_x - roi.min_x)*(roi.max_y - roi.min_y)) * TO_PERCENT;
            fill_rates.push_back(fill_rate);

            if (!plane_fit) return;

            std::vector<rs2::float3> points_set = points;
            std::vector<float> distances;

            // Reserve memory for the data
            distances.reserve(points.size());

            // Convert Z values into Depth values by aligning the Fitted plane with the Ground Truth (GT) plane
            // Calculate distance and disparity of Z values to the fitted plane.
            // Use the rotated plane fit to calculate GT errors
            for (auto point : points_set)
            {
                // Find distance from point to the reconstructed plane
                auto dist2plane = p.a*point.x + p.b*point.y + p.c*point.z + p.d;
                // Project the point to plane in 3D and find distance to the intersection point
                rs2::float3 plane_intersect = { float(point.x - dist2plane*p.a),
                    float(point.y - dist2plane*p.b),
                    float(point.z - dist2plane*p.c) };

                // Store distance, disparity and gt- error
                distances.push_back(dist2plane * TO_MM);
            }

            // Remove outliers [below 1% and above 99%)
            std::sort(points_set.begin(), points_set.end(), [](const rs2::float3& a, const rs2::float3& b) { return a.z < b.z; });
            size_t outliers = points_set.size() / 50;
            points_set.erase(points_set.begin(), points_set.begin() + outliers); // crop min 0.5% of the dataset
            points_set.resize(points_set.size() - outliers); // crop max 0.5% of the dataset

                                                             // Calculate Plane Fit RMS  (Spatial Noise) mm
            double plane_fit_err_sqr_sum = std::inner_product(distances.begin(), distances.end(), distances.begin(), 0.);
            auto rms_error_val = static_cast<float>(std::sqrt(plane_fit_err_sqr_sum / distances.size()));
            auto rms_error_val_per = TO_PERCENT * (rms_error_val / distance_mm);
            rmses.push_back(rms_error_val_per);
        };

        auto rms_std = 1000.f;
        auto new_rms_std = rms_std;
        auto count = 0;

        do
        {
            rms_std = new_rms_std;

            rmses.clear();

            for (int i = 0; i < 31; i++)
            {
                f = fetch_depth_frame();
                auto res = depth_quality::analyze_depth_image(f, sensor.get_depth_scale(), sensor.get_stereo_baseline(),
                    &intr, roi, 0, true, v, false, on_frame);

                _viewer.draw_plane = true;
                _viewer.roi_rect = res.plane_corners;
            }

            auto rmses_sum_sqr = std::inner_product(rmses.begin(), rmses.end(), rmses.begin(), 0.);
            new_rms_std = static_cast<float>(std::sqrt(rmses_sum_sqr / rmses.size()));
        } while ((new_rms_std < rms_std * 0.8f && new_rms_std > 10.f) && count++ < 10);

        std::sort(fill_rates.begin(), fill_rates.end());
        std::sort(rmses.begin(), rmses.end());

        auto median_fill_rate = fill_rates[fill_rates.size() / 2];
        auto median_rms = rmses[rmses.size() / 2];

        _viewer.draw_plane = show_plane;

        return { median_fill_rate, median_rms };
    }

    void on_chip_calib_manager::process_flow(std::function<void()> cleanup)
    {
        log(to_string() << "Starting calibration at speed " << _speed);

        _in_3d_view = _viewer.is_3d_view;
        _viewer.is_3d_view = true;

        debug_protocol dp = _dev;

        std::vector<uint8_t> fetch_calib{
            0x14, 0, 0xAB, 0xCD, 0x15, 0, 0, 0, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        auto calib = dp.send_and_receive_raw_data(fetch_calib);

        auto table = (STCoeff*)(calib.data() + 4 + sizeof(table_header));
        auto hd = (table_header*)(calib.data() + 4);
        auto crc = calc_crc32((uint8_t*)table, hd->table_size);
        _old_calib.resize(sizeof(table_header) + hd->table_size, 0);
        memcpy(_old_calib.data(), hd, _old_calib.size());

        auto ptr = (STCoeff*)(_old_calib.data() + sizeof(table_header));

        _was_streaming = _sub->streaming;
        if (!_was_streaming) 
        {
            start_viewer(0,0,0);
        }

        auto metrics_before = get_depth_metrics();
        _metrics.push_back(metrics_before);
        
        stop_viewer();

        _ui = std::make_shared<subdevice_ui_selection>(_sub->ui);
        _synchronized = _viewer.synchronization_enable.load();
        _post_processing = _sub->post_processing_enabled;
        _sub->post_processing_enabled = false;
        _viewer.synchronization_enable = false;
        
        start_viewer(256, 144, 90);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        uint8_t speed = 4 - _speed;

        DirectSearchCalibrationResult result;

        bool repeat = true;
        while (repeat)
        {

            std::vector<uint8_t> cmd =
            {
                0x14, 0x00, 0xab, 0xcd,
                0x80, 0x00, 0x00, 0x00,
                0x08, 0x00, 0x00, 0x00,
                speed, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
            };

            auto res = dp.send_and_receive_raw_data(cmd);

            memset(&result, 0, sizeof(DirectSearchCalibrationResult));

            int count = 0;
            bool done = false;
            do
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                cmd =
                {
                    0x14, 0x00, 0xab, 0xcd,
                    0x80, 0x00, 0x00, 0x00,
                    0x03, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00
                };

                res = dp.send_and_receive_raw_data(cmd);
                int32_t code = *((int32_t*)res.data());

                if (res.size() >= sizeof(DirectSearchCalibrationResult))
                {
                    result = *reinterpret_cast<DirectSearchCalibrationResult*>(res.data());
                    done = result.status != RS2_DSC_STATUS_RESULT_NOT_READY;
                }

                _progress = count * (2 * _speed);

            } while (count++ < 200 && !done);

            if (!done)
            {
                throw std::runtime_error("Timeout!");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (result.status != RS2_DSC_STATUS_EDGE_TOO_CLOSE)
            {
                repeat = false;
            }
            else
            {
                log("Edge to close... Slowing down");
                speed++;
                if (speed > 4) repeat = false;
            }
        }

        if (result.status != RS2_DSC_STATUS_SUCCESS)
        {
            throw std::runtime_error(to_string() << "Status = " << result.status);
        }

        std::vector<uint8_t> cmd =
        {
            0x14, 0x00, 0xab, 0xcd,
            0x80, 0x00, 0x00, 0x00,
            13, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };

        auto res = dp.send_and_receive_raw_data(cmd);
        int32_t code = *((int32_t*)res.data());
        if (res.size() >= sizeof(DscResultBuffer))
        {
            DscResultBuffer* result = reinterpret_cast<DscResultBuffer*>(res.data() + 4);
            table_header* header = reinterpret_cast<table_header*>(res.data() + 4 + sizeof(DscResultBuffer));

            _new_calib.resize(sizeof(table_header) + header->table_size, 0);
            memcpy(_new_calib.data(), header, _new_calib.size());

            STCoeff* table = reinterpret_cast<STCoeff*>(_new_calib.data() + sizeof(table_header));
        }

        _health = abs(result.healthCheck);

        log(to_string() << "Calibration completed, health factor = " << _health);

        stop_viewer();

        start_viewer(0, 0, 0); // Start with default settings

        apply_calib(true);

        auto metrics_after = get_depth_metrics();
        _metrics.push_back(metrics_after);

        _progress = 100;

        _done = true;
    }

    void on_chip_calib_manager::restore_workspace()
    {
        _viewer.is_3d_view = _in_3d_view;

        _viewer.synchronization_enable = _synchronized;

        stop_viewer();

        _sub->ui = *_ui;
        _ui.reset();

        _sub->post_processing_enabled = _post_processing;

        if (_was_streaming) start_viewer(0, 0, 0);
    }

    void on_chip_calib_manager::keep()
    {
        uint16_t size = (uint16_t)(0x14 + _new_calib.size());

        std::vector<uint8_t> save_calib{
            0x14, 0, 0xAB, 0xCD, 0x16, 0, 0, 0, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        auto up = (uint16_t*)save_calib.data();
        up[0] = size;

        save_calib.insert(save_calib.end(), _new_calib.data(), _new_calib.data() + _new_calib.size());

        debug_protocol dp = _dev;
        auto res = dp.send_and_receive_raw_data(save_calib);

    }

    void on_chip_calib_manager::apply_calib(bool use_new)
    {
        //use_new = false;

        table_header* hd = (table_header*)(use_new ? _new_calib.data() : _old_calib.data());
        STCoeff* table = (STCoeff*)((use_new ? _new_calib.data() : _old_calib.data()) + sizeof(table_header));

        uint16_t size = (uint16_t)(0x14 + hd->table_size);

        std::vector<uint8_t> apply_calib{
            0x14, 0, 0xAB, 0xCD, 0x51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        auto up = (uint16_t*)apply_calib.data();
        up[0] = size;

        apply_calib.insert(apply_calib.end(), (uint8_t*)table, ((uint8_t*)table) + hd->table_size);

        debug_protocol dp = _dev;
        auto res = dp.send_and_receive_raw_data(apply_calib);

    }

    void autocalib_notification_model::draw_content(ux_window& win, int x, int y, float t, std::string& error_message)
    {
        using namespace std;
        using namespace chrono;

        const auto bar_width = width - 115;

        ImGui::SetCursorScreenPos({ float(x + 9), float(y + 4) });

        ImVec4 shadow{ 1.f, 1.f, 1.f, 0.1f };
        ImGui::GetWindowDrawList()->AddRectFilled({ float(x), float(y) },
        { float(x + width), float(y + 25) }, ImColor(shadow));

        if (update_state != RS2_CALIB_STATE_COMPLETE)
        {
            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
                ImGui::Text("Calibration Health-Check");

            if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS ||
                update_state == RS2_CALIB_STATE_CALIB_COMPLETE)
                ImGui::Text("On-Chip Calibration");

            ImGui::SetCursorScreenPos({ float(x + 9), float(y + 27) });

            ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 1. - t));

            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
            {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

                ImGui::Text("Following devices support On-Chip Calibration:");
                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 47) });

                ImGui::PushStyleColor(ImGuiCol_Text, white);
                ImGui::Text(message.c_str());
                ImGui::PopStyleColor();

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 65) });
                ImGui::Text("Run quick calibration Health-Check? (~30 sec)");
            }
            else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
            {
                enable_dismiss = false;
                ImGui::Text("Camera is being calibrated...\nKeep the camera stationary pointing at a wall");
            }
            else if (update_state == RS2_CALIB_STATE_CALIB_COMPLETE)
            {
                auto health = get_manager().get_health();

                auto recommend_keep = health > 0.1f;

                ImGui::SetCursorScreenPos({ float(x + 15), float(y + 33) });

                if (!recommend_keep) ImGui::Text("Camera original calibration is OK");
                else if (health > 0.2f) ImGui::Text("We found much better calibration!"); 
                else ImGui::Text("We found better calibration for the device!");
                
                auto old_fr = get_manager().get_metric(false).first;
                auto new_fr = get_manager().get_metric(true).first;

                auto old_rms = fabs(get_manager().get_metric(false).second);
                auto new_rms = fabs(get_manager().get_metric(true).second);

                auto fr_improvement = 100.f * ((new_fr - old_fr) / old_fr);
                auto rms_improvement = 100.f * ((old_rms - new_rms) / old_rms);

                std::string old_units = "mm";
                if (old_rms > 10.f)
                {
                    old_rms /= 10.f;
                    old_units = "cm";
                }
                std::string new_units = "mm";
                if (new_rms > 10.f)
                {
                    new_rms /= 10.f;
                    new_units = "cm";
                }

                if (fr_improvement > 1.f)
                {
                    std::string txt = to_string() << "  Fill-Rate: " << std::setprecision(1) << std::fixed << new_fr << "%%";

                    if (!use_new_calib)
                    {
                        txt = to_string() << "  Fill-Rate: " << std::setprecision(1) << std::fixed << old_fr << "%%\n";
                    }

                    ImGui::SetCursorScreenPos({ float(x + 12), float(y + 90) });
                    ImGui::PushFont(win.get_large_font());
                    ImGui::Text(textual_icons::check);
                    ImGui::PopFont();

                    ImGui::SetCursorScreenPos({ float(x + 35), float(y + 92) });
                    ImGui::Text(txt.c_str());

                    if (use_new_calib)
                    {
                        ImGui::SameLine();

                        ImGui::PushStyleColor(ImGuiCol_Text, white);
                        txt = to_string() << " ( +" << std::fixed << std::setprecision(0) << fr_improvement << "%% )";
                        ImGui::Text(txt.c_str());
                        ImGui::PopStyleColor();
                    }

                    if (rms_improvement > 1.f)
                    {
                        if (use_new_calib)
                        {
                            txt = to_string() << "  Noise Estimate: " << std::setprecision(2) << std::fixed << new_rms << new_units;
                        }
                        else
                        {
                            txt = to_string() << "  Noise Estimate: " << std::setprecision(2) << std::fixed << old_rms << old_units;
                        }

                        ImGui::SetCursorScreenPos({ float(x + 12), float(y + 90 + ImGui::GetTextLineHeight() + 6) });
                        ImGui::PushFont(win.get_large_font());
                        ImGui::Text(textual_icons::check);
                        ImGui::PopFont();

                        ImGui::SetCursorScreenPos({ float(x + 35), float(y + 92 + ImGui::GetTextLineHeight() + 6) });
                        ImGui::Text(txt.c_str());

                        if (use_new_calib)
                        {
                            ImGui::SameLine();

                            ImGui::PushStyleColor(ImGuiCol_Text, white);
                            txt = to_string() << " ( -" << std::setprecision(0) << std::fixed << rms_improvement << "%% )";
                            ImGui::Text(txt.c_str());
                            ImGui::PopStyleColor();
                        }
                    }
                }
                else
                {
                    ImGui::SetCursorScreenPos({ float(x + 12), float(y + 100) });
                    ImGui::Text("Please compare new vs old calibration\nand decide if to keep or discard the result...");
                }

                ImGui::SetCursorScreenPos({ float(x + 9), float(y + 60) });

                if (ImGui::RadioButton("New", use_new_calib))
                {
                    use_new_calib = true;
                    get_manager().apply_calib(true);
                }

                ImGui::SetCursorScreenPos({ float(x + 150), float(y + 60) });
                if (ImGui::RadioButton("Original", !use_new_calib))
                {
                    use_new_calib = false;
                    get_manager().apply_calib(false);
                }

                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;

                if (recommend_keep)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                }

                std::string button_name = to_string() << "Apply New" << "##apply" << index;
                if (!use_new_calib) button_name = to_string() << "Keep Original" << "##original" << index;

                ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });
                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }))
                {
                    if (use_new_calib)
                    {
                        get_manager().keep();
                        update_state = RS2_CALIB_STATE_COMPLETE;
                        pinned = false;
                        enable_dismiss = false;
                        last_progress_time = last_interacted = system_clock::now() + milliseconds(500);
                    }
                    else dismissed = true;

                    get_manager().restore_workspace();
                }

                if (recommend_keep) ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", "New calibration values will be saved in device memory");
                }
            }

            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::Text("Calibration Complete");

            ImGui::SetCursorScreenPos({ float(x + 10), float(y + 35) });
            ImGui::PushFont(win.get_large_font());
            std::string txt = to_string() << textual_icons::throphy;
            ImGui::Text("%s", txt.c_str());
            ImGui::PopFont();

            ImGui::SetCursorScreenPos({ float(x + 40), float(y + 35) });

            ImGui::Text("Camera Calibration Applied Successfully");
        }

        ImGui::SetCursorScreenPos({ float(x + 5), float(y + height - 25) });

        if (update_manager)
        {
            if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT)
            {
                auto sat = 1.f + sin(duration_cast<milliseconds>(system_clock::now() - created_time).count() / 700.f) * 0.1f;
                ImGui::PushStyleColor(ImGuiCol_Button, saturate(sensor_header_light_blue, sat));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, saturate(sensor_header_light_blue, 1.5f));
                std::string button_name = to_string() << "Health-Check" << "##health_check" << index;

                if (ImGui::Button(button_name.c_str(), { float(bar_width), 20.f }) || update_manager->started())
                {
                    if (!update_manager->started()) update_manager->start(shared_from_this());

                    update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;
                    enable_dismiss = false;
                    last_progress_time = system_clock::now();
                }
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", "Keep the camera pointing at an object or a wall");
                }
            }
            else if (update_state == RS2_CALIB_STATE_CALIB_IN_PROCESS)
            {
                if (update_manager->done())
                {
                    update_state = RS2_CALIB_STATE_CALIB_COMPLETE;
                    enable_dismiss = true;
                    get_manager().apply_calib(true);
                    use_new_calib = true;
                }

                if (!expanded)
                {
                    if (update_manager->failed())
                    {
                        update_manager->check_error(error_message);
                        update_state = RS2_CALIB_STATE_FAILED;
                        pinned = false;
                        dismissed = true;
                    }

                    draw_progress_bar(win, bar_width);

                    ImGui::SetCursorScreenPos({ float(x + width - 105), float(y + height - 25) });

                    string id = to_string() << "Expand" << "##" << index;
                    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                    if (ImGui::Button(id.c_str(), { 100, 20 }))
                    {
                        expanded = true;
                    }

                    ImGui::PopStyleColor();
                }
            }
        }
    }

    void autocalib_notification_model::draw_expanded(ux_window& win, std::string& error_message)
    {
        if (update_manager->started() && update_state == RS2_CALIB_STATE_INITIAL_PROMPT) 
            update_state = RS2_CALIB_STATE_CALIB_IN_PROCESS;

        auto flags = ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0, 0, 0, 0 });
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(500, 100));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

        std::string title = "On-Chip Calibration";
        if (update_manager->failed()) title += " Failed";

        ImGui::OpenPopup(title.c_str());
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, flags))
        {
            ImGui::SetCursorPosX(200);
            std::string progress_str = to_string() << "Progress: " << update_manager->get_progress() << "%";
            ImGui::Text("%s", progress_str.c_str());

            ImGui::SetCursorPosX(5);

            draw_progress_bar(win, 490);

            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
            auto s = update_manager->get_log();
            ImGui::InputTextMultiline("##autocalib_log", const_cast<char*>(s.c_str()),
                s.size() + 1, { 490,100 }, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(190);
            if (visible || update_manager->done() || update_manager->failed())
            {
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    if (update_manager->failed())
                    {
                        update_state = RS2_CALIB_STATE_FAILED;
                        pinned = false;
                        dismissed = true;
                    }
                    expanded = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
                ImGui::PushStyleColor(ImGuiCol_Text, transparent);
                ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, transparent);
                ImGui::Button("OK", ImVec2(120, 0));
                ImGui::PopStyleColor(5);
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);

        error_message = "";
    }

    int autocalib_notification_model::calc_height()
    {
        if (update_state == RS2_CALIB_STATE_COMPLETE) return 65;
        else if (update_state == RS2_CALIB_STATE_INITIAL_PROMPT) return 120;
        else if (update_state == RS2_CALIB_STATE_CALIB_COMPLETE) return 170;
        else return 100;
    }

    void autocalib_notification_model::set_color_scheme(float t) const
    {
        notification_model::set_color_scheme(t);

        ImGui::PopStyleColor(1);

        ImVec4 c;

        if (update_state == RS2_CALIB_STATE_COMPLETE)
        {
            c = alpha(saturate(light_blue, 0.7f), 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
        else
        {
            c = alpha(sensor_bg, 1 - t);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
        }
    }

    autocalib_notification_model::autocalib_notification_model(std::string name,
        std::shared_ptr<on_chip_calib_manager> manager, bool exp)
        : process_notification_model(manager)
    {
        enable_expand = false;
        enable_dismiss = true;
        expanded = exp;
        if (expanded) visible = false;

        message = name;
        this->severity = RS2_LOG_SEVERITY_INFO;
        this->category = RS2_NOTIFICATION_CATEGORY_HARDWARE_EVENT;

        pinned = true;
    }
}
