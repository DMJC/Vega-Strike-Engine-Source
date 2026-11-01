/*
 * gl_misc.cpp
 *
 * Vega Strike - Space Simulation, Combat and Trading
 * Copyright (C) 2001-2026 The Vega Strike Contributors:
 * Project creator: Daniel Horn
 * Original development team: As listed in the AUTHORS file. Specifically: Alan Shieh
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


#include <cstring>
#define GL_MISC_CPP
#include "gl_globals.h"
#undef GL_MISC_CPP
#include "src/vegastrike.h"
#include "src/gfxlib.h"
#include "root_generic/vs_globals.h"
#include "gl_light.h"
#include "winsys.h"
#include "root_generic/options.h"
#include "configuration/configuration.h"
#include "gl_ffp_compat.h"
#include "gl_renderer.h"
#include "gl_shader_translate.h"


// disable clientside draw for debugging purposes
//#define NODRAW 1

// cached gl state
static unsigned int s_array_buffer = 0;

bool GFXMultiTexAvailable() {
    return gl_options.Multitexture != 0;
}

void GFXCircle(float x, float y, float wid, float hei) {
    float segmag =
            (Vector(wid * configuration().graphics.resolution_x, 0,
                    0)
                    - Vector(static_cast<double>(wid) * configuration().graphics.resolution_x * cos(2.0 * M_PI / 360.0),
                            static_cast<double>(hei) * configuration().graphics.resolution_y * sin(2.0 * M_PI / 360.0),
                            0)).Magnitude();
    int accuracy = (int) (360.0f * configuration().graphics.circle_accuracy_flt * (1.0f < segmag ? 1.0f : segmag));
    if (accuracy < 4) {
        accuracy = 4;
    }
    float iaccuracy = 1.0f / accuracy;

    std::vector<float> verts(3 * (accuracy + 1));
    float *v = &verts[0];
    for (int i = 0; i <= accuracy; i++) {
        *v++ = x + wid * cos(i * 2 * M_PI * iaccuracy);
        *v++ = y + hei * sin(i * 2 * M_PI * iaccuracy);
        *v++ = 0.0f;
    }
    GFXDraw(GFXLINESTRIP, &verts[0], accuracy + 1);
}

/**
 * Copies an interleaved client-memory array into the streaming vertex buffer
 * and points the streaming VAO's attributes at it.
 */
static void GFXDrawSetup(const float data[], int vnum, int vsize, int csize, int tsize0, int tsize1) {
    assert(data && vsize);
    const vsgl::InterleavedLayout layout = vsgl::ComputeInterleavedLayout(vsize, csize, tsize0, tsize1);

    vsgl::BindStreamVertexArray();
    const size_t base = vsgl::StreamVertexData(data, static_cast<size_t>(layout.stride_bytes) * vnum);
    auto attrib = [&](GLuint index, int size, int offset) {
        if (size > 0) {
            (*glVertexAttribPointer_p)(index, size, GL_FLOAT, GL_FALSE, layout.stride_bytes,
                    reinterpret_cast<const GLvoid *>(base + offset));
            (*glEnableVertexAttribArray_p)(index);
        } else {
            (*glDisableVertexAttribArray_p)(index);
        }
    };
    attrib(vsgl::kAttribVertex, layout.position_size, layout.position_offset);
    attrib(vsgl::kAttribColor, layout.color_size, layout.color_offset);
    attrib(vsgl::kAttribMultiTexCoord0, layout.tex0_size, layout.tex0_offset);
    attrib(vsgl::kAttribMultiTexCoord0 + 1, layout.tex1_size, layout.tex1_offset);
    // Other users of the streaming VAO (GFXQuadList) may have left these on
    attrib(vsgl::kAttribNormal, 0, 0);
    attrib(vsgl::kAttribMultiTexCoord0 + 2, 0, 0);
}

