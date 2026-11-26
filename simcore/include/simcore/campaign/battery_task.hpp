// simcore/campaign/battery_task.hpp — the battery's task: the regiment's combat order as a document (roadmap 14.1;
// docs/campaign/03_THE_BATTERY_MISSION.md §2, 05_THE_SYSTEMS.md §4).
//
// A battery changes position only on the regiment commander's order (voenizdat_osa_1991 pp. 232-239). The order's parts
// follow what the textbook has the battery commander put on his working map (pp. 240-245): the task and its reason, the
// situation (the attack directions, the helicopter, jammer and ARM launch lines), the position area (the main position,
// the reserve positions within 2 km, the dummies, the TZM's place), each launcher's sector of responsibility, readiness
// and emission, the time, coordination, reports and support. Places are named by their ids (anchors of the route
// service, nav/anchors.hpp), never by coordinates, so a changed map needs no change to a task.
//
// It travels as a document (the full paper) or, by radio, in its short form (TaskVoiceSummary), which the enemy's
// intercept service hears like any call. The battery commander's planner (campaign/battery_plan.hpp) turns it into the
// battery's orders (battery.hpp).
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "simcore/command/own_air.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace campaign {

enum class TaskKind { CoverObject, CoverTroops, CoverMarch, CoverCrossing, Ambush, Relocate, CoverRebuild };

inline const char* TaskKindName(TaskKind k) {
    switch (k) {
        case TaskKind::CoverObject: return "cover_object";
        case TaskKind::CoverTroops: return "cover_troops";
        case TaskKind::CoverMarch: return "cover_march";
        case TaskKind::CoverCrossing: return "cover_crossing";
        case TaskKind::Ambush: return "ambush";
        case TaskKind::Relocate: return "relocate";
        case TaskKind::CoverRebuild: return "cover_rebuild";
    }
    return "?";
}

