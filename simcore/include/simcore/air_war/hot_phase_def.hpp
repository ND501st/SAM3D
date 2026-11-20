// simcore/air_war/hot_phase_def.hpp — a faction's hot-phase profile as data (docs/air_battle/10_FACTIONS.md §7, roadmap
// B11): how its flights make the choices in the air (act or report, press on or turn back, talk or keep silent), each
// value with the bound the sources set, its source and confidence; its leaders' authority as gates; its latencies from
// the ground and in the air. Read by hot_phase.hpp (`HotPhase`, the officer's and the pilot's shifts).
//
// The block, in each faction file and in our side's data/command/own_air.json:
//
//   "hot_phase": {
//     "initiative": {"value": 0.25, "range": [0.1, 0.4], "source": "...", "confidence": "estimate"},
//     "press_on":   {"value": 0.6, "by_role": {"escort": 0.4}, "when_control_lost": true, "range": [0.4, 0.7], ...},
//     ... every key of HotParamName ...,
//     "reaction_delay_s": {"value": [2, 5], "improvised_extra_s": 2, "source": "...", "confidence": "..."},
//     "alert": [ {"id", "share", "takeoff_s", "hold_h", "airborne", "phase_scale", "source", "confidence"} ],
//     "retask_air": {"delay_s": [600, 1800], "never": 0.5, "source", "confidence"},
//     "authority": {"beyond_task", "free_hunt_from_phase", "free_hunt_depth_km", "retask_by", "retask_kinds",
//                   "recall_only", "leader_may", "source", "confidence"},
//     "on_control_lost": {"green": "abort", "skilled": "pairs", "source", "confidence"},
//     "source": "...", "confidence": "..."
//   }
//
// Strict: an unknown key fails, a value outside its `range` fails, a `range` outside 0..1 fails, a missing source or
// confidence fails. This header has no air war loader of its own beyond the block, so our side's file (command/own_air.hpp)
// reads it too.
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "simcore/air_war/c2_def.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace airwar {

// The 0..1 parameters (§7.1). reaction_delay_s is a band, not one of these.
enum class HotParam { Initiative, PlanAdherence, PressOn, Jettison, RadioDiscipline, EmissionDiscipline, AirCombat, Opportunity, TurnBack, Husbanding, Count };
constexpr int kHotParams = static_cast<int>(HotParam::Count);

inline const char* HotParamName(HotParam p) {
    switch (p) {
        case HotParam::Initiative: return "initiative";
        case HotParam::PlanAdherence: return "plan_adherence";
        case HotParam::PressOn: return "press_on";
        case HotParam::Jettison: return "jettison_readiness";
        case HotParam::RadioDiscipline: return "radio_discipline";
        case HotParam::EmissionDiscipline: return "emission_discipline";
        case HotParam::AirCombat: return "air_combat_skill";
        case HotParam::Opportunity: return "opportunity";
        case HotParam::TurnBack: return "turn_back";
        case HotParam::Husbanding: return "husbanding";
        case HotParam::Count: break;
    }
    return "?";
}

// One parameter: the faction's value, the bound its sources set, a value per flight role where the faction differs by
// role (when_control_lost: only once the flight has lost its controller), with its source.
struct HotValue {
    bool set = false;
    double value = 0.0;
    double lo = 0.0, hi = 1.0;
    double by_role[static_cast<int>(FlightRole::Count)] = {};
    bool has_role[static_cast<int>(FlightRole::Count)] = {};
    bool when_control_lost = false;
    std::string source;
    std::string confidence;

    // The faction's value for a flight of this role (control_lost: the flight has lost its controller).
    double For(FlightRole r, bool control_lost = false) const {
        const int i = static_cast<int>(r);
        if (i >= 0 && i < static_cast<int>(FlightRole::Count) && has_role[i] && (!when_control_lost || control_lost)) return by_role[i];
        return value;
    }
};

