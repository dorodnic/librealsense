/*
    IMUEvents.h

    Copyright © 2017 Occipital, Inc. All rights reserved.
    This file is part of the Structure SDK.
    Unauthorized copying of this file, via any medium is strictly prohibited.
    Proprietary and confidential.

    http://structure.io
*/

#pragma once

#include <ST/Macros.h>
#include <cmath>

namespace ST {

//------------------------------------------------------------------------------

/** @brief Container struct to hold an accelerometer sample. */
struct ST_API Acceleration
{
    /** X-axis accel (m/s) */
    double x = NAN;

    /** Y-axis accel (m/s) */
    double y = NAN;

    /** Z-axis accel (m/s) */
    double z = NAN;
};

//------------------------------------------------------------------------------

/** @brief Container struct to hold a gyroscope sample. */
struct ST_API RotationRate
{
    /** X-axis rotation (rad/s) */
    double x = NAN;
    /** Y-axis rotation (rad/s) */
    double y = NAN;
    /** Z-axis rotation (rad/s) */
    double z = NAN;
};

//------------------------------------------------------------------------------

/** @brief Container struct to hold an IMU attitude. It is really just a 3x3 matrix. */
struct ST_API Attitude
{
    double m11 = NAN, m12 = NAN, m13 = NAN;
    double m21 = NAN, m22 = NAN, m23 = NAN;
    double m31 = NAN, m32 = NAN, m33 = NAN;
};

//------------------------------------------------------------------------------

/** @brief Container struct to hold an iOS devices magnetic field reading. */
struct ST_API MagneticField
{
    /** X-axis strength */
    double x = NAN;
    /** Y-axis strength */
    double y = NAN;
    /** Z-axis strength */
    double z = NAN;

    /** Describe the confidence in this measurement. */
    enum class Accuracy
    {
        Uncalibrated = -1,

        Low,
        Medium,
        High
    };

    /** Confidence of the measurement. */
    Accuracy accuracy = Accuracy::Uncalibrated;
};

//------------------------------------------------------------------------------

/** @brief Defines to help the system know which device streamed an IMU data struct. */
enum class IMUDeviceId
{
    Invalid,

    /** Apple devices */
    iOS,

    /** Structure Core sensor or Structure Core data from an OCC file */
    StructureCore,

    /** Used as a placeholder */
    OtherDevice,


    HowMany,
};

//------------------------------------------------------------------------------

/** @brief [Deprecated] iOS-only container struct to hold all measurements in one, neat location.
    @deprecated Since we don't support iOS on Windows.
*/
struct ST_API DeviceMotion
{
    Attitude      attitude;
    RotationRate  rotationRate;
    Acceleration  gravity;
    Acceleration  userAcceleration;
    MagneticField magneticField;
};

//------------------------------------------------------------------------------

/** @brief Holds accelerometer data. */
struct ST_API AccelerometerEvent
{
     AccelerometerEvent();
    ~AccelerometerEvent();

    /** @brief Returns the timestamp of the accelerometer event, in seconds. */
    double timestamp() const;

    /** @brief Holds the XYZ acceleration data. */
    Acceleration acceleration() const;

    /** @brief Returns which device streamed this accelerometer event. */
    IMUDeviceId deviceId() const;

    /** @brief Returns the temperature of the accelerometer unit. */
    double temperature() const;

    /** @brief Sets the raw accelerometer event data. */
    void setAccelEvent(double x, double y, double z, double timestamp, const char* deviceID);

    ST_DECLARE_CLONABLE_OPAQUE_INTERNALS(AccelerometerEvent);
};

//------------------------------------------------------------------------------s

/** @brief Holds gyroscope data. */
struct ST_API GyroscopeEvent
{
     GyroscopeEvent ();
    ~GyroscopeEvent ();

    /** @brief Returns the timestamp of the gyroscope event, in seconds. */
    double timestamp() const;

    /** @brief Holds the XYZ gyroscope rotation data. */
    RotationRate rotationRate() const;

    /** @brief Returns which device streamed this gyroscope event. */
    IMUDeviceId deviceId() const;

    /** @brief Sets the raw gyroscope event data. */
    void setGyroEvent(double x, double y, double z, double timestamp, const char* deviceID);

    ST_DECLARE_CLONABLE_OPAQUE_INTERNALS(GyroscopeEvent);
};

//------------------------------------------------------------------------------

/** @brief [Deprecated] iOS-only container struct to hold all measurements in one, neat location.
    @deprecated Since we don't support iOS on Windows.
*/
struct ST_API DeviceMotionEvent
{
    double       timestamp = NAN;
    DeviceMotion deviceMotion;
    IMUDeviceId  deviceId;
};

//------------------------------------------------------------------------------

} // ST namespace


