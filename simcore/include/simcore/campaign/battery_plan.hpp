// simcore/campaign/battery_plan.hpp — the battery commander's planner, first form (roadmap 14.3, 14.5;
// docs/campaign/03_THE_BATTERY_MISSION.md §3-4, 05_THE_SYSTEMS.md §5).
//
// The textbook's sequence (voenizdat_osa_1991 pp. 240-245): understand the task, work out the time, the preliminary
// orders, assess the situation, decide, reconnoitre the position (a new one), the oral combat order, support. The steps
// and their order are the book's; their times are SAM3D estimates (data/campaign/battery_plan.json). Short of time the
// commander works each step toward its short time, all in the same proportion; with even that too long the battery is
// late, and the plan says by how much.
//
// What the order ends in is the mission kind's (14.5): a march by the roads to the main position (planned on the route
// service when «К маршу!» is given), and at its end the formation the task calls for: the pairs across the main attack
// direction round the main position (an object, a rebuild, a relocation), behind the covered troops on their axis, the
// river approach for a crossing, the ambush's prepared points. Already in the area, the formation follows the order.
// Covering a march (the battery in the covered column) is not planned in this form.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "simcore/battery.hpp"
#include "simcore/campaign/battery_task.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace campaign {

struct PlanStepDef {
    std::string id;
    std::string en;        // what the battery commander does, for the plan and the debrief
    std::string order;     // the crew menu's order he gives on the battery net at the step's end ("": none)
    std::string only;      // "reconnoitre": only for a position new to the battery
    double full_s = 0.0;
    double short_s = 0.0;
    bool parallel = false;  // runs alongside the march (support), not before it
};

struct PlanSequence {
    std::vector<PlanStepDef> steps;
    double occupy_s = 600.0;  // from the head's arrival at the main position to «к бою готов»
    const PlanStepDef* Find(const std::string& id) const {
        for (const PlanStepDef& s : steps) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }
};

namespace detail {

class PlanSequenceLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, PlanSequence& out) {
        if (!Keys(root, "", {"plan_id", "status", "source", "confidence", "notes", "steps", "occupy_s"}) || !Provenance(root, "")) return false;
        const json::Value* arr = OptionalArray(root, "", "steps");
        if (arr == nullptr) return Fail("steps", "required: the commander's sequence");
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("steps", i);
            const json::Value& v = arr->items[i];
            PlanStepDef s;
            if (!Keys(v, p, {"id", "en", "order", "only", "full_s", "short_s", "parallel", "source", "confidence", "notes"}) ||
                !Provenance(v, p) || !RequireString(v, p, "id", s.id) || !RequireString(v, p, "en", s.en) ||
                !OptionalString(v, p, "order", s.order) || !OptionalString(v, p, "only", s.only) ||
                !OptionalNumber(v, p, "full_s", s.full_s) || !OptionalNumber(v, p, "short_s", s.short_s) ||
                !OptionalBool(v, p, "parallel", s.parallel)) {
                return false;
            }
            if (!(s.full_s > 0.0) || s.short_s < 0.0 || s.short_s > s.full_s) return Fail(p, "0 <= short_s <= full_s, full_s > 0");
            if (!s.only.empty() && s.only != "reconnoitre") return Fail(Join(p, "only"), "\"reconnoitre\" or nothing");
            for (const PlanStepDef& o : out.steps) {
                if (o.id == s.id) return Fail(Join(p, "id"), "duplicate step '" + s.id + "'");
            }
            out.steps.push_back(s);
        }
        if (out.Find("combat_order") == nullptr) return Fail("steps", "the sequence has no combat_order step");
        const json::Value* o = root.Find("occupy_s");
        if (o == nullptr) return Fail("occupy_s", "required");
        if (!Keys(*o, "occupy_s", {"value", "source", "confidence", "notes"}) || !Provenance(*o, "occupy_s")) return false;
        const json::Value* n = o->Find("value");
        if (n == nullptr || !n->Is(json::Type::Number) || !(n->number > 0.0)) return Fail("occupy_s.value", "a number > 0");
        out.occupy_s = n->number;
        return true;
    }
};

}  // namespace detail

