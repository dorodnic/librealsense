/*******************************************************************************
INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2017 Intel Corporation. All Rights Reserved.
*******************************************************************************/

#ifndef MISC_H
#define MISC_H

#define DS5_FW_FILE_REV_OFFSET 0x18e
#define DS5_DFU_PID 0x0adb
#define DS5_USB2_DFU_PID 0x0adc
#define DS5_DFU_EXT_PID 0x0af5
#define DS5_DFU_CHECK_PERIOD 5
#define DS5_DFU_TIMEOUT 1000
#define DS5_OPMODE_TIMEOUT 30000
#define DS5_OPMODE_CHECK_TIMEOUT 1000

#include <sstream>
#include <exception>
#include <string>
#include <vector>


static const std::vector<uint16_t> ds5_dfu_pids = { DS5_DFU_PID, DS5_USB2_DFU_PID };

enum DFU_RETURN_CODES {
        DFU_FW_UPDATE_SUCCESS  = 0,
        DFU_FW_UPDATE_NOT_REQD = 1,
        DFU_FW_UPDATE_ERROR    = 2
};


struct to_string
{
	std::ostringstream ss;
	template<class T> to_string & operator << (const T & val) { ss << val; return *this; }
	operator std::string() const { return ss.str(); }
    operator std::exception() const { return std::exception(ss.str().c_str()); }
};

struct FW_REV
{
	union
	{
		unsigned int value;
		struct
		{
			unsigned char Build;
			unsigned char Patch;
			unsigned char Minor;
			unsigned char Major;

		} Revision;
	} u;

	std::string ToString() const {
		std::stringstream s;
		s 	<< (int)u.Revision.Major << "."
			<< (int)u.Revision.Minor << "."
			<< (int)u.Revision.Patch << "."
			<< (int)u.Revision.Build;
		return s.str();
	}

        friend bool operator>(const FW_REV &v1, const FW_REV &v2) {
                return  v1.u.Revision.Major > v2.u.Revision.Major ? true :
                        (v1.u.Revision.Major == v2.u.Revision.Major ? v1.u.Revision.Minor > v2.u.Revision.Minor ? true :
                        (v1.u.Revision.Minor == v2.u.Revision.Minor ? v1.u.Revision.Patch > v2.u.Revision.Patch ? true :
                        (v1.u.Revision.Patch == v2.u.Revision.Patch ? (v1.u.Revision.Build > v2.u.Revision.Build) : false) : false) : false);
        }
};

#endif //MISC_H
