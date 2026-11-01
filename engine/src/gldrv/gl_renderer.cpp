/*
 * gl_renderer.cpp
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

#include "gl_renderer.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "gl_ffp_compat.h"
#include "gl_ffp_shader.h"
#include "gl_shader_translate.h"
#include "src/gfxlib.h"
#include "src/vs_logging.h"

#ifndef GL_COPY_WRITE_BUFFER
#define GL_COPY_WRITE_BUFFER 0x8F37
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif

namespace vsgl {

namespace {

const int kNumGroups = static_cast<int>(StateGroup::Count);
const int kLights = FixedFunctionState::kMaxLights;
const int kUnits = FixedFunctionState::kMaxTextureUnits;
const int kTexMatrices = FixedFunctionState::kMaxTextureMatrices;

struct LightLocations {
    GLint ambient, diffuse, specular, position, half_vector, spot_direction, spot_exponent, spot_cutoff,
            spot_cos_cutoff, constant, linear, quadratic;
};

struct ProductLocations {
    GLint ambient, diffuse, specular;
};

struct MaterialLocations {
    GLint emission, ambient, diffuse, specular, shininess;
};

/// Locations of the built-in uniforms of one program (-1 when unused)
struct ProgramRecord {
    bool is_ffp = false;
    uint64_t uploaded[kNumGroups] = {};
    int point_smooth = -1;

    GLint mv, mv_inv, mv_t, mv_it;
    GLint proj, proj_inv, proj_t, proj_it;
    GLint mvp, mvp_inv, mvp_t, mvp_it;
    GLint normal_matrix, normal_scale;
    GLint tex[kTexMatrices], tex_inv[kTexMatrices], tex_t[kTexMatrices], tex_it[kTexMatrices];
    LightLocations light[kLights];
    ProductLocations front_product[kLights], back_product[kLights];
    MaterialLocations front_material, back_material;
    GLint light_model_ambient, front_scene_color, back_scene_color;
    GLint fog_color, fog_density, fog_start, fog_end, fog_scale;
    GLint env_color[kUnits];
    GLint eye_plane[4][kTexMatrices], object_plane[4][kTexMatrices];

    // fixed-function program controls
    GLint ffp_lighting, ffp_light_enabled, ffp_separate_specular, ffp_local_viewer, ffp_color_material,
            ffp_texgen[kUnits], ffp_fog_distance, ffp_tex_target, ffp_tex_env, ffp_tex_has_alpha, ffp_tex2d,
            ffp_texcube, ffp_texrect, ffp_fog_mode, ffp_alpha_func, ffp_alpha_ref, ffp_point_smooth;
};

ContextProfile g_profile = MakeProfile(ProfileKind::GL41Core, false);
std::map<GLuint, ProgramRecord> g_programs;
GLuint g_ffp_program[2] = {0, 0};  // smooth, flat
GLuint g_user_program = 0;
GLuint g_bound_program = 0;
GLuint g_stream_vao = 0;
float g_line_width_max = 1.0f;

/**
 * A ring of GPU memory that client-side vertex and index arrays are copied
 * into. On the 4.6 path it is persistently mapped and fenced in thirds;
 * on the 4.1 path it is orphaned when full and written through unsynchronized
 * MapBufferRange, which never touches a range a pending draw can still read.
 */
class StreamBuffer {
public:
    void Init(size_t capacity, bool persistent) {
        capacity_ = capacity;
        persistent_ = persistent && glBufferStorage_p != nullptr && glFenceSync_p != nullptr;
        (*glGenBuffersARB_p)(1, &buffer_);
        Allocate();
    }

    void Destroy() {
        for (vsglSync &fence : fences_) {
            if (fence != nullptr) {
                (*glDeleteSync_p)(fence);
                fence = nullptr;
            }
        }
        if (buffer_ != 0) {
            GFXForgetBuffer(buffer_);
            (*glDeleteBuffersARB_p)(1, &buffer_);
            buffer_ = 0;
        }
        mapped_ = nullptr;
    }

    GLuint Buffer() const {
        return buffer_;
    }

