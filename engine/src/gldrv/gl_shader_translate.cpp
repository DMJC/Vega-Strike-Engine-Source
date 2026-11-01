/*
 * gl_shader_translate.cpp
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

#include "gl_shader_translate.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <vector>

namespace vsgl {

namespace {

const int kVertexStage = 1;
const int kFragmentStage = 2;
const int kBothStages = kVertexStage | kFragmentStage;

int StageBit(ShaderStage stage) {
    return stage == ShaderStage::Vertex ? kVertexStage : kFragmentStage;
}

struct Rename {
    const char *from;
    const char *to;
    int stages;
};

// Identifiers (variables, types, constants) renamed wherever they appear.
const Rename kIdentifierRenames[] = {
        // transformation state
        {"gl_ModelViewMatrix", "vsc_ModelViewMatrix", kBothStages},
        {"gl_ModelViewMatrixInverse", "vsc_ModelViewMatrixInverse", kBothStages},
        {"gl_ModelViewMatrixTranspose", "vsc_ModelViewMatrixTranspose", kBothStages},
        {"gl_ModelViewMatrixInverseTranspose", "vsc_ModelViewMatrixInverseTranspose", kBothStages},
        {"gl_ProjectionMatrix", "vsc_ProjectionMatrix", kBothStages},
        {"gl_ProjectionMatrixInverse", "vsc_ProjectionMatrixInverse", kBothStages},
        {"gl_ProjectionMatrixTranspose", "vsc_ProjectionMatrixTranspose", kBothStages},
        {"gl_ProjectionMatrixInverseTranspose", "vsc_ProjectionMatrixInverseTranspose", kBothStages},
        {"gl_ModelViewProjectionMatrix", "vsc_ModelViewProjectionMatrix", kBothStages},
        {"gl_ModelViewProjectionMatrixInverse", "vsc_ModelViewProjectionMatrixInverse", kBothStages},
        {"gl_ModelViewProjectionMatrixTranspose", "vsc_ModelViewProjectionMatrixTranspose", kBothStages},
        {"gl_ModelViewProjectionMatrixInverseTranspose", "vsc_ModelViewProjectionMatrixInverseTranspose",
                kBothStages},
        {"gl_TextureMatrix", "vsc_TextureMatrix", kBothStages},
        {"gl_TextureMatrixInverse", "vsc_TextureMatrixInverse", kBothStages},
        {"gl_TextureMatrixTranspose", "vsc_TextureMatrixTranspose", kBothStages},
        {"gl_TextureMatrixInverseTranspose", "vsc_TextureMatrixInverseTranspose", kBothStages},
        {"gl_NormalMatrix", "vsc_NormalMatrix", kBothStages},
        {"gl_NormalScale", "vsc_NormalScale", kBothStages},
        // lighting state
        {"gl_LightSourceParameters", "vsc_LightSourceParameters", kBothStages},
        {"gl_LightSource", "vsc_LightSource", kBothStages},
        {"gl_MaterialParameters", "vsc_MaterialParameters", kBothStages},
        {"gl_FrontMaterial", "vsc_FrontMaterial", kBothStages},
        {"gl_BackMaterial", "vsc_BackMaterial", kBothStages},
        {"gl_LightModelParameters", "vsc_LightModelParameters", kBothStages},
        {"gl_LightModel", "vsc_LightModel", kBothStages},
        {"gl_LightModelProducts", "vsc_LightModelProducts", kBothStages},
        {"gl_FrontLightModelProduct", "vsc_FrontLightModelProduct", kBothStages},
        {"gl_BackLightModelProduct", "vsc_BackLightModelProduct", kBothStages},
        {"gl_LightProducts", "vsc_LightProducts", kBothStages},
        {"gl_FrontLightProduct", "vsc_FrontLightProduct", kBothStages},
        {"gl_BackLightProduct", "vsc_BackLightProduct", kBothStages},
        // texturing and fog state
        {"gl_TextureEnvColor", "vsc_TextureEnvColor", kBothStages},
        {"gl_EyePlaneS", "vsc_EyePlaneS", kBothStages},
        {"gl_EyePlaneT", "vsc_EyePlaneT", kBothStages},
        {"gl_EyePlaneR", "vsc_EyePlaneR", kBothStages},
        {"gl_EyePlaneQ", "vsc_EyePlaneQ", kBothStages},
        {"gl_ObjectPlaneS", "vsc_ObjectPlaneS", kBothStages},
        {"gl_ObjectPlaneT", "vsc_ObjectPlaneT", kBothStages},
        {"gl_ObjectPlaneR", "vsc_ObjectPlaneR", kBothStages},
        {"gl_ObjectPlaneQ", "vsc_ObjectPlaneQ", kBothStages},
        {"gl_FogParameters", "vsc_FogParameters", kBothStages},
        {"gl_Fog", "vsc_Fog", kBothStages},
        {"gl_ClipPlane", "vsc_ClipPlane", kBothStages},
        // implementation constants
        {"gl_MaxLights", "vsc_MaxLights", kBothStages},
        {"gl_MaxTextureCoords", "vsc_MaxTextureCoords", kBothStages},
        {"gl_MaxTextureUnits", "vsc_MaxTextureUnits", kBothStages},
        {"gl_MaxClipPlanes", "vsc_MaxClipPlanes", kBothStages},
        // vertex attributes
        {"gl_Vertex", "vsc_Vertex", kVertexStage},
        {"gl_Normal", "vsc_Normal", kVertexStage},
        {"gl_Color", "vsc_Color", kVertexStage},
        {"gl_SecondaryColor", "vsc_SecondaryColor", kVertexStage},
        {"gl_FogCoord", "vsc_FogCoord", kVertexStage},
        {"gl_MultiTexCoord0", "vsc_MultiTexCoord0", kVertexStage},
        {"gl_MultiTexCoord1", "vsc_MultiTexCoord1", kVertexStage},
        {"gl_MultiTexCoord2", "vsc_MultiTexCoord2", kVertexStage},
        {"gl_MultiTexCoord3", "vsc_MultiTexCoord3", kVertexStage},
        {"gl_MultiTexCoord4", "vsc_MultiTexCoord4", kVertexStage},
        {"gl_MultiTexCoord5", "vsc_MultiTexCoord5", kVertexStage},
        {"gl_MultiTexCoord6", "vsc_MultiTexCoord6", kVertexStage},
        {"gl_MultiTexCoord7", "vsc_MultiTexCoord7", kVertexStage},
        // varyings
        {"gl_FrontColor", "vsc_FrontColor", kVertexStage},
        {"gl_BackColor", "vsc_BackColor", kVertexStage},
        {"gl_FrontSecondaryColor", "vsc_FrontSecondaryColor", kVertexStage},
        {"gl_BackSecondaryColor", "vsc_BackSecondaryColor", kVertexStage},
        {"gl_TexCoord", "vsc_TexCoord", kBothStages},
        {"gl_FogFragCoord", "vsc_FogFragCoord", kBothStages},
        {"gl_ClipVertex", "vsc_ClipVertex", kVertexStage},
        // In the fragment stage gl_Color is the (clamped) interpolated front color
        {"gl_Color", "(clamp(vsc_FrontColor, 0.0, 1.0))", kFragmentStage},
        {"gl_SecondaryColor", "(clamp(vsc_FrontSecondaryColor, 0.0, 1.0))", kFragmentStage},
        // fragment outputs
        {"gl_FragColor", "vsc_FragColor", kFragmentStage},
        {"gl_FragData", "vsc_FragData", kFragmentStage},
};

// Legacy functions, mapped with preprocessor macros rather than renamed, so
// that the preprocessor resolves fplod.h-style conditional fallbacks exactly
// as the compiler sees them. Only names that occur in a shader get a macro.
const Rename kFunctionMacros[] = {
        {"ftransform", "vsc_ftransform", kVertexStage},
        {"texture1D", "texture", kBothStages},
        {"texture2D", "texture", kBothStages},
        {"texture3D", "texture", kBothStages},
        {"textureCube", "texture", kBothStages},
        {"texture2DRect", "texture", kBothStages},
        {"texture1DProj", "textureProj", kBothStages},
        {"texture2DProj", "textureProj", kBothStages},
        {"texture3DProj", "textureProj", kBothStages},
        {"texture2DRectProj", "textureProj", kBothStages},
        {"texture1DLod", "textureLod", kBothStages},
        {"texture2DLod", "textureLod", kBothStages},
        {"texture3DLod", "textureLod", kBothStages},
        {"textureCubeLod", "textureLod", kBothStages},
        {"texture1DProjLod", "textureProjLod", kBothStages},
        {"texture2DProjLod", "textureProjLod", kBothStages},
        {"texture3DProjLod", "textureProjLod", kBothStages},
        {"texture1DGradARB", "textureGrad", kBothStages},
        {"texture2DGradARB", "textureGrad", kBothStages},
        {"texture3DGradARB", "textureGrad", kBothStages},
        {"textureCubeGradARB", "textureGrad", kBothStages},
        {"texture2DRectGradARB", "textureGrad", kBothStages},
        {"texture1DProjGradARB", "textureProjGrad", kBothStages},
        {"texture2DProjGradARB", "textureProjGrad", kBothStages},
        {"texture3DProjGradARB", "textureProjGrad", kBothStages},
        {"shadow1D", "vsc_shadow", kBothStages},
        {"shadow2D", "vsc_shadow", kBothStages},
        {"shadow2DRect", "vsc_shadow", kBothStages},
        {"shadow1DProj", "vsc_shadowProj", kBothStages},
        {"shadow2DProj", "vsc_shadowProj", kBothStages},
};

// Words that were ordinary identifiers in old GLSL but are reserved or
// keywords in 4.x, with the version that took them away.
struct Reserved {
    const char *word;
    int reserved_since;
};

const Reserved kReservedWords[] = {
        {"texture", 130},  // the core texture() builtin
        {"smooth", 130},
        {"flat", 130},
        {"noperspective", 130},
        {"uint", 130},
        {"layout", 140},
        {"sample", 400},
        {"patch", 400},
        {"subroutine", 400},
        {"precise", 400},
        {"buffer", 430},
        {"shared", 430},
        {"coherent", 420},
        {"volatile", 420},
        {"restrict", 420},
        {"readonly", 420},
        {"writeonly", 420},
        {"common", 9999},  // reserved for future use in every version
        {"partition", 9999},
        {"active", 9999},
        {"filter", 9999},
        {"resource", 9999},
        {"input", 9999},
        {"output", 9999},
        {"superp", 9999},
};

// Built-in functions added after GLSL 1.10, with the version that added
// them. A legacy shader may define functions with these names; core GLSL
// forbids redefining built-ins, so those user functions get renamed.
const Reserved kNewerBuiltinFunctions[] = {
        {"outerProduct", 120},
        {"transpose", 120},
        {"sinh", 130},
        {"cosh", 130},
        {"tanh", 130},
        {"asinh", 130},
        {"acosh", 130},
        {"atanh", 130},
        {"trunc", 130},
        {"round", 130},
        {"roundEven", 130},
        {"modf", 130},
        {"isnan", 130},
        {"isinf", 130},
        {"textureSize", 130},
        {"texelFetch", 130},
        {"textureOffset", 130},
        {"textureProj", 130},
        {"textureLod", 130},
        {"textureGrad", 130},
        {"textureProjLod", 130},
        {"textureProjGrad", 130},
        {"inverse", 140},
        {"determinant", 150},
        {"fma", 400},
        {"frexp", 400},
        {"ldexp", 400},
        {"bitfieldExtract", 400},
        {"bitfieldInsert", 400},
        {"bitCount", 400},
        {"findLSB", 400},
        {"findMSB", 400},
        {"packUnorm2x16", 400},
        {"unpackUnorm2x16", 400},
        {"packHalf2x16", 420},
        {"unpackHalf2x16", 420},
};

// Extensions whose functionality is core in 4.x; their #extension lines go.
const char *const kCoreExtensions[] = {
        "GL_ARB_shader_texture_lod",
        "GL_ATI_shader_texture_lod",
        "GL_EXT_gpu_shader4",
        "GL_ARB_texture_rectangle",
        "GL_ARB_draw_buffers",
        "GL_ATI_draw_buffers",
        "GL_EXT_texture_array",
        "GL_ARB_shading_language_120",
};

bool IsIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool IsIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

enum class TokenKind {
    Identifier,
    Other,
};

struct Token {
    TokenKind kind;
    size_t begin;
    size_t end;
};

/**
 * Splits a source into identifiers and everything else (copied verbatim).
 * Comments and numbers are never reported as identifiers.
 */
