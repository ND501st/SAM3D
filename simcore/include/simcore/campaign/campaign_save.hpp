// simcore/campaign/campaign_save.hpp — the game's campaign: started, saved at its seams and resumed (roadmap 15.7, first
// form; track O's O1, docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md sections 9-10).
//
// The campaign is the game's main mode: one enemy, chosen at the start, and battery 21's war against it as one long
// battery mission with the enemy's air war on its clock (mission_session.hpp). data/campaign/campaigns.json names, for
// each adversary, the mission the war opens with, and for all of them the theatre, the regiment's tasking rules and the
// war's length (a game value). The session takes a seam at the regiment's evening briefing (MissionSession::TakeSeam);
// the game writes it here as the campaign's save, and Continue comes back there.
//
//   {"campaign_save": 1, "kind": "briefing", "label": "Day 2, 19:00", "adversary": "rebels", "mission": "scenarios/...",
//    "tasking": "campaign/tasking.json", "fronts": true, "u2_radius_m": 20000, "war_hours": 336, "seat": "battery_commander",
//    "battery": {...}, "air_war": {...}}
//
// "air_war" is air_war/save.hpp's save as an object (written again as its text on a load), "battery" the battery between
// its tasks (battery_state.hpp), with the task it held written in the task files' own format (data/campaign/tasks/), so the
// task loader reads it back.
// O5: "standing", the regiment's marks for the battery and its trust in its reports (grading.hpp), and the battery's
// "crew_grades", the marks its commander wrote for his crews; the task's "limits" with it (the fire mode the standing set).
// Not in the save: the crews' records before it and the outcomes' ledgers (so the reveal covers the session since the
// last save); the director's count of wakes.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/air_war/save.hpp"
#include "simcore/campaign/battery_state.hpp"
#include "simcore/campaign/battery_task.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/command/own_air.hpp"
#include "simcore/console_data.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace campaign {

// ------------------------------------------------------------------ the campaigns (data/campaign/campaigns.json)

struct CampaignAdversary {
    std::string id;       // the air war's faction ("rebels", "neutral", "warsaw_pact", "nato")
    std::string label;    // as the menu lists it
    std::string line;     // its briefing line
    std::string mission;  // the mission the war opens with (relative to the data folder)
    bool playable = false;
    std::string not_yet;  // not playable: why, and when it comes
    // Track O, O3: the battery's Osa beside the post (relative to the data folder) and its callsign ("" none).
    std::string osa_mission;
    std::string osa_callsign;
};

struct CampaignsData {
    std::string theatre;   // the air war's theatre (relative to the data folder)
    std::string tasking;   // the regiment's tasking rules
    bool fronts = true;    // track F's ground war and our commanders on
    double days = 14.0;    // the war's length (a game value)
    double u2_radius_m = 20000.0;
    unsigned seed = 1;     // a new campaign's war
    std::string briefings; // O3: the orders as people give them ("" none)
    std::vector<CampaignAdversary> adversaries;
    const CampaignAdversary* Find(const std::string& id) const {
        for (const CampaignAdversary& a : adversaries) {
            if (a.id == id) return &a;
        }
        return nullptr;
    }
};

namespace save_json {

inline std::string Str(const json::Value& v, const char* key, const std::string& fallback = std::string()) {
    const json::Value* x = v.Find(key);
    return x != nullptr && x->Is(json::Type::String) ? x->string : fallback;
}
inline double Num(const json::Value& v, const char* key, double fallback = 0.0) {
    const json::Value* x = v.Find(key);
    return x != nullptr && x->Is(json::Type::Number) ? x->number : fallback;
}
inline bool Flag(const json::Value& v, const char* key, bool fallback = false) {
    const json::Value* x = v.Find(key);
    return x != nullptr && x->Is(json::Type::Bool) ? x->boolean : fallback;
}

}  // namespace save_json

