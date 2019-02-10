/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#include "usb-enumerate.h"
#include "misc.h"

void UsbEnumerate::ClaimInterface(libusb_device *dev, uint8_t ifaceClass, uint8_t ifaceSubClass)
{
	int ret, i, j, k;
	struct libusb_config_descriptor *cfg_desc;

	ret = libusb_get_config_descriptor(dev, 0, &cfg_desc);
	if (ret)
		throw APP_EXCEPTION(to_string()
			<< "libusb_get_config_descriptor(...) returned " << ret << " - " << std::string(libusb_error_name(ret)));

	bool found = false;
	for (i = 0; i < cfg_desc->bNumInterfaces; i++) {
		const struct libusb_interface *iface = &cfg_desc->interface[i];

		for (j = 0; j < iface->num_altsetting; j++) {
			const struct libusb_interface_descriptor *ifdesc;
			ifdesc = &iface->altsetting[j];

			if (!(ifdesc->bInterfaceClass == ifaceClass
				&& ifdesc->bInterfaceSubClass == ifaceSubClass))
				continue;

			found = true;

			_device->iface_num = ifdesc->bInterfaceNumber;

			for (k = 0; k < ifdesc->bNumEndpoints; k++) {
				const struct libusb_endpoint_descriptor *ept;
				ept = &ifdesc->endpoint[k];

				if (ept->bEndpointAddress & LIBUSB_ENDPOINT_IN)
					_device->ep_addr_in =
						ept->bEndpointAddress;
				else
					_device->ep_addr_out =
						ept->bEndpointAddress;
			}

			break;
		}

		if (found)
			break;
	}

	if (found) {
		/*
		 * Tell libusb to detach any active kernel drivers. libusb will keep track
		 * of whether it found a kernel driver for this interface.
		 */
		int status = libusb_detach_kernel_driver(_device->usb_handle, _device->iface_num);
		status = libusb_claim_interface(_device->usb_handle, _device->iface_num);
		if(status < 0)
			throw APP_EXCEPTION(to_string()
					<< "libusb_claim_interface(...) returned " << status  << " - " << std::string(libusb_error_name(status)));
	}

	libusb_free_config_descriptor(cfg_desc);
}

// To be used with the DFU device
UsbEnumerate::UsbEnumerate(uint16_t vid, uint16_t pid, uint8_t ifaceClass, uint8_t ifaceSubClass)
	: _vid(vid), _pid(pid)
{
	int ret;
	struct libusb_device_descriptor desc;
	auto cont = std::make_shared<context>();

	auto deviceHandle = libusb_open_device_with_vid_pid(0, _vid, _pid);
	if (!deviceHandle)
		throw APP_EXCEPTION(to_string() <<
							"Enumerate failed. Camera not connected! pid="
							<< std::hex << _pid);

	_device = std::make_shared<device>(cont);
	_device->vid = vid;
	_device->pid = pid;
	_device->usb_handle = deviceHandle;
	_device->usb_device = libusb_get_device(deviceHandle);
	libusb_ref_device(_device->usb_device);

	_busnum = libusb_get_bus_number(_device->usb_device);
	_devnum = libusb_get_device_address(_device->usb_device);
	_device->busnum = _busnum;
	_device->devnum = _devnum;

	ret = libusb_get_device_descriptor(_device->usb_device, &desc);
	if (ret)
		throw APP_EXCEPTION(to_string()
			<< "libusb_get_device_descriptor(...) returned " << ret << " - " << std::string(libusb_error_name(ret)));

	_device->iserial = desc.iSerialNumber;

	if(_device->usb_device && !_device->usb_handle)
	{
		int status = libusb_open(_device->usb_device, &_device->usb_handle);
		if(status < 0)
			throw APP_EXCEPTION(to_string()
				<< "libusb_open(...) returned " << status + " - " + std::string(libusb_error_name(status)));
	}

	ClaimInterface(_device->usb_device, ifaceClass, ifaceSubClass);

}

