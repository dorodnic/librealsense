/*
    CaptureSessionSettings.h

    Copyright © 2017 Occipital, Inc. All rights reserved.
    This file is part of the Structure SDK.
    Unauthorized copying of this file, via any medium is strictly prohibited.
    Proprietary and confidential.

    http://structure.io
*/

#pragma once

#include <ST/Macros.h>

struct ALooper;

namespace ST
{

//------------------------------------------------------------------------------

/** @brief Possible sources for CaptureSession streaming. */
enum class CaptureSessionSourceId
{
    Invalid = -1,

    /** Stream from an OCC file. */
    OCC,

    /** Stream from a Structure Core. */
    StructureCore,

    HowMany
};

//------------------------------------------------------------------------------

/** @brief Possible playback modes for OCC streaming. */
enum class CaptureSessionOCCPlaybackMode
{
    Invalid = -1,

    /** @brief Force the FPS to a set limit. @see CaptureSessionSettings::OCCPlaybackSettings::rateLimit */
    RateLimited,

    /** @brief Stream all data in the file frame by frame and wait for the callback to return before sending another. */
    NonDropping,

    /** @brief Attempt to emulate Structure Core streaming fully, follows time interval between frames. */
    RealTime,

    HowMany,

    // Aliases
    Default = RealTime,
};

//------------------------------------------------------------------------------

/** @brief Possible resolutions for depth frames. */
enum class StructureCoreDepthResolution
{
    Invalid = -1,

    _320x240,
    _640x480,
    _1280x960,

    HowMany,

    // Aliases
       QVGA = _320x240,
        VGA = _640x480,
       SXGA = _1280x960,
    Default = QVGA,
};

/** @brief Preset depth range modes to use for different situations. */
enum class StructureCoreDepthRangeMode
{
    Invalid = -1,

    /** Estimated range of 0.35m to 0.92m */
    VeryShort,

    /** Estimated range of 0.41m to 1.36m */
    Short,

    /** Estimated range of 0.52m to 5.23m */
    Medium,

    /** Estimated range of 0.58m to 8.0m */
    Long,

    /** Estimated range of 0.58m to 10.0m */
    VeryLong,

    /** Estimated range of 0.35m to 10.0m */
    Hybrid,

    /** Don't use a preset, use the provided configuration options. */
    Default,
};

//------------------------------------------------------------------------------

/** @brief Possible resolutions for infrared frames. */
enum class StructureCoreInfraredResolution
{
    Invalid = -1,

    // FIXME: Add default resolution.

    HowMany,

    // Aliases
    Default = Invalid,
};

/** @brief Different data-format modes for infrared frames. */
enum class StructureCoreInfraredMode
{
    Invalid = -1,

    /** Only stream infrared frames from the left camera. */
    LeftCameraOnly,

    /** Only stream infrared frames from the right camera. */
    RightCameraOnly,

    /** Stream the left and the right cameras as one large infrared frame. New images with have width x 2. */
    BothCameras,

    HowMany,

    // Aliases
    Default = BothCameras,
};

//------------------------------------------------------------------------------

/** @brief Possible resolutions for visible frames. */
enum class StructureCoreVisibleResolution
{
    Invalid = -1,

    // FIXME: Add default resolution.

    HowMany,

    // Aliases
    Default = Invalid,
};

//------------------------------------------------------------------------------

/** @brief Possible update/stream rates for IMU data. */
enum class StructureCoreIMUUpdateRate
{
    Invalid = -1,

    AccelAndGyro_100Hz,
    AccelAndGyro_200Hz,
    AccelAndGyro_800Hz,
    AccelAndGyro_1000Hz,

    HowMany,

    // Aliases
    Default = AccelAndGyro_800Hz,
};

//------------------------------------------------------------------------------

/** @brief Structure Core sensor boot mode, mostly for debugging. */
enum class StructureCoreBootMode
{
    Invalid = -1,

    FromFlash,
    FromDisk,

    HowMany,

    // Aliases
    Default = FromFlash,
};

//------------------------------------------------------------------------------

/** @brief Possible frame/data dispatch options. For advanced users only. */
enum class CaptureSessionDispatcherId
{
    Invalid = -1,

    AndroidLooper,
    BackgroundThread,

    HowMany,

    // Aliases
#if   _WIN32
    Default = BackgroundThread,
#else
    Default = BackgroundThread,
#endif
};

//------------------------------------------------------------------------------

/** @brief Settings container for initializing CaptureSession. */
struct ST_API CaptureSessionSettings
{
    /** @brief The capture session source to stream. @see CaptureSessionSourceId */
    CaptureSessionSourceId source = CaptureSessionSourceId::Invalid;

    /** @brief Set to true to enable frame synchronization between visible or color and depth. */
    bool frameSyncEnabled = true;

    /** @brief Set to true to deliver IMU events on a separate, dedicated background thread. Only supported for Structure Core, currently. */
    bool lowLatencyIMU = true;

    /** @brief Set to true to apply a correction filter to the depth before streaming. This may effect performance. */
    bool applyExpensiveCorrection = false;

    /** @brief StructureCore specific settings. */
    struct ST_API StructureCoreSettings
    {
        /** @brief Set to true to enable depth streaming. */
        bool depthEnabled = true;

