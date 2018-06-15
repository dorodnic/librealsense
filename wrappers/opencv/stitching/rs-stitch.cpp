// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <opencv2/opencv.hpp>   // Include OpenCV API
#include "../cv-helpers.hpp"    // Helper functions for conversions between RealSense and OpenCV
//#include "example.hpp"          // Include short list of convenience functions for rendering

#include <librealsense2/rsutil.h>

#include <arcball_camera.h>

#include "rendering.h"
#include "ux-window.h"

#include <imgui.h>
#include <algorithm>            // std::min, std::max

#include <opencv2/imgproc.hpp>

#include <numeric>

using namespace cv;
using namespace rs2;

const size_t inWidth = 300;
const size_t inHeight = 300;
const float WHRatio = inWidth / (float)inHeight;
const float inScaleFactor = 0.007843f;
const float meanVal = 127.5;
const char* classNames[] = { "background",
                            "aeroplane", "bicycle", "bird", "boat",
                            "bottle", "bus", "car", "cat", "chair",
                            "cow", "diningtable", "dog", "horse",
                            "motorbike", "person", "pottedplant",
                            "sheep", "sofa", "train", "tvmonitor" };

// 3D-Viewer state
float3 pos = { 0.0f, 0.0f, -0.5f };
float3 target = { 0.0f, 0.0f, 0.5f };
float3 up;
bool fixed_up = true;
bool render_quads = true;
float scaledown = 1.f;

float view[16];
bool texture_wrapping_on = true;
GLint texture_border_mode = GL_CLAMP_TO_EDGE; // GL_CLAMP_TO_BORDER

void reset_camera(float3 p = { 0.0f, 0.0f, -0.5f })
{
    target = { 0.0f, 0.0f, 0.0f };
    pos = p;

    // initialize "up" to be tangent to the sphere!
    // up = cross(cross(look, world_up), look)
    {
        float3 look = { target.x - pos.x, target.y - pos.y, target.z - pos.z };
        look = look.normalize();

        float world_up[3] = { 0.0f, 1.0f, 0.0f };

        float across[3] = {
            look.y * world_up[2] - look.z * world_up[1],
            look.z * world_up[0] - look.x * world_up[2],
            look.x * world_up[1] - look.y * world_up[0],
        };

        up.x = across[1] * look.z - across[2] * look.y;
        up.y = across[2] * look.x - across[0] * look.z;
        up.z = across[0] * look.y - across[1] * look.x;

        float up_len = up.length();
        up.x /= -up_len;
        up.y /= -up_len;
        up.z /= -up_len;
    }
}

void update_3d_camera(const rect& viewer_rect,
    mouse_info& mouse, bool force)
{
    auto now = std::chrono::high_resolution_clock::now();
    static auto view_clock = std::chrono::high_resolution_clock::now();
    auto sec_since_update = std::chrono::duration<float, std::milli>(now - view_clock).count() / 1000;
    view_clock = now;

    if (fixed_up)
        up = { 0.f, -1.f, 0.f };

    auto dir = target - pos;
    auto x_axis = cross(dir, up);
    auto step = sec_since_update * 0.3f;

    if (ImGui::IsKeyPressed('w') || ImGui::IsKeyPressed('W'))
    {
        pos = pos + dir * step;
        target = target + dir * step;
    }
    if (ImGui::IsKeyPressed('s') || ImGui::IsKeyPressed('S'))
    {
        pos = pos - dir * step;
        target = target - dir * step;
    }
    if (ImGui::IsKeyPressed('d') || ImGui::IsKeyPressed('D'))
    {
        pos = pos + x_axis * step;
        target = target + x_axis * step;
    }
    if (ImGui::IsKeyPressed('a') || ImGui::IsKeyPressed('A'))
    {
        pos = pos - x_axis * step;
        target = target - x_axis * step;
    }

    arcball_camera_update(
        (float*)&pos, (float*)&target, (float*)&up, view,
        sec_since_update,
        0.2f, // zoom per tick
        -0.1f, // pan speed
        3.0f, // rotation multiplier
        static_cast<int>(viewer_rect.w), static_cast<int>(viewer_rect.h), // screen (window) size
        static_cast<int>(mouse.prev_cursor.x), static_cast<int>(mouse.cursor.x),
        static_cast<int>(mouse.prev_cursor.y), static_cast<int>(mouse.cursor.y),
        (ImGui::GetIO().MouseDown[2] || ImGui::GetIO().MouseDown[1]) ? 1 : 0,
        ImGui::GetIO().MouseDown[0] ? 1 : 0,
        mouse.mouse_wheel,
        0);

    mouse.prev_cursor = mouse.cursor;
}

