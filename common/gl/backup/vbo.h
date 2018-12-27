#pragma once

#include "rendering.h"

namespace rs2
{
    enum class vbo_type
    {
        array_buffer,
        element_array_buffer,
    };

    class vbo
    {
    public:
        vbo(vbo_type type = vbo_type::array_buffer);
        vbo(vbo&& other);
        ~vbo();

        void upload(int attribute, const float* xyz, int size, int count, bool dynamic = false);
        void upload(const int3* indx, int count);

        void draw_triangles();
        void draw_indexed_triangles();

        void bind();
        void unbind();

        uint32_t size() const { return _size; }
    private:
        vbo(const vbo& other) = delete;
        static int convert_type(vbo_type type);

        uint32_t _id;
        uint32_t _size = 0;
        vbo_type _type;
    };
}