// To be used in operational mode
UsbEnumerate::UsbEnumerate(int busnum, int devnum, uint16_t vid, const std::vector<uint16_t>& pids,
  uint8_t ifaceClass, uint8_t ifaceSubClass)
	: _busnum(busnum), _devnum(devnum),
	  _vid(vid)
{
	int ret, idx;
	struct libusb_device **list;
	ssize_t count;
	struct libusb_device *dev;
	struct libusb_device_descriptor desc;

	auto cont = std::make_shared<context>();

	count = libusb_get_device_list(NULL, &list);
	if (count < 0)
		throw APP_EXCEPTION(to_string()
			<< "libusb_get_device_list(...) returned " << count << " - " << std::string(libusb_error_name((int)count)));

	for (idx = 0; idx < count; idx++) {
		dev = list[idx];

		if (dev &&
		    busnum == libusb_get_bus_number(dev) &&
		    devnum == libusb_get_device_address(dev))
			break;
	}

	if (idx == count)
		throw APP_EXCEPTION(to_string()
			<< "Couldn't find dev with busnum = " << busnum
			<< " and devnum = " << devnum);

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret)
		throw APP_EXCEPTION(to_string()
			<< "libusb_get_device_descriptor(...) returned ");

	if (desc.idVendor != vid
		|| std::find(pids.begin(), pids.end(), desc.idProduct) == pids.end())
		throw APP_EXCEPTION(to_string()
			<< "Device found, but incorrect (vid, pid) combo. Found ("
			<< std::hex << desc.idVendor << ", "
			<< std::hex << desc.idProduct << ")");

	_device = std::make_shared<device>(cont);
	_device->busnum = busnum;
	_device->devnum = devnum;
	_device->vid = desc.idVendor;
	_device->pid = desc.idProduct;
	_device->usb_device = dev;
	_device->iserial = desc.iSerialNumber;
	libusb_ref_device(_device->usb_device);
	_pid = _device->pid;

	ret = libusb_open(_device->usb_device, &_device->usb_handle);
	if(ret < 0)
		throw APP_EXCEPTION(to_string() << "libusb_open(...) returned " << ret << " - " << std::string(libusb_error_name(ret)));

	ClaimInterface(dev, ifaceClass, ifaceSubClass);
	libusb_free_device_list(list, 0);
}

UsbEnumerate::~UsbEnumerate()
{
	libusb_release_interface(_device->usb_handle, _device->iface_num);

        libusb_attach_kernel_driver(_device->usb_handle, _device->iface_num);

	if (_device->usb_handle)
		libusb_close(_device->usb_handle);

	if (_device->usb_device)
		libusb_unref_device(_device->usb_device);
}

bool UsbEnumerate::GetDevice(std::shared_ptr<device>& dev)
{
	dev = _device;

	return (dev) ? true : false;
}

int UsbEnumerate::IsDeviceExist(uint16_t vid, uint16_t pid, bool silent)
{
	std::vector<uint16_t> pids{pid};
	return IsDeviceExist(vid, pids, silent);
}


const char* UsbEnumerate::usbSpeed(int speedCode)
{
    switch (speedCode)
    {
        case LIBUSB_SPEED_LOW: return "USB 1.1 (Low Speed 1.5MBit/s)";
        case LIBUSB_SPEED_FULL: return "USB 2.0 (Full Speed 12MBit/s)";
        case LIBUSB_SPEED_HIGH: return "USB 2.1 (High Speed 480MBit/s)"; /* USB 3.0 device connected to USB 2.0 HUB */
        case LIBUSB_SPEED_SUPER: return "USB 3.0 (Super Speed 5000MBit/s)";
        case LIBUSB_SPEED_UNKNOWN: return "Unknown USB speed";
    }
    return "Unknown USB speed";
}

