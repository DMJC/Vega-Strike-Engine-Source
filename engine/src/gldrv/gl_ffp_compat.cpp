/*
 * gl_ffp_compat.cpp
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

#include "gl_ffp_compat.h"

using vsgl::FixedFunctionState;

vsgl::FixedFunctionState &vsglState() {
    static FixedFunctionState state;
    return state;
}

namespace {

bool TextureTarget(GLenum cap, vsgl::TexTarget &target) {
    switch (cap) {
        case GL_TEXTURE_1D:
            target = vsgl::TexTarget::Tex1D;
            return true;
        case GL_TEXTURE_2D:
            target = vsgl::TexTarget::Tex2D;
            return true;
        case GL_TEXTURE_3D:
            target = vsgl::TexTarget::Tex3D;
            return true;
        case GL_TEXTURE_CUBE_MAP_EXT:
            target = vsgl::TexTarget::Cube;
            return true;
        case GL_TEXTURE_RECTANGLE_ARB:
            target = vsgl::TexTarget::Rect;
            return true;
        default:
            return false;
    }
}

bool TexCoordComponent(GLenum coord, vsgl::TexCoordComponent &component) {
    switch (coord) {
        case GL_S:
        case GL_TEXTURE_GEN_S:
            component = vsgl::TexCoordComponent::S;
            return true;
        case GL_T:
        case GL_TEXTURE_GEN_T:
            component = vsgl::TexCoordComponent::T;
            return true;
        case GL_R:
        case GL_TEXTURE_GEN_R:
            component = vsgl::TexCoordComponent::R;
            return true;
        case GL_Q:
        case GL_TEXTURE_GEN_Q:
            component = vsgl::TexCoordComponent::Q;
            return true;
        default:
            return false;
    }
}

bool LightIndex(GLenum light, int &index) {
    if (light >= GL_LIGHT0 && light < GL_LIGHT0 + static_cast<GLenum>(FixedFunctionState::kMaxLights)) {
        index = static_cast<int>(light - GL_LIGHT0);
        return true;
    }
    return false;
}

/// Sets a fixed-function capability; returns false for capabilities core GL still has
bool SetFixedFunctionCap(GLenum cap, bool enabled) {
    FixedFunctionState &s = vsglState();
    int light = 0;
    vsgl::TexTarget target;
    vsgl::TexCoordComponent component;
    if (LightIndex(cap, light)) {
        s.EnableLight(light, enabled);
        return true;
    }
    if (TextureTarget(cap, target)) {
        s.SetTextureTargetEnabled(target, enabled);
        return true;
    }
    if (cap == GL_TEXTURE_GEN_S || cap == GL_TEXTURE_GEN_T || cap == GL_TEXTURE_GEN_R || cap == GL_TEXTURE_GEN_Q) {
        TexCoordComponent(cap, component);
        s.SetTexGenEnabled(component, enabled);
        return true;
    }
    switch (cap) {
        case GL_LIGHTING:
            s.SetLighting(enabled);
            return true;
        case GL_FOG:
            s.SetFogEnabled(enabled);
            return true;
        case GL_ALPHA_TEST:
            s.SetAlphaTestEnabled(enabled);
            return true;
        case GL_COLOR_MATERIAL:
            s.SetColorMaterialEnabled(enabled);
            return true;
        case GL_POINT_SMOOTH:
            s.SetPointSmooth(enabled);
            return true;
        case GL_NORMALIZE:
        case GL_RESCALE_NORMAL:
            // The fixed-function program always normalizes
            return true;
        case GL_LINE_STIPPLE:
        case GL_POLYGON_STIPPLE:
            return true;
        default:
            return false;
    }
}

} // namespace

void vsglEnable(GLenum cap) {
    if (!SetFixedFunctionCap(cap, true)) {
        glEnable(cap);
    }
}

void vsglDisable(GLenum cap) {
    if (!SetFixedFunctionCap(cap, false)) {
        glDisable(cap);
    }
}

GLboolean vsglIsEnabled(GLenum cap) {
    const FixedFunctionState &s = vsglState();
    int light = 0;
    vsgl::TexTarget target;
    vsgl::TexCoordComponent component;
    if (LightIndex(cap, light)) {
        return s.LightEnabled(light) ? GL_TRUE : GL_FALSE;
    }
    if (TextureTarget(cap, target)) {
        return s.TextureTargetEnabled(s.GetActiveTexture(), target) ? GL_TRUE : GL_FALSE;
    }
    if (TexCoordComponent(cap, component) && cap != GL_S && cap != GL_T && cap != GL_R && cap != GL_Q) {
        return s.TexUnit(s.GetActiveTexture()).gen_enabled[static_cast<int>(component)] ? GL_TRUE : GL_FALSE;
    }
    switch (cap) {
        case GL_LIGHTING:
            return s.Lighting() ? GL_TRUE : GL_FALSE;
        case GL_FOG:
            return s.Fog().enabled ? GL_TRUE : GL_FALSE;
        case GL_ALPHA_TEST:
            return s.AlphaTestEnabled() ? GL_TRUE : GL_FALSE;
        case GL_COLOR_MATERIAL:
            return s.ColorMaterialEnabled() ? GL_TRUE : GL_FALSE;
        case GL_POINT_SMOOTH:
            return s.PointSmooth() ? GL_TRUE : GL_FALSE;
        case GL_NORMALIZE:
        case GL_RESCALE_NORMAL:
        case GL_LINE_STIPPLE:
        case GL_POLYGON_STIPPLE:
            return GL_FALSE;
        default:
            return glIsEnabled(cap);
    }
}

void vsglActiveTexture(GLenum texture) {
    vsglState().SetActiveTexture(static_cast<int>(texture - GL_TEXTURE0));
    if (glActiveTextureARB_p != nullptr) {
        (*glActiveTextureARB_p)(texture);
    }
}

void vsglMatrixMode(GLenum mode) {
    switch (mode) {
        case GL_PROJECTION:
            vsglState().SetMatrixMode(vsgl::MatrixMode::Projection);
            break;
        case GL_TEXTURE:
            vsglState().SetMatrixMode(vsgl::MatrixMode::Texture);
            break;
        case GL_MODELVIEW:
        default:
            vsglState().SetMatrixMode(vsgl::MatrixMode::ModelView);
            break;
    }
}

void vsglLoadIdentity() {
    vsglState().LoadIdentity();
}

void vsglLoadMatrixf(const GLfloat *m) {
    vsglState().LoadMatrix(vsgl::Mat4::FromFloats(m));
}

void vsglLoadMatrixd(const GLdouble *m) {
    vsglState().LoadMatrix(vsgl::Mat4::FromDoubles(m));
}

void vsglMultMatrixf(const GLfloat *m) {
    vsglState().MultMatrix(vsgl::Mat4::FromFloats(m));
}

void vsglPushMatrix() {
    vsglState().PushMatrix();
}

void vsglPopMatrix() {
    vsglState().PopMatrix();
}

void vsglTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    vsglState().Translate(x, y, z);
}

void vsglTranslated(GLdouble x, GLdouble y, GLdouble z) {
    vsglState().Translate(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

void vsglScalef(GLfloat x, GLfloat y, GLfloat z) {
    vsglState().Scale(x, y, z);
}

void vsglRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    vsglState().Rotate(angle, x, y, z);
}

void vsglLightf(GLenum light, GLenum pname, GLfloat param) {
    int index = 0;
    if (!LightIndex(light, index)) {
        return;
    }
    FixedFunctionState &s = vsglState();
    switch (pname) {
        case GL_CONSTANT_ATTENUATION:
            s.SetLightAttenuation(index, vsgl::LightAttenuation::Constant, param);
            break;
        case GL_LINEAR_ATTENUATION:
            s.SetLightAttenuation(index, vsgl::LightAttenuation::Linear, param);
            break;
        case GL_QUADRATIC_ATTENUATION:
            s.SetLightAttenuation(index, vsgl::LightAttenuation::Quadratic, param);
            break;
        case GL_SPOT_EXPONENT:
            s.SetLightSpotExponent(index, param);
            break;
        case GL_SPOT_CUTOFF:
            s.SetLightSpotCutoff(index, param);
            break;
        default:
            break;
    }
}

void vsglLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    int index = 0;
    if (!LightIndex(light, index)) {
        return;
    }
    FixedFunctionState &s = vsglState();
    switch (pname) {
        case GL_AMBIENT:
            s.SetLightColor(index, vsgl::LightColor::Ambient, params);
            break;
        case GL_DIFFUSE:
            s.SetLightColor(index, vsgl::LightColor::Diffuse, params);
            break;
        case GL_SPECULAR:
            s.SetLightColor(index, vsgl::LightColor::Specular, params);
            break;
        case GL_POSITION:
            s.SetLightPosition(index, params);
            break;
        case GL_SPOT_DIRECTION:
            s.SetLightSpotDirection(index, params);
            break;
        default:
            vsglLightf(light, pname, params[0]);
            break;
    }
}

void vsglLightModeli(GLenum pname, GLint param) {
    switch (pname) {
        case GL_LIGHT_MODEL_COLOR_CONTROL:
            vsglState().SetSeparateSpecular(param == GL_SEPARATE_SPECULAR_COLOR);
            break;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            vsglState().SetLocalViewer(param != 0);
            break;
        default:
            break;
    }
}

void vsglLightModelfv(GLenum pname, const GLfloat *params) {
    if (pname == GL_LIGHT_MODEL_AMBIENT) {
        vsglState().SetLightModelAmbient(params);
    } else {
        vsglLightModeli(pname, static_cast<GLint>(params[0]));
    }
}

void vsglMaterialfv(GLenum /*face*/, GLenum pname, const GLfloat *params) {
    // Two-sided lighting is never enabled, so only the front material matters
    FixedFunctionState &s = vsglState();
    switch (pname) {
        case GL_AMBIENT:
            s.SetMaterial(vsgl::MaterialParam::Ambient, params);
            break;
        case GL_DIFFUSE:
            s.SetMaterial(vsgl::MaterialParam::Diffuse, params);
            break;
        case GL_SPECULAR:
            s.SetMaterial(vsgl::MaterialParam::Specular, params);
            break;
        case GL_EMISSION:
            s.SetMaterial(vsgl::MaterialParam::Emission, params);
            break;
        case GL_AMBIENT_AND_DIFFUSE:
            s.SetMaterial(vsgl::MaterialParam::AmbientAndDiffuse, params);
            break;
        case GL_SHININESS:
            s.SetShininess(params[0]);
            break;
        default:
            break;
    }
}

