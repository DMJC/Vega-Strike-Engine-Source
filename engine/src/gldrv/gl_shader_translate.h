/*
 * gl_shader_translate.h
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
#ifndef VEGA_STRIKE_ENGINE_GLDRV_GL_SHADER_TRANSLATE_H
#define VEGA_STRIKE_ENGINE_GLDRV_GL_SHADER_TRANSLATE_H

/**
 * Translates compatibility-profile GLSL (1.10 - 1.30, as found in the
 * data/programs directory) into core-profile GLSL 4.10 / 4.60.
 *
 * The removed built-ins are renamed to "vsc_" identifiers that the
 * translator declares in a prelude:
 *  - built-in attributes (gl_Vertex, gl_MultiTexCoordN...) become vertex
 *    inputs at fixed locations (see AttribLocation),
 *  - built-in uniforms (gl_ModelViewMatrix, gl_LightSource[]...) become
 *    plain uniforms which the driver fills from the emulated fixed-function
 *    state,
 *  - built-in varyings (gl_TexCoord[], gl_FrontColor...) become matching
 *    out/in pairs, sized from what the two stages of a program use,
 *  - gl_FragColor becomes an output at location 0,
 *  - attribute/varying become in/out and the old texture functions map onto
 *    their core equivalents.
 *
 * Nothing in here talks to OpenGL.
 */

#include <set>
#include <string>

namespace vsgl {

enum class ShaderStage {
    Vertex,
    Fragment,
};

/// Fixed vertex attribute locations shared by shaders and vertex arrays
enum AttribLocation {
    kAttribVertex = 0,
    kAttribNormal = 1,
    kAttribColor = 2,
    kAttribSecondaryColor = 3,
    kAttribFogCoord = 4,
    kAttribMultiTexCoord0 = 8,  ///< 8..15 for gl_MultiTexCoord0..7
    kAttribCount = 16,
};

/// Built-in varyings a vertex/fragment pair has to agree on
struct VaryingSet {
    bool front_color = false;
    bool back_color = false;
    bool front_secondary_color = false;
    bool back_secondary_color = false;
    bool fog_frag_coord = false;
    /// Size of the vsc_TexCoord array, 0 when unused
    int tex_coords = 0;

    void Merge(const VaryingSet &o);
};

struct ShaderTranslation {
    std::string source;
    /// The #version found in the original source (110 when absent)
    int original_version = 110;
    /// "vsc_" identifiers referenced after translation, including prelude dependencies
    std::set<std::string> builtins;
};

struct ProgramTranslation {
    ShaderTranslation vertex;
    ShaderTranslation fragment;
    VaryingSet varyings;
};

/**
 * Translates a vertex/fragment pair so that their built-in varyings match.
 *
 * @param glsl_version  410 or 460
 */
ProgramTranslation TranslateLegacyProgram(const std::string &vertex_source,
        const std::string &fragment_source, int glsl_version);

/**
 * Builds the declarations for every "vsc_" identifier in `used` (plus
 * dependencies), for one stage. Exposed so the built-in fixed-function
 * shader can share the exact declarations the data shaders see.
 */
std::string BuildPrelude(ShaderStage stage, const std::set<std::string> &used, const VaryingSet &varyings);

/// Adds the identifiers the given ones depend on (e.g. vsc_LightSource needs vsc_MaxLights)
void AddPreludeDependencies(std::set<std::string> &used);

/// All "vsc_" identifiers referenced in a translated or hand-written source
std::set<std::string> CollectBuiltins(const std::string &source);

/// The varyings used by a stage's (already renamed) source
VaryingSet CollectVaryings(const std::string &source);

/// The "#version" of a legacy source, or 110 if it has none
int DetectVersion(const std::string &source);

} // namespace vsgl

#endif //VEGA_STRIKE_ENGINE_GLDRV_GL_SHADER_TRANSLATE_H
