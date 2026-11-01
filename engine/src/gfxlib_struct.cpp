/*
 * gfxlib_struct.cpp
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


#include "src/gfxlib_struct.h"
#include "src/gfxlib.h"
#include "gldrv/gl_globals.h"
#include "gldrv/gl_primitives.h"
#include "gldrv/gl_renderer.h"
#include "gldrv/gl_shader_translate.h"
#include <stdio.h>
#include "root_generic/xml_support.h"
#include "src/config_xml.h"
#include "root_generic/vs_globals.h"
#include "root_generic/vega_random.h"
#include "src/vs_logging.h"
#include "imgui.h"

#include "root_generic/options.h"

#include <cstddef>
#include <cstring>
#include <vector>

#include "configuration/configuration.h"


// Implicit conversion FROM ImU32
GFXColor::GFXColor(ImU32 col) {
    r = ((col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
    g = ((col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
    b = ((col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
    a = ((col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
}

// Implicit conversion TO ImU32
GFXColor::operator ImU32() const {
    ImU32 R = (ImU32)(r * 255.0f + 0.5f);
    ImU32 G = (ImU32)(g * 255.0f + 0.5f);
    ImU32 B = (ImU32)(b * 255.0f + 0.5f);
    ImU32 A = (ImU32)(a * 255.0f + 0.5f);

    return IM_COL32(R, G, B, A);
}


// The core-profile primitive mapping in gldrv/gl_primitives.h mirrors POLYTYPE
static_assert(static_cast<int>(vsgl::PolyType::Tri) == GFXTRI, "POLYTYPE mismatch");
static_assert(static_cast<int>(vsgl::PolyType::Quad) == GFXQUAD, "POLYTYPE mismatch");
static_assert(static_cast<int>(vsgl::PolyType::Line) == GFXLINE, "POLYTYPE mismatch");
static_assert(static_cast<int>(vsgl::PolyType::TriStrip) == GFXTRISTRIP, "POLYTYPE mismatch");
static_assert(static_cast<int>(vsgl::PolyType::QuadStrip) == GFXQUADSTRIP, "POLYTYPE mismatch");
static_assert(static_cast<int>(vsgl::PolyType::TriFan) == GFXTRIFAN, "POLYTYPE mismatch");
static_assert(static_cast<int>(vsgl::PolyType::LineStrip) == GFXLINESTRIP, "POLYTYPE mismatch");
static_assert(static_cast<int>(vsgl::PolyType::Poly) == GFXPOLY, "POLYTYPE mismatch");
static_assert(static_cast<int>(vsgl::PolyType::Point) == GFXPOINT, "POLYTYPE mismatch");

/// The core-profile GL primitive a POLYTYPE is drawn with (quads become triangles)
GLenum PolyLookup(POLYTYPE poly) {
    return vsgl::PrimitiveToGL(vsgl::MapPolyType(static_cast<vsgl::PolyType>(poly)).primitive);
}

static int IndexSize(char changed) {
    return (changed & INDEX_BYTE) ? 1 : ((changed & INDEX_SHORT) ? 2 : ((changed & INDEX_INT) ? 4 : 0));
}

/// Points the bound VAO at an interleaved GFXVertex / GFXColorVertex array in the bound VBO
template<typename V>
static void SetVertexAttributes(bool color) {
    const GLsizei stride = sizeof(V);
    auto attrib = [stride](GLuint index, GLint size, size_t offset) {
        (*glVertexAttribPointer_p)(index, size, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const GLvoid *>(offset));
        (*glEnableVertexAttribArray_p)(index);
    };
    attrib(vsgl::kAttribVertex, 3, offsetof(V, x));
    attrib(vsgl::kAttribNormal, 3, offsetof(V, i));
    // Same texture coordinate assignment as the old client arrays: the
    // texcoords on units 0 and 1, the tangent on unit 2
    attrib(vsgl::kAttribMultiTexCoord0, 2, offsetof(V, s));
    attrib(vsgl::kAttribMultiTexCoord0 + 1, 2, offsetof(V, s));
    attrib(vsgl::kAttribMultiTexCoord0 + 2, 4, offsetof(V, tx));
    if (color) {
        attrib(vsgl::kAttribColor, 4, offsetof(GFXColorVertex, r));
    } else {
        (*glDisableVertexAttribArray_p)(vsgl::kAttribColor);
    }
}

void GFXVertexList::RefreshDisplayList() {
    if (vertex_array == 0) {
        (*glGenVertexArrays_p)(1, &vertex_array);
    }
    if (vbo_data == 0) {
        (*glGenBuffersARB_p)(1, (GLuint *) &vbo_data);
    }
    (*glBindVertexArray_p)(vertex_array);

    const bool color = (changed & HAS_COLOR) != 0;
    const size_t vertex_size = color ? sizeof(GFXColorVertex) : sizeof(GFXVertex);
    const GLenum usage = (changed & CHANGE_MUTABLE) ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB;
    GFXBindBuffer(vbo_data);
    if (numVertices > 0) {
        if (numVertices == vbo_capacity) {
            (*glBufferSubData_p)(GL_ARRAY_BUFFER_ARB, 0, numVertices * vertex_size, data.vertices);
        } else {
            (*glBufferDataARB_p)(GL_ARRAY_BUFFER_ARB, numVertices * vertex_size, data.vertices, usage);
            vbo_capacity = numVertices;
        }
    }
    if (color) {
        SetVertexAttributes<GFXColorVertex>(true);
    } else {
        SetVertexAttributes<GFXVertex>(false);
    }

    // Work out the core-profile draws once; quads get indices of their own
    const int index_size = IndexSize(changed);
    const void *indices = (index_size != 0) ? static_cast<const void *>(index.b) : nullptr;
    std::vector<vsgl::PolyType> types(numlists);
    for (int i = 0; i < numlists; ++i) {
        types[i] = static_cast<vsgl::PolyType>(mode[i]);
    }
    if (draw_plan == nullptr) {
        draw_plan = new vsgl::DrawPlan;
    }
    *draw_plan = vsgl::BuildDrawPlan(numlists > 0 ? &types[0] : nullptr, offsets, numlists, indices, index_size);

    const void *element_data = nullptr;
    size_t element_bytes = 0;
    if (!draw_plan->indices.empty()) {
        element_data = &draw_plan->indices[0];
        element_bytes = draw_plan->indices.size() * sizeof(uint32_t);
    } else if (indices != nullptr) {
        size_t total = 0;
        for (int i = 0; i < numlists; ++i) {
            total += offsets[i];
        }
        element_data = indices;
        element_bytes = total * index_size;
    }
    if (element_bytes > 0) {
        if (display_list == 0) {
            (*glGenBuffersARB_p)(1, (GLuint *) &display_list);
        }
        vsgl::BindElementBuffer(display_list);
        (*glBufferDataARB_p)(GL_ELEMENT_ARRAY_BUFFER_ARB, element_bytes, element_data, usage);
    } else {
        vsgl::BindElementBuffer(0);
    }
    // The rebuilt or generated indices are now on the GPU
    draw_plan->indices.clear();
    draw_plan->indices.shrink_to_fit();
}

void GFXVertexList::BeginDrawState(GFXBOOL lock) {
    if (!numVertices) {
        return;
    }          //don't do anything if there are no vertices
    if (vertex_array == 0) {
        RefreshDisplayList();
    }
    (*glBindVertexArray_p)(vertex_array);
}

extern void /*GFXDRVAPI*/ GFXColor4f(const float r, const float g, const float b, const float a);

