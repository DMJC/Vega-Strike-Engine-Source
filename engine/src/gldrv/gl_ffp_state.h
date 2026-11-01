/*
 * gl_ffp_state.h
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
#ifndef VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_STATE_H
#define VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_STATE_H

/**
 * A CPU-side model of the OpenGL 2.x fixed-function pipeline state.
 *
 * Core profiles removed the matrix stacks, lights, materials, fog, alpha
 * test, texture environments and texture coordinate generation. The
 * renderer still thinks in those terms (and so do the GLSL programs in the
 * data directory, through gl_ModelViewMatrix, gl_LightSource and friends),
 * so this class reproduces the exact GL semantics - including the
 * transformation of light positions and eye planes by the modelview matrix
 * current at the time they are specified - and the driver feeds the result
 * to shaders as uniforms.
 *
 * Nothing in here talks to OpenGL, which is what makes it unit-testable.
 */

#include <cstdint>
#include <vector>

namespace vsgl {

/// Column-major 4x4 matrix, laid out exactly like OpenGL's.
struct Mat4 {
    float m[16];

    static Mat4 Identity();
    static Mat4 FromFloats(const float *column_major);
    static Mat4 FromDoubles(const double *column_major);
    static Mat4 Translation(float x, float y, float z);
    static Mat4 Scaling(float x, float y, float z);
    /// Same as glRotatef: angle in degrees around (x, y, z)
    static Mat4 Rotation(float angle_degrees, float x, float y, float z);
    static Mat4 Ortho(float left, float right, float bottom, float top, float znear, float zfar);
    static Mat4 Frustum(float left, float right, float bottom, float top, float znear, float zfar);

    float &operator()(int row, int col) {
        return m[col * 4 + row];
    }

    float operator()(int row, int col) const {
        return m[col * 4 + row];
    }

    bool operator==(const Mat4 &o) const;
};

Mat4 operator*(const Mat4 &a, const Mat4 &b);
Mat4 Transpose(const Mat4 &a);
/// General inverse. Returns false (and identity) for singular matrices.
bool Invert(const Mat4 &a, Mat4 &out);
/// Transforms a homogeneous point: out = a * v
void TransformPoint(const Mat4 &a, const float v[4], float out[4]);
/// The GL normal matrix: inverse-transpose of the upper-left 3x3, column-major
void NormalMatrix(const Mat4 &modelview, float out[9]);

enum class MatrixMode {
    ModelView,
    Projection,
    Texture,
};

enum class LightColor {
    Ambient,
    Diffuse,
    Specular,
};

enum class LightAttenuation {
    Constant,
    Linear,
    Quadratic,
};

enum class MaterialParam {
    Ambient,
    Diffuse,
    Specular,
    Emission,
    AmbientAndDiffuse,
};

enum class FogMode {
    Linear,
    Exp,
    Exp2,
};

/// How the eye-space distance used for fog is measured (NV_fog_distance)
enum class FogDistance {
    EyePlaneAbsolute,
    EyePlane,
    EyeRadial,
};

enum class CompareFunc {
    Never,
    Less,
    Equal,
    LEqual,
    Greater,
    NotEqual,
    GEqual,
    AlwaysPass,  // not "Always": X11 #defines that
};

enum class TexTarget {
    Off,
    Tex1D,
    Tex2D,
    Tex3D,
    Cube,
    Rect,
};

/**
 * Texture environment modes. The first five are the classic GL modes;
 * the rest are the specific GL_COMBINE setups GFXTextureEnv() builds.
 */
enum class TexEnvMode {
    Modulate,
    Replace,
    Add,
    Decal,
    Blend,
    Interpolate,  ///< prev * k + tex * (1-k), k = env color alpha
    Composite,    ///< prev * (1-tex.a) + tex * tex.a, alpha = env color alpha
    AddSigned,    ///< prev + tex - 0.5, alpha = prev alpha
    Detail,       ///< 2 * prev * tex
};

enum class TexGenMode {
    EyeLinear,
    ObjectLinear,
    SphereMap,
    ReflectionMap,
    NormalMap,
};

enum class TexCoordComponent {
    S = 0,
    T = 1,
    R = 2,
    Q = 3,
};

struct LightState {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    /// Eye-space position (transformed when it was specified)
    float position[4];
    /// Eye-space spot direction
    float spot_direction[3];
    float spot_exponent;
    float spot_cutoff;
    float constant_attenuation;
    float linear_attenuation;
    float quadratic_attenuation;
    bool enabled;
};

struct MaterialState {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float emission[4];
    float shininess;
};

struct FogState {
    bool enabled;
    FogMode mode;
    FogDistance distance;
    float density;
    float start;
    float end;
    float color[4];
};

struct TexUnitState {
    TexTarget target;
    TexEnvMode env_mode;
    float env_color[4];
    /// Whether the bound texture has an alpha channel; GL_REPLACE keeps the incoming alpha otherwise
    bool has_alpha;
    TexGenMode gen_mode[4];
    bool gen_enabled[4];
    /// Eye-space planes (multiplied by the inverse modelview when specified)
    float eye_plane[4][4];
    float object_plane[4][4];
};

/// State groups, for tracking what must be re-uploaded to a program.
enum class StateGroup {
    ModelView,
    Projection,
    TextureMatrix,
    Lights,
    Material,
    Fog,
    Texturing,
    Raster,  ///< current color, lighting/alpha-test enables, point smoothing...
    Count
};

class FixedFunctionState {
public:
    static const int kMaxLights = 8;
    static const int kMaxTextureUnits = 4;     ///< units the built-in shader can combine
    static const int kMaxTextureMatrices = 8;  ///< matches gl_MaxTextureCoords
    static const int kMaxMatrixStackDepth = 32;

