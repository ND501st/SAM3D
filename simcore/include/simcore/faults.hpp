#pragma once

// simcore/faults.hpp — faults that come about by themselves (docs/FAULTS.md; roadmap C4's fault model and G6.1's wear
// clocks and fault chances, in first form).
//
// The textbook (voenizdat_osa_1991 p212-219) says the Soviet system of maintenance was planned and preventive, done "by
// the amount of work" (kilometres, launches, cycles, hours of running) or the calendar, and that the crew inspects its
// vehicle before every fight and march (КО) and services it after (ЕТО). It gives no failure rates, nor does any open
// source found (docs/FAULTS.md): the rates here are game values.
//
// - Each fault of the catalogue (data/gameplay/<system>_faults.json) is one of the console's fault inputs. It ages on
//   its own clock, the hours its circuit runs (`runs_while`), faster in a hot rack (`heat`: twice as fast every
//   `heat_doubles_per_c` over `heat_from_c`) and with the maintenance behind (`maintenance`: kept, due, overdue).
// - Luck: each fault's life (in units of its mean life) is drawn once from the scenario's seed, an exponential draw;
//   the fault comes about when its clock has used it up. A new block (a repair) draws a new life.
// - Faults already there when the session starts: the hours of work since the last check (`since_check_h`) are run on
//   every clock first; a fault whose life they used up is there, unknown, for the function check to find.
// - A crew knows of a fault only by an instrument (`found_by`): while the instrument is set up to show it (the
//   check's switches, a lamp's supply) and the fault is there, the seat that reads it sees it. The runner has the
//   seat report it; nothing else tells the crew.
// - Repairs (`repair`): by the crew from the vehicle's own spares (ЗИП-1А) in a time, or only by the maintenance
//   vehicle's or the workshop's (ЗИП-1Б, ЗИП-2: G6.3, not in a session yet).
// - Random faults can be turned off (the missions menu's Faults, a scenario's "faults": {"random": false}); faults a
//   scenario or the instructor sets come regardless.

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/noise.hpp"

namespace simcore {

// An instrument that shows a fault: who reads it, and while what holds it shows the fault (the check's switches, the
// lamp's supply). It shows it only while the fault is there.
struct FaultFinding {
    std::string seat;
    std::vector<Condition> setup;
    std::string shows_en, shows_ru;  // what he sees ("КЗ stays dark")
    bool in_check = false;           // part of the function check: the check after a repair shows it again
};

struct FaultRepair {
    enum class By { Crew, Workshop };
    By by = By::Workshop;
    std::string seat;   // Crew: who does it
    double time_s = 0.0;
    std::string kit;    // the spares it takes ("ЗИП-1А")
};

struct FaultDef {
    std::string input;              // the console's fault input
    std::string name_en, name_ru;   // as the crew says it
    std::vector<Condition> runs_while;  // its clock runs while these hold
    double mtbf_h = 0.0;            // mean hours on that clock between failures (game value)
    bool heat = false;              // the racks' heat ages it
    std::vector<FaultFinding> found_by;
    FaultRepair repair;
    std::string source, confidence, notes;
};

struct FaultCatalogue {
    std::string system_id;
    std::vector<FaultDef> faults;
    double heat_from_c = 40.0, heat_doubles_per_c = 10.0;
    double kept = 1.0, due = 2.0, overdue = 4.0;  // the maintenance's multiplier on every clock
    double since_check_h = 4.0;                    // hours of work since the last check, unless the scenario says

