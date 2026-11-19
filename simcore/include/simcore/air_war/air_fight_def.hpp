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
//
// Roadmap S3 (the fight in detail, docs/air_battle/07_AIR_TO_AIR.md section 9): "mode" 1 (the default) flies the
// doctrine's tactics (beam, slash, turn) and the drag, the missiles as points with their energy, the fighters' radars as
// emitters with their look-down, the strikers' chaff and flares, the escort's station and the far fight at A1; 0 is B8's
// fight. "far_fight" (true or false) overrides the far fight. A fighter's "escorts" names the striker whose flight it
// guards. A controller vectors only the fighters an authority line of its faction lets it (the faction file's c2
// "vector" lines, read into `vector_lines`; a faction with none keeps B8's rule: any fighter assigned to it).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "simcore/air_war/air_combat_types.hpp"
#include "simcore/air_war/c2_def.hpp"
#include "simcore/em/model.hpp"
#include "simcore/json.hpp"
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
    std::string escorts;     // an escort: the striker (a target id) whose flight it guards ("": the nearest of its side's)
    // Ours with a base (S1, a pair the duty scrambled): sent home, he flies to it, not away from the fight and on.
    bool has_home = false;
    Vec3 home;               // ENU (the scenario's)
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
    int mode = 1;            // roadmap S3's fight (1) or B8's (0)
    int far_fight = -1;      // the far fight at A1: -1 with mode 1, 0 off, 1 on
    // Each faction's "vector" lines from its c2 authority table (roadmap S3): which post kind may vector which fighters.
    std::map<std::string, std::vector<C2AuthorityDef>> vector_lines;
    std::vector<AirFightFighterDef> fighters;
    std::vector<AirFightControllerDef> controllers;
    AirCombatDef combat;  // data/air_war/air_combat.json
    SensorsDef sensors;   // data/air_war/sensors.json
    em::EmLibrary radar_library;  // S3: the libraries its radars' EM entries are in (data/em/fighter_radars_1985.json), merged

    bool FarFight() const { return far_fight < 0 ? mode >= 1 : far_fight > 0; }
    // May a controller of `post_kind` vector a fighter of `faction` flying `role` in this war? A faction with no vector lines
    // keeps B8's rule (any fighter assigned to it); before S3 (mode 0) every assigned fighter is vectored.
    bool MayVector(const std::string& faction, const std::string& post_kind, const std::string& role) const {
        if (mode < 1) return true;
        const auto it = vector_lines.find(faction);
        if (it == vector_lines.end() || it->second.empty()) return true;
        for (const C2AuthorityDef& a : it->second) {
            if (a.post != post_kind || a.act != "vector" || a.from_year > year) continue;
            for (const std::string& m : a.missions) {
                if (m == role || m == "fighters") return true;
            }
        }
        return false;
    }

    const AirFightControllerDef* Controller(const std::string& id) const {
        for (const AirFightControllerDef& c : controllers) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }
};

// A faction file's "vector" lines (its c2 block's authority table, act "vector"), from the file's text. The faction's own
// loader (data.hpp) checks the table in full; this takes the lines only. False only if the text is not JSON.
inline bool VectorLinesFromJson(const std::string& text, std::vector<C2AuthorityDef>& out) {
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) return false;
    const json::Value* c2 = root.Find("c2");
    const json::Value* auth = c2 != nullptr ? c2->Find("authority") : nullptr;
    if (auth == nullptr || !auth->Is(json::Type::Array)) return true;
    for (const json::Value& v : auth->items) {
        const json::Value* act = v.Find("act");
        if (act == nullptr || act->string != "vector") continue;
        C2AuthorityDef a;
        a.act = "vector";
        if (const json::Value* post = v.Find("post")) a.post = post->string;
        if (const json::Value* ms = v.Find("missions")) {
            for (const json::Value& m : ms->items) a.missions.push_back(m.string);
        }
        if (const json::Value* y = v.Find("from_year")) a.from_year = static_cast<int>(y->number);
        out.push_back(a);
    }
    return true;
}

}  // namespace airwar
}  // namespace simcore
