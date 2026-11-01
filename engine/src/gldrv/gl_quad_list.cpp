/*
 * gl_quad_list.cpp
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


#include "gl_globals.h"
#include "gl_primitives.h"
#include "gl_renderer.h"
#include "gl_shader_translate.h"
#include <cstddef>
#include <vector>

#include "src/gfxlib.h"
#include "src/vegastrike.h"
#include "root_generic/vs_globals.h"
#include <stdio.h>
// #include "vegadisk/vsfilesystem.h"   // Is this still needed? -- stephengtuggy 2021-09-06
#include "src/vs_logging.h"

GFXQuadList::GFXQuadList(GFXBOOL color) : numVertices(0), numQuads(0) {
    data.vertices = NULL;
    Dirty = GFXFALSE;
    isColor = color;
}

GFXQuadList::~GFXQuadList() {
    if (isColor && data.colors) {
        free(data.colors);
    } else if (!isColor && data.vertices) {
        free(data.vertices);
    }
}

template<typename V>
static void SetQuadAttributes(size_t base, bool color) {
    const GLsizei stride = sizeof(V);
    auto attrib = [&](GLuint index, GLint size, size_t offset) {
        (*glVertexAttribPointer_p)(index, size, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(base + offset));
        (*glEnableVertexAttribArray_p)(index);
    };
    // What glInterleavedArrays(GL_T2F_[C4F_]N3F_V3F) used to set up
    attrib(vsgl::kAttribVertex, 3, offsetof(V, x));
    attrib(vsgl::kAttribNormal, 3, offsetof(V, i));
    attrib(vsgl::kAttribMultiTexCoord0, 2, offsetof(V, s));
    if (color) {
        attrib(vsgl::kAttribColor, 4, offsetof(GFXColorVertex, r));
    } else {
        (*glDisableVertexAttribArray_p)(vsgl::kAttribColor);
    }
    (*glDisableVertexAttribArray_p)(vsgl::kAttribMultiTexCoord0 + 1);
    (*glDisableVertexAttribArray_p)(vsgl::kAttribMultiTexCoord0 + 2);
}

void GFXQuadList::Draw() {
    if (!numQuads) {
        return;
    }
    vsgl::BindStreamVertexArray();
    const size_t count = static_cast<size_t>(numQuads) * 4;
    if (isColor) {
        const size_t base = vsgl::StreamVertexData(&data.colors[0], count * sizeof(GFXColorVertex));
        SetQuadAttributes<GFXColorVertex>(base, true);
    } else {
        const size_t base = vsgl::StreamVertexData(&data.vertices[0], count * sizeof(GFXVertex));
        SetQuadAttributes<GFXVertex>(base, false);
    }
    std::vector<uint32_t> indices;
    vsgl::AppendQuadIndices(0, static_cast<uint32_t>(count), indices);
    const size_t index_base = vsgl::StreamIndexData(&indices[0], indices.size() * sizeof(uint32_t));
    vsgl::PrepareDraw(vsgl::Primitive::Triangles, isColor != GFXFALSE);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT,
            reinterpret_cast<const GLvoid *>(index_base));
    ++gl_batches_this_frame;
    gl_vertices_this_frame += static_cast<int>(count);
}

int GFXQuadList::AddQuad(const GFXVertex *vertices, const GFXColorVertex *color) {
    int cur = numQuads * 4;
    if (cur + 3 >= numVertices) {
        if (!numVertices) {
            numVertices = 16;
            if (!isColor) {
                data.vertices = (GFXVertex *) malloc(numVertices * sizeof(GFXVertex));
            } else {
                data.colors = (GFXColorVertex *) malloc(numVertices * sizeof(GFXColorVertex));
            }
            quadassignments = (int *) malloc(numVertices * sizeof(int) / 4);
            for (int i = 0; i < numVertices / 8; i++) {
                quadassignments[i] = -1;
            }
        } else {
            numVertices *= 2;
            if (!isColor) {
                data.vertices = (GFXVertex *) realloc(data.vertices, numVertices * sizeof(GFXVertex));
            } else {
                data.colors = (GFXColorVertex *) realloc(data.colors, numVertices * sizeof(GFXColorVertex));
            }
            int *tmp = (int *) realloc(quadassignments, numVertices * sizeof(int) / 4);
            if (tmp == nullptr) {
                VS_LOG_AND_FLUSH(fatal, "Error reallocating quadassignments!");
                return -1;
            } else {
                quadassignments = tmp;
            }
        }
        for (int i = numVertices / 8; i < numVertices / 4; i++) {
            quadassignments[i] = -1;
        }
        Dirty = numVertices / 8;
        quadassignments[numQuads] = numQuads;
        numQuads++;
        if (!isColor && vertices) {
            memcpy(data.vertices + cur, vertices, 4 * sizeof(GFXVertex));
        }
        if (isColor && color) {
            memcpy(data.colors + cur, color, 4 * sizeof(GFXColorVertex));
        }
        return numQuads - 1;
    }
    for (int i = 0; i < numVertices / 4; i++) {
        if (quadassignments[i] == -1) {
            quadassignments[i] = numQuads;
            if (!isColor && vertices) {
                memcpy(data.vertices + (quadassignments[i] * 4), vertices, 4 * sizeof(GFXVertex));
            }
            if (isColor && color) {
                memcpy(data.colors + (quadassignments[i] * 4), color, 4 * sizeof(GFXColorVertex));
            }
            numQuads++;
            Dirty--;
            return i;
        }
    }
    VS_LOG_AND_FLUSH(fatal, "Fatal Error adding quads");
    //should NOT get here!
    return -1;
}

void GFXQuadList::DelQuad(int which) {
    if (quadassignments[which] >= numQuads) {
        VS_LOG(error, "error del");
        return;
    }
    if (which < 0 || which >= numVertices / 4 || quadassignments[which] == -1) {
        return;
    }
    Dirty++;
    for (int i = 0; i < numVertices / 4; i++) {
        if (quadassignments[i] == numQuads - 1) {
            if (isColor) {
                memcpy(data.colors + (quadassignments[which] * 4),
                        data.colors + ((numQuads - 1) * 4),
                        4 * sizeof(GFXColorVertex));
            } else {
                memcpy(data.vertices + (quadassignments[which] * 4),
                        data.vertices + ((numQuads - 1) * 4),
                        4 * sizeof(GFXVertex));
            }
            quadassignments[i] = quadassignments[which];
            quadassignments[which] = -1;
            numQuads--;
            return;
        }
    }
    VS_LOG(info, " error deleting engine flame");
}

void GFXQuadList::ModQuad(int which, const GFXVertex *vertices, float alpha) {
    if (which < 0 || which >= numVertices / 4 || quadassignments[which] == -1) {
        return;
    }
    if (isColor) {
        int w = quadassignments[which] * 4;

        data.colors[w + 0].SetVtx(vertices[0]);
        data.colors[w + 1].SetVtx(vertices[1]);
        data.colors[w + 2].SetVtx(vertices[2]);
        data.colors[w + 3].SetVtx(vertices[3]);
        if (alpha != -1) {
            if (alpha == 0) {
                alpha = .01;
            }
            float alp = (data.colors[w].r > data.colors[w].b)
                    ? ((data.colors[w].r > data.colors[w].g) ? data.colors[w].r : data.colors[w].g)
                    : ((data.colors[w].b > data.colors[w].g) ? data.colors[w].b : data.colors[w].g);
            if (alp > .0001) {
                float tmp[4] =
                        {alpha * data.colors[w + 0].r / alp, alpha * data.colors[w + 0].g / alp,
                                alpha * data.colors[w + 0].b / alp, alpha};
                memcpy(&data.colors[w + 0].r, tmp, sizeof(float) * 4);
                memcpy(&data.colors[w + 1].r, tmp, sizeof(float) * 4);
                memcpy(&data.colors[w + 2].r, tmp, sizeof(float) * 4);
                memcpy(&data.colors[w + 3].r, tmp, sizeof(float) * 4);
            }
        }
    } else {
        memcpy(data.vertices + (quadassignments[which] * 4), vertices, 4 * sizeof(GFXVertex));
    }
}

void GFXQuadList::ModQuad(int which, const GFXColorVertex *vertices) {
    if (which < 0 || which >= numVertices / 4 || quadassignments[which] == -1) {
        return;
    }
    if (isColor) {
        memcpy(data.vertices + (quadassignments[which] * 4), vertices, 4 * sizeof(GFXColorVertex));
    } else {
        data.vertices[(quadassignments[which] * 4) + 0].SetTexCoord(vertices[0].s,
                vertices[0].t).SetNormal(Vector(vertices[0].i,
                vertices[0].j,
                vertices[0].k)).
                SetVertex(
                Vector(vertices[0].x,
                        vertices
                        [
                                0
                        ].y, vertices[0].z));
        data.vertices[(quadassignments[which] * 4) + 1].SetTexCoord(vertices[1].s,
                vertices[1].t).SetNormal(Vector(vertices[1].i,
                vertices[1].j,
                vertices[1].k)).
                SetVertex(
                Vector(vertices[1].x,
                        vertices
                        [
                                1
                        ].y, vertices[1].z));
        data.vertices[(quadassignments[which] * 4) + 2].SetTexCoord(vertices[2].s,
                vertices[2].t).SetNormal(Vector(vertices[2].i,
                vertices[2].j,
                vertices[2].k)).
                SetVertex(
                Vector(vertices[2].x,
                        vertices
                        [
                                2
                        ].y, vertices[2].z));
        data.vertices[(quadassignments[which] * 4) + 3].SetTexCoord(vertices[3].s,
                vertices[3].t).SetNormal(Vector(vertices[3].i,
                vertices[3].j,
                vertices[3].k)).
                SetVertex(
                Vector(vertices[3].x,
                        vertices
                        [
                                3
                        ].y, vertices[3].z));
    }
}
