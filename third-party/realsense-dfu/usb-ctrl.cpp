/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#include "usb-ctrl.h"
#include <iostream>

// For use in DFU mode 
LibUsbControl::LibUsbControl(uint16_t vid, uint16_t pid, uint8_t ifaceClass, uint8_t ifaceSubClass)
{
	_usbEnumerate = std::unique_ptr<UsbEnumerate>(new UsbEnumerate(vid, pid, ifaceClass, ifaceSubClass));
	_usbEnumerate->GetDevice(_device);
}

// For use in operational mode 
LibUsbControl::LibUsbControl(int busnum, int devnum, uint16_t vid,
                             const std::vector<uint16_t>& pids,
                             uint8_t ifaceClass, uint8_t ifaceSubClass)
{
	_usbEnumerate = std::unique_ptr<UsbEnumerate>(new
			UsbEnumerate(busnum, devnum, vid, pids, ifaceClass, ifaceSubClass));
	_usbEnumerate->GetDevice(_device);
}

LibUsbControl::~LibUsbControl()
{
}

void LibUsbControl::ControlTransfer(LIBUSB_CTRL_EP_PACKET& ctp, unsigned int timeout)
{
	auto sts = libusb_control_transfer(_device->usb_handle,
			ctp.RequestType,
			ctp.Request,
			ctp.Value,
			ctp.Index | _device->iface_num,
			ctp.Data,
			ctp.Length,
			timeout);

	// TODO: Itay Carpis: Check why USB2 returns -1 and -4 when not ready
	if (sts == -99 || sts == -1 || sts == -4 || sts == -9)
		return;
	if (sts < 0)
		throw APP_EXCEPTION(to_string() << "libusb_control_transfer failed. err = " << sts << " - " << std::string(libusb_error_name(sts)));
}

int LibUsbControl::InterruptTransfer(unsigned char *buf, int bufLen)
{
	int transferred = 0;

	if (!buf)
		throw APP_EXCEPTION("Invalid input params");

	auto sts = libusb_interrupt_transfer(_device->usb_handle,
			_device->ep_addr_in,
			buf,
			bufLen,
			&transferred,
			500);

	if (sts < 0 && sts != LIBUSB_ERROR_TIMEOUT)
		throw APP_EXCEPTION(to_string() << "libusb_interrupt_transfer failed. err = "
					<< sts << " - " << std::string(libusb_error_name(sts)));

	return transferred;
}

bool LibUsbControl::GetSerialNumber(std::string& serialNumber)
{
	return _usbEnumerate->GetDeviceSerialNumber(serialNumber);
}
