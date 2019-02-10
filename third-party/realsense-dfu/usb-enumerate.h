/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#ifndef USBENUMERATE_H
#define USBENUMERATE_H

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
//#include <dirent.h>
#include <fcntl.h>
//#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
//#include <sys/mman.h>
//#include <sys/ioctl.h>
#include <algorithm>
#include <exception>

//#include <linux/usb/video.h>
//#include <linux/uvcvideo.h>
//#include <linux/videodev2.h>
#include "libusb.h"

#include "Misc.h"

#define APP_EXCEPTION std::exception

#define HW_MONITOR_COMMAND_SIZE         1024
#define HW_MONITOR_BUFFER_SIZE          (1000)

struct devinfo
{
	int busnum;
	int devnum;
	std::string serialnum;
};

struct context
{
	libusb_context * usb_context;
    int status;

	context()
	{
		status = libusb_init(&usb_context);
	}
	~context()
	{
		libusb_exit(usb_context);
	}
};

struct device
{
	const std::shared_ptr<context> parent;
	int vid, pid, busnum, devnum;
	uint8_t iface_num, ep_addr_in, ep_addr_out;
	uint8_t iserial;
	libusb_device * usb_device;
	libusb_device_handle * usb_handle;

	device(std::shared_ptr<context> parent) :
		parent(parent), usb_device(), usb_handle(), pid(0), vid(0), iserial(0) {}
};

class UsbEnumerate{
	public:
		UsbEnumerate(uint16_t vid, uint16_t pid,
				     uint8_t ifaceClass, uint8_t ifaceSubClass);
		UsbEnumerate(int busnum, int devnum, uint16_t vid,
					 const std::vector<uint16_t>& pids,
				     uint8_t ifaceClass, uint8_t ifaceSubClass);
		~UsbEnumerate();
		bool GetDevice(std::shared_ptr<device>& dev);
		static int IsDeviceExist(uint16_t vid, const std::vector<uint16_t>& pids, bool silent = false);
		static int IsDeviceExist(uint16_t vid, uint16_t pid, bool silent = true);
		static int NumDevicesExist(uint16_t vid, const std::vector<uint16_t>& pids, std::vector<devinfo>& info);
		bool GetDeviceSerialNumber(std::string& serialNumber);
        static const char* UsbEnumerate::usbSpeed(int speedCode);

	private:
		void ClaimInterface(libusb_device *dev,
				uint8_t ifaceClass, uint8_t ifaceSubClass);
		uint16_t _vid, _pid;
		int _busnum, _devnum;
		std::shared_ptr<device> _device;
};

#endif // USBENUMERATE_H