// The reaction delay of the faction's trained pilot from cue to manoeuvre (04_REACTIONS.md §4), and what an improvised
// manoeuvre adds (the Pact's preset programme is quick; anything else is not).
struct HotDelayDef {
    bool set = false;
    double lo_s = 2.0, hi_s = 5.0;
    double improvised_extra_s = 0.0;
    // Our side's (§7.2): the band with its nets jammed, its GCI's warning not reaching it (roadmap S5). < 0: none.
    double jammed_lo_s = -1.0, jammed_hi_s = -1.0;
    std::string source;
    std::string confidence;

    bool HasJammed() const { return jammed_lo_s >= 0.0 && jammed_hi_s >= jammed_lo_s; }
};

// The faction's pilots as the hot phase reads them (§8.3, roadmap S5): the hours its pilots fly in a peacetime year
// (their currency is these over the reference, data/gameplay/air_war.json hot_phase.currency_reference_h) and the combat
// sorties a pilot has survived when the war starts (its veterans).
struct HotPilotsDef {
    bool set = false;
    double hours_per_year = 0.0;
    double start_sorties = 0.0;
    std::string source;
    std::string confidence;
    std::string notes;
};

// An airborne flight changed (§7.4): within delay_s through the faction's posts, for the mission kinds its authority
// lets them retask; any other mission never with the chance `never` (1: never at all).
struct RetaskAirDef {
    bool set = false;
    C2Band delay_s;
    double never = 1.0;
    std::string source;
    std::string confidence;
    std::string notes;
};

// The gates (§7.3): lists, not numbers. A flight acts only inside them.
struct HotAuthorityDef {
    bool set = false;
    std::vector<std::string> beyond_task;   // missions whose flights may attack outside their task (MissionKind names)
    int free_hunt_from_phase = -1;          // the doctrine's phase (from 0) the free hunt opens in; -1 never
    double free_hunt_depth_km = 0.0;        // its zone: this deep behind our side's line
    std::vector<std::string> retask_by;     // post kinds that may change an airborne flight's task (divert or designate)
    std::vector<std::string> retask_kinds;  // the missions they may retask ("bai" for NATO's battlefield interdiction)
    bool recall_only = false;               // its posts may recall a flight and nothing more (the neutral)
    std::vector<std::string> leader_may;    // what the flight leader may change himself
    std::string source;
    std::string confidence;
    std::string notes;

    bool RetaskKind(const std::string& mission) const {
        return std::find(retask_kinds.begin(), retask_kinds.end(), mission) != retask_kinds.end();
    }
    bool BeyondTask(const std::string& mission) const {
        return std::find(beyond_task.begin(), beyond_task.end(), mission) != beyond_task.end();
    }
    bool LeaderMay(const std::string& act) const { return std::find(leader_may.begin(), leader_may.end(), act) != leader_may.end(); }
};

// What a flight does when it loses its controller, by its pilots' skill (07_AIR_TO_AIR.md §4.2, §5.1).
struct HotControlLostDef {
    bool set = false;
    std::string green = "abort";
    std::string skilled = "pairs";
    std::string source;
    std::string confidence;
};

struct HotPhaseDef {
    bool present = false;
    HotValue values[kHotParams];
    HotDelayDef reaction_delay;
    std::vector<C2AlertDef> alert;  // the alert pools' take-off and hold (the same ids as the C2 posts' pools)
    RetaskAirDef retask_air;
    HotAuthorityDef authority;
    HotControlLostDef on_control_lost;
    HotPilotsDef pilots;
    std::string source;
    std::string confidence;
    std::string notes;

    const HotValue& Value(HotParam p) const { return values[static_cast<int>(p)]; }
    const C2AlertDef* Alert(const std::string& id) const {
        for (const C2AlertDef& a : alert) {
            if (a.id == id) return &a;
        }
        return nullptr;
    }
};

// The leader's acts a faction may give its flight leaders (§7.3, third column).
inline const std::vector<std::string>& HotLeaderActs() {
    static const std::vector<std::string> k = {"route",  "attack_mode", "direction", "release_height", "alternate", "abort",
                                               "targets_in_flight", "programme", "attack", "break_off", "everything"};
    return k;
}

