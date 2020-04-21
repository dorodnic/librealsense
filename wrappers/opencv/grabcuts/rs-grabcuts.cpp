// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/rsutil.h> // Include RealSense Cross Platform API
#include <opencv2/opencv.hpp>   // Include OpenCV API
#include "../cv-helpers.hpp"    // Helper functions for conversions between RealSense and OpenCV

#include <algorithm>
#include <deque>
#include <numeric>
#include <chrono>

#include <thread>
#include <mutex>
#include <atomic>

struct point
{
    float x;
    float y;
    float z;
    float w;
};

point zero() { return { 0.f, 0.f, 0.f, 0.f }; }

point operator+(const point& a, const point& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

point operator*(float f, const point& b)
{
    return { f * b.x, f * b.y, f * b.z, f * b.w };
}


float distance(const point& a, const point& b)
{
    auto x = a.x - b.x;
    auto y = a.y - b.y;
    auto z = a.z - b.z;
    auto w = a.w - b.w;
    return sqrt(x*x + y*y + z*z + w*w);
}

// simple linear interpolation between two points
void lerp(point& dest, const point& a, const point& b, const float t)
{
    dest.x = a.x + (b.x-a.x)*t;
    dest.y = a.y + (b.y-a.y)*t;
    dest.z = a.z + (b.z-a.z)*t;
    dest.w = a.w + (b.w-a.w)*t;
}

// evaluate a point on a bezier-curve. t goes from 0 to 1.0
point bezier(const point& a, const point& b, const point& c, const point& d, const float t)
{
    point ab,bc,cd,abbc,bccd, dest;
    lerp(ab, a,b,t);           // point between a and b (green)
    lerp(bc, b,c,t);           // point between b and c (green)
    lerp(cd, c,d,t);           // point between c and d (green)
    lerp(abbc, ab,bc,t);       // point between ab and bc (blue)
    lerp(bccd, bc,cd,t);       // point between bc and cd (blue)
    lerp(dest, abbc,bccd,t);   // point on the bezier-curve (black)
    return dest;
}

point clean_data(std::deque<point>& data, float t, point& a, point& b, point& c, point& d)
{
    const int max_size = 8;
    if (data.size() > max_size)
    {
        auto sum = std::accumulate(data.begin(), data.end(), zero());
        auto mean = (1.f / data.size()) * sum;
        sort(data.begin(), data.end(), 
        [&](const point& a, const point& b)
        { 
            return distance(a, mean) > distance(b, mean);
        });


        // for (auto&& p : data)
        // {
        //     std::cout << distance(p, mean) << " ";
        // }
        // std::cout << std::endl;

        data.resize(data.size() * 0.8f);
        sort(data.begin(), data.end(), 
        [&](const point& a, const point& b)
        { 
            return a.w > b.w;
        });

    }

    while (data.size() > max_size) data.pop_back();
    
    if (data.size() > 3)
    {
        int step = data.size() / 4;
        a = data[0];
        b = data[step];
        c = data[2 * step];
        d = data[3 * step];

        auto delta = a.w - d.w;
        auto alpha = (t - d.w) / delta;

        //alpha = 1.3f + atan(alpha - 1.3f) / M_PI;
        
        return bezier(d, c, b, a, alpha);
    }
    else if (data.size())
    {
        a = data.front();
        b = data.front();
        c = data.front();
        d = data.front();
        return data.front();
    }
    else 
    {
        a = zero();
        b = zero();
        c = zero();
        d = zero();
        return zero();
    }
}

void draw_point(cv::Mat& m, rs2_intrinsics* intr, point xyz, int r, int g, int b, int size = 1)
{
    float pixel[2];
    float pt[3];
    pt[0] = xyz.x;
    pt[1] = xyz.y;
    pt[2] = xyz.z;
    rs2_project_point_to_pixel(pixel, intr, pt);
    pixel[0] = (pixel[0] / intr->width) * m.cols;
    pixel[1] = (pixel[1] / intr->height) * m.rows;
    float x = pixel[0];
    float y = pixel[1];

    using namespace cv;
    cv::line( m, Point( x, y-size ),
                Point( x, y+size ),
                Scalar( r, g, b), 2, 8, 0  );
    cv::line( m, Point( x-size, y ),
            Point( x+size, y ),
            Scalar( r, g, b), 2, 8, 0  );
}

int main(int argc, char * argv[]) try
{
    using namespace cv;
    using namespace rs2;
    using namespace std::chrono;

    // Define colorizer and align processing-blocks
    colorizer colorize;
    colorize.set_option(RS2_OPTION_HISTOGRAM_EQUALIZATION_ENABLED, 0.f);
    colorize.set_option(RS2_OPTION_COLOR_SCHEME, 3.f);
    colorize.set_option(RS2_OPTION_MIN_DISTANCE, 0.3f);
    colorize.set_option(RS2_OPTION_MAX_DISTANCE, 3.f);
    align align_to(RS2_STREAM_COLOR);

    rs2_intrinsics intr;

    pointcloud pc;

    // Start the camera
    pipeline pipe;
    config cfg;
    cfg.enable_device_from_file("/home/sergey/Documents/20200421_092527.bag");
    cfg.enable_stream(RS2_STREAM_DEPTH, 0, 0, RS2_FORMAT_Z16, 60);
    cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_RGB8, 60);
    pipe.start(cfg);

    const auto window_name = "Display Image";
    namedWindow(window_name, WINDOW_AUTOSIZE);

    CascadeClassifier face_cascade;
    face_cascade.load("haarcascade_frontalface_alt2.xml");

    int histSize = 256;
    float range[] = { 1, 255 }; //the upper boundary is exclusive
    const float* histRange = { range };
    bool uniform = true, accumulate = false;
    Mat hist;

    std::deque<float> zvalues;


    float dist = 0.f;
    int fx = 0; int fy = 0;

    // Skips some frames to allow for auto-exposure stabilization
    for (int i = 0; i < 10; i++) pipe.wait_for_frames();

    auto zero_time = high_resolution_clock::now();

    Mat output_mat = cv::Mat::zeros(Size(640, 480), CV_8U);

    int hist_w = 512, hist_h = 400;

    std::mutex mut;
    std::atomic<int> stop { 0 };

    std::deque<point> points;
    frame_queue q(1);

    std::mutex faces_mut;
    std::vector<Rect> faces_orig;

    std::deque<int> detection_history;

    std::thread face_detector([&](){
        while(!stop)
        {
            auto f = q.wait_for_frame();
            auto mat = frame_to_mat(f);

            std::vector<Rect> faces;
            face_cascade.detectMultiScale(mat, faces);

            faces_mut.lock();
            faces_orig = faces;
            faces_mut.unlock();
        }
    });

    std::thread t([&](){

        while (!stop){
        frameset data = pipe.wait_for_frames();
        // Make sure the frameset is spatialy aligned 
        // (each pixel in depth image corresponds to the same pixel in the color image)

        auto start = high_resolution_clock::now();

        //frameset aligned_set = align_to.process(data);
        frame depth = data.get_depth_frame();

        q.enqueue(data.get_color_frame());

        auto color_intr = data.get_color_frame().get_profile().as<rs2::video_stream_profile>().get_intrinsics();

        auto extr = data.get_color_frame().get_profile().get_extrinsics_to(depth.get_profile());
        auto extr1 = data.get_color_frame().get_profile().get_extrinsics_to(depth.get_profile());

        auto color_mat = frame_to_mat(data.get_color_frame());
        auto colorized_depth = colorize.process(depth);
        auto depth_mat = frame_to_mat(colorized_depth);

        auto ms1 = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
        
        faces_mut.lock();
        auto faces = faces_orig;
        faces_mut.unlock();

        auto ms2 = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

        auto points_3d = pc.calculate(depth);

        
        cv::Mat mask = cv::Mat::zeros(depth_mat.size(), CV_8U);
        auto h = depth.as<rs2::video_frame>().get_height();
        auto w = depth.as<rs2::video_frame>().get_width();
        auto vs = points_3d.get_vertices();


        auto depth_cpy = frame_to_mat(colorized_depth);
        cvtColor(depth_cpy, depth_cpy, COLOR_BGR2GRAY);


        cvtColor(depth_mat, depth_mat, COLOR_BGR2GRAY);


        if (faces.size())
        {
            auto* fc = &faces[0];
            int max_size = 0;

            for (auto&& face : faces)
            {
                if (face.width * face.height > max_size)
                {
                    max_size = face.width * face.height;
                    fc = &face;
                }
            }

            Point center(fc->x + fc->width*0.5, fc->y + fc->height*0.5);

            dist = depth_mat.at<uint8_t>(center);
            fx = center.x;
            fy = center.y;

            std::cout << "Before: " << fx << ", " << fy << "\n";

            float pixel[2];
            float from[2];
            from[0] = fx;
            from[1] = fy;

            rs2_project_color_pixel_to_depth_pixel(pixel, 
                (uint16_t*)depth.get_data(), depth.as<rs2::depth_frame>().get_units(), 
                0.2f, 3.f, &intr, &color_intr, &extr, &extr1, from);
            fx = pixel[0];
            fy = pixel[1];

            std::cout << "After: " << fx << ", " << fy << "\n";
        }        

        auto fv = vs[fy * w + fx];

        for (int j = 0; j < h; j++)
        {
            for (int i = 0; i < w; i++)
            {
                auto v = vs[j * w + i];
                if (v.x || v.y || v.z)
                {
                    auto d = (v.x - fv.x)*(v.x - fv.x) + (v.y - fv.y)*(v.y - fv.y) + (v.z - fv.z)*(v.z - fv.z);
                    if (d < 1.f)
                        mask.at<uint8_t>(Point(i, j)) = 255;
                }
            }
        }



        auto ms3 = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

        //std::cout << dist << std::endl;


        cv::Mat dstImage = cv::Mat::zeros(depth_mat.size(), depth_mat.type());   

        cv::Mat dstImage2 = cv::Mat::zeros(depth_mat.size(), depth_mat.type());   

        depth_mat.copyTo(dstImage, depth_mat <= dist);

        dstImage.copyTo(dstImage2, mask);

        dstImage2.setTo(cv::Scalar(255), dstImage2 == 0);

        std::vector<uint8_t> array;
        if (depth_mat.isContinuous()) {
            array.assign((uint8_t*)dstImage2.datastart, (uint8_t*)dstImage2.dataend);
        }
        else {
            for (int i = 0; i < dstImage2.rows; ++i) {
                array.insert(array.end(), dstImage2.ptr<uint8_t>(i), dstImage2.ptr<uint8_t>(i) + dstImage2.cols);
            }
        }

        array.erase(std::remove_if(
            array.begin(), array.end(),
            [](const uint8_t& x) {
            return x > 254; // put your condition here
        }), array.end());

        double min, max;
        cv::minMaxLoc(dstImage2, &min, &max);
        uint8_t fg = min;
        uint8_t bg = *max_element(std::begin(array), std::end(array));
        //uint8_t length = bg - fg;
        //bg = fg + 0.7 * length;

        std::vector<bool> is_bg(array.size(), false);
        std::vector<uint8_t>::iterator fg_start;
        bool stop = false;
        for (int i = 0; i < 10 && !stop; i++)
        {
            //std::cout << (int)fg << ", " << (int)bg << std::endl;
            stop = true;
            for (int j = 0; j < array.size(); j++)
            {
                auto new_val = fabs(array[j] - fg) > fabs(bg - array[j]);
                if (new_val != is_bg[j]) stop = false;
                is_bg[j] = new_val;
            }
            fg_start = std::stable_partition(array.begin(), array.end(), [&](const uint8_t& x) {
                size_t index = &x - &array[0];
                return is_bg[index];
            });

            auto bg_size = std::distance(begin(array), fg_start);
            auto sum = std::accumulate(array.begin(), fg_start, 0.0);
            double mean = sum / bg_size;
            bg = mean;

            auto fg_size = array.size() - bg_size;
            sum = std::accumulate(fg_start, array.end(), 0.0);
            mean = sum / fg_size;
            fg = mean;
        }

        float sum = 0.f;
        int count = 0;
        for (int j = 0; j < array.size(); j++)
        {
            if (!is_bg[j])
            {
                sum += fabs(array[j] - fg);
                count++;
            }
        }

        float fg_mean = count > 0 ? sum / count : 0.f;



        auto ms4 = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

        //std::cout << fg_mean << std::endl;

        calcHist( &dstImage2, 1, 0, Mat(), hist, 1, &histSize, &histRange, uniform, accumulate );

        Mat histImage( hist_h, hist_w, CV_8UC3, Scalar( 255,255,255) );
        normalize(hist, hist, 0, histImage.rows, NORM_MINMAX, -1, Mat() );



        auto ms5 = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();


        int bin_w = cvRound( (double) hist_w/histSize );

        for( int i = 1; i < histSize; i++ )
        {
            line( histImage, Point( bin_w*(i-1), hist_h - cvRound(hist.at<float>(i-1)) ),
                Point( bin_w*(i), hist_h - cvRound(hist.at<float>(i)) ),
                Scalar( 255, 0, 0), 2, 8, 0  );
        }

        line( histImage, Point( bin_w*fg, hist_h ),
                Point( bin_w*fg, hist_h - 100 ),
                Scalar( 0, 255, 0), 2, 8, 0  );

        line( histImage, Point( bin_w*bg, hist_h ),
                Point( bin_w*bg, hist_h - 100 ),
                Scalar( 0, 0, 255), 2, 8, 0  );

        if (bg - fg > 10)
        {
            dstImage2.setTo(cv::Scalar(255), dstImage2 < (fg - fg_mean * 1.5f));
            dstImage2.setTo(cv::Scalar(255), dstImage2 > (fg + fg_mean * 1.5f));

            detection_history.push_back(1);
        }
        else
        {
            dstImage2.setTo(cv::Scalar(255), dstImage2 < 256);

            detection_history.push_back(0);
        }

        std::vector<float> dists;
        float sx = 0.f; float sy = 0.f; float sz = 0.f;
        count = 0;

        for (int j = 0; j < h; j++)
        {
            for (int i = 0; i < w; i++)
            {
                if (dstImage2.at<uint8_t>(Point(i, j)) < 255)
                {
                    auto v = vs[j * w + i];
                    dists.push_back(v.z);
                    sx += v.x;
                    sy += v.y;
                    sz += v.z;
                    count++;
                }
            }
        }



        auto ms6 = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

        if (dists.size())
        {
            const auto middleItr = dists.begin() + dists.size() / 2;
            std::nth_element(dists.begin(), middleItr, dists.end());
            std::cout << fv.z - *middleItr << std::endl;

            sz = *middleItr;

            auto dist = std::pow(fv.z - *middleItr, 2.f) + std::pow(fv.x - sx / count, 2.f) + std::pow(fv.y - sy / count, 2.f);

            zvalues.push_back(fv.z - *middleItr);
            if (zvalues.size() > 50) zvalues.pop_front();

            std::cout << sx / count << ", " << sy / count << std::endl;

            float pixel[2];
            float pt[3];
            pt[0] = sx / count; pt[1] = sy / count; pt[2] = sz;
            intr = depth.get_profile().as<rs2::video_stream_profile>().get_intrinsics();
            rs2_project_point_to_pixel(pixel, &intr, pt);


            line( dstImage2, Point( pixel[0], pixel[1]-1 ),
                    Point( pixel[0], pixel[1]+1 ),
                    Scalar( 0, 0, 0), 2, 8, 0  );
            line( dstImage2, Point( pixel[0]-1, pixel[1] ),
                    Point( pixel[0]+1, pixel[1] ),
                    Scalar( 0, 0, 0), 2, 8, 0  );

            auto ts = duration_cast<microseconds>(high_resolution_clock::now() - zero_time).count();
            mut.lock();
            points.push_front(point{ sx / count, sy / count, sz, ts });
            mut.unlock();

        }

        

        Mat zplot( hist_h, hist_w, CV_8UC3, Scalar( 255,255,255) );
        

        for( int i = 1; i < zvalues.size(); i++ )
        {
            bin_w = hist_w / zvalues.size();

            line( zplot, Point( bin_w*(i-1), hist_h - cvRound(zvalues[i-1] * 500.f) ),
                Point( bin_w*(i), hist_h - cvRound(zvalues[i-1] * 500.f) ),
                Scalar( 255, 0, 0), 2, 8, 0  );
        }


        cvtColor(color_mat, color_mat, COLOR_BGR2GRAY);


        cv::resize(color_mat, color_mat, dstImage2.size(), 0, 0, cv::INTER_LINEAR);

        std::vector<Mat> channels { dstImage2, depth_cpy, color_mat };


        merge(channels, dstImage2);

        auto ms = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
        std::stringstream ss; ss << std::fixed << ms;
        putText(zplot, ss.str(), Point(30,30), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(200,200,250), 1, 8);

        ss.str(""); ss << std::fixed << ms1;
        putText(zplot, ss.str(), Point(30,50), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(200,200,250), 1, 8);

        ss.str(""); ss << std::fixed << ms2 - ms1;
        putText(zplot, ss.str(), Point(30,70), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(200,200,250), 1, 8);

        ss.str(""); ss << std::fixed << ms3 - ms2;
        putText(zplot, ss.str(), Point(30,80), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(200,200,250), 1, 8);

        ss.str(""); ss << std::fixed << ms4 - ms3;
        putText(zplot, ss.str(), Point(30,100), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(200,200,250), 1, 8);

        ss.str(""); ss << std::fixed << ms5 - ms4;
        putText(zplot, ss.str(), Point(30,120), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(200,200,250), 1, 8);

        ss.str(""); ss << std::fixed << ms6 - ms5;
        putText(zplot, ss.str(), Point(30,140), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(200,200,250), 1, 8);

        auto detection_window = 500 / ms;
        while (detection_history.size() > detection_window) detection_history.pop_front();
        
        //imshow("calcHist Demo", histImage );

        cv::resize(dstImage2, dstImage2, cv::Size(1280, 720), 0, 0, cv::INTER_LINEAR);

        cv::resize(histImage, histImage, cv::Size(300, 200), 0, 0, cv::INTER_LINEAR);
        cv::resize(zplot, zplot, cv::Size(300, 200), 0, 0, cv::INTER_LINEAR);



        histImage.copyTo(dstImage2(cv::Rect(dstImage2.cols - histImage.cols,dstImage2.rows - histImage.rows,histImage.cols, histImage.rows)));
        zplot.copyTo(dstImage2(cv::Rect(dstImage2.cols - histImage.cols,dstImage2.rows - histImage.rows - zplot.rows,zplot.cols, zplot.rows)));
        
        mut.lock();
        output_mat = dstImage2.clone();
        mut.unlock();
        }
    });

    double ms = 0.0;

    std::deque<point> history_points;

    while (waitKey(1) < 0 && getWindowProperty(window_name, WND_PROP_AUTOSIZE) >= 0)
    {
        
        auto conf = std::accumulate(begin(detection_history), end(detection_history), 0) / (float)detection_history.size();

        auto start = high_resolution_clock::now();
        // Extract foreground pixels based on refined mask from the algorithm
        mut.lock();

        Mat copy = output_mat.clone();


        std::stringstream ss; ss << std::fixed << ms;
        putText(copy, ss.str(), Point(30,30), 
            FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(0,0,255), 1, 8);


        auto ts = duration_cast<microseconds>(high_resolution_clock::now() - zero_time).count();
        point a, b, c, d;
        auto clean_pt = clean_data(points, ts, a, b, c, d);

        

        if (conf > 0.5f)
        {
            for (int i = 0; i < history_points.size(); i++)
            {
                auto t = (float)i/history_points.size();
                draw_point(copy, &intr, history_points[i], t * 255, 0, 0);
            }

            draw_point(copy, &intr, a, 0, 255, 0);
            draw_point(copy, &intr, b, 0, 0, 255);
            draw_point(copy, &intr, c, 0, 255, 255);
            draw_point(copy, &intr, d, 0, 0, 0);

            draw_point(copy, &intr, clean_pt, 255, 0, 0, 5);

            history_points.push_back(clean_pt);
            if (history_points.size() > 500)
                history_points.pop_front();
        }

        imshow(window_name, copy);
        mut.unlock();
        //imshow("calcHist Demo", histImage );
        //imshow("z plot", zplot );

        ms = duration_cast<microseconds>(high_resolution_clock::now() - start).count() / 1000.0;
    }

    stop = 1.f;
    if (t.joinable()) t.join();
    if (face_detector.joinable()) face_detector.join();

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}



