/*
 * planet_halo.cpp
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
#include "gfx/planet_halo.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <vector>

#include <boost/format.hpp>
#include <boost/json.hpp>

#include "src/gfxlib.h"
#include "src/universe.h"
#include "src/star_system.h"
#include "src/vs_logging.h"
#include "cmd/unit_generic.h"
#include "cmd/unit_util.h"
#include "configuration/configuration.h"
#include "vegadisk/vsfilesystem.h"

namespace PlanetHalos {
namespace {

const char *const kConfigFile = "planet_halos.json";

struct Table {
    bool loaded = false;
    bool enabled = true;
    std::map<std::string, std::unique_ptr<Style>> types;    // nullptr: no halo
    std::map<std::string, std::unique_ptr<Style>> planets;
    bool has_wildcard = false;
    std::unique_ptr<Style> wildcard;
};

Table g_table;

struct Queued {
    QVector position;
    float radius;
    const Style *style;
};

std::vector<Queued> g_near;
std::vector<Queued> g_far;

std::string Key(std::string s) {
    for (char &c : s) {
        c = (c == ' ') ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

float Number(const boost::json::object &o, const char *name, float fallback) {
    if (const boost::json::value *v = o.if_contains(name)) {
        if (v->is_number()) {
            return static_cast<float>(v->to_number<double>());
        }
        VS_LOG(warning, (boost::format("%1%: \"%2%\" should be a number") % kConfigFile % name));
    }
    return fallback;
}

Style ParseStyle(const boost::json::object &o, const Style &base) {
    Style s = base;
    if (const boost::json::value *v = o.if_contains("color")) {
        const boost::json::array *a = v->if_array();
        if (a && a->size() >= 3 && (*a)[0].is_number() && (*a)[1].is_number() && (*a)[2].is_number()) {
            s.color = GFXColor(static_cast<float>((*a)[0].to_number<double>()),
                               static_cast<float>((*a)[1].to_number<double>()),
                               static_cast<float>((*a)[2].to_number<double>()), 1.0f);
        } else {
            VS_LOG(warning, (boost::format("%1%: \"color\" should be [r, g, b]") % kConfigFile));
        }
    }
    s.thickness = std::max(Number(o, "thickness", s.thickness), 0.0f);
    s.falloff = std::max(Number(o, "falloff", s.falloff), 0.01f);
    s.density = std::max(Number(o, "density", s.density), 0.0f);
    s.night = std::min(std::max(Number(o, "night", s.night), 0.0f), 1.0f);
    if (const boost::json::value *v = o.if_contains("glow")) {
        if (v->is_bool()) {
            s.glow = v->as_bool();
        } else {
            VS_LOG(warning, (boost::format("%1%: \"glow\" should be true or false") % kConfigFile));
        }
    }
    return s;
}

/// An entry: an object gives a style, false/null gives no halo
std::unique_ptr<Style> ParseEntry(const boost::json::value &v, const Style &base, const std::string &name) {
    if (v.is_object()) {
        return std::unique_ptr<Style>(new Style(ParseStyle(v.as_object(), base)));
    }
    if (!(v.is_null() || (v.is_bool() && !v.as_bool()))) {
        VS_LOG(warning, (boost::format("%1%: entry \"%2%\" should be an object or false") % kConfigFile % name));
    }
    return nullptr;
}

void Load() {
    g_table.loaded = true;
    VSFileSystem::VSFile file;
    if (file.OpenReadOnly(kConfigFile, VSFileSystem::UnknownFile) > VSFileSystem::Ok) {
        VS_LOG(info, (boost::format("No %1%: planets have no atmospheric halos") % kConfigFile));
        g_table.enabled = false;
        return;
    }
    const std::string text = file.ReadFull();
    file.Close();
    try {
        const boost::json::object root = boost::json::parse(text).as_object();
        if (const boost::json::value *v = root.if_contains("enabled")) {
            g_table.enabled = v->is_bool() ? v->as_bool() : true;
        }
        Style defaults;
        if (const boost::json::value *v = root.if_contains("defaults")) {
            if (v->is_object()) {
                defaults = ParseStyle(v->as_object(), defaults);
            }
        }
        if (const boost::json::value *v = root.if_contains("types")) {
            if (v->is_object()) {
                for (const auto &entry : v->as_object()) {
                    const std::string name(entry.key());
                    if (name == "*") {
                        g_table.has_wildcard = true;
                        g_table.wildcard = ParseEntry(entry.value(), defaults, name);
                    } else {
                        g_table.types[Key(name)] = ParseEntry(entry.value(), defaults, name);
                    }
                }
            }
        }
        if (const boost::json::value *v = root.if_contains("planets")) {
            if (v->is_object()) {
                for (const auto &entry : v->as_object()) {
                    const std::string name(entry.key());
                    g_table.planets[Key(name)] = ParseEntry(entry.value(), defaults, name);
                }
            }
        }
        VS_LOG(info, (boost::format("Loaded %1%: %2% planet types, %3% planets") % kConfigFile
                % g_table.types.size() % g_table.planets.size()));
    } catch (const std::exception &e) {
        VS_LOG(error, (boost::format("Could not read %1%: %2%; planets have no atmospheric halos")
                % kConfigFile % e.what()));
        g_table.enabled = false;
    }
}

/// Positions of the suns of the active star system
std::vector<QVector> Suns() {
    std::vector<QVector> suns;
    StarSystem *system = _Universe->activeStarSystem();
    if (!system) {
        return suns;
    }
    Unit *un;
    for (un_iter it = system->gravitationalUnits().createIterator(); (un = *it) != nullptr; ++it) {
        if (UnitUtil::isSun(un)) {
            suns.push_back(un->Position());
        }
    }
    return suns;
}

void Draw(std::vector<Queued> &queue) {
    if (queue.empty()) {
        return;
    }
    const QVector camera = _Universe->AccessCamera()->GetPosition();
    const std::vector<QVector> suns = Suns();
    std::vector<GFXPlanetHalo> halos;
    halos.reserve(queue.size());
    for (const Queued &q : queue) {
        GFXPlanetHalo h;
        h.center = (q.position - camera).Cast();
        h.radius = q.radius;
        h.thickness = q.radius * q.style->thickness;
        h.falloff = q.style->falloff;
        h.color = q.style->color;
        h.density = q.style->density;
        h.night = q.style->night;
        h.additive = q.style->glow;
        // Lit by the nearest sun; with none, by the camera's direction
        QVector to_light = camera - q.position;
        double best = -1;
        for (const QVector &sun : suns) {
            const double d = (sun - q.position).MagnitudeSquared();
            if (best < 0 || d < best) {
                best = d;
                to_light = sun - q.position;
            }
        }
        h.sun = to_light.Cast();
        h.sun.Normalize();
        halos.push_back(h);
    }
    // Back to front, so nearer halos blend over farther ones
    std::sort(halos.begin(), halos.end(), [](const GFXPlanetHalo &a, const GFXPlanetHalo &b) {
        return a.center.MagnitudeSquared() > b.center.MagnitudeSquared();
    });
    GFXDrawPlanetHalos(halos);
    queue.clear();
}

} // namespace

const Style *StyleFor(const std::string &planet_name, const std::string &type) {
    if (!g_table.loaded) {
        Load();
    }
    if (!g_table.enabled) {
        return nullptr;
    }
    auto planet = g_table.planets.find(Key(planet_name));
    if (planet != g_table.planets.end()) {
        return planet->second.get();
    }
    auto found = g_table.types.find(Key(type));
    if (found != g_table.types.end()) {
        return found->second.get();
    }
    return g_table.has_wildcard ? g_table.wildcard.get() : nullptr;
}

void Queue(const QVector &position, float radius, const Style *style) {
    if (!style || radius <= 0 || style->thickness <= 0) {
        return;
    }
    // Same test Mesh::Draw uses to put a mesh in the far pass
    const double distance = (position - _Universe->AccessCamera()->GetPosition()).Magnitude();
    const double outer = radius * (1.0 + style->thickness);
    Queued q{position, radius, style};
    if (distance + outer > configuration().graphics.zfar_dbl) {
        g_far.push_back(q);
    } else {
        g_near.push_back(q);
    }
}

void BeginFrame() {
    g_near.clear();
    g_far.clear();
}

void DrawFar() {
    Draw(g_far);
}

void DrawNear() {
    Draw(g_near);
}

} // namespace PlanetHalos