inline bool LoadPlanSequence(const std::string& data_dir, const std::string& rel, PlanSequence& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "battery plan: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "battery plan: " + e;
        return false;
    }
    detail::PlanSequenceLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "battery plan: " + loader.Message();
        return false;
    }
    return true;
}

// What the planner is told: when the order came, where the places are, how long the march is.
struct PlanInputs {
    double given_s = 0.0;     // the regiment gave the order
    double received_s = 0.0;  // the battery commander has it
    double march_s = -1.0;    // the head's time by the roads to the main position (< 0: no march: the battery is in the area)
    // A place's position (an anchor of the route service); false if it is unknown.
    std::function<bool(const std::string&, Vec3&)> where;
};

struct PlannedStep {
    std::string id;
    std::string en;
    std::string order;  // the crew menu's order given at its end ("": none)
    double start_s = 0.0;
    double end_s = 0.0;
};

struct BatteryPlan {
    bool planned = false;
    std::string why;  // not planned: why
    std::vector<PlannedStep> steps;
    double ready_by_s = 0.0;
    double scale = 1.0;       // the sequence's steps between their short (0) and full (1) times
    double order_s = -1.0;    // the oral combat order given: the regime, the sectors, the move
    double march_at_s = -1.0; // «К маршу!» (< 0: no march)
    double march_s = -1.0;
    double arrive_s = -1.0;   // the head at the main position
    double ready_s = -1.0;    // «к бою готов» expected
    double late_s = 0.0;      // past the ready-by (0: in time)
    std::string to_place;     // the march's end: the main position
    bool has_formation = false;
    BatteryOrder formation;   // at the march's end (or at the order, in the area): the task's formation
    bool InTime() const { return late_s <= 0.0; }
    const PlannedStep* Step(const std::string& id) const {
        for (const PlannedStep& s : steps) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }
};

namespace detail {

inline Vec3 Toward(double deg) { return Vec3{std::sin(DegToRad(deg)), std::cos(DegToRad(deg)), 0.0}; }

}  // namespace detail