std::vector<Token> Tokenize(const std::string &s) {
    std::vector<Token> tokens;
    size_t i = 0;
    const size_t n = s.size();
    size_t other_begin = 0;
    auto flush_other = [&](size_t upto) {
        if (upto > other_begin) {
            tokens.push_back(Token{TokenKind::Other, other_begin, upto});
        }
    };
    while (i < n) {
        const char c = s[i];
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            while (i < n && s[i] != '\n') {
                ++i;
            }
        } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) {
                ++i;
            }
            i = std::min(n, i + 2);
        } else if (std::isdigit(static_cast<unsigned char>(c))
                || (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(s[i + 1])))) {
            while (i < n && (IsIdentChar(s[i]) || s[i] == '.')) {
                ++i;
            }
        } else if (IsIdentStart(c)) {
            flush_other(i);
            const size_t begin = i;
            while (i < n && IsIdentChar(s[i])) {
                ++i;
            }
            tokens.push_back(Token{TokenKind::Identifier, begin, i});
            other_begin = i;
        } else {
            ++i;
        }
    }
    flush_other(n);
    return tokens;
}

char NextNonSpace(const std::string &s, size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
    return pos < s.size() ? s[pos] : '\0';
}

/// Names the shader declares or defines as functions (or function-like macros)
std::set<std::string> CollectUserFunctions(const std::string &s, const std::vector<Token> &tokens) {
    static const std::set<std::string> kNotATypeName = {
            "return", "else", "do", "case", "if", "while", "for", "switch", "in", "out", "inout", "uniform",
    };
    std::set<std::string> names;
    const Token *prev_ident = nullptr;
    for (const Token &t : tokens) {
        if (t.kind != TokenKind::Identifier) {
            // Anything but whitespace between two identifiers breaks the "type name(" pattern
            const std::string text = s.substr(t.begin, t.end - t.begin);
            bool only_space = true;
            for (char c : text) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    only_space = false;
                    break;
                }
            }
            if (!only_space) {
                prev_ident = nullptr;
            }
            continue;
        }
        if (prev_ident != nullptr && NextNonSpace(s, t.end) == '(') {
            const std::string prev = s.substr(prev_ident->begin, prev_ident->end - prev_ident->begin);
            if (kNotATypeName.count(prev) == 0) {
                names.insert(s.substr(t.begin, t.end - t.begin));
            }
        }
        prev_ident = &t;
    }
    return names;
}

