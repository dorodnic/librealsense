/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#ifndef DFU_BASE_H
#define DFU_BASE_H

#include <string>
#include <functional>
#include "usb-ctrl.h"

/* DFU commands */
enum {
	DFU_DETACH	= 0,
	DFU_DNLOAD	= 1,
	DFU_UPLOAD	= 2,
	DFU_GETSTATUS	= 3,
	DFU_CLRSTATUS	= 4,
	DFU_GETSTATE	= 5,
	DFU_ABORT	= 6
};

/* DFU states */
enum {
	STATE_APP_IDLE			= 0x00,
	STATE_APP_DETACH		= 0x01,
	STATE_DFU_IDLE			= 0x02,
	STATE_DFU_DOWNLOAD_SYNC		= 0x03,
	STATE_DFU_DOWNLOAD_BUSY		= 0x04,
	STATE_DFU_DOWNLOAD_IDLE		= 0x05,
	STATE_DFU_MANIFEST_SYNC		= 0x06,
	STATE_DFU_MANIFEST		= 0x07,
	STATE_DFU_MANIFEST_WAIT_RESET	= 0x08,
	STATE_DFU_UPLOAD_IDLE		= 0x09,
	STATE_DFU_ERROR			= 0x0a
};


/* DFU status */
enum {
	DFU_STATUS_OK			= 0x00,
	DFU_STATUS_ERROR_TARGET		= 0x01,
	DFU_STATUS_ERROR_FILE		= 0x02,
	DFU_STATUS_ERROR_WRITE		= 0x03,
	DFU_STATUS_ERROR_ERASE		= 0x04,
	DFU_STATUS_ERROR_CHECK_ERASED	= 0x05,
	DFU_STATUS_ERROR_PROG		= 0x06,
	DFU_STATUS_ERROR_VERIFY		= 0x07,
	DFU_STATUS_ERROR_ADDRESS	= 0x08,
	DFU_STATUS_ERROR_NOTDONE	= 0x09,
	DFU_STATUS_ERROR_FIRMWARE	= 0x0a,
	DFU_STATUS_ERROR_VENDOR		= 0x0b,
	DFU_STATUS_ERROR_USBR		= 0x0c,
	DFU_STATUS_ERROR_UNKNOWN	= 0x0e,
	DFU_STATUS_ERROR_STALLEDPKT	= 0x0f,
};

//Callback function for getting status of Firmware update
typedef void(ON_PROGRESS_FUNC)(int numBytes, int total, void* userContext);

struct SerialNumber
{
	unsigned char Serial[6];
	unsigned char Spare[2];

	std::string ToString() const;
};

struct DFU_STATUS {
	unsigned char bStatus;
	unsigned int  bwPollTimeout;
	unsigned char bState;
	unsigned char iString;
};

class DFUBase;

struct UserContext
{
	DFUBase* _object;
	void* _context;
};

enum DFU_STATE {
	DFU_STATE_appIDLE = 0,
	DFU_STATE_appDETACH = 1,
	DFU_STATE_dfuIDLE = 2,
	DFU_STATE_dfuDNLOAD_SYNC = 3,
	DFU_STATE_dfuDNBUSY = 4,
	DFU_STATE_dfuDNLOAD_IDLE = 5,
	DFU_STATE_dfuMANIFEST_SYNC = 6,
	DFU_STATE_dfuMANIFEST = 7,
	DFU_STATE_dfuMANIFEST_WAIT_RST = 8,
	DFU_STATE_dfuUPLOAD_IDLE = 9,
	DFU_STATE_dfuERROR = 10
};

class DFUBase
{
public:
	virtual void UpdateFirmware(std::vector<uint8_t> data, 
        ON_PROGRESS_FUNC userCallback, void* userContext = nullptr) = 0;

protected:
	void DFUDownload(int length, unsigned short transaction, unsigned char *data, int* bytesSent);
	void DFUGetStatus(DFU_STATUS *status);
	void DFUDetach(unsigned short timeout);
	void FirmwareUploadStatus(unsigned char* buffer, unsigned short bufferLength, unsigned long* bytesRecieved);
	bool DownloadFirmwareProtected(ON_PROGRESS_FUNC onProgress, void* userContext);

	std::shared_ptr<LibUsbControl> _libUsbControl;
};

#endif // DFU_BASE_H
