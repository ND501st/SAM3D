// simcore/crew_data.hpp — the crew of a system as data (roadmap 8.1, docs/crew/).
//
// data/crew/<system>_crew.json: the seats (titles, duties, displays), the callout catalogue (orders, acknowledgements,
// reports, warnings, reminders, in Russian and English), the orders (the procedures each starts, or the task it hands
// the drill master), the tasks (named goals as console conditions), the world facts (what makes each simulation input
// true) and the automatic reports (the callout a seat speaks when a condition becomes true).
// data/crew/skill_levels.json: the skill tiers (docs/crew/05_SKILL.md).
//
// Both loaders are strict like the console's: unknown keys, missing sources and references to stations, procedures,
// controls, subsystems or inputs the console doesn't have are refused with the path to the problem.
#pragma once

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"

namespace simcore {

struct CrewSeatDef {
    std::string station;
    int number = 0;  // the seat's number in the drill's reports ("первый готов"); 0 for the commander
    std::string title_ru;
    std::string title_en;
    std::vector<std::string> duties;
    std::vector<std::string> displays;
};

enum class CalloutKind { Order, Ack, Report, Warning, Remind, Radio };

inline const char* ToString(CalloutKind k) {
    switch (k) {
        case CalloutKind::Order: return "order";
        case CalloutKind::Ack: return "ack";
        case CalloutKind::Report: return "report";
        case CalloutKind::Warning: return "warning";
        case CalloutKind::Remind: return "remind";
        case CalloutKind::Radio: return "radio";
    }
    return "?";
}

struct CalloutDef {
    std::string id;
    CalloutKind kind = CalloutKind::Report;
    std::string from;  // a station, "any", "commander", ...
    std::string to;    // a station, "crew", "battery", "any"
    std::string ru;
    std::string en;
    int priority = 1;  // 0 urgent, 1 normal, 2 routine
    bool ack = false;
};

struct OrderStart {
    std::string station;
    std::string procedure;
    bool queue = false;
};

struct OrderDef {
    std::string callout;  // the order's callout id (its words)
    std::string task;     // the task it asks for (may be empty)
    std::vector<OrderStart> procedures;
    // Work the order starts that is not a console drill (ScenarioRunner carries it out; IsOrderAction lists them):
    //   compass_survey  the crew chief finds Qc with the artillery compass at a halt (orientation.hpp)
    std::vector<std::string> actions;
};
inline bool IsOrderAction(const std::string& a) { return a == "compass_survey"; }

// A named goal: conditions on the console to make true (docs/crew/06_REFERENCE_PERFORMER.md).
struct CrewTask {
    std::string id;
    std::string name;
    std::vector<Condition> conditions;
    std::vector<Condition> prefer;  // the doctrine's way where the data allows several (К бою starts the turbine)
};

// What makes a simulation input true: who sets it, which seats' work and which conditions it needs.
struct CrewFact {
    std::string input;
    std::string set_by;
    std::vector<std::string> seats;
    std::vector<Condition> needs;
    std::string knowledge;
    bool by_action = false;  // one of `seats` makes it true by an action of his own (the driver halts)
};

// A callout a seat speaks by itself when `when` becomes true.
struct AutoReportDef {
    std::string callout;
    std::string from;
    Condition when;
};

struct CrewData {
    std::string system_id;
    std::vector<CrewSeatDef> seats;
    std::vector<CalloutDef> callouts;
    std::vector<OrderDef> orders;
    std::vector<CrewTask> tasks;
    std::vector<CrewFact> facts;
    std::vector<AutoReportDef> reports;
    std::vector<std::string> decision_controls;  // steps that are decisions (ПУСК): judgement delays them

