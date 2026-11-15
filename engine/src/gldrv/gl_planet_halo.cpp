/*
 * gl_planet_halo.cpp
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

/**
 * Atmospheric halos round planets.
 *
 * Each halo is a camera-facing quad covering the silhouette of the planet's
 * atmosphere shell. Its fragment shader casts the view ray through the
 * shell, sums the air density along it (falling off exponentially with
 * altitude) weighted by how sunlit each point is, and turns that into a
 * premultiplied-alpha colour: a bright rim at the limb, a light haze over
 * the day side of the disc, little on the night side. It writes the depth
 * of the point where the ray enters the atmosphere, so anything in front of
 * the planet hides the halo correctly.
 *
 * This is a native core-profile program with its own vertex array, not a
 * fixed-function or translated data-set shader.
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "gl_globals.h"
#include "gl_renderer.h"
#include "gl_ffp_compat.h"
#include "gl_ffp_state.h"
#include "src/gfxlib.h"
#include "src/vs_logging.h"

namespace {

const char *const kVertexBody = R"GLSL(
layout(location = 0) in vec3 a_position;   // relative to the camera, metres

uniform mat4 u_projection;
uniform float u_scale;

out vec3 v_position;

void main() {
    v_position = a_position;
    gl_Position = u_projection * vec4(a_position * u_scale, 1.0);
}
)GLSL";

const char *const kFragmentBody = R"GLSL(
in vec3 v_position;

uniform mat4 u_projection;
uniform float u_scale;
uniform vec3 u_center;       // planet centre relative to the camera, metres
uniform float u_radius;      // planet radius
uniform float u_thickness;   // atmosphere thickness above the surface
uniform float u_scale_height;
uniform vec3 u_sun;          // unit direction from the planet towards its light
uniform vec3 u_color;
uniform float u_density;     // optical depth multiplier
uniform float u_night;       // brightness of the unlit side, 0..1

out vec4 frag_color;

const int kSteps = 16;

void main() {
    vec3 dir = normalize(v_position);
    float tc = dot(u_center, dir);
    float b2 = dot(u_center, u_center) - tc * tc;     // squared miss distance of the ray
    float outer = u_radius + u_thickness;
    if (b2 >= outer * outer) {
        discard;
    }
    float half_chord = sqrt(outer * outer - b2);
    float t0 = max(tc - half_chord, 0.0);              // enters the atmosphere
    float t1 = tc + half_chord;                         // leaves it...
    float r2 = u_radius * u_radius;
    if (b2 < r2) {
        t1 = tc - sqrt(r2 - b2);                        // ...or hits the surface
    }
    if (t1 <= t0) {
        discard;
    }

    float dt = (t1 - t0) / float(kSteps);
    float optical = 0.0;
    float lit = 0.0;
    for (int i = 0; i < kSteps; ++i) {
        vec3 p = dir * (t0 + (float(i) + 0.5) * dt) - u_center;
        float r = length(p);
        float altitude = max(r - u_radius, 0.0);
        // Thins with height, and fades out over the top half of the shell so it has no hard edge
        float density = exp(-altitude / u_scale_height)
                * (1.0 - smoothstep(0.5, 1.0, altitude / u_thickness));
        float sunlight = mix(u_night, 1.0, smoothstep(-0.25, 0.3, dot(p / r, u_sun)));
        optical += density * dt;
        lit += density * sunlight * dt;
    }
    float light = lit / max(optical, 1e-6);
    float alpha = (1.0 - exp(-optical * u_density / u_scale_height)) * light;
    if (alpha <= 0.002) {
        discard;
    }
    frag_color = vec4(u_color * alpha, alpha);

    vec4 clip = u_projection * vec4(dir * t0 * u_scale, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
}
)GLSL";

struct HaloProgram {
    bool tried = false;
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint projection = -1;
    GLint scale = -1;
    GLint center = -1;
    GLint radius = -1;
    GLint thickness = -1;
    GLint scale_height = -1;
    GLint sun = -1;
    GLint color = -1;
    GLint density = -1;
    GLint night = -1;
};

HaloProgram g_halo;

bool EnsureProgram() {
    if (g_halo.tried) {
        return g_halo.program != 0;
    }
    g_halo.tried = true;
    const std::string version = vsgl::CurrentProfile().GLSLVersionDirective() + "\n";
    g_halo.program = vsgl::BuildProgram(version + kVertexBody, version + kFragmentBody, "planet halo");
    if (g_halo.program == 0) {
        VS_LOG(error, "Planet halos are disabled: their shader did not build");
        return false;
    }
    const GLuint p = g_halo.program;
    g_halo.projection = (*glGetUniformLocation_p)(p, "u_projection");
    g_halo.scale = (*glGetUniformLocation_p)(p, "u_scale");
    g_halo.center = (*glGetUniformLocation_p)(p, "u_center");
    g_halo.radius = (*glGetUniformLocation_p)(p, "u_radius");
    g_halo.thickness = (*glGetUniformLocation_p)(p, "u_thickness");
    g_halo.scale_height = (*glGetUniformLocation_p)(p, "u_scale_height");
    g_halo.sun = (*glGetUniformLocation_p)(p, "u_sun");
    g_halo.color = (*glGetUniformLocation_p)(p, "u_color");
    g_halo.density = (*glGetUniformLocation_p)(p, "u_density");
    g_halo.night = (*glGetUniformLocation_p)(p, "u_night");

    (*glGenVertexArrays_p)(1, &g_halo.vao);
    (*glGenBuffersARB_p)(1, &g_halo.vbo);
    (*glBindVertexArray_p)(g_halo.vao);
    GFXBindBuffer(g_halo.vbo);
    (*glEnableVertexAttribArray_p)(0);
    (*glVertexAttribPointer_p)(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    vsgl::BindStreamVertexArray();
    return true;
}

/**
 * The quad, as two triangles, centred on the planet and facing the camera,
 * big enough to cover the silhouette of a sphere of radius `outer`.
 * Returns false when the camera is inside (or almost inside) that sphere.
 */