struct Detection
{
    Rect r;
    float min_z, max_z;
    float confidence;
    float left, right, top, bottom;
};

void render_3d_view(const rect& viewer_rect, texture_buffer* texture, rs2::points points, 
    std::vector<Detection>& objects)
{
    glViewport(static_cast<GLint>(viewer_rect.x), 0,
        static_cast<GLsizei>(viewer_rect.w), static_cast<GLsizei>(viewer_rect.h));

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluPerspective(60, (float)viewer_rect.w / viewer_rect.h, 0.001f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(view);

    glDisable(GL_TEXTURE_2D);

    glEnable(GL_DEPTH_TEST);

    texture_buffer::draw_axis(0.1f, 1);

    glColor4f(1.f, 1.f, 1.f, 1.f);

    glLineWidth(1.f);
    glBegin(GL_LINES);

    auto vf_prof = points.get_profile().as<video_stream_profile>();
    auto intrin = vf_prof.get_intrinsics();

    glColor4f(36.f / 255, 44.f / 255, 51.f / 255, 0.5f);

    for (float d = 1; d < 6; d += 2)
    {
        auto get_point = [&](float x, float y) -> float3
        {
            float point[3];
            float pixel[2]{ x, y };
            rs2_deproject_pixel_to_point(point, &intrin, pixel, d);
            glVertex3f(0.f, 0.f, 0.f);
            glVertex3fv(point);
            return{ point[0], point[1], point[2] };
        };

        auto top_left = get_point(0, 0);
        auto top_right = get_point(static_cast<float>(intrin.width), 0);
        auto bottom_right = get_point(static_cast<float>(intrin.width), static_cast<float>(intrin.height));
        auto bottom_left = get_point(0, static_cast<float>(intrin.height));

        glVertex3fv(&top_left.x); glVertex3fv(&top_right.x);
        glVertex3fv(&top_right.x); glVertex3fv(&bottom_right.x);
        glVertex3fv(&bottom_right.x); glVertex3fv(&bottom_left.x);
        glVertex3fv(&bottom_left.x); glVertex3fv(&top_left.x);
    }

    glColor4f(1, 0, 0, 1);

    for (auto& o : objects)
    {
        auto object = o.r;

        auto get_point = [&](float x, float y, float z) -> float3
        {
            float point[3];
            float pixel[2]{ x, y };
            rs2_deproject_pixel_to_point(point, &intrin, pixel, z);
            //glVertex3f(0.f, 0.f, 0.f);
            glVertex3fv(point);
            return{ point[0], point[1], point[2] };
        };

        auto top_left = get_point(object.x / scaledown, object.y / scaledown, o.min_z);
        auto top_right = get_point(static_cast<float>(object.x / scaledown + object.width / scaledown), object.y / scaledown, o.min_z);
        auto bottom_right = get_point(static_cast<float>(object.x / scaledown + object.width / scaledown), 
            static_cast<float>(object.y / scaledown + object.height / scaledown), o.min_z);
        auto bottom_left = get_point(object.x / scaledown, 
            static_cast<float>(object.y / scaledown + object.height / scaledown), o.min_z);

        auto top_left2 = get_point(object.x / scaledown, object.y / scaledown, o.max_z);
        auto top_right2 = get_point(static_cast<float>(object.x / scaledown + object.width / scaledown), object.y / scaledown, o.max_z);
        auto bottom_right2 = get_point(static_cast<float>(object.x / scaledown + object.width / scaledown),
            static_cast<float>(object.y / scaledown + object.height / scaledown), o.max_z);
        auto bottom_left2 = get_point(object.x / scaledown,
            static_cast<float>(object.y / scaledown + object.height / scaledown), o.max_z);

        o.left = top_left2.x;
        o.right = top_right2.x;
        o.top = top_left2.y;
        o.bottom = bottom_right2.y;

        glVertex3fv(&top_left.x); glVertex3fv(&top_right.x);
        glVertex3fv(&top_right.x); glVertex3fv(&bottom_right.x);
        glVertex3fv(&bottom_right.x); glVertex3fv(&bottom_left.x);
        glVertex3fv(&bottom_left.x); glVertex3fv(&top_left.x);

        glVertex3fv(&top_left2.x); glVertex3fv(&top_right2.x);
        glVertex3fv(&top_right2.x); glVertex3fv(&bottom_right2.x);
        glVertex3fv(&bottom_right2.x); glVertex3fv(&bottom_left2.x);
        glVertex3fv(&bottom_left2.x); glVertex3fv(&top_left2.x);

        glVertex3fv(&top_left.x); glVertex3fv(&top_left2.x);
        glVertex3fv(&top_right.x); glVertex3fv(&top_right2.x);
        glVertex3fv(&bottom_right.x); glVertex3fv(&bottom_right2.x);
        glVertex3fv(&bottom_left.x); glVertex3fv(&bottom_left2.x);
    }

    glEnd();

    glColor4f(1.f, 1.f, 1.f, 1.f);

    // Non-linear correspondence customized for non-flat surface exploration
    //glPointSize(std::sqrt(viewer_rect.w / last_points.get_profile().as<video_stream_profile>().width()));

    auto tex = texture->get_gl_handle();
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_border_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_border_mode);

    //glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, tex_border_color);

    auto vertices = points.get_vertices();
    auto tex_coords = points.get_texture_coordinates();

    // Visualization with quads produces better results but requires further optimization
    glBegin(GL_QUADS);

    const auto threshold = 0.05f;
    auto width = vf_prof.width(), height = vf_prof.height();
    for (int x = 0; x < width - 1; ++x) {
        for (int y = 0; y < height - 1; ++y) {
            auto& va = vertices[y * width + x];

            bool show = false;
            for (auto o : objects)
            {
                if (o.min_z > va.z || o.max_z < va.z) continue;
                if (o.left > va.x || o.right < va.x) continue;
                if (o.top > va.y || o.bottom < va.y) continue;
                show = true;
            }

            if (show)
            {
                auto a = y * width + x, b = y * width + x + 1, c = (y + 1)*width + x, d = (y + 1)*width + x + 1;
                if (vertices[a].z && vertices[b].z && vertices[c].z && vertices[d].z
                    && abs(vertices[a].z - vertices[b].z) < threshold && abs(vertices[a].z - vertices[c].z) < threshold
                    && abs(vertices[b].z - vertices[d].z) < threshold && abs(vertices[c].z - vertices[d].z) < threshold) {
                    glVertex3fv(vertices[a]); glTexCoord2fv(tex_coords[a]);
                    glVertex3fv(vertices[b]); glTexCoord2fv(tex_coords[b]);
                    glVertex3fv(vertices[d]); glTexCoord2fv(tex_coords[d]);
                    glVertex3fv(vertices[c]); glTexCoord2fv(tex_coords[c]);
                }
            }
        }
    }
    glEnd();

    glDisable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();


    if (ImGui::IsKeyPressed('R') || ImGui::IsKeyPressed('r'))
    {
        reset_camera();
    }
}