inline bool LoadCampaignsData(const std::string& data_dir, const std::string& rel, CampaignsData& out, std::string* error = nullptr) {
    using namespace save_json;
    const auto fail = [&](const std::string& w) {
        if (error != nullptr) *error = rel + ": " + w;
        return false;
    };
    std::string text, e;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) return fail("cannot read it");
    json::Value root;
    if (!json::Parse(text, root, &e)) return fail(e);
    out = CampaignsData();
    out.theatre = Str(root, "theatre");
    out.tasking = Str(root, "tasking");
    out.fronts = Flag(root, "fronts", true);
    out.days = Num(root, "days", out.days);
    out.u2_radius_m = Num(root, "u2_radius_km", out.u2_radius_m / 1000.0) * 1000.0;
    out.seed = static_cast<unsigned>(Num(root, "seed", 1.0));
    out.briefings = Str(root, "briefings");
    if (out.theatre.empty() || out.tasking.empty()) return fail("needs \"theatre\" and \"tasking\"");
    if (!(out.days > 0.0)) return fail("days: > 0");
    const json::Value* list = root.Find("adversaries");
    if (list == nullptr || !list->Is(json::Type::Array) || list->items.empty()) return fail("needs \"adversaries\"");
    for (const json::Value& a : list->items) {
        CampaignAdversary c;
        c.id = Str(a, "id");
        c.label = Str(a, "label", c.id);
        c.line = Str(a, "line");
        c.mission = Str(a, "mission");
        c.playable = Flag(a, "playable");
        c.not_yet = Str(a, "not_yet");
        c.osa_mission = Str(a, "osa_mission");
        c.osa_callsign = Str(a, "osa_callsign");
        if (c.id.empty()) return fail("an adversary without an id");
        if (c.playable && c.mission.empty()) return fail(c.id + ": a playable adversary needs its \"mission\"");
        out.adversaries.push_back(c);
    }
    return true;
}

// ------------------------------------------------------------------ the task as its file's format

namespace save_json {

using namespace airwar::jsonb;

inline json::Value Strings(const std::vector<std::string>& v) {
    json::Value a = Arr();
    for (const std::string& s : v) Push(a, airwar::jsonb::Str(s));
    return a;
}

inline json::Value CooperationJson(const command::CooperationPlan& p) {
    json::Value o = Obj();
    Put(o, "method", airwar::jsonb::Str(p.method == command::CooperationMethod::ByZones ? "by_zones" : "one_zone"));
    Put(o, "split_height_m", airwar::jsonb::Num(p.split_height_m));
    Put(o, "fighter_floor_m", airwar::jsonb::Num(p.fighter_floor_m));
    Put(o, "floor_unable_m", airwar::jsonb::Num(p.floor_unable_m));
    Put(o, "units_unable", Strings(p.units_unable));
    json::Value zones = Arr();
    for (const command::OwnAirZone& z : p.zones) {
        json::Value x = Obj();
        Put(x, "id", airwar::jsonb::Str(z.id));
        Put(x, "kind", airwar::jsonb::Str(z.kind));
        json::Value path = Arr();
        for (const Vec3& pt : z.path) {
            json::Value q = Arr();
            Push(q, airwar::jsonb::Num(pt.x));
            Push(q, airwar::jsonb::Num(pt.y));
            Push(path, q);
        }
        Put(x, "path_m", path);
        Put(x, "half_width_m", airwar::jsonb::Num(z.half_width_m));
        Put(x, "floor_m", airwar::jsonb::Num(z.floor_m));
        Put(x, "ceiling_m", airwar::jsonb::Num(z.ceiling_m));
        Put(x, "from_h", airwar::jsonb::Num(z.from_h));
        Put(x, "to_h", airwar::jsonb::Num(z.to_h));
        Push(zones, x);
    }
    Put(o, "zones", zones);
    Put(o, "recognition_signal", airwar::jsonb::Str(p.recognition_signal));
    Put(o, "fire_prohibition_signal", airwar::jsonb::Str(p.fire_prohibition_signal));
    Put(o, "code_issue", airwar::jsonb::Str(p.code_issue));
    return o;
}

}  // namespace save_json

