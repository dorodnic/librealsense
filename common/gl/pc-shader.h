#pragma once

#include "rendering.h"
#include "shader.h"

namespace rs2
{
    class pointcloud_shader
    {
    public:
        pointcloud_shader();

        void begin();
        void end();

        void set_mvp(const matrix4& model,
                    const matrix4& view,
                    const matrix4& projection);


        int texture_slot() const { return 0; }
        int geometry_slot() const { return 1; }
        int uvs_slot() const { return 2; }

        void set_image_size(int width, int height);
        void set_min_delta_z(float min_delta_z);
    protected:
        pointcloud_shader(std::unique_ptr<shader_program> shader);

        std::unique_ptr<shader_program> _shader;

    private:
        void init();

        uint32_t _transformation_matrix_location;
        uint32_t _projection_matrix_location;
        uint32_t _camera_matrix_location;

        uint32_t _width_location, _height_location;
        uint32_t _min_delta_z_location;
    };
}