namespace hot_detail {

struct Reader {
    std::string* error = nullptr;
    bool Fail(const std::string& path, const std::string& what) const {
        if (error != nullptr) *error = path + ": " + what;
        return false;
    }
    bool Keys(const json::Value& v, const std::string& path, const std::vector<std::string>& allowed) const {
        if (!v.Is(json::Type::Object)) return Fail(path, "expected an object");
        for (const std::string& k : v.keys) {
            if (std::find(allowed.begin(), allowed.end(), k) == allowed.end()) return Fail(path + "." + k, "unknown key");
        }
        return true;
    }
    bool Cite(const json::Value& v, const std::string& path, std::string& source, std::string& confidence) const {
        const json::Value* s = v.Find("source");
        const json::Value* c = v.Find("confidence");
        if (s == nullptr || !s->Is(json::Type::String) || s->string.empty()) return Fail(path + ".source", "required");
        if (c == nullptr || !c->Is(json::Type::String)) return Fail(path + ".confidence", "required");
        if (c->string != "high" && c->string != "medium" && c->string != "low" && c->string != "estimate") {
            return Fail(path + ".confidence", "must be high, medium, low or estimate");
        }
        source = s->string;
        confidence = c->string;
        return true;
    }
    bool Pair(const json::Value& v, const std::string& path, double& lo, double& hi, double lowest, double highest) const {
        if (!v.Is(json::Type::Array) || v.items.size() != 2 || !v.items[0].Is(json::Type::Number) || !v.items[1].Is(json::Type::Number)) {
            return Fail(path, "expected [lo, hi]");
        }
        lo = v.items[0].number;
        hi = v.items[1].number;
        if (lo < lowest || hi > highest || lo > hi) return Fail(path, "lo <= hi, within the allowed span");
        return true;
    }
    bool Strings(const json::Value& v, const std::string& path, std::vector<std::string>& out) const {
        if (!v.Is(json::Type::Array)) return Fail(path, "expected a list of words");
        for (const json::Value& x : v.items) {
            if (!x.Is(json::Type::String)) return Fail(path, "expected a list of words");
            out.push_back(x.string);
        }
        return true;
    }
    bool Number(const json::Value& v, const std::string& path, double& out, double lo, double hi) const {
        if (!v.Is(json::Type::Number) || v.number < lo || v.number > hi) return Fail(path, "a number in range");
        out = v.number;
        return true;
    }
};

inline bool Mission(const std::string& m) {
    MissionKind k;
    return m == "bai" || ParseMissionKind(m, k);
}

inline bool LoadValue(const Reader& r, const json::Value& v, const std::string& p, HotValue& out) {
    if (!r.Keys(v, p, {"value", "range", "by_role", "when_control_lost", "source", "confidence", "notes"}) ||
        !r.Cite(v, p, out.source, out.confidence)) {
        return false;
    }
    const json::Value* val = v.Find("value");
    const json::Value* rng = v.Find("range");
    if (val == nullptr || !val->Is(json::Type::Number)) return r.Fail(p + ".value", "required, a number");
    if (rng == nullptr) return r.Fail(p + ".range", "required: the bound the sources set");
    if (!r.Pair(*rng, p + ".range", out.lo, out.hi, 0.0, 1.0)) return false;
    out.value = val->number;
    if (out.value < out.lo || out.value > out.hi) return r.Fail(p + ".value", "outside its range");
    if (const json::Value* br = v.Find("by_role")) {
        if (!br->Is(json::Type::Object)) return r.Fail(p + ".by_role", "expected {role: value}");
        for (std::size_t k = 0; k < br->keys.size() && k < br->items.size(); ++k) {
            FlightRole fr;
            if (!ParseFlightRole(br->keys[k], fr)) return r.Fail(p + ".by_role." + br->keys[k], "not a flight role");
            const int i = static_cast<int>(fr);
            if (!br->items[k].Is(json::Type::Number)) return r.Fail(p + ".by_role." + br->keys[k], "a number");
            out.by_role[i] = br->items[k].number;
            out.has_role[i] = true;
            if (out.by_role[i] < out.lo || out.by_role[i] > out.hi) return r.Fail(p + ".by_role." + br->keys[k], "outside its range");
        }
    }
    if (const json::Value* w = v.Find("when_control_lost")) {
        if (!w->Is(json::Type::Bool)) return r.Fail(p + ".when_control_lost", "true or false");
        out.when_control_lost = w->boolean;
    }
    out.set = true;
    return true;
}

inline bool LoadAlert(const Reader& r, const json::Value& v, const std::string& p, C2AlertDef& a) {
    if (!r.Keys(v, p, {"id", "share", "takeoff_s", "hold_h", "airborne", "phase_scale", "source", "confidence", "notes"}) ||
        !r.Cite(v, p, a.source, a.confidence)) {
        return false;
    }
    const json::Value* id = v.Find("id");
    if (id == nullptr || !id->Is(json::Type::String) || id->string.empty()) return r.Fail(p + ".id", "required");
    a.id = id->string;
    if (const json::Value* s = v.Find("share")) {
        if (!r.Number(*s, p + ".share", a.share, 0.0, 1.0)) return false;
    }
    if (const json::Value* t = v.Find("takeoff_s")) {
        if (!r.Pair(*t, p + ".takeoff_s", a.takeoff_s.lo, a.takeoff_s.hi, 0.0, 86400.0)) return false;
    }
    const json::Value* h = v.Find("hold_h");
    if (h == nullptr) return r.Fail(p + ".hold_h", "required, [lo, hi] hours");
    if (!r.Pair(*h, p + ".hold_h", a.hold_h.lo, a.hold_h.hi, 0.0, 240.0)) return false;
    if (const json::Value* ab = v.Find("airborne")) {
        if (!ab->Is(json::Type::Bool)) return r.Fail(p + ".airborne", "true or false");
        a.airborne = ab->boolean;
    }
    if (!a.airborne && !a.takeoff_s.Set()) return r.Fail(p + ".takeoff_s", "required for a pool on the ground");
    if (const json::Value* ps = v.Find("phase_scale")) {
        if (!ps->Is(json::Type::Array)) return r.Fail(p + ".phase_scale", "a list of numbers 0 .. 1");
        for (const json::Value& x : ps->items) {
            if (!x.Is(json::Type::Number) || x.number < 0.0 || x.number > 1.0) return r.Fail(p + ".phase_scale", "a list of numbers 0 .. 1");
            a.phase_scale.push_back(x.number);
        }
    }
    if (const json::Value* n = v.Find("notes")) a.notes = n->Is(json::Type::String) ? n->string : std::string();
    return true;
}

}  // namespace hot_detail