std::vector<std::string> SplitLines(const std::string &s) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= s.size()) {
        const size_t nl = s.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(s.substr(start));
            break;
        }
        lines.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

/// "#  directive rest" -> "directive", or "" when the line is not a directive
std::string DirectiveName(const std::string &line, size_t *rest = nullptr) {
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
        ++i;
    }
    if (i >= line.size() || line[i] != '#') {
        return std::string();
    }
    ++i;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
        ++i;
    }
    const size_t begin = i;
    while (i < line.size() && IsIdentChar(line[i])) {
        ++i;
    }
    if (rest != nullptr) {
        *rest = i;
    }
    return line.substr(begin, i - begin);
}

std::string FirstWordAfter(const std::string &line, size_t pos) {
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
        ++pos;
    }
    const size_t begin = pos;
    while (pos < line.size() && IsIdentChar(line[pos])) {
        ++pos;
    }
    return line.substr(begin, pos - begin);
}

struct Preprocessed {
    std::string body;
    std::vector<std::string> hoisted_extensions;
    int version = 110;
};

/**
 * Joins backslash-continued lines. GLSL only accepts line continuation from
 * 4.20, so the 4.10 path would reject the data shaders' multi-line macros.
 * The swallowed newlines are re-emitted after the joined line, so the line
 * numbers in compiler messages stay right.
 */
