/*
 * gl_renderer.h
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
#ifndef VEGA_STRIKE_ENGINE_GLDRV_GL_RENDERER_H
#define VEGA_STRIKE_ENGINE_GLDRV_GL_RENDERER_H

/**
 * The core-profile draw path shared by every GFX draw call:
 *
 *  - every draw goes through PrepareDraw(), which binds the user program,
 *    or the built-in fixed-function program when none is active, and
 *    uploads whatever emulated fixed-function state changed since that
 *    program last saw it;
 *  - vertex data lives in buffer objects: static VBOs owned by
 *    GFXVertexList, and a streaming ring buffer for client-memory draws
 *    (GFXDraw, GFXDrawElements, GFXQuadList...).
 */

#include <cstddef>
#include <vector>

#include "gl_globals.h"
#include "gl_primitives.h"
#include "gl_profile.h"

namespace vsgl {

/// Creates the fixed-function programs and streaming buffers. Needs a current context.
bool InitRenderer(const ContextProfile &profile);
void ShutdownRenderer();
const ContextProfile &CurrentProfile();

/// Called after a successful link: looks up the built-in uniforms the program uses.
void RegisterProgram(GLuint program);
void ForgetProgram(GLuint program);
/// GFXActivateShader(): 0 means "fixed function"
void SetUserProgram(GLuint program);
GLuint UserProgram();

/**
 * Binds the program for the next draw and brings its built-in uniforms up
 * to date. `color_array` tells whether the bound vertex array provides
 * colors; when it does not, the current color is used, as in GL 2.x.
 */
void PrepareDraw(Primitive primitive, bool color_array);

/// Bind the VAO used for client-memory draws
void BindStreamVertexArray();

/**
 * Copies vertex data into the streaming vertex buffer (binding it to
 * GL_ARRAY_BUFFER) and returns its byte offset there.
 */
size_t StreamVertexData(const void *data, size_t bytes);

/**
 * Copies indices into the streaming index buffer and binds it to
 * GL_ELEMENT_ARRAY_BUFFER of the *currently bound* VAO; returns the byte offset.
 */
size_t StreamIndexData(const void *data, size_t bytes);

/// Re-points the current VAO's element buffer at `buffer` (0 to unbind)
void BindElementBuffer(GLuint buffer);

/**
 * Issues the draws of a plan. Indexed commands read their indices from the
 * element buffer bound to the current VAO, starting at `index_base` bytes.
 */
void DrawCommands(const std::vector<DrawCommand> &commands, size_t index_base);

/// GL_UNSIGNED_BYTE/SHORT/INT for an index size of 1, 2 or 4
GLenum IndexType(int index_size);

/// Clamps a line width to what the context accepts (1.0 on forward-compatible contexts)
float ClampLineWidth(float width);

} // namespace vsgl

#endif //VEGA_STRIKE_ENGINE_GLDRV_GL_RENDERER_H