    FixedFunctionState();

    void Reset();

    // --- matrices (glMatrixMode / glLoadMatrix / ...) ---
    void SetMatrixMode(MatrixMode mode);
    MatrixMode GetMatrixMode() const {
        return matrix_mode_;
    }
    /// glActiveTexture: selects the texture matrix, environment and texgen state to edit
    void SetActiveTexture(int unit);
    int GetActiveTexture() const {
        return active_texture_;
    }
    void LoadIdentity();
    void LoadMatrix(const Mat4 &m);
    void MultMatrix(const Mat4 &m);
    void Translate(float x, float y, float z);
    void Scale(float x, float y, float z);
    void Rotate(float angle_degrees, float x, float y, float z);
    /// Returns false on stack overflow / underflow, like GL_STACK_OVERFLOW
    bool PushMatrix();
    bool PopMatrix();
    const Mat4 &CurrentMatrix() const;
    const Mat4 &ModelView() const;
    const Mat4 &Projection() const;
    const Mat4 &TextureMatrix(int unit) const;
    Mat4 ModelViewProjection() const;

    // --- lighting ---
    void SetLighting(bool enabled);
    bool Lighting() const {
        return lighting_;
    }
    void EnableLight(int light, bool enabled);
    bool LightEnabled(int light) const;
    /// position is transformed by the current modelview, as in glLightfv(GL_POSITION)
    void SetLightPosition(int light, const float position[4]);
    /// direction is transformed by the upper 3x3 of the current modelview
    void SetLightSpotDirection(int light, const float direction[3]);
    void SetLightColor(int light, LightColor which, const float color[4]);
    void SetLightAttenuation(int light, LightAttenuation which, float value);
    void SetLightSpotExponent(int light, float exponent);
    void SetLightSpotCutoff(int light, float cutoff_degrees);
    const LightState &Light(int light) const;
    void SetLightModelAmbient(const float color[4]);
    const float *LightModelAmbient() const {
        return light_model_ambient_;
    }
    void SetSeparateSpecular(bool separate);
    bool SeparateSpecular() const {
        return separate_specular_;
    }
    void SetLocalViewer(bool local);
    bool LocalViewer() const {
        return local_viewer_;
    }

    // --- materials ---
    void SetMaterial(MaterialParam which, const float value[4]);
    void SetShininess(float shininess);
    const MaterialState &Material() const {
        return material_;
    }
    void SetColorMaterialEnabled(bool enabled);
    bool ColorMaterialEnabled() const {
        return color_material_enabled_;
    }
    void SetColorMaterialMode(MaterialParam mode);
    MaterialParam ColorMaterialMode() const {
        return color_material_mode_;
    }
    /**
     * The material as a shader would observe it through gl_FrontMaterial: with
     * GL_COLOR_MATERIAL enabled the tracked components follow the current color.
     */
    MaterialState EffectiveMaterial() const;

