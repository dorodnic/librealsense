// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma once

#include "model-views.h"
#include "notifications.h"
#include "viewer.h"

namespace rs2
{
    class viewer_model
    {
    public:
        struct series_3d
        {
            int selected_depth_source_uid = 0;
            int selected_tex_source_uid = -1;

            bool render_quads = true;

            double last_picked = 0.0;
            float3 last_picked_value{};

            rs2::points last_points;
            std::shared_ptr<texture_buffer> last_texture;

            bool selected = false;

            matrix4 model;

            rs2::gl::pointcloud_renderer pc_renderer;

            series_3d() : pc_renderer() {}
        };

        void foreach_series(std::function<void(series_3d&)> action)
        {
            std::vector<std::shared_ptr<series_3d>> copy;
            {
                std::lock_guard<std::mutex> lock(_series_lock);
                copy = _series;
            }
            for (auto&& s : copy) action(*s);
        }

        series_3d& find_or_create_series(rs2::stream_profile p)
        {
            auto id = p.unique_id();
            if (streams_origin.find(id) != streams_origin.end())
                id = streams_origin[id];

            std::lock_guard<std::mutex> lock(_series_lock);

            for (auto&& s : _series)
            {
                if (s->selected_depth_source_uid == id) return *s;
            }
            auto res = std::make_shared<series_3d>();
            res->selected_depth_source_uid = id;
            res->model = identity_matrix();
            _series.push_back(res);
            return *_series[_series.size() - 1];
        }

        void reset_camera(float3 pos = { 0.0f, 0.0f, -1.0f });

        void update_configuration();

        const float panel_width = 340.f;
        const float panel_y = 50.f;
        const float default_log_h = 110.f;

        float get_output_height() const { return (is_output_collapsed ? default_log_h : 15); }
          
        rs2::frame handle_ready_frames(const rect& viewer_rect, ux_window& window, int devices, std::string& error_message);

        viewer_model();

        ~viewer_model()
        {
            // Stopping post processing filter rendering thread
            ppf.stop();
            streams.clear();
        }

        void begin_stream(std::shared_ptr<subdevice_model> d, rs2::stream_profile p);

        static std::vector<frame> get_frames(frame set);

        std::shared_ptr<texture_buffer> upload_frame(frame&& f);

        std::map<int, rect> calc_layout(const rect& r);

        void show_no_stream_overlay(ImFont* font, int min_x, int min_y, int max_x, int max_y);
        void show_no_device_overlay(ImFont* font, int min_x, int min_y);

        void show_paused_icon(ImFont* font, int x, int y, int id);
        void show_recording_icon(ImFont* font_18, int x, int y, int id, float alpha_delta);

        void popup_if_error(const ux_window& window, std::string& error_message);

        void popup(const ux_window& window, const std::string& header, const std::string& message, std::function<void()> configure);

        void popup_firmware_update_progress(const ux_window& window, const float progress);

        void show_event_log(ImFont* font_14, float x, float y, float w, float h);

        void render_pose(rs2::rect stream_rect, float buttons_heights);
        void try_select_pointcloud(ux_window& win, series_3d& s);

        void show_3dviewer_header(ImFont* font, rs2::rect stream_rect, bool& paused, std::string& error_message);

        void update_3d_camera(ux_window& win, const rect& viewer_rect, bool force = false);

        void show_top_bar(ux_window& window, const rect& viewer_rect, const device_models_list& devices);

        bool render_3d_view(const rect& view_rect, ux_window& win, float3* picked);

        void render_2d_view(const rect& view_rect, ux_window& win, int output_height,
            ImFont *font1, ImFont *font2, size_t dev_model_num, const mouse_info &mouse, std::string& error_message);

        void gc_streams();

        std::mutex streams_mutex;
        std::map<int, stream_model> streams;
        std::map<int, int> streams_origin;
        bool fullscreen = false;
        stream_model* selected_stream = nullptr;
        std::shared_ptr<syncer_model> syncer;
        post_processing_filters ppf;

        context ctx;
        notifications_model not_model;
        bool is_output_collapsed = false;
        bool is_3d_view = false;
        bool paused = false;
        bool metric_system = true;


        void draw_viewport(const rect& viewer_rect, 
            ux_window& window, int devices, std::string& error_message);

        bool allow_3d_source_change = true;
        bool allow_stream_close = true;

        std::array<float3, 4> roi_rect;
        bool draw_plane = false;

        bool draw_frustrum = true;
        bool support_non_syncronized_mode = true;
        std::atomic<bool> synchronization_enable;
        std::atomic<int> zo_sensors;

        press_button_model trajectory_button{ u8"\uf1b0", u8"\uf1b0","Draw trajectory", "Stop drawing trajectory", true };
        press_button_model grid_object_button{ u8"\uf1cb", u8"\uf1cb",  "Configure Grid", "Configure Grid", false };
        press_button_model pose_info_object_button{ u8"\uf05a", u8"\uf05a",  "Show pose stream info overlay", "Hide pose stream info overlay", false };

        bool show_pose_info_3d = false;

    private:
        void check_permissions();

        bool popup_triggered = false;

        struct rgb {
            uint32_t r, g, b;
        };

        struct rgb_per_distance {
            float depth_val;
            rgb rgb_val;
        };

        friend class post_processing_filters;
        std::map<int, rect> get_interpolated_layout(const std::map<int, rect>& l);
        void show_icon(ImFont* font_18, const char* label_str, const char* text, int x, int y,
                       int id, const ImVec4& color, const std::string& tooltip = "");
        void draw_color_ruler(const mouse_info& mouse,
                              const stream_model& s_model,
                              const rect& stream_rect,
                              std::vector<rgb_per_distance> rgb_per_distance_vec,
                              float ruler_length,
                              const std::string& ruler_units);
        float calculate_ruler_max_distance(const std::vector<float>& distances) const;

        streams_layout _layout;
        streams_layout _old_layout;
        std::chrono::high_resolution_clock::time_point _transition_start_time;

        // 3D-Viewer state
        float3 pos = { 0.0f, 0.0f, -0.5f };
        float3 target = { 0.0f, 0.0f, 0.0f };
        float3 up;
        bool fixed_up = true;

        float view[16];

        // Infinite pan / rotate feature:
        bool manipulating = false;
        float2 overflow = { 0.f, 0.f };

        rs2::gl::camera_renderer _cam_renderer;

        double _last_no_pick_time = 0.0;
        bool _pc_selected_down = false;
        bool _context_menu = false;
        double _selection_started = 0.0;

        std::mutex _series_lock;
        std::vector<std::shared_ptr<series_3d>> _series;
    };
}