std::vector<std::string> JoinContinuedLines(const std::vector<std::string> &lines) {
    std::vector<std::string> joined;
    std::string pending;
    int swallowed = 0;
    for (const std::string &line : lines) {
        // Tolerate CRLF files: a backslash before "\r\n" continues the line too
        const size_t end = (!line.empty() && line.back() == '\r') ? line.size() - 1 : line.size();
        if (end > 0 && line[end - 1] == '\\') {
            pending += line.substr(0, end - 1);
            ++swallowed;
            continue;
        }
        joined.push_back(pending + line);
        pending.clear();
        for (; swallowed > 0; --swallowed) {
            joined.push_back(std::string());
        }
    }
    if (!pending.empty() || swallowed > 0) {
        joined.push_back(pending);
        for (; swallowed > 0; --swallowed) {
            joined.push_back(std::string());
        }
    }
    return joined;
}

/// Removes #version, drops core-promoted #extension lines and hoists the rest
Preprocessed PreprocessDirectives(const std::string &source) {
    Preprocessed out;
    std::vector<std::string> lines = JoinContinuedLines(SplitLines(source));
    bool seen_version = false;
    int depth = 0;
    for (std::string &line : lines) {
        size_t rest = 0;
        const std::string directive = DirectiveName(line, &rest);
        if (directive.empty()) {
            continue;
        }
        if (directive == "version" && !seen_version) {
            seen_version = true;
            out.version = std::atoi(line.c_str() + rest);
            if (out.version <= 0) {
                out.version = 110;
            }
            line.clear();  // keep the line count so error messages still point at the right line
        } else if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
            ++depth;
        } else if (directive == "endif") {
            --depth;
        } else if (directive == "extension") {
            const std::string name = FirstWordAfter(line, rest);
            bool core = false;
            for (const char *ext : kCoreExtensions) {
                if (name == ext) {
                    core = true;
                    break;
                }
            }
            if (core) {
                line.clear();
            } else if (depth == 0) {
                out.hoisted_extensions.push_back(line);
                line.clear();
            }
        }
    }
    std::string body;
    for (size_t i = 0; i < lines.size(); ++i) {
        body += lines[i];
        if (i + 1 < lines.size()) {
            body += '\n';
        }
    }
    out.body = body;
    return out;
}

struct Renamed {
    std::string body;
    /// kFunctionMacros entries the shader mentions, in table order
    std::vector<const Rename *> macros;
};

Renamed RenameTokens(const std::string &s, ShaderStage stage, int version) {
    const int bit = StageBit(stage);
    const std::vector<Token> tokens = Tokenize(s);
    const std::set<std::string> user_functions = CollectUserFunctions(s, tokens);

    std::map<std::string, std::string> identifiers;
    for (const Rename &r : kIdentifierRenames) {
        if (r.stages & bit) {
            identifiers[r.from] = r.to;
        }
    }
    std::set<std::string> mentioned;
    std::set<std::string> reserved;
    for (const Reserved &r : kReservedWords) {
        if (version < r.reserved_since) {
            reserved.insert(r.word);
        }
    }
    std::set<std::string> shadowed_builtins;
    for (const Reserved &r : kNewerBuiltinFunctions) {
        if (version < r.reserved_since && user_functions.count(r.word) != 0) {
            shadowed_builtins.insert(r.word);
        }
    }

    std::string out;
    out.reserve(s.size() + s.size() / 8);
    for (const Token &t : tokens) {
        const std::string text = s.substr(t.begin, t.end - t.begin);
        if (t.kind != TokenKind::Identifier) {
            out += text;
            continue;
        }
        if (reserved.count(text) != 0) {
            out += "vsc_kw_" + text;
            continue;
        }
        if (shadowed_builtins.count(text) != 0) {
            out += "vsc_fn_" + text;
            continue;
        }
        auto ident = identifiers.find(text);
        if (ident != identifiers.end()) {
            out += ident->second;
            continue;
        }
        mentioned.insert(text);
        if (text == "attribute" && stage == ShaderStage::Vertex) {
            out += "in";
        } else if (text == "varying") {
            out += (stage == ShaderStage::Vertex) ? "out" : "in";
        } else {
            out += text;
        }
    }
    Renamed result;
    result.body = out;
    for (const Rename &r : kFunctionMacros) {
        if ((r.stages & bit) && mentioned.count(r.from) != 0) {
            result.macros.push_back(&r);
        }
    }
    return result;
}

// --- prelude -----------------------------------------------------------------

struct Declaration {
    const char *name;
    int stages;
    const char *code;
};

