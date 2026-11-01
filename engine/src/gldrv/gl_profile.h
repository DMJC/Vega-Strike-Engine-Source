/*
 * gl_profile.h
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
#ifndef VEGA_STRIKE_ENGINE_GLDRV_GL_PROFILE_H
#define VEGA_STRIKE_ENGINE_GLDRV_GL_PROFILE_H

#include <string>
#include <vector>

namespace vsgl {

/**
 * The OpenGL core profiles the renderer knows how to drive.
 *
 * GL 4.6 core is the primary target. GL 4.1 core is the ceiling on macOS
 * (which also requires a forward-compatible context) and serves as the
 * fallback on any other platform whose driver cannot create a 4.6 context.
 * Everything the renderer does must work on 4.1; 4.6 only adds optional
 * extras (debug output, persistently mapped streaming buffers).
 */
enum class ProfileKind {
    GL46Core,
    GL41Core,
};

struct ContextProfile {
    ProfileKind kind;
    int major;
    int minor;
    bool forward_compatible;

    /// "#version 460 core" or "#version 410 core"
    std::string GLSLVersionDirective() const;
    /// 460 or 410
    int GLSLVersion() const;
    /// KHR_debug / glDebugMessageCallback is core from 4.3
    bool HasDebugOutput() const;
    /// glBufferStorage (persistent mapping) is core from 4.4
    bool HasBufferStorage() const;
    /// Wide lines are an error in forward-compatible contexts
    bool SupportsWideLines() const;
    const char *Name() const;
};

ContextProfile MakeProfile(ProfileKind kind, bool apple);

/**
 * Returns the profiles to try, most capable first.
 *
 * @param apple  true when building for macOS, which cannot go past 4.1
 * @param requested  the graphics.gl_profile config value: "auto" (or empty),
 *                   "4.6" or "4.1". Unknown values are treated as "auto".
 */
std::vector<ContextProfile> ProfileCandidates(bool apple, const std::string &requested);

/// True if a driver-reported "major.minor" version satisfies the profile.
bool VersionSatisfies(const ContextProfile &profile, int major, int minor);

/// Parses the leading "major.minor" of a GL_VERSION string. Returns false on garbage.
bool ParseGLVersion(const char *version_string, int &major, int &minor);

} // namespace vsgl

#endif //VEGA_STRIKE_ENGINE_GLDRV_GL_PROFILE_H
