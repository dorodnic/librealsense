#pragma once

#include "rendering.h"
#include "vbo.h"
#include "loader.h"

#include <memory>

namespace rs2
{
    class vao
    {
    public:
        static std::unique_ptr<vao> create(const obj_mesh& m);

        vao(const float3* vert, const float2* uvs, const float3* normals, 
            const float3* tangents, int vert_count, const int3* indx, int indx_count);
        ~vao();
        void bind();
        void unbind();
        void draw();

        // void update_uvs(const float2* uvs);
        void update_positions(const float3* vert);

        vao(vao&& other);

    private:
        vao(const vao& other) = delete;

        uint32_t _id;
        int _vertex_count;
        vbo _vertexes, _normals, _indexes, _uvs, _tangents;
    };
}