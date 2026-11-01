/*
 * gl_profile.cpp
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

#include "gl_profile.h"

#include <cctype>
#include <cstdlib>

namespace vsgl {

std::string ContextProfile::GLSLVersionDirective() const {
    return "#version " + std::to_string(GLSLVersion()) + " core";
}

int ContextProfile::GLSLVersion() const {
    return major * 100 + minor * 10;
}

bool ContextProfile::HasDebugOutput() const {
    return major > 4 || (major == 4 && minor >= 3);
}

bool ContextProfile::HasBufferStorage() const {
    return major > 4 || (major == 4 && minor >= 4);
}

bool ContextProfile::SupportsWideLines() const {
    return !forward_compatible;
}

const char *ContextProfile::Name() const {
    switch (kind) {
        case ProfileKind::GL46Core:
            return "OpenGL 4.6 core";
        case ProfileKind::GL41Core:
        default:
            return "OpenGL 4.1 core";
    }
}

ContextProfile MakeProfile(ProfileKind kind, bool apple) {
    ContextProfile profile{};
    profile.kind = kind;
    switch (kind) {
        case ProfileKind::GL46Core:
            profile.major = 4;
            profile.minor = 6;
            break;
        case ProfileKind::GL41Core:
        default:
            profile.major = 4;
            profile.minor = 1;
            break;
    }
    // macOS only hands out core contexts when they are forward-compatible.
    // Elsewhere we avoid the flag: it would forbid wide lines for no benefit.
    profile.forward_compatible = apple;
    return profile;
}

std::vector<ContextProfile> ProfileCandidates(bool apple, const std::string &requested) {
    std::vector<ContextProfile> candidates;
    if (apple) {
        // There is no 4.6 on macOS, whatever the configuration asks for.
        candidates.push_back(MakeProfile(ProfileKind::GL41Core, true));
        return candidates;
    }
    if (requested == "4.1") {
        candidates.push_back(MakeProfile(ProfileKind::GL41Core, false));
        return candidates;
    }
    candidates.push_back(MakeProfile(ProfileKind::GL46Core, false));
    if (requested != "4.6") {
        candidates.push_back(MakeProfile(ProfileKind::GL41Core, false));
    }
    return candidates;
}

bool VersionSatisfies(const ContextProfile &profile, int major, int minor) {
    return major > profile.major || (major == profile.major && minor >= profile.minor);
}

bool ParseGLVersion(const char *version_string, int &major, int &minor) {
    if (version_string == nullptr) {
        return false;
    }
    // GL_VERSION is "<major>.<minor>[.<release>] <vendor info>", but GLES
    // prefixes it ("OpenGL ES 3.2 ..."), so skip to the first digit.
    const char *p = version_string;
    while (*p != '\0' && !std::isdigit(static_cast<unsigned char>(*p))) {
        ++p;
    }
    if (*p == '\0') {
        return false;
    }
    char *end = nullptr;
    const long maj = std::strtol(p, &end, 10);
    if (end == p || *end != '.') {
        return false;
    }
    const char *minor_start = end + 1;
    const long min = std::strtol(minor_start, &end, 10);
    if (end == minor_start) {
        return false;
    }
    major = static_cast<int>(maj);
    minor = static_cast<int>(min);
    return true;
}

} // namespace vsgl