// Read a "hot_phase" block. Every parameter of HotParamName and reaction_delay_s are required; alert, retask_air,
// authority and on_control_lost are optional (a faction without posts has none).
inline bool LoadHotPhase(const json::Value& b, const std::string& p, HotPhaseDef& out, std::string* error) {
    hot_detail::Reader r;
    r.error = error;
    std::vector<std::string> keys = {"reaction_delay_s", "alert", "retask_air", "authority", "on_control_lost", "pilots", "source", "confidence", "notes"};
    for (int i = 0; i < kHotParams; ++i) keys.push_back(HotParamName(static_cast<HotParam>(i)));
    if (!r.Keys(b, p, keys) || !r.Cite(b, p, out.source, out.confidence)) return false;
    if (const json::Value* n = b.Find("notes")) out.notes = n->Is(json::Type::String) ? n->string : std::string();
    for (int i = 0; i < kHotParams; ++i) {
        const std::string key = HotParamName(static_cast<HotParam>(i));
        const json::Value* v = b.Find(key.c_str());
        if (v == nullptr) return r.Fail(p + "." + key, "required");
        if (!hot_detail::LoadValue(r, *v, p + "." + key, out.values[i])) return false;
    }
    {
        const std::string dp = p + ".reaction_delay_s";
        const json::Value* d = b.Find("reaction_delay_s");
        if (d == nullptr) return r.Fail(dp, "required");
        if (!r.Keys(*d, dp, {"value", "improvised_extra_s", "jammed_s", "source", "confidence", "notes"}) ||
            !r.Cite(*d, dp, out.reaction_delay.source, out.reaction_delay.confidence)) {
            return false;
        }
        const json::Value* val = d->Find("value");
        if (val == nullptr) return r.Fail(dp + ".value", "required, [min, max] seconds");
        if (!r.Pair(*val, dp + ".value", out.reaction_delay.lo_s, out.reaction_delay.hi_s, 0.0, 60.0)) return false;
        if (const json::Value* x = d->Find("improvised_extra_s")) {
            if (!r.Number(*x, dp + ".improvised_extra_s", out.reaction_delay.improvised_extra_s, 0.0, 60.0)) return false;
        }
        if (const json::Value* x = d->Find("jammed_s")) {
            if (!r.Pair(*x, dp + ".jammed_s", out.reaction_delay.jammed_lo_s, out.reaction_delay.jammed_hi_s, 0.0, 60.0)) return false;
        }
        out.reaction_delay.set = true;
    }
    if (const json::Value* pl = b.Find("pilots")) {
        const std::string pp = p + ".pilots";
        HotPilotsDef& d = out.pilots;
        if (!r.Keys(*pl, pp, {"hours_per_year", "start_sorties", "source", "confidence", "notes"}) || !r.Cite(*pl, pp, d.source, d.confidence)) return false;
        const json::Value* h = pl->Find("hours_per_year");
        if (h == nullptr) return r.Fail(pp + ".hours_per_year", "required");
        if (!r.Number(*h, pp + ".hours_per_year", d.hours_per_year, 1.0, 1000.0)) return false;
        if (const json::Value* s = pl->Find("start_sorties")) {
            if (!r.Number(*s, pp + ".start_sorties", d.start_sorties, 0.0, 100.0)) return false;
        }
        if (const json::Value* n = pl->Find("notes")) d.notes = n->Is(json::Type::String) ? n->string : std::string();
        d.set = true;
    }
    if (const json::Value* al = b.Find("alert")) {
        if (!al->Is(json::Type::Array)) return r.Fail(p + ".alert", "a list of pools");
        for (std::size_t i = 0; i < al->items.size(); ++i) {
            C2AlertDef a;
            if (!hot_detail::LoadAlert(r, al->items[i], p + ".alert[" + std::to_string(i) + "]", a)) return false;
            if (out.Alert(a.id) != nullptr) return r.Fail(p + ".alert[" + std::to_string(i) + "].id", "duplicate pool '" + a.id + "'");
            out.alert.push_back(a);
        }
    }
    if (const json::Value* ra = b.Find("retask_air")) {
        const std::string rp = p + ".retask_air";
        RetaskAirDef& d = out.retask_air;
        if (!r.Keys(*ra, rp, {"delay_s", "never", "source", "confidence", "notes"}) || !r.Cite(*ra, rp, d.source, d.confidence)) return false;
        if (const json::Value* x = ra->Find("delay_s")) {
            if (!r.Pair(*x, rp + ".delay_s", d.delay_s.lo, d.delay_s.hi, 0.0, 86400.0)) return false;
        }
        if (const json::Value* x = ra->Find("never")) {
            if (!r.Number(*x, rp + ".never", d.never, 0.0, 1.0)) return false;
        }
        if (const json::Value* n = ra->Find("notes")) d.notes = n->Is(json::Type::String) ? n->string : std::string();
        d.set = true;
    }
    if (const json::Value* au = b.Find("authority")) {
        const std::string ap = p + ".authority";
        HotAuthorityDef& a = out.authority;
        if (!r.Keys(*au, ap, {"beyond_task", "free_hunt_from_phase", "free_hunt_depth_km", "retask_by", "retask_kinds", "recall_only",
                              "leader_may", "source", "confidence", "notes"}) ||
            !r.Cite(*au, ap, a.source, a.confidence)) {
            return false;
        }
        if (const json::Value* x = au->Find("beyond_task")) {
            if (!r.Strings(*x, ap + ".beyond_task", a.beyond_task)) return false;
        }
        for (const std::string& m : a.beyond_task) {
            if (!hot_detail::Mission(m)) return r.Fail(ap + ".beyond_task", "'" + m + "' is not a mission");
        }
        if (const json::Value* x = au->Find("free_hunt_from_phase")) {
            double ph = -1.0;
            if (!r.Number(*x, ap + ".free_hunt_from_phase", ph, -1.0, 20.0) || ph != static_cast<double>(static_cast<int>(ph))) {
                return r.Fail(ap + ".free_hunt_from_phase", "a whole number, -1 .. 20");
            }
            a.free_hunt_from_phase = static_cast<int>(ph);
        }
        if (const json::Value* x = au->Find("free_hunt_depth_km")) {
            if (!r.Number(*x, ap + ".free_hunt_depth_km", a.free_hunt_depth_km, 0.0, 1000.0)) return false;
        }
        if (a.free_hunt_from_phase >= 0 && a.free_hunt_depth_km <= 0.0) return r.Fail(ap + ".free_hunt_depth_km", "required with a free hunt");
        if (const json::Value* x = au->Find("retask_by")) {
            if (!r.Strings(*x, ap + ".retask_by", a.retask_by)) return false;
        }
        for (const std::string& k : a.retask_by) {
            if (std::find(C2PostKinds().begin(), C2PostKinds().end(), k) == C2PostKinds().end()) {
                return r.Fail(ap + ".retask_by", "'" + k + "' is not a post kind");
            }
        }
        if (const json::Value* x = au->Find("retask_kinds")) {
            if (!r.Strings(*x, ap + ".retask_kinds", a.retask_kinds)) return false;
        }
        for (const std::string& m : a.retask_kinds) {
            if (!hot_detail::Mission(m)) return r.Fail(ap + ".retask_kinds", "'" + m + "' is not a mission");
        }
        if (const json::Value* x = au->Find("recall_only")) {
            if (!x->Is(json::Type::Bool)) return r.Fail(ap + ".recall_only", "true or false");
            a.recall_only = x->boolean;
        }
        if (const json::Value* x = au->Find("leader_may")) {
            if (!r.Strings(*x, ap + ".leader_may", a.leader_may)) return false;
        }
        for (const std::string& k : a.leader_may) {
            if (std::find(HotLeaderActs().begin(), HotLeaderActs().end(), k) == HotLeaderActs().end()) {
                return r.Fail(ap + ".leader_may", "'" + k + "' is not a leader's act");
            }
        }
        if (a.recall_only && !a.retask_by.empty()) return r.Fail(ap + ".recall_only", "a recall-only faction retasks nothing");
        if (const json::Value* n = au->Find("notes")) a.notes = n->Is(json::Type::String) ? n->string : std::string();
        a.set = true;
    }
    if (const json::Value* cl = b.Find("on_control_lost")) {
        const std::string cp = p + ".on_control_lost";
        HotControlLostDef& c = out.on_control_lost;
        if (!r.Keys(*cl, cp, {"green", "skilled", "source", "confidence", "notes"}) || !r.Cite(*cl, cp, c.source, c.confidence)) return false;
        for (const char* k : {"green", "skilled"}) {
            const json::Value* x = cl->Find(k);
            if (x == nullptr || !x->Is(json::Type::String)) return r.Fail(cp + "." + k, "required: abort, pairs or continue");
            if (x->string != "abort" && x->string != "pairs" && x->string != "continue") return r.Fail(cp + "." + k, "abort, pairs or continue");
            (std::string(k) == "green" ? c.green : c.skilled) = x->string;
        }
        c.set = true;
    }
    out.present = true;
    return true;
}

}  // namespace airwar
}  // namespace simcore
