// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#pragma once

#include "gvd.h"

namespace librealsense
{
    namespace l500
    {
#pragma pack(1)
        typedef struct _rs_l500_gvd
        {
            number<uint16_t> StructureSize;
            number<uint8_t>   StructureVersion;
            number<uint8_t>   ProductType;
            number<uint8_t>   ProductID;
            number<uint8_t>   AdvancedModeEnabled;
            major_minor_version<uint8_t> AdvancedModeVersion;
            uint8_t p1[4];
            change_set_version FunctionalPayloadVersion;
            major_minor_version<uint8_t> EyeSafetyPayloadVersion;
            uint8_t p2[2];
            major_minor_version<uint8_t> DfuPayloadVersion;
            uint8_t p3[2];
            number<uint8_t>   FlashRoVersion;
            number<uint8_t>   FlashStatus;
            number<uint8_t>   FlashRwVersion;
            uint8_t p4[1];
            number<uint32_t> StrapState;
            number<uint32_t> OemId;
            number<uint32_t> oemVersion;
            number<uint16_t> MipiConfig;
            uint8_t p5[2];
            number<uint32_t> MipiFrequencies;
            uint8_t p6[12];
            serial<4> OpticModuleSerial;
            uint8_t p7[4];
            number<uint8_t>   OpticModuleVersion;
            uint8_t p8[1];
            number<uint32_t> OpticModuleMM;
            serial<6> AsicModuleSerial;
            uint8_t p9[2];
            serial<6> AsicModuleChipID;
            uint8_t p10[2];
            number<uint8_t>   AsicModuleVersion;
            uint8_t p11[3];
            number<uint32_t> AsicModuleMm;
            number<uint16_t> LeftSensorID;
            number<uint8_t>   LeftSensorVersion;
            uint8_t p12[9];
            number<uint16_t> RightSensorID;
            number<uint8_t>   RightSensorVersion;
            uint8_t p13[9];
            number<uint16_t> FishEyeSensorID;
            number<uint8_t>   FishEyeSensorVersion;
            uint8_t p14[9];
            number<uint8_t>   imuACCChipID;
            number<uint8_t>   imuGyroChipID;
            number<uint32_t> imuSTChipID;
            uint8_t p15[6];
            number<uint16_t> RgbModuleID;
            number<uint8_t>   RgbModuleVersion;
            uint8_t   padding26;
            serial<6> RgbModuleSN;
            uint16_t padding27;
            number<uint16_t> RgbIspFWVersion;
            uint32_t padding28;
            number<uint8_t>   WinUsbSupport;
            uint8_t   padding29;
            uint8_t   padding30;
            uint8_t   padding31;
            number<uint8_t>   HwType;
            uint8_t   padding32;
            uint8_t   padding33;
            uint8_t   padding34;
            number<uint8_t>   SkuComponent;
            uint8_t   padding35;
            uint8_t   padding36;
            uint8_t   padding37;
            number<uint8_t>   DepthCamera;
            uint8_t   padding38;
            uint8_t   padding39;
            uint8_t   padding40;
            number<uint8_t>   DepthActiveMode;
            uint8_t   padding41;
            uint8_t   padding42;
            uint8_t   padding43;
            number<uint8_t>   WithRGB;
            uint8_t   padding44;
            uint8_t   padding45;
            uint8_t   padding46;
            number<uint8_t>   WithImu;
            uint8_t   padding47;
            uint8_t   padding48;
            uint8_t   padding49;
            number<uint8_t>   ProjectorType;
            uint8_t   padding50;
            uint8_t   padding51;
            uint8_t   padding52;
            major_minor_version<uint8_t> EepromVersion;
            major_minor_version<uint8_t> EepromModuleInfoVersion;
            major_minor_version<uint8_t> AsicModuleInfoTableVersion;
            major_minor_version<uint8_t> CalibrationTableVersion;
            major_minor_version<uint8_t> CoefficientsVersion;
            major_minor_version<uint8_t> DepthVersion;
            major_minor_version<uint8_t> RgbVersion;
            major_minor_version<uint8_t> FishEyeVersion;
            major_minor_version<uint8_t> ImuVersion;
            major_minor_version<uint8_t> LensShadingVersion;
            major_minor_version<uint8_t> ProjectorVersion;
            major_minor_version<uint8_t> SNVersion;
            major_minor_version<uint8_t> ThermalLoopTableVersion;
            change_set_version MMFWVersion;
            serial<6> MMSN;
            uint16_t padding53;
            major_minor_version<uint16_t> MMVersion;
            number<uint8_t>   eepromLockStatus;
            uint8_t padding54;
        }rs_l500_gvd;
#pragma pack(pop)

    }
}
