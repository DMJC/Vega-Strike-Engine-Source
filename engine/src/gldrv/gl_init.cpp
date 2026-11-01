/*
 * gl_init.cpp
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


#include <assert.h>
#include <set>
#include <string>

#define GL_INIT_CPP
#include "gl_globals.h"
#undef GL_INIT_CPP
#include "gl_include.h"
#include "root_generic/vs_globals.h"
#include "root_generic/xml_support.h"
#include "src/config_xml.h"
#include "winsys.h"
#include "src/gfxlib.h"
#include "root_generic/options.h"
#include "src/vs_logging.h"

#if !defined (_WIN32) && !defined (__CYGWIN__)

#include <stdlib.h>

#else
#ifndef NOMINMAX
#define NOMINMAX
#endif //tells VCC not to generate min/max macros
// #if defined(__APPLE__) && defined (__MACH__)
// #include <GL/glew.h>
// #else
#define GL_TEXTURE_CUBE_MAP_SEAMLESS_ARB 0x884F
// #endif
#include <windows.h>
#endif
#define GL_GLEXT_PROTOTYPES 1
// See https://github.com/vegastrike/Vega-Strike-Engine-Source/pull/851#discussion_r1589254766
#if defined(__APPLE__) && defined (__MACH__)
    #include <gl.h>
    #include <glext.h>
    #include <dlfcn.h>
#else
    #include <gl.h>
    #include <glext.h>
#endif
#ifdef GL_EXT_compiled_vertex_array
# ifndef PFNGLLOCKARRAYSEXTPROC
#  undef GL_EXT_compiled_vertex_array
# endif
#endif
//typedef void (APIENTRY * PFNGLLOCKARRAYSEXTPROC) (GLint first, GLsizei count);
//typedef void (APIENTRY * PFNGLUNLOCKARRAYSEXTPROC) (void);

#if !defined (WIN32) && !defined (__HAIKU__) && !defined (__APPLE__) && !defined (MACOSX)
    # define GLX_GLXEXT_PROTOTYPES 1
    # define GLX_GLXEXT_LEGACY 1
    # include <GL/glx.h>
    # include <GL/glext.h>
#endif

#include <stdio.h>
#include "gl_init.h"
#include "gl_ffp_compat.h"
#include "gl_renderer.h"

#include "configuration/configuration.h"
#define WINDOW_TITLE "Vega Strike " VERSION

#if defined (CG_SUPPORT)
#include "src/cg_global.h"
#endif

PFNGLBINDBUFFERARBPROC glBindBufferARB_p = nullptr;
PFNGLGENBUFFERSPROC glGenBuffersARB_p = nullptr;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB_p = nullptr;
PFNGLBUFFERDATAARBPROC glBufferDataARB_p = nullptr;
PFNGLMAPBUFFERARBPROC glMapBufferARB_p = nullptr;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB_p = nullptr;

PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB_p = nullptr;
PFNGLCLIENTACTIVETEXTUREARBPROC glActiveTextureARB_p = nullptr;
PFNGLCOLORTABLEEXTPROC glColorTable_p = nullptr;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB_p = nullptr;
PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB_p = nullptr;

PFNGLLOCKARRAYSEXTPROC glLockArraysEXT_p = nullptr;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT_p = nullptr;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D_p = nullptr;
PFNGLMULTIDRAWARRAYSEXTPROC glMultiDrawArrays_p = nullptr;
PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElements_p = nullptr;

PFNGLGETSHADERIVPROC glGetShaderiv_p = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv_p = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog_p = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog_p = nullptr;
PFNGLCREATESHADERPROC glCreateShader_p = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource_p = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader_p = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram_p = nullptr;
PFNGLATTACHSHADERPROC glAttachShader_p = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram_p = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram_p = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation_p = nullptr;
PFNGLUNIFORM1FPROC glUniform1f_p = nullptr;
PFNGLUNIFORM2FPROC glUniform2f_p = nullptr;
PFNGLUNIFORM3FPROC glUniform3f_p = nullptr;
PFNGLUNIFORM4FPROC glUniform4f_p = nullptr;

PFNGLUNIFORM1IPROC glUniform1i_p = nullptr;
PFNGLUNIFORM2IPROC glUniform2i_p = nullptr;
PFNGLUNIFORM3IPROC glUniform3i_p = nullptr;
PFNGLUNIFORM4IPROC glUniform4i_p = nullptr;

PFNGLUNIFORM1FVPROC glUniform1fv_p = nullptr;
PFNGLUNIFORM2FVPROC glUniform2fv_p = nullptr;
PFNGLUNIFORM3FVPROC glUniform3fv_p = nullptr;
PFNGLUNIFORM4FVPROC glUniform4fv_p = nullptr;

PFNGLUNIFORM1IVPROC glUniform1iv_p = nullptr;
PFNGLUNIFORM2IVPROC glUniform2iv_p = nullptr;
PFNGLUNIFORM3IVPROC glUniform3iv_p = nullptr;
PFNGLUNIFORM4IVPROC glUniform4iv_p = nullptr;

PFNGLDELETESHADERPROC glDeleteShader_p = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram_p = nullptr;

#define VSGL_DEFINE_FUNCTION(ret, name, params) vsgl_pfn_##name gl##name##_p = nullptr;
VSGL_CORE_FUNCTIONS(VSGL_DEFINE_FUNCTION)
VSGL_OPTIONAL_FUNCTIONS(VSGL_DEFINE_FUNCTION)
#undef VSGL_DEFINE_FUNCTION

typedef void ( *(*get_gl_proc_fptr_t)(const GLubyte *))();
#ifdef _WIN32
    typedef char * GET_GL_PTR_TYP;
    #define GET_GL_PROC wglGetProcAddress
#else
    #if defined(__HAIKU__)
        typedef char * GET_GL_PTR_TYP;
        #define GET_GL_PROC glutGetProcAddress
    #elif defined (__APPLE__) && defined (__MACH__)
        typedef const char * GET_GL_PTR_TYP;
        void * vega_dlsym(GET_GL_PTR_TYP function_name) {
            return dlsym(RTLD_SELF, function_name);
        }
        #define GET_GL_PROC vega_dlsym
    #else
        typedef GLubyte *GET_GL_PTR_TYP;
        #define GET_GL_PROC glXGetProcAddressARB
    #endif
#endif

#if defined (CG_SUPPORT)
CG_Cloak *cloak_cg = new CG_Cloak();
#endif

bool vsglLoadCoreFunctions(const char **missing) {
#define VSGL_LOAD_FUNCTION(ret, name, params) \
    gl##name##_p = reinterpret_cast<vsgl_pfn_##name>(GET_GL_PROC((GET_GL_PTR_TYP) "gl" #name)); \
    if (gl##name##_p == nullptr) { \
        if (missing != nullptr) { \
            *missing = "gl" #name; \
        } \
        return false; \
    }
    VSGL_CORE_FUNCTIONS(VSGL_LOAD_FUNCTION)
#undef VSGL_LOAD_FUNCTION
    return true;
}

void vsglLoadOptionalFunctions() {
#define VSGL_LOAD_FUNCTION(ret, name, params) \
    gl##name##_p = reinterpret_cast<vsgl_pfn_##name>(GET_GL_PROC((GET_GL_PTR_TYP) "gl" #name));
    VSGL_OPTIONAL_FUNCTIONS(VSGL_LOAD_FUNCTION)
#undef VSGL_LOAD_FUNCTION
}

/* CENTRY */
int vsExtensionSupported(const char *extension) {
    // Core profiles only list extensions one by one (GL_EXTENSIONS through
    // glGetString is an error there).
    static std::set<std::string> extensions;
    static bool queried = false;
    if (!queried && glGetStringi_p != nullptr) {
        queried = true;
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (GLint i = 0; i < count; ++i) {
            const GLubyte *name = (*glGetStringi_p)(GL_EXTENSIONS, static_cast<GLuint>(i));
            if (name != nullptr) {
                extensions.insert(reinterpret_cast<const char *>(name));
            }
        }
    }
    return extensions.count(extension) != 0 ? 1 : 0;
}