void GFXVertexList::EndDrawState(GFXBOOL lock) {
    if (changed & HAS_COLOR) {
        GFXColor4f(1, 1, 1, 1);
    }
}

void GFXVertexList::Draw(enum POLYTYPE poly, int numV) {
    INDEX index;
    index.b = NULL;
    Draw(&poly, index, 1, &numV);
}

void GFXVertexList::Draw(enum POLYTYPE poly, int numV, unsigned char *index) {
    char tmpchanged = changed;
    changed = sizeof(unsigned char) | ((~HAS_INDEX) & changed);
    INDEX tmp;
    tmp.b = (index);
    Draw(&poly, tmp, 1, &numV);
    changed = tmpchanged;
}

void GFXVertexList::Draw(enum POLYTYPE poly, int numV, unsigned short *index) {
    char tmpchanged = changed;
    changed = sizeof(unsigned short) | ((~HAS_INDEX) & changed);
    INDEX tmp;
    tmp.s = (index);
    Draw(&poly, tmp, 1, &numV);
    changed = tmpchanged;
}

void GFXVertexList::Draw(enum POLYTYPE poly, int numV, unsigned int *index) {
    char tmpchanged = changed;
    changed = sizeof(unsigned int) | ((~HAS_INDEX) & changed);
    INDEX tmp;
    tmp.i = (index);
    Draw(&poly, tmp, 1, &numV);
    changed = tmpchanged;
}