int UsbEnumerate::IsDeviceExist(uint16_t vid, const std::vector<uint16_t>& pids, bool silent)
{
	struct libusb_device **list;
	struct libusb_device *dev;
	struct libusb_device_descriptor desc;
	ssize_t count;
	int ret;
	auto cont = std::make_shared<context>();
	count = libusb_get_device_list(NULL, &list);
    if (silent == false)
    {
        printf("Scanning all USB devices (%zd) for Intel cameras\n", count);
    }

	if (count < 0)
		throw APP_EXCEPTION(to_string()
			<< "libusb_get_device_list(...) returned " << count << " - " << std::string(libusb_error_name((int)count)));

	for (int idx = 0; idx < count; idx++) {
		dev = list[idx];

		ret = libusb_get_device_descriptor(dev, &desc);
		if (ret)
			throw APP_EXCEPTION(to_string()
			  <<     "libusb_get_device_descriptor(...) returned " << ret << " - " << std::string(libusb_error_name(ret)));

		if (desc.idVendor == vid
		  && std::find(pids.begin(), pids.end(), desc.idProduct) != pids.end())
		{
            if (silent == false)
            {
                printf("Found: VID = 0x%X, PID = 0x%X, BUS number = %d, Device number = %d, Speed = %s\n", desc.idVendor, desc.idProduct, libusb_get_bus_number(dev), libusb_get_device_address(dev), usbSpeed(libusb_get_device_speed(dev)));
            }

			return desc.idProduct;
		}
	}

	libusb_free_device_list(list, 0);
	return 0;
}

int UsbEnumerate::NumDevicesExist(uint16_t vid, const std::vector<uint16_t>& pids, std::vector<devinfo>& info)
{
	struct libusb_device **list;
	struct libusb_device *dev;
	struct libusb_device_descriptor desc;
	ssize_t count;
	int ret, number_devices = 0;
	auto cont = std::make_shared<context>();
	count = libusb_get_device_list(NULL, &list);
	if (count < 0)
		throw APP_EXCEPTION(to_string()
			<< "libusb_get_device_list(...) returned " << count << " - " << std::string(libusb_error_name((int)count)));

	for (int idx = 0; idx < count; idx++) {
		dev = list[idx];

		ret = libusb_get_device_descriptor(dev, &desc);
		if (ret)
			throw APP_EXCEPTION(to_string()
			  << "libusb_get_device_descriptor(...) returned " << ret << " - " << std::string(libusb_error_name(ret)));

		if (desc.idVendor == vid
		  && std::find(pids.begin(), pids.end(), desc.idProduct) != pids.end())
		{
			unsigned char serial_cstr[32] = {0};
			libusb_device_handle *handle;

			int status = libusb_open(dev, &handle);
			if(status < 0)
				throw APP_EXCEPTION(to_string()
					<< "libusb_open(...) returned " << status << " - " << std::string(libusb_error_name(status)));


			// TODO: Itay Carpis, Check why does it fail on USB2
			ret = libusb_get_string_descriptor_ascii(handle,
							desc.iSerialNumber,
							serial_cstr,
							sizeof(serial_cstr));
//			if (ret < 0)
//				printf("Error: libusb_get_string_descriptor_ascii returned error (%d)\n", ret);

			libusb_close(handle);

			info[number_devices].serialnum = std::string(reinterpret_cast<const char*> (serial_cstr));
			info[number_devices].busnum = libusb_get_bus_number(dev);
			info[number_devices].devnum = libusb_get_device_address(dev);

			++number_devices;
		}
	}

	libusb_free_device_list(list, 0);
	return number_devices;
}

bool UsbEnumerate::GetDeviceSerialNumber(std::string& serialNumber)
{
	unsigned char serial_cstr[32] = {0};
	int ret = 255;

	if (!_device.get()) {
        printf("Error: Cannot get serial number -- device is null\n");
		return false;
	}

	// TODO: Itay Carpis, Check why does it fail on USB2
	ret = libusb_get_string_descriptor_ascii(_device->usb_handle,
					_device->iserial,
					serial_cstr,
					sizeof(serial_cstr));
//	if (ret < 0) {
//		printf("Error: libusb_get_string_descriptor_ascii returned error (%d)\n", ret);
//		return false;
//	}

	serialNumber = std::string(reinterpret_cast<const char*> (serial_cstr));

	return true;
}