// Declarations in emission order. Varyings and fragment outputs are handled separately.
const Declaration kDeclarations[] = {
        {"vsc_MaxLights", kBothStages, "const int vsc_MaxLights = 8;"},
        {"vsc_MaxTextureCoords", kBothStages, "const int vsc_MaxTextureCoords = 8;"},
        {"vsc_MaxTextureUnits", kBothStages, "const int vsc_MaxTextureUnits = 4;"},
        {"vsc_MaxClipPlanes", kBothStages, "const int vsc_MaxClipPlanes = 8;"},

        {"vsc_LightSourceParameters", kBothStages,
                "struct vsc_LightSourceParameters {\n"
                "    vec4 ambient;\n"
                "    vec4 diffuse;\n"
                "    vec4 specular;\n"
                "    vec4 position;\n"
                "    vec4 halfVector;\n"
                "    vec3 spotDirection;\n"
                "    float spotExponent;\n"
                "    float spotCutoff;\n"
                "    float spotCosCutoff;\n"
                "    float constantAttenuation;\n"
                "    float linearAttenuation;\n"
                "    float quadraticAttenuation;\n"
                "};"},
        {"vsc_MaterialParameters", kBothStages,
                "struct vsc_MaterialParameters {\n"
                "    vec4 emission;\n"
                "    vec4 ambient;\n"
                "    vec4 diffuse;\n"
                "    vec4 specular;\n"
                "    float shininess;\n"
                "};"},
        {"vsc_LightModelParameters", kBothStages, "struct vsc_LightModelParameters {\n    vec4 ambient;\n};"},
        {"vsc_LightModelProducts", kBothStages, "struct vsc_LightModelProducts {\n    vec4 sceneColor;\n};"},
        {"vsc_LightProducts", kBothStages,
                "struct vsc_LightProducts {\n"
                "    vec4 ambient;\n"
                "    vec4 diffuse;\n"
                "    vec4 specular;\n"
                "};"},
        {"vsc_FogParameters", kBothStages,
                "struct vsc_FogParameters {\n"
                "    vec4 color;\n"
                "    float density;\n"
                "    float start;\n"
                "    float end;\n"
                "    float scale;\n"
                "};"},

        {"vsc_ModelViewMatrix", kBothStages, "uniform mat4 vsc_ModelViewMatrix;"},
        {"vsc_ModelViewMatrixInverse", kBothStages, "uniform mat4 vsc_ModelViewMatrixInverse;"},
        {"vsc_ModelViewMatrixTranspose", kBothStages, "uniform mat4 vsc_ModelViewMatrixTranspose;"},
        {"vsc_ModelViewMatrixInverseTranspose", kBothStages, "uniform mat4 vsc_ModelViewMatrixInverseTranspose;"},
        {"vsc_ProjectionMatrix", kBothStages, "uniform mat4 vsc_ProjectionMatrix;"},
        {"vsc_ProjectionMatrixInverse", kBothStages, "uniform mat4 vsc_ProjectionMatrixInverse;"},
        {"vsc_ProjectionMatrixTranspose", kBothStages, "uniform mat4 vsc_ProjectionMatrixTranspose;"},
        {"vsc_ProjectionMatrixInverseTranspose", kBothStages, "uniform mat4 vsc_ProjectionMatrixInverseTranspose;"},
        {"vsc_ModelViewProjectionMatrix", kBothStages, "uniform mat4 vsc_ModelViewProjectionMatrix;"},
        {"vsc_ModelViewProjectionMatrixInverse", kBothStages, "uniform mat4 vsc_ModelViewProjectionMatrixInverse;"},
        {"vsc_ModelViewProjectionMatrixTranspose", kBothStages,
                "uniform mat4 vsc_ModelViewProjectionMatrixTranspose;"},
        {"vsc_ModelViewProjectionMatrixInverseTranspose", kBothStages,
                "uniform mat4 vsc_ModelViewProjectionMatrixInverseTranspose;"},
        {"vsc_TextureMatrix", kBothStages, "uniform mat4 vsc_TextureMatrix[vsc_MaxTextureCoords];"},
        {"vsc_TextureMatrixInverse", kBothStages, "uniform mat4 vsc_TextureMatrixInverse[vsc_MaxTextureCoords];"},
        {"vsc_TextureMatrixTranspose", kBothStages,
                "uniform mat4 vsc_TextureMatrixTranspose[vsc_MaxTextureCoords];"},
        {"vsc_TextureMatrixInverseTranspose", kBothStages,
                "uniform mat4 vsc_TextureMatrixInverseTranspose[vsc_MaxTextureCoords];"},
        {"vsc_NormalMatrix", kBothStages, "uniform mat3 vsc_NormalMatrix;"},
        {"vsc_NormalScale", kBothStages, "uniform float vsc_NormalScale;"},

        {"vsc_LightSource", kBothStages, "uniform vsc_LightSourceParameters vsc_LightSource[vsc_MaxLights];"},
        {"vsc_FrontMaterial", kBothStages, "uniform vsc_MaterialParameters vsc_FrontMaterial;"},
        {"vsc_BackMaterial", kBothStages, "uniform vsc_MaterialParameters vsc_BackMaterial;"},
        {"vsc_LightModel", kBothStages, "uniform vsc_LightModelParameters vsc_LightModel;"},
        {"vsc_FrontLightModelProduct", kBothStages, "uniform vsc_LightModelProducts vsc_FrontLightModelProduct;"},
        {"vsc_BackLightModelProduct", kBothStages, "uniform vsc_LightModelProducts vsc_BackLightModelProduct;"},
        {"vsc_FrontLightProduct", kBothStages, "uniform vsc_LightProducts vsc_FrontLightProduct[vsc_MaxLights];"},
        {"vsc_BackLightProduct", kBothStages, "uniform vsc_LightProducts vsc_BackLightProduct[vsc_MaxLights];"},
        {"vsc_TextureEnvColor", kBothStages, "uniform vec4 vsc_TextureEnvColor[vsc_MaxTextureUnits];"},
        {"vsc_EyePlaneS", kBothStages, "uniform vec4 vsc_EyePlaneS[vsc_MaxTextureCoords];"},
        {"vsc_EyePlaneT", kBothStages, "uniform vec4 vsc_EyePlaneT[vsc_MaxTextureCoords];"},
        {"vsc_EyePlaneR", kBothStages, "uniform vec4 vsc_EyePlaneR[vsc_MaxTextureCoords];"},
        {"vsc_EyePlaneQ", kBothStages, "uniform vec4 vsc_EyePlaneQ[vsc_MaxTextureCoords];"},
        {"vsc_ObjectPlaneS", kBothStages, "uniform vec4 vsc_ObjectPlaneS[vsc_MaxTextureCoords];"},
        {"vsc_ObjectPlaneT", kBothStages, "uniform vec4 vsc_ObjectPlaneT[vsc_MaxTextureCoords];"},
        {"vsc_ObjectPlaneR", kBothStages, "uniform vec4 vsc_ObjectPlaneR[vsc_MaxTextureCoords];"},
        {"vsc_ObjectPlaneQ", kBothStages, "uniform vec4 vsc_ObjectPlaneQ[vsc_MaxTextureCoords];"},
        {"vsc_Fog", kBothStages, "uniform vsc_FogParameters vsc_Fog;"},
        {"vsc_ClipPlane", kBothStages, "uniform vec4 vsc_ClipPlane[vsc_MaxClipPlanes];"},

        {"vsc_Vertex", kVertexStage, "layout(location = 0) in vec4 vsc_Vertex;"},
        {"vsc_Normal", kVertexStage, "layout(location = 1) in vec3 vsc_Normal;"},
        {"vsc_Color", kVertexStage, "layout(location = 2) in vec4 vsc_Color;"},
        {"vsc_SecondaryColor", kVertexStage, "layout(location = 3) in vec4 vsc_SecondaryColor;"},
        {"vsc_FogCoord", kVertexStage, "layout(location = 4) in float vsc_FogCoord;"},
        {"vsc_MultiTexCoord0", kVertexStage, "layout(location = 8) in vec4 vsc_MultiTexCoord0;"},
        {"vsc_MultiTexCoord1", kVertexStage, "layout(location = 9) in vec4 vsc_MultiTexCoord1;"},
        {"vsc_MultiTexCoord2", kVertexStage, "layout(location = 10) in vec4 vsc_MultiTexCoord2;"},
        {"vsc_MultiTexCoord3", kVertexStage, "layout(location = 11) in vec4 vsc_MultiTexCoord3;"},
        {"vsc_MultiTexCoord4", kVertexStage, "layout(location = 12) in vec4 vsc_MultiTexCoord4;"},
        {"vsc_MultiTexCoord5", kVertexStage, "layout(location = 13) in vec4 vsc_MultiTexCoord5;"},
        {"vsc_MultiTexCoord6", kVertexStage, "layout(location = 14) in vec4 vsc_MultiTexCoord6;"},
        {"vsc_MultiTexCoord7", kVertexStage, "layout(location = 15) in vec4 vsc_MultiTexCoord7;"},
        {"vsc_ClipVertex", kVertexStage, "vec4 vsc_ClipVertex;  // no user clip planes in core"},
};