inline bool ParseTaskKind(const std::string& s, TaskKind& out) {
    for (TaskKind k : {TaskKind::CoverObject, TaskKind::CoverTroops, TaskKind::CoverMarch, TaskKind::CoverCrossing, TaskKind::Ambush,
                       TaskKind::Relocate, TaskKind::CoverRebuild}) {
        if (s == TaskKindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

// A line of the situation on the working map, as bearings from the covered object (true, clockwise from..to).
struct TaskLine {
    std::string kind;       // "attack", "helicopter", "jammer", "arm"
    double from_deg = 0.0;
    double to_deg = 0.0;
    double range_km = 0.0;  // how far out (a jammer's orbit, an ARM launch line); 0: not given
    std::string text;
    // The middle of the span, clockwise from `from_deg`.
    double MiddleDeg() const {
        double span = to_deg - from_deg;
        while (span < 0.0) span += 360.0;
        double m = from_deg + 0.5 * span;
        while (m >= 360.0) m -= 360.0;
        return m;
    }
};

// A launcher's sector of responsibility (pp. 243, 262-266: duty launchers 45-60 deg on the helicopter lines).
struct TaskSector {
    std::string vehicle;
    double from_deg = 0.0;
    double to_deg = 0.0;
    bool duty = false;
};

// The order's limits (track O, O4; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md sections 3 (D3, D4) and 6): the rules
// within which the battery commander, and through his combat order each launcher commander, decides for himself. They are
// rules, not walls: an NPC keeps them, a player may break them, and a breach counts only as the records show it
// (campaign/report.hpp's CheckLimits). A value < 0 takes the held map's (data/gameplay/held_map.json).
struct OrderLimits {
    double spot_m = -1.0;           // the battery's position within this of the order's main point (D3)
    double launcher_spot_m = -1.0;  // a launcher commander's spot within this of his place in the formation (D3's 1 km)
    std::string cover_unit;         // the unit the battery keeps with: a mover of the session's roads ("" none)
    double leash_m = -1.0;          // within this of it (pp. 232-239: 1.5-3 km behind the covered companies)
    std::string fire;               // "designation" (on the battery commander's only) or "independent" ("": not limited)
    bool Any() const { return spot_m >= 0.0 || launcher_spot_m >= 0.0 || !cover_unit.empty() || leash_m >= 0.0 || !fire.empty(); }
};

// The fire modes (pvo_bu_ix_1984: search on the senior commander's designation or independently; the period's names are
// to be confirmed, docs/campaign/12 section 6).
inline const char* FireModeWords(const std::string& mode) {
    if (mode == "designation") return "on the battery commander's designation only";
    if (mode == "independent") return "search and fire on your own in your sector";
    return "as it is";
}
// The emission regimes from the least to the most emitting (emission_regime.hpp's): a regime above the order's is a breach.
inline int EmissionOpenness(const std::string& regime) {
    if (regime == "silent") return 0;
    if (regime == "arm") return 1;
    if (regime == "limited") return 2;
    if (regime == "free") return 3;
    return -1;
}

struct BatteryTask {
    std::string id;
    TaskKind kind = TaskKind::CoverObject;
    std::string given_by;      // the post that gives it (data/command/chain_pannonia.json)
    std::string unit;          // "battery 21"
    std::string covered;       // the covered object's or unit's place (an anchor)
    std::string task_text;     // "cover the Kesmark bridges from low-flying attack"
    std::string short_ru;      // the task in the radio's short form: «прикрыть мосты Кесмарк»
    std::string area_ru;       // the main position as said on the radio («у мостов, западный берег»; "": its place's name)
    std::vector<std::string> reasons;
    std::vector<TaskLine> situation;
    std::string habits;        // what the regiment knows of the enemy's habits
    std::string main;          // the main position (a place)
    std::vector<std::string> reserve;
    std::vector<std::string> dummies;
    std::string tzm_place;
    std::vector<TaskSector> sectors;
    int readiness = 1;         // the readiness to hold: no. 1 or no. 2
    std::string readiness_text;
    std::string emission;      // the emission regime by name ("free", "limited", "arm", "silent"; "": as it is)
    double on_turns = 0.0;     // its bursts (limited, arm; 0: the data's)
    double off_turns = 0.0;
    bool reconnoitre = false;  // the position is new to the battery: the commander reconnoitres it before deciding (p243)
    bool roving = false;       // an ambush: the pairs move between its points
    double axis_deg = 0.0;     // a crossing: across the river, toward the far bank; troops: their direction of advance
    double width_m = 100.0;    // a crossing: the river's width
    double ready_in_s = 0.0;   // ready this long after the order is given
    double lasts_s = -1.0;     // the task's length from the order (< 0: until relieved)
    std::vector<std::string> coordination;
    std::vector<std::string> reports;
    std::vector<std::string> support;
    // The plan of cooperation with our fighters (S1, docs/air_battle/08_OUR_FIGHTERS.md section 5): by zones or one zone, the
    // corridors and duty zones with their heights and hours (ENU of the task's map), the signals and the codes' issue.
    command::CooperationPlan own_air;
    OrderLimits limits;        // O4: the order's limits (none: as before)
    // Track O, O6: where the regiment's command post stands for this task (a place, an anchor of the roads; "": where it
    // is). A task with the division's move names the new one: the regiment's column goes there, and its evening briefings
    // are held there once it has arrived (campaign/mission_session.hpp).
    std::string regiment_cp;

    // The main attack direction from the covered object (the first attack line's middle); false if the order gives none.
    bool MainAttackDeg(double& out) const {
        for (const TaskLine& l : situation) {
            if (l.kind == "attack") {
                out = l.MiddleDeg();
                return true;
            }
        }
        return false;
    }
    // Every place the task names, for a check that each is an anchor.
    std::vector<std::string> Places() const {
        std::vector<std::string> out;
        for (const std::string& p : {covered, main, tzm_place, regiment_cp}) {
            if (!p.empty()) out.push_back(p);
        }
        out.insert(out.end(), reserve.begin(), reserve.end());
        out.insert(out.end(), dummies.begin(), dummies.end());
        return out;
    }
};

namespace detail {

class BatteryTaskLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, BatteryTask& t) {
        if (!Keys(root, "", {"task_id", "kind", "given_by", "unit", "covered", "task", "short_ru", "area_ru", "reasons", "situation", "habits",
                             "area", "sectors", "readiness", "readiness_text", "emission", "on_turns", "off_turns", "reconnoitre",
                             "roving", "axis_deg", "width_m", "ready_in_s", "lasts_s", "coordination", "reports", "support",
                             "own_air", "limits", "regiment_cp", "source", "confidence", "notes"}) ||
            !Provenance(root, "")) {
            return false;
        }
        std::string kind;
        if (!RequireString(root, "", "task_id", t.id) || !RequireString(root, "", "kind", kind) ||
            !RequireString(root, "", "given_by", t.given_by) || !RequireString(root, "", "unit", t.unit) ||
            !OptionalString(root, "", "covered", t.covered) || !RequireString(root, "", "task", t.task_text) ||
            !RequireString(root, "", "short_ru", t.short_ru) || !OptionalString(root, "", "area_ru", t.area_ru) ||
            !OptionalString(root, "", "habits", t.habits) || !OptionalString(root, "", "regiment_cp", t.regiment_cp) ||
            !OptionalString(root, "", "readiness_text", t.readiness_text) || !OptionalString(root, "", "emission", t.emission) ||
            !OptionalNumber(root, "", "on_turns", t.on_turns) || !OptionalNumber(root, "", "off_turns", t.off_turns) ||
            !OptionalBool(root, "", "reconnoitre", t.reconnoitre) || !OptionalBool(root, "", "roving", t.roving) ||
            !OptionalNumber(root, "", "axis_deg", t.axis_deg) || !OptionalNumber(root, "", "width_m", t.width_m) ||
            !OptionalNumber(root, "", "ready_in_s", t.ready_in_s) || !OptionalNumber(root, "", "lasts_s", t.lasts_s) ||
            !OptionalInt(root, "", "readiness", t.readiness)) {
            return false;
        }
        if (!ParseTaskKind(kind, t.kind)) {
            return Fail("kind", "cover_object, cover_troops, cover_march, cover_crossing, ambush, relocate or cover_rebuild");
        }
        if (t.readiness != 1 && t.readiness != 2) return Fail("readiness", "1 or 2");
        if (!t.emission.empty() && t.emission != "free" && t.emission != "limited" && t.emission != "arm" && t.emission != "silent") {
            return Fail("emission", "free, limited, arm or silent");
        }
        if ((t.on_turns > 0.0 || t.off_turns > 0.0) && t.emission != "limited" && t.emission != "arm") {
            return Fail("on_turns", "only the limited and arm regimes have bursts");
        }
        if (!(t.ready_in_s > 0.0)) return Fail("ready_in_s", "required: how long after the order the battery is to be ready, > 0");
        if (t.lasts_s >= 0.0 && t.lasts_s < t.ready_in_s) return Fail("lasts_s", "the task ends before the battery is to be ready");
        if (t.width_m <= 0.0) return Fail("width_m", "> 0");
        const auto any = [](const std::string&) { return std::string(); };
        if (!StringList(root, "", "reasons", t.reasons, any) || !StringList(root, "", "coordination", t.coordination, any) ||
            !StringList(root, "", "reports", t.reports, any) || !StringList(root, "", "support", t.support, any)) {
            return false;
        }
        if (t.reasons.empty()) return Fail("reasons", "required: the order says why");
        if (const json::Value* oa = root.Find("own_air")) {
            std::string e;
            if (!command::LoadCooperationPlan(*oa, "own_air", t.own_air, &e)) return Fail("own_air", e);
        }
        if (!LoadArea(root, t) || !LoadSituation(root, t) || !LoadSectors(root, t) || !LoadLimits(root, t)) return false;
        if (t.kind != TaskKind::CoverMarch && t.main.empty()) return Fail("area.main", "required: where the battery goes");
        if ((t.kind == TaskKind::CoverObject || t.kind == TaskKind::CoverRebuild || t.kind == TaskKind::CoverCrossing ||
             t.kind == TaskKind::CoverTroops) && t.covered.empty()) {
            return Fail("covered", std::string("required for ") + TaskKindName(t.kind));
        }
        if (t.kind == TaskKind::Ambush && t.reserve.empty()) return Fail("area.reserve", "an ambush needs its other prepared points");
        return true;
    }

private:
    bool LoadArea(const json::Value& root, BatteryTask& t) {
        const json::Value* a = root.Find("area");
        if (a == nullptr) return true;
        const auto any = [](const std::string&) { return std::string(); };
        return Keys(*a, "area", {"main", "reserve", "dummies", "tzm", "notes"}) && OptionalString(*a, "area", "main", t.main) &&
               OptionalString(*a, "area", "tzm", t.tzm_place) && StringList(*a, "area", "reserve", t.reserve, any) &&
               StringList(*a, "area", "dummies", t.dummies, any);
    }
    bool LoadSituation(const json::Value& root, BatteryTask& t) {
        const json::Value* arr = OptionalArray(root, "", "situation");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("situation", i);
            const json::Value& v = arr->items[i];
            TaskLine l;
            if (!Keys(v, p, {"kind", "from_deg", "to_deg", "range_km", "text"}) || !RequireString(v, p, "kind", l.kind) ||
                !OptionalNumber(v, p, "from_deg", l.from_deg) || !OptionalNumber(v, p, "to_deg", l.to_deg) ||
                !OptionalNumber(v, p, "range_km", l.range_km) || !OptionalString(v, p, "text", l.text)) {
                return false;
            }
            if (l.kind != "attack" && l.kind != "helicopter" && l.kind != "jammer" && l.kind != "arm") {
                return Fail(Join(p, "kind"), "attack, helicopter, jammer or arm");
            }
            if (!Bearing(l.from_deg) || !Bearing(l.to_deg)) return Fail(p, "bearings are 0-360 deg");
            t.situation.push_back(l);
        }
        return true;
    }
    bool LoadSectors(const json::Value& root, BatteryTask& t) {
        const json::Value* arr = OptionalArray(root, "", "sectors");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("sectors", i);
            const json::Value& v = arr->items[i];
            TaskSector s;
            if (!Keys(v, p, {"vehicle", "from_deg", "to_deg", "duty"}) || !RequireString(v, p, "vehicle", s.vehicle) ||
                !OptionalNumber(v, p, "from_deg", s.from_deg) || !OptionalNumber(v, p, "to_deg", s.to_deg) ||
                !OptionalBool(v, p, "duty", s.duty)) {
                return false;
            }
            if (!Bearing(s.from_deg) || !Bearing(s.to_deg) || s.from_deg == s.to_deg) return Fail(p, "a sector is two bearings, 0-360 deg");
            for (const TaskSector& o : t.sectors) {
                if (o.vehicle == s.vehicle) return Fail(Join(p, "vehicle"), "a second sector for '" + s.vehicle + "'");
            }
            t.sectors.push_back(s);
        }
        return true;
    }
    bool LoadLimits(const json::Value& root, BatteryTask& t) {
        const json::Value* l = root.Find("limits");
        if (l == nullptr) return true;
        OrderLimits& o = t.limits;
        if (!Keys(*l, "limits", {"spot_m", "launcher_spot_m", "cover_unit", "leash_m", "fire", "source", "confidence", "notes"}) ||
            !OptionalNumber(*l, "limits", "spot_m", o.spot_m) || !OptionalNumber(*l, "limits", "launcher_spot_m", o.launcher_spot_m) ||
            !OptionalString(*l, "limits", "cover_unit", o.cover_unit) || !OptionalNumber(*l, "limits", "leash_m", o.leash_m) ||
            !OptionalString(*l, "limits", "fire", o.fire)) {
            return false;
        }
        if (!o.fire.empty() && o.fire != "designation" && o.fire != "independent") return Fail("limits.fire", "designation or independent");
        if (o.leash_m >= 0.0 && o.cover_unit.empty()) return Fail("limits.leash_m", "a leash needs the unit it is to (cover_unit)");
        return true;
    }
    static bool Bearing(double d) { return d >= 0.0 && d <= 360.0; }
};

}  // namespace detail

