/*
    CameraFrames.h

    Copyright © 2017 Occipital, Inc. All rights reserved.
    This file is part of the Structure SDK.
    Unauthorized copying of this file, via any medium is strictly prohibited.
    Proprietary and confidential.

    http://structure.io
*/

#pragma once

#include <ST/Macros.h>
#include <ST/MathTypes.h>
#include <cstdint>

namespace ST
{

//------------------------------------------------------------------------------

// forward declarations
struct Intrinsics;
struct Mesh;

//------------------------------------------------------------------------------


/** @brief Container class for infrared frame data. */
struct ST_API InfraredFrame
{
     InfraredFrame();
    ~InfraredFrame();

    /** @brief Returns true if we have data. */
    bool isValid() const;

    /** @brief Returns the width of the image. */
    int width() const;

    /** @brief Returns the height of the image. */
    int height() const;

    /** @brief Returns a const-ptr to the underlying 16bit data. */
    const uint16_t* data() const;

    /** @brief Returns the timestamp of the image data (middle exposure). */
    double timestamp() const;

    /** @brief Returns the timestamp of when it was received in the CaptureSession. */
    double arrivalTimestamp() const;

    /** @brief Returns the infrared intrinsics for this frame. */
    const Intrinsics intrinsics() const;

    ST_DECLARE_CLONABLE_OPAQUE_INTERNALS(InfraredFrame);
};

/** @brief Container class for color frame data. */
struct ST_API ColorFrame
{
     ColorFrame();
    ~ColorFrame();

    /** @brief Returns true if we have data. */
    bool isValid() const;

    /** @brief Returns the width of the image. */
    int width() const;

    /** @brief Returns the height of the image. */
    int height() const;

    /** @brief Returns a const-ptr to the underlying y channel data. */
    const uint8_t* yData() const;

    /** @brief Returns the size of the y channel data in bytes. */
    size_t         ySize() const;

    /** @brief Returns a const-ptr to the underlying y channel data. */
    const uint8_t* cbcrData() const;

    /** @brief Returns the size of the y channel data in bytes. */
    size_t         cbcrSize() const;

    /** @brief Returns a const-ptr to the underlying RGB data. */
    const uint8_t* rgbData() const;

    /** @brief Returns the size of the RGB data in bytes. */
    size_t         rgbSize() const;


    /** @brief Returns the timestamp of the image data (middle exposure). */
    double timestamp() const;

    /** @brief Returns the timestamp of the image data (end of exposure). */
    double endOfExposureTimestamp() const;

    /** @brief Returns the timestamp of when it was received in the CaptureSession. */
    double arrivalTimestamp() const;

    /** @brief Returns the timestamp of when the frame was ready to get dispatched to the non-early delegate. */
    double endOfPreprocessingTimestamp() const;

    /** @brief Create a projection matrix from the intrinsics of the image to be used in OpenGL functions. */
    Matrix4 glProjectionMatrix() const;

    /** @brief Returns a copy of the image, but undistorted. */
    ColorFrame undistorted() const;

    /** @brief Returns the color intrinsics for this frame. */
    const Intrinsics intrinsics() const;


    ST_DECLARE_CLONABLE_OPAQUE_INTERNALS(ColorFrame);
};

struct ST_API DepthFrame
{
     DepthFrame();
    ~DepthFrame();

    /** @brief Returns true if we have data. */
    bool isValid() const;

    /** @brief Returns the width of the image. */
    int width() const;

    /** @brief Returns the height of the image. */
    int height() const;

    /** @brief Returns const-ptr of the shift data. Only supported for first-generation Structure Sensor, not Structure Core. */
    const uint16_t* shiftData() const;

    /** @brief Contiguous chunk of `width * height` float depth values in millimeters. */
    const float* depthInMillimeters() const;

    /** @brief Returns depth in millimeters at (x,y) if it exists, NAN otherwise. */
    float operator()(int x, int y) const;


    /** @brief Returns the timestamp of the image data (middle exposure). */
    double timestamp() const;

    /** @brief Returns the timestamp of when it was received in the CaptureSession. */
    double arrivalTimestamp() const;

    /** @brief Create a projection matrix from the intrinsics of the image to be used in OpenGL functions. */
    Matrix4 glProjectionMatrix() const;

    /** @brief Return true if this frame is registered to the input color frame. */
    bool isRegisteredTo(const ColorFrame& cameraFrame) const;

    /** @brief Returns a transformation matrix to bring a color frame into the depth frame of reference. */
    Matrix4 colorCameraPoseInDepthCoordinateFrame() const;

    /** @brief Returns the 3d point in the image at row, col (x, y). */
    Vector3f unprojectPoint(int row, int col) const;

    /** @brief Runs correction and cleanup methods internally. For advanced users only. */
    bool applyExpensiveCorrection();

    /** @brief Returns the depth intrinsics for this frame. */
    const Intrinsics intrinsics() const;

