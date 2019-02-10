/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#include "dfu-d400.h"

#define USB_VENDOR_SPECIFIC_CLASS 255
#define USB_VENDOR_SPECIFIC_SUBCLASS 255

std::string FW_STATUS::ToString() const {
	std::string str;
	std::stringstream s;

	FW_REV fwHighestVer;
	FW_REV fwLastVer;

	fwHighestVer.u.value = FW_highestVersion;
	fwLastVer.u.value = FW_lastVersion;

	s	<< "- DFU_version = " << DFU_version << std::endl
		<< "- DFU_isLocked = " << DFU_isLocked << std::endl
		<< "- FW_highestVersion = " << fwHighestVer.ToString() << std::endl
		<< "- FW_lastVersion = " << fwLastVer.ToString() << std::endl
		<< "- SerialNumber = " << serialNum.ToString() << std::endl;

	return s.str();
}

DFU_DS5::DFU_DS5()
{
	for (uint16_t pid : ds5_dfu_pids)
	{
		if (UsbEnumerate::IsDeviceExist(INTEL_CAMERAS_VID, pid))
		{
			_libUsbControl = std::make_shared<LibUsbControl>(INTEL_CAMERAS_VID, pid,
									 USB_VENDOR_SPECIFIC_CLASS,
									 USB_VENDOR_SPECIFIC_SUBCLASS);
		}
	}

	if (!_libUsbControl)
		throw APP_EXCEPTION("DFU_DS5() Failed! DS5 device not found.");
}

void DFU_DS5::Init(const std::string &fwFilePath)
{
}

void DFU_DS5::Finalize()
{
	_libUsbControl = nullptr;
}

void DFU_DS5::CheckDFUStateBeforeUpdate()
{
	DFU_STATUS status;

	/* Request DFU state, make sure state is DFU_IDLE. */
	DFUGetStatus(&status);
	printf("DFU Info:\n");
	printf("- DFU_Status: %d\n", status.bStatus);
	printf("- DFU_Timeout: %d\n", status.bwPollTimeout);
	printf("- DFU_State: %d\n", status.bState);
	printf("- DFU_iString: %d\n", status.iString);

	if (status.bState != DFU_STATE::DFU_STATE_appIDLE && status.bState != DFU_STATE_dfuIDLE)
		throw APP_EXCEPTION("Warning: unexpected DFU state from the last session\n");
}

void DFU_DS5::GetFWStatus(FW_STATUS* fwStatus)
{
	unsigned long byteRecieved = 0;
	FirmwareUploadStatus((unsigned char *) fwStatus, sizeof(*fwStatus), &byteRecieved);
}

bool DFU_DS5::PostDownloadProtocol(void)
{
	DFU_STATUS status;
	std::vector<int> states = { DFU_STATE_dfuMANIFEST_WAIT_RST, DFU_STATE_dfuERROR };

	printf("\nRunning post download processes...");
	auto res = WaitForStates(states, 15000, 200, &status, true);
	if (!res) {
        printf("Error: Failed to burn Realtek!\n");
		return false;
	}

	if (status.bState == DFU_STATE_dfuMANIFEST_WAIT_RST) {
		printf("Post download processes done\n");
		return true;
	}

	if (status.bStatus != DFU_STATUS_OK) {
        printf("Error: Bad DFU status. Status = %s\n",
				GetDFUStatusAsString(status.bStatus).c_str());
		return false;
	}
	if (status.bState == DFU_STATE_dfuERROR) {
        printf("Error: DFU in ERROR state\n");
		return false;
	}

	return false;
}

bool DFU_DS5::DownloadFirmware(ON_PROGRESS_FUNC onProgress, void* userContext)
{
	return DownloadFirmwareProtected(onProgress, userContext);
}

void DFU_DS5::UpdateFirmware(ON_PROGRESS_FUNC userCallback, void* userContext)
{
	DFUDetach(1000);

	CheckDFUStateBeforeUpdate();

	GetFWVersionInDFUMode();

	UserContext context;
	context._object = this;
	context._context = userContext;

	auto ret = DownloadFirmware(userCallback, static_cast<void*>(&context));
	if (!ret)
		throw APP_EXCEPTION("Firmware Update Failed");
}

void DFU_DS5::GetFWVersionFromFile(FW_REV* rev)
{
	if (_fwFile == nullptr)
		throw APP_EXCEPTION(to_string() << "failed to open FW file, error = " << -errno);
	_fwFile->Read(DS5_FW_FILE_REV_OFFSET, sizeof(*rev),
		(unsigned char *)&rev->u.value, sizeof(rev->u.value));

	if (!rev->u.value)
		throw APP_EXCEPTION("Incorrect or corrupted FW file. Please check.");
}

void DFU_DS5::GetFWVersionInDFUMode()
{
	FW_REV fwRevOnFile;
    FW_STATUS fwStatus = {0};

	// Get FW version from file
	GetFWVersionFromFile(&fwRevOnFile);

	// Get FW version from DFU device
	GetFWStatus(&fwStatus);
	printf("%s\n", fwStatus.ToString().c_str());
}

bool DFU_DS5::InitSerialNumber()
{
	DFUDetach(1000);

    FW_STATUS fwStatus = {0};

	GetFWStatus(&fwStatus);

	if (fwStatus.DFU_version < 25) {
        printf("Error: InitSerialNumber API is not supported for cameras "
				"with DFU version < 25!\n");
		return false;
	}

	m_serialNumber = fwStatus.serialNum.ToString();

	return true;
}

bool DFU_DS5::WaitDeviceExitDFU()
{
	int timeout = DS5_DFU_TIMEOUT;

	/* Wait for DFU finish and restart camera */
	while (UsbEnumerate::IsDeviceExist(INTEL_CAMERAS_VID, ds5_dfu_pids, true) && timeout > 0) 
    {
		Sleep(DS5_DFU_CHECK_PERIOD);
		timeout -= DS5_DFU_CHECK_PERIOD;
	}

	if (timeout <= 0) {
        printf("Error: Camera restart time out! (%d)\n", timeout);
		return false;
	}

	return true;
}

int DFU_DS5::doAction(CmdLineArgs &args, std::shared_ptr<State> &prev)
{
	if (args.print_fw_version) 
	{
		throw std::runtime_error("option -p cannot be used in recovery mode. Please run the tool without this option for recovery.");
	}

	UpdateFirmware(ProgressBarCallBack);
	Finalize();

	printf("Exiting from DFU mode...\n");
	if (WaitDeviceExitDFU() == false)
    {
        throw std::runtime_error("Restart device timeout");
    }
	return State::STATE_FLASHED;
}