    const FaultDef* Find(const std::string& input) const {
        for (const FaultDef& f : faults) {
            if (f.input == input) return &f;
        }
        return nullptr;
    }
    double MaintenanceFactor(const std::string& state) const {
        if (state == "due") return due;
        if (state == "overdue") return overdue;
        return kept;
    }
};

// A scenario's "faults": random faults on or off, the maintenance behind the vehicle, the hours since its last check.
struct FaultOptions {
    bool random = true;
    std::string maintenance = "kept";  // "kept" | "due" | "overdue"
    double since_check_h = -1.0;       // < 0: the catalogue's
};

namespace detail {

class FaultsLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, const ConsoleData& console, FaultCatalogue& out) {
        if (!Keys(root, "", {"system_id", "status", "notes", "source", "confidence", "heat", "maintenance", "since_check_h", "faults"})) {
            return false;
        }
        if (!RequireString(root, "", "system_id", out.system_id)) return false;
        if (out.system_id != console.system_id) {
            return Fail("system_id", "'" + out.system_id + "' doesn't match the console's '" + console.system_id + "'");
        }
        if (!OptionalNumber(root, "", "since_check_h", out.since_check_h)) return false;
        if (out.since_check_h < 0.0) return Fail("since_check_h", "must be >= 0");
        if (const json::Value* h = root.Find("heat")) {
            if (!Keys(*h, "heat", {"from_c", "doubles_per_c", "source", "confidence", "notes"}) ||
                !OptionalNumber(*h, "heat", "from_c", out.heat_from_c) ||
                !OptionalNumber(*h, "heat", "doubles_per_c", out.heat_doubles_per_c)) {
                return false;
            }
            if (!(out.heat_doubles_per_c > 0.0)) return Fail("heat.doubles_per_c", "must be > 0");
        }
        if (const json::Value* m = root.Find("maintenance")) {
            if (!Keys(*m, "maintenance", {"kept", "due", "overdue", "source", "confidence", "notes"}) ||
                !OptionalNumber(*m, "maintenance", "kept", out.kept) || !OptionalNumber(*m, "maintenance", "due", out.due) ||
                !OptionalNumber(*m, "maintenance", "overdue", out.overdue)) {
                return false;
            }
            if (!(out.kept > 0.0) || out.due < out.kept || out.overdue < out.due) {
                return Fail("maintenance", "kept > 0 and kept <= due <= overdue");
            }
        }
        const json::Value* arr = OptionalArray(root, "", "faults");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            FaultDef f;
            if (!LoadFault(arr->items[i], Index("faults", i), console, f)) return false;
            if (out.Find(f.input) != nullptr) return Fail(Index("faults", i), "fault '" + f.input + "' twice");
            out.faults.push_back(std::move(f));
        }
        return true;
    }

