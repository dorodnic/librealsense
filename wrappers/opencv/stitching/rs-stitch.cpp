// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <opencv2/opencv.hpp>   // Include OpenCV API
#include "../cv-helpers.hpp"    // Helper functions for conversions between RealSense and OpenCV
#include "example.hpp"          // Include short list of convenience functions for rendering

#include <algorithm>            // std::min, std::max
#include <deque>

#include <opencv2/rgbd.hpp>
#include <opencv2/imgproc.hpp>

// Helper functions
void register_glfw_callbacks(window& app, glfw_state& app_state);

using namespace cv;
using namespace cv::rgbd;

void draw_pointcloud2(float width, float height, glfw_state& app_state, 
    const std::deque<rs2::points>& points, texture* textures, int count, int first,
    const std::deque<Mat>& rts)
{
    // OpenGL commands that prep screen for the pointcloud
    glPopMatrix();
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    glClearColor(153.f / 255, 153.f / 255, 153.f / 255, 1);
    glClear(GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    gluPerspective(60, width / height, 0.01f, 10.0f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    gluLookAt(0, 0, 0, 0, 0, 1, 0, -1, 0);

    glTranslatef(0, 0, +0.5f + app_state.offset_y*0.05f);
    glRotated(app_state.pitch, 1, 0, 0);
    glRotated(app_state.yaw, 0, 1, 0);
    glTranslatef(0, 0, -0.5f);

    glPointSize(width / 640);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    int k = 0;
    Mat Rt = Mat::eye(4, 4, CV_32FC1);

    for (auto p : points)
    {
        glBindTexture(GL_TEXTURE_2D, textures[first % count].get_gl_handle());
        float tex_border_color[] = { 0.8f, 0.8f, 0.8f, 0.8f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, tex_border_color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); // GL_CLAMP_TO_EDGE
        glBegin(GL_POINTS);

        Rt = rts[k] * Rt;

        /* this segment actually prints the pointcloud */
        auto vertices = p.get_vertices();              // get vertices
        auto tex_coords = p.get_texture_coordinates(); // and texture coordinates
        for (int i = 0; i < p.size(); i++)
        {
            if (vertices[i].z)
            {
                rs2::vertex v = vertices[i];
                v.x = v.x * Rt.at<float>(0, 0) + v.y * Rt.at<float>(0, 1) + v.z * Rt.at<float>(0, 2) + Rt.at<float>(0, 3);
                v.y = v.x * Rt.at<float>(1, 0) + v.y * Rt.at<float>(1, 1) + v.z * Rt.at<float>(1, 2) + Rt.at<float>(1, 3);
                v.z = v.x * Rt.at<float>(2, 0) + v.y * Rt.at<float>(2, 1) + v.z * Rt.at<float>(2, 2) + Rt.at<float>(2, 3);

                // upload the point and texture coordinates only for points we have depth data for
                glVertex3fv(v);
                glTexCoord2fv(tex_coords[i]);
            }
        }

        // OpenGL cleanup
        glEnd();
        first++;
        k++;
    }

    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
    glPushMatrix();
}


int main(int argc, char * argv[]) try
{
    auto optr = Odometry::create("RgbdICPOdometry");
    optr.staticCast<RgbdOdometry>()->setMaxDepth(3);

    Ptr<OdometryFrame> curr = Ptr<OdometryFrame>(new OdometryFrame());
    Ptr<OdometryFrame> prev = Ptr<OdometryFrame>(new OdometryFrame());
    bool first = true;

    const int HISTORY = 100;

    // Create a simple OpenGL window for rendering:
    window app(1280, 720, "RealSense Pointcloud Example");
    // Construct an object to manage view state
    glfw_state app_state;
    // register callbacks to allow manipulation of the pointcloud
    register_glfw_callbacks(app, app_state);
    texture depth_image, color_image;

    // Declare pointcloud object, for calculating pointclouds and texture mappings
    rs2::pointcloud pc;
    rs2::align align(RS2_STREAM_COLOR);
    // We want the points object to be persistent so we can display the last cloud when a frame drops
    rs2::points points;
    rs2::colorizer colorizer;

    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 15);
    cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 15);

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;

    // Start streaming with default recommended configuration
    auto prof = pipe.start(cfg);
    auto vp = prof.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>();
    auto intrin = vp.get_intrinsics();
    Mat cameraMatrix = Mat::eye(3, 3, CV_32FC1);
    cameraMatrix.at<float>(0, 0) = intrin.fx;
    cameraMatrix.at<float>(1, 1) = intrin.fy;
    cameraMatrix.at<float>(0, 2) = intrin.ppx;
    cameraMatrix.at<float>(1, 2) = intrin.ppy;
    optr->setCameraMatrix(cameraMatrix);

    std::deque<rs2::points> pts;
    texture textures[HISTORY];
    int first_tex = 0;
    int next_tex = 0;

    std::deque<Mat> rts;

    while (app) // Application still alive?
    {
        // Wait for the next set of frames from the camera
        auto frames = pipe.wait_for_frames();

        frames = align.process(frames);

        auto d = frames.get_depth_frame();
        auto c = frames.get_color_frame();

        auto w = d.get_width();
        auto h = d.get_height();

        Mat image(h, w, CV_8UC3, (void*)c.get_data());
        Mat depth(h, w, CV_16UC1, (void*)d.get_data());
        Mat gray;
        cvtColor(image, gray, COLOR_BGR2GRAY);
        image.release();

        Mat depth_flt;
        depth.convertTo(depth_flt, CV_32FC1, 0.001f);
        depth_flt.setTo(std::numeric_limits<float>::quiet_NaN(), depth == 0);
        depth.release();

        curr->image = gray;
        curr->depth = depth_flt;

        bool res = false;
        Mat Rt = Mat::eye(4, 4, CV_32FC1);
        if (!first)
            res = optr->compute(curr, prev, Rt);

        float view[16];
        float view2[16];

        first = false;
        if (res)
        {
            Rt.convertTo(Rt, CV_32FC1);

            std::cout.precision(3);
            /*for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                    std::cout << std::fixed << ((Rt.at<float>(i, j) > 0) ? " " : "") << Rt.at<float>(i, j) << " ";
                std::cout << "\n";
            }
            std::cout << "\n";*/

            points = pc.calculate(d);
            pc.map_to(c);

            if (pts.size() == HISTORY)
            {
                pts.pop_front();
                rts.pop_front();
                first_tex = (first_tex + 1) % HISTORY;
            }
            points.keep();
            pts.push_back(points);
            rts.push_back(Rt);
            textures[next_tex].upload(c);
            next_tex = (next_tex + 1) % HISTORY;
        }
        else
        {
            Rt = Mat::eye(4, 4, CV_32FC1);
        }

        draw_pointcloud2(app.width(), app.height(), app_state, pts, textures, HISTORY, first_tex, rts);

        //if (!prev.empty()) prev.release();
        if (!prev.empty())
            prev->release();
        std::swap(prev, curr);

        //// Generate the pointcloud and texture mappings
        

        //// Tell pointcloud object to map to this color frame
        

        //// Upload the color frame to OpenGL

        //// Draw the pointcloud
        

        //depth_image.render(colorizer.colorize(d), { 0,               0, app.width() / 2, app.height() });
        //color_image.render(c, { app.width() / 2, 0, app.width() / 2, app.height() });
    }

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}