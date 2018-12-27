#include "camera-shader.h"
#include <glad/glad.h>

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