void vsglColorMaterial(GLenum /*face*/, GLenum mode) {
    FixedFunctionState &s = vsglState();
    switch (mode) {
        case GL_EMISSION:
            s.SetColorMaterialMode(vsgl::MaterialParam::Emission);
            break;
        case GL_AMBIENT:
            s.SetColorMaterialMode(vsgl::MaterialParam::Ambient);
            break;
        case GL_DIFFUSE:
            s.SetColorMaterialMode(vsgl::MaterialParam::Diffuse);
            break;
        case GL_SPECULAR:
            s.SetColorMaterialMode(vsgl::MaterialParam::Specular);
            break;
        case GL_AMBIENT_AND_DIFFUSE:
        default:
            s.SetColorMaterialMode(vsgl::MaterialParam::AmbientAndDiffuse);
            break;
    }
}

void vsglColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    vsglState().SetCurrentColor(r, g, b, a);
}

void vsglColor4fv(const GLfloat *v) {
    vsglState().SetCurrentColor(v[0], v[1], v[2], v[3]);
}

void vsglColor3f(GLfloat r, GLfloat g, GLfloat b) {
    vsglState().SetCurrentColor(r, g, b, 1.0f);
}

void vsglFogi(GLenum pname, GLint param) {
    FixedFunctionState &s = vsglState();
    switch (pname) {
        case GL_FOG_MODE:
            if (param == GL_LINEAR) {
                s.SetFogMode(vsgl::FogMode::Linear);
            } else if (param == GL_EXP2) {
                s.SetFogMode(vsgl::FogMode::Exp2);
            } else {
                s.SetFogMode(vsgl::FogMode::Exp);
            }
            break;
        case GL_FOG_DISTANCE_MODE_NV:
            if (param == GL_EYE_RADIAL_NV) {
                s.SetFogDistance(vsgl::FogDistance::EyeRadial);
            } else if (param == GL_EYE_PLANE) {
                s.SetFogDistance(vsgl::FogDistance::EyePlane);
            } else {
                s.SetFogDistance(vsgl::FogDistance::EyePlaneAbsolute);
            }
            break;
        default:
            // GL_FOG_INDEX only matters in color index mode
            vsglFogf(pname, static_cast<GLfloat>(param));
            break;
    }
}