bool BuildQuad(const GFXPlanetHalo &h, std::vector<float> &out) {
    const double outer = h.radius + h.thickness;
    const double dist = h.center.Magnitude();
    if (dist <= outer * 1.001) {
        return false;
    }
    // Half-size at the centre's depth that covers the silhouette cone, plus a margin
    const double half = outer * dist / std::sqrt(dist * dist - outer * outer) * 1.02;
    Vector forward = h.center;
    forward.Normalize();
    Vector helper = std::fabs(forward.j) < 0.9 ? Vector(0, 1, 0) : Vector(1, 0, 0);
    Vector right = forward.Cross(helper);
    right.Normalize();
    Vector up = right.Cross(forward);
    const Vector c = h.center;
    const Vector r = right * static_cast<float>(half);
    const Vector u = up * static_cast<float>(half);
    const Vector corners[6] = {c - r - u, c + r - u, c + r + u, c - r - u, c + r + u, c - r + u};
    for (const Vector &v : corners) {
        out.push_back(v.i);
        out.push_back(v.j);
        out.push_back(v.k);
    }
    return true;
}

} // namespace

void GFXDrawPlanetHalos(const std::vector<GFXPlanetHalo> &halos) {
    if (halos.empty() || !EnsureProgram()) {
        return;
    }
    std::vector<float> vertices;
    std::vector<const GFXPlanetHalo *> drawn;
    vertices.reserve(halos.size() * 18);
    for (const GFXPlanetHalo &h : halos) {
        if (BuildQuad(h, vertices)) {
            drawn.push_back(&h);
        }
    }
    if (drawn.empty()) {
        return;
    }

    // Leave culling and depth writes as they were found
    const bool was_culling = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    GLboolean was_writing_depth = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &was_writing_depth);

    GFXPushBlendMode();
    GFXBlendMode(ONE, INVSRCALPHA);   // premultiplied alpha
    GFXEnable(DEPTHTEST);
    GFXDisable(DEPTHWRITE);
    GFXDisable(CULLFACE);

    vsgl::BindProgram(g_halo.program);
    (*glBindVertexArray_p)(g_halo.vao);
    GFXBindBuffer(g_halo.vbo);
    (*glBufferDataARB_p)(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);

    (*glUniformMatrix4fv_p)(g_halo.projection, 1, GL_FALSE, vsglState().Projection().m);
    (*glUniform1f_p)(g_halo.scale, static_cast<float>(GFX_SCALE));
    for (size_t i = 0; i < drawn.size(); ++i) {
        const GFXPlanetHalo &h = *drawn[i];
        (*glUniform3f_p)(g_halo.center, h.center.i, h.center.j, h.center.k);
        (*glUniform1f_p)(g_halo.radius, h.radius);
        (*glUniform1f_p)(g_halo.thickness, h.thickness);
        (*glUniform1f_p)(g_halo.scale_height, std::max(h.thickness * h.falloff, 1e-3f));
        (*glUniform3f_p)(g_halo.sun, h.sun.i, h.sun.j, h.sun.k);
        (*glUniform3f_p)(g_halo.color, h.color.r, h.color.g, h.color.b);
        (*glUniform1f_p)(g_halo.density, h.density);
        (*glUniform1f_p)(g_halo.night, h.night);
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(i * 6), 6);
    }

    vsgl::BindStreamVertexArray();
    if (was_culling) {
        GFXEnable(CULLFACE);
    }
    if (was_writing_depth) {
        GFXEnable(DEPTHWRITE);
    }
    GFXPopBlendMode();
}