        /** @brief Set to true to enable infrared streaming. */
        bool infraredEnabled = false;
        
        /** @brief Set to true to enable visible streaming. */
        bool visibleEnabled = false;
        
        /** @brief Set to true to enable accelerometer streaming. */
        bool accelerometerEnabled = false;
        
        /** @brief Set to true to enable gyroscope streaming. */
        bool gyroscopeEnabled = false;

        /** @brief The target resolution for streamed depth frames. @see StructureCoreDepthResolution */
        StructureCoreDepthResolution depthResolution = StructureCoreDepthResolution::Default;

        /** @brief The preset depth range mode for streamed depth frames. Modifies the min/max range of the depth values. */
        StructureCoreDepthRangeMode depthRangeMode = StructureCoreDepthRangeMode::Default;
    
        /** @brief The target resolution for streamed depth frames. @see StructureCoreInfraredResolution
            Non-default infrared and visible resolutions are currently unavailable.
        */
        StructureCoreInfraredResolution infraredResolution = StructureCoreInfraredResolution::Default;

        /** @brief The target resolution for streamed visible frames. @see StructureCoreVisibleResolution
            Non-default infrared and visible resolutions are currently unavailable.
        */
        StructureCoreVisibleResolution visibleResolution = StructureCoreVisibleResolution::Default;

        /** @brief Set to true to apply gamma correction to incoming visible frames. */
        bool visibleApplyGammaCorrection = false;

        /** @brief Enable auto-exposure for infrared frames. */
        bool infraredAutoExposureEnabled = false;

        /** @brief Specifies how to stream the infrared frames. @see StructureCoreInfraredMode */
        StructureCoreInfraredMode infraredMode = StructureCoreInfraredMode::Default;

        /** @brief The target stream rate for IMU data. (gyro and accel) */
        StructureCoreIMUUpdateRate imuUpdateRate = StructureCoreIMUUpdateRate::Default;

        /** @brief Debugging boot mode for firmware builds. */
        StructureCoreBootMode bootMode = StructureCoreBootMode::Default;

        /** @brief Serial number of sensor to stream. If null, the first connected sensor will be used. */
        const char* sensorSerial = nullptr;

        /** @brief USB file descriptor. The file descriptor needs to be valid and opened. Android-only. */
        int usbDeviceFileDescriptor = -1;

        /** @brief USB dev/bus path. This device path is informative and optional. Android-only. */
        const char* usbDevicePath = nullptr;

        /** @brief Debugging path for firmware builds. */
        const char* firmwareBootPath = nullptr;

        /** @brief Maximum amount of time (in milliseconds) to wait for a sensor to connect before throwing a timeout error. */
        int sensorInitializationTimeout = 6000;

        /** @brief The target framerate for the infrared camera. If the value is not supported, the default is 30. */
        float infraredFramerate = 30.f;

        /** @brief The target framerate for the depth sensor. If the value is not supported, the default is 30. */
        float depthFramerate    = 30.f;

        /** @brief The target framerate for the visible camera. If the value is not supported, the default is 30. */
        float visibleFramerate  = 30.f;

        /** @brief The initial visible exposure to start streaming with (milliseconds, but set in seconds). */
        float initialVisibleExposure = 0.016f;

        /** @brief The initial visible gain to start streaming with. Can be any number between 1 and 8. */
        float initialVisibleGain = 2.0f;

        /** @brief The initial infrared exposure to start streaming with. */
        float initialInfraredExposure = 0.0146f;
    
        /** @brief The initial infrared gain to start streaming with. Can be 0, 1, 2, or 3. */
        int initialInfraredGain = 3;

        /** @brief Setting this to true will eliminate saturation issues, but might result in sparser depth. */
        bool disableInfraredIntensityBalance = true;

        /** @brief Setting this to true will reduce latency, but might drop more frame */
        bool latencyReducerEnabled = true;

        /** @brief Laser projector power setting from 0.0 to 1.0 inclusive. Projector will only activate if required by streaming configuration. */
        float initialProjectorPower = 1.f;
    }
    structureCore;

    /** @brief OCC specific settings. */
    struct ST_API OCCPlaybackSettings
    {
        /** @brief The path to the OCC file. */
        const char* path = nullptr;

        /** @brief Set to true to enable streaming early color frames. */
        bool earlyColorEnabled = false;

        /** @brief Set to true to restart the OCC from the beginning, running forever until it is stopped. */
        bool autoReplay   = true;

        /** @brief The mode of playback for OCC streaming. @see CaptureSessionOCCPlaybackMode */
        CaptureSessionOCCPlaybackMode playbackMode = CaptureSessionOCCPlaybackMode::Default;

        /** @brief For non-realtime modes of OCC streaming, this will limit the FPS of streaming. */
        float rateLimit    = 30.f;
    }
    occ;

    /** @brief The type of dispatch service to use internally for data delivery. For advanced users only. */
    CaptureSessionDispatcherId dispatcher = CaptureSessionDispatcherId::Default;

    /** @brief The android looper will be auto-created for the thread calling startStreaming, when unspecified. Android-only. */
    ALooper* looper = nullptr;

    /** @brief Attempts to read presaved settings from "[Documents]/CaptureSessionSettings.txt". */
    void readSavedSettings();

    void persistCaptureSettings();
};

//------------------------------------------------------------------------------

} // ST namespace
