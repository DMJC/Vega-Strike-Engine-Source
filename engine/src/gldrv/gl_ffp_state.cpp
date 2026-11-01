/*
 * gl_ffp_state.cpp
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

#include "gl_ffp_state.h"

#include <cmath>
#include <cstring>

namespace vsgl {

namespace {

const double kPi = 3.14159265358979323846;

void Copy4(float dst[4], const float src[4]) {
    std::memcpy(dst, src, 4 * sizeof(float));
}

void Set4(float dst[4], float a, float b, float c, float d) {
    dst[0] = a;
    dst[1] = b;
    dst[2] = c;
    dst[3] = d;
}

int TargetIndex(TexTarget target) {
    return static_cast<int>(target);
}

} // namespace

// ---------------------------------------------------------------------------
// Mat4
// ---------------------------------------------------------------------------

Mat4 Mat4::Identity() {
    Mat4 r;
    std::memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::FromFloats(const float *column_major) {
    Mat4 r;
    std::memcpy(r.m, column_major, sizeof(r.m));
    return r;
}

Mat4 Mat4::FromDoubles(const double *column_major) {
    Mat4 r;
    for (int i = 0; i < 16; ++i) {
        r.m[i] = static_cast<float>(column_major[i]);
    }
    return r;
}

Mat4 Mat4::Translation(float x, float y, float z) {
    Mat4 r = Identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

Mat4 Mat4::Scaling(float x, float y, float z) {
    Mat4 r = Identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

Mat4 Mat4::Rotation(float angle_degrees, float x, float y, float z) {
    const double len = std::sqrt(double(x) * x + double(y) * y + double(z) * z);
    if (len == 0.0) {
        return Identity();
    }
    const double nx = x / len, ny = y / len, nz = z / len;
    const double a = angle_degrees * kPi / 180.0;
    const double c = std::cos(a), s = std::sin(a), t = 1.0 - c;
    Mat4 r = Identity();
    r(0, 0) = float(nx * nx * t + c);
    r(0, 1) = float(nx * ny * t - nz * s);
    r(0, 2) = float(nx * nz * t + ny * s);
    r(1, 0) = float(ny * nx * t + nz * s);
    r(1, 1) = float(ny * ny * t + c);
    r(1, 2) = float(ny * nz * t - nx * s);
    r(2, 0) = float(nz * nx * t - ny * s);
    r(2, 1) = float(nz * ny * t + nx * s);
    r(2, 2) = float(nz * nz * t + c);
    return r;
}

Mat4 Mat4::Ortho(float left, float right, float bottom, float top, float znear, float zfar) {
    Mat4 r = Identity();
    r(0, 0) = 2.0f / (right - left);
    r(1, 1) = 2.0f / (top - bottom);
    r(2, 2) = -2.0f / (zfar - znear);
    r(0, 3) = -(right + left) / (right - left);
    r(1, 3) = -(top + bottom) / (top - bottom);
    r(2, 3) = -(zfar + znear) / (zfar - znear);
    return r;
}

Mat4 Mat4::Frustum(float left, float right, float bottom, float top, float znear, float zfar) {
    Mat4 r;
    std::memset(r.m, 0, sizeof(r.m));
    r(0, 0) = 2.0f * znear / (right - left);
    r(1, 1) = 2.0f * znear / (top - bottom);
    r(0, 2) = (right + left) / (right - left);
    r(1, 2) = (top + bottom) / (top - bottom);
    r(2, 2) = -(zfar + znear) / (zfar - znear);
    r(2, 3) = -2.0f * zfar * znear / (zfar - znear);
    r(3, 2) = -1.0f;
    return r;
}

bool Mat4::operator==(const Mat4 &o) const {
    return std::memcmp(m, o.m, sizeof(m)) == 0;
}

Mat4 operator*(const Mat4 &a, const Mat4 &b) {
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += double(a(row, k)) * double(b(k, col));
            }
            r(row, col) = static_cast<float>(sum);
        }
    }
    return r;
}

Mat4 Transpose(const Mat4 &a) {
    Mat4 r;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r(row, col) = a(col, row);
        }
    }
    return r;
}

bool Invert(const Mat4 &a, Mat4 &out) {
    // Cofactor expansion in double precision; the engine's modelview carries a
    // 1/1024 scale, so float precision is not quite enough here.
    double m[16], inv[16];
    for (int i = 0; i < 16; ++i) {
        m[i] = a.m[i];
    }
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14]
            + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14]
            - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13]
            + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13]
            - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14]
            - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14]
            + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13]
            - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13]
            + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14]
            + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14]
            - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13]
            + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13]
            - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10]
            - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10]
            + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9]
            - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9]
            + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0.0 || !std::isfinite(det)) {
        out = Mat4::Identity();
        return false;
    }
    const double inv_det = 1.0 / det;
    for (int i = 0; i < 16; ++i) {
        out.m[i] = static_cast<float>(inv[i] * inv_det);
    }
    return true;
}

void TransformPoint(const Mat4 &a, const float v[4], float out[4]) {
    float r[4];
    for (int row = 0; row < 4; ++row) {
        r[row] = a(row, 0) * v[0] + a(row, 1) * v[1] + a(row, 2) * v[2] + a(row, 3) * v[3];
    }
    Copy4(out, r);
}

void NormalMatrix(const Mat4 &modelview, float out[9]) {
    // inverse-transpose of the upper 3x3, i.e. cofactor matrix / determinant
    double a[3][3];
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            a[row][col] = modelview(row, col);
        }
    }
    double cof[3][3];
    cof[0][0] = a[1][1] * a[2][2] - a[1][2] * a[2][1];
    cof[0][1] = -(a[1][0] * a[2][2] - a[1][2] * a[2][0]);
    cof[0][2] = a[1][0] * a[2][1] - a[1][1] * a[2][0];
    cof[1][0] = -(a[0][1] * a[2][2] - a[0][2] * a[2][1]);
    cof[1][1] = a[0][0] * a[2][2] - a[0][2] * a[2][0];
    cof[1][2] = -(a[0][0] * a[2][1] - a[0][1] * a[2][0]);
    cof[2][0] = a[0][1] * a[1][2] - a[0][2] * a[1][1];
    cof[2][1] = -(a[0][0] * a[1][2] - a[0][2] * a[1][0]);
    cof[2][2] = a[0][0] * a[1][1] - a[0][1] * a[1][0];
    const double det = a[0][0] * cof[0][0] + a[0][1] * cof[0][1] + a[0][2] * cof[0][2];
    const double inv_det = (det == 0.0) ? 0.0 : 1.0 / det;
    // (A^-1)^T = cof / det; store column-major
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out[col * 3 + row] = static_cast<float>(cof[row][col] * inv_det);
        }
    }
}

// ---------------------------------------------------------------------------
// FixedFunctionState
// ---------------------------------------------------------------------------

const int FixedFunctionState::kMaxLights;
const int FixedFunctionState::kMaxTextureUnits;
const int FixedFunctionState::kMaxTextureMatrices;
const int FixedFunctionState::kMaxMatrixStackDepth;

FixedFunctionState::FixedFunctionState() : serial_(0) {
    Reset();
}

void FixedFunctionState::Reset() {
    matrix_mode_ = MatrixMode::ModelView;
    active_texture_ = 0;
    modelview_stack_.assign(1, Mat4::Identity());
    projection_stack_.assign(1, Mat4::Identity());
    for (auto &stack : texture_stacks_) {
        stack.assign(1, Mat4::Identity());
    }

    // Defaults from the GL 2.1 specification, table 6.9 onwards
    lighting_ = false;
    for (int i = 0; i < kMaxLights; ++i) {
        LightState &l = lights_[i];
        Set4(l.ambient, 0, 0, 0, 1);
        if (i == 0) {
            Set4(l.diffuse, 1, 1, 1, 1);
            Set4(l.specular, 1, 1, 1, 1);
        } else {
            Set4(l.diffuse, 0, 0, 0, 1);
            Set4(l.specular, 0, 0, 0, 1);
        }
        Set4(l.position, 0, 0, 1, 0);
        l.spot_direction[0] = 0;
        l.spot_direction[1] = 0;
        l.spot_direction[2] = -1;
        l.spot_exponent = 0;
        l.spot_cutoff = 180;
        l.constant_attenuation = 1;
        l.linear_attenuation = 0;
        l.quadratic_attenuation = 0;
        l.enabled = false;
    }
    Set4(light_model_ambient_, 0.2f, 0.2f, 0.2f, 1.0f);
    separate_specular_ = false;
    local_viewer_ = false;

    Set4(material_.ambient, 0.2f, 0.2f, 0.2f, 1.0f);
    Set4(material_.diffuse, 0.8f, 0.8f, 0.8f, 1.0f);
    Set4(material_.specular, 0, 0, 0, 1);
    Set4(material_.emission, 0, 0, 0, 1);
    material_.shininess = 0;
    color_material_enabled_ = false;
    color_material_mode_ = MaterialParam::AmbientAndDiffuse;

    Set4(current_color_, 1, 1, 1, 1);

    fog_.enabled = false;
    fog_.mode = FogMode::Exp;
    fog_.distance = FogDistance::EyePlaneAbsolute;
    fog_.density = 1;
    fog_.start = 0;
    fog_.end = 1;
    Set4(fog_.color, 0, 0, 0, 0);

    alpha_test_enabled_ = false;
    alpha_func_ = CompareFunc::AlwaysPass;
    alpha_ref_ = 0;

    for (int u = 0; u < kMaxTextureUnits; ++u) {
        TexUnitState &t = tex_units_[u];
        t.target = TexTarget::Off;
        t.env_mode = TexEnvMode::Modulate;
        Set4(t.env_color, 0, 0, 0, 0);
        t.has_alpha = true;
        for (int c = 0; c < 4; ++c) {
            t.gen_mode[c] = TexGenMode::EyeLinear;
            t.gen_enabled[c] = false;
            Set4(t.eye_plane[c], 0, 0, 0, 0);
            Set4(t.object_plane[c], 0, 0, 0, 0);
        }
        // S and T planes default to x and y
        t.eye_plane[0][0] = t.object_plane[0][0] = 1;
        t.eye_plane[1][1] = t.object_plane[1][1] = 1;
        for (bool &enabled : tex_target_enabled_[u]) {
            enabled = false;
        }
    }

    point_smooth_ = false;
    flat_shading_ = false;

    for (int g = 0; g < static_cast<int>(StateGroup::Count); ++g) {
        versions_[g] = ++serial_;
    }
}

void FixedFunctionState::Touch(StateGroup group) {
    versions_[static_cast<int>(group)] = ++serial_;
}

std::vector<Mat4> &FixedFunctionState::CurrentStack() {
    switch (matrix_mode_) {
        case MatrixMode::Projection:
            return projection_stack_;
        case MatrixMode::Texture:
            if (active_texture_ >= 0 && active_texture_ < kMaxTextureMatrices) {
                return texture_stacks_[active_texture_];
            }
            // Out-of-range units have no matrix; edits land on unit 0's like a no-op would.
            return texture_stacks_[0];
        case MatrixMode::ModelView:
        default:
            return modelview_stack_;
    }
}

const std::vector<Mat4> &FixedFunctionState::CurrentStack() const {
    return const_cast<FixedFunctionState *>(this)->CurrentStack();
}

StateGroup FixedFunctionState::CurrentMatrixGroup() const {
    switch (matrix_mode_) {
        case MatrixMode::Projection:
            return StateGroup::Projection;
        case MatrixMode::Texture:
            return StateGroup::TextureMatrix;
        case MatrixMode::ModelView:
        default:
            return StateGroup::ModelView;
    }
}

void FixedFunctionState::SetMatrixMode(MatrixMode mode) {
    matrix_mode_ = mode;
}

void FixedFunctionState::SetActiveTexture(int unit) {
    active_texture_ = unit;
}

void FixedFunctionState::LoadIdentity() {
    CurrentStack().back() = Mat4::Identity();
    Touch(CurrentMatrixGroup());
}

void FixedFunctionState::LoadMatrix(const Mat4 &m) {
    CurrentStack().back() = m;
    Touch(CurrentMatrixGroup());
}

void FixedFunctionState::MultMatrix(const Mat4 &m) {
    Mat4 &top = CurrentStack().back();
    top = top * m;
    Touch(CurrentMatrixGroup());
}

void FixedFunctionState::Translate(float x, float y, float z) {
    MultMatrix(Mat4::Translation(x, y, z));
}

void FixedFunctionState::Scale(float x, float y, float z) {
    MultMatrix(Mat4::Scaling(x, y, z));
}

void FixedFunctionState::Rotate(float angle_degrees, float x, float y, float z) {
    MultMatrix(Mat4::Rotation(angle_degrees, x, y, z));
}

bool FixedFunctionState::PushMatrix() {
    std::vector<Mat4> &stack = CurrentStack();
    if (static_cast<int>(stack.size()) >= kMaxMatrixStackDepth) {
        return false;
    }
    stack.push_back(stack.back());
    return true;
}

bool FixedFunctionState::PopMatrix() {
    std::vector<Mat4> &stack = CurrentStack();
    if (stack.size() <= 1) {
        return false;
    }
    stack.pop_back();
    Touch(CurrentMatrixGroup());
    return true;
}

const Mat4 &FixedFunctionState::CurrentMatrix() const {
    return CurrentStack().back();
}

const Mat4 &FixedFunctionState::ModelView() const {
    return modelview_stack_.back();
}

const Mat4 &FixedFunctionState::Projection() const {
    return projection_stack_.back();
}

const Mat4 &FixedFunctionState::TextureMatrix(int unit) const {
    if (unit < 0 || unit >= kMaxTextureMatrices) {
        return texture_stacks_[0].back();
    }
    return texture_stacks_[unit].back();
}

Mat4 FixedFunctionState::ModelViewProjection() const {
    return Projection() * ModelView();
}

void FixedFunctionState::SetLighting(bool enabled) {
    if (lighting_ != enabled) {
        lighting_ = enabled;
        Touch(StateGroup::Raster);
    }
}

void FixedFunctionState::EnableLight(int light, bool enabled) {
    if (ValidLight(light) && lights_[light].enabled != enabled) {
        lights_[light].enabled = enabled;
        Touch(StateGroup::Lights);
    }
}

bool FixedFunctionState::LightEnabled(int light) const {
    return ValidLight(light) && lights_[light].enabled;
}

void FixedFunctionState::SetLightPosition(int light, const float position[4]) {
    if (!ValidLight(light)) {
        return;
    }
    TransformPoint(ModelView(), position, lights_[light].position);
    Touch(StateGroup::Lights);
}

void FixedFunctionState::SetLightSpotDirection(int light, const float direction[3]) {
    if (!ValidLight(light)) {
        return;
    }
    const float v[4] = {direction[0], direction[1], direction[2], 0.0f};
    float out[4];
    TransformPoint(ModelView(), v, out);
    lights_[light].spot_direction[0] = out[0];
    lights_[light].spot_direction[1] = out[1];
    lights_[light].spot_direction[2] = out[2];
    Touch(StateGroup::Lights);
}

void FixedFunctionState::SetLightColor(int light, LightColor which, const float color[4]) {
    if (!ValidLight(light)) {
        return;
    }
    switch (which) {
        case LightColor::Ambient:
            Copy4(lights_[light].ambient, color);
            break;
        case LightColor::Diffuse:
            Copy4(lights_[light].diffuse, color);
            break;
        case LightColor::Specular:
            Copy4(lights_[light].specular, color);
            break;
    }
    Touch(StateGroup::Lights);
}

void FixedFunctionState::SetLightAttenuation(int light, LightAttenuation which, float value) {
    if (!ValidLight(light)) {
        return;
    }
    switch (which) {
        case LightAttenuation::Constant:
            lights_[light].constant_attenuation = value;
            break;
        case LightAttenuation::Linear:
            lights_[light].linear_attenuation = value;
            break;
        case LightAttenuation::Quadratic:
            lights_[light].quadratic_attenuation = value;
            break;
    }
    Touch(StateGroup::Lights);
}

void FixedFunctionState::SetLightSpotExponent(int light, float exponent) {
    if (ValidLight(light)) {
        lights_[light].spot_exponent = exponent;
        Touch(StateGroup::Lights);
    }
}

void FixedFunctionState::SetLightSpotCutoff(int light, float cutoff_degrees) {
    if (ValidLight(light)) {
        lights_[light].spot_cutoff = cutoff_degrees;
        Touch(StateGroup::Lights);
    }
}

const LightState &FixedFunctionState::Light(int light) const {
    return lights_[ValidLight(light) ? light : 0];
}

void FixedFunctionState::SetLightModelAmbient(const float color[4]) {
    Copy4(light_model_ambient_, color);
    // The light model ambient feeds gl_FrontLightModelProduct, a material product
    Touch(StateGroup::Lights);
    Touch(StateGroup::Material);
}

void FixedFunctionState::SetSeparateSpecular(bool separate) {
    if (separate_specular_ != separate) {
        separate_specular_ = separate;
        Touch(StateGroup::Lights);
    }
}

void FixedFunctionState::SetLocalViewer(bool local) {
    if (local_viewer_ != local) {
        local_viewer_ = local;
        Touch(StateGroup::Lights);
    }
}

void FixedFunctionState::SetMaterial(MaterialParam which, const float value[4]) {
    switch (which) {
        case MaterialParam::Ambient:
            Copy4(material_.ambient, value);
            break;
        case MaterialParam::Diffuse:
            Copy4(material_.diffuse, value);
            break;
        case MaterialParam::Specular:
            Copy4(material_.specular, value);
            break;
        case MaterialParam::Emission:
            Copy4(material_.emission, value);
            break;
        case MaterialParam::AmbientAndDiffuse:
            Copy4(material_.ambient, value);
            Copy4(material_.diffuse, value);
            break;
    }
    Touch(StateGroup::Material);
}

void FixedFunctionState::SetShininess(float shininess) {
    material_.shininess = shininess;
    Touch(StateGroup::Material);
}

void FixedFunctionState::SetColorMaterialEnabled(bool enabled) {
    if (color_material_enabled_ != enabled) {
        color_material_enabled_ = enabled;
        Touch(StateGroup::Material);
    }
}

void FixedFunctionState::SetColorMaterialMode(MaterialParam mode) {
    if (color_material_mode_ != mode) {
        color_material_mode_ = mode;
        Touch(StateGroup::Material);
    }
}

MaterialState FixedFunctionState::EffectiveMaterial() const {
    MaterialState m = material_;
    if (!color_material_enabled_) {
        return m;
    }
    switch (color_material_mode_) {
        case MaterialParam::Ambient:
            Copy4(m.ambient, current_color_);
            break;
        case MaterialParam::Diffuse:
            Copy4(m.diffuse, current_color_);
            break;
        case MaterialParam::Specular:
            Copy4(m.specular, current_color_);
            break;
        case MaterialParam::Emission:
            Copy4(m.emission, current_color_);
            break;
        case MaterialParam::AmbientAndDiffuse:
            Copy4(m.ambient, current_color_);
            Copy4(m.diffuse, current_color_);
            break;
    }
    return m;
}

void FixedFunctionState::SetCurrentColor(float r, float g, float b, float a) {
    if (current_color_[0] == r && current_color_[1] == g && current_color_[2] == b && current_color_[3] == a) {
        return;
    }
    Set4(current_color_, r, g, b, a);
    Touch(StateGroup::Raster);
    if (color_material_enabled_) {
        Touch(StateGroup::Material);
    }
}

void FixedFunctionState::SetFogEnabled(bool enabled) {
    if (fog_.enabled != enabled) {
        fog_.enabled = enabled;
        Touch(StateGroup::Fog);
    }
}

void FixedFunctionState::SetFogMode(FogMode mode) {
    fog_.mode = mode;
    Touch(StateGroup::Fog);
}

void FixedFunctionState::SetFogDistance(FogDistance distance) {
    fog_.distance = distance;
    Touch(StateGroup::Fog);
}

void FixedFunctionState::SetFogDensity(float density) {
    fog_.density = density;
    Touch(StateGroup::Fog);
}

void FixedFunctionState::SetFogStart(float start) {
    fog_.start = start;
    Touch(StateGroup::Fog);
}

void FixedFunctionState::SetFogEnd(float end) {
    fog_.end = end;
    Touch(StateGroup::Fog);
}

void FixedFunctionState::SetFogColor(const float color[4]) {
    Copy4(fog_.color, color);
    Touch(StateGroup::Fog);
}

void FixedFunctionState::SetAlphaTestEnabled(bool enabled) {
    if (alpha_test_enabled_ != enabled) {
        alpha_test_enabled_ = enabled;
        Touch(StateGroup::Raster);
    }
}

void FixedFunctionState::SetAlphaFunc(CompareFunc func, float ref) {
    // GL clamps the reference value to [0, 1]
    if (ref < 0.0f) {
        ref = 0.0f;
    } else if (ref > 1.0f) {
        ref = 1.0f;
    }
    alpha_func_ = func;
    alpha_ref_ = ref;
    Touch(StateGroup::Raster);
}

void FixedFunctionState::SetTextureTargetEnabled(TexTarget target, bool enabled) {
    if (target == TexTarget::Off || active_texture_ < 0 || active_texture_ >= kMaxTextureUnits) {
        return;
    }
    bool &flag = tex_target_enabled_[active_texture_][TargetIndex(target)];
    if (flag != enabled) {
        flag = enabled;
        tex_units_[active_texture_].target = EffectiveTarget(active_texture_);
        Touch(StateGroup::Texturing);
    }
}

bool FixedFunctionState::TextureTargetEnabled(int unit, TexTarget target) const {
    if (unit < 0 || unit >= kMaxTextureUnits || target == TexTarget::Off) {
        return false;
    }
    return tex_target_enabled_[unit][TargetIndex(target)];
}

TexTarget FixedFunctionState::EffectiveTarget(int unit) const {
    if (unit < 0 || unit >= kMaxTextureUnits) {
        return TexTarget::Off;
    }
    const bool *e = tex_target_enabled_[unit];
    if (e[TargetIndex(TexTarget::Cube)]) {
        return TexTarget::Cube;
    }
    if (e[TargetIndex(TexTarget::Tex3D)]) {
        return TexTarget::Tex3D;
    }
    if (e[TargetIndex(TexTarget::Rect)]) {
        return TexTarget::Rect;
    }
    if (e[TargetIndex(TexTarget::Tex2D)]) {
        return TexTarget::Tex2D;
    }
    if (e[TargetIndex(TexTarget::Tex1D)]) {
        return TexTarget::Tex1D;
    }
    return TexTarget::Off;
}

void FixedFunctionState::SetTexEnvMode(TexEnvMode mode) {
    if (active_texture_ < 0 || active_texture_ >= kMaxTextureUnits) {
        return;
    }
    tex_units_[active_texture_].env_mode = mode;
    Touch(StateGroup::Texturing);
}

void FixedFunctionState::SetTexEnvColor(const float color[4]) {
    if (active_texture_ < 0 || active_texture_ >= kMaxTextureUnits) {
        return;
    }
    Copy4(tex_units_[active_texture_].env_color, color);
    Touch(StateGroup::Texturing);
}

void FixedFunctionState::SetTextureHasAlpha(bool has_alpha) {
    if (active_texture_ < 0 || active_texture_ >= kMaxTextureUnits) {
        return;
    }
    if (tex_units_[active_texture_].has_alpha != has_alpha) {
        tex_units_[active_texture_].has_alpha = has_alpha;
        Touch(StateGroup::Texturing);
    }
}

void FixedFunctionState::SetTexGenMode(TexCoordComponent coord, TexGenMode mode) {
    if (active_texture_ < 0 || active_texture_ >= kMaxTextureUnits) {
        return;
    }
    tex_units_[active_texture_].gen_mode[static_cast<int>(coord)] = mode;
    Touch(StateGroup::Texturing);
}

void FixedFunctionState::SetTexGenEnabled(TexCoordComponent coord, bool enabled) {
    if (active_texture_ < 0 || active_texture_ >= kMaxTextureUnits) {
        return;
    }
    bool &flag = tex_units_[active_texture_].gen_enabled[static_cast<int>(coord)];
    if (flag != enabled) {
        flag = enabled;
        Touch(StateGroup::Texturing);
    }
}

void FixedFunctionState::SetTexGenEyePlane(TexCoordComponent coord, const float plane[4]) {
    if (active_texture_ < 0 || active_texture_ >= kMaxTextureUnits) {
        return;
    }
    // p_eye = p * M^-1, i.e. (M^-1)^T * p as a column vector
    Mat4 inv;
    Invert(ModelView(), inv);
    TransformPoint(Transpose(inv), plane, tex_units_[active_texture_].eye_plane[static_cast<int>(coord)]);
    Touch(StateGroup::Texturing);
}

void FixedFunctionState::SetTexGenObjectPlane(TexCoordComponent coord, const float plane[4]) {
    if (active_texture_ < 0 || active_texture_ >= kMaxTextureUnits) {
        return;
    }
    Copy4(tex_units_[active_texture_].object_plane[static_cast<int>(coord)], plane);
    Touch(StateGroup::Texturing);
}

const TexUnitState &FixedFunctionState::TexUnit(int unit) const {
    if (unit < 0 || unit >= kMaxTextureUnits) {
        return tex_units_[0];
    }
    return tex_units_[unit];
}

void FixedFunctionState::SetPointSmooth(bool enabled) {
    if (point_smooth_ != enabled) {
        point_smooth_ = enabled;
        Touch(StateGroup::Raster);
    }
}

void FixedFunctionState::SetFlatShading(bool flat) {
    if (flat_shading_ != flat) {
        flat_shading_ = flat;
        Touch(StateGroup::Raster);
    }
}

} // namespace vsgl