    const CalloutDef* FindCallout(const std::string& id) const {
        for (const CalloutDef& c : callouts) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }
    const OrderDef* FindOrder(const std::string& callout) const {
        for (const OrderDef& o : orders) {
            if (o.callout == callout) return &o;
        }
        return nullptr;
    }
    const CrewTask* FindTask(const std::string& id) const {
        for (const CrewTask& t : tasks) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }
    const CrewFact* FindFact(const std::string& input) const {
        for (const CrewFact& f : facts) {
            if (f.input == input) return &f;
        }
        return nullptr;
    }
    const CrewSeatDef* FindSeat(const std::string& station) const {
        for (const CrewSeatDef& s : seats) {
            if (s.station == station) return &s;
        }
        return nullptr;
    }
};

// One skill tier (docs/crew/05_SKILL.md). Every behaviour of an NPC reads these.
struct SkillTier {
    std::string id;
    std::string name_ru;
    std::string name_en;
    double perception_s = 1.0;
    double miss_chance = 0.0;
    double reaction_s = 1.0;
    double reaction_jitter_s = 0.0;
    double wheel_rate_factor = 1.0;
    double tv_lag_s = 0.4;
    double aim_noise_deg = 0.0;
    double range_error_m = 0.0;
    double azimuth_error_deg = 0.0;
    double slip_chance = 0.0;
    double omission_chance = 0.0;
    std::string knows = "all";  // "basic": the standard drills only; "all": jamming, EMCON and faults too
    double launch_timing_s = 0.0;
    double priority_error_chance = 0.0;
    double branch_error_chance = 0.0;
    double callout_delay_s = 0.0;
    double ack_discipline = 1.0;
    double stress_factor = 1.0;
    double patience_s = 5.0;
};

// What a crew member's drill work takes from a tier (procedures.hpp).
inline CrewSkill ToCrewSkill(const SkillTier& t) {
    CrewSkill s;
    s.tier = t.id;
    s.reaction_s = t.reaction_s;
    s.jitter_s = t.reaction_jitter_s;
    s.slip_chance = t.slip_chance;
    s.omission_chance = t.omission_chance;
    s.patience_s = t.patience_s;
    s.stress_factor = t.stress_factor;
    s.decision_delay_s = t.launch_timing_s;
    return s;
}

struct SkillTable {
    std::vector<SkillTier> tiers;
    std::string reference_tier;  // the drill master's tier

    const SkillTier* Find(const std::string& id) const {
        for (const SkillTier& t : tiers) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }
    const SkillTier* Reference() const { return Find(reference_tier); }
};

namespace detail {

class CrewLoader : public ConsoleLoader {
public:
    bool LoadCrew(const json::Value& root, const ConsoleData& console, CrewData& out) {
        if (!Keys(root, "", {"system_id", "display_name", "status", "notes", "source", "confidence", "seats", "callouts", "orders",
                             "tasks", "facts", "reports", "decisions"}) ||
            !RequireString(root, "", "system_id", out.system_id) || !Provenance(root, "")) {
            return false;
        }
        if (out.system_id != console.system_id) {
            return Fail("system_id", "'" + out.system_id + "' is not the console's '" + console.system_id + "'");
        }
        return LoadSeats(root, console, out) && LoadCallouts(root, console, out) && LoadOrders(root, console, out) &&
               LoadTasks(root, console, out) && LoadFacts(root, console, out) && LoadReports(root, console, out) &&
               LoadDecisions(root, console, out);
    }

