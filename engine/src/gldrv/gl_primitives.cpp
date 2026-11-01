/*
 * gl_primitives.cpp
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

#include "gl_primitives.h"

#include <cstring>

namespace vsgl {

namespace {

// The GLenum values, spelled out so this file needs no GL headers.
const unsigned int kGL_POINTS = 0x0000;
const unsigned int kGL_LINES = 0x0001;
const unsigned int kGL_LINE_STRIP = 0x0003;
const unsigned int kGL_TRIANGLES = 0x0004;
const unsigned int kGL_TRIANGLE_STRIP = 0x0005;
const unsigned int kGL_TRIANGLE_FAN = 0x0006;

bool Mergeable(Primitive p) {
    return p == Primitive::Triangles || p == Primitive::Lines || p == Primitive::Points;
}

void AddCommand(DrawPlan &plan, const DrawCommand &cmd) {
    if (cmd.count == 0) {
        return;
    }
    if (!plan.commands.empty()) {
        DrawCommand &last = plan.commands.back();
        if (Mergeable(cmd.primitive) && last.primitive == cmd.primitive && last.indexed == cmd.indexed
                && last.index_size == cmd.index_size && last.first + last.count == cmd.first) {
            last.count += cmd.count;
            return;
        }
    }
    plan.commands.push_back(cmd);
}

} // namespace

PrimitiveMapping MapPolyType(PolyType type) {
    switch (type) {
        case PolyType::Tri:
            return {Primitive::Triangles, false};
        case PolyType::Quad:
            return {Primitive::Triangles, true};
        case PolyType::Line:
            return {Primitive::Lines, false};
        case PolyType::TriStrip:
            return {Primitive::TriangleStrip, false};
        case PolyType::QuadStrip:
            return {Primitive::TriangleStrip, false};
        case PolyType::TriFan:
            return {Primitive::TriangleFan, false};
        case PolyType::LineStrip:
            return {Primitive::LineStrip, false};
        case PolyType::Poly:
            return {Primitive::TriangleFan, false};
        case PolyType::Point:
            return {Primitive::Points, false};
    }
    return {Primitive::Triangles, false};
}

unsigned int PrimitiveToGL(Primitive primitive) {
    switch (primitive) {
        case Primitive::Points:
            return kGL_POINTS;
        case Primitive::Lines:
            return kGL_LINES;
        case Primitive::LineStrip:
            return kGL_LINE_STRIP;
        case Primitive::TriangleStrip:
            return kGL_TRIANGLE_STRIP;
        case Primitive::TriangleFan:
            return kGL_TRIANGLE_FAN;
        case Primitive::Triangles:
        default:
            return kGL_TRIANGLES;
    }
}

uint32_t UsableVertexCount(PolyType type, uint32_t count) {
    switch (type) {
        case PolyType::Tri:
            return count - count % 3;
        case PolyType::Quad:
            return count - count % 4;
        case PolyType::Line:
            return count - count % 2;
        case PolyType::QuadStrip:
            // A quad strip needs pairs; an odd trailing vertex is ignored.
            return count < 4 ? 0 : count - count % 2;
        case PolyType::TriStrip:
        case PolyType::TriFan:
        case PolyType::Poly:
            return count < 3 ? 0 : count;
        case PolyType::LineStrip:
            return count < 2 ? 0 : count;
        case PolyType::Point:
        default:
            return count;
    }
}

void AppendQuadIndices(uint32_t first, uint32_t count, std::vector<uint32_t> &out) {
    const uint32_t quads = count / 4;
    out.reserve(out.size() + quads * 6);
    for (uint32_t q = 0; q < quads; ++q) {
        const uint32_t v = first + q * 4;
        out.push_back(v);
        out.push_back(v + 1);
        out.push_back(v + 2);
        out.push_back(v);
        out.push_back(v + 2);
        out.push_back(v + 3);
    }
}

uint32_t ReadIndex(const void *indices, int index_size, size_t i) {
    switch (index_size) {
        case 1:
            return static_cast<const uint8_t *>(indices)[i];
        case 2:
            return static_cast<const uint16_t *>(indices)[i];
        case 4:
        default:
            return static_cast<const uint32_t *>(indices)[i];
    }
}

void AppendQuadIndices(const void *indices, int index_size, uint32_t first, uint32_t count,
        std::vector<uint32_t> &out) {
    const uint32_t quads = count / 4;
    out.reserve(out.size() + quads * 6);
    for (uint32_t q = 0; q < quads; ++q) {
        const size_t base = first + q * 4;
        const uint32_t a = ReadIndex(indices, index_size, base);
        const uint32_t b = ReadIndex(indices, index_size, base + 1);
        const uint32_t c = ReadIndex(indices, index_size, base + 2);
        const uint32_t d = ReadIndex(indices, index_size, base + 3);
        out.push_back(a);
        out.push_back(b);
        out.push_back(c);
        out.push_back(a);
        out.push_back(c);
        out.push_back(d);
    }
}

DrawPlan BuildDrawPlan(const PolyType *types, const int *counts, int num_lists,
        const void *indices, int index_size) {
    DrawPlan plan;
    bool any_quads = false;
    for (int i = 0; i < num_lists; ++i) {
        if (MapPolyType(types[i]).expand_quads) {
            any_quads = true;
            break;
        }
    }

    uint32_t source_offset = 0;
    if (indices != nullptr && any_quads) {
        // Rebuild one 32-bit index buffer holding every list, so that the
        // expanded quads and the untouched lists share a single buffer.
        plan.rebuilt_indices = true;
        for (int i = 0; i < num_lists; ++i) {
            const uint32_t count = counts[i] > 0 ? static_cast<uint32_t>(counts[i]) : 0;
            const PrimitiveMapping mapping = MapPolyType(types[i]);
            const uint32_t usable = UsableVertexCount(types[i], count);
            const uint32_t first = static_cast<uint32_t>(plan.indices.size());
            if (mapping.expand_quads) {
                AppendQuadIndices(indices, index_size, source_offset, usable, plan.indices);
            } else {
                for (uint32_t k = 0; k < usable; ++k) {
                    plan.indices.push_back(ReadIndex(indices, index_size, source_offset + k));
                }
            }
            const uint32_t emitted = static_cast<uint32_t>(plan.indices.size()) - first;
            AddCommand(plan, DrawCommand{mapping.primitive, true, 4, first, emitted});
            source_offset += count;
        }
        return plan;
    }

    for (int i = 0; i < num_lists; ++i) {
        const uint32_t count = counts[i] > 0 ? static_cast<uint32_t>(counts[i]) : 0;
        const PrimitiveMapping mapping = MapPolyType(types[i]);
        const uint32_t usable = UsableVertexCount(types[i], count);
        if (indices != nullptr) {
            AddCommand(plan, DrawCommand{mapping.primitive, true, index_size, source_offset, usable});
        } else if (mapping.expand_quads) {
            const uint32_t first = static_cast<uint32_t>(plan.indices.size());
            AppendQuadIndices(source_offset, usable, plan.indices);
            AddCommand(plan, DrawCommand{mapping.primitive, true, 4, first, QuadIndexCount(usable)});
        } else {
            AddCommand(plan, DrawCommand{mapping.primitive, false, 0, source_offset, usable});
        }
        source_offset += count;
    }
    return plan;
}

InterleavedLayout ComputeInterleavedLayout(int vsize, int csize, int tsize0, int tsize1) {
    InterleavedLayout layout{};
    const int f = static_cast<int>(sizeof(float));
    layout.position_offset = 0;
    layout.position_size = vsize;
    layout.color_offset = vsize * f;
    layout.color_size = csize;
    layout.tex0_offset = (vsize + csize) * f;
    layout.tex0_size = tsize0;
    layout.tex1_offset = (vsize + csize + tsize0) * f;
    layout.tex1_size = tsize1;
    layout.stride_bytes = (vsize + csize + tsize0 + tsize1) * f;
    return layout;
}

} // namespace vsgl
