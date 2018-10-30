#include "shader.h"

#include <glad/glad.h>

#include <iostream>

using namespace rs2;

int shader_program::get_uniform_location(const std::string& name)
{
    return glGetUniformLocation(_id, name.c_str());
}

void shader_program::load_uniform(int location, int value)
{
    glUniform1i(location, value);
}

void shader_program::load_uniform(int location, float value)
{
    glUniform1f(location, value);
}

void shader_program::load_uniform(int location, bool value)
{
    load_uniform(location, value ? 1.f : 0.f);
}

void shader_program::load_uniform(int location, const float3& vec)
{
    glUniform3f(location, vec.x, vec.y, vec.z);
}

void shader_program::load_uniform(int location, const float2& vec)
{
    glUniform2f(location, vec.x, vec.y);
}

void shader_program::load_uniform(int location, const matrix4& matrix)
{
    glUniformMatrix4fv(location, 1, GL_FALSE, (float*)&matrix);
}

void shader_program::bind_attribute(int attr, const std::string& name)
{
    glBindAttribLocation(_id, attr, name.c_str());
}

shader::shader(const std::string& shader_code, shader_type type)
{
    auto lambda = [&](){
        switch(type)
        {
            case shader_type::vertex: return GL_VERTEX_SHADER;
            case shader_type::fragment: return GL_FRAGMENT_SHADER;
            default:
                throw std::runtime_error("Unknown shader type!");
        }
    };
    const auto gl_type = lambda();
    
    GLuint shader_id = glCreateShader(gl_type);

    char const * source_ptr = shader_code.c_str();
    int length = shader_code.size();
    glShaderSource(shader_id, 1, &source_ptr, &length);
    
    glCompileShader(shader_id);

    GLint result;
    int log_length;
    
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);
    if ((result == GL_FALSE) && (log_length > 0)){
        std::vector<char> error_message(log_length+1);
        glGetShaderInfoLog(shader_id, log_length, NULL, &error_message[0]);
        std::string error(&error_message[0]);
	    std::cerr << error;
        glDeleteShader(shader_id);
        throw std::runtime_error(error);
    }
    
    _id = shader_id;
}
 
shader::~shader()
{
    glDeleteShader(_id);
}

shader_program::shader_program()
{
    GLuint program_id = glCreateProgram();
    _id = program_id;
}
shader_program::~shader_program()
{
    glUseProgram(0);
	glDeleteProgram(_id);
}

void shader_program::attach(const shader& shader)
{
    _shaders.push_back(&shader);
}
void shader_program::link()
{
    for(auto ps : _shaders)
    {
        glAttachShader(_id, ps->get_id());
    }
    
    glLinkProgram(_id);
    
    GLint result;
    int log_length;

    glGetProgramiv(_id, GL_LINK_STATUS, &result);
    glGetProgramiv(_id, GL_INFO_LOG_LENGTH, &log_length);
    if ((result == GL_FALSE) && (log_length > 0)){
        std::vector<char> error_message(log_length+1);
        glGetProgramInfoLog(_id, log_length, NULL, &error_message[0]);
        std::string error(&error_message[0]);
        std::cerr << error;
        for(auto ps : _shaders)
        {
            glDetachShader(_id, ps->get_id());
        }
        throw std::runtime_error(error);
    }
    
    glValidateProgram(_id);

    glGetProgramiv(_id, GL_VALIDATE_STATUS, &result);
    glGetProgramiv(_id, GL_INFO_LOG_LENGTH, &log_length);
    if ((result == GL_FALSE) && (log_length > 0)){
        std::vector<char> error_message(log_length+1);
        glGetProgramInfoLog(_id, log_length, NULL, &error_message[0]);
        std::string error(&error_message[0]);
        std::cerr << error;
        for(auto ps : _shaders)
        {
            glDetachShader(_id, ps->get_id());
        }
        throw std::runtime_error(error);
    }

    for(auto ps : _shaders)
    {
        glDetachShader(_id, ps->get_id());
    }
    _shaders.clear();
}

void shader_program::begin() const
{
    glUseProgram(_id);
}
void shader_program::end() const
{
    glUseProgram(0);
}

std::unique_ptr<shader_program> shader_program::load(
                                const std::string& vertex_shader,
                                const std::string& fragment_shader)
{
    std::unique_ptr<shader_program> res(new shader_program());
    shader vertex(vertex_shader, shader_type::vertex);
    shader fragment(fragment_shader, shader_type::fragment);
    res->attach(vertex);
    res->attach(fragment);
    res->link();
    return std::move(res);
}