struct Dependency {
    const char *name;
    const char *needs;
};

const Dependency kDependencies[] = {
        {"vsc_LightSource", "vsc_LightSourceParameters"},
        {"vsc_LightSource", "vsc_MaxLights"},
        {"vsc_FrontLightProduct", "vsc_LightProducts"},
        {"vsc_FrontLightProduct", "vsc_MaxLights"},
        {"vsc_BackLightProduct", "vsc_LightProducts"},
        {"vsc_BackLightProduct", "vsc_MaxLights"},
        {"vsc_FrontMaterial", "vsc_MaterialParameters"},
        {"vsc_BackMaterial", "vsc_MaterialParameters"},
        {"vsc_LightModel", "vsc_LightModelParameters"},
        {"vsc_FrontLightModelProduct", "vsc_LightModelProducts"},
        {"vsc_BackLightModelProduct", "vsc_LightModelProducts"},
        {"vsc_Fog", "vsc_FogParameters"},
        {"vsc_TextureMatrix", "vsc_MaxTextureCoords"},
        {"vsc_TextureMatrixInverse", "vsc_MaxTextureCoords"},
        {"vsc_TextureMatrixTranspose", "vsc_MaxTextureCoords"},
        {"vsc_TextureMatrixInverseTranspose", "vsc_MaxTextureCoords"},
        {"vsc_TextureEnvColor", "vsc_MaxTextureUnits"},
        {"vsc_EyePlaneS", "vsc_MaxTextureCoords"},
        {"vsc_EyePlaneT", "vsc_MaxTextureCoords"},
        {"vsc_EyePlaneR", "vsc_MaxTextureCoords"},
        {"vsc_EyePlaneQ", "vsc_MaxTextureCoords"},
        {"vsc_ObjectPlaneS", "vsc_MaxTextureCoords"},
        {"vsc_ObjectPlaneT", "vsc_MaxTextureCoords"},
        {"vsc_ObjectPlaneR", "vsc_MaxTextureCoords"},
        {"vsc_ObjectPlaneQ", "vsc_MaxTextureCoords"},
        {"vsc_ClipPlane", "vsc_MaxClipPlanes"},
        {"vsc_ftransform", "vsc_ModelViewProjectionMatrix"},
        {"vsc_ftransform", "vsc_Vertex"},
};

