/*
 * gl_render_target.h
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
#ifndef VEGA_STRIKE_ENGINE_GLDRV_GL_RENDER_TARGET_H
#define VEGA_STRIKE_ENGINE_GLDRV_GL_RENDER_TARGET_H

/**
 * Offscreen render targets (framebuffer objects).
 *
 * A RenderTarget is a colour buffer plus an optional depth/stencil buffer
 * that the renderer can draw into instead of the window. While one is bound
 * (BindRenderTarget), every GFX viewport and scissor computation uses its
 * size rather than the window's, so the scene, cockpit and HUD draw into it
 * unchanged.
 *
 * The colour buffer is either a texture the target owns, or a texture owned
 * by someone else (CreateForTexture) - e.g. an OpenXR swapchain image. With
 * samples > 1 the target draws into multisampled renderbuffers and Resolve()
 * copies them into the colour texture.
 *
 * Everything here is core in GL 3.0, so it works on both the 4.6 and 4.1
 * profiles.
 */

#include "gl_globals.h"

namespace vsgl {

struct RenderTargetDesc {
    int width = 0;
    int height = 0;
    /// Colour internal format; GL_RGBA8 or GL_SRGB8_ALPHA8 (see WindowColorFormat())
    GLenum color_format = GL_RGBA8;
    /// 1 = single-sampled; >1 renders to multisampled buffers resolved by Resolve()
    int samples = 1;
    /// Attach a 24-bit depth / 8-bit stencil buffer
    bool depth_stencil = true;
};

class RenderTarget {
public:
    RenderTarget() = default;
    ~RenderTarget();
    RenderTarget(const RenderTarget &) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;
    RenderTarget(RenderTarget &&other) noexcept;
    RenderTarget &operator=(RenderTarget &&other) noexcept;

    /// Creates a target with its own colour texture. Returns false (and leaves
    /// the target invalid) if the driver rejects the configuration.
    bool Create(const RenderTargetDesc &desc);

    /// Creates a target that renders into an existing texture it does not own.
    /// `texture_target` is GL_TEXTURE_2D or GL_TEXTURE_2D_ARRAY (then `layer`
    /// picks the slice). `desc.color_format` must be the texture's format.
    bool CreateForTexture(GLuint texture, GLenum texture_target, GLint layer, const RenderTargetDesc &desc);

    /// Releases the GL objects. Unbinds the target first if it is bound.
    void Destroy();

    bool Valid() const {
        return draw_fbo_ != 0;
    }
    int Width() const {
        return desc_.width;
    }
    int Height() const {
        return desc_.height;
    }
    int Samples() const {
        return desc_.samples;
    }
    const RenderTargetDesc &Desc() const {
        return desc_;
    }
    /// The single-sampled colour texture holding the result (after Resolve()).
    GLuint ColorTexture() const {
        return color_texture_;
    }
    /// The framebuffer drawing goes into (multisampled when Samples() > 1).
    GLuint DrawFramebuffer() const {
        return draw_fbo_;
    }
    /// The framebuffer holding the single-sampled result.
    GLuint ResolvedFramebuffer() const {
        return resolve_fbo_ != 0 ? resolve_fbo_ : draw_fbo_;
    }

    /// Copies the multisampled colour into ColorTexture(); a no-op when
    /// single-sampled. Leaves the GL framebuffer bindings as they were.
    void Resolve();

    /// Tells the driver the depth/stencil contents need not be kept (saves
    /// bandwidth on tiled GPUs). Only on the 4.3+ path; a no-op elsewhere.
    void DiscardDepthStencil();

private:
    bool Build(GLuint external_texture, GLenum texture_target, GLint layer, const RenderTargetDesc &desc);
    void Release();

    RenderTargetDesc desc_;
    GLuint draw_fbo_ = 0;
    GLuint resolve_fbo_ = 0;            // only when multisampled
    GLuint color_texture_ = 0;
    bool owns_color_texture_ = false;
    GLuint color_renderbuffer_ = 0;     // multisampled colour
    GLuint depth_renderbuffer_ = 0;
};

/// Directs subsequent drawing into `target`, or into the window when null.
/// Sets the viewport to cover the whole target and disables the scissor test;
/// the next GFXSubwindow narrows it again as usual.
void BindRenderTarget(RenderTarget *target);

/// The bound target, or null when drawing into the window.
RenderTarget *CurrentRenderTarget();

/// Size in pixels of whatever is being drawn into: the bound target, else the window.
void DrawableSize(int &width, int &height);

/// The colour format matching the window's back buffer, so an offscreen
/// image looks the same once copied to it: sRGB when the window is sRGB.
GLenum WindowColorFormat();

/// Largest width/height and sample count the driver accepts for a target.
int MaxRenderTargetSize();
int MaxRenderTargetSamples();

/// Resolves `source` and copies it, scaled to fit, over the whole window,
/// then leaves the window bound (as BindRenderTarget(nullptr) would).
void BlitToWindow(RenderTarget &source);

} // namespace vsgl

#endif //VEGA_STRIKE_ENGINE_GLDRV_GL_RENDER_TARGET_H
