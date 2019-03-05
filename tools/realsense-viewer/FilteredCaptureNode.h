#pragma once

#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>

namespace realsense_ros
{

    class FilteredCaptureNode
    {
    public:
        FilteredCaptureNode();

        void process(rs2::frameset& frames, rs2::frame_source& src);

    private:
        static constexpr float DOWNSAMPLE_FACTOR = 4.0f;
        static constexpr size_t NUM_SUB_IMAGES = 2; // this number directly relates to how many threads are used

        struct SubImages {
            cv::Mat depthResized;
            cv::Mat grayResized;
            cv::Mat maskEdge;
            cv::Mat maskHarris;
            cv::Mat maskCombined;
            cv::Mat depthOutput;
            cv::Mat grayFloat;
            cv::Mat corners;
            cv::Mat scharrX;
            cv::Mat scharrY;
            cv::Mat absScharrX;
            cv::Mat absScharrY;
        };

        SubImages subImages_[NUM_SUB_IMAGES];

        void harrisFilter(SubImages* subImage);
        void edgeFilter(SubImages* subImage);

        // To avoid re-allocation every cycle some matrices are members
        cv::Mat matDepthResized_;
        cv::Mat matGrayResized_;
        cv::Mat maskEdge_;
        cv::Mat maskHarris_;
        cv::Mat maskCombined_;
        cv::Mat matDepthOutput_;
        cv::Mat matGrayFloat_;
        cv::Mat matCorners_;
        cv::Mat matScharrX_;
        cv::Mat matScharrY_;
        cv::Mat matAbsScharrX_;
        cv::Mat matAbsScharrY_;

        rs2::depth_frame _depthOriginal;
        rs2::video_frame _grayOriginal;

        rs2::stream_profile _output_ir_profile;
        rs2::stream_profile _output_depth_profile;

        void initOrResetImageMatrices(const cv::Mat& matGrayResized);
    };

}
