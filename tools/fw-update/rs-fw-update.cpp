// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp>

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "tclap/CmdLine.h"
#include "tclap/ValueArg.h"

using namespace TCLAP;

std::vector<uint8_t> read_fw_file(std::string file_path)
{
    std::vector<uint8_t> rv;

    std::ifstream file(file_path, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open())
    {
        rv.resize(file.tellg());

        file.seekg(0, std::ios::beg);
        file.read((char*)rv.data(), rv.size());
        file.close();
    }

    return rv;
}

bool try_update(rs2::context ctx, std::vector<uint8_t> fw_image)
{
    auto fwu_devs = ctx.query_devices(RS2_PRODUCT_LINE_RECOVERY);
    for (auto&& d : fwu_devs)
    {
        auto fwu_dev = d.as<rs2::fw_update_device>();

        if (!fwu_dev)// || !fwu_dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
            continue;
        auto sn = fwu_dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
        std::cout << std::endl << "FW update started" << std::endl << std::endl;
        fwu_dev.update_fw(fw_image, [&](const float progress)
        {
            printf("\rFW update progress: %d[%%]", (int)(progress * 100));
        });
        std::cout << std::endl << std::endl << "FW update done" << std::endl;
        return true;
    }
    return false;
}

void list_devices(rs2::context ctx)
{
    auto devs = ctx.query_devices(RS2_PRODUCT_LINE_DEPTH | RS2_PRODUCT_LINE_RECOVERY);
    switch (devs.size())
    {
    case 0: std::cout << std::endl << "There are no connected devices" << std::endl; break;
    case 1: std::cout << std::endl << "One connected device detected" << std::endl; break;
    default: std::cout << std::endl << devs.size() << " connected devices detected" << std::endl; break;
    }

    if (devs.size() == 0)
        return;

    std::cout << std::endl << "Connected devices:" << std::endl;

    std::map<rs2_camera_info, std::string> camera_info;

    int counter = 0;
    for (auto&& d : devs)
    {
        for (int i = 0; i < RS2_CAMERA_INFO_COUNT; i++)
        {
            auto info = (rs2_camera_info)i;
            camera_info[info] = d.supports(info) ? d.get_info(info) : "unknown";
        }

        std::cout << ++counter << ") " <<
            "Name: " << camera_info[RS2_CAMERA_INFO_NAME] <<
            ", Serial number: " << camera_info[RS2_CAMERA_INFO_SERIAL_NUMBER] <<
            ", FW version: " << camera_info[RS2_CAMERA_INFO_FIRMWARE_VERSION] << std::endl;
    }
}

int main(int argc, char** argv) try
{
    rs2::context ctx;
    std::condition_variable cv;
    std::mutex mutex;
    std::string selected_serial_number;

    bool device_available = false;
    bool done = false;

    CmdLine cmd("librealsense rs-fw-update tool", ' ', RS2_API_VERSION_STR);

    SwitchArg list_devices_arg("l", "list_devices", "List all available devices");
    SwitchArg recover_arg("r", "recover", "Recover all connected deviced which are in recovery mode and update them to the default fw");
    ValueArg<std::string> file_arg("f", "file", "Path to a fw image file to update", false, "", "string");
    ValueArg<std::string> serial_number_arg("s", "serial_number", "The serial number of the device to be update", false, "", "string");

    cmd.add(list_devices_arg);
    cmd.add(recover_arg);
    cmd.add(file_arg);
    cmd.add(serial_number_arg);

    cmd.parse(argc, argv);

    bool recovery_request = recover_arg.getValue();

    if (list_devices_arg.isSet())
    {
        list_devices(ctx);
        return EXIT_SUCCESS;
    }

    if (serial_number_arg.isSet())
    {
        selected_serial_number = serial_number_arg.getValue();
        std::cout << std::endl << "Search for device with serial number: " << selected_serial_number << std::endl;
    }

    if (!serial_number_arg.isSet() && !recover_arg.isSet() && !list_devices_arg.isSet())
    {
        std::cout << std::endl << "Either recovery or serial number must be selected" << std::endl << std::endl;
        return EXIT_FAILURE;
    }

    if (!file_arg.isSet())
    {
        std::cout << std::endl << "FW file must be selected" << std::endl << std::endl;
        return EXIT_FAILURE;
    }

    std::string fw_file_path = file_arg.getValue();
    std::vector<uint8_t> fw_image = read_fw_file(fw_file_path);

    if (fw_image.size() == 0)
    {
        std::cout << std::endl << "Failed to read FW file" << std::endl << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << std::endl << "Update to FW: " << fw_file_path << std::endl;

    ctx.set_devices_changed_callback([&](rs2::event_information& info)
    {
        {
            std::lock_guard<std::mutex> lk(mutex);
            device_available = true;
        }
        cv.notify_one();
    });

    auto devs = ctx.query_devices(RS2_PRODUCT_LINE_DEPTH);
    bool device_found = false;

    for (auto&& d : devs)
    {
        auto sn = d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
        if (sn != selected_serial_number)
            continue;
        device_found = true;
        auto fw = d.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION);
        std::cout << std::endl << "device found, current FW version: " << fw << std::endl;

        d.enter_to_fw_update_mode();

        std::unique_lock<std::mutex> lk(mutex);
        if (!cv.wait_for(lk, std::chrono::seconds(5), [&] { return device_available; }))
            break;

        device_available = false;
        int retries = 50;
        while (ctx.query_devices(RS2_PRODUCT_LINE_RECOVERY).size() == 0 && retries--)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (retries > 0)
        {
            std::cout << std::endl << "device in FW update mode, start updating." << std::endl;
            done = try_update(ctx, fw_image);
        }
        else
        {
            std::cout << std::endl << "failed to locate a device in FW update mode." << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::unique_lock<std::mutex> lk(mutex);
    cv.wait_for(lk, std::chrono::seconds(5), [&] { return !done || device_available; });

    if (recover_arg.isSet())
    {
        std::cout << std::endl << "check for devices in recovery mode..." << std::endl;
        if (try_update(ctx, fw_image))
        {
            std::cout << std::endl << "device recoverd" << std::endl;
            return EXIT_SUCCESS;
        }
        else
        {
            std::cout << std::endl << "no devices in recovery mode found." << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (!device_found && !recover_arg.isSet())
    {
        std::cout << std::endl << "couldn't find the requested serial number" << std::endl;
    }

    if (done)
    {
        auto devs = ctx.query_devices(RS2_PRODUCT_LINE_DEPTH);
        for (auto&& d : devs)
        {
            auto sn = d.supports(RS2_CAMERA_INFO_SERIAL_NUMBER) ? d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER) : "unknown";
            if (sn != selected_serial_number)
                continue;

            auto fw = d.supports(RS2_CAMERA_INFO_FIRMWARE_VERSION) ? d.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION) : "unknown";
            std::cout << std::endl << "device " << sn << " successfully updated to FW: " << fw << std::endl;
        }
    }

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
