/*
    CaptureSessionTypes.h

    Copyright © 2017 Occipital, Inc. All rights reserved.
    This file is part of the Structure SDK.
    Unauthorized copying of this file, via any medium is strictly prohibited.
    Proprietary and confidential.

    http://structure.io
*/

#pragma once

#include <ST/Macros.h>
#include <ST/IMUEvents.h>
#include <ST/CameraFrames.h>
#include <ST/CaptureSessionSettings.h>

namespace ST
{

// forward declarations
struct CaptureSession;
struct CaptureSessionSample;

//------------------------------------------------------------------------------

/** @brief Events describing the internal state changes of the CaptureSession. */
enum class CaptureSessionEventId
{
    /** Hopefully, shouldn't ever get used. */
    Unknown,

    /** Sensor is connected. */
    Connected,

    /** Sensor is booting up. */
    Booting,

    /** Sensor is ready to stream. */
    Ready,

    /** Sensor was disconnected. */
    Disconnected,

    /** An error happened internally. */
    Error,

    /** A USB error happened internally. */
    UsbError,

    /** Sensor has entered low power mode */
    LowPowerMode,

    /** Sensor has entered recovery mode. (for firmware flashing, etc.) */
    RecoveryMode,

    /** Sensor has corrupt production line data. Please contact Occipital if this happens. */
    ProdDataCorrupt,

    /** Calibration for the sensor is either missing or invalid. */
    CalibrationMissingOrInvalid,

    /** Sensor has outdated firmware. */
    FWVersionMismatch,

    /** Sensor is attempting to update firmware from a BIN file. */
    FWUpdate,

    /** The sensor firmware update completed successfully. */
    FWUpdateComplete,

    /** The sensor failed to update firmware. */
    FWUpdateFailed,

    /** The sensor has corrupt firmware. Please contact Occipital if this happens. */
    FWCorrupt,

    /** End of file has been reached when replaying data. */
    EndOfFile,
};

/** @brief Describes the current interface of the USB port the StructureCore is connected to. */
enum class CaptureSessionUSBVersion
{
    /** Unknown USB interface version. Hopefully, shouldn't ever get used. */
    Unknown,

    /** USB1 interface. Very bad if your device reports a USB1 connection. */
    USB1,

    /** USB2 interface. Not as bad as USB1, but not as good as USB3. */
    USB2,

    /** USB3 interface. Very good, and the preferred USB interface. */
    USB3,
};

//------------------------------------------------------------------------------

/** @brief Called repeatedly with each new sample while streaming from the session. */
using CaptureSessionOutputCallback = void (*) (void* userdata, CaptureSession* session, const CaptureSessionSample& sample);

/** @brief Called when an event occurs in the session. */
using CaptureSessionEventCallback = void (*) (void* userdata, CaptureSession* session, const CaptureSessionEventId& event);

/** @brief Called repeatedly with each new sample when synchronously reading a file. Return false to abort reading and true to continue. */
using SynchronousCaptureOutputCallback = bool (*) (void* userdata, const CaptureSessionSample& sample);

//------------------------------------------------------------------------------

struct ST_API CaptureSessionSample
{
    /** @brief All types of data streamed from CaptureSession. */
    enum class Type
    {
        Invalid = -1,

        /** Structure Core or iOS device accelerometer packet. */
        AccelerometerEvent,

        /** Structure Core or iOS device gyroscope packet. */
        GyroscopeEvent,

        /** An iOS device motion packet. */
        DeviceMotionEvent,

        /** Structure Core or Structure Sensor infrared frame. */
        InfraredFrame,

        /** Structure Core or Structure Sensor depth frame. */
        DepthFrame,

        /** Structure Core visible camera frame. */
        VisibleFrame,

        /** iOS color camera frame. */
        ColorFrame,

        /** Synchronized Structure Core Visible+Depth or iOS Color+Depth frame pair, depending on your connected device. */
        SynchronizedFrames,

        /** Set of images from multiple cameras. Will not work for Structure Core, requires a specific setup. Safe to ignore. */
        MultiCameraColorFrame,

        HowMany
    };

    /** @brief The type of the sample. Will dictate which variable member to access for valid data. */
    Type type = Type::Invalid;

    /** @brief Structure Core or iOS device gyroscope packet. */
    GyroscopeEvent gyroscopeEvent;

    /** @brief Structure Core or iOS device accelerometer packet. */
    AccelerometerEvent accelerometerEvent;

    /** @brief An iOS device motion packet. */
    DeviceMotionEvent deviceMotionEvent;

    /** @brief Structure Core or Structure Sensor depth frame. */
    DepthFrame depthFrame;

    /** @brief Structure Core or Structure Sensor infrared frame. */
    InfraredFrame infraredFrame;

    /** @brief Structure Core visible camera frame. */
    ColorFrame visibleFrame;

    /** @brief iOS color camera frame. */
    ColorFrame colorFrame;

    /** @brief Color frames from multiple sources. */
    MultiCameraColorFrame multiCameraColorFrame;

    /** @brief Convert the enum to a readable string. @see CaptureSessionSample::Type */
    static const char* toString(CaptureSessionSample::Type type);

    /** @brief Convert the enum to a readable string. @see CaptureSessionEventId */
    static const char* toString(CaptureSessionEventId state);

    bool isImage() const;

    /** @brief Constructor. */
    CaptureSessionSample();
    ~CaptureSessionSample();

    /** @brief Copy constructor. Will hard copy data. */
    CaptureSessionSample(const CaptureSessionSample& other);

    void* reserved;
};

//------------------------------------------------------------------------------

} // ST namespace