    ST_DECLARE_CLONABLE_OPAQUE_INTERNALS(DepthFrame);
};

//------------------------------------------------------------------------------

struct ST_API MultiCameraColorFrame
{
    MultiCameraColorFrame();
    ~MultiCameraColorFrame();

    bool isValid(int index) const;

    int width(int index) const;
    int height(int index) const;

    ColorFrame getColorFrame(int index) const;

    double timestamp(int index) const;
    double arrivalTimestamp(int index) const;
    double endOfExposureTimestamp(int index) const;
    double endOfPreprocessingTimestamp(int index) const;

    int getNumberOfCameras() const;

    ColorFrame undistorted(int index) const;


    ST_DECLARE_CLONABLE_OPAQUE_INTERNALS(MultiCameraColorFrame);
};

//------------------------------------------------------------------------------

struct ST_API Intrinsics
{
    /** @brief Returns a zero-intrinsics struct. Used for blank initialization. */
    static const Intrinsics Zero();

    /** @brief Returns a zero-intrinsics struct with a valid width and height. Used for blank initialization. */
    static const Intrinsics Identity(int width, int height);

    /** @brief Width of the image. */
    int32_t width = 0;

    /** @brief Height of the image. */
    int32_t height = 0;
        
    /** @brief X focal length, in pixels. */
    float fx = NAN;

    /** @brief Y focal length, in pixels. */
    float fy = NAN;

    /** @brief Principal point x-coordinate, in float coordinate space. */
    float cx = NAN;

    /** @brief Principal point y-coordinate, in float coordinate space. */
    float cy = NAN;
    
    /** @brief Distortion parameters. */
    float k1 = NAN, k2 = NAN, p1 = NAN, p2 = NAN, k3 = NAN;

    /** @brief Returns true if the values are not NAN or zero. */
    bool isValid() const;

    /** @brief Returns true if the p1 and p2 distortion coefficients are 0. */
    bool p1p2AreZero() const;

    /** @brief Returns true if the k3, p1 and p2 distortion coefficients are 0. */
    bool k3p1p2AreZero() const;
    
    /** @brief Returns true if all of the distortion coefficients are 0. */
    bool k1k2k3p1p2AreZero() const;
        
    /** @brief Utility method for comparing the distortion coefficients of two Intrisics.
        @return True, if the distortion coefficients match. Otherwise, false.
    */
    bool hasSameDistortionAs(const Intrinsics& rhs) const;

    /** @brief Returns the inverse Intrinsic. */
    Intrinsics inverted();

    /** @brief Sets all distortion coefficients to 0 and disables undistortion. */
    void setZeroDistortion();
    
    /** @brief Utility method for setting an Intrinsic to 0. */
    void setIdentity(int width, int height);

    /** @brief Comparative strategy for distortion. */
    enum class MatchingStrategy
    {
        /** Try to match the vertical FOV conservatively, making sure there won't be any border. */
        VerticalFieldOfViewOf,

        /** Try to match the horizontal FOV, potentially accepting a vertical border. */
        AtLeastTheHorizontalFieldOfViewOf,

        /** Try to match the full FOV. */
        AtLeastTheFullFieldOfViewOf,
    };

    /** @brief Convert the distortion coefficients using a predefined intrinsics to try and match from.
        @param matching The MatchingStrategy to use when converting.
        @param targetFOVIntrinsics The predefined target intrinsics to match to.
    */
    void setZeroDistortion(MatchingStrategy matching, const Intrinsics& targetFOVIntrinsics);
    void setZeroDistortionAndHorizontalFOV(float hfovInDegrees);

    /** @brief Methods for Intrinsic creation using a fill technique. */
    enum class FillingStrategy
    {
        DownsamplingWithAveraging,
        DownsamplingWithAveragingAndCropping,
        DecimatingWithoutAveraging,
    };

    /** @brief Self-populate with intrinsics obtained by performing the fill strategy on the input parameters, in order to reach the specified width & height.
        Be careful that you meet the assumptions of this method or the intrinsics will be wrong.
    */
    void fillFrom(FillingStrategy filling, const Intrinsics& other, int newImageWidth, int newImageHeight);

    /** @brief Return intrinsics for an image size as close as possible to 320x240, but making sure it's a round scale factor.
        For example, calling this on a 648x484 image will return intrinsics for a 324x242 image.
        This means one can simply transform from the source image to the new image by using a scale factor on each dimension, no offset handling needed.
    */
    Intrinsics closestRoundQVGALikeIntrinsics() const;

    Intrinsics() = default;
    Intrinsics(const Intrinsics&) = default;
    Intrinsics(int width, int height, float fx, float fy, float cx, float cy, float k1, float k2, float k3, float p1, float p2);
    bool operator==(const Intrinsics& rhs) const;
    bool operator!=(const Intrinsics& rhs) const;
    Intrinsics& operator=(const Intrinsics&) & = default;
};

} // ST namespace
