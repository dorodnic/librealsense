/*
    CaptureSession.h

    Copyright © 2017 Occipital, Inc. All rights reserved.
    This file is part of the Structure SDK.
    Unauthorized copying of this file, via any medium is strictly prohibited.
    Proprietary and confidential.

    http://structure.io
*/

#pragma once

#include <ST/MathTypes.h>
#include <ST/CaptureSessionTypes.h>

namespace ST
{

//------------------------------------------------------------------------------

struct CaptureSession;
struct CaptureSessionSample;

//------------------------------------------------------------------------------

/** @brief Pure-virtual class used to control where CaptureSession stream data is output to. */
struct ST_API CaptureSessionDelegate
{
    virtual ~CaptureSessionDelegate() {}

    /** @brief Gets called when any type of data is streamed from CaptureSession.
        @param session The parent CaptureSession this delegate is attached to.
        @param sample The streamed data. @see CaptureSessionSample
    */
    virtual void captureSessionDidOutputSample(CaptureSession* session, const CaptureSessionSample& sample) = 0;

    /** @brief Gets called when a lone visible frame gets sent through before tracking is running. This can be ignored for most users.
        @param session The parent CaptureSession this delegate is attached to.
        @param sample The streamed data. @see CaptureSessionSample
    */
    virtual void captureSessionDidOutputEarlySample(CaptureSession* session, const CaptureSessionSample& sample) {}

    /** @brief Gets called when an incomplete sample is dropped or streamed from CaptureSession. This can be ignored for most users.
        @param session The parent CaptureSession this delegate is attached to.
        @param sample The streamed data. @see CaptureSessionSample
    */
    virtual void captureSessionDidDropPartiallySynchronizedSample(CaptureSession* session, const CaptureSessionSample& sample) {}

    /** @brief Gets called when the state of streaming changes. @see CaptureSessionEventId
        @param session The parent CaptureSession this delegate is attached to.
        @param
    */
    virtual void captureSessionEventDidOccur(CaptureSession* session, CaptureSessionEventId event) {}
};

//------------------------------------------------------------------------------

/** @brief The main context for connecting to and streaming data from a Structure Core.

    You will need to create a CaptureSessionDelegate instance in order to receive samples.

    Example initialization:
    @code{.cpp}
    struct MyCaptureSessionDelegate : public ST::CaptureSessionDelegate {
        void captureSessionDidOutputSample (CaptureSession* session, const CaptureSessionSample& sample) override {
            switch (sample.type) {
            case ST::CaptureSessionSample::Type::SynchronizedFrames:
                // we have sync'd frames!
                // ST::Matrix4 cameraPose = ST::Matrix4::nan();
                // bridgeEngine.provideRgbdFrame(sample.depthFrame, sample.visibleFrame, cameraPose);
                break;
            case ST::CaptureSessionSample::Type::AccelerometerEvent:
                // we have accel data!
                // bridgeEngine.provideAccelData(sample.accelerometerEvent);
                break;
            case ST::CaptureSessionSample::Type::GyroscopeEvent:
                // we have gyro data!
                // bridgeEngine.provideGyroData(sample.gyroscopeEvent);
                break;
            };
        }
    };

    // configure the options to stream everything needed for tracking
    ST::CaptureSessionSettings settings;
    settings.structureCore.depthEnabled         = true;
    settings.structureCore.visibleEnabled       = true;
    settings.structureCore.accelerometerEnabled = true;
    settings.structureCore.gyroscopeEnabled     = true;

    // create your delegate class
    MyCaptureSessionDelegate delegate;

    // attach the delegate to a CaptureSession
    ST::CaptureSession captureSession;
    captureSession.setDelegate(&delegate);

    // apply settings and start streaming!
    captureSession.startMonitoring(settings);
    captureSession.startStreaming();
    @endcode
*/
struct ST_API CaptureSession
{
    /** @brief Constructor. */
     CaptureSession();
    ~CaptureSession();