private:
    bool LoadFault(const json::Value& v, const std::string& p, const ConsoleData& console, FaultDef& f) {
        if (!Keys(v, p, {"input", "name_en", "name_ru", "runs_while", "mtbf_h", "heat", "found_by", "repair", "source",
                         "confidence", "notes"}) ||
            !RequireString(v, p, "input", f.input) || !RequireString(v, p, "name_en", f.name_en) ||
            !RequireString(v, p, "name_ru", f.name_ru) || !OptionalNumber(v, p, "mtbf_h", f.mtbf_h) ||
            !OptionalBool(v, p, "heat", f.heat) || !RequireString(v, p, "source", f.source) ||
            !RequireString(v, p, "confidence", f.confidence) || !OptionalString(v, p, "notes", f.notes) ||
            !LoadConditions(v, p, "runs_while", console.panel, console.equipment, f.runs_while)) {
            return false;
        }
        if (!console.equipment.HasInput(f.input)) return Fail(Join(p, "input"), "unknown input '" + f.input + "'");
        if (f.mtbf_h < 0.0) return Fail(Join(p, "mtbf_h"), "must be >= 0 (0: it never comes by itself)");
        if (f.mtbf_h > 0.0 && f.runs_while.empty()) return Fail(Join(p, "runs_while"), "required with mtbf_h: its clock");
        const json::Value* fb = OptionalArray(v, p, "found_by");
        if (fb == nullptr && !message_.empty()) return false;
        if (fb == nullptr || fb->items.empty()) return Fail(Join(p, "found_by"), "required: the instrument that shows it");
        for (std::size_t i = 0; i < fb->items.size(); ++i) {
            const std::string q = Index(Join(p, "found_by"), i);
            const json::Value& w = fb->items[i];
            FaultFinding ff;
            if (!Keys(w, q, {"seat", "setup", "shows_en", "shows_ru", "in_check", "notes"}) || !RequireString(w, q, "seat", ff.seat) ||
                !RequireString(w, q, "shows_en", ff.shows_en) || !RequireString(w, q, "shows_ru", ff.shows_ru) ||
                !OptionalBool(w, q, "in_check", ff.in_check) ||
                !LoadConditions(w, q, "setup", console.panel, console.equipment, ff.setup)) {
                return false;
            }
            if (!console.stations.empty() && console.FindStation(ff.seat) == nullptr) {
                return Fail(Join(q, "seat"), "'" + ff.seat + "' is not a station of the console");
            }
            if (ff.setup.empty()) return Fail(Join(q, "setup"), "required: what the instrument needs to show it");
            for (const Condition& c : ff.setup) {  // the instrument shows the fault: it never reads the fault itself
                if (c.kind == Condition::Kind::InputIs && c.id == f.input) return Fail(Join(q, "setup"), "must not name the fault's own input");
            }
            f.found_by.push_back(std::move(ff));
        }
        const json::Value* r = v.Find("repair");
        if (r == nullptr) return Fail(Join(p, "repair"), "required");
        const std::string rp = Join(p, "repair");
        std::string by;
        if (!Keys(*r, rp, {"by", "seat", "time_s", "kit", "notes"}) || !RequireString(*r, rp, "by", by) ||
            !OptionalString(*r, rp, "seat", f.repair.seat) || !OptionalNumber(*r, rp, "time_s", f.repair.time_s) ||
            !OptionalString(*r, rp, "kit", f.repair.kit)) {
            return false;
        }
        if (by == "crew") {
            f.repair.by = FaultRepair::By::Crew;
            if (console.FindStation(f.repair.seat) == nullptr) return Fail(Join(rp, "seat"), "a station of the console, who repairs it");
            if (!(f.repair.time_s > 0.0)) return Fail(Join(rp, "time_s"), "must be > 0");
        } else if (by == "workshop") {
            f.repair.by = FaultRepair::By::Workshop;
        } else {
            return Fail(Join(rp, "by"), "\"crew\" or \"workshop\"");
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadFaultCatalogue(const std::string& json_text, const ConsoleData& console, FaultCatalogue& out,
                               std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    FaultCatalogue loaded;
    detail::FaultsLoader loader;
    if (!loader.Load(root, console, loaded)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(loaded);
    return true;
}

inline bool LoadFaultCatalogueFile(const std::string& path, const ConsoleData& console, FaultCatalogue& out,
                                   std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    if (!LoadFaultCatalogue(text.str(), console, out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

// The faults' truth: their clocks, their lives, which have come about. The runner owns one; the crew knows only what
// an instrument shows them (Shows).
class FaultModel {
public:
    void Configure(const FaultCatalogue& catalogue, const FaultOptions& options, std::uint64_t seed) {
        catalogue_ = catalogue;
        configured_ = true;
        options_ = options;
        seed_ = seed;
        state_.assign(catalogue.faults.size(), State{});
        for (std::size_t i = 0; i < state_.size(); ++i) state_[i].life = DrawLife(i);
    }
    bool Configured() const { return configured_; }
    bool Random() const { return options_.random; }
    void SetRandom(bool on) {  // before Start; a life drawn once the dice are on
        options_.random = on;
        for (std::size_t i = 0; i < state_.size(); ++i) state_[i].life = DrawLife(i);
    }
    const FaultOptions& Options() const { return options_; }
    const FaultCatalogue* Catalogue() const { return configured_ ? &catalogue_ : nullptr; }

    // The hours of work since the last check, run on every clock; the faults they bring about are there now (their
    // inputs set). Returns their inputs.
    std::vector<std::string> Start(ConsoleData& console) {
        std::vector<std::string> out;
        if (!configured_) return out;
        const double since = options_.since_check_h >= 0.0 ? options_.since_check_h : catalogue_.since_check_h;
        for (std::size_t i = 0; i < state_.size(); ++i) {
            const FaultDef& f = catalogue_.faults[i];
            if (!(f.mtbf_h > 0.0)) continue;
            state_[i].worked_h += since;
            state_[i].used += since * Maintenance() / f.mtbf_h;
            if (Due(i) && !console.equipment.GetInput(f.input)) {
                console.equipment.SetInput(f.input, true);
                state_[i].came_s = 0.0;
                out.push_back(f.input);
            }
        }
        return out;
    }

    // One step: every clock whose circuit runs ages; a fault whose life is used up comes about. Returns the new ones.
    // rack_c: the equipment racks' temperature (NaN when the vehicle's climate is not simulated).
    std::vector<std::string> Step(double t, double dt, ConsoleData& console, double rack_c) {
        std::vector<std::string> out;
        if (!configured_ || dt <= 0.0) return out;
        const double heat = HeatFactor(rack_c);
        for (std::size_t i = 0; i < state_.size(); ++i) {
            const FaultDef& f = catalogue_.faults[i];
            if (!(f.mtbf_h > 0.0) || !console.equipment.AllHold(f.runs_while, console.panel)) continue;
            const double h = dt / 3600.0;
            state_[i].worked_h += h;
            state_[i].used += h * Maintenance() * (f.heat ? heat : 1.0) / f.mtbf_h;
            if (Due(i) && !console.equipment.GetInput(f.input)) {
                console.equipment.SetInput(f.input, true);
                state_[i].came_s = t;
                out.push_back(f.input);
            }
        }
        return out;
    }

    // Whether this instrument shows its fault now: the fault is there and the instrument is set up to show it.
    static bool Shows(const FaultDef& f, const FaultFinding& w, const ConsoleData& console) {
        return console.equipment.GetInput(f.input) && console.equipment.AllHold(w.setup, console.panel);
    }

    // The block replaced: the fault cleared, a new block's life drawn, its clock from nought.
    void Repaired(const std::string& input, ConsoleData& console) {
        console.equipment.SetInput(input, false);
        if (!configured_) return;
        for (std::size_t i = 0; i < state_.size(); ++i) {
            if (catalogue_.faults[i].input != input) continue;
            State& s = state_[i];
            ++s.blocks;
            s.used = 0.0;
            s.worked_h = 0.0;
            s.came_s = -1.0;
            s.life = DrawLife(i);
        }
    }

    // The hours a fault's circuit has worked since its block was new or last checked (G6's wear clocks).
    double WorkedH(const std::string& input) const {
        const int i = IndexOf(input);
        return i < 0 ? 0.0 : state_[static_cast<std::size_t>(i)].worked_h;
    }
    // The share of its drawn life used up (1: it has come about). For the tests and the debrief.
    double Used(const std::string& input) const {
        const int i = IndexOf(input);
        if (i < 0) return 0.0;
        const State& s = state_[static_cast<std::size_t>(i)];
        return std::isfinite(s.life) && s.life > 0.0 ? s.used / s.life : 0.0;
    }
    // When it came about by itself (s; 0 at the start; -1 not, or set by the scenario).
    double CameAt(const std::string& input) const {
        const int i = IndexOf(input);
        return i < 0 ? -1.0 : state_[static_cast<std::size_t>(i)].came_s;
    }
    double Maintenance() const { return configured_ ? catalogue_.MaintenanceFactor(options_.maintenance) : 1.0; }
    double HeatFactor(double rack_c) const {
        if (!configured_ || !std::isfinite(rack_c) || rack_c <= catalogue_.heat_from_c) return 1.0;
        return std::pow(2.0, (rack_c - catalogue_.heat_from_c) / catalogue_.heat_doubles_per_c);
    }

private:
    struct State {
        double used = 0.0;       // the hazard used up (in mean lives)
        double life = 0.0;       // its drawn life (in mean lives): an exponential draw
        double worked_h = 0.0;
        double came_s = -1.0;
        unsigned blocks = 0;     // blocks replaced: each draws a new life
    };
    FaultCatalogue catalogue_;  // its own copy: the model outlives nothing it points at
    bool configured_ = false;
    FaultOptions options_;
    std::uint64_t seed_ = 0;
    std::vector<State> state_;

    bool Due(std::size_t i) const { return state_[i].used >= state_[i].life; }
    int IndexOf(const std::string& input) const {
        if (!configured_) return -1;
        for (std::size_t i = 0; i < catalogue_.faults.size(); ++i) {
            if (catalogue_.faults[i].input == input) return static_cast<int>(i);
        }
        return -1;
    }
    // An exponential draw from the seed, the fault's place and its block: luck, the same for the same seed.
    double DrawLife(std::size_t i) const {
        if (!options_.random) return std::numeric_limits<double>::infinity();
        const std::uint64_t z = noise::Mix64(seed_ * 0x9E3779B97F4A7C15ull + (static_cast<std::uint64_t>(i) + 1) * 0xBF58476D1CE4E5B9ull +
                                             static_cast<std::uint64_t>(state_[i].blocks) * 0x94D049BB133111EBull + 0xFA17u);
        const double u = (static_cast<double>(z >> 11) + 0.5) * (1.0 / 9007199254740992.0);  // (0, 1)
        return -std::log(u);
    }
};

}  // namespace simcore