const char *const kShadowHelpers =
        "vec4 vsc_shadow(sampler1DShadow s, vec3 p) { return vec4(texture(s, p)); }\n"
        "vec4 vsc_shadow(sampler2DShadow s, vec3 p) { return vec4(texture(s, p)); }\n"
        "vec4 vsc_shadow(sampler2DRectShadow s, vec3 p) { return vec4(texture(s, p)); }\n";
const char *const kShadowBiasHelpers =
        "vec4 vsc_shadow(sampler1DShadow s, vec3 p, float bias) { return vec4(texture(s, p, bias)); }\n"
        "vec4 vsc_shadow(sampler2DShadow s, vec3 p, float bias) { return vec4(texture(s, p, bias)); }\n";
const char *const kShadowProjHelpers =
        "vec4 vsc_shadowProj(sampler1DShadow s, vec4 p) { return vec4(textureProj(s, p)); }\n"
        "vec4 vsc_shadowProj(sampler2DShadow s, vec4 p) { return vec4(textureProj(s, p)); }\n";
const char *const kShadowProjBiasHelpers =
        "vec4 vsc_shadowProj(sampler1DShadow s, vec4 p, float bias) { return vec4(textureProj(s, p, bias)); }\n"
        "vec4 vsc_shadowProj(sampler2DShadow s, vec4 p, float bias) { return vec4(textureProj(s, p, bias)); }\n";

void AppendVaryings(std::ostringstream &os, ShaderStage stage, const VaryingSet &v) {
    const char *dir = stage == ShaderStage::Vertex ? "out" : "in";
    if (v.front_color) {
        os << dir << " vec4 vsc_FrontColor;\n";
    }
    if (v.back_color) {
        os << dir << " vec4 vsc_BackColor;\n";
    }
    if (v.front_secondary_color) {
        os << dir << " vec4 vsc_FrontSecondaryColor;\n";
    }
    if (v.back_secondary_color) {
        os << dir << " vec4 vsc_BackSecondaryColor;\n";
    }
    if (v.fog_frag_coord) {
        os << dir << " float vsc_FogFragCoord;\n";
    }
    if (v.tex_coords > 0) {
        os << dir << " vec4 vsc_TexCoord[" << v.tex_coords << "];\n";
    }
}

bool ContainsIdentifier(const std::set<std::string> &used, const char *name) {
    return used.count(name) != 0;
}

} // namespace

void VaryingSet::Merge(const VaryingSet &o) {
    front_color = front_color || o.front_color;
    back_color = back_color || o.back_color;
    front_secondary_color = front_secondary_color || o.front_secondary_color;
    back_secondary_color = back_secondary_color || o.back_secondary_color;
    fog_frag_coord = fog_frag_coord || o.fog_frag_coord;
    tex_coords = std::max(tex_coords, o.tex_coords);
}

int DetectVersion(const std::string &source) {
    for (const std::string &line : SplitLines(source)) {
        size_t rest = 0;
        if (DirectiveName(line, &rest) == "version") {
            const int v = std::atoi(line.c_str() + rest);
            return v > 0 ? v : 110;
        }
    }
    return 110;
}

std::set<std::string> CollectBuiltins(const std::string &source) {
    std::set<std::string> used;
    for (const Token &t : Tokenize(source)) {
        if (t.kind == TokenKind::Identifier && t.end - t.begin > 4 && source.compare(t.begin, 4, "vsc_") == 0) {
            used.insert(source.substr(t.begin, t.end - t.begin));
        }
    }
    return used;
}

VaryingSet CollectVaryings(const std::string &source) {
    VaryingSet v;
    const std::vector<Token> tokens = Tokenize(source);
    bool dynamic_tex_index = false;
    int max_tex_index = -1;
    for (const Token &t : tokens) {
        if (t.kind != TokenKind::Identifier) {
            continue;
        }
        const std::string name = source.substr(t.begin, t.end - t.begin);
        if (name == "vsc_FrontColor") {
            v.front_color = true;
        } else if (name == "vsc_BackColor") {
            v.back_color = true;
        } else if (name == "vsc_FrontSecondaryColor") {
            v.front_secondary_color = true;
        } else if (name == "vsc_BackSecondaryColor") {
            v.back_secondary_color = true;
        } else if (name == "vsc_FogFragCoord") {
            v.fog_frag_coord = true;
        } else if (name == "vsc_TexCoord") {
            // Constant subscripts let us size the array; anything else needs all of it.
            size_t p = t.end;
            while (p < source.size() && std::isspace(static_cast<unsigned char>(source[p]))) {
                ++p;
            }
            bool constant = false;
            if (p < source.size() && source[p] == '[') {
                ++p;
                while (p < source.size() && std::isspace(static_cast<unsigned char>(source[p]))) {
                    ++p;
                }
                const size_t digits = p;
                while (p < source.size() && std::isdigit(static_cast<unsigned char>(source[p]))) {
                    ++p;
                }
                const size_t digits_end = p;
                while (p < source.size() && std::isspace(static_cast<unsigned char>(source[p]))) {
                    ++p;
                }
                if (digits_end > digits && p < source.size() && source[p] == ']') {
                    constant = true;
                    max_tex_index = std::max(max_tex_index, std::atoi(source.c_str() + digits));
                }
            }
            if (!constant) {
                dynamic_tex_index = true;
            }
        }
    }
    if (dynamic_tex_index) {
        v.tex_coords = 8;
    } else if (max_tex_index >= 0) {
        v.tex_coords = std::min(8, max_tex_index + 1);
    }
    return v;
}