    /// Copies `bytes` in and returns their offset in the buffer
    size_t Write(const void *data, size_t bytes) {
        const size_t aligned = StreamAlign(bytes);
        if (aligned > StreamRingRegionSize(capacity_, 3)) {
            Grow(aligned * 4);
        }
        if (persistent_) {
            const size_t region_size = StreamRingRegionSize(capacity_, 3);
            if (cursor_ + aligned > (region_ + 1) * region_size) {
                // Leaving this third: fence it, then make sure the GPU is done with the next one
                fences_[region_] = (*glFenceSync_p)(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                region_ = (region_ + 1) % 3;
                cursor_ = region_ * region_size;
                if (fences_[region_] != nullptr) {
                    (*glClientWaitSync_p)(fences_[region_], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
                    (*glDeleteSync_p)(fences_[region_]);
                    fences_[region_] = nullptr;
                }
            }
            std::memcpy(static_cast<char *>(mapped_) + cursor_, data, bytes);
        } else {
            (*glBindBufferARB_p)(GL_COPY_WRITE_BUFFER, buffer_);
            if (cursor_ + aligned > capacity_) {
                (*glBufferDataARB_p)(GL_COPY_WRITE_BUFFER, capacity_, nullptr, GL_STREAM_DRAW);
                cursor_ = 0;
            }
            void *dst = (*glMapBufferRange_p)(GL_COPY_WRITE_BUFFER, cursor_, bytes,
                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
            if (dst != nullptr) {
                std::memcpy(dst, data, bytes);
                (*glUnmapBufferARB_p)(GL_COPY_WRITE_BUFFER);
            } else {
                (*glBufferSubData_p)(GL_COPY_WRITE_BUFFER, cursor_, bytes, data);
            }
            (*glBindBufferARB_p)(GL_COPY_WRITE_BUFFER, 0);
        }
        const size_t offset = cursor_;
        cursor_ += aligned;
        return offset;
    }

private:
    void Allocate() {
        (*glBindBufferARB_p)(GL_COPY_WRITE_BUFFER, buffer_);
        if (persistent_) {
            const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
            (*glBufferStorage_p)(GL_COPY_WRITE_BUFFER, capacity_, nullptr, flags);
            mapped_ = (*glMapBufferRange_p)(GL_COPY_WRITE_BUFFER, 0, capacity_, flags);
            if (mapped_ == nullptr) {
                VS_LOG(warning, "Persistent mapping of the streaming buffer failed; using orphaning instead");
                persistent_ = false;
                GFXForgetBuffer(buffer_);
                (*glDeleteBuffersARB_p)(1, &buffer_);
                (*glGenBuffersARB_p)(1, &buffer_);
                (*glBindBufferARB_p)(GL_COPY_WRITE_BUFFER, buffer_);
            }
        }
        if (!persistent_) {
            (*glBufferDataARB_p)(GL_COPY_WRITE_BUFFER, capacity_, nullptr, GL_STREAM_DRAW);
        }
        (*glBindBufferARB_p)(GL_COPY_WRITE_BUFFER, 0);
        cursor_ = 0;
        region_ = 0;
    }

    void Grow(size_t capacity) {
        // A fresh buffer: the old one stays alive for draws already queued
        Destroy();
        capacity_ = capacity;
        (*glGenBuffersARB_p)(1, &buffer_);
        Allocate();
    }

    GLuint buffer_ = 0;
    size_t capacity_ = 0;
    size_t cursor_ = 0;
    bool persistent_ = false;
    void *mapped_ = nullptr;
    size_t region_ = 0;
    vsglSync fences_[3] = {nullptr, nullptr, nullptr};
};

StreamBuffer g_stream_vertices;
StreamBuffer g_stream_indices;

GLint Location(GLuint program, const std::string &name) {
    return (*glGetUniformLocation_p)(program, name.c_str());
}

std::string Indexed(const char *base, int i, const char *member = nullptr) {
    std::string s = std::string(base) + "[" + std::to_string(i) + "]";
    if (member != nullptr) {
        s += ".";
        s += member;
    }
    return s;
}

void LookUp(GLuint program, ProgramRecord &r) {
    r.mv = Location(program, "vsc_ModelViewMatrix");
    r.mv_inv = Location(program, "vsc_ModelViewMatrixInverse");
    r.mv_t = Location(program, "vsc_ModelViewMatrixTranspose");
    r.mv_it = Location(program, "vsc_ModelViewMatrixInverseTranspose");
    r.proj = Location(program, "vsc_ProjectionMatrix");
    r.proj_inv = Location(program, "vsc_ProjectionMatrixInverse");
    r.proj_t = Location(program, "vsc_ProjectionMatrixTranspose");
    r.proj_it = Location(program, "vsc_ProjectionMatrixInverseTranspose");
    r.mvp = Location(program, "vsc_ModelViewProjectionMatrix");
    r.mvp_inv = Location(program, "vsc_ModelViewProjectionMatrixInverse");
    r.mvp_t = Location(program, "vsc_ModelViewProjectionMatrixTranspose");
    r.mvp_it = Location(program, "vsc_ModelViewProjectionMatrixInverseTranspose");
    r.normal_matrix = Location(program, "vsc_NormalMatrix");
    r.normal_scale = Location(program, "vsc_NormalScale");
    for (int i = 0; i < kTexMatrices; ++i) {
        r.tex[i] = Location(program, Indexed("vsc_TextureMatrix", i));
        r.tex_inv[i] = Location(program, Indexed("vsc_TextureMatrixInverse", i));
        r.tex_t[i] = Location(program, Indexed("vsc_TextureMatrixTranspose", i));
        r.tex_it[i] = Location(program, Indexed("vsc_TextureMatrixInverseTranspose", i));
    }
    for (int i = 0; i < kLights; ++i) {
        LightLocations &l = r.light[i];
        l.ambient = Location(program, Indexed("vsc_LightSource", i, "ambient"));
        l.diffuse = Location(program, Indexed("vsc_LightSource", i, "diffuse"));
        l.specular = Location(program, Indexed("vsc_LightSource", i, "specular"));
        l.position = Location(program, Indexed("vsc_LightSource", i, "position"));
        l.half_vector = Location(program, Indexed("vsc_LightSource", i, "halfVector"));
        l.spot_direction = Location(program, Indexed("vsc_LightSource", i, "spotDirection"));
        l.spot_exponent = Location(program, Indexed("vsc_LightSource", i, "spotExponent"));
        l.spot_cutoff = Location(program, Indexed("vsc_LightSource", i, "spotCutoff"));
        l.spot_cos_cutoff = Location(program, Indexed("vsc_LightSource", i, "spotCosCutoff"));
        l.constant = Location(program, Indexed("vsc_LightSource", i, "constantAttenuation"));
        l.linear = Location(program, Indexed("vsc_LightSource", i, "linearAttenuation"));
        l.quadratic = Location(program, Indexed("vsc_LightSource", i, "quadraticAttenuation"));
        for (int side = 0; side < 2; ++side) {
            const char *base = side == 0 ? "vsc_FrontLightProduct" : "vsc_BackLightProduct";
            ProductLocations &p = side == 0 ? r.front_product[i] : r.back_product[i];
            p.ambient = Location(program, Indexed(base, i, "ambient"));
            p.diffuse = Location(program, Indexed(base, i, "diffuse"));
            p.specular = Location(program, Indexed(base, i, "specular"));
        }
    }
    for (int side = 0; side < 2; ++side) {
        const std::string base = side == 0 ? "vsc_FrontMaterial." : "vsc_BackMaterial.";
        MaterialLocations &m = side == 0 ? r.front_material : r.back_material;
        m.emission = Location(program, base + "emission");
        m.ambient = Location(program, base + "ambient");
        m.diffuse = Location(program, base + "diffuse");
        m.specular = Location(program, base + "specular");
        m.shininess = Location(program, base + "shininess");
    }
    r.light_model_ambient = Location(program, "vsc_LightModel.ambient");
    r.front_scene_color = Location(program, "vsc_FrontLightModelProduct.sceneColor");
    r.back_scene_color = Location(program, "vsc_BackLightModelProduct.sceneColor");
    r.fog_color = Location(program, "vsc_Fog.color");
    r.fog_density = Location(program, "vsc_Fog.density");
    r.fog_start = Location(program, "vsc_Fog.start");
    r.fog_end = Location(program, "vsc_Fog.end");
    r.fog_scale = Location(program, "vsc_Fog.scale");
    static const char *const kEyePlanes[4] = {"vsc_EyePlaneS", "vsc_EyePlaneT", "vsc_EyePlaneR", "vsc_EyePlaneQ"};
    static const char *const kObjectPlanes[4] =
            {"vsc_ObjectPlaneS", "vsc_ObjectPlaneT", "vsc_ObjectPlaneR", "vsc_ObjectPlaneQ"};
    for (int c = 0; c < 4; ++c) {
        for (int u = 0; u < kTexMatrices; ++u) {
            r.eye_plane[c][u] = Location(program, Indexed(kEyePlanes[c], u));
            r.object_plane[c][u] = Location(program, Indexed(kObjectPlanes[c], u));
        }
    }
    for (int u = 0; u < kUnits; ++u) {
        r.env_color[u] = Location(program, Indexed("vsc_TextureEnvColor", u));
        r.ffp_texgen[u] = Location(program, Indexed("vsc_ffp_texgen", u));
    }
    r.ffp_lighting = Location(program, "vsc_ffp_lighting");
    r.ffp_light_enabled = Location(program, "vsc_ffp_light_enabled");
    r.ffp_separate_specular = Location(program, "vsc_ffp_separate_specular");
    r.ffp_local_viewer = Location(program, "vsc_ffp_local_viewer");
    r.ffp_color_material = Location(program, "vsc_ffp_color_material");
    r.ffp_fog_distance = Location(program, "vsc_ffp_fog_distance");
    r.ffp_tex_target = Location(program, "vsc_ffp_tex_target");
    r.ffp_tex_env = Location(program, "vsc_ffp_tex_env");
    r.ffp_tex_has_alpha = Location(program, "vsc_ffp_tex_has_alpha");
    r.ffp_tex2d = Location(program, "vsc_ffp_tex2d");
    r.ffp_texcube = Location(program, "vsc_ffp_texcube");
    r.ffp_texrect = Location(program, "vsc_ffp_texrect");
    r.ffp_fog_mode = Location(program, "vsc_ffp_fog_mode");
    r.ffp_alpha_func = Location(program, "vsc_ffp_alpha_func");
    r.ffp_alpha_ref = Location(program, "vsc_ffp_alpha_ref");
    r.ffp_point_smooth = Location(program, "vsc_ffp_point_smooth");
}

void UploadMatrix(GLint loc, const Mat4 &m, bool transpose = false) {
    if (loc >= 0) {
        (*glUniformMatrix4fv_p)(loc, 1, transpose ? GL_TRUE : GL_FALSE, m.m);
    }
}

/// Uploads m, its inverse, its transpose and its inverse-transpose to whichever locations are in use
void UploadMatrixFamily(GLint loc, GLint inv_loc, GLint t_loc, GLint it_loc, const Mat4 &m) {
    UploadMatrix(loc, m);
    UploadMatrix(t_loc, m, true);
    if (inv_loc >= 0 || it_loc >= 0) {
        Mat4 inv;
        Invert(m, inv);
        UploadMatrix(inv_loc, inv);
        UploadMatrix(it_loc, inv, true);
    }
}

void Upload4(GLint loc, const float *v) {
    if (loc >= 0) {
        (*glUniform4fv_p)(loc, 1, v);
    }
}

void Upload1(GLint loc, float v) {
    if (loc >= 0) {
        (*glUniform1f_p)(loc, v);
    }
}

void Upload1i(GLint loc, int v) {
    if (loc >= 0) {
        (*glUniform1i_p)(loc, v);
    }
}

void Product(const float *a, const float *b, float *out) {
    for (int i = 0; i < 4; ++i) {
        out[i] = a[i] * b[i];
    }
}

int FfpTarget(TexTarget target) {
    switch (target) {
        case TexTarget::Tex2D:
            return kFfpTex2D;
        case TexTarget::Cube:
            return kFfpTexCube;
        case TexTarget::Rect:
            return kFfpTexRect;
        case TexTarget::Tex1D:
        case TexTarget::Tex3D: {
            static bool warned = false;
            if (!warned) {
                warned = true;
                VS_LOG(warning, "Fixed-function 1D/3D texturing is not emulated on the core profile");
            }
            return kFfpTexNone;
        }
        case TexTarget::Off:
        default:
            return kFfpTexNone;
    }
}

void UploadMatrices(ProgramRecord &r, const FixedFunctionState &s, bool mv_dirty, bool proj_dirty) {
    const Mat4 &mv = s.ModelView();
    const Mat4 &proj = s.Projection();
    if (mv_dirty) {
        UploadMatrixFamily(r.mv, r.mv_inv, r.mv_t, r.mv_it, mv);
        if (r.normal_matrix >= 0) {
            float nm[9];
            NormalMatrix(mv, nm);
            (*glUniformMatrix3fv_p)(r.normal_matrix, 1, GL_FALSE, nm);
        }
        if (r.normal_scale >= 0) {
            // GL_RESCALE_NORMAL's factor: 1 / |third row of the inverse modelview's upper 3x3|
            Mat4 inv;
            Invert(mv, inv);
            const float len = std::sqrt(inv(2, 0) * inv(2, 0) + inv(2, 1) * inv(2, 1) + inv(2, 2) * inv(2, 2));
            Upload1(r.normal_scale, len > 0.0f ? 1.0f / len : 1.0f);
        }
    }
    if (proj_dirty) {
        UploadMatrixFamily(r.proj, r.proj_inv, r.proj_t, r.proj_it, proj);
    }
    if (r.mvp >= 0 || r.mvp_inv >= 0 || r.mvp_t >= 0 || r.mvp_it >= 0) {
        UploadMatrixFamily(r.mvp, r.mvp_inv, r.mvp_t, r.mvp_it, proj * mv);
    }
}

void UploadTextureMatrices(ProgramRecord &r, const FixedFunctionState &s) {
    for (int i = 0; i < kTexMatrices; ++i) {
        UploadMatrixFamily(r.tex[i], r.tex_inv[i], r.tex_t[i], r.tex_it[i], s.TextureMatrix(i));
    }
}

void UploadLights(ProgramRecord &r, const FixedFunctionState &s) {
    GLint enabled[kLights];
    for (int i = 0; i < kLights; ++i) {
        const LightState &light = s.Light(i);
        const LightLocations &l = r.light[i];
        enabled[i] = light.enabled ? 1 : 0;
        Upload4(l.ambient, light.ambient);
        Upload4(l.diffuse, light.diffuse);
        Upload4(l.specular, light.specular);
        Upload4(l.position, light.position);
        if (l.half_vector >= 0) {
            // The infinite-viewer half vector: normalize(direction to light + (0, 0, 1))
            float d[3] = {light.position[0], light.position[1], light.position[2]};
            float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (len > 0) {
                d[0] /= len;
                d[1] /= len;
                d[2] /= len;
            }
            d[2] += 1.0f;
            len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            const float h[4] = {len > 0 ? d[0] / len : 0, len > 0 ? d[1] / len : 0, len > 0 ? d[2] / len : 1, 0};
            Upload4(l.half_vector, h);
        }
        if (l.spot_direction >= 0) {
            (*glUniform3fv_p)(l.spot_direction, 1, light.spot_direction);
        }
        Upload1(l.spot_exponent, light.spot_exponent);
        Upload1(l.spot_cutoff, light.spot_cutoff);
        Upload1(l.spot_cos_cutoff, light.spot_cutoff >= 180.0f
                ? -1.0f : std::cos(light.spot_cutoff * 3.14159265358979f / 180.0f));
        Upload1(l.constant, light.constant_attenuation);
        Upload1(l.linear, light.linear_attenuation);
        Upload1(l.quadratic, light.quadratic_attenuation);
    }
    if (r.ffp_light_enabled >= 0) {
        (*glUniform1iv_p)(r.ffp_light_enabled, kLights, enabled);
    }
    Upload1i(r.ffp_separate_specular, s.SeparateSpecular() ? 1 : 0);
    Upload1i(r.ffp_local_viewer, s.LocalViewer() ? 1 : 0);
}

void UploadMaterial(ProgramRecord &r, const FixedFunctionState &s) {
    // The built-in program applies GL_COLOR_MATERIAL per vertex itself; user
    // programs see what gl_FrontMaterial reported in GL 2.x.
    const MaterialState m = r.is_ffp ? s.Material() : s.EffectiveMaterial();
    for (const MaterialLocations *ml : {&r.front_material, &r.back_material}) {
        Upload4(ml->emission, m.emission);
        Upload4(ml->ambient, m.ambient);
        Upload4(ml->diffuse, m.diffuse);
        Upload4(ml->specular, m.specular);
        Upload1(ml->shininess, m.shininess);
    }
    Upload4(r.light_model_ambient, s.LightModelAmbient());
    if (r.front_scene_color >= 0 || r.back_scene_color >= 0) {
        const float *a = s.LightModelAmbient();
        const float scene[4] = {m.emission[0] + m.ambient[0] * a[0], m.emission[1] + m.ambient[1] * a[1],
                m.emission[2] + m.ambient[2] * a[2], m.diffuse[3]};
        Upload4(r.front_scene_color, scene);
        Upload4(r.back_scene_color, scene);
    }
    for (int i = 0; i < kLights; ++i) {
        const LightState &light = s.Light(i);
        for (const ProductLocations *p : {&r.front_product[i], &r.back_product[i]}) {
            float v[4];
            if (p->ambient >= 0) {
                Product(m.ambient, light.ambient, v);
                Upload4(p->ambient, v);
            }
            if (p->diffuse >= 0) {
                Product(m.diffuse, light.diffuse, v);
                Upload4(p->diffuse, v);
            }
            if (p->specular >= 0) {
                Product(m.specular, light.specular, v);
                Upload4(p->specular, v);
            }
        }
    }
    Upload1i(r.ffp_color_material, s.ColorMaterialEnabled() ? static_cast<int>(s.ColorMaterialMode()) : -1);
}

void UploadFog(ProgramRecord &r, const FixedFunctionState &s) {
    const FogState &fog = s.Fog();
    Upload4(r.fog_color, fog.color);
    Upload1(r.fog_density, fog.density);
    Upload1(r.fog_start, fog.start);
    Upload1(r.fog_end, fog.end);
    Upload1(r.fog_scale, fog.end != fog.start ? 1.0f / (fog.end - fog.start) : 0.0f);
    int mode = kFfpFogOff;
    if (fog.enabled) {
        mode = fog.mode == FogMode::Linear ? kFfpFogLinear : (fog.mode == FogMode::Exp2 ? kFfpFogExp2 : kFfpFogExp);
    }
    Upload1i(r.ffp_fog_mode, mode);
    Upload1i(r.ffp_fog_distance, static_cast<int>(fog.distance));
}

void UploadTexturing(ProgramRecord &r, const FixedFunctionState &s) {
    GLint targets[kUnits], envs[kUnits], has_alpha[kUnits], tex2d[kUnits], texcube[kUnits], texrect[kUnits];
    for (int u = 0; u < kUnits; ++u) {
        const TexUnitState &t = s.TexUnit(u);
        Upload4(r.env_color[u], t.env_color);
        for (int c = 0; c < 4; ++c) {
            Upload4(r.eye_plane[c][u], t.eye_plane[c]);
            Upload4(r.object_plane[c][u], t.object_plane[c]);
        }
        if (r.ffp_texgen[u] >= 0) {
            GLint gen[4];
            for (int c = 0; c < 4; ++c) {
                gen[c] = t.gen_enabled[c] ? static_cast<GLint>(t.gen_mode[c]) + 1 : 0;
            }
            (*glUniform4iv_p)(r.ffp_texgen[u], 1, gen);
        }
        targets[u] = FfpTarget(s.EffectiveTarget(u));
        envs[u] = static_cast<GLint>(t.env_mode);
        has_alpha[u] = t.has_alpha ? 1 : 0;
        const FfpSamplerUnits units = FixedFunctionSamplerUnits(u, targets[u]);
        tex2d[u] = units.tex2d;
        texcube[u] = units.texcube;
        texrect[u] = units.texrect;
    }
    // Samplers beyond the combiner units still exist in user programs; for
    // others these are simply absent.
    for (int c = 0; c < 4; ++c) {
        for (int u = kUnits; u < kTexMatrices; ++u) {
            Upload4(r.eye_plane[c][u], s.TexUnit(0).eye_plane[c]);
            Upload4(r.object_plane[c][u], s.TexUnit(0).object_plane[c]);
        }
    }
    if (r.ffp_tex_target >= 0) {
        (*glUniform1iv_p)(r.ffp_tex_target, kUnits, targets);
    }
    if (r.ffp_tex_env >= 0) {
        (*glUniform1iv_p)(r.ffp_tex_env, kUnits, envs);
    }
    if (r.ffp_tex_has_alpha >= 0) {
        (*glUniform1iv_p)(r.ffp_tex_has_alpha, kUnits, has_alpha);
    }
    if (r.ffp_tex2d >= 0) {
        (*glUniform1iv_p)(r.ffp_tex2d, kUnits, tex2d);
    }
    if (r.ffp_texcube >= 0) {
        (*glUniform1iv_p)(r.ffp_texcube, kUnits, texcube);
    }
    if (r.ffp_texrect >= 0) {
        (*glUniform1iv_p)(r.ffp_texrect, kUnits, texrect);
    }
}

void UploadRaster(ProgramRecord &r, const FixedFunctionState &s) {
    Upload1i(r.ffp_lighting, s.Lighting() ? 1 : 0);
    const CompareFunc func = s.AlphaTestEnabled() ? s.AlphaFunc() : CompareFunc::AlwaysPass;
    Upload1i(r.ffp_alpha_func, static_cast<int>(func));
    Upload1(r.ffp_alpha_ref, s.AlphaRef());
}

GLuint CompileStage(GLenum type, const std::string &source, const char *what) {
    GLuint shader = (*glCreateShader_p)(type);
    const char *src = source.c_str();
    (*glShaderSource_p)(shader, 1, &src, nullptr);
    (*glCompileShader_p)(shader);
    GLint ok = 0;
    (*glGetShaderiv_p)(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        GLsizei len = 0;
        (*glGetShaderInfoLog_p)(shader, sizeof(log) - 1, &len, log);
        log[len] = '\0';
        VS_LOG(error, (boost::format("Failed to compile the %1%: %2%") % what % log));
        (*glDeleteShader_p)(shader);
        return 0;
    }
    return shader;
}

GLuint BuildFixedFunctionProgram(bool flat) {
    const int version = g_profile.GLSLVersion();
    GLuint vs = CompileStage(GL_VERTEX_SHADER, FixedFunctionVertexSource(version, flat),
            "fixed-function vertex shader");
    GLuint fs = CompileStage(GL_FRAGMENT_SHADER, FixedFunctionFragmentSource(version, flat),
            "fixed-function fragment shader");
    if (vs == 0 || fs == 0) {
        return 0;
    }
    GLuint program = (*glCreateProgram_p)();
    (*glAttachShader_p)(program, vs);
    (*glAttachShader_p)(program, fs);
    (*glLinkProgram_p)(program);
    (*glDeleteShader_p)(vs);
    (*glDeleteShader_p)(fs);
    GLint ok = 0;
    (*glGetProgramiv_p)(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        GLsizei len = 0;
        (*glGetProgramInfoLog_p)(program, sizeof(log) - 1, &len, log);
        log[len] = '\0';
        VS_LOG(error, (boost::format("Failed to link the fixed-function program: %1%") % log));
        (*glDeleteProgram_p)(program);
        return 0;
    }
    RegisterProgram(program);
    g_programs[program].is_ffp = true;
    return program;
}

void APIENTRY DebugCallback(GLenum /*source*/, GLenum /*type*/, GLuint id, GLenum severity, GLsizei /*length*/,
        const GLchar *message, const void * /*user*/) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        VS_LOG(error, (boost::format("OpenGL debug message %1%: %2%") % id % message));
    } else {
        VS_LOG(warning, (boost::format("OpenGL debug message %1%: %2%") % id % message));
    }
}

} // namespace