    // --- current vertex attributes ---
    void SetCurrentColor(float r, float g, float b, float a);
    const float *CurrentColor() const {
        return current_color_;
    }

    // --- fog ---
    void SetFogEnabled(bool enabled);
    void SetFogMode(FogMode mode);
    void SetFogDistance(FogDistance distance);
    void SetFogDensity(float density);
    void SetFogStart(float start);
    void SetFogEnd(float end);
    void SetFogColor(const float color[4]);
    const FogState &Fog() const {
        return fog_;
    }

    // --- alpha test ---
    void SetAlphaTestEnabled(bool enabled);
    void SetAlphaFunc(CompareFunc func, float ref);
    bool AlphaTestEnabled() const {
        return alpha_test_enabled_;
    }
    CompareFunc AlphaFunc() const {
        return alpha_func_;
    }
    float AlphaRef() const {
        return alpha_ref_;
    }

    // --- texturing (all operate on the active texture unit) ---
    /// glEnable(GL_TEXTURE_2D) and friends. Enabling one target disables none of the others
    /// in GL, but only the highest-priority enabled one is used; see EffectiveTarget().
    void SetTextureTargetEnabled(TexTarget target, bool enabled);
    bool TextureTargetEnabled(int unit, TexTarget target) const;
    /// GL's precedence: cube > 3D > rect > 2D > 1D
    TexTarget EffectiveTarget(int unit) const;
    void SetTexEnvMode(TexEnvMode mode);
    void SetTexEnvColor(const float color[4]);
    void SetTextureHasAlpha(bool has_alpha);
    void SetTexGenMode(TexCoordComponent coord, TexGenMode mode);
    void SetTexGenEnabled(TexCoordComponent coord, bool enabled);
    /// Eye planes are multiplied by the inverse of the current modelview, as in glTexGenfv(GL_EYE_PLANE)
    void SetTexGenEyePlane(TexCoordComponent coord, const float plane[4]);
    void SetTexGenObjectPlane(TexCoordComponent coord, const float plane[4]);
    const TexUnitState &TexUnit(int unit) const;

    // --- raster state ---
    void SetPointSmooth(bool enabled);
    bool PointSmooth() const {
        return point_smooth_;
    }
    void SetFlatShading(bool flat);
    bool FlatShading() const {
        return flat_shading_;
    }

    /// Monotonic serial of the last change to a group; never returns 0 after a change.
    uint64_t Version(StateGroup group) const {
        return versions_[static_cast<int>(group)];
    }

private:
    void Touch(StateGroup group);
    bool ValidLight(int light) const {
        return light >= 0 && light < kMaxLights;
    }
    std::vector<Mat4> &CurrentStack();
    const std::vector<Mat4> &CurrentStack() const;
    StateGroup CurrentMatrixGroup() const;

    MatrixMode matrix_mode_;
    int active_texture_;
    std::vector<Mat4> modelview_stack_;
    std::vector<Mat4> projection_stack_;
    std::vector<Mat4> texture_stacks_[kMaxTextureMatrices];

    bool lighting_;
    LightState lights_[kMaxLights];
    float light_model_ambient_[4];
    bool separate_specular_;
    bool local_viewer_;

    MaterialState material_;
    bool color_material_enabled_;
    MaterialParam color_material_mode_;

    float current_color_[4];

    FogState fog_;

    bool alpha_test_enabled_;
    CompareFunc alpha_func_;
    float alpha_ref_;

    TexUnitState tex_units_[kMaxTextureUnits];
    bool tex_target_enabled_[kMaxTextureUnits][6];

    bool point_smooth_;
    bool flat_shading_;

    uint64_t versions_[static_cast<int>(StateGroup::Count)];
    uint64_t serial_;
};

} // namespace vsgl

#endif //VEGA_STRIKE_ENGINE_GLDRV_GL_FFP_STATE_H