inline bool LoadBatteryTaskJson(const std::string& text, BatteryTask& out, std::string* error = nullptr) {
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "battery task: " + e;
        return false;
    }
    detail::BatteryTaskLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "battery task: " + loader.Message();
        return false;
    }
    return true;
}

// A task file, relative to the data directory (data/campaign/tasks/).
inline bool LoadBatteryTask(const std::string& data_dir, const std::string& rel, BatteryTask& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "battery task: cannot read " + rel;
        return false;
    }
    return LoadBatteryTaskJson(text, out, error);
}

namespace detail {

inline std::string Deg(double d) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%03.0f", d);
    return buf;
}
inline std::string JoinWords(const std::vector<std::string>& w, const char* sep) {
    std::string s;
    for (std::size_t i = 0; i < w.size(); ++i) s += (i == 0 ? "" : sep) + w[i];
    return s;
}

}  // namespace detail

// The order as the paper the battery commander holds, its parts in the textbook's order (pp. 240-245). `clock` gives a
// session second as a time of day, `place` a place's name by its id.
inline std::string TaskDocument(const BatteryTask& t, double given_s, const std::function<std::string(double)>& clock,
                                const std::function<std::string(const std::string&)>& place) {
    std::string s = "Combat order to " + t.unit + ", " + clock(given_s) + ".";
    s += "\n1. Task: " + t.task_text + ".";
    s += "\n   Why: " + detail::JoinWords(t.reasons, " ");
    if (!t.situation.empty() || !t.habits.empty()) {
        s += "\n2. The situation:";
        for (const TaskLine& l : t.situation) {
            s += "\n   " + l.kind + " " + detail::Deg(l.from_deg) + "-" + detail::Deg(l.to_deg);
            if (l.range_km > 0.0) {
                char km[24];
                std::snprintf(km, sizeof(km), ", %.0f km out", l.range_km);
                s += km;
            }
            if (!l.text.empty()) s += ": " + l.text;
        }
        if (!t.habits.empty()) s += "\n   " + t.habits;
    }
    s += "\n3. Position area: main position " + place(t.main);
    std::vector<std::string> names;
    for (const std::string& r : t.reserve) names.push_back(place(r));
    if (!names.empty()) s += "; reserve " + detail::JoinWords(names, ", ");
    names.clear();
    for (const std::string& d : t.dummies) names.push_back(place(d));
    if (!names.empty()) s += "; dummies " + detail::JoinWords(names, ", ");
    if (!t.tzm_place.empty()) s += "; the TZM at " + place(t.tzm_place);
    s += ".";
    if (!t.sectors.empty()) {
        s += "\n4. Sectors:";
        for (const TaskSector& c : t.sectors) {
            s += " " + c.vehicle + " " + detail::Deg(c.from_deg) + "-" + detail::Deg(c.to_deg) + (c.duty ? " (duty)" : "") + ";";
        }
    }
    s += "\n5. Readiness no. " + std::to_string(t.readiness) + (t.readiness_text.empty() ? std::string() : ": " + t.readiness_text);
    if (!t.emission.empty()) s += "; emission " + t.emission + " at most";
    if (!t.limits.fire.empty()) s += "; fire " + std::string(FireModeWords(t.limits.fire));
    s += ".";
    if (t.limits.spot_m >= 0.0 || t.limits.launcher_spot_m >= 0.0 || !t.limits.cover_unit.empty()) {  // O4: the order's limits
        s += "\n   Limits:";
        char buf[160];
        if (t.limits.spot_m >= 0.0) {
            std::snprintf(buf, sizeof(buf), " the battery within %.1f km of its main position;", t.limits.spot_m / 1000.0);
            s += buf;
        }
        if (t.limits.launcher_spot_m >= 0.0) {
            std::snprintf(buf, sizeof(buf), " each launcher within %.1f km of its place;", t.limits.launcher_spot_m / 1000.0);
            s += buf;
        }
        if (!t.limits.cover_unit.empty()) {
            if (t.limits.leash_m >= 0.0) {
                std::snprintf(buf, sizeof(buf), " keep within %.1f km of the covered unit (%s), behind it;", t.limits.leash_m / 1000.0, t.limits.cover_unit.c_str());
            } else {
                std::snprintf(buf, sizeof(buf), " keep with the covered unit (%s), behind it;", t.limits.cover_unit.c_str());
            }
            s += buf;
        }
        if (!s.empty() && s.back() == ';') s.back() = '.';
    }
    s += "\n6. Time: ready by " + clock(given_s + t.ready_in_s) + "; " +
         (t.lasts_s < 0.0 ? std::string("until relieved") : "until " + clock(given_s + t.lasts_s)) + ".";
    std::vector<std::string> coordination = t.coordination;
    if (t.own_air.present) coordination.push_back(t.own_air.Words());
    if (!coordination.empty()) s += "\n7. Coordination: " + detail::JoinWords(coordination, "; ") + ".";
    if (!t.reports.empty()) s += "\n8. Reports: " + detail::JoinWords(t.reports, "; ") + ".";
    if (!t.support.empty()) s += "\n9. Support: " + detail::JoinWords(t.support, "; ") + ".";
    return s;
}

// The order's short form for the radio (the regiment's duty officer to the battery commander): the task, the main
// position and the time; the rest follows on paper.
inline std::string TaskVoiceSummary(const BatteryTask& t, double given_s, const std::function<std::string(double)>& clock,
                                    const std::function<std::string(const std::string&)>& place) {
    return "Боевая задача: " + t.short_ru + ". Район — " + (t.area_ru.empty() ? place(t.main) : t.area_ru) + ". Готовность — " +
           clock(given_s + t.ready_in_s) + ".";
}

}  // namespace campaign
}  // namespace simcore
