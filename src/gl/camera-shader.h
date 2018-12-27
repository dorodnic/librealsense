#pragma once

#include "rendering.h"
#include "opengl3.h"
#include "synthetic-stream-gl.h"

namespace rs2
{
    class camera_shader
    {
    public:
        camera_shader();

        void begin();
        void end();

        void set_mvp(const matrix4& model,
                    const matrix4& view,
                    const matrix4& projection);
    protected:
        std::unique_ptr<shader_program> _shader;

    private:
        void init();

        uint32_t _transformation_matrix_location;
        uint32_t _projection_matrix_location;
        uint32_t _camera_matrix_location;
    };

    class camera_renderer : public filter
    {
    public:
        camera_renderer(std::shared_ptr<librealsense::gl::context> ctx)
            : filter([this](frame f, frame_source& s) { func(f, s); })
        {
            register_simple_option(OPTION_COMPATIBILITY_MODE, option_range{ 0, 1, 0, 1 });
        }
        
        static const auto OPTION_COMPATIBILITY_MODE = rs2_option(RS2_OPTION_COUNT + 1);
    private:
        void func(frame data, frame_source& source)
        {
            if (config_file::instance().get(configurations::performance::glsl_for_rendering))
            {
                _shader->begin();
                //cam_shader.set_mvp(identity_matrix(), view_mat, perspective_mat);
                //_shader->draw();
                _shader->end();
            }
            else
            {
                glBegin(GL_TRIANGLES);
                for (auto& i : camera_mesh.indexes)
                {
                    auto v0 = camera_mesh.positions[i.x];
                    auto v1 = camera_mesh.positions[i.y];
                    auto v2 = camera_mesh.positions[i.z];
                    glVertex3fv(&v0.x);
                    glVertex3fv(&v1.x);
                    glVertex3fv(&v2.x);
                    glColor4f(0.036f, 0.044f, 0.051f, 0.3f);
                }
                glEnd();
            }
        }

        std::shared_ptr<librealsense::gl::context> _ctx;
        std::shared_ptr<camera_shader> _shader;
        std::unique_ptr<vao> _camera_model;
    };
}