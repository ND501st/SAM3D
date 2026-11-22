// simcore/command/own_air.hpp — our fighters as the battery meets them (roadmap track B, B10; docs/air_battle/08_OUR_FIGHTERS.md):
// the fighter regiments' duty by a rule table, the sky shared with the SAM troops, the notices of our flights, the
// identification codes and their schedule, the fire rule a crew keeps on what it was told, and the trace of a launch at
// one of ours.
//
//  - The data: data/command/own_air.json (the rule table, our ground control, the notice's coding work, the change of the
//    codes, the cooperation's defaults, the enemy's learning) and data/command/iff_codes.json (Kremniy-2M's filters and
//    their period, soviet_iff).
//  - The codes (section 2.7, 6): an IffSchedule is drawn for each day from a keyed stream (Stream::AirCombat, keys from
//    kOwnAirKey: B8's live fight keys from 0xF1600000, so none is shared); a holder (a crew, a post) keeps the filter of the
//    day's paper only once the paper reached it, else the last one it fitted; a pilot misses a change by a keyed chance and
//    answers on the old filter until the next. A reply needs the same system and the same filter: no side test.
//  - The sky (section 5): a CooperationPlan by zones or one zone shared by height, its corridors and duty zones with their
//    heights and clock hours, the fighters' floor over a unit and its drop to 3,000 m over units the records say cannot fire.
//  - The notice (section 6): Franchuk's fields, said as group 402 with PVO grid squares, heights and clock times
//    (NoticeWords), and read back from what was written (ReadNotice): a slip in the journal is carried into the board.
//  - The fire rule (section 5): JudgeTrack on what the crew holds: a track answering friend and off every notice is a
//    violator, one with no reply inside a notified corridor at its height and hour is held, the rest is free.
//  - The trace (section 8): TraceFriendlyFire says from the records why a launch at one of ours happened.
//  - The duty (section 3): FighterDuty keeps each regiment's pairs at readiness 1, 2 and 3 by the rule table and scrambles
//    them against the groups on our sector's picture (never the truth), each take-off a keyed draw in its state's window.
//
// Std only; the json loader is simcore's own.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/hot_phase_def.hpp"
#include "simcore/em/random.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/pvo_grid.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace command {

// The keys of B10's draws on Stream::AirCombat (disjoint from B8's 0xF16xxxxx).
constexpr std::uint64_t kOwnAirKey = 0xB10A0000ull;

// ------------------------------------------------------------------------------------------------ the data

struct ReadinessDef {
    int state = 1;
    double takeoff_min_s = 180.0;
    double takeoff_max_s = 300.0;
    double held_min_h = 1.0;
    double held_max_h = 2.0;
};

struct DutyDef {
    int day_pairs_r1 = 1;
    int day_pairs_r2 = 2;
    int night_pairs_r1 = 1;
    int night_pairs_r2 = 1;
    int warning_extra_r1 = 1;
    int pairs_per_group = 1;
    double group_m = 10000.0;
    double scramble_reach_m = 90000.0;
    double night_from_h = 20.0;
    double night_to_h = 6.0;
};

struct GciDef {
    int channels = 3;
    bool automated = true;
    double age_auto_s = 10.0;
    double age_manual_min_s = 180.0;
    double age_manual_max_s = 360.0;
    double update_s = 10.0;
    double error_m = 1500.0;
    double radar_range_m = 220000.0;
    double mast_m = 25.0;
};

struct NoticeDef {
    double hop_work_s = 30.0;       // coding and decoding by the talk table at each hop (a game value: own_air.json)
    int precedence = 1;
    bool liaison_at_regiment = false;
    double liaison_s = 30.0;
    double corridor_half_width_m = 5000.0;
    double slack_s = 300.0;
};

struct IffChangeDef {
    double job_s = 40.0;      // the senior operator fits the new filter
    double pilot_miss = 0.03; // a pilot misses a change
    double handover_s = 600.0;
};

struct CooperationDefaults {
    std::string method = "by_zones";
    double floor_unable_m = 3000.0;
    double floor_margin_m = 1000.0;
    double split_height_m = 5000.0;
    double near_target_m = 600.0;
};

struct EnemyLearningDef {
    double heard_per_use = 0.25;     // the enemy's COMINT hears a day's use of a corridor
    double break_talk_table = 0.15;  // and reads the notice's coded groups
    int learn_after = 3;             // heard this often: a believed corridor
    double sigma_m = 8000.0;
    double use_creativity = 0.55;    // a planner this creative times a raid to its window
    double use_age_h = 72.0;         // on a belief no older than this
    // S1: the campaign's use of a belief (air_war/campaign.hpp, resolve.hpp).
    double shift_h = 2.0;            // the planner moves a raid's time over target at most this far into a believed window
    double hold_fire = 0.5;          // a defence near a corridor in its window loses this share of its fire (it holds for ours)
    double near_m = 15000.0;         // a defence this near a corridor's path holds for it
    double margin_m = 10000.0;       // a belief this far off the corridor (beyond its half width) still finds it
    double hours_per_day = 6.0;      // a corridor's hours of use a day in the campaign (from 6 h; our flights by day)
};

// The fighter regiments' reports of their losses and claims (S1, section 8): the pair's leader calls a loss or a kill he saw
// to the GCI at once, the regiment's duty officer has it a little later and sends it down the chain like a notice; a loss
// nobody saw is reported missing once the aircraft's fuel would be out. Game values (own_air.json "reports").
struct ReportsDef {
    double seen_s = 120.0;
    double missing_min_s = 1800.0;
    double missing_max_s = 3600.0;
    double claim_s = 300.0;
    double hop_work_s = 30.0;
    int precedence = 1;
    double match_s = 600.0;          // the regiment's check: a loss this near a launch in time
    double match_m = 10000.0;        // and in place meets it
};

// The allied false friend (S1, section 7): in a war with the Pact, its allies' aircraft fly the same Kremniy over the same
// war filters on their own schedule; one answers our interrogator when both hold the same filter. An inference, flagged.
struct FalseFriendDef {
    bool on = true;
    double allied_share = 0.5;       // the Pact's aircraft that are its allies' (on Kremniy), not Soviet (on Parol)
};

struct OwnAirDef {
    std::vector<ReadinessDef> readiness;
    DutyDef duty;
    double alert_low_min_m = 1500.0, alert_low_max_m = 2000.0;
    double alert_high_min_m = 10000.0, alert_high_max_m = 12000.0;
    double alert_inside_min_km = 50.0, alert_inside_max_km = 60.0;
    double pursuit_min_km = 25.0, pursuit_max_km = 30.0;
    int alert_pairs = 1;             // pairs a regiment puts on airborne alert on a raid warning (S1)
    double alert_orbit_s = 2400.0;   // how long a pair holds its zone before it goes home unvectored
    ReportsDef reports;
    FalseFriendDef false_friend;
    GciDef gci;
    NoticeDef notice;
    IffChangeDef iff_change;
    CooperationDefaults coop;
    EnemyLearningDef learning;
    // How our fighters choose in the air (roadmap B11, docs/air_battle/10_FACTIONS.md section 7.2, "Our side"): the same block
    // as each enemy faction's, read by the same functions (air_war/hot_phase.hpp). No alert pools (the readiness above).
    airwar::HotPhaseDef hot;

    const ReadinessDef* Readiness(int state) const {
        for (const ReadinessDef& r : readiness) {
            if (r.state == state) return &r;
        }
        return nullptr;
    }
};

enum class IffSystem { None = 0, Kremniy = 1, Parol = 2, MarkXII = 3 };