bool InitRenderer(const ContextProfile &profile) {
    g_profile = profile;

    if (profile.HasDebugOutput() && glDebugMessageCallback_p != nullptr) {
        const char *debug = std::getenv("VS_GL_DEBUG");
        if (debug != nullptr && debug[0] != '\0' && debug[0] != '0') {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            (*glDebugMessageCallback_p)(DebugCallback, nullptr);
            VS_LOG(info, "OpenGL debug output enabled");
        }
    }

    g_ffp_program[0] = BuildFixedFunctionProgram(false);
    g_ffp_program[1] = BuildFixedFunctionProgram(true);
    if (g_ffp_program[0] == 0) {
        return false;
    }
    if (g_ffp_program[1] == 0) {
        g_ffp_program[1] = g_ffp_program[0];
    }

    (*glGenVertexArrays_p)(1, &g_stream_vao);
    (*glBindVertexArray_p)(g_stream_vao);
    g_stream_vertices.Init(8u << 20, profile.HasBufferStorage());
    g_stream_indices.Init(2u << 20, profile.HasBufferStorage());

    GLfloat range[2] = {1.0f, 1.0f};
    glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
    g_line_width_max = profile.SupportsWideLines() ? range[1] : 1.0f;

    // Defaults for attributes no array provides, as in GL 2.x
    (*glVertexAttrib4f_p)(kAttribNormal, 0, 0, 1, 0);
    (*glVertexAttrib4f_p)(kAttribColor, 1, 1, 1, 1);
    (*glVertexAttrib4f_p)(kAttribSecondaryColor, 0, 0, 0, 1);
    for (int i = 0; i < 8; ++i) {
        (*glVertexAttrib4f_p)(kAttribMultiTexCoord0 + i, 0, 0, 0, 1);
    }
    VS_LOG(important_info, (boost::format("Core renderer ready (%1%, GLSL %2%, %3% streaming buffers)")
            % profile.Name() % profile.GLSLVersion()
            % (profile.HasBufferStorage() && glBufferStorage_p != nullptr ? "persistent" : "orphaned")));
    return true;
}

