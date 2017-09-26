/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2017 Intel Corporation. All Rights Reserved. */

#ifndef R4XX_ADVANCED_MODE_HPP
#define R4XX_ADVANCED_MODE_HPP

#include "rs.hpp"
#include "rs_advanced_mode.h"

namespace rs400
{
    class advanced_mode : public rs2::device
    {
    public:
        advanced_mode(rs2::device d)
                : rs2::device(d.get())
        {
            if(rs2_is_device_extendable_to(_dev.get(), RS2_EXTENSION_ADVANCED_MODE, rs2::handle_error()) == 0)
            {
                _dev = nullptr;
            }
        }

        void toggle_advanced_mode(bool enable)
        {
            rs2_toggle_advanced_mode(_dev.get(), enable, rs2::handle_error());
        }

        bool is_enabled() const
        {
            int enabled = 0;
            rs2_is_enabled(_dev.get(), &enabled, rs2::handle_error());
            return !!enabled;
        }

        void set_depth_control(STDepthControlGroup& group)
        {
            rs2_set_depth_control(_dev.get(), &group, rs2::handle_error());
        }

        STDepthControlGroup get_depth_control(int mode = 0) const
        {
            STDepthControlGroup group{};
            rs2_get_depth_control(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_rsm(STRsm& group)
        {
            rs2_set_rsm(_dev.get(), &group, rs2::handle_error());
        }

        STRsm get_rsm(int mode = 0) const
        {
            STRsm group{};
            rs2_get_rsm(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_rau_support_vector_control(STRauSupportVectorControl& group)
        {
            rs2_set_rau_support_vector_control(_dev.get(), &group, rs2::handle_error());
        }

        STRauSupportVectorControl get_rau_support_vector_control(int mode = 0) const
        {
            STRauSupportVectorControl group{};
            rs2_get_rau_support_vector_control(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_color_control(STColorControl& group)
        {
            rs2_set_color_control(_dev.get(),  &group, rs2::handle_error());
        }

        STColorControl get_color_control(int mode = 0) const
        {
            STColorControl group{};
            rs2_get_color_control(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_rau_thresholds_control(STRauColorThresholdsControl& group)
        {
            rs2_set_rau_thresholds_control(_dev.get(), &group, rs2::handle_error());
        }

        STRauColorThresholdsControl get_rau_thresholds_control(int mode = 0) const
        {
            STRauColorThresholdsControl group{};
            rs2_get_rau_thresholds_control(_dev.get(), &group, mode, rs2::handle_error());

            return group;
        }

        void set_slo_color_thresholds_control(STSloColorThresholdsControl& group)
        {
            rs2_set_slo_color_thresholds_control(_dev.get(), &group, rs2::handle_error());
        }

        STSloColorThresholdsControl get_slo_color_thresholds_control(int mode = 0) const
        {
            STSloColorThresholdsControl group{};
            rs2_get_slo_color_thresholds_control(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_slo_penalty_control(STSloPenaltyControl& group)
        {
            rs2_set_slo_penalty_control(_dev.get(), &group, rs2::handle_error());
        }

        STSloPenaltyControl get_slo_penalty_control(int mode = 0) const
        {
            STSloPenaltyControl group{};
            rs2_get_slo_penalty_control(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_hdad(STHdad& group)
        {
            rs2_set_hdad(_dev.get(), &group, rs2::handle_error());
        }

        STHdad get_hdad(int mode = 0) const
        {
            STHdad group{};
            rs2_get_hdad(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_color_correction(STColorCorrection& group)
        {
            rs2_set_color_correction(_dev.get(), &group, rs2::handle_error());
        }

        STColorCorrection get_color_correction(int mode = 0) const
        {
            STColorCorrection group{};
            rs2_get_color_correction(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_depth_table(STDepthTableControl& group)
        {
            rs2_set_depth_table(_dev.get(), &group, rs2::handle_error());
        }

        STDepthTableControl get_depth_table(int mode = 0) const
        {
            STDepthTableControl group{};
            rs2_get_depth_table(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_ae_control(STAEControl& group)
        {
            rs2_set_ae_control(_dev.get(), &group, rs2::handle_error());
        }

        STAEControl get_ae_control(int mode = 0) const
        {
            STAEControl group{};
            rs2_get_ae_control(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        void set_census(STCensusRadius& group)
        {
            rs2_set_census(_dev.get(), &group, rs2::handle_error());
        }

        STCensusRadius get_census(int mode = 0) const
        {
            STCensusRadius group{};
            rs2_get_census(_dev.get(), &group, mode, rs2::handle_error());
            return group;
        }

        std::string serialize_json() const
        {
            std::string results;
            std::shared_ptr<rs2_raw_data_buffer> json_data(
                    rs2_serialize_json(_dev.get(), rs2::handle_error()),
                    rs2_delete_raw_data);

            auto size = rs2_get_raw_data_size(json_data.get(), rs2::handle_error());
            auto start = rs2_get_raw_data(json_data.get(), rs2::handle_error());

            results.insert(results.begin(), start, start + size);
            return results;
        }

        void load_json(const std::string& json_content)
        {
            rs2_load_json(_dev.get(),
                          json_content.data(),
                          json_content.size(),
                          rs2::handle_error());
        }
    };
}

inline std::ostream & operator << (std::ostream & o, rs2_rs400_visual_preset preset) { return o << rs2_rs400_visual_preset_to_string(preset); }

inline bool operator==(const STDepthControlGroup& a, const STDepthControlGroup& b)
{
    return (a.plusIncrement == b.plusIncrement &&
        a.minusDecrement == b.minusDecrement &&
        a.deepSeaMedianThreshold == b.deepSeaMedianThreshold &&
        a.scoreThreshA == b.scoreThreshA &&
        a.scoreThreshB == b.scoreThreshB &&
        a.textureDifferenceThreshold == b.textureDifferenceThreshold &&
        a.textureCountThreshold == b.textureCountThreshold &&
        a.deepSeaSecondPeakThreshold == b.deepSeaSecondPeakThreshold &&
        a.deepSeaNeighborThreshold == b.deepSeaNeighborThreshold &&
        a.lrAgreeThreshold == b.lrAgreeThreshold);
}

inline bool operator==(const STRsm& a, const STRsm& b)
{
    return (a.rsmBypass == b.rsmBypass        &&
        a.diffThresh == b.diffThresh       &&
        a.sloRauDiffThresh == b.sloRauDiffThresh &&
        a.removeThresh == b.removeThresh);
}

inline bool operator==(const STRauSupportVectorControl& a, const STRauSupportVectorControl& b)
{
    return (a.minWest == b.minWest  &&
        a.minEast == b.minEast  &&
        a.minWEsum == b.minWEsum &&
        a.minNorth == b.minNorth &&
        a.minSouth == b.minSouth &&
        a.minNSsum == b.minNSsum &&
        a.uShrink == b.uShrink  &&
        a.vShrink == b.vShrink);
}

inline bool operator==(const STColorControl& a, const STColorControl& b)
{
    return (a.disableSADColor == b.disableSADColor      &&
        a.disableRAUColor == b.disableRAUColor      &&
        a.disableSLORightColor == b.disableSLORightColor &&
        a.disableSLOLeftColor == b.disableSLOLeftColor  &&
        a.disableSADNormalize == b.disableSADNormalize);
}

inline bool operator==(const STRauColorThresholdsControl& a, const STRauColorThresholdsControl& b)
{
    return (a.rauDiffThresholdRed == b.rauDiffThresholdRed   &&
        a.rauDiffThresholdGreen == b.rauDiffThresholdGreen &&
        a.rauDiffThresholdBlue == b.rauDiffThresholdBlue);
}

inline bool operator==(const STSloColorThresholdsControl& a, const STSloColorThresholdsControl& b)
{
    return (a.diffThresholdRed == b.diffThresholdRed   &&
        a.diffThresholdGreen == b.diffThresholdGreen &&
        a.diffThresholdBlue == b.diffThresholdBlue);
}

inline bool operator==(const STSloPenaltyControl& a, const STSloPenaltyControl& b)
{
    return (a.sloK1Penalty == b.sloK1Penalty     &&
        a.sloK2Penalty == b.sloK2Penalty     &&
        a.sloK1PenaltyMod1 == b.sloK1PenaltyMod1 &&
        a.sloK2PenaltyMod1 == b.sloK2PenaltyMod1 &&
        a.sloK1PenaltyMod2 == b.sloK1PenaltyMod2 &&
        a.sloK2PenaltyMod2 == b.sloK2PenaltyMod2);
}

inline bool operator==(const STHdad& a, const STHdad& b)
{
    return (a.lambdaCensus == b.lambdaCensus &&
        a.lambdaAD == b.lambdaAD     &&
        a.ignoreSAD == b.ignoreSAD);
}

inline bool operator==(const STColorCorrection& a, const STColorCorrection& b)
{
    return (a.colorCorrection1 == b.colorCorrection1  &&
        a.colorCorrection2 == b.colorCorrection2  &&
        a.colorCorrection3 == b.colorCorrection3  &&
        a.colorCorrection4 == b.colorCorrection4  &&
        a.colorCorrection5 == b.colorCorrection5  &&
        a.colorCorrection6 == b.colorCorrection6  &&
        a.colorCorrection7 == b.colorCorrection7  &&
        a.colorCorrection8 == b.colorCorrection8  &&
        a.colorCorrection9 == b.colorCorrection9  &&
        a.colorCorrection10 == b.colorCorrection10 &&
        a.colorCorrection11 == b.colorCorrection11 &&
        a.colorCorrection12 == b.colorCorrection12);
}

inline bool operator==(const STAEControl& a, const STAEControl& b)
{
    return (a.meanIntensitySetPoint == b.meanIntensitySetPoint);
}

inline bool operator==(const STDepthTableControl& a, const STDepthTableControl& b)
{
    return (a.depthUnits == b.depthUnits     &&
        a.depthClampMin == b.depthClampMin  &&
        a.depthClampMax == b.depthClampMax  &&
        a.disparityMode == b.disparityMode  &&
        a.disparityShift == b.disparityShift);
}

inline bool operator==(const STCensusRadius& a, const STCensusRadius& b)
{
    return (a.uDiameter == b.uDiameter &&
        a.vDiameter == b.vDiameter);
}


#endif // R4XX_ADVANCED_MODE_HPP
