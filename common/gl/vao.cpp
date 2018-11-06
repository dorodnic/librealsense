#include "vao.h"

#include <glad/glad.h>

using namespace rs2;

vao::vao(const float3* vert, const float2* uvs, const float3* normals,
         const float3* tangents, int vert_count, const int3* indx, int indx_count)
    : _vertexes(vbo_type::array_buffer),
      _uvs(vbo_type::array_buffer),
      _indexes(vbo_type::element_array_buffer),
      _tangents(vbo_type::array_buffer),
      _vertex_count(vert_count)
{
    glGenVertexArrays(1, &_id);
    bind();
    _indexes.upload(indx, indx_count);
    _vertexes.upload(0, (float*)vert, 3, vert_count, true);
    if (normals) _normals.upload(2, (float*)normals, 3, vert_count);
    if (tangents) _tangents.upload(3, (float*)tangents, 3, vert_count);
    if (uvs) _uvs.upload(1, (float*)uvs, 2, vert_count);
    unbind();
}

std::unique_ptr<vao> vao::create(const obj_mesh& mesh)
{
    return std::unique_ptr<vao>(new vao(mesh.positions.data(),
        mesh.uvs.data(),
        mesh.normals.data(),
        mesh.tangents.data(),
        mesh.positions.size(),
        mesh.indexes.data(),
        mesh.indexes.size()));
}

vao::vao(vao&& other)
    : _id(other._id), 
      _indexes(std::move(other._indexes)),
      _vertexes(std::move(other._vertexes)),
      _uvs(std::move(other._uvs)),
      _normals(std::move(other._normals)),
      _tangents(std::move(other._tangents))
{
    other._id = 0;
}

vao::~vao()
{
    if (_id) glDeleteVertexArrays(1, &_id);
}

void vao::bind()
{
    glBindVertexArray(_id);
}

void vao::unbind()
{
    glBindVertexArray(0);
}

void vao::draw()
{
    bind();

    glEnableVertexAttribArray(0); // vertex
    if (_uvs.size())        glEnableVertexAttribArray(1); // uv
    if (_normals.size())    glEnableVertexAttribArray(2); // normals
    if (_tangents.size())   glEnableVertexAttribArray(3); // tangents
    
    _indexes.draw_indexed_triangles();
    
    glDisableVertexAttribArray(0);
    if (_uvs.size())        glDisableVertexAttribArray(1);
    if (_normals.size())    glDisableVertexAttribArray(2);
    if (_tangents.size())   glDisableVertexAttribArray(3);

    unbind();
}

// void vao::update_uvs(const float2* uvs)
// {
//     _uvs.upload(1, (float*)uvs, 2, _vertex_count, true);
// }

void vao::update_positions(const float3* vert)
{
    _vertexes.upload(0, (float*)vert, 3, _vertex_count, true);
}