// The plan for a task, from when its commander has it. `tactics` gives the formation's distances.
inline BatteryPlan PlanBatteryTask(const BatteryTask& task, const PlanSequence& seq, const BatteryTactics& tactics, const PlanInputs& in) {
    BatteryPlan plan;
    plan.ready_by_s = in.given_s + task.ready_in_s;
    if (task.kind == TaskKind::CoverMarch) {
        plan.why = "covering a march: the battery marches in the covered column (not planned in this form)";
        return plan;
    }
    Vec3 main_at, covered_at;
    if (!in.where || !in.where(task.main, main_at)) {
        plan.why = "the main position '" + task.main + "' is not on the map";
        return plan;
    }
    if (!task.covered.empty() && !in.where(task.covered, covered_at)) {
        plan.why = "the covered place '" + task.covered + "' is not on the map";
        return plan;
    }
    std::vector<Vec3> points{main_at};
    for (const std::string& r : task.reserve) {
        Vec3 p;
        if (!in.where(r, p)) {
            plan.why = "the reserve position '" + r + "' is not on the map";
            return plan;
        }
        points.push_back(p);
    }

    // The sequence, its steps worked toward their short times by one proportion to leave the march and the occupation
    // their time before the ready-by.
    std::vector<const PlanStepDef*> serial, parallel;
    for (const PlanStepDef& s : seq.steps) {
        if (s.only == "reconnoitre" && !task.reconnoitre) continue;
        (s.parallel ? parallel : serial).push_back(&s);
    }
    double full = 0.0, brief = 0.0;
    for (const PlanStepDef* s : serial) {
        full += s->full_s;
        brief += s->short_s;
    }
    const bool march = in.march_s >= 0.0;
    const double avail = plan.ready_by_s - in.received_s - (march ? in.march_s : 0.0) - seq.occupy_s;
    plan.scale = avail >= full ? 1.0 : avail <= brief || full <= brief ? 0.0 : (avail - brief) / (full - brief);
    double t = in.received_s;
    for (const PlanStepDef* s : serial) {
        const double d = s->short_s + (s->full_s - s->short_s) * plan.scale;
        plan.steps.push_back({s->id, s->en, s->order, t, t + d});
        t += d;
        if (s->id == "combat_order") plan.order_s = t;
    }
    if (plan.order_s < 0.0) plan.order_s = t;
    for (const PlanStepDef* s : parallel) {
        const double d = s->short_s + (s->full_s - s->short_s) * plan.scale;
        plan.steps.push_back({s->id, s->en, s->order, plan.order_s, plan.order_s + d});
    }
    if (march) {
        plan.march_at_s = plan.order_s;
        plan.march_s = in.march_s;
        plan.arrive_s = plan.march_at_s + in.march_s;
        plan.to_place = task.main;
    } else {
        plan.arrive_s = plan.order_s;
    }
    plan.ready_s = plan.arrive_s + seq.occupy_s;
    plan.late_s = std::max(0.0, plan.ready_s - plan.ready_by_s);

    // The formation the task calls for, at the march's end.
    BatteryOrder& f = plan.formation;
    f.from = "battery";
    double attack = 0.0;
    const bool has_attack = task.MainAttackDeg(attack);
    switch (task.kind) {
        case TaskKind::CoverObject:
        case TaskKind::CoverRebuild:
        case TaskKind::Relocate:
            if (!has_attack) break;  // no direction given: «К бою!» along the march's last leg
            // The pairs across the main attack direction, centred on the main position (FormationSlots puts them
            // behind_covered_m behind `covered` along the axis).
            f.kind = BatteryOrder::Kind::CombatFormation;
            f.axis_deg = attack;
            f.covered = main_at + detail::Toward(attack) * tactics.behind_covered_m;
            plan.has_formation = true;
            break;
        case TaskKind::CoverTroops:
            f.kind = BatteryOrder::Kind::CombatFormation;  // behind the covered companies, on their axis (pp. 232-239)
            f.axis_deg = task.axis_deg;
            f.covered = covered_at;
            plan.has_formation = true;
            break;
        case TaskKind::CoverCrossing:
            f.kind = BatteryOrder::Kind::RiverCrossing;
            f.covered = covered_at;
            f.axis_deg = task.axis_deg;
            f.width_m = task.width_m;
            plan.has_formation = true;
            break;
        case TaskKind::Ambush:
            f.kind = BatteryOrder::Kind::Ambush;
            f.route = points;
            f.roving = task.roving;
            plan.has_formation = true;
            break;
        case TaskKind::CoverMarch: break;
    }
    plan.planned = true;
    return plan;
}

// The plan as the battery commander's timeline (the ideal one the helper and the debrief go by).
// `name`: a place's name by its id (none: the id).
inline std::string PlanText(const BatteryPlan& p, const std::function<std::string(double)>& clock,
                            const std::function<std::string(const std::string&)>& name = nullptr) {
    if (!p.planned) return "Not planned: " + p.why + ".";
    std::string s = "The battery commander's plan";
    if (p.scale < 1.0) {
        char k[64];
        std::snprintf(k, sizeof(k), " (short of time: the sequence at %.0f %% of its full length)", 100.0 * p.scale);
        s += k;
    }
    s += ":";
    for (const PlannedStep& st : p.steps) s += "\n" + clock(st.start_s) + "-" + clock(st.end_s) + "  " + st.en;
    if (p.march_at_s >= 0.0) s += "\n" + clock(p.march_at_s) + "  «К маршу!» to " + (name ? name(p.to_place) : p.to_place) + ", the head there at " + clock(p.arrive_s);
    s += "\n" + clock(p.ready_s) + "  ready, by the order " + clock(p.ready_by_s);
    if (!p.InTime()) {
        char late[48];
        std::snprintf(late, sizeof(late), ": %.0f min late", p.late_s / 60.0);
        s += late;
    }
    return s;
}

}  // namespace campaign
}  // namespace simcore
