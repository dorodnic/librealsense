#include "camera-shader.h"
#include <glad/glad.h>

using namespace rs2;

#include <res/d435.h>
#include <res/d415.h>
#include <res/sr300.h>

static const char* vertex_shader_text =
"#version 110\n"
"\n"
"attribute vec3 position;\n"
"uniform mat4 transformationMatrix;\n"
"uniform mat4 projectionMatrix;\n"
"uniform mat4 cameraMatrix;\n"
"\n"
"void main(void) {\n"
"    vec4 worldPosition = transformationMatrix * vec4(position.xyz, 1.0);\n"
"    gl_Position = projectionMatrix * cameraMatrix * worldPosition;\n"
"}\n";

static const char* fragment_shader_text =
"#version 110\n"
"\n"
"void main(void) {\n"
"    gl_FragColor = vec4(36.0 / 1000.0, 44.0 / 1000.0, 51.0 / 1000.0, 0.3);\n"
"}\n";

using namespace rs2;

namespace librealsense
{
    namespace gl
    {
        camera_shader::camera_shader()
        {
            _shader = shader_program::load(
                vertex_shader_text,
                fragment_shader_text);

            init();
        }

        void camera_shader::init()
        {
            _shader->bind_attribute(0, "position");

            _transformation_matrix_location = _shader->get_uniform_location("transformationMatrix");
            _projection_matrix_location = _shader->get_uniform_location("projectionMatrix");
            _camera_matrix_location = _shader->get_uniform_location("cameraMatrix");
        }

        void camera_shader::begin() { _shader->begin(); }
        void camera_shader::end() { _shader->end(); }

        void camera_shader::set_mvp(const matrix4& model,
            const matrix4& view,
            const matrix4& projection)
        {
            _shader->load_uniform(_transformation_matrix_location, model);
            _shader->load_uniform(_camera_matrix_location, view);
            _shader->load_uniform(_projection_matrix_location, projection);
        }

        void camera_renderer::cleanup_gpu_resources()
        {
            _shader.reset();
            _camera_model.clear();
        }

        void camera_renderer::create_gpu_resources()
        {
            if (glsl_enabled())
            {
                _shader = std::make_shared<camera_shader>(); 

                for (auto&& mesh : camera_mesh)
                {
                    _camera_model.push_back(vao::create(mesh));
                }
            }
        }

        camera_renderer::~camera_renderer()
        {
            perform_gl_action([&]()
            {
                cleanup_gpu_resources();
            });
        }

        camera_renderer::camera_renderer()
        {
            {
                obj_mesh d415;
                uncompress_d415_obj(d415.positions, d415.normals, d415.indexes);
                camera_mesh.push_back(d415);
            }

            {
                obj_mesh d435;
                uncompress_d435_obj(d435.positions, d435.normals, d435.indexes);
                camera_mesh.push_back(d435);
            }

            {
                obj_mesh sr300;
                uncompress_sr300_obj(sr300.positions, sr300.normals, sr300.indexes);
                camera_mesh.push_back(sr300);
            }

            for (auto&& mesh : camera_mesh)
            {
                for (auto& xyz : mesh.positions)
                {
                    xyz = xyz / 1000.f;
                    xyz.x *= -1;
                    xyz.y *= -1;
                }
            }


            initialize();
        }

        bool starts_with(const std::string& s, const std::string& prefix)
        {
            auto i = s.begin(), j = prefix.begin();
            for (; i != s.end() && j != prefix.end() && *i == *j;
                i++, j++);
            return j == prefix.end();
        }

        rs2::frame camera_renderer::process_frame(const rs2::frame_source& src, const rs2::frame& f)
        {
            const auto& dev = ((frame_interface*)f.get())->get_sensor()->get_device();

            int index = -1;

            if (dev.supports_info(RS2_CAMERA_INFO_NAME))
            {
                auto dev_name = dev.get_info(RS2_CAMERA_INFO_NAME);
                if (starts_with(dev_name, "Intel RealSense D415")) index = 0;
                if (starts_with(dev_name, "Intel RealSense D435")) index = 1;
                if (starts_with(dev_name, "Intel RealSense SR300")) index = 2;
            };

            if (index >= 0)
            {
                perform_gl_action([&]()
                {
                    if (glsl_enabled())
                    {
                        _shader->begin();
                        _shader->set_mvp(get_matrix(
                            RS2_GL_MATRIX_TRANSFORMATION), 
                            get_matrix(RS2_GL_MATRIX_CAMERA), 
                            get_matrix(RS2_GL_MATRIX_PROJECTION)
                        );
                        _camera_model[index]->draw();
                        _shader->end();
                    }
                    else
                    {
                        /*glBegin(GL_TRIANGLES);
                        auto& mesh = camera_mesh[index];
                        for (auto& i : mesh.indexes)
                        {
                            auto v0 = mesh.positions[i.x];
                            auto v1 = mesh.positions[i.y];
                            auto v2 = mesh.positions[i.z];
                            glVertex3fv(&v0.x);
                            glVertex3fv(&v1.x);
                            glVertex3fv(&v2.x);
                            glColor4f(0.036f, 0.044f, 0.051f, 0.3f);
                        }
                        glEnd();*/
                    }
                }); 
            }

            return f;
        }
    }
}