void GFXVertexList::DrawOnce() {
    LoadDrawState();
    BeginDrawState(GFXFALSE);
    Draw();
    EndDrawState(GFXFALSE);
}

void GFXVertexList::Draw() {
    Draw(mode, index, numlists, offsets);
}

void GFXVertexList::Draw(enum POLYTYPE *mode, const INDEX index, const int numlists, const int *offsets) {
    if (vertex_array == 0 || numlists <= 0) {
        return;
    }
    // Smooth points and lines need alpha blending to show their antialiasing
    bool blendchange = false;
    bool same_mode = true;
    for (int i = 1; i < numlists; ++i) {
        same_mode = same_mode && mode[i] == mode[0];
    }
    if (same_mode) {
        switch (mode[0]) {
            case GFXLINE:
            case GFXLINESTRIP:
            case GFXPOLY:
            case GFXPOINT:
                if (((mode[0] == GFXPOINT) && configuration().graphics.smooth_points)
                        || ((mode[0] != GFXPOINT) && configuration().graphics.smooth_lines)) {
                    BLENDFUNC src, dst;
                    GFXGetBlendMode(src, dst);
                    if ((dst != ZERO) && ((src == ONE) || (src == SRCALPHA))) {
                        GFXPushBlendMode();
                        GFXBlendMode(SRCALPHA, dst);
                        GFXEnable(SMOOTH);
                        blendchange = true;
                    }
                }
                break;
            default:
                break;
        }
    }

    const vsgl::Primitive primitive = vsgl::MapPolyType(static_cast<vsgl::PolyType>(mode[0])).primitive;
    const bool own_lists = mode == this->mode && offsets == this->offsets && numlists == this->numlists
            && memcmp(&index, &this->index, sizeof(INDEX)) == 0;
    (*glBindVertexArray_p)(vertex_array);
    if (own_lists && draw_plan != nullptr) {
        vsgl::PrepareDraw(primitive, (changed & HAS_COLOR) != 0);
        vsgl::DrawCommands(draw_plan->commands, 0);
    } else {
        // A one-off selection of this list's vertices (e.g. Draw(GFXTRI, n, indices)):
        // plan it on the spot and stream whatever indices it needs.
        const int index_size = index.b != nullptr ? IndexSize(changed) : 0;
        const void *indices = index_size != 0 ? static_cast<const void *>(index.b) : nullptr;
        std::vector<vsgl::PolyType> types(numlists);
        size_t total = 0;
        for (int i = 0; i < numlists; ++i) {
            types[i] = static_cast<vsgl::PolyType>(mode[i]);
            total += offsets[i];
        }
        const vsgl::DrawPlan plan = vsgl::BuildDrawPlan(&types[0], offsets, numlists, indices, index_size);
        size_t index_base = 0;
        if (!plan.indices.empty()) {
            index_base = vsgl::StreamIndexData(&plan.indices[0], plan.indices.size() * sizeof(uint32_t));
        } else if (indices != nullptr) {
            index_base = vsgl::StreamIndexData(indices, total * index_size);
        }
        vsgl::PrepareDraw(primitive, (changed & HAS_COLOR) != 0);
        vsgl::DrawCommands(plan.commands, index_base);
        // Put this list's own element buffer back into its vertex array
        vsgl::BindElementBuffer(display_list);
    }

    if (blendchange) {
        GFXPopBlendMode();
        GFXDisable(SMOOTH);
    }
}

