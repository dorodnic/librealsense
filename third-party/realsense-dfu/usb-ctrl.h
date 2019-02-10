/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#ifndef LIBUSBCONTROL_H
#define LIBUSBCONTROL_H

#include "usb-enumerate.h"

struct LIBUSB_CTRL_EP_PACKET {
    unsigned char   RequestType;
    unsigned char   Request;
    unsigned short  Value;
    unsigned short  Index;
    unsigned char   Data[HW_MONITOR_COMMAND_SIZE + 1] = {0};
    unsigned short  Length;
};

class LibUsbControl
{
public:
    LibUsbControl(uint16_t vid, uint16_t uint16_t,
			      uint8_t ifaceClass, uint8_t ifaceSubClass);
    LibUsbControl(int busnum, int devnum, uint16_t vid, const std::vector<uint16_t>& pids,
			      uint8_t ifaceClass, uint8_t ifaceSubClass);

    ~LibUsbControl();
	void ControlTransfer(LIBUSB_CTRL_EP_PACKET& ctp, unsigned int timeout = 0);
    int InterruptTransfer(unsigned char *buf, int bufLen);
    bool GetSerialNumber(std::string& serialNumber);

private:
    std::shared_ptr<device> _device;
    std::unique_ptr<UsbEnumerate> _usbEnumerate;
};

#endif // LIBUSBCONTROL_H