    bool LoadSkills(const json::Value& root, SkillTable& out) {
        if (!Keys(root, "", {"skill_id", "display_name", "status", "source", "confidence", "notes", "reference_tier", "tiers",
                             "calibration"}) ||
            !Provenance(root, "") || !RequireString(root, "", "reference_tier", out.reference_tier)) {
            return false;
        }
        const json::Value* arr = OptionalArray(root, "", "tiers");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("tiers", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& t = arr->items[i];
            const std::string p = Index("tiers", i);
            if (!Keys(t, p, {"id", "name_ru", "name_en", "perception_s", "miss_chance", "reaction_s", "reaction_jitter_s",
                             "wheel_rate_factor", "tv_lag_s", "aim_noise_deg", "range_error_m", "azimuth_error_deg",
                             "slip_chance", "omission_chance", "knows", "launch_timing_s", "priority_error_chance",
                             "branch_error_chance", "callout_delay_s", "ack_discipline", "stress_factor", "patience_s", "notes"})) {
                return false;
            }
            SkillTier s;
            if (!RequireString(t, p, "id", s.id) || !OptionalString(t, p, "name_ru", s.name_ru) ||
                !OptionalString(t, p, "name_en", s.name_en) || !OptionalString(t, p, "knows", s.knows)) {
                return false;
            }
            struct Field {
                const char* key;
                double* value;
                double low;
                double high;
            };
            const Field fields[] = {{"perception_s", &s.perception_s, 0.0, 60.0},
                                    {"miss_chance", &s.miss_chance, 0.0, 1.0},
                                    {"reaction_s", &s.reaction_s, 0.0, 60.0},
                                    {"reaction_jitter_s", &s.reaction_jitter_s, 0.0, 60.0},
                                    {"wheel_rate_factor", &s.wheel_rate_factor, 0.01, 5.0},
                                    {"tv_lag_s", &s.tv_lag_s, 0.0, 10.0},
                                    {"aim_noise_deg", &s.aim_noise_deg, 0.0, 10.0},
                                    {"range_error_m", &s.range_error_m, 0.0, 10000.0},
                                    {"azimuth_error_deg", &s.azimuth_error_deg, 0.0, 30.0},
                                    {"slip_chance", &s.slip_chance, 0.0, 1.0},
                                    {"omission_chance", &s.omission_chance, 0.0, 1.0},
                                    {"launch_timing_s", &s.launch_timing_s, 0.0, 120.0},
                                    {"priority_error_chance", &s.priority_error_chance, 0.0, 1.0},
                                    {"branch_error_chance", &s.branch_error_chance, 0.0, 1.0},
                                    {"callout_delay_s", &s.callout_delay_s, 0.0, 60.0},
                                    {"ack_discipline", &s.ack_discipline, 0.0, 1.0},
                                    {"stress_factor", &s.stress_factor, 1.0, 10.0},
                                    {"patience_s", &s.patience_s, 0.0, 600.0}};
            for (const Field& f : fields) {
                if (t.Find(f.key) == nullptr) return Fail(Join(p, f.key), "required");
                if (!OptionalNumber(t, p, f.key, *f.value)) return false;
                if (*f.value < f.low || *f.value > f.high) {
                    return Fail(Join(p, f.key), "must be from " + Num(f.low) + " to " + Num(f.high));
                }
            }
            if (s.knows != "basic" && s.knows != "all") return Fail(Join(p, "knows"), "must be basic or all");
            for (const SkillTier& other : out.tiers) {
                if (other.id == s.id) return Fail(Join(p, "id"), "duplicate tier '" + s.id + "'");
            }
            out.tiers.push_back(std::move(s));
        }
        if (out.Find(out.reference_tier) == nullptr) return Fail("reference_tier", "unknown tier '" + out.reference_tier + "'");
        return true;
    }

private:
    static std::string Num(double v) {
        std::ostringstream ss;
        ss << v;
        return ss.str();
    }

    bool KnownSeat(const ConsoleData& console, const std::string& s, bool allow_groups) const {
        if (console.FindStation(s) != nullptr) return true;
        return allow_groups && (s == "any" || s == "crew" || s == "battery");
    }

    bool StringList(const json::Value& obj, const std::string& path, const char* key, std::vector<std::string>& out) {
        const json::Value* arr = OptionalArray(obj, path, key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            if (!arr->items[i].Is(json::Type::String)) return Fail(Index(Join(path, key), i), "expected a string");
            out.push_back(arr->items[i].string);
        }
        return true;
    }

    bool LoadSeats(const json::Value& root, const ConsoleData& console, CrewData& out) {
        const json::Value* arr = OptionalArray(root, "", "seats");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& s = arr->items[i];
            const std::string p = Index("seats", i);
            CrewSeatDef seat;
            if (!Keys(s, p, {"station", "number", "title_ru", "title_en", "duties", "displays", "source", "confidence", "notes"}) ||
                !RequireString(s, p, "station", seat.station) || !OptionalInt(s, p, "number", seat.number) ||
                !OptionalString(s, p, "title_ru", seat.title_ru) || !OptionalString(s, p, "title_en", seat.title_en) ||
                !StringList(s, p, "duties", seat.duties) || !StringList(s, p, "displays", seat.displays) || !Provenance(s, p)) {
                return false;
            }
            if (!KnownSeat(console, seat.station, false)) return Fail(Join(p, "station"), "unknown station '" + seat.station + "'");
            out.seats.push_back(std::move(seat));
        }
        for (const ConsoleStation& st : console.stations) {
            if (out.FindSeat(st.id) == nullptr) return Fail("seats", "the console's station '" + st.id + "' has no seat");
        }
        return true;
    }

