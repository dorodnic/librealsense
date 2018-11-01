#include "texture-2d-shader.h"
#include <glad/glad.h>

static const char* vertex_shader_text =
"#version 110\n"
"attribute vec3 position;\n"
"attribute vec2 textureCoords;\n"
"varying vec2 textCoords;\n"
"uniform vec2 elementPosition;\n"
"uniform vec2 elementScale;\n"
"void main(void)\n"
"{\n"
"    gl_Position = vec4(position * vec3(elementScale, 1.0) + vec3(elementPosition, 0.0), 1.0);\n"
"    textCoords = textureCoords;\n"
"}";

static const char* splash_shader_text =
"#version 110\n"
"varying vec2 textCoords;\n"
"uniform sampler2D textureSampler;\n"
"uniform float opacity;\n"
"uniform vec2 rayOrigin;\n"
"uniform float power;\n"
"void main(void) {\n"
"    vec4 FragColor = texture2D(textureSampler, textCoords);\n"
"        int samples = 120;\n"
"        vec2 delta = vec2(textCoords - rayOrigin);\n"
"        delta *= 1.0 /  float(samples) * 0.99;"
"        vec2 coord = textCoords;\n"
"        float illuminationDecay = power;\n"
"        for(int i=0; i < samples ; i++)\n"
"        {\n"
"           coord -= delta;\n"
"           vec4 texel = texture2D(textureSampler, coord);\n"
"           texel *= illuminationDecay * 0.4;\n"
"           texel.x *= 80.0 / 255.0;\n"
"           texel.y *= 99.0 / 255.0;\n"
"           texel.z *= 115.0 / 255.0;\n"
"           FragColor += texel;\n"
"           illuminationDecay *= power;\n"
"        }\n"
"        FragColor = clamp(FragColor, 0.0, 1.0);\n"
"    gl_FragColor = vec4(FragColor.xyz, opacity);\n"
"}";

static const char* fragment_shader_text =
"#version 110\n"
"varying vec2 textCoords;\n"
"uniform sampler2D textureSampler;\n"
"uniform float opacity;\n"
"void main(void) {\n"
"    vec4 color = texture2D(textureSampler, textCoords);\n"
"    gl_FragColor = vec4(color.xyz, opacity);\n"
"}";

using namespace rs2;

texture_2d_shader::texture_2d_shader(std::unique_ptr<shader_program> shader)
    : _shader(std::move(shader))
{
    init();
}

texture_2d_shader::texture_2d_shader(const char* custom_fragment)
{
    _shader = shader_program::load(
        vertex_shader_text,
        custom_fragment);

    init();
}

texture_2d_shader::texture_2d_shader()
{
    _shader = shader_program::load(
        vertex_shader_text,
        fragment_shader_text);

    init();
}

void visualizer_2d::draw_texture(uint32_t tex, float opacity)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    tex_2d_shader->begin();
    tex_2d_shader->set_opacity(opacity);
    tex_2d_shader->end();
    draw_texture({ 0.f, 0.f }, { 1.f, 1.f }, tex);
    glDisable(GL_BLEND);
}

void texture_2d_shader::set_position_and_scale(
    const float2& position,
    const float2& scale)
{
    _shader->load_uniform(_position_location, position);
    _shader->load_uniform(_scale_location, scale);
}

void splash_screen_shader::set_ray_center(const float2& center)
{
    _shader->load_uniform(_rays_location, center);
}

void splash_screen_shader::set_power(float power)
{
    _shader->load_uniform(_power_location, power);
}

void texture_2d_shader::init()
{
    _shader->bind_attribute(0, "position");
    _shader->bind_attribute(1, "textureCoords");

    _position_location = _shader->get_uniform_location("elementPosition");
    _scale_location = _shader->get_uniform_location("elementScale");
    _opacity_location = _shader->get_uniform_location("opacity");

    auto texture0_sampler_location = _shader->get_uniform_location("textureSampler");

    _shader->begin();
    _shader->load_uniform(texture0_sampler_location, 0);
    set_opacity(1.f);
    _shader->end();
}

splash_screen_shader::splash_screen_shader()
    : texture_2d_shader(shader_program::load(
        vertex_shader_text,
        splash_shader_text))
{

    _rays_location = _shader->get_uniform_location("rayOrigin");
    _power_location = _shader->get_uniform_location("power");
}

void texture_2d_shader::begin() { _shader->begin(); }
void texture_2d_shader::end() { _shader->end(); }

void texture_2d_shader::set_opacity(float opacity)
{
    _shader->load_uniform(_opacity_location, opacity);
}

void texture_visualizer::draw(texture_2d_shader& shader, uint32_t tex)
{
    shader.begin();
    shader.set_position_and_scale(_position, _scale);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    _geometry->draw();
    glBindTexture(GL_TEXTURE_2D, 0);
    shader.end();
}

obj_mesh texture_visualizer::create_mesh()
{
    obj_mesh res;

    res.positions.emplace_back(float3{-1.f, -1.f, 0.f});
    res.positions.emplace_back(float3{1.f, -1.f, 0.f});
    res.positions.emplace_back(float3{1.f, 1.f, 0.f});
    res.positions.emplace_back(float3{-1.f, 1.f, 0.f});

    res.uvs.emplace_back(float2{0.f, 1.f});
    res.uvs.emplace_back(float2{1.f, 1.f});
    res.uvs.emplace_back(float2{1.f, 0.f});
    res.uvs.emplace_back(float2{0.f, 0.f});
    
    res.indexes.emplace_back(int3{0, 1, 2});
    res.indexes.emplace_back(int3{2, 3, 0});

    return res;
}
