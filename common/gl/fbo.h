#pragma once

#include "rendering.h"

namespace rs2
{
    class fbo
    {
    public:
        fbo(int w, int h);

        void createTextureAttachment(texture_buffer& color_tex);

        void createDepthTextureAttachment(texture_buffer& depth_tex);

        void bind();

        void unbind();

        void createDepthBufferAttachment();

        ~fbo();

        std::string get_status();

        int get_width() const { return _w; }
        int get_height() const { return _h; }

    private:
        uint32_t _id;
        uint32_t _db = 0;
        int _w, _h;
    };
}