/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#ifndef DFU_DS5_H
#define DFU_DS5_H

#include "dfu-base.h"

#define INTEL_CAMERAS_VID 0x8086

struct FW_STATUS
{
    uint32_t               spare1;
    uint32_t               FW_lastVersion;
    uint32_t               FW_highestVersion;
    uint16_t               FW_DownloadStatus;
    uint16_t               DFU_isLocked;
    uint16_t               DFU_version;
    SerialNumber           serialNum;
    uint8_t                spare2[42];
    std::string ToString() const;
};

class DFU_DS5 : public DFUBase
{
public:
	DFU_DS5();

    void UpdateFirmware(std::vector<uint8_t> data, ON_PROGRESS_FUNC userCallback, void* userContext = nullptr) override;
};

#endif // DFU_DS5_H
