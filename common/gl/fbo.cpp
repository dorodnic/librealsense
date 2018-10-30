#include "fbo.h"

using namespace rs2;

fbo::fbo(int w, int h) : _w(w), _h(h)
{
    glGenFramebuffers(1, &_id);
    glBindFramebuffer(GL_FRAMEBUFFER, _id);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
}

void fbo::createTextureAttachment(texture_buffer& color_tex)
{
    auto handle = color_tex.get_gl_handle();
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _w, _h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, handle, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void fbo::createDepthTextureAttachment(texture_buffer& depth_tex)
{
    auto handle = depth_tex.get_gl_handle();
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, _w, _h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, handle, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void fbo::bind()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, _id);
    glViewport(0, 0, _w, _h);
}

void fbo::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void fbo::createDepthBufferAttachment()
{
    if (_db) glDeleteRenderbuffers(1, &_db);
    glGenRenderbuffers(1, &_db);
    glBindRenderbuffer(GL_RENDERBUFFER, _db);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, _w, _h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _db);
}

fbo::~fbo()
{
    glDeleteRenderbuffers(1, &_db);
    glDeleteFramebuffers(1, &_id);
}

std::string fbo::get_status()
{
    std::string res = "UNKNOWN";

    bind();
    auto s = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (s == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) res = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    else if (s == 0x8CD9) res = "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
    else if (s == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) res = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    else if (s == GL_FRAMEBUFFER_UNSUPPORTED) res = "GL_FRAMEBUFFER_UNSUPPORTED";
    else if (s == GL_FRAMEBUFFER_COMPLETE) res = "GL_FRAMEBUFFER_COMPLETE";

    unbind();

    return res;
}
