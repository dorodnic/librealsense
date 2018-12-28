#pragma once

#include "opengl3.h"
#include "synthetic-stream-gl.h"
#include "proc/synthetic-stream.h"

namespace librealsense
{
    namespace gl
    {
        class camera_shader
        {
        public:
            camera_shader();

            void begin();
            void end();

            void set_mvp(const rs2::matrix4& model,
                        const rs2::matrix4& view,
                        const rs2::matrix4& projection);
        protected:
            std::unique_ptr<rs2::shader_program> _shader;

        private:
            void init();

            uint32_t _transformation_matrix_location;
            uint32_t _projection_matrix_location;
            uint32_t _camera_matrix_location;
        };

        class camera_renderer : public stream_filter_processing_block, 
                                public gpu_rendering_object,
                                public matrix_container
        {
        public:
            camera_renderer();

            void cleanup_gpu_resources() override;
            void create_gpu_resources() override;

            rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;
        private:
            void on_frame(frame f, frame_source& src);

            std::vector<rs2::obj_mesh> camera_mesh;
            std::shared_ptr<camera_shader> _shader;
            std::vector<std::unique_ptr<rs2::vao>> _camera_model;
        };

        // class camera_renderer : public rs2::filter, public gpu_rendering_object
        // {
        // public:
        //     camera_renderer()
        //         : rs2::filter([this](rs2::frame f, rs2::frame_source& s) { func(f, s); })
        //     {
        //     }
        // private:
        //     void func(rs2::frame data, rs2::frame_source& source)
        //     {
        //         if (glsl_enabled())
        //         {
        //             _shader->begin();
        //             //cam_shader.set_mvp(identity_matrix(), view_mat, perspective_mat);
        //             //_shader->draw();
        //             _shader->end();
        //         }
        //         else
        //         {
        //             glBegin(GL_TRIANGLES);
        //             for (auto& i : camera_mesh.indexes)
        //             {
        //                 auto v0 = camera_mesh.positions[i.x];
        //                 auto v1 = camera_mesh.positions[i.y];
        //                 auto v2 = camera_mesh.positions[i.z];
        //                 glVertex3fv(&v0.x);
        //                 glVertex3fv(&v1.x);
        //                 glVertex3fv(&v2.x);
        //                 glColor4f(0.036f, 0.044f, 0.051f, 0.3f);
        //             }
        //             glEnd();
        //         }
        //     }

        //     rs2::obj_mesh camera_mesh;
        //     std::shared_ptr<camera_shader> _shader;
        //     std::unique_ptr<rs2::vao> _camera_model;
        // };
    }
}