// The task in the format of data/campaign/tasks/ (LoadBatteryTaskJson reads it back to the same task).
inline json::Value TaskJsonValue(const BatteryTask& t) {
    using namespace airwar::jsonb;
    json::Value o = Obj();
    Put(o, "task_id", Str(t.id));
    Put(o, "kind", Str(TaskKindName(t.kind)));
    Put(o, "given_by", Str(t.given_by));
    Put(o, "unit", Str(t.unit));
    if (!t.covered.empty()) Put(o, "covered", Str(t.covered));
    Put(o, "task", Str(t.task_text));
    Put(o, "short_ru", Str(t.short_ru));
    if (!t.area_ru.empty()) Put(o, "area_ru", Str(t.area_ru));
    Put(o, "reasons", save_json::Strings(t.reasons));
    json::Value sit = Arr();
    for (const TaskLine& l : t.situation) {
        json::Value x = Obj();
        Put(x, "kind", Str(l.kind));
        Put(x, "from_deg", Num(l.from_deg));
        Put(x, "to_deg", Num(l.to_deg));
        Put(x, "range_km", Num(l.range_km));
        Put(x, "text", Str(l.text));
        Push(sit, x);
    }
    Put(o, "situation", sit);
    if (!t.habits.empty()) Put(o, "habits", Str(t.habits));
    json::Value area = Obj();
    if (!t.main.empty()) Put(area, "main", Str(t.main));
    Put(area, "reserve", save_json::Strings(t.reserve));
    Put(area, "dummies", save_json::Strings(t.dummies));
    if (!t.tzm_place.empty()) Put(area, "tzm", Str(t.tzm_place));
    Put(o, "area", area);
    json::Value secs = Arr();
    for (const TaskSector& s : t.sectors) {
        json::Value x = Obj();
        Put(x, "vehicle", Str(s.vehicle));
        Put(x, "from_deg", Num(s.from_deg));
        Put(x, "to_deg", Num(s.to_deg));
        Put(x, "duty", Bool(s.duty));
        Push(secs, x);
    }
    Put(o, "sectors", secs);
    Put(o, "readiness", Num(t.readiness));
    if (!t.readiness_text.empty()) Put(o, "readiness_text", Str(t.readiness_text));
    if (!t.emission.empty()) Put(o, "emission", Str(t.emission));
    if (t.on_turns > 0.0) Put(o, "on_turns", Num(t.on_turns));
    if (t.off_turns > 0.0) Put(o, "off_turns", Num(t.off_turns));
    Put(o, "reconnoitre", Bool(t.reconnoitre));
    Put(o, "roving", Bool(t.roving));
    Put(o, "axis_deg", Num(t.axis_deg));
    Put(o, "width_m", Num(t.width_m));
    Put(o, "ready_in_s", Num(t.ready_in_s));
    Put(o, "lasts_s", Num(t.lasts_s));
    Put(o, "coordination", save_json::Strings(t.coordination));
    Put(o, "reports", save_json::Strings(t.reports));
    Put(o, "support", save_json::Strings(t.support));
    if (t.own_air.present) Put(o, "own_air", save_json::CooperationJson(t.own_air));
    if (t.limits.Any()) {  // O4's limits (O5: the regiment's fire mode by the battery's standing travels in them)
        json::Value l = Obj();
        if (t.limits.spot_m >= 0.0) Put(l, "spot_m", Num(t.limits.spot_m));
        if (t.limits.launcher_spot_m >= 0.0) Put(l, "launcher_spot_m", Num(t.limits.launcher_spot_m));
        if (!t.limits.cover_unit.empty()) Put(l, "cover_unit", Str(t.limits.cover_unit));
        if (t.limits.leash_m >= 0.0) Put(l, "leash_m", Num(t.limits.leash_m));
        if (!t.limits.fire.empty()) Put(l, "fire", Str(t.limits.fire));
        Put(o, "limits", l);
    }
    Put(o, "source", Str("SAM3D: the task as the battery held it when the campaign was saved"));
    Put(o, "confidence", Str("estimate"));
    return o;
}

// ------------------------------------------------------------------ the save

struct CampaignSave {
    int version = 1;
    std::string kind;       // "briefing"
    std::string label;      // "Day 2, 19:00"
    std::string adversary;
    std::string mission;    // the campaign's mission (relative to the data folder)
    std::string tasking;
    bool fronts = true;
    double u2_radius_m = 20000.0;
    double war_hours = 0.0; // the war's length, campaign hours from its start
    std::string seat;       // the player's
    BatteryState battery;
    std::string air_war;    // air_war/save.hpp's text
    std::string briefings;     // O3 ("" none)
    std::string osa_mission;   // O3: the Osa beside the post ("" none)
    std::string osa_callsign;
    RegimentStanding standing; // O5: the regiment's marks for the battery and its trust in its reports
};

