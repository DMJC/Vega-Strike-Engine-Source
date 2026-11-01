/*
 * glut_support.cpp
 *
 * Vega Strike - Space Simulation, Combat and Trading
 * Copyright (C) 2001-2026 The Vega Strike Contributors:
 * Project creator: Daniel Horn
 * Original development team: As listed in the AUTHORS file. Specifically: David Ranger
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

#include <png.h>
#include "glut_support.h"
#include "src/vs_logging.h"
#include "gldrv/gl_globals.h"
#include "gldrv/gl_ffp_compat.h"
#include "src/gfxlib.h"
#include "configuration/configuration.h"


#define isspAce(chr)                                                                  \
    ( (chr == '\t') || (chr == '\n') || (chr == '\v') || (chr == '\f') || (chr == '\r') \
     || (chr == ' ') )

float colors[] = {1, 1, 1, 1};

//x,y must be -1 to 1, with -1,-1 being the bottom left corner. x,y must be the top left corner to draw
void ShowColor(float x, float y, float wid, float hei, float red, float green, float blue, float alpha) {
    float cols[4] = {red, green, blue, alpha};
    if (wid < 0 || hei < 0) {
        VS_LOG(error, "Cannot draw color with negative height or width");
        return;
    }
    //Make sure we don't exceed the program
    if (x + wid > 1) {
        wid = 1 - x;
    }
    if (y - hei < -1) {
        hei = -1 + y;
    }
#ifdef DEBUG
    VS_LOG(debug, (boost::format("Displaying color at %1%,%2%") % x % y));
    VS_LOG(debug, (boost::format("with the dimensions of %1%,%2%") % wid % hei));
    VS_LOG(debug, (boost::format("With the color %1%,%2%,%3%,%4%") % red % green % blue % alpha));
    VS_LOG(debug, "-----------------------------");
#endif
    const float verts[4 * 6] = {
            x, y - hei, cols[0], cols[1], cols[2], cols[3],
            x, y, cols[0], cols[1], cols[2], cols[3],
            x + wid, y, cols[0], cols[1], cols[2], cols[3],
            x + wid, y - hei, cols[0], cols[1], cols[2], cols[3],
    };
    vsglDisable(GL_TEXTURE_2D);
    GFXDraw(GFXQUAD, verts, 4, 2, 4);
    vsglEnable(GL_TEXTURE_2D);
}

float word_length(const char *str) {
    float length = 0;
    while (*str && *str != '\\' && (!isspAce(*str))) {
        char mychar = *str++;
        length += glutStrokeWidth(GLUT_STROKE_ROMAN, mychar);
    }
    return length / 2500;
}

void ShowText(float x, float y, float wid, int size, const char *str, int no_end) {
    // GLUT stroke fonts draw with immediate mode inside freeglut, which a core
    // profile does not have. Nothing instantiates the legacy TextArea/Button
    // that used this; the game's text goes through TextPlane.
    static bool warned = false;
    if (!warned) {
        warned = true;
        VS_LOG(warning, "ShowText: GLUT stroke text is not available on the OpenGL core profile");
    }
}

float WidthOfChar(char chr) {
    float width = glutStrokeWidth(GLUT_STROKE_ROMAN, chr);
    width /= 2500;
    return width;
}
