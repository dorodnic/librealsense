/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#include <chrono>
#include <iostream>
#include <ctime>
#include <algorithm>
#include "dfu-base.h"

#ifndef NOMINMAX
#define NOMINMAX 
#endif
#define INTERFACE_NUMBER 0

std::string SerialNumber::ToString() const
{
	std::stringstream serialNum;

	for (int i = 0 ; i < sizeof(Serial) ; ++i) {
		char temp[3] = {0};
		snprintf(temp, sizeof(temp), "%02X", Serial[i]);
		serialNum << temp;
	}

	return serialNum.str();
}

void DFUBase::DFUGetStatus(DFU_STATUS *status)
{
	if (!status)
		throw APP_EXCEPTION("Invalid input params");

	LIBUSB_CTRL_EP_PACKET packet;
	const int dataLen = 6;

	/* Initialize the status data structure */
	status->bStatus = DFU_STATUS_ERROR_UNKNOWN;
	status->bwPollTimeout = 0;
	status->bState = STATE_DFU_ERROR;
	status->iString = 0;

	packet.RequestType = 0xa1;
	packet.Request = DFU_GETSTATUS;
	packet.Index = static_cast<unsigned short>(INTERFACE_NUMBER);
	packet.Length = dataLen;
	packet.Value = 0;

	_libUsbControl->ControlTransfer(packet);

	if (packet.Length == dataLen) {
		status->bStatus = packet.Data[0];
		status->bwPollTimeout = ((0xff & packet.Data[3]) << 16) |
			((0xff & packet.Data[2]) << 8) |
			(0xff & packet.Data[1]);

		status->bState = packet.Data[4];
		status->iString = packet.Data[5];
	}
}

void DFUBase::DFUDetach(unsigned short timeout)
{
	LIBUSB_CTRL_EP_PACKET packet;

	packet.RequestType = 0x21;
	packet.Request = DFU_DETACH;
	packet.Index = static_cast<unsigned short>(INTERFACE_NUMBER);
	packet.Length = 0;
	packet.Value = timeout;

	_libUsbControl->ControlTransfer(packet, timeout);
}

void DFUBase::FirmwareUploadStatus(unsigned char* buffer, unsigned short bufferLength, unsigned long* bytesReceived)
{
	if (!buffer || !bytesReceived)
		throw APP_EXCEPTION("Invalid input params");

	LIBUSB_CTRL_EP_PACKET packet;

	packet.RequestType = 0xa1;
	packet.Request = DFU_UPLOAD;
	packet.Index = static_cast<unsigned short>(INTERFACE_NUMBER);
	packet.Length = bufferLength;
	packet.Value = 0;
	
	_libUsbControl->ControlTransfer(packet);

    memcpy_s(buffer, bufferLength, packet.Data, bufferLength);
	*bytesReceived = bufferLength;
}

bool TimeOutAchived(std::clock_t StartTime, int64_t TimeOut)
{
	auto CurrentTime = std::chrono::system_clock::now();
	if (double(std::clock() - StartTime ) /  CLOCKS_PER_SEC
		>= TimeOut)
		return true;

	return false;
}

bool DFUBase::WaitForStates(std::vector<int> states, int timeout, int pollingIntervals,
				DFU_STATUS* status, bool printDots)
{
	if (!status)
		throw APP_EXCEPTION("Invalid input params");

	auto StartTime = std::clock();
	int i = 0;

	do {
		if (printDots && ((i % 7) == 0))
			std::cout << ".";

		i++;

		DFUGetStatus(status);

		auto num = count_if(states.begin(), states.end(),
				[&](int val) {
					if (status->bState == val)
						return true;

					return false;
				}
				);

		if (num > 0 ) {
			if (printDots)
				std::cout << std::endl;
			return true;
		}

		// otherwise, wait for 5 milliseconds.
		if(TimeOutAchived(StartTime, timeout)) {
			if (printDots)
				std::cout << std::endl;
            printf("Error: Time out expired! stop waiting\n");
			return false;
		}

		Sleep(pollingIntervals / 1000);

	} while (true);

}

