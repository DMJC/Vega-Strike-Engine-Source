/*
 * gl_ffp_shader.cpp
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

#include "gl_ffp_shader.h"
#include "gl_shader_translate.h"

namespace vsgl {

namespace {

// Follows the lighting, texgen and texture environment equations of the
// OpenGL 2.1 specification (sections 2.14, 2.11.4 and 3.8.13).
const char *const kVertexBody = R"GLSL(
uniform int vsc_ffp_lighting;
uniform int vsc_ffp_light_enabled[vsc_MaxLights];
uniform int vsc_ffp_separate_specular;
uniform int vsc_ffp_local_viewer;
// -1 off; otherwise 0 emission, 1 ambient, 2 diffuse, 3 specular, 4 ambient and diffuse
uniform int vsc_ffp_color_material;
// per unit and coordinate: 0 off, else TexGenMode + 1
uniform ivec4 vsc_ffp_texgen[4];
// 0 |eye z|, 1 -eye z, 2 eye distance
uniform int vsc_ffp_fog_distance;

FFP_INTERP out vec4 vsc_FrontColor;
FFP_INTERP out vec4 vsc_FrontSecondaryColor;
out float vsc_FogFragCoord;
out vec4 vsc_TexCoord[4];

float ffp_texgen(int mode, vec4 eye_plane, vec4 object_plane, vec4 eye, vec4 obj,
                 vec3 sphere, vec3 reflected, vec3 normal, int component, float original) {
    if (mode == 1) {
        return dot(eye_plane, eye);
    } else if (mode == 2) {
        return dot(object_plane, obj);
    } else if (mode == 3) {
        return component < 2 ? sphere[component] : original;
    } else if (mode == 4) {
        return component < 3 ? reflected[component] : original;
    } else if (mode == 5) {
        return component < 3 ? normal[component] : original;
    }
    return original;
}

vec4 ffp_texcoord(int unit, vec4 original, vec4 eye, vec3 normal) {
    ivec4 gen = vsc_ffp_texgen[unit];
    vec4 tc = original;
    if (gen != ivec4(0)) {
        vec3 u = normalize(eye.xyz);
        vec3 r = reflect(u, normal);
        float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
        vec3 sphere = vec3(r.x / m + 0.5, r.y / m + 0.5, 0.0);
        tc.x = ffp_texgen(gen.x, vsc_EyePlaneS[unit], vsc_ObjectPlaneS[unit], eye, vsc_Vertex, sphere, r, normal, 0, tc.x);
        tc.y = ffp_texgen(gen.y, vsc_EyePlaneT[unit], vsc_ObjectPlaneT[unit], eye, vsc_Vertex, sphere, r, normal, 1, tc.y);
        tc.z = ffp_texgen(gen.z, vsc_EyePlaneR[unit], vsc_ObjectPlaneR[unit], eye, vsc_Vertex, sphere, r, normal, 2, tc.z);
        tc.w = ffp_texgen(gen.w, vsc_EyePlaneQ[unit], vsc_ObjectPlaneQ[unit], eye, vsc_Vertex, sphere, r, normal, 3, tc.w);
    }
    return vsc_TextureMatrix[unit] * tc;
}

void main() {
    vec4 eye = vsc_ModelViewMatrix * vsc_Vertex;
    gl_Position = vsc_ModelViewProjectionMatrix * vsc_Vertex;
    vec3 normal = vsc_NormalMatrix * vsc_Normal;
    float nlen = length(normal);
    normal = nlen > 0.0 ? normal / nlen : vec3(0.0, 0.0, 1.0);

    if (vsc_ffp_lighting != 0) {
        vec4 mat_emission = vsc_FrontMaterial.emission;
        vec4 mat_ambient = vsc_FrontMaterial.ambient;
        vec4 mat_diffuse = vsc_FrontMaterial.diffuse;
        vec4 mat_specular = vsc_FrontMaterial.specular;
        if (vsc_ffp_color_material == 0) {
            mat_emission = vsc_Color;
        } else if (vsc_ffp_color_material == 1) {
            mat_ambient = vsc_Color;
        } else if (vsc_ffp_color_material == 2) {
            mat_diffuse = vsc_Color;
        } else if (vsc_ffp_color_material == 3) {
            mat_specular = vsc_Color;
        } else if (vsc_ffp_color_material == 4) {
            mat_ambient = vsc_Color;
            mat_diffuse = vsc_Color;
        }
        float shininess = vsc_FrontMaterial.shininess;
        vec3 view_dir = vsc_ffp_local_viewer != 0 ? normalize(-eye.xyz) : vec3(0.0, 0.0, 1.0);

        vec3 ambient = vec3(0.0);
        vec3 diffuse = vec3(0.0);
        vec3 specular = vec3(0.0);
        for (int i = 0; i < vsc_MaxLights; ++i) {
            if (vsc_ffp_light_enabled[i] == 0) {
                continue;
            }
            vec4 lp = vsc_LightSource[i].position;
            vec3 L;
            float attenuation = 1.0;
            if (lp.w != 0.0) {
                vec3 d = lp.xyz / lp.w - eye.xyz;
                float dist = length(d);
                L = dist > 0.0 ? d / dist : vec3(0.0, 0.0, 1.0);
                attenuation = 1.0 / max(vsc_LightSource[i].constantAttenuation
                        + vsc_LightSource[i].linearAttenuation * dist
                        + vsc_LightSource[i].quadraticAttenuation * dist * dist, 1e-6);
            } else {
                L = normalize(lp.xyz);
            }
            if (vsc_LightSource[i].spotCutoff != 180.0) {
                float spot = dot(-L, normalize(vsc_LightSource[i].spotDirection));
                attenuation *= spot < vsc_LightSource[i].spotCosCutoff
                        ? 0.0 : pow(max(spot, 0.0), vsc_LightSource[i].spotExponent);
            }
            float n_dot_l = max(dot(normal, L), 0.0);
            ambient += attenuation * vsc_LightSource[i].ambient.rgb * mat_ambient.rgb;
            diffuse += attenuation * n_dot_l * vsc_LightSource[i].diffuse.rgb * mat_diffuse.rgb;
            if (n_dot_l > 0.0) {
                vec3 h = normalize(L + view_dir);
                float n_dot_h = max(dot(normal, h), 0.0);
                float f = shininess > 0.0 ? pow(n_dot_h, shininess) : 1.0;
                specular += attenuation * f * vsc_LightSource[i].specular.rgb * mat_specular.rgb;
            }
        }
        vec3 primary = mat_emission.rgb + mat_ambient.rgb * vsc_LightModel.ambient.rgb + ambient + diffuse;
        if (vsc_ffp_separate_specular != 0) {
            vsc_FrontColor = clamp(vec4(primary, mat_diffuse.a), 0.0, 1.0);
            vsc_FrontSecondaryColor = vec4(clamp(specular, 0.0, 1.0), 0.0);
        } else {
            vsc_FrontColor = clamp(vec4(primary + specular, mat_diffuse.a), 0.0, 1.0);
            vsc_FrontSecondaryColor = vec4(0.0);
        }
    } else {
        vsc_FrontColor = clamp(vsc_Color, 0.0, 1.0);
        vsc_FrontSecondaryColor = vec4(0.0);
    }

    vsc_TexCoord[0] = ffp_texcoord(0, vsc_MultiTexCoord0, eye, normal);
    vsc_TexCoord[1] = ffp_texcoord(1, vsc_MultiTexCoord1, eye, normal);
    vsc_TexCoord[2] = ffp_texcoord(2, vsc_MultiTexCoord2, eye, normal);
    vsc_TexCoord[3] = ffp_texcoord(3, vsc_MultiTexCoord3, eye, normal);

    if (vsc_ffp_fog_distance == 2) {
        vsc_FogFragCoord = length(eye.xyz);
    } else if (vsc_ffp_fog_distance == 1) {
        vsc_FogFragCoord = -eye.z;
    } else {
        vsc_FogFragCoord = abs(eye.z);
    }
}
)GLSL";

const char *const kFragmentBody = R"GLSL(
// per unit: FfpTexTarget
uniform int vsc_ffp_tex_target[4];
// per unit: TexEnvMode
uniform int vsc_ffp_tex_env[4];
// per unit: whether the bound texture has an alpha channel (affects GL_REPLACE)
uniform int vsc_ffp_tex_has_alpha[4];
uniform sampler2D vsc_ffp_tex2d[4];
uniform samplerCube vsc_ffp_texcube[4];
uniform sampler2DRect vsc_ffp_texrect[4];
// FfpFogMode
uniform int vsc_ffp_fog_mode;
// CompareFunc; Always when the alpha test is off
uniform int vsc_ffp_alpha_func;
uniform float vsc_ffp_alpha_ref;
uniform int vsc_ffp_point_smooth;

FFP_INTERP in vec4 vsc_FrontColor;
FFP_INTERP in vec4 vsc_FrontSecondaryColor;
in float vsc_FogFragCoord;
in vec4 vsc_TexCoord[4];

vec4 ffp_combine(int mode, vec4 prev, vec4 tex, vec4 env, int has_alpha) {
    if (mode == 0) {        // modulate
        return prev * tex;
    } else if (mode == 1) { // replace
        return vec4(tex.rgb, has_alpha != 0 ? tex.a : prev.a);
    } else if (mode == 2) { // add
        return vec4(prev.rgb + tex.rgb, prev.a * tex.a);
    } else if (mode == 3) { // decal
        return vec4(mix(prev.rgb, tex.rgb, tex.a), prev.a);
    } else if (mode == 4) { // blend
        return vec4(mix(prev.rgb, env.rgb, tex.rgb), prev.a * tex.a);
    } else if (mode == 5) { // interpolate with the constant alpha
        return mix(tex, prev, env.a);
    } else if (mode == 6) { // composite
        return vec4(mix(prev.rgb, tex.rgb, tex.a), env.a);
    } else if (mode == 7) { // add signed
        return vec4(prev.rgb + tex.rgb - 0.5, prev.a);
    } else if (mode == 8) { // detail
        return 2.0 * prev * tex;
    }
    return prev;
}

// Sampler arrays may only be indexed by constants in GLSL 4.10, so the
// units are applied one by one with literal indices.
vec4 ffp_apply_unit(vec4 color, int target, int env_mode, int has_alpha, vec4 env_color,
                    sampler2D tex2d, samplerCube texcube, sampler2DRect texrect, vec4 tc) {
    if (target == 0) {
        return color;
    }
    vec4 tex;
    if (target == 2) {
        tex = texture(texcube, tc.xyz);
    } else if (target == 3) {
        tex = textureProj(texrect, tc);
    } else {
        tex = textureProj(tex2d, tc);
    }
    return clamp(ffp_combine(env_mode, color, tex, env_color, has_alpha), 0.0, 1.0);
}

bool ffp_compare(int func, float a, float ref) {
    if (func == 0) return false;
    if (func == 1) return a < ref;
    if (func == 2) return a == ref;
    if (func == 3) return a <= ref;
    if (func == 4) return a > ref;
    if (func == 5) return a != ref;
    if (func == 6) return a >= ref;
    return true;
}

void main() {
    if (vsc_ffp_point_smooth != 0) {
        vec2 pc = gl_PointCoord * 2.0 - 1.0;
        if (dot(pc, pc) > 1.0) {
            discard;
        }
    }
    vec4 color = vsc_FrontColor;
    color = ffp_apply_unit(color, vsc_ffp_tex_target[0], vsc_ffp_tex_env[0], vsc_ffp_tex_has_alpha[0],
            vsc_TextureEnvColor[0], vsc_ffp_tex2d[0], vsc_ffp_texcube[0], vsc_ffp_texrect[0], vsc_TexCoord[0]);
    color = ffp_apply_unit(color, vsc_ffp_tex_target[1], vsc_ffp_tex_env[1], vsc_ffp_tex_has_alpha[1],
            vsc_TextureEnvColor[1], vsc_ffp_tex2d[1], vsc_ffp_texcube[1], vsc_ffp_texrect[1], vsc_TexCoord[1]);
    color = ffp_apply_unit(color, vsc_ffp_tex_target[2], vsc_ffp_tex_env[2], vsc_ffp_tex_has_alpha[2],
            vsc_TextureEnvColor[2], vsc_ffp_tex2d[2], vsc_ffp_texcube[2], vsc_ffp_texrect[2], vsc_TexCoord[2]);
    color = ffp_apply_unit(color, vsc_ffp_tex_target[3], vsc_ffp_tex_env[3], vsc_ffp_tex_has_alpha[3],
            vsc_TextureEnvColor[3], vsc_ffp_tex2d[3], vsc_ffp_texcube[3], vsc_ffp_texrect[3], vsc_TexCoord[3]);
    color.rgb = clamp(color.rgb + vsc_FrontSecondaryColor.rgb, 0.0, 1.0);
    if (vsc_ffp_fog_mode != 0) {
        float c = vsc_FogFragCoord;
        float f;
        if (vsc_ffp_fog_mode == 1) {
            f = (vsc_Fog.end - c) / max(vsc_Fog.end - vsc_Fog.start, 1e-6);
        } else if (vsc_ffp_fog_mode == 2) {
            f = exp(-vsc_Fog.density * c);
        } else {
            float dc = vsc_Fog.density * c;
            f = exp(-dc * dc);
        }
        color.rgb = mix(vsc_Fog.color.rgb, color.rgb, clamp(f, 0.0, 1.0));
    }
    if (!ffp_compare(vsc_ffp_alpha_func, color.a, vsc_ffp_alpha_ref)) {
        discard;
    }
    vsc_FragColor = color;
}
)GLSL";

std::string Assemble(ShaderStage stage, int glsl_version, bool flat_shading, const char *body) {
    std::set<std::string> used = CollectBuiltins(body);
    // The FFP program declares its own varyings (they may need "flat")
    for (const char *varying : {"vsc_FrontColor", "vsc_FrontSecondaryColor", "vsc_FogFragCoord", "vsc_TexCoord"}) {
        used.erase(varying);
    }
    std::string src = "#version " + std::to_string(glsl_version) + " core\n";
    src += flat_shading ? "#define FFP_INTERP flat\n" : "#define FFP_INTERP smooth\n";
    src += BuildPrelude(stage, used, VaryingSet());
    src += body;
    return src;
}

} // namespace

std::string FixedFunctionVertexSource(int glsl_version, bool flat_shading) {
    return Assemble(ShaderStage::Vertex, glsl_version, flat_shading, kVertexBody);
}

std::string FixedFunctionFragmentSource(int glsl_version, bool flat_shading) {
    return Assemble(ShaderStage::Fragment, glsl_version, flat_shading, kFragmentBody);
}

FfpSamplerUnits FixedFunctionSamplerUnits(int unit, int ffp_tex_target) {
    FfpSamplerUnits units{kFfpDummyUnit2D, kFfpDummyUnitCube, kFfpDummyUnitRect};
    switch (ffp_tex_target) {
        case kFfpTexCube:
            units.texcube = unit;
            break;
        case kFfpTexRect:
            units.texrect = unit;
            break;
        case kFfpTex2D:
        default:
            // An unused unit still points its 2D sampler at itself: harmless, and
            // the common case of a texture bound but not enabled stays cheap.
            units.tex2d = unit;
            break;
    }
    return units;
}

} // namespace vsgl
