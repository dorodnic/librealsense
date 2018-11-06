#include "pc-shader.h"
#include <glad/glad.h>

static const char* vertex_shader_text =
"#version 110\n"
"\n"
"attribute vec3 position;\n"
"attribute vec2 textureCoords;\n"
"\n"
"varying float valid;\n"
"varying vec2 positionCoords;\n"
"varying vec2 sampledUvs;\n"
"\n"
"uniform mat4 transformationMatrix;\n"
"uniform mat4 projectionMatrix;\n"
"uniform mat4 cameraMatrix;\n"
"\n"
"uniform sampler2D uvsSampler;\n"
"uniform sampler2D positionsSampler;\n"
"\n"
"uniform float imageWidth;\n"
"uniform float imageHeight;\n"
"uniform float minDeltaZ;\n"
"\n"
"void main(void) {\n"
"    vec4 uvs = texture2D(uvsSampler, textureCoords);\n"
"    vec4 pos = texture2D(positionsSampler, textureCoords);\n"
"    float pixelWidth = 1.0 / imageWidth;\n"
"    float pixelHeight = 1.0 / imageHeight;\n"
"\n"
"    vec2 tex_left = vec2(max(textureCoords.x - pixelWidth, 0.0), textureCoords.y);\n"
"    vec2 tex_right = vec2(min(textureCoords.x + pixelWidth, 1.0), textureCoords.y);\n"
"    vec2 tex_top = vec2(textureCoords.x, max(textureCoords.y - pixelHeight, 0.0));\n"
"    vec2 tex_buttom = vec2(textureCoords.x, min(textureCoords.y + pixelHeight, 1.0));\n"
"\n"
"    vec4 pos_left = texture2D(positionsSampler, tex_left);\n"
"    vec4 pos_right = texture2D(positionsSampler, tex_right);\n"
"    vec4 pos_top = texture2D(positionsSampler, tex_top);\n"
"    vec4 pos_buttom = texture2D(positionsSampler, tex_buttom);\n"
"\n"
"    valid = 0.0;\n"
"    if (abs(pos_left.z - pos.z) > minDeltaZ) valid = 1.0;\n"
"    if (abs(pos_right.z - pos.z) > minDeltaZ) valid = 1.0;\n"
"    if (abs(pos_top.z - pos.z) > minDeltaZ) valid = 1.0;\n"
"    if (abs(pos_buttom.z - pos.z) > minDeltaZ) valid = 1.0;\n"
"    if (abs(pos.z) < 0.01) valid = 1.0;\n"
"    //valid = 0.0;\n"
"    if (valid > 0.0) pos = vec4(1.0, 1.0, 1.0, 0.0);\n"
"    else pos = vec4(pos.xyz, 1.0);\n"
"    vec4 worldPosition = transformationMatrix * pos;\n"
"    gl_Position = projectionMatrix * cameraMatrix * worldPosition;\n"
"\n"
"    positionCoords = position.xy;\n"
"    sampledUvs = uvs.xy;\n"
"}\n";

static const char* fragment_shader_text =
"#version 110\n"
"\n"
"varying float valid;\n"
"varying vec2 sampledUvs;\n"
"varying vec2 positionCoords;\n"
"\n"
"uniform sampler2D textureSampler;\n"
"\n"
"void main(void) {\n"
"    if (valid > 0.0) discard;\n"
"    vec4 color = texture2D(textureSampler, sampledUvs);\n"
"\n"
"    gl_FragColor = vec4(color.xyz, 1.0);\n"
"}\n";

using namespace rs2;

pointcloud_shader::pointcloud_shader(std::unique_ptr<shader_program> shader)
    : _shader(std::move(shader))
{
    init();
}

pointcloud_shader::pointcloud_shader()
{
    _shader = shader_program::load(
        vertex_shader_text,
        fragment_shader_text);

    init();
}

void pointcloud_shader::init()
{
    _shader->bind_attribute(0, "position");
    _shader->bind_attribute(1, "textureCoords");
    _shader->bind_attribute(2, "normal");
    _shader->bind_attribute(3, "tangent");

    _transformation_matrix_location = _shader->get_uniform_location("transformationMatrix");
    _projection_matrix_location = _shader->get_uniform_location("projectionMatrix");
    _camera_matrix_location = _shader->get_uniform_location("cameraMatrix");

    _width_location = _shader->get_uniform_location("imageWidth");
    _height_location = _shader->get_uniform_location("imageHeight");
    _min_delta_z_location = _shader->get_uniform_location("minDeltaZ");

    auto texture0_sampler_location = _shader->get_uniform_location("textureSampler");
    auto texture1_sampler_location = _shader->get_uniform_location("positionsSampler");
    auto texture2_sampler_location = _shader->get_uniform_location("uvsSampler");

    _shader->begin();
    _shader->load_uniform(_min_delta_z_location, 0.2f);
    _shader->load_uniform(texture0_sampler_location, texture_slot());
    _shader->load_uniform(texture1_sampler_location, geometry_slot());
    _shader->load_uniform(texture2_sampler_location, uvs_slot());
    _shader->end();
}

void pointcloud_shader::begin() { _shader->begin(); }
void pointcloud_shader::end() { _shader->end(); }

void pointcloud_shader::set_mvp(const matrix4& model,
    const matrix4& view,
    const matrix4& projection)
{
    _shader->load_uniform(_transformation_matrix_location, model);
    _shader->load_uniform(_camera_matrix_location, view);
    _shader->load_uniform(_projection_matrix_location, projection);
}

void pointcloud_shader::set_image_size(int width, int height)
{
    _shader->load_uniform(_width_location, (float)width);
    _shader->load_uniform(_height_location, (float)height);
}

void pointcloud_shader::set_min_delta_z(float min_delta_z)
{
    _shader->load_uniform(_min_delta_z_location, min_delta_z);
}