    /** @brief Attach a CaptureSessionDelegate to send output samples to. */
    void setDelegate(CaptureSessionDelegate* delegate);

    /** @brief Attach a static function to receive samples. Replaces the delegate.
        Example usage:
        @code{.cpp}
        class MyClass {
            MyClass() {
                // initialization of the capture session is implied
                ...
                mCaptureSession.setOutputCallback(&function, this);
            }

            static void static_outputCallback(void* userdata, CaptureSession* session, const CaptureSessionSample& sample) {
                // will call member variable
                reinterpret_cast<MyClass*>(userdata)->captureSessionDidOutputSample(session, sample);
            }

            void captureSessionDidOutputSample(CaptureSession* session, const CaptureSessionSample& sample) {
                // do something with the data!
            }

            ST::CaptureSession mCaptureSession;
        };
        @endcode
    */
    void setOutputCallback(CaptureSessionOutputCallback callback, void* userdata);

    /** @brief Attach a static function to receive sensor events. Replaces the delegate.
        Example usage:
        @code{.cpp}
        class MyClass {
            MyClass() {
                // initialization of the capture session is implied
                ...
                mCaptureSession.setEventCallback(&sensorStatusCallbackFunc, this);
            }

            static void sensorStatusCallbackFunc(void* userdata, CaptureSession* session, const CaptureSessionEventId& event)
            {
                reinterpret_cast<MyClass*>(userdata)->captureSessionEventDidOccur(session, event);
            }

            void captureSessionEventDidOccur(CaptureSession* session, CaptureSessionEventId event) {
                // process incoming events!
            }

            ST::CaptureSession mCaptureSession;
        };
        @endcode
    */
    void setEventCallback(CaptureSessionEventCallback callback, void* userdata);

    /** @brief Initializes the CaptureSession and prepares it to start streaming data. Must be called at least once before streaming is started.
        If an error occurs, the captureSessionEventDidOccur delegate will trigger. @see CaptureSessionDelegate::captureSessionEventDidOccur
        @param settings Settings for the capture session
        @return True, if able to initialize the session using the provided settings.
    */
    bool startMonitoring(const CaptureSessionSettings& settings);

    /** @brief Attempt to start streaming data from a specified source. Streaming will start immediately after.
        If an error occurs, the captureSessionEventDidOccur delegate will trigger. @see CaptureSessionDelegate::captureSessionEventDidOccur
        @return True, if able to initialize. Otherwise, false.
    */
    bool startStreaming();

    /** @brief Stop streaming from the specified source. */
    void stopStreaming();

    /** @brief Re-initialize streaming from the specified source.
        Note that a Structure Core will be disconnected entirely for a brief moment after.
        @return True, if able to reboot the source. Otherwise, false.
    */
    bool rebootCaptureSource();

    /** @brief Obtain the serial number for a connected Structure Core. */
    const char* sensorSerialNumber() const;


    CaptureSessionUSBVersion USBVersion() const;

    /** @brief Set the exposure and gain of the visible frames from a StructureCore. */
    void setVisibleCameraExposureAndGain(float exposure, float gain);

    /** @brief Get the exposure and gain of the visible frames from a StructureCore. */
    void getVisibleCameraExposureAndGain(float* exposure, float* gain) const;

    /** @brief Set the exposure and gain of the infrared frames from a StructureCore. */
    void setInfraredCamerasExposureAndGain(float exposure, float gain);

    /** @brief Get the exposure and gain of the infrared frames from a StructureCore. */
    void getInfraredCamerasExposureAndGain(float* exposure, float* gain) const;

    /** @brief Returns the settings provided during initialization. */
    const CaptureSessionSettings& settings() const;

    void persistExposureAndGainSettings(bool persist_infrared);
    void clearSavedSettings();

public:

    ST_DECLARE_OPAQUE_INTERNALS(CaptureSession);
};

//------------------------------------------------------------------------------

}