static void GFXDrawStreamed(POLYTYPE type, const float data[], int vnum, const void *indices, int index_size,
        int nelem, int vsize, int csize, int tsize0, int tsize1) {
    GFXDrawSetup(data, vnum, vsize, csize, tsize0, tsize1);
    const vsgl::PolyType poly = static_cast<vsgl::PolyType>(type);
    const int count = indices != nullptr ? nelem : vnum;
    const vsgl::DrawPlan plan = vsgl::BuildDrawPlan(&poly, &count, 1, indices, index_size);
    size_t index_base = 0;
    if (!plan.indices.empty()) {
        index_base = vsgl::StreamIndexData(&plan.indices[0], plan.indices.size() * sizeof(uint32_t));
    } else if (indices != nullptr) {
        index_base = vsgl::StreamIndexData(indices, static_cast<size_t>(nelem) * index_size);
    }
    vsgl::PrepareDraw(vsgl::MapPolyType(poly).primitive, csize != 0);
    vsgl::DrawCommands(plan.commands, index_base);
}

void /*GFXDRVAPI*/ GFXDraw(POLYTYPE type, const float data[], int vnum,
        int vsize, int csize, int tsize0, int tsize1) {
#ifndef NODRAW
    if (vnum <= 0) {
        return;
    }
    GFXDrawStreamed(type, data, vnum, nullptr, 0, 0, vsize, csize, tsize0, tsize1);
#endif
}

void /*GFXDRVAPI*/ GFXDrawElements(POLYTYPE type,
        const float data[], int vnum, const unsigned char indices[], int nelem,
        int vsize, int csize, int tsize0, int tsize1) {
#ifndef NODRAW
    if (vnum <= 0 || nelem <= 0) {
        return;
    }
    GFXDrawStreamed(type, data, vnum, indices, 1, nelem, vsize, csize, tsize0, tsize1);
#endif
}

void /*GFXDRVAPI*/ GFXDrawElements(POLYTYPE type,
        const float data[], int vnum, const unsigned short indices[], int nelem,
        int vsize, int csize, int tsize0, int tsize1) {
#ifndef NODRAW
    if (vnum <= 0 || nelem <= 0) {
        return;
    }
    GFXDrawStreamed(type, data, vnum, indices, 2, nelem, vsize, csize, tsize0, tsize1);
#endif
}

void /*GFXDRVAPI*/ GFXDrawElements(POLYTYPE type,
        const float data[], int vnum, const unsigned int indices[], int nelem,
        int vsize, int csize, int tsize0, int tsize1) {
#ifndef NODRAW
    if (vnum <= 0 || nelem <= 0) {
        return;
    }
    GFXDrawStreamed(type, data, vnum, indices, 4, nelem, vsize, csize, tsize0, tsize1);
#endif
}

void /*GFXDRVAPI*/ GFXBindBuffer(unsigned int vbo_data) {
    if (s_array_buffer != vbo_data) {
        s_array_buffer = vbo_data;
        (*glBindBufferARB_p)(GL_ARRAY_BUFFER_ARB, vbo_data);
    }
}

void GFXForgetBuffer(unsigned int vbo_data) {
    if (s_array_buffer == vbo_data) {
        s_array_buffer = 0;
    }
}

void /*GFXDRVAPI*/ GFXBindElementBuffer(unsigned int element_data) {
    // The element buffer binding belongs to the bound vertex array object,
    // so it cannot be cached globally.
    (*glBindBufferARB_p)(GL_ELEMENT_ARRAY_BUFFER_ARB, element_data);
}

void /*GFXDRVAPI*/ GFXBeginScene() {
    GFXLoadIdentity(MODEL);     //bad this should instead load the cached view matrix
    light_rekey_frame();
}

void /*GFXDRVAPI*/ GFXEndScene() {
#ifdef NODRAW
    GFXClear( GFXTRUE );
#endif
}

void /*GFXDRVAPI*/ GFXClear(const GFXBOOL colorbuffer, const GFXBOOL depthbuffer, const GFXBOOL stencilbuffer) {
    glClear((colorbuffer ? GL_COLOR_BUFFER_BIT : 0)
            | (depthbuffer ? GL_DEPTH_BUFFER_BIT : 0)
            | (stencilbuffer ? GL_STENCIL_BUFFER_BIT : 0));
}

