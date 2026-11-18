// simcore/air_war/air_fight_def.hpp — a session's air battle (roadmap track B, B8; docs/air_battle/07_AIR_TO_AIR.md
// section 5): which of the scenario's aircraft fight in the air, for which side, under which controller, and the data
// they fight by (the scenario's "air_fight" block). Played by AirFightSession (air_fight_session.hpp).
//
//   "air_fight": {
//     "year": 1985,
//     "fighters": [ { "target": "eagle1", "type": "f15c", "faction": "nato", "role": "sweep", "controller": "e3a" },
//                   { "target": "red11", "type": "mig21mf", "faction": "ours", "regiment": "fighter_rgt_1", "controller": "kecel_gci" } ],
//     "controllers": [ { "id": "e3a", "side": "enemy", "kind": "airborne_warning", "east_m": -80000, "north_m": 0, "alt_m": 9000,
//                        "range_m": 350000, "post": "e3a" },
//                      { "id": "kecel_gci", "side": "ours", "kind": "radar_control", "east_m": 0, "north_m": -20000, "range_m": 150000 } ]
//   }
//
// A fighter is a target of the scenario (in a flight, so its pilot can be steered). A controller's `post` names the
// session's air radio post its vectors go out on (B9's channels and the EM link, so they can be lost or jammed); a
// controller with none is heard at once (our side's nets are B10's). Our fighters here are the minimal ones a test or a
// range day needs; our GCI, alert and corridors as actors are B10's.
#pragma once

#include <string>
#include <vector>

#include "simcore/air_war/air_combat_types.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

struct AirFightFighterDef {
    std::string target;      // the scenario target's id
    std::string type;        // the aircraft type (its fit in air_combat.json, its sensors in sensors.json)
    std::string faction;     // nato, warsaw_pact, neutral, rebels, ours: its doctrine
    std::string role = "escort";  // escort, sweep, cap, intercept, strike (a striker only defends itself)
    std::string regiment;    // ours: the regiment a kill is written to
    std::string controller;  // the controller's id, or none
    double skill = -1.0;     // < 0: the doctrine's
};

struct AirFightControllerDef {
    std::string id;
    std::string side = "ours";             // ours, enemy
    std::string kind = "radar_control";    // radar_control (a GCI post), airborne_warning (E-3A)
    Vec3 pos;                              // ENU (the scenario's)
    double range_m = 150000.0;
    std::string post;                      // the air radio post its vectors go out on ("": heard at once)
};

struct AirFightDef {
    bool present = false;
    int year = 1985;
    std::vector<AirFightFighterDef> fighters;
    std::vector<AirFightControllerDef> controllers;
    AirCombatDef combat;  // data/air_war/air_combat.json
    SensorsDef sensors;   // data/air_war/sensors.json

    const AirFightControllerDef* Controller(const std::string& id) const {
        for (const AirFightControllerDef& c : controllers) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }
};

}  // namespace airwar
}  // namespace simcore