namespace save_json {

inline json::Value StandingJson(const RegimentStanding& s) {
    namespace jb = airwar::jsonb;
    json::Value o = jb::Obj();
    jb::Put(o, "unit", jb::Str(s.unit));
    jb::Put(o, "checked", jb::Num(s.checked));
    jb::Put(o, "agreed", jb::Num(s.agreed));
    jb::Put(o, "checks", jb::Num(s.checks));
    jb::Put(o, "summed_h", jb::Num(s.summed_h));
    json::Value days = jb::Arr();
    for (const DayMark& d : s.days) {
        json::Value x = jb::Obj();
        jb::Put(x, "campaign_h", jb::Num(d.campaign_h));
        jb::Put(x, "grade", jb::Num(d.grade));
        json::Value obj = jb::Arr();
        for (const auto& g : d.objectives) {
            json::Value y = jb::Obj();
            jb::Put(y, "id", jb::Str(g.first));
            jb::Put(y, "grade", jb::Num(g.second));
            jb::Push(obj, y);
        }
        jb::Put(x, "objectives", obj);
        jb::Put(x, "lines", Strings(d.lines));
        jb::Push(days, x);
    }
    jb::Put(o, "days", days);
    return o;
}
inline void ReadStanding(const json::Value& o, RegimentStanding& s) {
    s.unit = Str(o, "unit", s.unit);
    s.checked = static_cast<int>(Num(o, "checked"));
    s.agreed = static_cast<int>(Num(o, "agreed"));
    s.checks = static_cast<int>(Num(o, "checks"));
    s.summed_h = Num(o, "summed_h", -1.0);
    if (const json::Value* days = o.Find("days")) {
        for (const json::Value& x : days->items) {
            DayMark d;
            d.campaign_h = Num(x, "campaign_h", -1.0);
            d.grade = static_cast<int>(Num(x, "grade"));
            if (const json::Value* obj = x.Find("objectives")) {
                for (const json::Value& y : obj->items) d.objectives.push_back({Str(y, "id"), static_cast<int>(Num(y, "grade"))});
            }
            if (const json::Value* lines = x.Find("lines")) {
                for (const json::Value& l : lines->items) d.lines.push_back(l.string);
            }
            s.days.push_back(d);
        }
    }
}
inline json::Value CrewGradesJson(const std::vector<CrewGrade>& g) {
    namespace jb = airwar::jsonb;
    json::Value a = jb::Arr();
    for (const CrewGrade& c : g) {
        json::Value x = jb::Obj();
        jb::Put(x, "campaign_h", jb::Num(c.campaign_h));
        jb::Put(x, "crew", jb::Str(c.crew));
        jb::Put(x, "grade", jb::Num(c.grade));
        jb::Put(x, "by_player", jb::Bool(c.by_player));
        jb::Put(x, "reasons", Strings(c.reasons));
        jb::Push(a, x);
    }
    return a;
}
inline void ReadCrewGrades(const json::Value& a, std::vector<CrewGrade>& out) {
    for (const json::Value& x : a.items) {
        CrewGrade c;
        c.campaign_h = Num(x, "campaign_h", -1.0);
        c.crew = Str(x, "crew");
        c.grade = static_cast<int>(Num(x, "grade", 5.0));
        c.by_player = Flag(x, "by_player");
        if (const json::Value* r = x.Find("reasons")) {
            for (const json::Value& l : r->items) c.reasons.push_back(l.string);
        }
        out.push_back(c);
    }
}

}  // namespace save_json