GFXBOOL /*GFXDRVAPI*/ GFXCapture(char *filename) {
    return GFXFALSE;
}

static float last_factor = 0;
static float last_units = 0;

void GFXGetPolygonOffset(float *factor, float *units) {
    *factor = last_factor;
    *units = last_units;
}

void /*GFXDRVAPI*/ GFXPolygonOffset(float factor, float units) {
    last_factor = factor;
    last_units = units;
    if (!factor && !units) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_POLYGON_OFFSET_POINT);
        glDisable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(0, 0);
    } else {
        glPolygonOffset(factor, units);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_POLYGON_OFFSET_POINT);
        glEnable(GL_POLYGON_OFFSET_LINE);
    }
}

void /*GFXDRVAPI*/ GFXPolygonMode(const enum POLYMODE polymode) {
    GLenum mode;
    switch (polymode) {
        default:
        case GFXFILLMODE:
            mode = GL_FILL;
            break;
        case GFXLINEMODE:
            mode = GL_LINE;
            break;
        case GFXPOINTMODE:
            mode = GL_POINT;
            break;
    }
    glPolygonMode(GL_FRONT_AND_BACK, mode);
}

void /*GFXDRVAPI*/ GFXCullFace(const enum POLYFACE polyface) {
    GLenum face;
    switch (polyface) {
        case GFXFRONT:
            face = GL_FRONT;
            break;
        default:
        case GFXBACK:
            face = GL_BACK;
            break;
        case GFXFRONTANDBACK:
            face = GL_FRONT_AND_BACK;
            break;
    }
    glCullFace(face);
}

void GFXPointSize(const float size) {
    glPointSize(size);
}

void GFXLineWidth(const float size) {
    // Forward-compatible contexts (macOS) reject anything wider than 1
    glLineWidth(vsgl::ClampLineWidth(size));
}

void /*GFXDRVAPI*/ GFXColorf(const GFXColor &col) {
    vsglColor4fv(&col.r);
}

GFXColor GFXColorf() {
    const float *col = vsglState().CurrentColor();
    return GFXColor(col[0], col[1], col[2], col[3]);
}

#if 0
//HELL slow on the TNT...we can't have it
void /*GFXDRVAPI*/ GFXBlendColor( const GFXColor &col )
{
#ifndef WIN32
    glBlendColor( col.r, col.g, col.b, col.a );
#endif
}
#endif

void /*GFXDRVAPI*/ GFXColor4f(const float r, const float g, const float b, const float a) {
    vsglColor4f(r, g, b, a);
}

// Display lists do not exist in core profiles; vertex lists keep their data
// in buffer objects instead (see gfxlib_struct.cpp).
int GFXCreateList() {
    return 0;
}

GFXBOOL GFXEndList() {
    return GFXFALSE;
}

void GFXCallList(int list) {
}

void GFXDeleteList(int list) {
}

void GFXSubwindow(int x, int y, int xsize, int ysize) {
    glViewport(0, 0, native_resolution_x, native_resolution_y);
    glScissor(x, y, xsize, ysize);
    if (x == 0 && y == 0 && xsize == configuration().graphics.resolution_x && ysize == configuration().graphics.resolution_y) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        glEnable(GL_SCISSOR_TEST);
    }
}

void GFXSubwindow(float x, float y, float xsize, float ysize) {
    GFXSubwindow(int(x * configuration().graphics.resolution_x), int(y * configuration().graphics.resolution_y), int(xsize * configuration().graphics.resolution_x),
            int(ysize * configuration().graphics.resolution_y));
}

Vector GFXDeviceToEye(int x, int y) {
    float l, r, b, t, n, f;
    GFXGetFrustumVars(true, &l, &r, &b, &t, &n, &f);
    return Vector((l + (r - l) * float(x) / configuration().graphics.resolution_x),
            (t + (b - t) * float(y) / configuration().graphics.resolution_y),
            n);
}