    bool LoadCallouts(const json::Value& root, const ConsoleData& console, CrewData& out) {
        const json::Value* arr = OptionalArray(root, "", "callouts");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& c = arr->items[i];
            const std::string p = Index("callouts", i);
            CalloutDef d;
            std::string kind;
            if (!Keys(c, p, {"id", "kind", "from", "to", "ru", "en", "priority", "ack", "source", "confidence", "notes"}) ||
                !RequireString(c, p, "id", d.id) || !RequireString(c, p, "kind", kind) || !RequireString(c, p, "from", d.from) ||
                !RequireString(c, p, "to", d.to) || !RequireString(c, p, "ru", d.ru) || !RequireString(c, p, "en", d.en) ||
                !OptionalInt(c, p, "priority", d.priority) || !OptionalBool(c, p, "ack", d.ack) || !Provenance(c, p)) {
                return false;
            }
            if (kind == "order") d.kind = CalloutKind::Order;
            else if (kind == "ack") d.kind = CalloutKind::Ack;
            else if (kind == "report") d.kind = CalloutKind::Report;
            else if (kind == "warning") d.kind = CalloutKind::Warning;
            else if (kind == "remind") d.kind = CalloutKind::Remind;
            else if (kind == "radio") d.kind = CalloutKind::Radio;
            else return Fail(Join(p, "kind"), "must be order, ack, report, warning, remind or radio");
            if (d.priority < 0 || d.priority > 2) return Fail(Join(p, "priority"), "must be 0, 1 or 2");
            if (!KnownSeat(console, d.from, true)) return Fail(Join(p, "from"), "unknown station '" + d.from + "'");
            if (!KnownSeat(console, d.to, true)) return Fail(Join(p, "to"), "unknown station '" + d.to + "'");
            if (out.FindCallout(d.id) != nullptr) return Fail(Join(p, "id"), "duplicate callout '" + d.id + "'");
            out.callouts.push_back(std::move(d));
        }
        return true;
    }

    bool LoadOrders(const json::Value& root, const ConsoleData& console, CrewData& out) {
        const json::Value* arr = OptionalArray(root, "", "orders");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& o = arr->items[i];
            const std::string p = Index("orders", i);
            OrderDef d;
            if (!Keys(o, p, {"callout", "task", "procedures", "actions", "source", "confidence", "notes"}) ||
                !RequireString(o, p, "callout", d.callout) || !OptionalString(o, p, "task", d.task) || !Provenance(o, p)) {
                return false;
            }
            const CalloutDef* c = out.FindCallout(d.callout);
            if (c == nullptr || c->kind != CalloutKind::Order) return Fail(Join(p, "callout"), "'" + d.callout + "' is not an order callout");
            const json::Value* ps = OptionalArray(o, p, "procedures");
            if (ps != nullptr) {
                for (std::size_t j = 0; j < ps->items.size(); ++j) {
                    const json::Value& s = ps->items[j];
                    const std::string sp = Index(Join(p, "procedures"), j);
                    OrderStart st;
                    if (!Keys(s, sp, {"station", "procedure", "queue"}) || !RequireString(s, sp, "station", st.station) ||
                        !RequireString(s, sp, "procedure", st.procedure) || !OptionalBool(s, sp, "queue", st.queue)) {
                        return false;
                    }
                    const Procedure* pr = console.FindProcedure(st.procedure);
                    if (pr == nullptr) return Fail(Join(sp, "procedure"), "unknown procedure '" + st.procedure + "'");
                    if (pr->station != st.station) {
                        return Fail(Join(sp, "station"), "procedure '" + st.procedure + "' is the " + pr->station + "'s, not the " + st.station + "'s");
                    }
                    d.procedures.push_back(std::move(st));
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (!StringList(o, p, "actions", d.actions)) return false;
            for (std::size_t j = 0; j < d.actions.size(); ++j) {
                if (!IsOrderAction(d.actions[j])) return Fail(Index(Join(p, "actions"), j), "unknown action '" + d.actions[j] + "'");
            }
            out.orders.push_back(std::move(d));
        }
        return true;
    }

    bool LoadTasks(const json::Value& root, const ConsoleData& console, CrewData& out) {
        const json::Value* arr = OptionalArray(root, "", "tasks");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& t = arr->items[i];
            const std::string p = Index("tasks", i);
            CrewTask d;
            if (!Keys(t, p, {"id", "name", "conditions", "prefer", "source", "confidence", "notes"}) || !RequireString(t, p, "id", d.id) ||
                !RequireString(t, p, "name", d.name) || !Provenance(t, p) ||
                !LoadConditions(t, p, "conditions", console.panel, console.equipment, d.conditions) ||
                !LoadConditions(t, p, "prefer", console.panel, console.equipment, d.prefer)) {
                return false;
            }
            if (d.conditions.empty()) return Fail(Join(p, "conditions"), "required, a non-empty list");
            if (out.FindTask(d.id) != nullptr) return Fail(Join(p, "id"), "duplicate task '" + d.id + "'");
            out.tasks.push_back(std::move(d));
        }
        for (std::size_t i = 0; i < out.orders.size(); ++i) {
            const std::string& task = out.orders[i].task;
            if (!task.empty() && out.FindTask(task) == nullptr) return Fail(Join(Index("orders", i), "task"), "unknown task '" + task + "'");
        }
        return true;
    }

