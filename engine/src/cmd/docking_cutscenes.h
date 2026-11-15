/*
 * docking_cutscenes.h
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
#ifndef VEGA_STRIKE_ENGINE_CMD_DOCKING_CUTSCENES_H
#define VEGA_STRIKE_ENGINE_CMD_DOCKING_CUTSCENES_H

#include "cmd/container.h"

/**
 * Lets a script play a cutscene when the player lands on or takes off from
 * a planet.
 *
 * While enabled:
 *  - a player dock with a planet that would succeed is deferred instead:
 *    the planet is recorded as the pending landing and the player stays in
 *    flight. The script picks it up (TakePendingLanding), plays its shot and
 *    then completes the dock (PlayerCompleteLanding in firekeyboard.cpp),
 *    or abandons it (CancelLanding).
 *  - the base the player undocks from is recorded as the pending takeoff.
 *
 * Docking with anything that is not a planet is never deferred.
 */
namespace DockingCutscenes {

struct State {
    bool enabled = false;
    /// A landing is deferred: waiting for the script, or its shot is playing
    bool landing_in_progress = false;
    /// Set while PlayerCompleteLanding docks, so its attempts are let through
    bool completing = false;
    /// Game time the landing was deferred, to notice a script that never takes it
    float deferred_at = 0;
    UnitContainer pending_landing;
    UnitContainer pending_takeoff;
};

inline State &state() {
    static State s;
    return s;
}

inline void SetEnabled(bool enabled) {
    State &s = state();
    s.enabled = enabled;
    if (!enabled) {
        s.landing_in_progress = false;
        s.pending_landing.SetUnit(nullptr);
        s.pending_takeoff.SetUnit(nullptr);
    }
}

inline bool Enabled() {
    return state().enabled;
}

inline Unit *TakePendingLanding() {
    Unit *un = state().pending_landing.GetUnit();
    state().pending_landing.SetUnit(nullptr);
    return un;
}

inline Unit *TakePendingTakeoff() {
    Unit *un = state().pending_takeoff.GetUnit();
    state().pending_takeoff.SetUnit(nullptr);
    return un;
}

/// Docks the player with the planet of a deferred landing, as the dock key
/// would (base screen included). False if the dock is not possible yet;
/// the landing then stays deferred so the script can retry or cancel.
bool PlayerCompleteLanding(Unit *player, Unit *planet);

inline void CancelLanding() {
    state().landing_in_progress = false;
    state().pending_landing.SetUnit(nullptr);
}

} // namespace DockingCutscenes

#endif //VEGA_STRIKE_ENGINE_CMD_DOCKING_CUTSCENES_H
