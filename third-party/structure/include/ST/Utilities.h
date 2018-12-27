/*
    Utilities.h

    Copyright © 2017 Occipital, Inc. All rights reserved.
    This file is part of the Structure SDK.
    Unauthorized copying of this file, via any medium is strictly prohibited.
    Proprietary and confidential.

    http://structure.io
*/

#pragma once

#include <ST/Macros.h>
#include <string>
#include <functional>
#include <cstdarg>
#include <array>
#include <vector>

namespace ST
{
    /** @brief Expands defined templates into valid file paths,
     * For example, `resolveSmartPath("[AppDocuments]/data/file.txt");`
       will return "C:\Users\you\Documents\data\file.txt".
        @param inputPath Shorthanded path to a file or directory.
        @return Expanded absolute path to the given inputPath.
    */
    ST_API std::string resolveSmartPath(const std::string& inputPath);

    /** @brief Path of the output folder for profiling logs. */
    ST_API const char* createdProfilingFolder();

    /** @brief Utility function to get a formatted local time string, using std::put_time.
        @param format The structured format of the output string, see std::put_time for
       more information. The default format looks like: "2017-02-15_11-45-38".
        @return A string detailing the local time using the format specified.
    */
    ST_API std::string formattedStringFromLocaltime(const std::string& format = "%Y-%m-%d_%H-%M-%S");

    /** @brief Get the current time in seconds in the time reference of BridgeEngine.
        Useful for retrieving the current time for pose prediction.
    */
    ST_API double getTimestampNow();

    /** @brief Sets the internal GUI-based debugging verbosity. 1 is default, -1 will disable the HUD entirely. */
    ST_API bool setVisualLoggingVerbosity(int verbosity = 1);

    /** @brief Sets the internal console-based debugging verbosity. 0 is default, 1 will likely slow your application. */
    ST_API bool setConsoleLoggingVerbosity(int verbosity = 0);

    /** @brief Sets the internal console-based debugging/logging system to write out logs to a file. This is disabled by default.
        The logs will be located in [AppDocuments]/occ on Windows and Linux, and "/data/occ" on Android.
    */
    ST_API void setConsoleLoggingToWriteToFile(bool writeLogsToFile = false);

    /** @brief Initialize visual logging. Safe to call multiple times. */
    ST_API void initializeVisualLogging();

    /** @brief Forces all visual logging windows to hide themselves. */
    ST_API void hideAllVisualLoggingWindows();

    /** @brief Forces all visual logging windows to show themselves. */
    ST_API void showAllVisualLoggingWindows();

    /** @brief Returns true if the visual logging windows are visible. */
    ST_API bool areVisualLoggingWindowsShown();

    /** @brief Returns true if the two floats are equal within floating point error. */
    ST_API bool floatEquals(float rhs, float lhs);

    //------------------------------------------------------------------------------

    struct OnlineMeanEstimator
    {
        double n = 0.;
        double mean = 0.;
        double minValue = std::numeric_limits<double>::max ();  // makes sure it will get
                                                                // overwritten by the
                                                                // first value
        double maxValue = std::numeric_limits<double>::lowest ();  // make sure it will
                                                                   // get overwritten by
                                                                   // the first value

        // From Knuth
        // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm.
        void update (double x)
        {
            mean = (mean * n + x) / (n + 1.0);
            n += 1.0;

            minValue = std::min (minValue, x);
            maxValue = std::max (maxValue, x);
        }

        void update (const OnlineMeanEstimator& rhs)
        {
            if (rhs.n < 0.5) return;
            mean = (mean * n + rhs.mean * rhs.n) / (n + rhs.n);
            n += rhs.n;

            minValue = std::min (rhs.minValue, minValue);
            maxValue = std::max (rhs.maxValue, maxValue);
        }
    };

    //------------------------------------------------------------------------------

    // Utility class to monitor the performance of a real-time system.
    class ST_API PerformanceMonitor
    {
    public:
        PerformanceMonitor ();
        ~PerformanceMonitor ();

    public:
        double averageFpsFromFrameCount () const;
        double samplingPeriodInSeconds () const;
        int numSamples () const;

        double meanValue () const;
        double minValue () const;
        double maxValue () const;

        double meanPeriod () const;
        double minPeriod () const;
        double maxPeriod () const;

        double timestampOfFirstSample() const;
        double valueOfLastSample() const;

    public:
        void addSampleWithTimestamp (const double sampleTimestamp,
                                     const double sampleValue);