void ShutdownRenderer() {
    g_stream_vertices.Destroy();
    g_stream_indices.Destroy();
    if (g_stream_vao != 0) {
        (*glDeleteVertexArrays_p)(1, &g_stream_vao);
        g_stream_vao = 0;
    }
}

const ContextProfile &CurrentProfile() {
    return g_profile;
}

void RegisterProgram(GLuint program) {
    if (program == 0) {
        return;
    }
    ProgramRecord record;
    LookUp(program, record);
    g_programs[program] = record;
}

void ForgetProgram(GLuint program) {
    g_programs.erase(program);
    if (g_bound_program == program) {
        g_bound_program = 0;
    }
    if (g_user_program == program) {
        g_user_program = 0;
    }
}

void SetUserProgram(GLuint program) {
    g_user_program = program;
    if (program != 0 && g_bound_program != program) {
        (*glUseProgram_p)(program);
        g_bound_program = program;
    }
}

GLuint UserProgram() {
    return g_user_program;
}

void PrepareDraw(Primitive primitive, bool color_array) {
    const FixedFunctionState &s = vsglState();
    GLuint program = g_user_program;
    if (program == 0) {
        program = g_ffp_program[s.FlatShading() ? 1 : 0];
    }
    if (g_bound_program != program) {
        (*glUseProgram_p)(program);
        g_bound_program = program;
    }
    auto it = g_programs.find(program);
    if (it == g_programs.end()) {
        RegisterProgram(program);
        it = g_programs.find(program);
    }
    ProgramRecord &r = it->second;

    auto dirty = [&](StateGroup g) {
        return r.uploaded[static_cast<int>(g)] != s.Version(g);
    };
    const bool mv = dirty(StateGroup::ModelView);
    const bool proj = dirty(StateGroup::Projection);
    if (mv || proj) {
        UploadMatrices(r, s, mv, proj);
    }
    if (dirty(StateGroup::TextureMatrix)) {
        UploadTextureMatrices(r, s);
    }
    const bool lights = dirty(StateGroup::Lights);
    if (lights) {
        UploadLights(r, s);
    }
    // Light products depend on both the lights and the material
    if (lights || dirty(StateGroup::Material)) {
        UploadMaterial(r, s);
    }
    if (dirty(StateGroup::Fog)) {
        UploadFog(r, s);
    }
    if (dirty(StateGroup::Texturing)) {
        UploadTexturing(r, s);
    }
    if (dirty(StateGroup::Raster)) {
        UploadRaster(r, s);
    }
    for (int g = 0; g < kNumGroups; ++g) {
        r.uploaded[g] = s.Version(static_cast<StateGroup>(g));
    }
    if (r.ffp_point_smooth >= 0) {
        const int smooth = (primitive == Primitive::Points && s.PointSmooth()) ? 1 : 0;
        if (smooth != r.point_smooth) {
            Upload1i(r.ffp_point_smooth, smooth);
            r.point_smooth = smooth;
        }
    }
    if (!color_array) {
        (*glVertexAttrib4fv_p)(kAttribColor, s.CurrentColor());
    }
}