inline const char* IffSystemName(IffSystem s) {
    switch (s) {
        case IffSystem::None: return "none";
        case IffSystem::Kremniy: return "kremniy";
        case IffSystem::Parol: return "parol";
        case IffSystem::MarkXII: return "mark_xii";
    }
    return "?";
}
inline bool ParseIffSystem(const std::string& s, IffSystem& out) {
    for (IffSystem k : {IffSystem::None, IffSystem::Kremniy, IffSystem::Parol, IffSystem::MarkXII}) {
        if (s == IffSystemName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

struct IffCodesDef {
    IffSystem system = IffSystem::Kremniy;
    int filters = 12;
    int peace_first = 1, peace_last = 8;
    int war_first = 9, war_last = 12;
    int alignment = 4;
    double period_min_h = 1.0;
    double period_max_h = 6.0;
    std::vector<std::string> document_means;
    std::string issued_by = "rgt_signals_chief";
    double issued_h = 18.0;
};

namespace own_air_detail {
inline bool Unknown(const json::Value& v, const std::string& path, std::initializer_list<const char*> allowed, std::string* error) {
    for (const std::string& k : v.keys) {
        bool ok = false;
        for (const char* a : allowed) ok = ok || k == a;
        if (!ok) {
            if (error != nullptr) *error = path + ": unknown key '" + k + "'";
            return true;
        }
    }
    return false;
}
inline double Num(const json::Value& v, const char* key, double def) {
    const json::Value* f = v.Find(key);
    return f != nullptr && f->Is(json::Type::Number) ? f->number : def;
}
inline bool Pair(const json::Value& v, const char* key, double& a, double& b) {
    const json::Value* f = v.Find(key);
    if (f == nullptr) return true;
    if (f->Is(json::Type::Object)) f = f->Find("value");
    if (f == nullptr || !f->Is(json::Type::Array) || f->items.size() != 2 || !f->items[0].Is(json::Type::Number) || !f->items[1].Is(json::Type::Number)) {
        return false;
    }
    a = f->items[0].number;
    b = f->items[1].number;
    return true;
}
inline std::string Str(const json::Value& v, const char* key, const std::string& def = std::string()) {
    const json::Value* f = v.Find(key);
    return f != nullptr && f->Is(json::Type::String) ? f->string : def;
}
inline bool ReadFile(const std::string& path, json::Value& root, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string e;
    if (!json::Parse(ss.str(), root, &e)) {
        if (error != nullptr) *error = path + ": " + e;
        return false;
    }
    return true;
}
}  // namespace own_air_detail

inline bool LoadOwnAirFile(const std::string& path, OwnAirDef& out, std::string* error = nullptr) {
    using namespace own_air_detail;
    const auto fail = [&](const std::string& w) {
        if (error != nullptr) *error = path + ": " + w;
        return false;
    };
    json::Value root;
    if (!ReadFile(path, root, error)) return false;
    std::string e;
    if (Unknown(root, "", {"own_air_id", "status", "source", "confidence", "notes", "readiness", "readiness_source", "duty", "airborne_alert",
                           "pursuit_limit_km", "gci", "notice", "iff_change", "cooperation", "enemy_learning", "hot_phase", "reports",
                           "false_friend"}, &e)) {
        return fail(e);
    }
    out = OwnAirDef();
    {
        const json::Value* hp = root.Find("hot_phase");
        if (hp == nullptr) return fail("hot_phase: required (roadmap B11)");
        std::string he;
        if (!airwar::LoadHotPhase(*hp, "hot_phase", out.hot, &he)) return fail(he);
        if (!out.hot.alert.empty()) return fail("hot_phase.alert: our fighters' readiness is the readiness block's");
    }
    const json::Value* rs = root.Find("readiness");
    if (rs == nullptr || !rs->Is(json::Type::Array) || rs->items.empty()) return fail("readiness: expected the states");
    for (const json::Value& r : rs->items) {
        if (Unknown(r, "readiness", {"state", "takeoff_s", "held_h", "notes"}, &e)) return fail(e);
        ReadinessDef d;
        d.state = static_cast<int>(Num(r, "state", 0.0));
        if (!Pair(r, "takeoff_s", d.takeoff_min_s, d.takeoff_max_s) || !Pair(r, "held_h", d.held_min_h, d.held_max_h)) {
            return fail("readiness: takeoff_s and held_h are [min, max]");
        }
        if (d.state < 1 || d.state > 3 || d.takeoff_min_s <= 0.0 || d.takeoff_max_s < d.takeoff_min_s) return fail("readiness: a state 1-3 with its take-off window");
        out.readiness.push_back(d);
    }
    if (const json::Value* d = root.Find("duty")) {
        if (Unknown(*d, "duty", {"day_pairs_r1", "day_pairs_r2", "night_pairs_r1", "night_pairs_r2", "warning_extra_r1", "pairs_per_group", "group_m",
                                 "scramble_reach_m", "night_from_h", "night_to_h", "source", "confidence", "notes"}, &e)) {
            return fail(e);
        }
        DutyDef& u = out.duty;
        u.day_pairs_r1 = static_cast<int>(Num(*d, "day_pairs_r1", u.day_pairs_r1));
        u.day_pairs_r2 = static_cast<int>(Num(*d, "day_pairs_r2", u.day_pairs_r2));
        u.night_pairs_r1 = static_cast<int>(Num(*d, "night_pairs_r1", u.night_pairs_r1));
        u.night_pairs_r2 = static_cast<int>(Num(*d, "night_pairs_r2", u.night_pairs_r2));
        u.warning_extra_r1 = static_cast<int>(Num(*d, "warning_extra_r1", u.warning_extra_r1));
        u.pairs_per_group = static_cast<int>(Num(*d, "pairs_per_group", u.pairs_per_group));
        u.group_m = Num(*d, "group_m", u.group_m);
        u.scramble_reach_m = Num(*d, "scramble_reach_m", u.scramble_reach_m);
        u.night_from_h = Num(*d, "night_from_h", u.night_from_h);
        u.night_to_h = Num(*d, "night_to_h", u.night_to_h);
    }
    if (const json::Value* a = root.Find("airborne_alert")) {
        if (Unknown(*a, "airborne_alert", {"low_m", "high_m", "centre_inside_km", "pairs", "orbit_s", "source", "confidence", "notes", "game_values"}, &e)) {
            return fail(e);
        }
        if (!Pair(*a, "low_m", out.alert_low_min_m, out.alert_low_max_m) || !Pair(*a, "high_m", out.alert_high_min_m, out.alert_high_max_m) ||
            !Pair(*a, "centre_inside_km", out.alert_inside_min_km, out.alert_inside_max_km)) {
            return fail("airborne_alert: [min, max] pairs");
        }
        out.alert_pairs = static_cast<int>(Num(*a, "pairs", out.alert_pairs));
        out.alert_orbit_s = Num(*a, "orbit_s", out.alert_orbit_s);
        if (out.alert_pairs < 0 || out.alert_orbit_s <= 0.0) return fail("airborne_alert: pairs >= 0 and orbit_s > 0");
    }
    if (!Pair(root, "pursuit_limit_km", out.pursuit_min_km, out.pursuit_max_km)) return fail("pursuit_limit_km: {\"value\": [min, max]}");
    if (const json::Value* g = root.Find("gci")) {
        if (Unknown(*g, "gci", {"channels", "automated", "age_auto_s", "age_manual_s", "update_s", "error_m", "radar_range_m", "mast_m", "source",
                                "confidence", "notes"}, &e)) {
            return fail(e);
        }
        GciDef& c = out.gci;
        c.channels = static_cast<int>(Num(*g, "channels", c.channels));
        if (const json::Value* au = g->Find("automated")) c.automated = au->Is(json::Type::Bool) ? au->boolean : c.automated;
        c.age_auto_s = Num(*g, "age_auto_s", c.age_auto_s);
        if (!Pair(*g, "age_manual_s", c.age_manual_min_s, c.age_manual_max_s)) return fail("gci.age_manual_s: [min, max]");
        c.update_s = Num(*g, "update_s", c.update_s);
        c.error_m = Num(*g, "error_m", c.error_m);
        c.radar_range_m = Num(*g, "radar_range_m", c.radar_range_m);
        c.mast_m = Num(*g, "mast_m", c.mast_m);
        if (c.channels < 1 || c.update_s <= 0.0) return fail("gci: at least one channel and an update period");
    }
    if (const json::Value* n = root.Find("notice")) {
        if (Unknown(*n, "notice", {"hop_work_s", "precedence", "liaison_at_regiment", "liaison_s", "corridor_half_width_m", "slack_s", "source",
                                   "confidence", "notes"}, &e)) {
            return fail(e);
        }
        NoticeDef& c = out.notice;
        c.hop_work_s = Num(*n, "hop_work_s", c.hop_work_s);
        c.precedence = static_cast<int>(Num(*n, "precedence", c.precedence));
        if (const json::Value* l = n->Find("liaison_at_regiment")) c.liaison_at_regiment = l->Is(json::Type::Bool) && l->boolean;
        c.liaison_s = Num(*n, "liaison_s", c.liaison_s);
        c.corridor_half_width_m = Num(*n, "corridor_half_width_m", c.corridor_half_width_m);
        c.slack_s = Num(*n, "slack_s", c.slack_s);
    }
    if (const json::Value* c = root.Find("iff_change")) {
        if (Unknown(*c, "iff_change", {"job_s", "pilot_miss", "handover_s", "source", "confidence", "notes"}, &e)) return fail(e);
        out.iff_change.job_s = Num(*c, "job_s", out.iff_change.job_s);
        out.iff_change.pilot_miss = Num(*c, "pilot_miss", out.iff_change.pilot_miss);
        out.iff_change.handover_s = Num(*c, "handover_s", out.iff_change.handover_s);
    }
    if (const json::Value* c = root.Find("cooperation")) {
        if (Unknown(*c, "cooperation", {"method", "floor_unable_m", "floor_margin_m", "split_height_m", "near_target_m", "source", "confidence", "notes"}, &e)) {
            return fail(e);
        }
        CooperationDefaults& d = out.coop;
        d.method = Str(*c, "method", d.method);
        if (d.method != "by_zones" && d.method != "one_zone") return fail("cooperation.method: by_zones or one_zone");
        d.floor_unable_m = Num(*c, "floor_unable_m", d.floor_unable_m);
        d.floor_margin_m = Num(*c, "floor_margin_m", d.floor_margin_m);
        d.split_height_m = Num(*c, "split_height_m", d.split_height_m);
        d.near_target_m = Num(*c, "near_target_m", d.near_target_m);
    }
    if (const json::Value* l = root.Find("enemy_learning")) {
        if (Unknown(*l, "enemy_learning", {"heard_per_use", "break_talk_table", "learn_after", "sigma_m", "use_creativity", "use_age_h", "shift_h",
                                           "hold_fire", "near_m", "margin_m", "hours_per_day", "source", "confidence", "notes"}, &e)) {
            return fail(e);
        }
        EnemyLearningDef& d = out.learning;
        d.heard_per_use = Num(*l, "heard_per_use", d.heard_per_use);
        d.break_talk_table = Num(*l, "break_talk_table", d.break_talk_table);
        d.learn_after = static_cast<int>(Num(*l, "learn_after", d.learn_after));
        d.sigma_m = Num(*l, "sigma_m", d.sigma_m);
        d.use_creativity = Num(*l, "use_creativity", d.use_creativity);
        d.use_age_h = Num(*l, "use_age_h", d.use_age_h);
        d.shift_h = Num(*l, "shift_h", d.shift_h);
        d.hold_fire = Num(*l, "hold_fire", d.hold_fire);
        d.near_m = Num(*l, "near_m", d.near_m);
        d.margin_m = Num(*l, "margin_m", d.margin_m);
        d.hours_per_day = Num(*l, "hours_per_day", d.hours_per_day);
        if (d.hold_fire < 0.0 || d.hold_fire > 1.0 || d.shift_h < 0.0) return fail("enemy_learning: hold_fire in [0, 1], shift_h >= 0");
    }
    if (const json::Value* r = root.Find("reports")) {
        if (Unknown(*r, "reports", {"seen_s", "missing_s", "claim_s", "hop_work_s", "precedence", "match_s", "match_m", "source", "confidence", "notes"}, &e)) {
            return fail(e);
        }
        ReportsDef& d = out.reports;
        d.seen_s = Num(*r, "seen_s", d.seen_s);
        if (r->Find("missing_s") != nullptr && !Pair(*r, "missing_s", d.missing_min_s, d.missing_max_s)) return fail("reports.missing_s: [min, max]");
        d.claim_s = Num(*r, "claim_s", d.claim_s);
        d.hop_work_s = Num(*r, "hop_work_s", d.hop_work_s);
        d.precedence = static_cast<int>(Num(*r, "precedence", d.precedence));
        d.match_s = Num(*r, "match_s", d.match_s);
        d.match_m = Num(*r, "match_m", d.match_m);
        if (d.seen_s < 0.0 || d.missing_max_s < d.missing_min_s || d.claim_s < 0.0) return fail("reports: delays >= 0, missing_s [min, max]");
    }
    if (const json::Value* f = root.Find("false_friend")) {
        if (Unknown(*f, "false_friend", {"on", "allied_share", "source", "confidence", "notes"}, &e)) return fail(e);
        if (const json::Value* on = f->Find("on")) out.false_friend.on = on->Is(json::Type::Bool) && on->boolean;
        out.false_friend.allied_share = Num(*f, "allied_share", out.false_friend.allied_share);
        if (out.false_friend.allied_share < 0.0 || out.false_friend.allied_share > 1.0) return fail("false_friend.allied_share: in [0, 1]");
    }
    if (out.Readiness(1) == nullptr || out.Readiness(2) == nullptr) return fail("readiness: states 1 and 2 are needed");
    return true;
}

inline bool LoadIffCodesFile(const std::string& path, IffCodesDef& out, std::string* error = nullptr) {
    using namespace own_air_detail;
    const auto fail = [&](const std::string& w) {
        if (error != nullptr) *error = path + ": " + w;
        return false;
    };
    json::Value root;
    if (!ReadFile(path, root, error)) return false;
    std::string e;
    if (Unknown(root, "", {"iff_id", "status", "source", "confidence", "notes", "system", "filters", "peace", "war", "alignment", "period_h", "document"}, &e)) {
        return fail(e);
    }
    out = IffCodesDef();
    const std::string sys = Str(root, "system", "kremniy_2m");
    out.system = sys.rfind("kremniy", 0) == 0 ? IffSystem::Kremniy : sys == "parol" ? IffSystem::Parol : sys == "mark_xii" ? IffSystem::MarkXII : IffSystem::None;
    if (out.system == IffSystem::None) return fail("system: kremniy_2m, parol or mark_xii");
    out.filters = static_cast<int>(Num(root, "filters", out.filters));
    double a = 0.0, b = 0.0;
    if (!Pair(root, "peace", a, b)) return fail("peace: [first, last]");
    if (root.Find("peace") != nullptr) {
        out.peace_first = static_cast<int>(a);
        out.peace_last = static_cast<int>(b);
    }
    if (!Pair(root, "war", a, b)) return fail("war: [first, last]");
    if (root.Find("war") != nullptr) {
        out.war_first = static_cast<int>(a);
        out.war_last = static_cast<int>(b);
    }
    out.alignment = static_cast<int>(Num(root, "alignment", out.alignment));
    if (const json::Value* p = root.Find("period_h")) {
        out.period_min_h = Num(*p, "min", out.period_min_h);
        out.period_max_h = Num(*p, "max", out.period_max_h);
    }
    if (const json::Value* d = root.Find("document")) {
        if (const json::Value* m = d->Find("means")) {
            for (const json::Value& x : m->items) {
                if (x.Is(json::Type::String)) out.document_means.push_back(x.string);
            }
        }
        out.issued_by = Str(*d, "issued_by", out.issued_by);
        out.issued_h = Num(*d, "issued_h", out.issued_h);
    }
    const auto in_range = [&](int f) { return f >= 1 && f <= out.filters; };
    if (!in_range(out.peace_first) || !in_range(out.peace_last) || !in_range(out.war_first) || !in_range(out.war_last) || out.peace_last < out.peace_first ||
        out.war_last < out.war_first) {
        return fail("peace and war: filters within 1.." + std::to_string(out.filters));
    }
    if (out.period_min_h <= 0.0 || out.period_max_h < out.period_min_h) return fail("period_h: min > 0, max >= min");
    return true;
}

// ------------------------------------------------------------------------------------------------ the codes

// The day's schedule of filters (soviet_iff: changed on a pre-arranged schedule every one to six hours). Hours are local
// clock hours from midnight of day 0 (a session's: (clock_start_s + t) / 3600). Each day starts on a new filter at
// midnight (a new paper); a filter never follows itself.
class IffSchedule {
public:
    struct Change {
        double from_h = 0.0;
        int filter = 0;
    };

    // Days first_day..last_day (inclusive), in war (filters 9-12) or peace (1-8).
    static IffSchedule Days(const IffCodesDef& c, bool war, int first_day, int last_day, std::uint64_t seed) {
        IffSchedule s;
        const int lo = war ? c.war_first : c.peace_first;
        const int hi = war ? c.war_last : c.peace_last;
        const int span = hi - lo + 1;
        int last = 0;
        for (int day = first_day; day <= last_day; ++day) {
            const std::uint64_t dk = kOwnAirKey + 0x100ull + static_cast<std::uint64_t>(day + 1000);
            double h = 24.0 * day;
            for (std::uint64_t n = 0; h < 24.0 * (day + 1) - 1e-9; ++n) {
                int f = lo + static_cast<int>(em::Uniform01(em::DrawHash(seed, em::Stream::AirCombat, dk, n, 1)) * span);
                f = std::min(f, hi);
                if (f == last && span > 1) f = lo + (f - lo + 1) % span;
                s.changes_.push_back({h, f});
                last = f;
                h += c.period_min_h + (c.period_max_h - c.period_min_h) * em::Uniform01(em::DrawHash(seed, em::Stream::AirCombat, dk, n, 2));
            }
        }
        return s;
    }
    // A fixed filter from the start (a scenario without a schedule: every holder on one filter).
    static IffSchedule Fixed(int filter) {
        IffSchedule s;
        s.changes_.push_back({-1e18, filter});
        return s;
    }

    bool Empty() const { return changes_.empty(); }
    const std::vector<Change>& Changes() const { return changes_; }
    // The change in force at hour h (-1 before the first).
    int Index(double h) const {
        int i = -1;
        for (std::size_t k = 0; k < changes_.size(); ++k) {
            if (changes_[k].from_h <= h + 1e-9) i = static_cast<int>(k);
        }
        return i;
    }
    int FilterAt(double h) const {
        const int i = Index(h);
        return i < 0 ? (changes_.empty() ? 0 : changes_.front().filter) : changes_[static_cast<std::size_t>(i)].filter;
    }
    // The next change after hour h (1e18: none).
    double NextChangeH(double h) const {
        for (const Change& c : changes_) {
            if (c.from_h > h + 1e-9) return c.from_h;
        }
        return 1e18;
    }

private:
    std::vector<Change> changes_;
};

// What a crew or a post holds: the day's paper (when it reached it), and the filter it had fitted before.
struct IffHolding {
    bool has_document = true;
    double document_h = -1e18;  // when the paper reached it
    int last_filter = 0;        // the filter it had fitted before the paper (yesterday's last; 0: none)

    int FilterAt(const IffSchedule& s, double h) const {
        if (has_document && h >= document_h && !s.Empty()) return s.FilterAt(h);
        return last_filter;
    }
};

// A pilot's transponder: he sets it from the schedule at his briefing and switches by the clock in the air; a change he
// misses (a keyed chance) leaves him on the previous filter until the next one.
inline int PilotFilterAt(const IffSchedule& s, double h, double miss, std::uint64_t seed, std::uint64_t pilot) {
    const int i = s.Index(h);
    if (i < 0) return s.FilterAt(h);
    for (int k = i; k > 0; --k) {
        const double u = em::Uniform01(em::DrawHash(seed, em::Stream::AirCombat, kOwnAirKey + 0x200ull, pilot, static_cast<std::uint64_t>(k)));
        if (u >= miss) return s.Changes()[static_cast<std::size_t>(k)].filter;
    }
    return s.Changes()[0].filter;
}

// A reply rides the interrogation only with the same system and the same filter (section 7: a code answers whoever asks with it).
inline bool IffAnswers(IffSystem transponder, int transponder_filter, IffSystem interrogator, int interrogator_filter) {
    return transponder != IffSystem::None && transponder_filter > 0 && transponder == interrogator && transponder_filter == interrogator_filter;
}

// ------------------------------------------------------------------------------------------------ the sky shared

// A corridor, a duty zone or a SAM zone's edge: a path with a half width, between heights, during clock hours.
struct OwnAirZone {
    std::string id;
    std::string kind = "corridor";  // "corridor", "duty_zone", "sam_zone"
    std::vector<Vec3> path;         // ENU (the scenario's); one point: a circle of half_width_m
    double half_width_m = 5000.0;
    double floor_m = 0.0;
    double ceiling_m = 1e9;
    double from_h = -1e18;  // local clock hours
    double to_h = 1e18;

    // Horizontal distance from the path.
    double DistanceM(const Vec3& p) const {
        if (path.empty()) return 1e18;
        if (path.size() == 1) return std::hypot(p.x - path[0].x, p.y - path[0].y);
        double best = 1e18;
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            const double ax = path[i].x, ay = path[i].y, bx = path[i + 1].x, by = path[i + 1].y;
            const double dx = bx - ax, dy = by - ay;
            const double l2 = dx * dx + dy * dy;
            double u = l2 > 1e-9 ? ((p.x - ax) * dx + (p.y - ay) * dy) / l2 : 0.0;
            u = std::clamp(u, 0.0, 1.0);
            best = std::min(best, std::hypot(p.x - (ax + u * dx), p.y - (ay + u * dy)));
        }
        return best;
    }
    bool Holds(const Vec3& p, double alt_m, double clock_h, double slack_m = 0.0, double slack_h = 0.0) const {
        if (clock_h < from_h - slack_h || clock_h > to_h + slack_h) return false;
        if (alt_m < floor_m - slack_m * 0.2 || alt_m > ceiling_m + slack_m * 0.2) return false;
        return DistanceM(p) <= half_width_m + slack_m;
    }
};

// Our dummy traffic's false corridors on a day (roadmap S6, docs/air_battle/09 section 8): `count` a day, the k-th from the
// real corridor (day + k) in turn of `real`, turned `turn_rad` about its airfield (its first point), one way on even turns and
// the other on odd ones, named "<real id> B" as the enemy would label a second corridor from the same field. A real one whose
// airfield is not working (`working(z)` false) or that has no way out gives none that turn. The campaign's daily feed
// (AirCampaign::FeedDummyCorridors) and a session's dummy transmitters (campaign/mission_session.hpp) take the same corridors.
template <class Working>
inline std::vector<OwnAirZone> FalseCorridors(const std::vector<OwnAirZone>& real, int day, int count, double turn_rad, Working working) {
    std::vector<OwnAirZone> out;
    if (real.empty()) return out;
    for (int k = 0; k < count; ++k) {
        const OwnAirZone& r = real[static_cast<std::size_t>(day + k) % real.size()];
        if (r.path.size() < 2 || !working(r)) continue;
        const double a = ((day + k) % 2 == 0 ? 1.0 : -1.0) * turn_rad;
        const Vec3 o = r.path.front(), e = r.path.back();
        const double dx = e.x - o.x, dy = e.y - o.y;
        OwnAirZone z = r;
        z.id = r.id + " B";
        z.path = {o, Vec3{o.x + dx * std::cos(a) + dy * std::sin(a), o.y - dx * std::sin(a) + dy * std::cos(a), 0.0}};
        out.push_back(z);
    }
    return out;
}

enum class CooperationMethod { ByZones, OneZone };

inline const char* CooperationMethodName(CooperationMethod m) { return m == CooperationMethod::ByZones ? "by_zones" : "one_zone"; }

// The cooperation plan as the battery was told it (its order's "own_air" block, then the notices): by zones, or one zone
// shared by height (mitronin_1976 pp. 5-9).
struct CooperationPlan {
    bool present = false;
    CooperationMethod method = CooperationMethod::ByZones;
    double split_height_m = 0.0;           // one zone by height: fighters above, SAMs below
    std::vector<OwnAirZone> zones;         // corridors, duty zones
    double fighter_floor_m = 0.0;          // above the regiment's highest kill limit (0: not given)
    double floor_unable_m = 3000.0;        // over units the records report unable to fire (mitronin_1976 p. 9)
    std::vector<std::string> units_unable; // as the regiment's records have them
    std::string recognition_signal;
    std::string fire_prohibition_signal;
    std::string code_issue;                // the identification code document's issue named in the order
    int version = 0;

    // Our fighters' floor of unrestricted use over a unit's zone: 3,000 m over one the records say cannot fire.
    double FloorOver(const std::string& unit) const {
        for (const std::string& u : units_unable) {
            if (u == unit) return floor_unable_m;
        }
        return fighter_floor_m;
    }
    // The coordination line of the order (battery_task.hpp's free text, now from the structure).
    std::string Words() const {
        std::string s = std::string("cooperation with our fighters ") + (method == CooperationMethod::ByZones ? "by zones" : "in one zone by height");
        if (method == CooperationMethod::OneZone && split_height_m > 0.0) s += " (fighters above " + std::to_string(static_cast<int>(split_height_m)) + " m)";
        for (const OwnAirZone& z : zones) {
            s += "; our fighters' " + std::string(z.kind == "duty_zone" ? "duty zone " : "corridor ") + z.id;
            if (z.ceiling_m < 1e8) s += " at " + std::to_string(static_cast<int>(z.floor_m)) + "-" + std::to_string(static_cast<int>(z.ceiling_m)) + " m";
        }
        if (!recognition_signal.empty()) s += "; recognition signal " + recognition_signal;
        if (!fire_prohibition_signal.empty()) s += "; fire prohibition signal " + fire_prohibition_signal;
        if (!code_issue.empty()) s += "; codes by " + code_issue;
        return s;
    }
};

// The "own_air" block of a task file or a scenario: {"method", "split_height_m", "fighter_floor_m", "floor_unable_m",
// "units_unable", "zones": [{"id", "kind", "path_m": [[e, n], ...], "half_width_m", "floor_m", "ceiling_m", "from_h",
// "to_h"}], "recognition_signal", "fire_prohibition_signal", "code_issue", "source", "confidence", "notes"}.
inline bool LoadCooperationPlan(const json::Value& v, const std::string& path, CooperationPlan& out, std::string* error = nullptr) {
    using namespace own_air_detail;
    const auto fail = [&](const std::string& w) {
        if (error != nullptr) *error = path + ": " + w;
        return false;
    };
    std::string e;
    if (!v.Is(json::Type::Object)) return fail("expected an object");
    if (Unknown(v, path, {"method", "split_height_m", "fighter_floor_m", "floor_unable_m", "units_unable", "zones", "recognition_signal",
                          "fire_prohibition_signal", "code_issue", "source", "confidence", "notes"}, &e)) {
        if (error != nullptr) *error = e;
        return false;
    }
    out = CooperationPlan();
    out.present = true;
    const std::string m = Str(v, "method", "by_zones");
    if (m != "by_zones" && m != "one_zone") return fail("method: by_zones or one_zone");
    out.method = m == "by_zones" ? CooperationMethod::ByZones : CooperationMethod::OneZone;
    out.split_height_m = Num(v, "split_height_m", 0.0);
    out.fighter_floor_m = Num(v, "fighter_floor_m", 0.0);
    out.floor_unable_m = Num(v, "floor_unable_m", out.floor_unable_m);
    if (const json::Value* u = v.Find("units_unable")) {
        for (const json::Value& x : u->items) {
            if (x.Is(json::Type::String)) out.units_unable.push_back(x.string);
        }
    }
    out.recognition_signal = Str(v, "recognition_signal");
    out.fire_prohibition_signal = Str(v, "fire_prohibition_signal");
    out.code_issue = Str(v, "code_issue");
    if (const json::Value* zs = v.Find("zones")) {
        if (!zs->Is(json::Type::Array)) return fail("zones: expected a list");
        for (std::size_t i = 0; i < zs->items.size(); ++i) {
            const json::Value& z = zs->items[i];
            const std::string zp = path + ".zones[" + std::to_string(i) + "]";
            if (Unknown(z, zp, {"id", "kind", "path_m", "half_width_m", "floor_m", "ceiling_m", "from_h", "to_h", "notes"}, &e)) {
                if (error != nullptr) *error = e;
                return false;
            }
            OwnAirZone d;
            d.id = Str(z, "id");
            d.kind = Str(z, "kind", d.kind);
            if (d.id.empty()) return fail("zones: each needs an id");
            if (d.kind != "corridor" && d.kind != "duty_zone" && d.kind != "sam_zone") return fail(zp + ".kind: corridor, duty_zone or sam_zone");
            const json::Value* pm = z.Find("path_m");
            if (pm == nullptr || !pm->Is(json::Type::Array) || pm->items.empty()) return fail(zp + ".path_m: [[east_m, north_m], ...]");
            for (const json::Value& pt : pm->items) {
                if (!pt.Is(json::Type::Array) || pt.items.size() < 2) return fail(zp + ".path_m: [[east_m, north_m], ...]");
                d.path.push_back(Vec3{pt.items[0].number, pt.items[1].number, 0.0});
            }
            d.half_width_m = Num(z, "half_width_m", d.half_width_m);
            d.floor_m = Num(z, "floor_m", d.floor_m);
            d.ceiling_m = Num(z, "ceiling_m", d.ceiling_m);
            d.from_h = Num(z, "from_h", d.from_h);
            d.to_h = Num(z, "to_h", d.to_h);
            if (d.ceiling_m < d.floor_m) return fail(zp + ": ceiling below floor");
            out.zones.push_back(std::move(d));
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ the notice

// A clock time said as four digits ("1405") for local clock hours, and back.
inline std::string ClockWords(double clock_h) {
    double d = std::fmod(clock_h, 24.0);
    if (d < 0.0) d += 24.0;
    int m = static_cast<int>(std::lround(d * 60.0)) % (24 * 60);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%02d%02d", m / 60, m % 60);
    return buf;
}

// The PVO grid's squares of a place (three levels: about 4 km), and back: a square read against where the reader has reason
// to think it is (the nearest big square with those row and column digits).
inline std::string SquareOf(const LocalFrame& frame, const Vec3& p) {
    const GeoPoint g = frame.EnuToGeodetic(Vec3{p.x, p.y, 0.0});
    return pvo_grid::Encode(RadToDeg(g.lat_rad), RadToDeg(g.lon_rad), 3);
}
inline bool ReadSquare(const LocalFrame& frame, const std::string& digits, const Vec3& near_to, Vec3& out) {
    if (digits.size() < 3) return false;
    const GeoPoint g = frame.EnuToGeodetic(Vec3{near_to.x, near_to.y, 0.0});
    const double lat = RadToDeg(g.lat_rad), lon = RadToDeg(g.lon_rad);
    const double lat_b = std::floor(lat / pvo_grid::kBigLatDeg) * pvo_grid::kBigLatDeg;
    const double lon_b = std::floor(lon / pvo_grid::kBigLonDeg) * pvo_grid::kBigLonDeg;
    const int row = digits[0] - '0', col = digits[1] - '0';
    double best = 1e18;
    bool found = false;
    for (int dr = -3; dr <= 3; ++dr) {
        for (int dc = -3; dc <= 3; ++dc) {
            const double lat0 = lat_b + dr * pvo_grid::kBigLatDeg, lon0 = lon_b + dc * pvo_grid::kBigLonDeg;
            const int r = ((static_cast<int>(std::floor(lat0 / pvo_grid::kBigLatDeg)) % 10) + 10) % 10;
            const int c = ((static_cast<int>(std::floor(lon0 / pvo_grid::kBigLonDeg)) % 10) + 10) % 10;
            if (r != row || c != col) continue;
            double la = 0.0, lo = 0.0;
            if (!pvo_grid::Centre(digits, lat0, lon0, la, lo)) return false;
            const Vec3 e = frame.GeodeticToEnu(GeoPoint::FromDegrees(la, lo, g.alt_m));
            const double d = std::hypot(e.x - near_to.x, e.y - near_to.y);
            if (d < best) {
                best = d;
                out = Vec3{e.x, e.y, 0.0};
                found = true;
            }
        }
    }
    return found;
}

// A notice of our flight (mt_franchuk_front p. 6): the flight, its type and count, its route as squares, its heights, the
// clock hours it crosses the SAM zones, its return, planned or actual, and its amendment.
struct OwnFlightNotice {
    int number = 0;
    int amendment = 0;
    bool actual = false;
    std::string flight;
    std::string type = "MiG-21";
    int count = 2;
    std::vector<Vec3> route;  // ENU; said as squares
    double half_width_m = 5000.0;
    double floor_m = 0.0;
    double ceiling_m = 0.0;
    double cross_from_h = 0.0;  // local clock hours
    double cross_to_h = 0.0;
    double back_h = -1.0;

    OwnAirZone AsZone() const {
        OwnAirZone z;
        z.id = flight;
        z.kind = "corridor";
        z.path = route;
        z.half_width_m = half_width_m;
        z.floor_m = floor_m;
        z.ceiling_m = ceiling_m;
        z.from_h = cross_from_h;
        z.to_h = cross_to_h;
        return z;
    }
};

// The notice as the regiment says it on its net: talk-table group 402 «В зоне свои самолёты» with its squares, heights and
// times (talk_tables.json group 402; pvo_bu_x_1984 art. 30 for the squares).
inline std::string NoticeWords(const OwnFlightNotice& n, const LocalFrame& frame) {
    std::string s = "402. Notice " + std::to_string(n.number) + ". Own aircraft: " + std::to_string(n.count) + " " + n.type + ", flight " + n.flight + ", route";
    for (std::size_t i = 0; i < n.route.size(); ++i) s += (i == 0 ? " " : " - ") + SquareOf(frame, n.route[i]);
    s += ", heights " + std::to_string(static_cast<int>(std::lround(n.floor_m))) + " to " + std::to_string(static_cast<int>(std::lround(n.ceiling_m)));
    s += ", crossing " + ClockWords(n.cross_from_h) + " to " + ClockWords(n.cross_to_h);
    if (n.back_h >= 0.0) s += ", back " + ClockWords(n.back_h);
    if (n.amendment > 0) s += ", amendment " + std::to_string(n.amendment);
    s += n.actual ? ", actual." : ", planned.";
    return s;
}

namespace own_air_detail {
inline std::size_t After(const std::string& s, const std::string& word, std::size_t from = 0) {
    const std::size_t at = s.find(word, from);
    return at == std::string::npos ? std::string::npos : at + word.size();
}
inline bool Digits(const std::string& s, std::size_t at, std::size_t n, int& out) {
    if (at == std::string::npos || at + n > s.size()) return false;
    int v = 0;
    for (std::size_t k = 0; k < n; ++k) {
        const char c = s[at + k];
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}
inline bool ClockAt(const std::string& s, std::size_t at, double near_h, double& out) {
    int hhmm = 0;
    if (!Digits(s, at, 4, hhmm)) return false;
    const double h = (hhmm / 100) + (hhmm % 100) / 60.0;
    // The clock time nearest the reader's own hour (a notice for the day it is read).
    const double base = 24.0 * std::floor(near_h / 24.0);
    double best = base + h;
    for (double d : {-24.0, 24.0}) {
        if (std::fabs(base + h + d - near_h) < std::fabs(best - near_h)) best = base + h + d;
    }
    out = best;
    return true;
}
inline bool IntAt(const std::string& s, std::size_t at, int& out) {
    if (at == std::string::npos || at >= s.size() || s[at] < '0' || s[at] > '9') return false;
    out = std::atoi(s.c_str() + at);
    return true;
}
}  // namespace own_air_detail

// A notice read back from what was written (the journal's line, slips and all): its squares read against `near_to` (the
// battery), its times against the reader's clock hour. False if the line holds no group 402 or no route.
inline bool ReadNotice(const std::string& text, const LocalFrame& frame, const Vec3& near_to, double clock_h, OwnFlightNotice& out) {
    using namespace own_air_detail;
    if (text.find("402") == std::string::npos) return false;
    OwnFlightNotice n;
    int number = 0;
    if (IntAt(text, After(text, "Notice "), number)) n.number = number;
    std::size_t at = After(text, "Own aircraft: ");
    if (at != std::string::npos) {
        int c = 0;
        if (IntAt(text, at, c)) n.count = c;
        const std::size_t sp = text.find(' ', at);
        const std::size_t comma = text.find(',', at);
        if (sp != std::string::npos && comma != std::string::npos && sp < comma) n.type = text.substr(sp + 1, comma - sp - 1);
    }
    at = After(text, "flight ");
    if (at != std::string::npos) {
        const std::size_t comma = text.find(',', at);
        n.flight = text.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    }
    at = After(text, "route ");
    if (at == std::string::npos) return false;
    const std::size_t end = text.find(',', at);
    std::string route = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
    std::size_t p = 0;
    while (p < route.size()) {
        while (p < route.size() && (route[p] < '0' || route[p] > '9')) ++p;
        std::size_t q = p;
        while (q < route.size() && route[q] >= '0' && route[q] <= '9') ++q;
        if (q > p) {
            Vec3 e;
            if (ReadSquare(frame, route.substr(p, q - p), near_to, e)) n.route.push_back(e);
        }
        p = q;
    }
    if (n.route.empty()) return false;
    at = After(text, "heights ");
    int lo = 0, hi = 0;
    if (IntAt(text, at, lo)) {
        n.floor_m = lo;
        const std::size_t to = After(text, " to ", at);
        if (IntAt(text, to, hi)) n.ceiling_m = hi;
    }
    at = After(text, "crossing ");
    double h0 = 0.0, h1 = 0.0;
    if (ClockAt(text, at, clock_h, h0)) {
        n.cross_from_h = h0;
        n.cross_to_h = ClockAt(text, After(text, " to ", at), clock_h, h1) ? h1 : h0;
        if (n.cross_to_h < n.cross_from_h) n.cross_to_h += 24.0;
    }
    at = After(text, "back ");
    double hb = 0.0;
    if (ClockAt(text, at, clock_h, hb)) n.back_h = hb;
    at = After(text, "amendment ");
    int am = 0;
    if (IntAt(text, at, am)) n.amendment = am;
    n.actual = text.find("actual") != std::string::npos;
    out = n;
    return true;
}

// ------------------------------------------------------------------------------------------------ the fire rule

enum class OwnAirCall { Free, Hold, Violator, Friend };

inline const char* OwnAirCallName(OwnAirCall c) {
    switch (c) {
        case OwnAirCall::Free: return "free";
        case OwnAirCall::Hold: return "hold";
        case OwnAirCall::Violator: return "violator";
        case OwnAirCall::Friend: return "friend";
    }
    return "?";
}

// A track as a crew holds it: where it believes it is, its height, the clock hour, and whether it answered the crew's
// interrogator.
struct TrackSeen {
    Vec3 pos;
    double alt_m = 0.0;
    double clock_h = 0.0;
    bool replied = false;
};

// The rule a crew keeps on what it was told (section 5): a track answering friend on a notified route at its height and
// hour is a friend; answering friend off every notice, a violator (reported, not engaged: pvo_bu_x_1984 art. 56); with no
// reply inside a notified corridor at its height and hour, held (by zones), or above the split height (one zone); the rest
// is free.
inline OwnAirCall JudgeTrack(const TrackSeen& t, const std::vector<OwnFlightNotice>& known, const CooperationPlan& told, double slack_m,
                             double slack_h) {
    bool notified = false;
    for (const OwnFlightNotice& n : known) notified = notified || n.AsZone().Holds(t.pos, t.alt_m, t.clock_h, slack_m, slack_h);
    for (const OwnAirZone& z : told.zones) notified = notified || z.Holds(t.pos, t.alt_m, t.clock_h, slack_m, slack_h);
    if (t.replied) return notified || (known.empty() && told.zones.empty()) ? OwnAirCall::Friend : OwnAirCall::Violator;
    if (notified) return OwnAirCall::Hold;
    if (told.present && told.method == CooperationMethod::OneZone && told.split_height_m > 0.0 && t.alt_m > told.split_height_m) return OwnAirCall::Hold;
    return OwnAirCall::Free;
}

// ------------------------------------------------------------------------------------------------ the enemy learns

// What the enemy learns of our corridors and our holding of fire (section 9): each use of a corridor it may hear (its
// COMINT on our GCI's net and the notices' coded groups when it has broken the talk table); heard learn_after times, it
// believes the corridor, placed within sigma_m of the truth; a planner creative enough times a raid into a believed
// corridor at its hours (our batteries hold fire there), while the belief is fresh. Keyed draws on kOwnAirKey + 0x500.
struct BelievedCorridor {
    std::string id;
    std::vector<Vec3> path;
    double floor_m = 0.0;
    double ceiling_m = 0.0;
    double from_h = 0.0;   // the hours of day it was used in (0-24)
    double to_h = 0.0;
    int heard = 0;
    double last_h = -1e18; // last heard
};

class EnemyCorridorIntel {
public:
    void Init(const EnemyLearningDef& d, std::uint64_t seed) {
        def_ = d;
        seed_ = seed;
        heard_.clear();
        uses_ = 0;
    }
    // One use of a corridor by our flights at hour h (absolute clock hours). True when the enemy heard it.
    bool Use(const OwnAirZone& z, double h) {
        const std::uint64_t n = ++uses_;
        const double p = std::min(1.0, def_.heard_per_use + def_.break_talk_table * (1.0 - def_.heard_per_use));
        if (em::Uniform01(em::DrawHash(seed_, em::Stream::AirCombat, kOwnAirKey + 0x500ull, n, 1)) >= p) return false;
        Record(z, h, n);
        return true;
    }
    // One use whose hearing a session's EM world decided (roadmap S6's follow-ups: its listeners heard the use's traffic by
    // link margin, or did not): counted as a use like Use, with no draw of the chance. True when heard.
    bool UseHeard(const OwnAirZone& z, double h, bool heard) {
        const std::uint64_t n = ++uses_;
        if (!heard) return false;
        Record(z, h, n);
        return true;
    }
    // A save's state (S1): what it heard so far and how many uses it has counted (the draws are keyed on the count).
    const std::vector<BelievedCorridor>& Heard() const { return heard_; }
    std::uint64_t Uses() const { return uses_; }
    void Restore(const std::vector<BelievedCorridor>& heard, std::uint64_t uses) {
        heard_ = heard;
        uses_ = uses;
    }
    // The corridors it believes at hour h: heard often enough, recently enough.
    std::vector<BelievedCorridor> Believed(double h) const {
        std::vector<BelievedCorridor> out;
        for (const BelievedCorridor& c : heard_) {
            if (c.heard >= def_.learn_after && h - c.last_h <= def_.use_age_h) out.push_back(c);
        }
        return out;
    }
    // Does a planner of this creativity send a raid by a believed corridor? (Only a creative one: use_creativity.)
    bool Exploits(double creativity, double h) const { return creativity >= def_.use_creativity && !Believed(h).empty(); }

private:
    // A use heard: the corridor believed where it was heard (placed within sigma_m on its first hearing), its hours widened.
    void Record(const OwnAirZone& z, double h, std::uint64_t n) {
        BelievedCorridor* b = nullptr;
        for (BelievedCorridor& c : heard_) {
            if (c.id == z.id) b = &c;
        }
        if (b == nullptr) {
            heard_.push_back(BelievedCorridor());
            b = &heard_.back();
            b->id = z.id;
            const double ex = def_.sigma_m * em::StandardNormal(em::DrawHash(seed_, em::Stream::AirCombat, kOwnAirKey + 0x501ull, n, 1));
            const double ey = def_.sigma_m * em::StandardNormal(em::DrawHash(seed_, em::Stream::AirCombat, kOwnAirKey + 0x501ull, n, 2));
            for (const Vec3& q : z.path) b->path.push_back(q + Vec3{ex, ey, 0.0});
            b->floor_m = z.floor_m;
            b->ceiling_m = z.ceiling_m;
            b->from_h = std::fmod(z.from_h, 24.0);
            b->to_h = std::fmod(z.to_h, 24.0);
        }
        else {  // heard again: the hours it was used in widen to take this use in (hours of day, no wrap past midnight)
            b->from_h = std::min(b->from_h, std::fmod(z.from_h, 24.0));
            b->to_h = std::max(b->to_h, std::fmod(z.to_h, 24.0));
        }
        ++b->heard;
        b->last_h = h;
    }

    EnemyLearningDef def_;
    std::uint64_t seed_ = 0;
    std::vector<BelievedCorridor> heard_;
    std::uint64_t uses_ = 0;
};

// ------------------------------------------------------------------------------------------------ the regiments' reports

// A fighter regiment's report of a loss or a kill (S1, section 8): as its pilots and controllers believe it, never the truth
// beyond what they saw (a loss to our own missile is "by a missile from below" when the wingman saw it, else "cause unknown").
enum class OwnAirReportKind { Loss, Claim };
struct OwnAirReport {
    OwnAirReportKind kind = OwnAirReportKind::Loss;
    std::string regiment;
    std::string flight;
    std::string type = "MiG-21";   // ours for a loss; the enemy's as claimed for a claim
    int count = 1;
    double clock_h = 0.0;          // when it happened (local clock hours)
    Vec3 where;                    // ENU (the session's)
    std::string cause;             // a loss: "in air combat", "by a missile from below", "cause unknown"
};

// The report as the regiment says it to the battery on its net: talk-table words (a sam3d placeholder group 403, as group 402
// is the notice), the PVO grid square, the clock time.
inline std::string OwnAirReportWords(const OwnAirReport& r, const LocalFrame& frame) {
    std::string s = "403. Fighter regiment " + r.regiment + ": ";
    if (r.kind == OwnAirReportKind::Loss) s += "loss, " + std::to_string(r.count) + " " + r.type;
    else s += "claim, " + std::to_string(r.count) + " " + r.type + " destroyed";
    s += ", flight " + r.flight + ", square " + SquareOf(frame, r.where) + ", at " + ClockWords(r.clock_h);
    if (r.kind == OwnAirReportKind::Loss) s += ", " + (r.cause.empty() ? std::string("cause unknown") : r.cause);
    return s + ".";
}

// The same read back from what was written (the regiment's check reads the journal's line, slips and all).
inline bool ReadOwnAirReport(const std::string& text, const LocalFrame& frame, const Vec3& near_to, double clock_h, OwnAirReport& out) {
    using namespace own_air_detail;
    out = OwnAirReport();
    std::size_t at = text.find("403. Fighter regiment ");
    if (at == std::string::npos) return false;
    at += 22;
    const std::size_t colon = text.find(": ", at);
    if (colon == std::string::npos) return false;
    out.regiment = text.substr(at, colon - at);
    std::size_t p = colon + 2;
    if (text.compare(p, 6, "loss, ") == 0) {
        out.kind = OwnAirReportKind::Loss;
        p += 6;
    } else if (text.compare(p, 7, "claim, ") == 0) {
        out.kind = OwnAirReportKind::Claim;
        p += 7;
    } else {
        return false;
    }
    if (!IntAt(text, p, out.count)) return false;
    const std::size_t sp = text.find(' ', p);
    const std::size_t fl = text.find(", flight ", p);
    if (sp == std::string::npos || fl == std::string::npos || fl < sp) return false;
    out.type = text.substr(sp + 1, fl - sp - 1);
    const std::string destroyed = " destroyed";
    if (out.kind == OwnAirReportKind::Claim && out.type.size() > destroyed.size() &&
        out.type.compare(out.type.size() - destroyed.size(), destroyed.size(), destroyed) == 0) {
        out.type.resize(out.type.size() - destroyed.size());
    }
    const std::size_t sq = text.find(", square ", fl);
    if (sq == std::string::npos) return false;
    out.flight = text.substr(fl + 9, sq - fl - 9);
    const std::size_t sq_end = text.find(',', sq + 9);
    if (sq_end == std::string::npos) return false;
    if (!ReadSquare(frame, text.substr(sq + 9, sq_end - sq - 9), near_to, out.where)) return false;
    const std::size_t clk = text.find(", at ", sq_end);
    if (clk == std::string::npos || !ClockAt(text, clk + 5, clock_h, out.clock_h)) return false;
    if (out.kind == OwnAirReportKind::Loss) {
        const std::size_t c = text.find(", ", clk + 5);
        if (c != std::string::npos) {
            out.cause = text.substr(c + 2);
            if (!out.cause.empty() && out.cause.back() == '.') out.cause.pop_back();
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ in the air

// Our fighters' line beyond the front (S1, section 3; mt_turchin_coop p. 9): a flight breaks off 25-30 km past the front line,
// each regiment's within the window by a keyed draw (kOwnAirKey + 0x700).
inline double PursuitLimitM(const OwnAirDef& d, std::uint64_t seed, std::uint64_t regiment_key) {
    const double u = em::Uniform01(em::DrawHash(seed, em::Stream::AirCombat, kOwnAirKey + 0x700ull, regiment_key, 1));
    return 1000.0 * (d.pursuit_min_km + (d.pursuit_max_km - d.pursuit_min_km) * u);
}

// An airborne-alert zone (section 3; mt_turchin_coop pp. 6-7): its centre so far inside our territory from the front line,
// its height in the low or the high band, each by a keyed draw (kOwnAirKey + 0xA00). `front` is a point of the front line and
// `to_enemy` the unit horizontal direction across it toward the enemy.
struct AlertZone {
    Vec3 centre;
    double alt_m = 2000.0;
    bool low = true;
};
inline AlertZone AlertZoneOf(const OwnAirDef& d, const Vec3& front, const Vec3& to_enemy, bool low, std::uint64_t seed, std::uint64_t regiment_key) {
    const double u1 = em::Uniform01(em::DrawHash(seed, em::Stream::AirCombat, kOwnAirKey + 0xA00ull, regiment_key, 1));
    const double u2 = em::Uniform01(em::DrawHash(seed, em::Stream::AirCombat, kOwnAirKey + 0xA00ull, regiment_key, 2));
    AlertZone z;
    z.low = low;
    const double inside_m = 1000.0 * (d.alert_inside_min_km + (d.alert_inside_max_km - d.alert_inside_min_km) * u1);
    z.centre = Vec3{front.x - to_enemy.x * inside_m, front.y - to_enemy.y * inside_m, 0.0};
    z.alt_m = low ? d.alert_low_min_m + (d.alert_low_max_m - d.alert_low_min_m) * u2 : d.alert_high_min_m + (d.alert_high_max_m - d.alert_high_min_m) * u2;
    return z;
}

// The allied false friend (section 7): is this enemy aircraft one of the Pact's allies' (on Kremniy), by a keyed draw
// (kOwnAirKey + 0x600), and the allies' own schedule of the war filters (another day's draw, kOwnAirKey + 0x610).
inline bool AlliedAircraft(const FalseFriendDef& d, std::uint64_t seed, std::uint64_t aircraft_key) {
    if (!d.on) return false;
    return em::Uniform01(em::DrawHash(seed, em::Stream::AirCombat, kOwnAirKey + 0x600ull, aircraft_key, 1)) < d.allied_share;
}

// ------------------------------------------------------------------------------------------------ the trace

// A notice's way down the chain (the debrief's chain from the truth: the post and the hour each had it).
struct NoticeHop {
    std::string post;
    double h = 0.0;
};
struct NoticeTrace {
    std::string flight;
    double sent_h = -1e18;          // when its origin sent it (session hours: before the session is negative; -1e18 unknown)
    std::vector<NoticeHop> hops;
    double at_battery_h = -1.0;     // said on the regiment's net to the battery (-1: never)
    double written_h = -1.0;        // written in the battery's journal (-1: never)
};

// Why the battery fired on one of ours, from the records (section 8): the journal's notice against the launch, the codes
// each side held.
struct FriendlyFireFinding {
    std::string aircraft;
    std::string flight;
    double launch_h = 0.0;
    std::string square;
    bool notice_in_journal = false;   // a notice of the flight was written before the launch
    bool notice_late = false;         // one reached the battery after the launch
    bool no_notice = false;           // none reached it
    bool code_mismatch = false;       // the aircraft answered on another filter than the battery's
    bool transponder_off = false;
    int battery_filter = 0;
    int aircraft_filter = 0;
    std::string why;
    std::string chain;                // the notice's way, post by post (debrief only)
};

inline FriendlyFireFinding TraceFriendlyFire(const std::string& aircraft, const std::string& flight, double launch_h, const std::string& square,
                                             const NoticeTrace* trace, int battery_filter, int aircraft_filter, double start_hour) {
    FriendlyFireFinding f;
    f.aircraft = aircraft;
    f.flight = flight;
    f.launch_h = launch_h;
    f.square = square;
    f.battery_filter = battery_filter;
    f.aircraft_filter = aircraft_filter;
    f.transponder_off = aircraft_filter <= 0;
    f.code_mismatch = aircraft_filter > 0 && aircraft_filter != battery_filter;
    const auto clock = [start_hour](double h) { return ClockWords(h + start_hour); };
    if (trace == nullptr || trace->at_battery_h < 0.0) {
        f.no_notice = true;
    } else if (trace->written_h >= 0.0 && trace->written_h <= launch_h) {
        f.notice_in_journal = true;
    } else {
        f.notice_late = true;
    }
    if (f.no_notice) f.why = "no notice of the flight reached the battery";
    else if (f.notice_late) f.why = "the notice reached the battery at " + clock(trace->at_battery_h) + ", after the launch at " + clock(launch_h);
    else f.why = "the notice was in the journal at " + clock(trace->written_h) + ", before the launch at " + clock(launch_h);
    if (f.transponder_off) f.why += "; the aircraft gave no reply (its transponder off)";
    else if (f.code_mismatch) f.why += "; the battery interrogated on filter " + std::to_string(battery_filter) + ", the aircraft answered on " + std::to_string(aircraft_filter);
    if (trace != nullptr) {
        f.chain = "sent " + (trace->sent_h > -1e17 ? clock(trace->sent_h) : std::string("?"));
        for (const NoticeHop& hp : trace->hops) f.chain += ", " + hp.post + " " + clock(hp.h);
        f.chain += trace->at_battery_h >= 0.0 ? ", the battery's net " + clock(trace->at_battery_h) : std::string(", never at the battery");
        if (trace->written_h >= 0.0) f.chain += ", the journal " + clock(trace->written_h);
    }
    return f;
}

// ------------------------------------------------------------------------------------------------ the duty

// A fighter regiment as the rule table sees it: its base, its type, its people's skill, its base's working share (a struck
// base launches fewer and later), and the area whose objects it covers.
struct FighterRegimentDef {
    std::string id;
    std::string base;          // the base's place id
    Vec3 base_pos;             // ENU (the session's)
    std::string type = "mig21mf";
    std::string name = "MiG-21";
    double skill = 0.45;
    int aircraft = 36;
    double function = 1.0;
    Vec3 area_centre;          // the objects it covers
    double area_radius_m = 60000.0;
    std::string gci;           // the post that controls its flights
};

// A track on our sector's picture (our radars and warning posts, with their delays and errors), never the truth.
struct SectorTrack {
    std::string id;
    Vec3 pos;
    Vec3 vel;
    bool hostile = true;
};

struct Scramble {
    std::string regiment;
    int number = 0;            // the pair's number in the regiment's book
    int readiness = 1;
    double ordered_s = 0.0;
    double airborne_s = 0.0;
    std::string against;       // the group's first track
    Vec3 toward;               // where the group was on the picture
    Vec3 base;
    std::string why;
    bool alert = false;        // up to an airborne-alert zone (S1), not against a group
};

// The fighter regiments' duty and scrambles by the rule table (section 3), on our sector's picture.
class FighterDuty {
public:
    void Init(const OwnAirDef& def, const std::vector<FighterRegimentDef>& regiments, double clock_start_s, std::uint64_t seed) {
        def_ = &def;
        seed_ = seed;
        clock_start_s_ = clock_start_s;
        regs_.clear();
        for (const FighterRegimentDef& r : regiments) {
            Reg g;
            g.def = r;
            const bool night = Night(0.0);
            const double f = std::clamp(r.function, 0.0, 1.0);
            g.r1 = static_cast<int>(std::lround((night ? def.duty.night_pairs_r1 : def.duty.day_pairs_r1) * f));
            g.r2 = static_cast<int>(std::lround((night ? def.duty.night_pairs_r2 : def.duty.day_pairs_r2) * f));
            regs_.push_back(g);
        }
    }
    // A raid warning at the sector's command post: more pairs to readiness 1 (from 2).
    void RaidWarning(double t) {
        if (warned_) return;
        warned_ = true;
        warned_s_ = t;
        for (Reg& g : regs_) {
            const int n = std::min(def_->duty.warning_extra_r1, g.r2);
            g.r1 += n;
            g.r2 -= n;
        }
    }
    // The scrambles the rule table orders now: against each group on the picture coming at a regiment's area that no pair
    // answers yet, the pairs of the nearest regiment ready soonest, while the controller has a channel free.
    std::vector<Scramble> Step(double t, const std::vector<SectorTrack>& picture, int free_channels) {
        std::vector<Scramble> out;
        if (def_ == nullptr) return out;
        // The groups: hostile tracks within group_m of each other.
        std::vector<std::vector<const SectorTrack*>> groups;
        for (const SectorTrack& s : picture) {
            if (!s.hostile) continue;
            bool put = false;
            for (auto& g : groups) {
                if ((g.front()->pos - s.pos).Length() < def_->duty.group_m) {
                    g.push_back(&s);
                    put = true;
                    break;
                }
            }
            if (!put) groups.push_back({&s});
        }
        for (const auto& g : groups) {
            if (free_channels <= 0) break;
            const SectorTrack& lead = *g.front();
            bool answered = false;
            for (const Scramble& s : orders_) answered = answered || s.against == lead.id || (s.toward - lead.pos).Length() < def_->duty.group_m;
            for (const std::string& id : answered_) answered = answered || id == lead.id;
            if (answered) continue;
            // The regiment whose area it comes at.
            Reg* best = nullptr;
            double best_d = 1e18;
            for (Reg& r : regs_) {
                const Vec3 to = r.def.area_centre - lead.pos;
                const double d = std::hypot(to.x, to.y);
                const bool coming = lead.vel.x * to.x + lead.vel.y * to.y > 0.0 || d < r.def.area_radius_m;
                if (d > def_->duty.scramble_reach_m + r.def.area_radius_m || !coming || r.r1 + r.r2 <= 0) continue;
                if (d < best_d) {
                    best_d = d;
                    best = &r;
                }
            }
            if (best == nullptr) continue;
            answered_.push_back(lead.id);
            for (int k = 0; k < def_->duty.pairs_per_group && free_channels > 0 && best->r1 + best->r2 > 0; ++k) {
                Scramble s = Take(*best, t, lead.pos, "a raid group on the sector's picture coming at " + best->def.id + "'s area, a guidance channel free");
                s.against = lead.id;
                orders_.push_back(s);
                out.push_back(s);
                --free_channels;
            }
        }
        return out;
    }
    // Airborne alert on a raid warning (S1, mt_turchin_coop pp. 6-7): `pairs` of each regiment's ready pairs up to a zone
    // (`zone_of` gives the regiment's zone centre); each a scramble with alert set, against no group.
    std::vector<Scramble> Alert(double t, int pairs, const std::function<Vec3(const FighterRegimentDef&)>& zone_of) {
        std::vector<Scramble> out;
        if (def_ == nullptr || alerted_) return out;
        alerted_ = true;
        for (Reg& r : regs_) {
            for (int k = 0; k < pairs && r.r1 + r.r2 > 0; ++k) {
                Scramble s = Take(r, t, zone_of(r.def), "a raid warning: a pair up to " + r.def.id + "'s airborne-alert zone");
                s.alert = true;
                orders_.push_back(s);
                out.push_back(s);
            }
        }
        return out;
    }
    const std::vector<Scramble>& Orders() const { return orders_; }
    int Pairs(const std::string& regiment, int state) const {
        for (const Reg& r : regs_) {
            if (r.def.id == regiment) return state == 1 ? r.r1 : state == 2 ? r.r2 : 0;
        }
        return 0;
    }
    const FighterRegimentDef* Regiment(const std::string& id) const {
        for (const Reg& r : regs_) {
            if (r.def.id == id) return &r.def;
        }
        return nullptr;
    }
    bool Warned() const { return warned_; }

    static std::uint64_t Key(const std::string& s) {
        std::uint64_t h = 1469598103934665603ull;
        for (char c : s) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
        return h;
    }

private:
    struct Reg {
        FighterRegimentDef def;
        int r1 = 0;
        int r2 = 0;
        int launched = 0;
    };
    // A pair taken from the regiment's readiness 1, else 2, airborne by its state's window (a keyed draw, kOwnAirKey + 0x300).
    Scramble Take(Reg& r, double t, const Vec3& toward, const std::string& why) {
        Scramble s;
        s.regiment = r.def.id;
        s.number = ++r.launched;
        s.readiness = r.r1 > 0 ? 1 : 2;
        (s.readiness == 1 ? r.r1 : r.r2) -= 1;
        s.ordered_s = t;
        const ReadinessDef* rd = def_->Readiness(s.readiness);
        const double u = em::Uniform01(em::DrawHash(seed_, em::Stream::AirCombat, kOwnAirKey + 0x300ull, Key(r.def.id), static_cast<std::uint64_t>(s.number)));
        double take = rd != nullptr ? rd->takeoff_min_s + (rd->takeoff_max_s - rd->takeoff_min_s) * u : 300.0;
        take /= std::max(0.25, std::clamp(r.def.function, 0.0, 1.0));  // a struck base launches later
        s.airborne_s = t + take;
        s.toward = toward;
        s.base = r.def.base_pos;
        s.why = why;
        return s;
    }
    bool Night(double t) const {
        double h = std::fmod((clock_start_s_ + t) / 3600.0, 24.0);
        if (h < 0.0) h += 24.0;
        const DutyDef& d = def_->duty;
        return d.night_from_h > d.night_to_h ? (h >= d.night_from_h || h < d.night_to_h) : (h >= d.night_from_h && h < d.night_to_h);
    }

    const OwnAirDef* def_ = nullptr;
    std::uint64_t seed_ = 0;
    double clock_start_s_ = 0.0;
    std::vector<Reg> regs_;
    std::vector<Scramble> orders_;
    std::vector<std::string> answered_;
    bool warned_ = false;
    double warned_s_ = -1.0;
    bool alerted_ = false;
};

}  // namespace command
}  // namespace simcore
