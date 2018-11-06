#pragma once

#include "rendering.h"
#include "shader.h"

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
}