void BindStreamVertexArray() {
    (*glBindVertexArray_p)(g_stream_vao);
}

size_t StreamVertexData(const void *data, size_t bytes) {
    const size_t offset = g_stream_vertices.Write(data, bytes);
    GFXBindBuffer(g_stream_vertices.Buffer());
    return offset;
}

size_t StreamIndexData(const void *data, size_t bytes) {
    const size_t offset = g_stream_indices.Write(data, bytes);
    BindElementBuffer(g_stream_indices.Buffer());
    return offset;
}

void BindElementBuffer(GLuint buffer) {
    (*glBindBufferARB_p)(GL_ELEMENT_ARRAY_BUFFER, buffer);
}

GLenum IndexType(int index_size) {
    switch (index_size) {
        case 1:
            return GL_UNSIGNED_BYTE;
        case 2:
            return GL_UNSIGNED_SHORT;
        case 4:
        default:
            return GL_UNSIGNED_INT;
    }
}

void DrawCommands(const std::vector<DrawCommand> &commands, size_t index_base) {
    for (const DrawCommand &cmd : commands) {
        const GLenum mode = PrimitiveToGL(cmd.primitive);
        if (cmd.indexed) {
            const size_t offset = index_base + static_cast<size_t>(cmd.first) * cmd.index_size;
            glDrawElements(mode, cmd.count, IndexType(cmd.index_size), reinterpret_cast<const GLvoid *>(offset));
        } else {
            glDrawArrays(mode, cmd.first, cmd.count);
        }
        ++gl_batches_this_frame;
        gl_vertices_this_frame += cmd.count;
    }
}

float ClampLineWidth(float width) {
    if (width < 1.0f) {
        return 1.0f;
    }
    return width > g_line_width_max ? g_line_width_max : width;
}

} // namespace vsgl