GFXVertexList::~GFXVertexList() {
    if (vertex_array != 0) {
        (*glDeleteVertexArrays_p)(1, &vertex_array);
        vertex_array = 0;
    }
    if (vbo_data != 0) {
        GFXForgetBuffer(vbo_data);
        (*glDeleteBuffersARB_p)(1, (GLuint *) &vbo_data);
        vbo_data = 0;
    }
    if (display_list != 0) {
        (*glDeleteBuffersARB_p)(1, (GLuint *) &display_list);
        display_list = 0;
    }
    delete draw_plan;
    draw_plan = nullptr;
    if (vbo_elements != nullptr) {
        (*glDeleteBuffersARB_p)(1, vbo_elements);
        vbo_elements = nullptr;
    }
    if (offsets != nullptr) {
        delete[] offsets;
        offsets = nullptr;
    }
    if (mode != nullptr) {
        delete[] mode;
        mode = nullptr;
    }
    if (changed & HAS_COLOR) {
        if (data.colors != nullptr) {
            free(data.colors);
            data.colors = nullptr;
        }
    } else if (data.vertices != nullptr) {
        free(data.vertices);
        data.vertices = nullptr;
    }
}

union GFXVertexList::VDAT *GFXVertexList::Map(bool read, bool write) {
    // The vertices are always kept in client memory as well (the buffer
    // object is a copy), so mapping just exposes them.
    return &data;
}

void GFXVertexList::UnMap() {
}

///Returns the array of vertices to be mutated
union GFXVertexList::VDAT *GFXVertexList::BeginMutate(int offset) {
    return this->Map(false, true);
}

///Ends mutation and refreshes display list
void GFXVertexList::EndMutate(int newvertexsize) {
    this->UnMap();
    if (!(changed & CHANGE_MUTABLE)) {
        changed |= CHANGE_CHANGE;
    }
    if (newvertexsize) {
        numVertices = newvertexsize;
        //Must keep synchronized - we'll only permit changing vertex count on single-list objects
        if (numlists == 1) {
            *offsets = numVertices;
        }
    }
    if (!vbo_data) {
        RenormalizeNormals();
        RefreshDisplayList();
    } else {
        RefreshDisplayList();
    }
    if (changed & CHANGE_CHANGE) {
        changed &= (~CHANGE_CHANGE);
    }
}

//private, only for inheriters
GFXVertexList::GFXVertexList() :
        numVertices(0),
        mode(0),
        unique_mode(0),
        display_list(0),
        vbo_data(0),
        numlists(0),
        offsets(0),
        changed(0) {
    // ctor
}

POLYTYPE *GFXVertexList::GetPolyType() const {
    return mode;
}

int *GFXVertexList::GetOffsets() const {
    return offsets;
}

int GFXVertexList::GetNumLists() const {
    return numlists;
}

///local helper funcs for procedural Modification
void SetVector(const double factor, Vector *pv) {
    pv->i = pv->i * factor;
    pv->j = pv->j * factor;
    pv->k = pv->k * factor;
}

void GFXSphereVertexList::ProceduralModification() {
    GFXVertex *v = sphere->BeginMutate(0)->vertices;
    const int ROWS = 28;
    int row[ROWS];
    for (int i = 0; i < ROWS; i++) {
        row[i] = numVertices / ROWS * i;
    }

    Vector vert[ROWS];
    int direction[ROWS / 2];

    for (int i = 0; i < numVertices; i++) {
        for (int j = 0; j < ROWS; j++) {
            if (row[j] < numVertices / ROWS * (j + 1)) {
                vert[j] = v[row[j]].GetPosition();
            }
        }

        for (int j = 0; j < ROWS / 2; j++) {
            direction[j] = (int) VegaRandom::Instance().UniformInclusive(0.0, 5.0);
        }
        if (i % 4 == 1) {
            for (int j = 0; j < ROWS; j += 2) {
                if (direction[j / 2] > 2) {
                    SetVector(1.003, &vert[j]);
                }
            }

        }

        if (i % 4 == 0) {
            for (int j = 1; j < ROWS; j += 2) {
                if (direction[(j - 1) / 2] > 2) {
                    SetVector(1.003, &vert[j]);
                }
            }
        }

        for (int j = 0; j < ROWS; j++) {
            if (row[j] < numVertices / ROWS * (j + 1)) {
                v[row[j]].SetVertex(vert[j]);
            }
        }

        for (int j = 0; j < ROWS; j++) {
            row[j]++;
        }
    }

    sphere->EndMutate( /*numVertices*/ );
}
