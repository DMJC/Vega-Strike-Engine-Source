/*
 * gl_render_target.cpp
 *
 * Vega Strike - Space Simulation, Combat and Trading
 * Copyright (C) 2001-2026 The Vega Strike Contributors:
 * Project creator: Daniel Horn
 * Original development team: As listed in the AUTHORS file
 * Current development team: Roy Falk, Benjamen R. Meyer, Stephen G. Tuggy
 *
 * https://github.com/vegastrike/Vega-Strike-Engine-Source
 *
 * This file is part of Vega Strike.
 *
 * Vega Strike is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Vega Strike is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Vega Strike.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "gl_render_target.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <boost/format.hpp>

#include "winsys.h"
#include "src/gfxlib.h"
#include "src/vs_logging.h"
#include "configuration/configuration.h"

#ifndef GL_TEXTURE_BINDING_2D_ARRAY
#define GL_TEXTURE_BINDING_2D_ARRAY 0x8C1D
#endif
#ifndef GL_DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_READ_FRAMEBUFFER_BINDING
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#endif
#ifndef GL_RENDERBUFFER_BINDING
#define GL_RENDERBUFFER_BINDING 0x8CA7
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace vsgl {

namespace {

RenderTarget *g_current_target = nullptr;

const char *FramebufferStatusName(GLenum status) {
    switch (status) {
        case 0x8CD6: return "INCOMPLETE_ATTACHMENT";
        case 0x8CD7: return "INCOMPLETE_MISSING_ATTACHMENT";
        case 0x8CDB: return "INCOMPLETE_DRAW_BUFFER";
        case 0x8CDC: return "INCOMPLETE_READ_BUFFER";
        case 0x8CDD: return "UNSUPPORTED";
        case 0x8D56: return "INCOMPLETE_MULTISAMPLE";
        case 0x8DA8: return "INCOMPLETE_LAYER_TARGETS";
        default: return "unknown";
    }
}

// Restores the framebuffer and renderbuffer bindings on scope exit, so building
// or resolving a target never disturbs what the caller had bound.
class FramebufferBindingGuard {
public:
    FramebufferBindingGuard() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer_);
    }
    ~FramebufferBindingGuard() {
        (*glBindFramebuffer_p)(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_));
        (*glBindFramebuffer_p)(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_));
        (*glBindRenderbuffer_p)(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer_));
    }
private:
    GLint draw_ = 0;
    GLint read_ = 0;
    GLint renderbuffer_ = 0;
};

bool CheckComplete(GLuint fbo, const char *what) {
    (*glBindFramebuffer_p)(GL_FRAMEBUFFER, fbo);
    const GLenum status = (*glCheckFramebufferStatus_p)(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        VS_LOG(error, (boost::format("Render target %1% framebuffer incomplete: %2% (0x%3$x)")
                % what % FramebufferStatusName(status) % status));
        return false;
    }
    return true;
}

GLuint MakeRenderbuffer(int samples, GLenum format, int width, int height) {
    GLuint renderbuffer = 0;
    (*glGenRenderbuffers_p)(1, &renderbuffer);
    (*glBindRenderbuffer_p)(GL_RENDERBUFFER, renderbuffer);
    // samples == 0 gives an ordinary single-sampled renderbuffer
    (*glRenderbufferStorageMultisample_p)(GL_RENDERBUFFER, samples > 1 ? samples : 0, format, width, height);
    return renderbuffer;
}

} // namespace

RenderTarget::~RenderTarget() {
    Destroy();
}

RenderTarget::RenderTarget(RenderTarget &&other) noexcept {
    *this = std::move(other);
}

RenderTarget &RenderTarget::operator=(RenderTarget &&other) noexcept {
    if (this != &other) {
        Destroy();
        desc_ = other.desc_;
        draw_fbo_ = std::exchange(other.draw_fbo_, 0);
        resolve_fbo_ = std::exchange(other.resolve_fbo_, 0);
        color_texture_ = std::exchange(other.color_texture_, 0);
        owns_color_texture_ = std::exchange(other.owns_color_texture_, false);
        color_renderbuffer_ = std::exchange(other.color_renderbuffer_, 0);
        depth_renderbuffer_ = std::exchange(other.depth_renderbuffer_, 0);
        if (g_current_target == &other) {
            g_current_target = this;
        }
    }
    return *this;
}

bool RenderTarget::Create(const RenderTargetDesc &desc) {
    return Build(0, GL_TEXTURE_2D, 0, desc);
}

bool RenderTarget::CreateForTexture(GLuint texture, GLenum texture_target, GLint layer,
        const RenderTargetDesc &desc) {
    if (texture == 0) {
        VS_LOG(error, "RenderTarget::CreateForTexture: no texture given");
        return false;
    }
    return Build(texture, texture_target, layer, desc);
}

bool RenderTarget::Build(GLuint external_texture, GLenum texture_target, GLint layer,
        const RenderTargetDesc &requested) {
    Destroy();

    RenderTargetDesc desc = requested;
    const int max_size = MaxRenderTargetSize();
    if (desc.width <= 0 || desc.height <= 0 || desc.width > max_size || desc.height > max_size) {
        VS_LOG(error, (boost::format("Render target size %1%x%2% is outside 1..%3%")
                % desc.width % desc.height % max_size));
        return false;
    }
    desc.samples = std::max(1, std::min(desc.samples, MaxRenderTargetSamples()));
    if (desc.samples != std::max(1, requested.samples)) {
        VS_LOG(info, (boost::format("Render target: %1%x MSAA unsupported, using %2%x")
                % requested.samples % desc.samples));
    }
    desc_ = desc;

    FramebufferBindingGuard guard;

    // The single-sampled colour texture the result ends up in
    if (external_texture != 0) {
        color_texture_ = external_texture;
        owns_color_texture_ = false;
    } else {
        GLint previous_texture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
        glGenTextures(1, &color_texture_);
        owns_color_texture_ = true;
        glBindTexture(GL_TEXTURE_2D, color_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(desc.color_format), desc.width, desc.height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
        texture_target = GL_TEXTURE_2D;
        layer = 0;
    }

    auto attach_texture = [&]() {
        if (texture_target == GL_TEXTURE_2D_ARRAY) {
            (*glFramebufferTextureLayer_p)(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_texture_, 0, layer);
        } else {
            (*glFramebufferTexture2D_p)(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, color_texture_, 0);
        }
    };

    (*glGenFramebuffers_p)(1, &draw_fbo_);
    (*glBindFramebuffer_p)(GL_FRAMEBUFFER, draw_fbo_);
    if (desc.samples > 1) {
        // Draw into multisampled renderbuffers; Resolve() copies into the texture
        color_renderbuffer_ = MakeRenderbuffer(desc.samples, desc.color_format, desc.width, desc.height);
        (*glFramebufferRenderbuffer_p)(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_renderbuffer_);
    } else {
        attach_texture();
    }
    if (desc.depth_stencil) {
        depth_renderbuffer_ = MakeRenderbuffer(desc.samples, GL_DEPTH24_STENCIL8, desc.width, desc.height);
        (*glFramebufferRenderbuffer_p)(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                depth_renderbuffer_);
    }
    if (!CheckComplete(draw_fbo_, "draw")) {
        Release();
        return false;
    }

    if (desc.samples > 1) {
        (*glGenFramebuffers_p)(1, &resolve_fbo_);
        (*glBindFramebuffer_p)(GL_FRAMEBUFFER, resolve_fbo_);
        attach_texture();
        if (!CheckComplete(resolve_fbo_, "resolve")) {
            Release();
            return false;
        }
    }

    // Start from a defined image rather than whatever the driver hands out
    (*glBindFramebuffer_p)(GL_FRAMEBUFFER, draw_fbo_);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | (desc.depth_stencil ? (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) : 0));
    if (scissor) {
        glEnable(GL_SCISSOR_TEST);
    }

    VS_LOG(info, (boost::format("Created %1%x%2% render target (%3%, %4%x MSAA%5%)")
            % desc.width % desc.height
            % (desc.color_format == GL_SRGB8_ALPHA8 ? "sRGB" : "linear")
            % desc.samples
            % (owns_color_texture_ ? "" : ", external texture")));
    return true;
}

void RenderTarget::Destroy() {
    if (g_current_target == this) {
        BindRenderTarget(nullptr);
    }
    Release();
}

void RenderTarget::Release() {
    if (draw_fbo_ != 0) {
        (*glDeleteFramebuffers_p)(1, &draw_fbo_);
    }
    if (resolve_fbo_ != 0) {
        (*glDeleteFramebuffers_p)(1, &resolve_fbo_);
    }
    if (color_renderbuffer_ != 0) {
        (*glDeleteRenderbuffers_p)(1, &color_renderbuffer_);
    }
    if (depth_renderbuffer_ != 0) {
        (*glDeleteRenderbuffers_p)(1, &depth_renderbuffer_);
    }
    if (owns_color_texture_ && color_texture_ != 0) {
        glDeleteTextures(1, &color_texture_);
    }
    draw_fbo_ = resolve_fbo_ = color_renderbuffer_ = depth_renderbuffer_ = color_texture_ = 0;
    owns_color_texture_ = false;
}

void RenderTarget::Resolve() {
    if (!Valid() || resolve_fbo_ == 0) {
        return;
    }
    FramebufferBindingGuard guard;
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    (*glBindFramebuffer_p)(GL_READ_FRAMEBUFFER, draw_fbo_);
    (*glBindFramebuffer_p)(GL_DRAW_FRAMEBUFFER, resolve_fbo_);
    (*glBlitFramebuffer_p)(0, 0, desc_.width, desc_.height, 0, 0, desc_.width, desc_.height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
    if (scissor) {
        glEnable(GL_SCISSOR_TEST);
    }
}

void RenderTarget::DiscardDepthStencil() {
    if (!Valid() || depth_renderbuffer_ == 0 || glInvalidateFramebuffer_p == nullptr) {
        return;
    }
    FramebufferBindingGuard guard;
    (*glBindFramebuffer_p)(GL_FRAMEBUFFER, draw_fbo_);
    const GLenum attachment = GL_DEPTH_STENCIL_ATTACHMENT;
    (*glInvalidateFramebuffer_p)(GL_FRAMEBUFFER, 1, &attachment);
}

void BindRenderTarget(RenderTarget *target) {
    if (target != nullptr && !target->Valid()) {
        target = nullptr;
    }
    g_current_target = target;
    (*glBindFramebuffer_p)(GL_FRAMEBUFFER, target != nullptr ? target->DrawFramebuffer() : 0);
    int width = 0;
    int height = 0;
    DrawableSize(width, height);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
}

RenderTarget *CurrentRenderTarget() {
    return g_current_target;
}

void DrawableSize(int &width, int &height) {
    if (g_current_target != nullptr) {
        width = g_current_target->Width();
        height = g_current_target->Height();
    } else {
        width = native_resolution_x;
        height = native_resolution_y;
    }
}

GLenum WindowColorFormat() {
    return gl_options.ext_srgb_framebuffer ? GL_SRGB8_ALPHA8 : GL_RGBA8;
}

int MaxRenderTargetSize() {
    GLint renderbuffer_size = 0;
    GLint texture_size = 0;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &renderbuffer_size);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture_size);
    return std::max(1, static_cast<int>(std::min(renderbuffer_size, texture_size)));
}

int MaxRenderTargetSamples() {
    GLint samples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &samples);
    return std::max(1, static_cast<int>(samples));
}

void BlitToWindow(RenderTarget &source) {
    if (!source.Valid()) {
        BindRenderTarget(nullptr);
        return;
    }
    source.Resolve();
    source.DiscardDepthStencil();

    // Copy the encoded bytes as they are: with FRAMEBUFFER_SRGB off the blit
    // neither decodes the sRGB source nor re-encodes into the sRGB window.
    // The scissor test applies to blits, so it must be off too.
    const GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_SCISSOR_TEST);

    (*glBindFramebuffer_p)(GL_READ_FRAMEBUFFER, source.ResolvedFramebuffer());
    (*glBindFramebuffer_p)(GL_DRAW_FRAMEBUFFER, 0);
    const bool same_size = source.Width() == native_resolution_x && source.Height() == native_resolution_y;
    (*glBlitFramebuffer_p)(0, 0, source.Width(), source.Height(),
            0, 0, native_resolution_x, native_resolution_y,
            GL_COLOR_BUFFER_BIT, same_size ? GL_NEAREST : GL_LINEAR);

    if (srgb) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    }
    BindRenderTarget(nullptr);
}

} // namespace vsgl

// ---------------------------------------------------------------------------
// Scene target: the in-flight scene rendered offscreen at graphics.render_scale
// times the window size, then scaled onto the window.
// ---------------------------------------------------------------------------

namespace {

vsgl::RenderTarget g_scene_target;
bool g_scene_target_failed = false;   // don't retry every frame after a failure
int g_failed_width = 0;
int g_failed_height = 0;

float SceneRenderScale() {
    const float scale = configuration().graphics.render_scale_flt;
    if (!std::isfinite(scale)) {
        return 1.0F;
    }
    return std::max(0.25F, std::min(scale, 4.0F));
}

} // namespace

void GFXBeginSceneRender() {
    const float scale = SceneRenderScale();
    if (std::fabs(scale - 1.0F) < 0.001F || native_resolution_x <= 0 || native_resolution_y <= 0) {
        if (g_scene_target.Valid()) {
            g_scene_target.Destroy();
        }
        return;
    }
    const int width = std::max(1, static_cast<int>(std::lround(native_resolution_x * scale)));
    const int height = std::max(1, static_cast<int>(std::lround(native_resolution_y * scale)));
    const GLenum format = vsgl::WindowColorFormat();

    const vsgl::RenderTargetDesc &have = g_scene_target.Desc();
    if (!g_scene_target.Valid() || have.width != width || have.height != height || have.color_format != format) {
        if (g_scene_target_failed && g_failed_width == width && g_failed_height == height) {
            return;
        }
        vsgl::RenderTargetDesc desc;
        desc.width = width;
        desc.height = height;
        desc.color_format = format;
        if (!g_scene_target.Create(desc)) {
            VS_LOG(error, (boost::format("graphics.render_scale %1%: could not create a %2%x%3% scene target; "
                    "drawing to the window directly") % scale % width % height));
            g_scene_target_failed = true;
            g_failed_width = width;
            g_failed_height = height;
            return;
        }
        g_scene_target_failed = false;
    }
    vsgl::BindRenderTarget(&g_scene_target);
}

void GFXEndSceneRender() {
    if (vsgl::CurrentRenderTarget() == &g_scene_target) {
        vsgl::BlitToWindow(g_scene_target);
    }
}

void GFXDrawableSize(int &width, int &height) {
    vsgl::DrawableSize(width, height);
}