int main(int argc, char * argv[]) try
{
    using namespace cv::dnn;

    Net net = readNetFromCaffe("MobileNetSSD_deploy.prototxt",
        "MobileNetSSD_deploy.caffemodel");

    // Create a simple OpenGL window for rendering:
    ux_window app("RealSense Pointcloud Example");

    app.on_load = []() { return true; };

    texture_buffer color_tex;

    // Declare pointcloud object, for calculating pointclouds and texture mappings
    rs2::pointcloud pc;
    rs2::align align(RS2_STREAM_DEPTH);

    rs2::decimation_filter df;
    df.set_option(RS2_OPTION_FILTER_MAGNITUDE, scaledown);
    rs2::spatial_filter sf;
    rs2::temporal_filter tf;
    rs2::hole_filling_filter hf;

    // We want the points object to be persistent so we can display the last cloud when a frame drops
    rs2::points points;
    rs2::video_frame last_color(frame{});
    rs2::video_frame last_aligned(frame{});
    rs2::colorizer colorizer;

    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_DEPTH, 0, 0, RS2_FORMAT_Z16,  15);
    cfg.enable_stream(RS2_STREAM_COLOR, 0, 0, RS2_FORMAT_RGB8, 15);

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;

    // Start streaming with default recommended configuration
    auto prof = pipe.start(cfg);
    prof.get_device().first<depth_sensor>().set_option(RS2_OPTION_VISUAL_PRESET, 3.f);
    auto vp = prof.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>();
    auto intrin = vp.get_intrinsics();

    auto ratio = vp.width() / vp.height();
    auto new_w = 300 * ratio;

    Size cropSize(300, 300);
    Rect crop(Point((new_w - 300) / 2, 0),
        cropSize);

    std::vector<Detection> detected_objects;

    std::atomic_bool alive(true);
    std::atomic_bool alive2(true);

    std::mutex mx;

    namedWindow("win", WINDOW_AUTOSIZE);

    std::thread t([&]() {
        frameset frames;
        while (alive)
        {
            if (pipe.poll_for_frames(&frames))
            {

                auto d0 = frames.get_depth_frame();
                auto c0 = frames.get_color_frame();

                frames = align.process(frames);

                auto d = frames.get_depth_frame();
                auto c = frames.get_color_frame();


                d = df.process(d0);
                d = sf.process(d);
                d = tf.process(d);
                d = hf.process(d);

                points = pc.calculate(d);
                pc.map_to(c0);
                last_color = c0;
                last_aligned = c;

                {
                    std::lock_guard<std::mutex> lock(mx);
                    for (auto& detected_object : detected_objects)
                    {
                        if (detected_object.r.width * detected_object.r.height > 0)
                        {
                            auto depth_mat = depth_frame_to_meters(pipe, d0);
                            detected_object.r = detected_object.r & cv::Rect(0, 0, depth_mat.cols, depth_mat.rows);
                            depth_mat = depth_mat(detected_object.r);

                            depth_mat.setTo(cv::Scalar(1000.f), depth_mat == 0);

                            //auto color_mat = frame_to_mat(c);
                            //color_mat = color_mat(detected_object.r);

                            std::vector<double> array;
                            if (depth_mat.isContinuous()) {
                                array.assign((double*)depth_mat.datastart, (double*)depth_mat.dataend);
                            }
                            else {
                                for (int i = 0; i < depth_mat.rows; ++i) {
                                    array.insert(array.end(), depth_mat.ptr<double>(i), depth_mat.ptr<double>(i) + depth_mat.cols);
                                }
                            }

                            array.erase(std::remove_if(
                                array.begin(), array.end(),
                                [](const int& x) {
                                return x > 10; // put your condition here
                            }), array.end());

                            double min, max;
                            cv::minMaxLoc(depth_mat, &min, &max);
                            double fg = min;
                            double bg = *max_element(std::begin(array), std::end(array));
                            double length = bg - fg;
                            bg = fg + 0.7 * length;

                            std::vector<bool> is_bg(array.size(), false);
                            std::vector<double>::iterator fg_start;
                            bool stop = false;
                            for (int i = 0; i < 10 && !stop; i++)
                            {
                                stop = true;
                                for (int j = 0; j < array.size(); j++)
                                {
                                    auto new_val = fabs(array[j] - fg) > fabs(bg - array[j]);
                                    if (new_val != is_bg[j]) stop = false;
                                    is_bg[j] = new_val;
                                }
                                fg_start = std::stable_partition(array.begin(), array.end(), [&](const double& x) {
                                    size_t index = &x - &array[0];
                                    return is_bg[index];
                                });

                                auto bg_size = std::distance(begin(array), fg_start);
                                double sum = std::accumulate(array.begin(), fg_start, 0.0);
                                double mean = sum / bg_size;
                                bg = mean;

                                auto fg_size = array.size() - bg_size;
                                sum = std::accumulate(fg_start, array.end(), 0.0);
                                mean = sum / fg_size;
                                fg = mean;
                            }

                            std::vector<double> fg_only(fg_start, std::end(array));
                            std::sort(fg_only.begin(), fg_only.end());
                            size_t outliers = fg_only.size() / 20;
                            fg_only.resize(fg_only.size() - outliers); // crop max 0.5% of the dataset

                            detected_object.min_z = min;
                            detected_object.max_z = *fg_only.rbegin();

                            //cv::imshow("win", color_mat);
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    std::thread t2([&]() {
        while (alive2)
        {
            if (last_aligned)
            {
                auto color_mat = frame_to_mat(last_aligned);

                resize(color_mat, color_mat, cropSize);

                Mat inputBlob = blobFromImage(color_mat, inScaleFactor,
                    Size(inWidth, inHeight), meanVal, false); //Convert Mat to batch of images
                net.setInput(inputBlob, "data"); //set the network input
                Mat detection = net.forward("detection_out"); //compute output

                Mat detectionMat(detection.size[2], detection.size[3], CV_32F, detection.ptr<float>());

                // Crop both color and depth frames
                color_mat = color_mat(crop);
                //depth_mat = depth_mat(crop);

                std::vector<Detection> draft;

                float confidenceThreshold = 0.8f;
                for (int i = 0; i < detectionMat.rows; i++)
                {
                    float confidence = detectionMat.at<float>(i, 2);

                    if (confidence > confidenceThreshold)
                    {
                        size_t objectClass = (size_t)(detectionMat.at<float>(i, 1));

                        int xLeftBottom = static_cast<int>(detectionMat.at<float>(i, 3) * color_mat.cols);
                        int yLeftBottom = static_cast<int>(detectionMat.at<float>(i, 4) * color_mat.rows);
                        int xRightTop = static_cast<int>(detectionMat.at<float>(i, 5) * color_mat.cols);
                        int yRightTop = static_cast<int>(detectionMat.at<float>(i, 6) * color_mat.rows);

                        Rect object((int)xLeftBottom, (int)yLeftBottom,
                            (int)(xRightTop - xLeftBottom),
                            (int)(yRightTop - yLeftBottom));

                        Detection d;
                        Rect detected_object = object;
                        detected_object.x *= last_aligned.get_height() / 300.f;
                        detected_object.y *= last_aligned.get_height() / 300.f;
                        detected_object.width *= last_aligned.get_height() / 300.f;
                        detected_object.height *= last_aligned.get_height() / 300.f;
                        detected_object.x += (last_aligned.get_width() - last_aligned.get_height()) / 2;
                        d.confidence = confidence;
                        d.min_z = 0.f; 
                        d.max_z = 0.f;
                        d.r = detected_object;

                        draft.push_back(d);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(mx);

                    std::sort(draft.begin(), draft.end(),
                        [](const Detection& a, const Detection& b) { return a.confidence > b.confidence; });

                    if (draft.size() != detected_objects.size())
                    {
                        detected_objects.clear();
                        detected_objects.resize(draft.size());
                    }
                    for (int i = 0; i < draft.size(); i++)
                    {
                        detected_objects[i].confidence = draft[i].confidence;
                        detected_objects[i].r = draft[i].r;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    while (app) // Application still alive?
    {
        if (last_color) color_tex.upload(last_color);

        rect view_rect{ 0, 0, app.width(), app.height() };
        update_3d_camera(view_rect, app.get_mouse(), true);
        if (points)
        {
            std::lock_guard<std::mutex> lock(mx);
            std::vector<Detection> obj_copy(detected_objects.begin(), detected_objects.end());
            render_3d_view(view_rect, &color_tex, points, obj_copy);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    alive = false;
    t.join();
    alive2 = false;
    t2.join();

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