void AddPreludeDependencies(std::set<std::string> &used) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Dependency &d : kDependencies) {
            if (used.count(d.name) != 0 && used.count(d.needs) == 0) {
                used.insert(d.needs);
                changed = true;
            }
        }
    }
}

std::string BuildPrelude(ShaderStage stage, const std::set<std::string> &used_in, const VaryingSet &varyings) {
    std::set<std::string> used = used_in;
    AddPreludeDependencies(used);
    const int bit = StageBit(stage);

    std::ostringstream os;
    if (stage == ShaderStage::Vertex) {
        os << "invariant gl_Position;\n";
    }
    for (const Declaration &d : kDeclarations) {
        if ((d.stages & bit) && used.count(d.name) != 0) {
            os << d.code << "\n";
        }
    }
    AppendVaryings(os, stage, varyings);
    if (stage == ShaderStage::Fragment) {
        if (ContainsIdentifier(used, "vsc_FragData")) {
            os << "layout(location = 0) out vec4 vsc_FragData[8];\n";
        } else if (ContainsIdentifier(used, "vsc_FragColor")) {
            os << "layout(location = 0) out vec4 vsc_FragColor;\n";
        }
    }
    if (stage == ShaderStage::Vertex && ContainsIdentifier(used, "vsc_ftransform")) {
        os << "vec4 vsc_ftransform() { return vsc_ModelViewProjectionMatrix * vsc_Vertex; }\n";
    }
    if (ContainsIdentifier(used, "vsc_shadow")) {
        os << kShadowHelpers;
        if (stage == ShaderStage::Fragment) {
            os << kShadowBiasHelpers;
        }
    }
    if (ContainsIdentifier(used, "vsc_shadowProj")) {
        os << kShadowProjHelpers;
        if (stage == ShaderStage::Fragment) {
            os << kShadowProjBiasHelpers;
        }
    }
    return os.str();
}

ProgramTranslation TranslateLegacyProgram(const std::string &vertex_source,
        const std::string &fragment_source, int glsl_version) {
    ProgramTranslation result;

    const Preprocessed vp = PreprocessDirectives(vertex_source);
    const Preprocessed fp = PreprocessDirectives(fragment_source);
    const Renamed vrenamed = RenameTokens(vp.body, ShaderStage::Vertex, vp.version);
    const Renamed frenamed = RenameTokens(fp.body, ShaderStage::Fragment, fp.version);
    const std::string &vbody = vrenamed.body;
    const std::string &fbody = frenamed.body;

    // The vertex stage writes every built-in varying either stage mentions;
    // the fragment stage only declares what it reads.
    VaryingSet vertex_varyings = CollectVaryings(vbody);
    const VaryingSet fragment_varyings = CollectVaryings(fbody);
    vertex_varyings.Merge(fragment_varyings);
    // The fragment array must match the vertex one in size
    VaryingSet fragment_decl = fragment_varyings;
    if (fragment_decl.tex_coords > 0) {
        fragment_decl.tex_coords = vertex_varyings.tex_coords;
    }
    result.varyings = vertex_varyings;

    const std::string version_line = "#version " + std::to_string(glsl_version) + " core\n";

    struct Stage {
        const Preprocessed *pre;
        const Renamed *renamed;
        const std::string *body;
        ShaderStage stage;
        const VaryingSet *varyings;
        ShaderTranslation *out;
    };
    const Stage stages[] = {
            {&vp, &vrenamed, &vbody, ShaderStage::Vertex, &vertex_varyings, &result.vertex},
            {&fp, &frenamed, &fbody, ShaderStage::Fragment, &fragment_decl, &result.fragment},
    };
    for (const Stage &st : stages) {
        std::set<std::string> used = CollectBuiltins(*st.body);
        for (const Rename *macro : st.renamed->macros) {
            if (std::strncmp(macro->to, "vsc_", 4) == 0) {
                used.insert(macro->to);
            }
        }
        AddPreludeDependencies(used);
        std::string src = version_line;
        for (const std::string &ext : st.pre->hoisted_extensions) {
            src += ext + "\n";
        }
        // Texture LOD sampling is native in core GLSL; this keeps fplod.h's
        // fallbacks (which would redefine builtins) compiled out.
        src += "#define VGL_NV_fragment_program2 1\n";
        for (const Rename *macro : st.renamed->macros) {
            src += std::string("#define ") + macro->from + " " + macro->to + "\n";
        }
        std::string prelude = BuildPrelude(st.stage, used, *st.varyings);
        if (st.stage == ShaderStage::Vertex && st.body->find("invariant gl_Position") != std::string::npos) {
            // the shader already says it; saying it twice is an error
            const std::string line = "invariant gl_Position;\n";
            prelude.erase(0, line.size());
        }
        src += prelude;
        src += "#line 1\n";
        src += *st.body;
        src += "\n";
        st.out->source = src;
        st.out->original_version = st.pre->version;
        st.out->builtins = used;
    }
    return result;
}

} // namespace vsgl
