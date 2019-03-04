#pragma once

#include <opencv2/opencv.hpp>

namespace realsense_ros
{
    void downsample4x4SIMDMin(const cv::Mat& source, cv::Mat* pDest);
}