void vsglFogf(GLenum pname, GLfloat param) {
    FixedFunctionState &s = vsglState();
    switch (pname) {
        case GL_FOG_DENSITY:
            s.SetFogDensity(param);
            break;
        case GL_FOG_START:
            s.SetFogStart(param);
            break;
        case GL_FOG_END:
            s.SetFogEnd(param);
            break;
        default:
            break;
    }
}

void vsglFogfv(GLenum pname, const GLfloat *params) {
    if (pname == GL_FOG_COLOR) {
        vsglState().SetFogColor(params);
    } else {
        vsglFogf(pname, params[0]);
    }
}

vsgl::CompareFunc vsglCompareFunc(GLenum func) {
    switch (func) {
        case GL_NEVER:
            return vsgl::CompareFunc::Never;
        case GL_LESS:
            return vsgl::CompareFunc::Less;
        case GL_EQUAL:
            return vsgl::CompareFunc::Equal;
        case GL_LEQUAL:
            return vsgl::CompareFunc::LEqual;
        case GL_GREATER:
            return vsgl::CompareFunc::Greater;
        case GL_NOTEQUAL:
            return vsgl::CompareFunc::NotEqual;
        case GL_GEQUAL:
            return vsgl::CompareFunc::GEqual;
        case GL_ALWAYS:
        default:
            return vsgl::CompareFunc::AlwaysPass;
    }
}

