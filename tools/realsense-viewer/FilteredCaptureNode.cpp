#include "FilteredCaptureNode.h"
#include "simd.h"

namespace realsense_ros
{
    FilteredCaptureNode::FilteredCaptureNode()
    {
    }

    void FilteredCaptureNode::process(rs2::frameset& frames, rs2::frame_source& src)
    {
        constexpr float DOWNSAMPLE_FRACTION = 1.0f / DOWNSAMPLE_FACTOR;

        rs2::depth_frame depthOriginal = frames.get_depth_frame();
        rs2::video_frame grayOriginal = frames.get_infrared_frame(0);

        if (!depthOriginal || !grayOriginal)
        {
            //LOG(WARNING) << "No valid image retrieved";
            return;
        }

        cv::Mat matDepth(cv::Size(depthOriginal.get_width(), depthOriginal.get_height()), CV_16U, (void*)depthOriginal.get_data(), cv::Mat::AUTO_STEP);
        cv::Mat matGray(cv::Size(grayOriginal.get_width(), grayOriginal.get_height()), CV_8U, (void*)grayOriginal.get_data(), cv::Mat::AUTO_STEP);

        downsample4x4SIMDMin(matDepth, &matDepthResized_);

        cv::resize(matGray, matGrayResized_, cv::Size(), DOWNSAMPLE_FRACTION, DOWNSAMPLE_FRACTION, cv::INTER_NEAREST);
        //downsampleTimer_.stop();

        //opencvTimer_.start();

        if (!_output_ir_profile)
        {
            auto p = grayOriginal.get_profile().as<rs2::video_stream_profile>();
            auto intr = p.get_intrinsics();
            intr.width = p.width() / DOWNSAMPLE_FACTOR;
            intr.height = p.height() / DOWNSAMPLE_FACTOR;
            // TODO: Update pp and fp?
            _output_ir_profile = p.clone(p.stream_type(), p.stream_index(), p.format(),
                p.width() / DOWNSAMPLE_FACTOR, p.height() / DOWNSAMPLE_FACTOR, intr);
        }

        // make sure proper matrices are allocated and necessary resets from frame to frame are happening
        initOrResetImageMatrices(matGrayResized_);

#pragma omp parallel for
        for (int i = 0; i < NUM_SUB_IMAGES; i++)
        {
            //for(size_t i = 0; i < NUM_SUB_IMAGES; i++){
            edgeFilter(&subImages_[i]);
            harrisFilter(&subImages_[i]);
            cv::bitwise_or(subImages_[i].maskEdge, subImages_[i].maskHarris, subImages_[i].maskCombined);

            // morphology: open(src, element) = dilate(erode(src,element))
            cv::morphologyEx(subImages_[i].maskCombined, subImages_[i].maskCombined, cv::MORPH_OPEN, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3)));
            subImages_[i].depthResized.copyTo(subImages_[i].depthOutput, subImages_[i].maskCombined);
        }

        auto res_ir = src.allocate_video_frame(_output_ir_profile, grayOriginal, 0,
            grayOriginal.get_width() / DOWNSAMPLE_FACTOR, grayOriginal.get_height() / DOWNSAMPLE_FACTOR,
            grayOriginal.get_bytes_per_pixel() * grayOriginal.get_height() / DOWNSAMPLE_FACTOR);

        src.frame_ready(res_ir);

        ////opencvTimer_.stop();

        //if (!depthCameraInfo_)
        //{
        //    depthCameraInfo_ = getCameraInfoMessage(depthOriginal, 0.0f, DOWNSAMPLE_FACTOR);
        //}

        //if (!grayLeftCameraInfo_)
        //{
        //    grayLeftCameraInfo_ = getCameraInfoMessage(grayOriginal, 0.0f, DOWNSAMPLE_FACTOR);
        //}

        //publishImage(pubDepth_, matDepthOutput_, depthCameraInfo_, getFrameBaseName() + "/depth", receiveTime);
        //publishImage(pubLeft_, matGrayResized_, grayLeftCameraInfo_, getFrameBaseName() + "/stereo/left", receiveTime);
    }

    void FilteredCaptureNode::harrisFilter(SubImages* subImage)
    {
        subImage->grayResized.convertTo(subImage->grayFloat, CV_32F);

        cv::cornerHarris(subImage->grayFloat, subImage->corners, 2, 3, 0.04);

        cv::threshold(subImage->corners, subImage->corners, 300, 255, cv::THRESH_BINARY);

        subImage->corners.convertTo(subImage->maskHarris, CV_8U);
    }

    void FilteredCaptureNode::edgeFilter(SubImages* subImage)
    {
        cv::Scharr(subImage->grayResized, subImage->scharrX, CV_16S, 1, 0);
        cv::convertScaleAbs(subImage->scharrX, subImage->absScharrX);

        cv::Scharr(subImage->grayResized, subImage->scharrY, CV_16S, 0, 1);
        cv::convertScaleAbs(subImage->scharrY, subImage->absScharrY);

        cv::addWeighted(subImage->absScharrX, 0.5, subImage->absScharrY, 0.5, 0, subImage->maskEdge);

        cv::threshold(subImage->maskEdge, subImage->maskEdge, 192, 255, cv::THRESH_BINARY);
    }

    void FilteredCaptureNode::initOrResetImageMatrices(const cv::Mat& matGrayResized) {
        const int sizeX = matGrayResized.cols;
        const int sizeY = matGrayResized.rows;
        const int sizeYperSubImage = sizeY / NUM_SUB_IMAGES;
        const int sizeXtimesSizeY = sizeX * sizeY;

        bool needToReinitialize = false;
        // test on one case if something has changed - this would then apply to all other image matrices as well
        if (maskEdge_.size() != matGrayResized.size() || maskEdge_.type() != CV_8U) {
            needToReinitialize = true;
        }

        if (!needToReinitialize) {
            // this needs resetting every frame
            memset(matDepthOutput_.data, 0, 2 * sizeXtimesSizeY);
            return;
        }

        static_assert(NUM_SUB_IMAGES >= 1 && NUM_SUB_IMAGES < 64, "NUM_SUB_IMAGES value might be wrong");

        maskEdge_ = cv::Mat(matGrayResized.size(), CV_8U);
        maskHarris_ = cv::Mat(matGrayResized.size(), CV_8U);
        maskCombined_ = cv::Mat(matGrayResized.size(), CV_8U);
        matDepthOutput_ = cv::Mat(matGrayResized.size(), CV_16U);
        matGrayFloat_ = cv::Mat(matGrayResized.size(), CV_32F);
        matCorners_ = cv::Mat(matGrayResized.size(), CV_32F);
        matScharrX_ = cv::Mat(matGrayResized.size(), CV_16S);
        matScharrY_ = cv::Mat(matGrayResized.size(), CV_16S);
        matAbsScharrX_ = cv::Mat(matGrayResized.size(), CV_8U);
        matAbsScharrY_ = cv::Mat(matGrayResized.size(), CV_8U);

        // this needs to be cleared out, as we only copy to it with a mask later
        memset(matDepthOutput_.data, 0, 2 * sizeXtimesSizeY);

        for (int i = 0; i < NUM_SUB_IMAGES; i++) {
            cv::Rect rect(0, i * sizeYperSubImage, sizeX, sizeYperSubImage);

            subImages_[i].depthResized = matDepthResized_(rect);
            subImages_[i].grayResized = matGrayResized_(rect);
            subImages_[i].maskEdge = maskEdge_(rect);
            subImages_[i].maskHarris = maskHarris_(rect);
            subImages_[i].maskCombined = maskCombined_(rect);
            subImages_[i].depthOutput = matDepthOutput_(rect);
            subImages_[i].grayFloat = matGrayFloat_(rect);
            subImages_[i].corners = matCorners_(rect);
            subImages_[i].scharrX = matScharrX_(rect);
            subImages_[i].scharrY = matScharrY_(rect);
            subImages_[i].absScharrX = matAbsScharrX_(rect);
            subImages_[i].absScharrY = matAbsScharrY_(rect);
        }
    }

}
