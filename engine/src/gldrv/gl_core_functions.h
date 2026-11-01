/*
 * gl_core_functions.h
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
#ifndef VEGA_STRIKE_ENGINE_GLDRV_GL_CORE_FUNCTIONS_H
#define VEGA_STRIKE_ENGINE_GLDRV_GL_CORE_FUNCTIONS_H

/**
 * Entry points of the OpenGL 3.x/4.x core API used by the renderer, loaded at
 * runtime (Windows' opengl32.dll exports only GL 1.1, and macOS's legacy
 * headers do not declare them). The typedefs are spelled out here so this
 * works with any platform's gl.h. Include via gl_globals.h.
 *
 * The functions marked optional only exist on 4.3+/4.4+ (the 4.6 path);
 * their pointers stay null on the 4.1 path.
 */

#include <cstddef>

#ifndef APIENTRY
#define APIENTRY
#endif

typedef void (APIENTRY *vsglDebugProc)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
        const GLchar *message, const void *user_param);
typedef struct __GLsync *vsglSync;

#define VSGL_CORE_FUNCTIONS(X) \
    X(void, GenVertexArrays, (GLsizei n, GLuint *arrays)) \
    X(void, BindVertexArray, (GLuint array)) \
    X(void, DeleteVertexArrays, (GLsizei n, const GLuint *arrays)) \
    X(void, VertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, \
            const void *pointer)) \
    X(void, EnableVertexAttribArray, (GLuint index)) \
    X(void, DisableVertexAttribArray, (GLuint index)) \
    X(void, VertexAttrib4f, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)) \
    X(void, VertexAttrib4fv, (GLuint index, const GLfloat *v)) \
    X(void, UniformMatrix3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)) \
    X(void, UniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)) \
    X(void, BufferSubData, (GLenum target, ptrdiff_t offset, ptrdiff_t size, const void *data)) \
    X(void *, MapBufferRange, (GLenum target, ptrdiff_t offset, ptrdiff_t length, GLbitfield access)) \
    X(vsglSync, FenceSync, (GLenum condition, GLbitfield flags)) \
    X(GLenum, ClientWaitSync, (vsglSync sync, GLbitfield flags, unsigned long long timeout)) \
    X(void, DeleteSync, (vsglSync sync)) \
    X(void, GenerateMipmap, (GLenum target)) \
    X(const GLubyte *, GetStringi, (GLenum name, GLuint index)) \
    X(void, GetFramebufferAttachmentParameteriv, (GLenum target, GLenum attachment, GLenum pname, GLint *params)) \
    X(void, BindFragDataLocation, (GLuint program, GLuint color, const GLchar *name))

#define VSGL_OPTIONAL_FUNCTIONS(X) \
    X(void, BufferStorage, (GLenum target, ptrdiff_t size, const void *data, GLbitfield flags)) \
    X(void, DebugMessageCallback, (vsglDebugProc callback, const void *user_param)) \
    X(void, DebugMessageControl, (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, \
            GLboolean enabled))

#define VSGL_DECLARE_FUNCTION(ret, name, params) \
    typedef ret (APIENTRY *vsgl_pfn_##name) params; \
    extern vsgl_pfn_##name gl##name##_p;
VSGL_CORE_FUNCTIONS(VSGL_DECLARE_FUNCTION)
VSGL_OPTIONAL_FUNCTIONS(VSGL_DECLARE_FUNCTION)
#undef VSGL_DECLARE_FUNCTION

// Enums the legacy headers on some platforms lack
#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020
#endif
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#define GL_MAP_COHERENT_BIT 0x0080
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#define GL_ALREADY_SIGNALED 0x911A
#define GL_TIMEOUT_EXPIRED 0x911B
#define GL_CONDITION_SATISFIED 0x911C
#define GL_WAIT_FAILED 0x911D
#endif
#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#endif
#ifndef GL_DONT_CARE
#define GL_DONT_CARE 0x1100
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING 0x8210
#endif
#ifndef GL_SRGB
#define GL_SRGB 0x8C40
#endif
#ifndef GL_MAX_TEXTURE_IMAGE_UNITS
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#endif
#ifndef GL_ALIASED_LINE_WIDTH_RANGE
#define GL_ALIASED_LINE_WIDTH_RANGE 0x846E
#endif
#ifndef GL_TEXTURE_CUBE_MAP_SEAMLESS
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#endif
#ifndef GL_CONTEXT_FLAGS
#define GL_CONTEXT_FLAGS 0x821E
#endif

/// Loads the core entry points; returns false (naming the first missing one) if any is absent.
bool vsglLoadCoreFunctions(const char **missing);
/// Loads the 4.3+/4.4+ entry points the 4.6 path can use; those that are absent stay null.
void vsglLoadOptionalFunctions();

#endif //VEGA_STRIKE_ENGINE_GLDRV_GL_CORE_FUNCTIONS_H