bool vsVendorMatch(const char *vendor) {
    static const GLubyte *_glvendor = NULL;

    if (_glvendor == NULL) {
        _glvendor = glGetString(GL_VENDOR);
        if (_glvendor != NULL) {
            VS_LOG(info, (boost::format("OpenGL Vendor: %1%") % (const char *) _glvendor));
        }
    }

    if (_glvendor != NULL) {
        // NOTE: Don't be fooled by substrings within words
        //      Should match whole-words ONLY

        static const std::string glvendor = strtolower(std::string((const char *) _glvendor));
        static const std::string::size_type glvendor_sz = glvendor.length();
        std::string svendor = strtolower(vendor);
        std::string::size_type svendor_sz = svendor.length();
        std::string::size_type pos = glvendor.find(svendor);

        if (pos == std::string::npos) {
            return false;
        }

        if ((pos > 0) && isalnum(glvendor[pos - 1])) {
            return false;
        }

        if ((pos + svendor_sz) < glvendor_sz && isalnum(glvendor[pos + svendor_sz])) {
            return false;
        }

        return true;
    } else {
        return false;
    }
}

void init_opengl_extensions() {
    GLint num_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    VS_LOG(trace, (boost::format("OpenGL extensions supported: %1%") % num_extensions));

    // Fixed-function era extensions (compiled vertex arrays, paletted
    // textures, client-side multitexture) do not exist on a core profile.
    glLockArraysEXT_p = nullptr;
    glUnlockArraysEXT_p = nullptr;
    glColorTable_p = nullptr;
    glMultiTexCoord2fARB_p = nullptr;
    glMultiTexCoord4fARB_p = nullptr;
    glClientActiveTextureARB_p = nullptr;

    // Everything below is core since GL 2.0 - 3.0; load the core entry points.
    glMultiDrawArrays_p = (PFNGLMULTIDRAWARRAYSEXTPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glMultiDrawArrays");
    glMultiDrawElements_p = (PFNGLMULTIDRAWELEMENTSEXTPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glMultiDrawElements");
    glBindBufferARB_p = (PFNGLBINDBUFFERARBPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glBindBuffer");
    glGenBuffersARB_p = (PFNGLGENBUFFERSPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glGenBuffers");
    glDeleteBuffersARB_p = (PFNGLDELETEBUFFERSARBPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glDeleteBuffers");
    glBufferDataARB_p = (PFNGLBUFFERDATAARBPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glBufferData");
    glMapBufferARB_p = (PFNGLMAPBUFFERARBPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glMapBuffer");
    glUnmapBufferARB_p = (PFNGLUNMAPBUFFERARBPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUnmapBuffer");
    glActiveTextureARB_p = (PFNGLCLIENTACTIVETEXTUREARBPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glActiveTexture");
    glCompressedTexImage2D_p =
            (PFNGLCOMPRESSEDTEXIMAGE2DPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glCompressedTexImage2D");
    glGetShaderiv_p = (PFNGLGETSHADERIVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glGetShaderiv");
    glGetProgramiv_p = (PFNGLGETPROGRAMIVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glGetProgramiv");
    glGetShaderInfoLog_p = (PFNGLGETSHADERINFOLOGPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glGetShaderInfoLog");
    glGetProgramInfoLog_p = (PFNGLGETPROGRAMINFOLOGPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glGetProgramInfoLog");
    glCreateShader_p = (PFNGLCREATESHADERPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glCreateShader");
    glCreateProgram_p = (PFNGLCREATEPROGRAMPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glCreateProgram");
    glShaderSource_p = (PFNGLSHADERSOURCEPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glShaderSource");
    glCompileShader_p = (PFNGLCOMPILESHADERPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glCompileShader");
    glAttachShader_p = (PFNGLATTACHSHADERPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glAttachShader");
    glLinkProgram_p = (PFNGLLINKPROGRAMPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glLinkProgram");
    glUseProgram_p = (PFNGLUSEPROGRAMPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUseProgram");
    glGetUniformLocation_p = (PFNGLGETUNIFORMLOCATIONPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glGetUniformLocation");
    glUniform1f_p = (PFNGLUNIFORM1FPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform1f");
    glUniform2f_p = (PFNGLUNIFORM2FPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform2f");
    glUniform3f_p = (PFNGLUNIFORM3FPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform3f");
    glUniform4f_p = (PFNGLUNIFORM4FPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform4f");
    glUniform1i_p = (PFNGLUNIFORM1IPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform1i");
    glUniform2i_p = (PFNGLUNIFORM2IPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform2i");
    glUniform3i_p = (PFNGLUNIFORM3IPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform3i");
    glUniform4i_p = (PFNGLUNIFORM4IPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform4i");
    glUniform1fv_p = (PFNGLUNIFORM1FVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform1fv");
    glUniform2fv_p = (PFNGLUNIFORM2FVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform2fv");
    glUniform3fv_p = (PFNGLUNIFORM3FVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform3fv");
    glUniform4fv_p = (PFNGLUNIFORM4FVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform4fv");
    glUniform1iv_p = (PFNGLUNIFORM1IVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform1iv");
    glUniform2iv_p = (PFNGLUNIFORM2IVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform2iv");
    glUniform3iv_p = (PFNGLUNIFORM3IVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform3iv");
    glUniform4iv_p = (PFNGLUNIFORM4IVPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glUniform4iv");
    glDeleteShader_p = (PFNGLDELETESHADERPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glDeleteShader");
    glDeleteProgram_p = (PFNGLDELETEPROGRAMPROC) GET_GL_PROC((GET_GL_PTR_TYP) "glDeleteProgram");

    switch (configuration().graphics.fog_detail) {
        case 0:
            vsglFogi(GL_FOG_DISTANCE_MODE_NV, GL_EYE_PLANE_ABSOLUTE_NV);
            break;
        case 1:
            vsglFogi(GL_FOG_DISTANCE_MODE_NV, GL_EYE_PLANE);
            break;
        case 2:
            vsglFogi(GL_FOG_DISTANCE_MODE_NV, GL_EYE_RADIAL_NV);
            break;
    }
    // Generic texture compression is core; S3TC is still an extension.
    if (vsExtensionSupported("GL_EXT_texture_compression_s3tc")) {
        VS_LOG(trace, "OpenGL::S3TC Texture Compression supported");
    } else {
        (configuration()).graphics.s3tc = false;
        VS_LOG(info, "OpenGL::S3TC Texture Compression unsupported");
    }
    {
        // gl_options.Multitexture holds the configured reflection setting until
        // here; a nonzero value enables multitexturing with every image unit.
        GLint units = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &units);
        if (gl_options.Multitexture && units > 1) {
            gl_options.Multitexture = units;
            VS_LOG(trace, (boost::format("OpenGL::Multitexture supported (%1% units)") % gl_options.Multitexture));
        } else {
            gl_options.Multitexture = 0;
        }
    }
    // Cube maps, edge/border clamping and sRGB framebuffers are core.
    gl_options.cubemap = 1;
    {
        // Ask the default framebuffer whether its back buffer is sRGB-capable
        GLint encoding = 0;
        (*glGetFramebufferAttachmentParameteriv_p)(GL_FRAMEBUFFER, GL_BACK_LEFT,
                GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
        if (glGetError() == GL_NO_ERROR && encoding == GL_SRGB) {
            gl_options.ext_srgb_framebuffer = true;
            VS_LOG(trace, "OpenGL::sRGB Framebuffer supported");
        } else {
            gl_options.ext_srgb_framebuffer = false;
            VS_LOG(info, "OpenGL::sRGB Framebuffer unsupported by visual");
        }
    }
    // Explicit-LOD texture lookups (what NV_fragment_program2 used to signal)
    // are part of every core GLSL version.
    gl_options.nv_fp2 = true;
    if (GFXDefaultShaderSupported()) {
        if (gl_options.Multitexture < 16) {
            gl_options.Multitexture = 16;
        }
    }

    // some useful generic limits
    GLint max_vertices = -1, max_indices = -1;
    glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &max_indices);
    glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &max_vertices);
    gl_options.max_array_indices = max_indices;
    gl_options.max_array_vertices = max_vertices;

    VS_LOG(info, (boost::format("Max vertex array indices: %1%") % gl_options.max_array_indices));
    VS_LOG(info, (boost::format("Max vertex array vertices: %1%") % gl_options.max_array_vertices));
}

// Copy the config-driven gl_options fields from configuration(). These were set
// inline during GFXInit (one-shot). Factored out so GFXReinitConfig can re-run
// them after an in-game settings change. Only the fields that directly mirror a
// config value are copied here; the hardware-limited ones (max_rect_dimension,
// max_array_*, max_texture_dimension) are one-shot GL/extension queries and are
// not re-derived.
//
// `apply_technique` controls whether the technique/shader-affecting fields are
// copied: applied at startup (GFXInit), but NOT on a live re-init (GFXReinitConfig).
// gl_options.Multitexture (the reflection flag) drives how many texture layers a
// sprite/technique draws; changing it mid-game forces the technique rendering to
// re-evaluate and produces broken visuals until a restart. So like shaders, a
// reflection/technique change is written out but only takes effect on restart.
static void reapply_gl_options(bool apply_technique) {
    gl_options.wireframe = configuration().graphics.use_wireframe;
    gl_options.smooth_shade = configuration().graphics.smooth_shade;
    gl_options.mipmap = configuration().graphics.mipmap_detail;
    gl_options.compression = configuration().graphics.texture_compression;
    if (apply_technique) {
        gl_options.Multitexture = configuration().graphics.reflection;
    }
    gl_options.display_lists = configuration().graphics.displaylists;
}

static void initfov() {
    g_game.detaillevel = configuration().graphics.model_detail_flt;
    g_game.use_textures = configuration().graphics.use_textures;
    g_game.use_ship_textures = configuration().graphics.use_ship_textures;
    g_game.use_planet_textures = configuration().graphics.use_planet_textures;
    g_game.use_logos = configuration().graphics.use_logos;
    g_game.use_sprites = configuration().graphics.use_vs_sprites;
    g_game.use_animations = configuration().graphics.use_animations;
    g_game.use_videos = configuration().graphics.use_videos;

    /*
     *  FILE * fp = fopen ("glsetup.txt","r");
     *  if (fp) {
     *  VSFileSystem::Fscanf (fp,"fov %f\n",&configuration().graphics.fov);
     *  VSFileSystem::Fscanf (fp,"aspect %f\n",&g_game.aspect);
     *  VSFileSystem::Fscanf (fp,"znear %f\n",&configuration().graphics.znear);
     *  VSFileSystem::Fscanf (fp,"zfar %f\n",&configuration().graphics.zfar);
     *  VSFileSystem::Close (fp);
     *  }
     */
}

// Re-initialize the graphics runtime state from the in-memory configuration() after
// an in-game settings change (the config screen's Save path). Reuses the bootstrap
// copy code (initfov + reapply_gl_options) so the change takes effect without a
// restart. Also re-binds the GL viewport to the current drawable size. Does NOT
// recreate the window or GL context (those stay one-shot); see gl_init.h.
void GFXReinitConfig() {
    initfov();
    // Do NOT re-apply the technique/shader-affecting gl_options (Multitexture /
    // reflection): changing it mid-game breaks the technique rendering (see
    // reapply_gl_options). Reflection/technique changes apply on restart.
    reapply_gl_options(/*apply_technique=*/false);
    glViewport(0, 0, native_resolution_x, native_resolution_y);
}

static void Reshape(int x, int y) {
    (configuration()).graphics.resolution_x = x;
    (configuration()).graphics.resolution_y = y;
    VS_LOG(trace, (boost::format("Reshaping %1% %2%") % x % y));
}

extern void GFXInitTextureManager();

void GFXInit(int argc, char **argv) {
    char vsname[12] = "Vega Strike";
    char vsicon[9] = "vega.ico";
    winsys_init(&argc, argv, &vsname[0], &vsicon[0]);

    const char *missing = nullptr;
    if (!vsglLoadCoreFunctions(&missing)) {
        VS_LOG_FLUSH_EXIT(fatal, (boost::format("The OpenGL driver lacks %1%, which %2% requires")
                % missing % winsys_gl_profile().Name()), 1);
    }
    vsglLoadOptionalFunctions();

    glViewport(0, 0, native_resolution_x, native_resolution_y);
    
    static GFXColor clearcol = vs_config->getColor("space_background");;
    gl_options.wireframe = configuration().graphics.use_wireframe;
    (configuration()).graphics.max_texture_dimension = configuration().graphics.max_texture_dimension;
    (configuration()).graphics.max_movie_dimension = configuration().graphics.max_movie_dimension;
    bool textsupported =
            (vsExtensionSupported("GL_ARB_texture_non_power_of_two") || vsExtensionSupported("GL_ARB_texture_rectangle")
                    || vsExtensionSupported("GL_NV_texture_rectangle")) ? "true" : "false";

    (configuration()).graphics.rect_textures = configuration().graphics.rect_textures ? true : textsupported;

    if (configuration().graphics.rect_textures) {
        VS_LOG(trace, "RECT textures supported");

        // Fetch max rect textue dimension
        GLint max_rect_dimension = 65535;
        glGetIntegerv(
                GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB,
                &max_rect_dimension);

        gl_options.max_rect_dimension = max_rect_dimension;
        VS_LOG(trace, (boost::format("RECT max texture dimension: %1%") % max_rect_dimension));
    }

    bool vidsupported = (configuration().graphics.rect_textures
                         || (vsExtensionSupported("GL_ARB_texture_non_power_of_two") && vsVendorMatch("nvidia")));

    (configuration()).graphics.pot_video_textures = configuration().graphics.pot_video_textures ? true : vidsupported;

    if (!configuration().graphics.pot_video_textures && configuration().graphics.rect_textures) {
        // Enforce max rect texture for movies, which use them
        if (configuration().graphics.max_movie_dimension > gl_options.max_rect_dimension) {
            (configuration()).graphics.max_movie_dimension = gl_options.max_rect_dimension;
        }
    }

    /*if (configuration().graphics.pot_video_textures) {
        VS_LOG(info, "Forcing POT video textures");
    } else {
        VS_LOG(trace, "Using NPOT video textures");
    }*/
    // Config-driven gl_options copies (extracted so GFXReinitConfig can re-run them
    // on an in-game settings change). The self-assignment lines below keep their
    // original meaning (config already holds the value; assigned back for clarity).
    reapply_gl_options(/*apply_technique=*/true);   // startup: apply all config-driven gl_options
    (configuration()).graphics.smooth_lines = configuration().graphics.smooth_lines;
    (configuration()).graphics.smooth_points = configuration().graphics.smooth_points;
    (configuration()).graphics.s3tc = configuration().graphics.s3tc;
    (configuration()).graphics.ext_clamp_to_edge = configuration().graphics.ext_clamp_to_edge;
    (configuration()).graphics.ext_clamp_to_border = configuration().graphics.ext_clamp_to_border;

    glClearColor(clearcol.r, clearcol.g, clearcol.b, clearcol.a);
    winsys_set_reshape_func(Reshape);
    initfov();
    vsglShadeModel(GL_SMOOTH);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    if (gl_options.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    if (gl_options.smooth_shade == 0) {
        vsglShadeModel(GL_FLAT);
    }
    vsglEnable(GL_ALPHA_TEST);
    vsglAlphaFunc(GL_GREATER, 0.0);

    init_opengl_extensions();
    if (!vsgl::InitRenderer(winsys_gl_profile())) {
        VS_LOG_FLUSH_EXIT(fatal, "Could not build the fixed-function emulation program", 1);
    }
    GFXInitTextureManager();
    if (gl_options.Multitexture) {
        GFXActiveTexture(0);
    }
    vsglEnable(GL_TEXTURE_2D);          //use two-dimensional texturing

    // Paletted textures are expanded to RGBA on upload
    gl_options.PaletteExt = 0;
    vsglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
    if (gl_options.Multitexture) {
        for (int i = 1; i < 4; ++i) {
            GFXActiveTexture(i);
            GFXTextureEnv(i, GFXADDTEXTURE);
            // Choose cubemap or spheremap coordinates.
            if (i == 1) {
#ifdef NV_CUBE_MAP
                GFXToggleTexture(true, 1, CUBEMAP);
                GFXTextureCoordGenMode(1, CUBE_MAP_GEN, NULL, NULL);
#else
                const float tempo[4] = {1, 0, 0, 0};
                GFXToggleTexture( true, 1, TEXTURE2D );
                GFXTextureCoordGenMode( 1, SPHERE_MAP_GEN, tempo, tempo );
#endif
            }
        }
    }
    GFXActiveTexture(0);
    glClearDepth(1);
    glEnable(GL_BLEND);
    vsglDisable(GL_ALPHA_TEST);
    GFXBlendMode(ONE, ZERO);

    vsglColor3f(0, 0, 0);
    vsglMatrixMode(GL_MODELVIEW);
    vsglLoadIdentity();
    vsglMatrixMode(GL_TEXTURE);
    vsglLoadIdentity();     //set all matricies to identity
    vsglMatrixMode(GL_PROJECTION);
    vsglLoadIdentity();

    vsglEnable(GL_LIGHTING);

    glDisable(GL_LINE_SMOOTH);
    vsglDisable(GL_POINT_SMOOTH);

    int con;
    GFXCreateLightContext(con);
    //glutSetCursor(GLUT_CURSOR_NONE);
    /* Avoid scrambled screen on startup - Twice, for triple buffering */
    if (configuration().graphics.clear_on_startup) {
        glClear(GL_COLOR_BUFFER_BIT);
        winsys_swap_buffers();
        glClear(GL_COLOR_BUFFER_BIT);
        winsys_swap_buffers();
    }
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    winsys_show_cursor(true);
}

void GFXLoop(void main_loop()) {
    winsys_set_display_func(main_loop);
    winsys_set_idle_func(main_loop);

    //glutDisplayFunc(main_loop);
    //glutIdleFunc (main_loop);
    static bool are_we_looping = false;
    /// so we can call this function multiple times to change the display and idle functions
    if (!are_we_looping) {
        are_we_looping = true;
        winsys_process_events();
    }
}

void GFXShutdown() {
    extern void GFXDestroyAllLights();

    vsgl::ShutdownRenderer();
    GFXDestroyAllTextures();
    GFXDestroyAllLights();
    if (configuration().graphics.full_screen) {
        winsys_shutdown();
    }
}