inline std::string CampaignSaveJson(const CampaignSave& s) {
    using namespace airwar::jsonb;
    json::Value root = Obj();
    Put(root, "campaign_save", Num(s.version));
    Put(root, "kind", Str(s.kind));
    Put(root, "label", Str(s.label));
    Put(root, "adversary", Str(s.adversary));
    Put(root, "mission", Str(s.mission));
    Put(root, "tasking", Str(s.tasking));
    Put(root, "fronts", Bool(s.fronts));
    Put(root, "u2_radius_m", Num(s.u2_radius_m));
    Put(root, "war_hours", Num(s.war_hours));
    Put(root, "seat", Str(s.seat));
    if (!s.briefings.empty()) Put(root, "briefings", Str(s.briefings));
    if (!s.osa_mission.empty()) Put(root, "osa_mission", Str(s.osa_mission));
    if (!s.osa_callsign.empty()) Put(root, "osa_callsign", Str(s.osa_callsign));
    const BatteryState& b = s.battery;
    json::Value bj = Obj();
    Put(bj, "campaign_h", Num(b.campaign_h));
    Put(bj, "local_h", Num(b.local_h));
    Put(bj, "day_of_year", Num(b.day_of_year));
    json::Value own = Arr();
    for (double c : {b.own.x, b.own.y, b.own.z}) Push(own, Num(c));
    Put(bj, "own_enu_m", own);
    Put(bj, "deployed", Bool(b.deployed));
    json::Value vs = Arr();
    for (const SavedVehicle& v : b.vehicles) {
        json::Value x = Obj();
        Put(x, "id", Str(v.id));
        json::Value at = Arr();
        for (double c : {v.position.x, v.position.y, v.position.z}) Push(at, Num(c));
        Put(x, "enu_m", at);
        Put(x, "missiles", Num(v.missiles));
        Put(x, "stock", Num(v.stock));
        Push(vs, x);
    }
    Put(bj, "vehicles", vs);
    Put(bj, "siding_tested", Num(b.siding_tested));
    Put(bj, "siding_untested", Num(b.siding_untested));
    if (b.has_task) Put(bj, "task", TaskJsonValue(b.task));
    Put(bj, "task_ready", Bool(b.task_ready));
    Put(bj, "at", Str(b.at));
    Put(bj, "arrived_h", Num(b.arrived_h));
    if (!b.crew_grades.empty()) Put(bj, "crew_grades", save_json::CrewGradesJson(b.crew_grades));
    Put(root, "battery", bj);
    Put(root, "standing", save_json::StandingJson(s.standing));
    json::Value war;
    std::string e;
    if (!s.air_war.empty() && json::Parse(s.air_war, war, &e)) Put(root, "air_war", war);
    return Dump(root, 1, 17);
}

inline bool LoadCampaignSaveJson(const std::string& text, CampaignSave& out, std::string* error = nullptr) {
    using namespace save_json;
    const auto fail = [error](const std::string& w) {
        if (error != nullptr) *error = "the campaign's save: " + w;
        return false;
    };
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) return fail(e);
    out = CampaignSave();
    out.version = static_cast<int>(Num(root, "campaign_save", 0.0));
    if (out.version != 1) return fail("campaign_save 1 expected");
    out.kind = Str(root, "kind");
    out.label = Str(root, "label");
    out.adversary = Str(root, "adversary");
    out.mission = Str(root, "mission");
    out.tasking = Str(root, "tasking");
    out.fronts = Flag(root, "fronts", true);
    out.u2_radius_m = Num(root, "u2_radius_m", out.u2_radius_m);
    out.war_hours = Num(root, "war_hours");
    out.seat = Str(root, "seat");
    out.briefings = Str(root, "briefings");
    out.osa_mission = Str(root, "osa_mission");
    out.osa_callsign = Str(root, "osa_callsign");
    if (out.adversary.empty() || out.mission.empty()) return fail("no adversary or mission");
    const json::Value* bj = root.Find("battery");
    if (bj == nullptr) return fail("no battery");
    BatteryState& b = out.battery;
    b.present = true;
    b.campaign_h = Num(*bj, "campaign_h");
    b.local_h = Num(*bj, "local_h");
    b.day_of_year = static_cast<int>(Num(*bj, "day_of_year", 288.0));
    const auto vec = [](const json::Value* a, Vec3& v) {
        if (a == nullptr || a->items.size() < 3) return false;
        v = Vec3{a->items[0].number, a->items[1].number, a->items[2].number};
        return true;
    };
    if (!vec(bj->Find("own_enu_m"), b.own)) return fail("battery.own_enu_m");
    b.deployed = Flag(*bj, "deployed", true);
    if (const json::Value* vs = bj->Find("vehicles")) {
        for (const json::Value& x : vs->items) {
            SavedVehicle v;
            v.id = Str(x, "id");
            if (!vec(x.Find("enu_m"), v.position)) return fail("battery.vehicles: " + v.id + " has no place");
            v.missiles = static_cast<int>(Num(x, "missiles", -1.0));
            v.stock = Num(x, "stock", -1.0);
            b.vehicles.push_back(v);
        }
    }
    b.siding_tested = static_cast<int>(Num(*bj, "siding_tested", -1.0));
    b.siding_untested = static_cast<int>(Num(*bj, "siding_untested", -1.0));
    if (const json::Value* t = bj->Find("task")) {
        std::ostringstream s;
        airwar::jsonb::Write(s, *t, 0, 0, 17);
        if (!LoadBatteryTaskJson(s.str(), b.task, &e)) return fail(e);
        b.has_task = true;
    }
    b.task_ready = Flag(*bj, "task_ready");
    b.at = Str(*bj, "at");
    b.arrived_h = Num(*bj, "arrived_h", -1.0);
    if (const json::Value* g = bj->Find("crew_grades")) save_json::ReadCrewGrades(*g, b.crew_grades);
    if (const json::Value* st = root.Find("standing")) save_json::ReadStanding(*st, out.standing);
    const json::Value* war = root.Find("air_war");
    if (war == nullptr) return fail("no air war");
    out.air_war = airwar::jsonb::Dump(*war, 1, 17);
    return true;
}