    bool LoadFacts(const json::Value& root, const ConsoleData& console, CrewData& out) {
        const json::Value* arr = OptionalArray(root, "", "facts");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& f = arr->items[i];
            const std::string p = Index("facts", i);
            CrewFact d;
            if (!Keys(f, p, {"input", "set_by", "seats", "needs", "knowledge", "by_action", "source", "confidence", "notes"}) ||
                !RequireString(f, p, "input", d.input) || !RequireString(f, p, "set_by", d.set_by) || !OptionalBool(f, p, "by_action", d.by_action) ||
                !StringList(f, p, "seats", d.seats) || !OptionalString(f, p, "knowledge", d.knowledge) || !Provenance(f, p) ||
                !LoadConditions(f, p, "needs", console.panel, console.equipment, d.needs)) {
                return false;
            }
            if (!console.equipment.HasInput(d.input)) return Fail(Join(p, "input"), "unknown input '" + d.input + "'");
            for (std::size_t j = 0; j < d.seats.size(); ++j) {
                if (!KnownSeat(console, d.seats[j], false)) return Fail(Index(Join(p, "seats"), j), "unknown station '" + d.seats[j] + "'");
            }
            if (out.FindFact(d.input) != nullptr) return Fail(Join(p, "input"), "duplicate fact for '" + d.input + "'");
            if (d.by_action && d.seats.size() != 1) return Fail(Join(p, "by_action"), "needs exactly one seat");
            out.facts.push_back(std::move(d));
        }
        return true;
    }

    // "decisions": { "controls": [...], "source", "confidence" }: the controls whose steps are a decision.
    bool LoadDecisions(const json::Value& root, const ConsoleData& console, CrewData& out) {
        const json::Value* d = root.Find("decisions");
        if (d == nullptr) return true;
        if (!Keys(*d, "decisions", {"controls", "source", "confidence", "notes"}) || !Provenance(*d, "decisions") ||
            !StringList(*d, "decisions", "controls", out.decision_controls)) {
            return false;
        }
        for (std::size_t i = 0; i < out.decision_controls.size(); ++i) {
            if (!console.panel.Has(out.decision_controls[i])) {
                return Fail(Index("decisions.controls", i), "unknown control '" + out.decision_controls[i] + "'");
            }
        }
        return true;
    }

    bool LoadReports(const json::Value& root, const ConsoleData& console, CrewData& out) {
        const json::Value* arr = OptionalArray(root, "", "reports");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& r = arr->items[i];
            const std::string p = Index("reports", i);
            AutoReportDef d;
            const json::Value* when = r.Find("when");
            if (!Keys(r, p, {"callout", "from", "when", "source", "confidence", "notes"}) || !RequireString(r, p, "callout", d.callout) ||
                !RequireString(r, p, "from", d.from) || !Provenance(r, p)) {
                return false;
            }
            if (when == nullptr) return Fail(Join(p, "when"), "required");
            std::vector<Condition> one;
            json::Value wrapper;
            wrapper.type = json::Type::Object;
            wrapper.keys.push_back("when");
            json::Value list;
            list.type = json::Type::Array;
            list.items.push_back(*when);
            wrapper.items.push_back(list);
            if (!LoadConditions(wrapper, p, "when", console.panel, console.equipment, one)) return false;
            d.when = one.front();
            if (out.FindCallout(d.callout) == nullptr) return Fail(Join(p, "callout"), "unknown callout '" + d.callout + "'");
            if (!KnownSeat(console, d.from, false)) return Fail(Join(p, "from"), "unknown station '" + d.from + "'");
            out.reports.push_back(std::move(d));
        }
        return true;
    }
};

inline bool ReadTextFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream text;
    text << file.rdbuf();
    out = text.str();
    return true;
}

}  // namespace detail

inline bool LoadCrewData(const std::string& json_text, const ConsoleData& console, CrewData& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    CrewData d;
    detail::CrewLoader loader;
    if (!loader.LoadCrew(root, console, d)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(d);
    return true;
}

inline bool LoadCrewDataFile(const std::string& path, const ConsoleData& console, CrewData& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(path, text)) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    if (!LoadCrewData(text, console, out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

inline bool LoadSkillTable(const std::string& json_text, SkillTable& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    SkillTable t;
    detail::CrewLoader loader;
    if (!loader.LoadSkills(root, t)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(t);
    return true;
}

inline bool LoadSkillTableFile(const std::string& path, SkillTable& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(path, text)) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    if (!LoadSkillTable(text, out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

}  // namespace simcore
