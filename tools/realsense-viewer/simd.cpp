#include "simd.h"

#include <emmintrin.h>
#include <smmintrin.h>


namespace realsense_ros
{

    // SSE version is about 50% slower than the AVX2 version on NUC7i7BNH
    void downsample4x4SIMDMin(const cv::Mat& source, cv::Mat* pDest)
    {
        if (source.cols % 8 != 0)
        {
            //LOG(ERROR) << __func__ << "Input image width must be multiple of 8 (found " << source.cols << ")";
            return;
        }

        if (source.rows % 4 != 0)
        {
            //LOG(ERROR) << __func__ << "Input image height must be multiple of 4 (found " << source.rows << ")";
            return;
        }

        if (source.type() != CV_16U)
        {
            //LOG(ERROR) << __func__ << "Invalid input image type (only CV_16U supported)";
            return;
        }

        if (pDest->cols != source.cols / 4 || pDest->rows != source.rows / 4 || pDest->type() != CV_16U)
        {
            *pDest = cv::Mat(source.rows / 4, source.cols / 4, CV_16U);
        }

        __m128i ones = _mm_set1_epi16(1);

        // Note on multi-threading here, 2018-08-17
        // This function is called for every depth image coming from RealSense
        // without MT this function takes on Joule in average 0.47 ms
        // with    MT this function takes on Joule in average 0.15 ms
        const size_t sizeYDiv4const = source.rows / 4;
#pragma omp parallel for
        for (int y = 0; y < sizeYDiv4const; y++)
        {
            for (uint16_t x = 0; x < source.cols; x += 8) {
                const int newY = y * 4;
                // load data rows
                __m128i A = _mm_loadu_si128((const __m128i*)&source.at<uint16_t>(newY, x));
                __m128i B = _mm_loadu_si128((const __m128i*)&source.at<uint16_t>(newY + 1, x));
                __m128i C = _mm_loadu_si128((const __m128i*)&source.at<uint16_t>(newY + 2, x));
                __m128i D = _mm_loadu_si128((const __m128i*)&source.at<uint16_t>(newY + 3, x));

                // subtract 1 to shift invalid pixels to max value (16bit integer underflow)
                A = _mm_sub_epi16(A, ones);
                B = _mm_sub_epi16(B, ones);
                C = _mm_sub_epi16(C, ones);
                D = _mm_sub_epi16(D, ones);

                // calculate minimum
                __m128i rowMin = _mm_min_epu16(D, C);
                rowMin = _mm_min_epu16(rowMin, B);
                rowMin = _mm_min_epu16(rowMin, A);

                __m128i shuf32 = _mm_shuffle_epi32(rowMin, _MM_SHUFFLE(2, 3, 0, 1));

                __m128i min32 = _mm_min_epu16(rowMin, shuf32);

                __m128i shuf16 = _mm_shufflelo_epi16(min32, _MM_SHUFFLE(3, 2, 0, 1));
                shuf16 = _mm_shufflehi_epi16(shuf16, _MM_SHUFFLE(3, 2, 0, 1));

                __m128i min2 = _mm_min_epu16(min32, shuf16);

                // undo invalid pixel shifting by adding one
                min2 = _mm_add_epi16(min2, ones);

                uint16_t minA = _mm_extract_epi16(min2, 0);
                uint16_t minB = _mm_extract_epi16(min2, 4);

                pDest->at<uint16_t>(y, x / 4) = minA;
                pDest->at<uint16_t>(y, x / 4 + 1) = minB;
            }
        }
    }
}
