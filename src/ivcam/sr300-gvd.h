// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#pragma once

#include "gvd.h"

namespace librealsense
{
    namespace ivcam
    {
#pragma pack(1)
        typedef struct _rs_sr300_gvd
        {
            change_set_version_padded FunctionalPayloadVersion;
            major_minor_version<uint8_t> ChipVersion;
            number<uint32_t> AsicVersion;
            change_set_version_padded CoreVersion;
            change_set_version_padded UsbVersion;
            change_set_version_padded DriversVersion;
            change_set_version_padded MEMSVersion;
            change_set_version_padded DFUVersion;
            number<uint32_t> LiguriaApiVersion;
            major_minor_version<uint8_t> FlashRwVersion;
            major_minor_version<uint8_t> OEMVersion;
            major_minor_version<uint8_t> CalibVersion;
            major_minor_version<uint8_t> MlBinVersion;
            major_minor_version<uint8_t> ProjPatternVersion;
            major_minor_version<uint8_t> FlashRoVersion;
            major_minor_version<uint8_t> RgbSensorTblVersion;
            major_minor_version<uint8_t> IrSensorTblVersion;
            major_minor_version<uint8_t> PmbVersion;
            major_minor_version<uint8_t> ClibRoVersion;
            major_minor_version<uint8_t> ProjGainVersion;
            number<uint8_t> ExternalIsp;
            number<uint8_t> HwType;
            major_minor_version<uint8_t> LDVersion;
            major_minor_version<uint8_t> PMBHwversion;
            major_minor_version_padding<uint8_t,4> IrHWVersion;
            number<uint32_t> RgbSensorVersion;
            number<uint16_t>  RgbIspVersion;
            number<uint16_t>  RgbIspFWVersion;
            uint8_t AsicId[32];
            serial<6> ModuleSerialVersion;
            number<uint8_t> IrPixelVer;
            major_minor_version<uint8_t> OEMUnLockVersion;
            number<uint8_t> OemLock;
            number<uint32_t> OemId;
            number<uint16_t>  IctTesterMode;
            number<uint16_t>  PmbTesterMode;
            number<uint16_t>  SendorTesterMode;
            number<uint16_t>  IvcamTesterMode;
            number<uint32_t> StarpState;
            number<uint8_t> Locked;
            number<uint8_t> EngLabMode;
            number<uint32_t> MmNumber;
        }rs_sr300_gvd;
#pragma pack(pop)
    }
}