        void addSample (double sampleValue)
        {
            addSampleWithTimestamp (getTimestampNow (), sampleValue);
        }

        void startNewSequenceFromLastSample ();

    private:
        ST_DECLARE_OPAQUE_INTERNALS (PerformanceMonitor);
    };

    //------------------------------------------------------------------------------

    class ScopeTimeForPerformanceMonitor
    {
    public:
        ScopeTimeForPerformanceMonitor (
            PerformanceMonitor& monitor,
            double samplingPeriodBeforeCallbackAndRestart = 1.0,
            std::function<void(const PerformanceMonitor& monitor)> callBackAfterPeriod =
                nullptr)
            : _monitor (monitor), _startTime (getTimestampNow ()),
              _samplingPeriodBeforeCallbackAndRestart (
                  samplingPeriodBeforeCallbackAndRestart),
              _callBackAfterPeriod (callBackAfterPeriod)
        {
        }

        ~ScopeTimeForPerformanceMonitor ()
        {
            _monitor.addSample (getTimestampNow () - _startTime);

            if (_monitor.samplingPeriodInSeconds () >
                _samplingPeriodBeforeCallbackAndRestart)
            {
                if (_callBackAfterPeriod) _callBackAfterPeriod (_monitor);
                _monitor.startNewSequenceFromLastSample ();
            }
        }

    private:
        PerformanceMonitor& _monitor;
        double _startTime = NAN;
        double _samplingPeriodBeforeCallbackAndRestart = NAN;
        std::function<void(const PerformanceMonitor& monitor)> _callBackAfterPeriod = nullptr;
    };

    //------------------------------------------------------------------------------

    // Use the macros below instead
    class ST_API TimeCountDoNotUseDirectly
    {
    public:
        TimeCountDoNotUseDirectly (const std::string& name, int debug_level = 1,
                                   double minDurationInSecondToShow = -1.0 /* no min */);
        ~TimeCountDoNotUseDirectly ();

        double elapsedMsecs (const std::string& marker = "") const;

        double elapsedMsecsNoPrint () const;

        double stop (const std::string& marker = "");

    private:
        ST_DECLARE_OPAQUE_INTERNALS(TimeCountDoNotUseDirectly);
    };

#define ST_TIMECOUNT(VarName, Name, DebugLevel)                                          \
    ST::TimeCountDoNotUseDirectly VarName (Name, DebugLevel)
#define ST_TIMECOUNT_IF_HIGHER_THAN(VarName, Name, DebugLevel, MinDurationSeconds)       \
    ST::TimeCountDoNotUseDirectly VarName (Name, DebugLevel, MinDurationSeconds)
#define ST_TIMECOUNT_ELAPSED(VarName, Marker) VarName.elapsedMsecs (Marker)
#define ST_TIMECOUNT_STOP(VarName) VarName.stop ()

    /** 
        The profiling file will go in the same folder as all the Occipital profiler output, 
        usually under AppDocuments/Occipital/profiling .
    */
    struct ST_API ProfilingFileLogger
    {
        ProfilingFileLogger(const std::string& fileName, const char* firstLine);
        ~ProfilingFileLogger();
        void printf(const char * format, ...);

    private:
        ST_DECLARE_OPAQUE_INTERNALS(ProfilingFileLogger);
    };

    //------------------------------------------------------------------------------

    // Fixed-size strings, compatible with std::atomic.
    template <unsigned N>
    struct StringNBytes : public std::array<char, N>
    {
        StringNBytes() noexcept /* noexcept required to be used in an atomic */
        {
            this->fill(0);
        }

        StringNBytes(const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            vsnprintf(this->data(), N, format, args);
            va_end(args);
        }

        // Null-terminated.
        bool empty() const { return (*this)[0] == 0; }
    };
    using String64Bytes = StringNBytes<64>;
    using String128Bytes = StringNBytes<128>;

    //------------------------------------------------------------------------------

    /** @brief Information describing a connected Structure Core device. */
    struct ST_API ConnectedSensorInfo
    {
        /** @brief Name of sensor manufacturer */
        std::string manufacturer;
        /** @brief Product name of sensor */
        std::string product;
        /** @brief Serial number of sensor */
        std::string serial;
        /** @brief Is the sensor booted? Always true in normal usage. */
        bool booted = false;
        /** @brief Is the sensor available to use? False if already in use by another process. */
        bool available = false;
    };

    /** @brief List all connected Structure Core devices. Unavailable on Android. */
    ST_API std::vector<ConnectedSensorInfo> enumerateConnectedSensors();
}  // ST namespace