void vsglAlphaFunc(GLenum func, GLclampf ref) {
    vsglState().SetAlphaFunc(vsglCompareFunc(func), ref);
}

void vsglShadeModel(GLenum mode) {
    vsglState().SetFlatShading(mode == GL_FLAT);
}

void vsglTexEnvi(GLenum target, GLenum pname, GLint param) {
    if (target != GL_TEXTURE_ENV || pname != GL_TEXTURE_ENV_MODE) {
        return;
    }
    FixedFunctionState &s = vsglState();
    switch (param) {
        case GL_REPLACE:
            s.SetTexEnvMode(vsgl::TexEnvMode::Replace);
            break;
        case GL_ADD:
            s.SetTexEnvMode(vsgl::TexEnvMode::Add);
            break;
        case GL_DECAL:
            s.SetTexEnvMode(vsgl::TexEnvMode::Decal);
            break;
        case GL_BLEND:
            s.SetTexEnvMode(vsgl::TexEnvMode::Blend);
            break;
        case GL_MODULATE:
        default:
            s.SetTexEnvMode(vsgl::TexEnvMode::Modulate);
            break;
    }
}

void vsglTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) {
    if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_COLOR) {
        vsglState().SetTexEnvColor(params);
    }
}

void vsglTexGeni(GLenum coord, GLenum pname, GLint param) {
    vsgl::TexCoordComponent component;
    if (pname != GL_TEXTURE_GEN_MODE || !TexCoordComponent(coord, component)) {
        return;
    }
    vsgl::TexGenMode mode;
    switch (param) {
        case GL_OBJECT_LINEAR:
            mode = vsgl::TexGenMode::ObjectLinear;
            break;
        case GL_SPHERE_MAP:
            mode = vsgl::TexGenMode::SphereMap;
            break;
        case GL_REFLECTION_MAP:
            mode = vsgl::TexGenMode::ReflectionMap;
            break;
        case GL_NORMAL_MAP:
            mode = vsgl::TexGenMode::NormalMap;
            break;
        case GL_EYE_LINEAR:
        default:
            mode = vsgl::TexGenMode::EyeLinear;
            break;
    }
    vsglState().SetTexGenMode(component, mode);
}

void vsglTexGenfv(GLenum coord, GLenum pname, const GLfloat *params) {
    vsgl::TexCoordComponent component;
    if (!TexCoordComponent(coord, component)) {
        return;
    }
    if (pname == GL_EYE_PLANE) {
        vsglState().SetTexGenEyePlane(component, params);
    } else if (pname == GL_OBJECT_PLANE) {
        vsglState().SetTexGenObjectPlane(component, params);
    }
}
