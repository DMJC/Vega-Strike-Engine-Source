/*
 * gl_ffp_shader.h
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
#ifndef VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_SHADER_H
#define VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_SHADER_H

/**
 * Source of the program that stands in for the fixed-function pipeline
 * whenever no user program is active. It reads the same "vsc_" built-in
 * uniforms as translated data shaders, plus the "vsc_ffp_" controls below.
 *
 * Nothing in here talks to OpenGL.
 */

#include <string>

namespace vsgl {

/// Dummy texture units that unused sampler types point at (see FixedFunctionSamplerUnits)
const int kFfpDummyUnit2D = 13;
const int kFfpDummyUnitRect = 14;
const int kFfpDummyUnitCube = 15;

/// Values of vsc_ffp_tex_target
enum FfpTexTarget {
    kFfpTexNone = 0,
    kFfpTex2D = 1,
    kFfpTexCube = 2,
    kFfpTexRect = 3,
};

/// Values of vsc_ffp_fog_mode
enum FfpFogMode {
    kFfpFogOff = 0,
    kFfpFogLinear = 1,
    kFfpFogExp = 2,
    kFfpFogExp2 = 3,
};

std::string FixedFunctionVertexSource(int glsl_version, bool flat_shading);
std::string FixedFunctionFragmentSource(int glsl_version, bool flat_shading);

/**
 * GL refuses to draw when samplers of different types refer to the same
 * texture unit, even if the shader never samples them. Each unit therefore
 * points only the sampler matching its enabled target at itself and the
 * others at a per-type dummy unit.
 */
struct FfpSamplerUnits {
    int tex2d;
    int texcube;
    int texrect;
};

FfpSamplerUnits FixedFunctionSamplerUnits(int unit, int ffp_tex_target);

} // namespace vsgl

#endif //VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_SHADER_H