bool DFUBase::DownloadFirmwareProtected(ON_PROGRESS_FUNC onProgress, void* userContext)
{
	auto totalBytesSent = 0;
	unsigned short transaction = 0;
	auto buf = _fwFile->firmware;

	if (!buf)
		throw APP_EXCEPTION("FW data is null");

	auto bytesSent = 0;

	// loop until end of file.
	while (totalBytesSent < _fwFile->filesize) 
    {
		if (onProgress)
			onProgress(totalBytesSent, _fwFile->filesize, userContext);

		// we calculate bytes left to send
		int bytesLeft = _fwFile->filesize - totalBytesSent;

		// send the DFU payload of this transaction
		DFUDownload(bytesLeft, transaction++, buf, &bytesSent);

		// add bytes send and advance the buffer.
		totalBytesSent += bytesSent;
		buf += bytesSent;

		// each packet we need to make the sure the device is ready to recieve more payload.
		std::vector<int> states = { DFU_STATE_dfuDNLOAD_IDLE, DFU_STATE_dfuERROR };
		DFU_STATUS status;
		WaitForStates(states, 5000, 5, &status);

		// we break if status in not valid.
		if (status.bStatus != DFU_STATUS_OK) {
			if (status.bStatus == DFU_STATUS_ERROR_FILE)
                printf("\nError: FW file seems corrupted."
					"Are you sure you are using the signed FW file?\n");
			else {
                printf("\nError: Error at transaction %d\nFW state = %s\n",
					transaction, GetDFUStatusAsString(status.bStatus).c_str());
			}
			return false;
		}

		if (status.bState == DFU_STATE_dfuERROR) {
            printf("\nError: Encountered DFU_STATE_dfuERROR at transaction %d\n",
					transaction);
			return false;
		}
	}

	// we send all firmware to the device, signal that this is the end by a 0 size data.
	DFUDownload(0, transaction, nullptr, &bytesSent);


	// sleeping for 100 mil just in case.
	Sleep(100);

    // Update progress bar on completion
    if (onProgress)
    {
        onProgress(totalBytesSent, _fwFile->filesize, userContext);
    }

	return PostDownloadProtocol();
}

#define DEF_TO_STRING(x) 	case x:\
{ \
	std::stringstream s; \
	s << #x; \
	return s.str(); \
}

std::string DFUBase::GetDFUStatusAsString(int status)
{
	switch (status) {
		DEF_TO_STRING(DFU_STATUS_OK);
		DEF_TO_STRING(DFU_STATUS_ERROR_TARGET);
		DEF_TO_STRING(DFU_STATUS_ERROR_FILE);
		DEF_TO_STRING(DFU_STATUS_ERROR_WRITE);
		DEF_TO_STRING(DFU_STATUS_ERROR_ERASE);
		DEF_TO_STRING(DFU_STATUS_ERROR_CHECK_ERASED);
		DEF_TO_STRING(DFU_STATUS_ERROR_PROG);
		DEF_TO_STRING(DFU_STATUS_ERROR_VERIFY);
		DEF_TO_STRING(DFU_STATUS_ERROR_ADDRESS);
		DEF_TO_STRING(DFU_STATUS_ERROR_NOTDONE);
		DEF_TO_STRING(DFU_STATUS_ERROR_FIRMWARE);
		DEF_TO_STRING(DFU_STATUS_ERROR_VENDOR);
		DEF_TO_STRING(DFU_STATUS_ERROR_USBR);
		DEF_TO_STRING(DFU_STATUS_ERROR_STALLEDPKT);
	default:
		std::stringstream sstream;
		sstream << "Unrecognized DFU_STATUS = 0x" << std::hex << status;
		return sstream.str();
	}
}

void DFUBase::DFUDownload(int length, unsigned short transaction, unsigned char *data, int* bytesSent)
{
	if ((length && !data) || !bytesSent)
		throw APP_EXCEPTION("Invalid input params");

	LIBUSB_CTRL_EP_PACKET packet;

	// if it is the last packet, it may be smaller then the tansfer size.
	auto chunk_size = std::min(length, HW_MONITOR_COMMAND_SIZE);

	packet.RequestType = 0x21;
	packet.Request = DFU_DNLOAD;
	packet.Index = static_cast<unsigned short>(INTERFACE_NUMBER);
	packet.Length = chunk_size,
		packet.Value = transaction;

    if (chunk_size)
    {
        memcpy_s(packet.Data, sizeof(packet.Data), data, chunk_size);
    }
		
	_libUsbControl->ControlTransfer(packet);

	*bytesSent = chunk_size;
}