// ------------------------------------------------------------------ starting and resuming

// A new campaign against `adversary`: the session's definition (the mission's file under data_dir) and the save's header.
inline bool NewCampaign(const CampaignsData& c, const std::string& adversary, const std::string& data_dir, const std::string& seat,
                        MissionSessionDef& def, CampaignSave& header, std::string* error = nullptr) {
    const CampaignAdversary* a = c.Find(adversary);
    if (a == nullptr || !a->playable) {
        if (error != nullptr) *error = a == nullptr ? "no campaign against '" + adversary + "'" : a->label + ": " + a->not_yet;
        return false;
    }
    def = MissionSessionDef();
    def.scenario = data_dir + "/" + a->mission;
    def.faction = a->id;
    def.theatre = c.theatre;
    def.seed = c.seed;
    def.tasking = c.tasking;
    def.fronts = c.fronts;
    def.u2_radius_m = c.u2_radius_m;
    def.hours = c.days * 24.0;  // from the mission's start, the war's first evening
    def.seams = true;
    def.briefings = c.briefings;
    if (!a->osa_mission.empty()) {
        def.osa_scenario = data_dir + "/" + a->osa_mission;
        def.osa_callsign = a->osa_callsign;
    }
    header = CampaignSave();
    header.adversary = a->id;
    header.mission = a->mission;
    header.tasking = c.tasking;
    header.fronts = c.fronts;
    header.u2_radius_m = c.u2_radius_m;
    header.seat = seat;
    header.briefings = c.briefings;
    header.osa_mission = a->osa_mission;
    header.osa_callsign = a->osa_callsign;
    return true;
}

// The session's definition that resumes a save (`save` must outlive the session's start: its battery is pointed at).
inline MissionSessionDef ResumeCampaign(const CampaignSave& save, const std::string& data_dir) {
    MissionSessionDef def;
    def.scenario = data_dir + "/" + save.mission;
    def.faction = save.adversary;
    def.tasking = save.tasking;
    def.fronts = save.fronts;
    def.u2_radius_m = save.u2_radius_m;
    def.air_war_save = save.air_war;
    def.battery = &save.battery;
    def.hours = std::max(1.0, save.war_hours - save.battery.campaign_h);
    def.seams = true;
    def.briefing_due = save.kind == "briefing";
    def.briefings = save.briefings;
    def.standing = &save.standing;  // O5
    if (!save.osa_mission.empty()) {
        def.osa_scenario = data_dir + "/" + save.osa_mission;
        def.osa_callsign = save.osa_callsign;
    }
    return def;
}

// The save of a seam: the header the campaign started with, the seam's war and battery, its label.
inline CampaignSave SaveOfSeam(const CampaignSave& header, const MissionSession& s, const MissionSession::Seam& seam) {
    CampaignSave out = header;
    out.kind = seam.kind;
    out.air_war = seam.air_war;
    out.battery = seam.battery;
    out.standing = seam.standing;
    if (out.war_hours <= 0.0) out.war_hours = s.CampaignHourAtStart() + s.Runner().Def().time_limit_s / 3600.0;
    const double start_hour = s.Campaign() != nullptr ? s.Campaign()->Data().theatre.start_hour : 0.0;
    const int day = static_cast<int>(std::floor((seam.battery.campaign_h + start_hour) / 24.0)) + 1;
    const long long m = std::llround(seam.battery.local_h * 60.0) % (24 * 60);
    char b[64];
    std::snprintf(b, sizeof(b), "Day %d, %02lld:%02lld", day, m / 60, m % 60);
    out.label = b;
    return out;
}

}  // namespace campaign
}  // namespace simcore
