/*
 * planet_halo.h
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
#ifndef VEGA_STRIKE_ENGINE_GFX_PLANET_HALO_H
#define VEGA_STRIKE_ENGINE_GFX_PLANET_HALO_H

#include <string>

#include "src/gfxlib_struct.h"
#include "gfx_generic/vec.h"

/**
 * Atmospheric halos round planets, styled per planet type from the data
 * set's planet_halos.json:
 *
 *   {
 *     "enabled": true,
 *     "defaults": { "thickness": 0.06, "falloff": 0.3, "density": 0.15, "night": 0.05 },
 *     "types":   { "agricultural": { "color": [0.35, 0.6, 1.0] }, ... },
 *     "planets": { "New_Detroit": { "color": [0.55, 0.52, 0.46], "thickness": 0.1 } }
 *   }
 *
 * A planet's type is its texture's name ("planets/agricultural.png" ->
 * "agricultural"). "planets" entries (by planet name) win over "types";
 * a "*" type applies to every other planet. Any entry may be false to give
 * no halo. Fields an entry leaves out come from "defaults":
 *   color      [r, g, b], 0..1
 *   thickness  atmosphere height, as a fraction of the planet's radius
 *   falloff    scale height of the air, as a fraction of the thickness
 *              (small: thin, sharp rim; large: deep, even haze)
 *   density    how opaque the air is (raise for smog)
 *   night      brightness of the unlit side, 0..1
 * Suns and jump points never get a halo.
 */
namespace PlanetHalos {

struct Style {
    GFXColor color{0.4f, 0.65f, 1.0f, 1.0f};
    float thickness = 0.06f;
    float falloff = 0.3f;
    float density = 0.15f;
    float night = 0.05f;
};

/// The style for a planet, by its name and then its type; nullptr for no halo
const Style *StyleFor(const std::string &planet_name, const std::string &type);

/// Drops halos queued but not drawn (a pass that did not run); call at the start of a frame
void BeginFrame();

/// Queues a halo for this frame (call when the planet is drawn)
void Queue(const QVector &position, float radius, const Style *style);

/// Draws the halos of planets drawn in the far pass (end of Mesh::ProcessZFarMeshes)
void DrawFar();

/// Draws the halos of planets drawn in the near pass (after Mesh::ProcessUndrawnMeshes)
void DrawNear();

} // namespace PlanetHalos

#endif //VEGA_STRIKE_ENGINE_GFX_PLANET_HALO_H
