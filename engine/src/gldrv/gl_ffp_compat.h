/*
 * gl_ffp_compat.h
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
#ifndef VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_COMPAT_H
#define VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_COMPAT_H

/**
 * Drop-in replacements, with the same signatures and GL enums, for the
 * fixed-function entry points core profiles removed. They update the
 * emulated state in vsglState() instead of the driver's; the renderer turns
 * that state into uniforms at draw time.
 *
 * vsglEnable/vsglDisable/vsglIsEnabled take any capability: fixed-function
 * ones (GL_LIGHTING, GL_TEXTURE_2D, GL_FOG...) go to the emulation, the
 * rest straight to GL.
 */

#include "gl_globals.h"
#include "gl_ffp_state.h"

// Fixed-function enums some platforms' headers lack
#ifndef GL_REFLECTION_MAP
#define GL_REFLECTION_MAP 0x8512
#endif
#ifndef GL_NORMAL_MAP
#define GL_NORMAL_MAP 0x8511
#endif
#ifndef GL_FOG_DISTANCE_MODE_NV
#define GL_FOG_DISTANCE_MODE_NV 0x855A
#endif
#ifndef GL_EYE_RADIAL_NV
#define GL_EYE_RADIAL_NV 0x855B
#endif
#ifndef GL_EYE_PLANE_ABSOLUTE_NV
#define GL_EYE_PLANE_ABSOLUTE_NV 0x855C
#endif
#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif
#ifndef GL_RESCALE_NORMAL
#define GL_RESCALE_NORMAL 0x803A
#endif

/// The emulated fixed-function state of the (single) GL context
vsgl::FixedFunctionState &vsglState();

void vsglEnable(GLenum cap);
void vsglDisable(GLenum cap);
GLboolean vsglIsEnabled(GLenum cap);

void vsglActiveTexture(GLenum texture);

void vsglMatrixMode(GLenum mode);
void vsglLoadIdentity();
void vsglLoadMatrixf(const GLfloat *m);
void vsglLoadMatrixd(const GLdouble *m);
void vsglMultMatrixf(const GLfloat *m);
void vsglPushMatrix();
void vsglPopMatrix();
void vsglTranslatef(GLfloat x, GLfloat y, GLfloat z);
void vsglTranslated(GLdouble x, GLdouble y, GLdouble z);
void vsglScalef(GLfloat x, GLfloat y, GLfloat z);
void vsglRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);

void vsglLightf(GLenum light, GLenum pname, GLfloat param);
void vsglLightfv(GLenum light, GLenum pname, const GLfloat *params);
void vsglLightModeli(GLenum pname, GLint param);
void vsglLightModelfv(GLenum pname, const GLfloat *params);
void vsglMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
void vsglColorMaterial(GLenum face, GLenum mode);

void vsglColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void vsglColor4fv(const GLfloat *v);
void vsglColor3f(GLfloat r, GLfloat g, GLfloat b);

void vsglFogi(GLenum pname, GLint param);
void vsglFogf(GLenum pname, GLfloat param);
void vsglFogfv(GLenum pname, const GLfloat *params);

void vsglAlphaFunc(GLenum func, GLclampf ref);
void vsglShadeModel(GLenum mode);

/// Only GL_TEXTURE_ENV_MODE is meaningful; GFXTextureEnv() sets the combiner modes directly.
void vsglTexEnvi(GLenum target, GLenum pname, GLint param);
void vsglTexEnvfv(GLenum target, GLenum pname, const GLfloat *params);
void vsglTexGeni(GLenum coord, GLenum pname, GLint param);
void vsglTexGenfv(GLenum coord, GLenum pname, const GLfloat *params);

/// Maps a GL comparison enum (GL_LESS...) to the emulation's
vsgl::CompareFunc vsglCompareFunc(GLenum func);

#endif //VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_COMPAT_H
