// simcore/ground_war/ground_war.hpp — the ground war's clock and its line (track F, F3.3; docs/fronts/03_GROUND_WAR.md
// sections 2, 3.3, 4 and 12, docs/fronts/README.md "Efficiency").
//
// GroundWar holds both sides' formations at U0 (the enemy's divisions, brigades and regiments its plan names, ours by the
// deployment against this adversary), each with its equipment by type summed over its tree, and the enemy plan's axes.
// AirCampaign steps it with the campaign's hour (StepTo, after the hour's air results and before the next plan reads the
// line); it moves the line of contact's stations (airwar::LineOfContact::SetStationOffset) from its axes' penetrations,
// so the air war's distances to the front, the air front's bands and their readers follow the line.
//
// What is stepped and what is not:
//   * an event queue keyed by time (a binary heap on a vector sized at Build) holds what happens at a set hour: an
//     offensive's start, a second echelon's commitment, a river crossed, the day's end; a quiet hour costs a look at its
//     top;
//   * each hour, only the axes attacking are resolved, one F3.2 ResolveSector each: both sides' power from their
//     formations' strength, the defender's posture (the security zone's covering share in delay, then the main
//     defensive zone prepared), the ground, the night, the weather and a river line; losses and the advance applied;
//   * everything else is closed form: a formation out of the fight takes its rest rate (FM 101-10-1's "inactive" in
//     contact, "reserve" out of it) over the hours since it was last touched (LossOverHours), when it is next touched;
//   * an axis's ground and rivers are sampled along its route once, at Build (CountryMap lookups never run per hour);
//   * no allocation in the hourly step once built: the vectors are sized at Build, the log's capacity reserved.
// Two phases an hour (every axis's power read, then every loss applied by formation in a fixed order), so the order the
// axes are resolved in changes nothing; every draw is a fork keyed by (sector, day) or by the formation.
//
// Who attacks until F3.4's commanders decide (03 section 5 replaces it): each axis of the enemy's plan with its first
// echelon from the plan's start (start_after_h), its second echelon `clock.second_echelon_after_h` later if the axis has
// advanced (committed to success; at once if the first is spent), to its objective or the plan's political depth; an axis
// halts there, when its attackers are spent, or when it has not advanced for `clock.stall_halt_h` with no echelon left.
// A formation is spent below its doctrine's ineffective line (the Pact and ours) or past its breakpoint drawn per
// formation (NATO and the small forces) and leaves the fight. Our side holds: the first echelon and flank screens in
// their sectors, the security zone's covering share (`clock.security_zone_share`) delaying until the enemy is through it,
// the second echelon of a sector once the enemy reaches the main defensive zone; no withdrawal and no counterattack. The
// rebels' raids are F3.4's (their line holds, quiet).
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/front_line.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/country_map.hpp"
#include "simcore/ground_war/combat.hpp"
#include "simcore/ground_war/ground_data.hpp"
#include "simcore/ground_war/units.hpp"

namespace simcore {
namespace groundwar {

// ------------------------------------------------------------------------------------------------------------ the data

// What a campaign's ground war is built from: the tables, the enemy's forces and plan, our defence (loaded once a
// campaign, shared by its copies).
struct GroundWarData {
    std::string faction;
    GroundTables tables;
    GroundForceDef enemy;
    GroundDefenceDef ours;
};

// The ground files exist for a faction (a faction without data/air_war/ground_<faction>.json keeps its front static).
inline bool HasGroundData(const std::string& data_dir, const std::string& faction) {
    return std::ifstream(data_dir + "/air_war/ground_" + faction + ".json").good();
}

inline bool LoadGroundWarData(const std::string& data_dir, const std::string& faction, GroundWarData& out, std::string* error = nullptr) {
    out.faction = faction;
    if (!LoadGroundTables(data_dir, out.tables, error) || !LoadGroundForce(data_dir, faction, out.enemy, error) ||
        !LoadGroundDefence(data_dir, out.ours, error)) {
        return false;
    }
    if (out.ours.For(faction) == nullptr) {
        if (error != nullptr) *error = "campaign/ground_defence.json: no deployment against '" + faction + "'";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------- the state

// One equipment type a formation holds (only the types the weights count), its count kept as a real number.
struct Holding {
    std::size_t weight = 0;  // index into GroundTables::weights
    double count = 0.0;
    bool tank = false;
};

// A formation at U0: the actor, with its equipment summed over its tree (F3.2's GroundUnit for its strength and state).
struct Formation {
    GroundUnit unit;
    std::string name;
    std::string kind;        // the order of battle's: division, brigade, regiment...
    bool spent = false;      // ineffective or past its breakpoint: out of the fight (F3.4 re-forms it)
    // The enemy's: its axis (-1 none) and echelon; a line formation's sector (the raids' lines, by its garrison).
    int axis = -1;
    bool second = false;
    // Ours: its role and run of sectors (-1 off the line) and along the line; thin when wider than its doctrine's frontage.
    DeployRole role = DeployRole::Reserve;
    int first_sector = -1;
    int last_sector = -1;
    bool thin = false;
    double breakpoint = 1.0;  // cumulative losses at which it is spent (Breakpoints doctrine), else 1
    std::vector<Holding> holdings;
    double personnel = 0.0;
    // The closed form: the rest rate's action and the hour it was last applied to.
    LossAction rest = LossAction::Reserve;
    double settled_h = 0.0;
    // Tank losses by FM 101-10-1 table 7-7 (ours, while delaying or defending): the commitment, from when, on what.
    int commitment = -1;          // a TankCommitment, -1 none
    double commit_h = 0.0;
    double tanks_at_commit = 0.0;
    int zone = -1;                // 0 the security zone, 1 the main defensive zone, -1 not engaged
    double tanks_lost = 0.0;
    double tanks_reparable = 0.0;  // of those lost, the table's reparable share (F3.4 or later returns them)
    double contact_from_h = -1.0;  // ours: when it was first engaged (its days in contact)

    double Tanks() const {
        double n = 0.0;
        for (const Holding& h : holdings) n += h.tank ? h.count : 0.0;
        return n;
    }
    bool InSector(int s) const { return first_sector >= 0 && s >= first_sector && s <= last_sector; }
};

struct RiverOnAxis {
    double depth_m = 0.0;  // where the axis's route crosses it, beyond the base line
    std::string river;
    int bridge_object = -1;  // the air war's bridge there (theatre object index), or -1
    bool road_bridge = false;  // the map has a road crossing there (intact unless an air war object says otherwise)
};

enum class AxisStatus : std::uint8_t { Waiting, Attacking, Halted };
enum class HaltReason : std::uint8_t { None, Objective, Spent, Stalled };

inline const char* HaltReasonName(HaltReason r) {
    switch (r) {
        case HaltReason::None: return "none";
        case HaltReason::Objective: return "objective";
        case HaltReason::Spent: return "spent";
        case HaltReason::Stalled: return "stalled";
    }
    return "?";
}

struct Axis {
    std::string id;
    std::string name;
    int sector = -1;
    double along_m = 0.0;      // the penetration's middle along the base line (its border crossing's projection)
    double objective_m = 0.0;  // how deep its objective is (or the plan's political depth)
    double depth_m = 0.0;      // how far it has pushed the line in
    double half_width_m = 4000.0;
    double taper_m = 12500.0;
    AxisStatus status = AxisStatus::Waiting;
    HaltReason halted = HaltReason::None;
    double start_h = 0.0;
    double last_advance_h = 0.0;
    bool second_committed = false;
    bool second_held = false;  // its axis had not moved when the second echelon was due: held back (F3.4 sends it elsewhere)
    std::vector<int> first;   // formation indices
    std::vector<int> second;
    std::vector<int> defenders;  // ours on the line in its sector (first echelon, flank screens)
    std::vector<int> depth_defenders;  // ours of the sector's second echelon, engaged at the main defensive zone
    std::vector<Terrain> ground;  // the defender's ground by depth, every clock.terrain_step_km
    std::vector<RiverOnAxis> rivers;
    std::size_t next_river = 0;
    double crossing_until_h = -1.0;  // crossing its next river until then (-1: not crossing)
    int draw_day = -1000000;
    double draw = 1.0;
    double last_ratio = 0.0;
    Opposition last_opposition = Opposition::Stalled;
};

// What the ground war says happened, for the campaign's journal (never read by the simulation).
enum class GroundNote : std::uint8_t { AxisStarts, SecondEchelon, RiverReached, RiverCrossed, Spent, AxisHalted, DayLine };
struct GroundLogEntry {
    double h = 0.0;
    GroundNote what = GroundNote::AxisStarts;
    int axis = -1;
    int formation = -1;
    int sector = -1;
    double value = 0.0;
    HaltReason reason = HaltReason::None;
    bool intact = false;  // RiverReached: the bridge was standing
};

// Where the hour goes, by part (docs/fronts/README.md's budget lines), when profiling: steady_clock, summed.
struct GroundProfile {
    enum Part : int { Combat, Decisions, Movement, Line, Picture, Reports, kParts };
    std::uint64_t ns[kParts] = {};
    std::uint64_t hours = 0;
    std::vector<double> day_ms;  // the whole step's ms by campaign day (0 the war's first)
    static const char* Name(int p) {
        static const char* const kNames[kParts] = {"combat", "decisions", "movement", "line", "picture", "reports"};
        return p >= 0 && p < kParts ? kNames[p] : "?";
    }
    double Ms(int p) const { return static_cast<double>(ns[p]) * 1e-6; }
};

// What the campaign lends the hour: the day part, the weather, the bridges.
struct GroundEnv {
    const airwar::TheatreDef* theatre = nullptr;
    const std::vector<airwar::Weather>* weather = nullptr;     // by campaign day from 1 (the campaign's weather_)
    const std::vector<airwar::ObjectState>* truth = nullptr;  // parallel to theatre->objects
};

// ------------------------------------------------------------------------------------------------------- the ground war

class GroundWar {
public:
    // Builds both sides at U0 on the campaign's line (its sectors by id), samples each axis's ground and rivers on the
    // map (`country`, may be null: open ground, no rivers) and queues the plan's start. False with a message when the
    // data name a sector the line lacks.
    bool Build(std::shared_ptr<const GroundWarData> data, const airwar::LineOfContact& line, const airwar::TheatreDef& theatre,
               const CountryMap* country, unsigned seed, std::string* error = nullptr) {
        *this = GroundWar();
        data_ = std::move(data);
        if (!data_) return Fail(error, "no ground data");
        const GroundWarData& d = *data_;
        const GroundTables& t = d.tables;
        root_ = airwar::Rng(static_cast<std::uint64_t>(seed) * 0xA24BAED4963EE407ull + 0x67A0E5ull);
        start_hour_ = theatre.start_hour;
        const DeploymentDef* dep = d.ours.For(d.faction);
        if (dep == nullptr) return Fail(error, "no deployment of ours against " + d.faction);
        security_zone_m_ = dep->security_zone_km * 1000.0;
        // The enemy's formations: the units its plan names.
        const GroundPlanDef& plan = d.enemy.plan;
        for (std::size_t a = 0; a < plan.axes.size(); ++a) {
            for (const std::string& id : plan.axes[a].first_echelon) AddEnemy(id);
            for (const std::string& id : plan.axes[a].second_echelon) AddEnemy(id);
        }
        for (const std::string& id : plan.reserve) AddEnemy(id);
        if (plan.has_raids) {
            for (const std::string& id : plan.raids.lines) AddEnemy(id);
        }
        // Ours: the deployment against this adversary.
        for (const DeployedUnitDef& u : dep->units) {
            const int fi = AddFormation(d.ours.oob, u.unit, Side::Ours);
            if (fi < 0) return Fail(error, "our deployment names '" + u.unit + "', not in " + d.ours.oob_file);
            Formation& f = forms_[static_cast<std::size_t>(fi)];
            f.role = u.role;
            if (!u.sectors.empty()) {
                f.first_sector = line.SectorIndex(u.sectors.front());
                f.last_sector = line.SectorIndex(u.sectors.back());
                if (f.first_sector < 0 || f.last_sector < 0) return Fail(error, "our " + u.unit + "'s sectors are not the line's");
                const double width_km = (line.Sector(static_cast<std::size_t>(f.last_sector)).to_m - line.Sector(static_cast<std::size_t>(f.first_sector)).from_m) / 1000.0;
                const FrontageDef* fr = d.ours.doctrine.Frontage(f.kind, "defence");
                f.thin = fr != nullptr && fr->width_km.hi > 0.0 && width_km > fr->width_km.hi;
            }
        }
        // Each formation's tree, its equipment and people summed, stopping at another actor's tree.
        for (std::size_t i = 0; i < forms_.size(); ++i) Equip(i);
        // Breakpoints drawn per formation (NATO, the small forces); the Pact and ours go by their lines.
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Formation& f = forms_[i];
            const EffectivenessDef& e = Doctrine(f.unit.side).effectiveness;
            if (e.kind != EffectivenessKind::Breakpoints) continue;
            airwar::Rng r = root_.Fork(0xB4EA0000ull + i);
            f.breakpoint = DrawBreakpoint(e, f.unit.side == Side::Enemy && f.axis >= 0, r.Uniform(), 0.0, t.trait_shift);
        }
        // The axes.
        axes_.resize(plan.axes.size());
        for (std::size_t a = 0; a < plan.axes.size(); ++a) {
            const AxisDef& def = plan.axes[a];
            Axis& x = axes_[a];
            x.id = def.id;
            x.name = def.name;
            x.sector = line.SectorIndex(def.sector);
            if (x.sector < 0) return Fail(error, "the axis " + def.id + "'s sector '" + def.sector + "' is not the line's");
            x.half_width_m = 500.0 * t.breakthrough_width_km.Mid();
            x.taper_m = 1000.0 * t.taper_km.Mid();
            // Its border crossing: the route's place nearest the base line; the penetration is drawn there.
            std::size_t cross = 0;
            double best = 1e18;
            for (std::size_t k = 0; k < def.route.size(); ++k) {
                const double dd = line.Place(def.route[k].pos).distance_m;
                if (dd < best) {
                    best = dd;
                    cross = k;
                }
            }
            x.along_m = def.route.empty() ? line.Sector(static_cast<std::size_t>(x.sector)).from_m : line.Place(def.route[cross].pos).along_m;
            const ObjectiveDef* obj = plan.Objective(def.objective);
            double objective_m = 0.0;
            if (obj != nullptr) {
                const airwar::FrontPlace pl = line.Place(obj->place.pos);
                objective_m = pl.side * pl.distance_m;
            }
            if (plan.max_depth_km > 0.0) objective_m = objective_m > 0.0 ? std::min(objective_m, plan.max_depth_km * 1000.0) : plan.max_depth_km * 1000.0;
            x.objective_m = std::max(objective_m, 1000.0);
            for (std::size_t i = 0; i < forms_.size(); ++i) {
                Formation& f = forms_[i];
                if (f.unit.side == Side::Enemy) {
                    if (std::find(def.first_echelon.begin(), def.first_echelon.end(), f.unit.id) != def.first_echelon.end()) {
                        f.axis = static_cast<int>(a);
                        x.first.push_back(static_cast<int>(i));
                    } else if (std::find(def.second_echelon.begin(), def.second_echelon.end(), f.unit.id) != def.second_echelon.end()) {
                        f.axis = static_cast<int>(a);
                        f.second = true;
                        x.second.push_back(static_cast<int>(i));
                    }
                } else if (f.InSector(x.sector)) {
                    if (f.role == DeployRole::FirstEchelon || f.role == DeployRole::FlankScreen) x.defenders.push_back(static_cast<int>(i));
                    if (f.role == DeployRole::SecondEchelon) x.depth_defenders.push_back(static_cast<int>(i));
                }
            }
            SampleAxis(x, def, line, theatre, country);
        }
        // The enemy's line formations off the axes (the raids' lines) hold the sector their garrison projects into.
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Formation& f = forms_[i];
            if (f.unit.side != Side::Enemy || f.axis >= 0 || !plan.has_raids) continue;
            if (std::find(plan.raids.lines.begin(), plan.raids.lines.end(), f.unit.id) == plan.raids.lines.end()) continue;
            const GroundPlace* g = GarrisonOf(f.unit.id);
            if (g != nullptr && !g->at.empty()) f.first_sector = f.last_sector = line.SectorOf(g->pos);
        }
        // Defenders of two axes at once share themselves between them (two phases an hour: order-free).
        shares_.assign(forms_.size(), 0.0);
        loss_.assign(forms_.size() * (axes_.empty() ? 1 : axes_.size()), 0.0);
        results_.assign(axes_.size(), SectorResult());
        engaged_.assign(forms_.size() * (axes_.empty() ? 1 : axes_.size()), 0);
        moved_.assign(axes_.size(), 0);
        log_.reserve(256);
        queue_.reserve(16 + 4 * axes_.size());
        // The plan's start; the days' ends.
        const double start = std::max(0.0, plan.start_after_h);
        Push(start, EventKind::Start, -1);
        Push(NextMidnight(0.0), EventKind::DayEnd, -1);
        built_ = true;
        return true;
    }

    bool Built() const { return built_; }

    // Steps whole hours up to t1 (an hour's events at its start, then its combat), moving `line`'s stations.
    void Step(double t1, const GroundEnv& env, airwar::LineOfContact& line) {
        if (!built_) return;
        while (now_h_ + 1.0 <= t1 + 1e-9) {
            Hour(now_h_, env, line);
            now_h_ += 1.0;
        }
    }

    // ---- reading it

    double Now() const { return now_h_; }
    const std::vector<Formation>& Formations() const { return forms_; }
    const std::vector<Axis>& Axes() const { return axes_; }
    const GroundWarData& Data() const { return *data_; }
    // A formation's strength now, its rest rate since it was last touched applied in closed form (nothing changes).
    double StrengthAt(std::size_t i, double h) const {
        const Formation& f = forms_[i];
        const double rest = f.spent ? 0.0 : LossOverHours(DailyLoss(data_->tables, f.rest, 1), h - f.settled_h);
        return f.unit.strength * (1.0 - rest);
    }
    // The line's depth in a sector: its deepest station.
    static double SectorDepth(const airwar::LineOfContact& line, std::size_t sector) {
        double best = 0.0;
        for (std::size_t i = 0; i < line.StationCount(); ++i) {
            const double a = line.StationAlong(i);
            if (a >= line.SectorFrom(sector) && a < line.SectorTo(sector)) best = std::max(best, line.StationOffset(i));
        }
        return best;
    }
    // What happened since the last ClearLog (the campaign journals it and clears it).
    const std::vector<GroundLogEntry>& Log() const { return log_; }
    void ClearLog() { log_.clear(); }

    // Profiling (fronts_bench): the parts' times by steady_clock, off by default.
    void SetProfiling(bool on) { profiling_ = on; }
    bool Profiling() const { return profiling_; }
    const GroundProfile& Profile() const { return profile_; }
    // The campaign's journaling of the hour's log (the reports part), timed by the campaign, added to that hour's day.
    void AddReportsNs(std::uint64_t ns, double h) {
        if (!profiling_) return;
        profile_.ns[GroundProfile::Reports] += ns;
        const std::size_t day = DayIndex(h);
        if (profile_.day_ms.size() <= day) profile_.day_ms.resize(day + 1, 0.0);
        profile_.day_ms[day] += static_cast<double>(ns) * 1e-6;
    }
    // A test's: resolve the axes in the reverse order (the war must not change).
    void SetReverseOrder(bool on) { reverse_ = on; }
    std::size_t QueueSize() const { return queue_.size(); }

private:
    enum class EventKind : std::uint8_t { Start, CommitSecond, Crossed, DayEnd };
    struct Event {
        double h = 0.0;
        std::uint32_t seq = 0;
        EventKind kind = EventKind::Start;
        int axis = -1;
    };
    struct Later {
        bool operator()(const Event& a, const Event& b) const { return a.h != b.h ? a.h > b.h : a.seq > b.seq; }
    };
    using Clock = std::chrono::steady_clock;

    std::shared_ptr<const GroundWarData> data_;
    std::vector<Formation> forms_;
    std::vector<Axis> axes_;
    std::vector<Event> queue_;  // a min-heap by (hour, sequence)
    std::uint32_t seq_ = 0;
    std::vector<double> shares_;       // by formation: 1 / the axes it defends this hour
    std::vector<double> loss_;         // by formation x axis: this hour's loss share from that axis
    std::vector<std::uint8_t> engaged_;  // by formation x axis: engaged on it this hour (1 security zone, 2 main zone, 3 attacking)
    std::vector<SectorResult> results_;
    std::vector<std::uint8_t> moved_;
    std::vector<GroundLogEntry> log_;
    airwar::Rng root_;
    double now_h_ = 0.0;
    double start_hour_ = 0.0;
    double security_zone_m_ = 0.0;
    bool built_ = false;
    bool profiling_ = false;
    bool reverse_ = false;
    GroundProfile profile_;

    static bool Fail(std::string* error, const std::string& m) {
        if (error != nullptr) *error = "ground war: " + m;
        return false;
    }

    const GroundDoctrineDef& Doctrine(Side s) const { return s == Side::Ours ? data_->ours.doctrine : data_->enemy.doctrine; }

    const GroundPlace* GarrisonOf(const std::string& id) const {
        const command::OrderOfBattle& oob = data_->enemy.oob;
        for (std::size_t k = 0; k < oob.units.size(); ++k) {
            if (oob.units[k].id == id && k < data_->enemy.garrisons.size()) return &data_->enemy.garrisons[k];
        }
        return nullptr;
    }

    void AddEnemy(const std::string& id) { AddFormation(data_->enemy.oob, id, Side::Enemy); }

    int AddFormation(const command::OrderOfBattle& oob, const std::string& id, Side side) {
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            if (forms_[i].unit.id == id && forms_[i].unit.side == side) return static_cast<int>(i);
        }
        const command::OobUnit* u = oob.Find(id);
        if (u == nullptr) return -1;
        Formation f;
        f.unit.id = id;
        f.unit.side = side;
        f.unit.state = UnitState::Reserve;
        f.name = u->name.empty() ? id : u->name;
        f.kind = u->kind;
        forms_.push_back(std::move(f));
        return static_cast<int>(forms_.size()) - 1;
    }

    // Whether `id` is another formation of the same side (its tree is its own).
    bool IsActor(const std::string& id, Side side, std::size_t except) const {
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            if (i != except && forms_[i].unit.side == side && forms_[i].unit.id == id) return true;
        }
        return false;
    }

    // Under `root` without passing through another actor's tree.
    bool InTree(const command::OrderOfBattle& oob, const command::OobUnit& u, const std::string& root, Side side, std::size_t self) const {
        for (const command::OobUnit* p = &u; p != nullptr; p = p->parent.empty() ? nullptr : oob.Find(p->parent)) {
            if (p->id == root) return true;
            if (IsActor(p->id, side, self)) return false;
        }
        return false;
    }

    void Equip(std::size_t i) {
        Formation& f = forms_[i];
        const GroundTables& t = data_->tables;
        const command::OrderOfBattle& oob = f.unit.side == Side::Ours ? data_->ours.oob : data_->enemy.oob;
        for (const command::OobUnit& u : oob.units) {
            if (!InTree(oob, u, f.unit.id, f.unit.side, i)) continue;
            f.personnel += static_cast<double>(u.personnel.Total());
            for (const std::pair<std::string, int>& e : u.equipment) {
                bool known = false;
                const double w = t.WeightOf(e.first, &known);
                if (!known || w <= 0.0) continue;
                const std::size_t wi = static_cast<std::size_t>(
                    std::lower_bound(t.weights.begin(), t.weights.end(), e.first,
                                     [](const std::pair<std::string, double>& a, const std::string& b) { return a.first < b; }) -
                    t.weights.begin());
                bool merged = false;
                for (Holding& h : f.holdings) {
                    if (h.weight == wi) {
                        h.count += static_cast<double>(e.second);
                        merged = true;
                    }
                }
                if (!merged) {
                    Holding h;
                    h.weight = wi;
                    h.count = static_cast<double>(e.second);
                    h.tank = t.IsTank(e.first);
                    f.holdings.push_back(h);
                }
            }
        }
        f.unit.strength = f.unit.establishment = Strength(f);
    }

    double Strength(const Formation& f) const {
        const GroundTables& t = data_->tables;
        double s = t.personnel_per_man * f.personnel;
        for (const Holding& h : f.holdings) s += t.weights[h.weight].second * h.count;
        return s;
    }

    // ---- the axis's ground, sampled once

    static bool SegmentsCross(double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy, double* t_ab) {
        const double rx = bx - ax, ry = by - ay, sx = dx - cx, sy = dy - cy;
        const double den = rx * sy - ry * sx;
        if (std::fabs(den) < 1e-12) return false;
        const double t = ((cx - ax) * sy - (cy - ay) * sx) / den;
        const double u = ((cx - ax) * ry - (cy - ay) * rx) / den;
        if (t < 0.0 || t >= 1.0 || u < 0.0 || u > 1.0) return false;
        *t_ab = t;
        return true;
    }

    Terrain TerrainAt(const CountryMap* country, const Vec3& p) const {
        if (country == nullptr) return Terrain::Open;
        const std::string cover = country->LandCoverAt(p.x, p.y);
        if (cover == "marsh") return Terrain::Marsh;
        if (cover == "town") return Terrain::Urban;
        if (cover == "forest") return Terrain::Forest;
        const double above = country->GroundHeight(p.x, p.y) - country->Def().ground.base_m;
        if (above >= data_->tables.mountains_above_m) return Terrain::Mountains;
        if (above >= data_->tables.hills_above_m) return Terrain::Hills;
        return Terrain::Open;
    }

    // The route from the border crossing to the objective, sampled every step: the ground at each depth, and where it
    // crosses a river with the bridge there.
    void SampleAxis(Axis& x, const AxisDef& def, const airwar::LineOfContact& line, const airwar::TheatreDef& theatre, const CountryMap* country) {
        const GroundTables& t = data_->tables;
        const double step_m = 1000.0 * t.terrain_step_km;
        const std::size_t n = static_cast<std::size_t>(std::ceil(x.objective_m / step_m)) + 1;
        x.ground.assign(n, Terrain::Open);
        // The route's points from the one nearest the line on; the objective's place ends it if it is not on it.
        std::vector<Vec3> pts;
        std::size_t cross = 0;
        double best = 1e18;
        for (std::size_t k = 0; k < def.route.size(); ++k) {
            const double dd = line.Place(def.route[k].pos).distance_m;
            if (dd < best) {
                best = dd;
                cross = k;
            }
        }
        const ObjectiveDef* obj = data_->enemy.plan.Objective(def.objective);
        const std::string goal = obj != nullptr ? obj->place.at : def.objective;
        for (std::size_t k = cross; k < def.route.size(); ++k) {
            pts.push_back(def.route[k].pos);
            if (def.route[k].at == goal) break;
        }
        if (pts.size() < 2) {  // no route: straight in along the line's normal
            const Vec3 a = line.PointAt(x.along_m);
            pts.clear();
            pts.push_back(a);
            pts.push_back(a + line.InwardNormal(x.along_m) * x.objective_m);
        }
        const auto depth_of = [&line](const Vec3& p) {
            const airwar::FrontPlace pl = line.Place(p);
            return pl.side * pl.distance_m;
        };
        // The ground: walk the route; each sample's terrain goes to its depth's cell (the first sample to reach it).
        std::vector<std::uint8_t> filled(n, 0);
        for (std::size_t k = 0; k + 1 < pts.size(); ++k) {
            const Vec3 a = pts[k], b = pts[k + 1];
            const double len = airwar::Dist2d(a, b);
            const int m = std::max(1, static_cast<int>(std::ceil(len / step_m)));
            for (int j = 0; j <= m; ++j) {
                const Vec3 p = a + (b - a) * (static_cast<double>(j) / static_cast<double>(m));
                const double dep = depth_of(p);
                if (dep < 0.0) continue;
                const std::size_t c = std::min(n - 1, static_cast<std::size_t>(dep / step_m));
                if (filled[c] == 0) {
                    x.ground[c] = TerrainAt(country, p);
                    filled[c] = 1;
                }
            }
            // Rivers this leg crosses, in order along it.
            if (country == nullptr) continue;
            std::vector<std::pair<double, RiverOnAxis>> found;
            for (const MapLineDef& r : country->Def().ground.rivers) {
                for (std::size_t s = 1; s < r.points.size(); ++s) {
                    double tt = 0.0;
                    if (!SegmentsCross(a.x, a.y, b.x, b.y, r.points[s - 1].east_m, r.points[s - 1].north_m, r.points[s].east_m, r.points[s].north_m, &tt)) {
                        continue;
                    }
                    const Vec3 at = a + (b - a) * tt;
                    RiverOnAxis ro;
                    ro.depth_m = depth_of(at);
                    ro.river = r.id;
                    double near_m = 1000.0 * t.bridge_reach_km;
                    for (std::size_t o = 0; o < theatre.objects.size(); ++o) {
                        if (theatre.objects[o].kind != airwar::ObjectKind::Bridge) continue;
                        const double dd = airwar::Dist2d(theatre.objects[o].pos, at);
                        if (dd <= near_m) {
                            near_m = dd;
                            ro.bridge_object = static_cast<int>(o);
                        }
                    }
                    double road_m = 1e18;
                    country->NearestCrossing(at.x, at.y, CountryNet::Road, &road_m);
                    ro.road_bridge = road_m <= 1000.0 * t.bridge_reach_km;
                    found.push_back({tt, ro});
                }
            }
            std::stable_sort(found.begin(), found.end(),
                             [](const std::pair<double, RiverOnAxis>& p, const std::pair<double, RiverOnAxis>& q) { return p.first < q.first; });
            for (const auto& fr : found) {
                if (fr.second.depth_m > 0.0 && fr.second.depth_m < x.objective_m) x.rivers.push_back(fr.second);
            }
        }
        for (std::size_t c = 1; c < n; ++c) {
            if (filled[c] == 0) x.ground[c] = x.ground[c - 1];
        }
        std::stable_sort(x.rivers.begin(), x.rivers.end(), [](const RiverOnAxis& p, const RiverOnAxis& q) { return p.depth_m < q.depth_m; });
    }

    // ---- the queue

    void Push(double h, EventKind k, int axis) {
        Event e;
        e.h = h;
        e.seq = seq_++;
        e.kind = k;
        e.axis = axis;
        queue_.push_back(e);
        std::push_heap(queue_.begin(), queue_.end(), Later());
    }

    // The war's day an hour falls in (0 its first), by the theatre's clock.
    std::size_t DayIndex(double h) const {
        return static_cast<std::size_t>(std::max(0.0, std::floor((start_hour_ + h) / 24.0) - std::floor(start_hour_ / 24.0)));
    }

    double NextMidnight(double h) const {
        const double abs_h = start_hour_ + h;
        return (std::floor(abs_h / 24.0 + 1e-9) + 1.0) * 24.0 - start_hour_;
    }

    void Note(double h, GroundNote what, int axis, int formation, int sector, double value, HaltReason reason = HaltReason::None, bool intact = false) {
        GroundLogEntry e;
        e.h = h;
        e.what = what;
        e.axis = axis;
        e.formation = formation;
        e.sector = sector;
        e.value = value;
        e.reason = reason;
        e.intact = intact;
        log_.push_back(e);
    }

    // ---- the closed form

    // A formation's rest rate over the hours since it was last touched, then its rest from now.
    void Settle(std::size_t i, double h) {
        Formation& f = forms_[i];
        if (!f.spent && h > f.settled_h) ApplyLoss(f, LossOverHours(DailyLoss(data_->tables, f.rest, 1), h - f.settled_h), -1.0);
        f.settled_h = std::max(f.settled_h, h);
    }
    void SetRest(std::size_t i, LossAction rest, double h) {
        Settle(i, h);
        forms_[i].rest = rest;
    }

    // A share of strength lost: every holding and the people in proportion; `tank_share` >= 0 replaces the tanks' (table 7-7).
    void ApplyLoss(Formation& f, double share, double tank_share) {
        const double s = std::min(1.0, std::max(0.0, share));
        for (Holding& h : f.holdings) {
            const double ts = h.tank && tank_share >= 0.0 ? std::min(1.0, tank_share) : s;
            const double lost = h.count * ts;
            h.count -= lost;
            if (h.tank) {
                f.tanks_lost += lost;
                f.tanks_reparable += lost * data_->tables.reparable_share;
            }
        }
        f.personnel *= 1.0 - s;
        f.unit.strength = Strength(f);
    }

    // ---- the hour

    void Hour(double h0, const GroundEnv& env, airwar::LineOfContact& line) {
        Clock::time_point c0;
        if (profiling_) c0 = Clock::now();
        Clock::time_point lap = c0;
        const auto Lap = [&](GroundProfile::Part p) {
            if (!profiling_) return;
            const Clock::time_point t = Clock::now();
            profile_.ns[p] += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t - lap).count());
            lap = t;
        };
        // The hour's events.
        while (!queue_.empty() && queue_.front().h <= h0 + 1e-9) {
            std::pop_heap(queue_.begin(), queue_.end(), Later());
            const Event e = queue_.back();
            queue_.pop_back();
            Handle(e, h0, line);
        }
        Lap(GroundProfile::Decisions);
        // Phase 1: every attacking axis's sides read from the hour's start.
        const std::size_t na = axes_.size();
        bool any = false;
        for (const Axis& x : axes_) any = any || x.status == AxisStatus::Attacking;
        if (any) {
            std::fill(shares_.begin(), shares_.end(), 0.0);
            std::fill(engaged_.begin(), engaged_.end(), static_cast<std::uint8_t>(0));
            for (std::size_t a = 0; a < na; ++a) {
                if (axes_[a].status != AxisStatus::Attacking) continue;
                for (int fi : axes_[a].defenders) shares_[static_cast<std::size_t>(fi)] += forms_[static_cast<std::size_t>(fi)].spent ? 0.0 : 1.0;
                if (axes_[a].depth_m >= security_zone_m_) {
                    for (int fi : axes_[a].depth_defenders) shares_[static_cast<std::size_t>(fi)] += forms_[static_cast<std::size_t>(fi)].spent ? 0.0 : 1.0;
                }
            }
            for (std::size_t k = 0; k < na; ++k) {
                const std::size_t a = reverse_ ? na - 1 - k : k;
                if (axes_[a].status == AxisStatus::Attacking) Resolve(a, h0, env);
            }
            Lap(GroundProfile::Combat);
            // Phase 2: every formation's losses, by formation in a fixed order, its axes' in theirs.
            for (std::size_t i = 0; i < forms_.size(); ++i) ApplyHour(i, h0);
            Lap(GroundProfile::Combat);
            for (std::size_t a = 0; a < na; ++a) {
                if (axes_[a].status == AxisStatus::Attacking) Advance(a, h0, env);
            }
            Lap(GroundProfile::Movement);
            for (std::size_t a = 0; a < na; ++a) {
                if (axes_[a].status == AxisStatus::Attacking) Check(a, h0 + 1.0);
            }
            Lap(GroundProfile::Decisions);
        }
        // The line where an axis moved.
        for (std::size_t a = 0; a < na; ++a) {
            if (moved_[a] == 0) continue;
            moved_[a] = 0;
            DrawLine(a, line);
        }
        Lap(GroundProfile::Line);
        if (profiling_) {
            ++profile_.hours;
            const double ms = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - c0).count()) * 1e-6;
            const std::size_t day = DayIndex(h0);
            if (profile_.day_ms.size() <= day) profile_.day_ms.resize(day + 1, 0.0);
            profile_.day_ms[day] += ms;
        }
    }

    void Handle(const Event& e, double h0, const airwar::LineOfContact& line) {
        switch (e.kind) {
            case EventKind::Start: {
                // Contact along the front: where both sides stand in a sector, the rest rate is "inactive" from now.
                for (std::size_t i = 0; i < forms_.size(); ++i) {
                    const Formation& f = forms_[i];
                    if (f.first_sector < 0) continue;
                    for (int s = f.first_sector; s <= f.last_sector; ++s) {
                        if (SideIn(s, f.unit.side == Side::Ours ? Side::Enemy : Side::Ours)) {
                            SetRest(i, LossAction::Inactive, h0);
                            break;
                        }
                    }
                }
                for (std::size_t a = 0; a < axes_.size(); ++a) {
                    Axis& x = axes_[a];
                    x.status = AxisStatus::Attacking;
                    x.start_h = x.last_advance_h = h0;
                    for (int fi : x.first) StartAttack(static_cast<std::size_t>(fi), h0);
                    Note(h0, GroundNote::AxisStarts, static_cast<int>(a), -1, x.sector, x.objective_m);
                    if (!x.second.empty()) Push(h0 + data_->tables.second_echelon_after_h, EventKind::CommitSecond, static_cast<int>(a));
                }
                break;
            }
            case EventKind::CommitSecond: {
                Axis& x = axes_[static_cast<std::size_t>(e.axis)];
                // To success: only an axis still attacking that has moved.
                if (x.status == AxisStatus::Attacking && !x.second_committed) {
                    if (x.depth_m > 0.0) {
                        Commit(static_cast<std::size_t>(e.axis), h0);
                    } else {
                        x.second_held = true;
                    }
                }
                break;
            }
            case EventKind::Crossed: {
                Axis& x = axes_[static_cast<std::size_t>(e.axis)];
                if (x.crossing_until_h >= 0.0 && x.next_river < x.rivers.size()) {
                    Note(h0, GroundNote::RiverCrossed, e.axis, -1, x.sector, x.rivers[x.next_river].depth_m);
                    ++x.next_river;
                    x.crossing_until_h = -1.0;
                    x.last_advance_h = h0;
                }
                break;
            }
            case EventKind::DayEnd: {
                for (std::size_t s = 0; s < line.SectorCount(); ++s) {
                    const double dep = SectorDepth(line, s);
                    if (dep > 0.0) Note(h0, GroundNote::DayLine, -1, -1, static_cast<int>(s), dep);
                }
                Push(NextMidnight(h0), EventKind::DayEnd, -1);
                break;
            }
        }
    }

    bool SideIn(int s, Side side) const {
        for (const Formation& f : forms_) {
            if (f.unit.side != side || f.spent) continue;
            if (f.InSector(s)) return true;
            if (side == Side::Enemy && f.axis >= 0 && axes_[static_cast<std::size_t>(f.axis)].sector == s && !f.second) return true;
        }
        return false;
    }

    void StartAttack(std::size_t i, double h0) {
        Settle(i, h0);
        Formation& f = forms_[i];
        f.unit.posture = Posture::Attack;
        f.unit.state = UnitState::InLine;
    }

    void Commit(std::size_t a, double h0) {
        Axis& x = axes_[a];
        x.second_committed = true;
        x.last_advance_h = std::max(x.last_advance_h, h0);
        for (int fi : x.second) StartAttack(static_cast<std::size_t>(fi), h0);
        Note(h0, GroundNote::SecondEchelon, static_cast<int>(a), x.second.empty() ? -1 : x.second.front(), x.sector, x.depth_m);
    }

    double AttackPower(const Axis& x) const {
        const GroundTables& t = data_->tables;
        double p = 0.0;
        for (int fi : x.first) {
            const Formation& f = forms_[static_cast<std::size_t>(fi)];
            if (!f.spent) p += UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion);
        }
        if (x.second_committed) {
            for (int fi : x.second) {
                const Formation& f = forms_[static_cast<std::size_t>(fi)];
                if (!f.spent) p += UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion);
            }
        }
        return p;
    }

    // Phase 1: one axis's hour, from the hour's start (nothing written but its own result and its engagements).
    void Resolve(std::size_t a, double h0, const GroundEnv& env) {
        const GroundTables& t = data_->tables;
        Axis& x = axes_[a];
        const std::size_t na = axes_.size();
        const bool in_zone = x.depth_m < security_zone_m_;
        SectorSide att;
        att.power = AttackPower(x);
        att.posture = Posture::Attack;
        att.contact_day = static_cast<int>(std::floor((h0 - x.start_h) / 24.0));
        SectorSide def;
        def.posture = in_zone ? Posture::Delay : Posture::PreparedDefence;
        def.covering = in_zone;
        int days = 0;
        bool thin = false;
        const auto add = [&](int fi, std::uint8_t zone) {
            const std::size_t i = static_cast<std::size_t>(fi);
            const Formation& f = forms_[i];
            if (f.spent || shares_[i] <= 0.0) return;
            const double share = (1.0 / shares_[i]) * (zone == 1 ? t.security_zone_share : 1.0);
            def.power += UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion) * share;
            engaged_[i * na + a] = zone;
            thin = thin || f.thin;
            if (f.contact_from_h >= 0.0) days = std::max(days, static_cast<int>(std::floor((h0 - f.contact_from_h) / 24.0)));
        };
        for (int fi : x.defenders) add(fi, static_cast<std::uint8_t>(in_zone ? 1 : 2));
        if (!in_zone) {
            for (int fi : x.depth_defenders) add(fi, static_cast<std::uint8_t>(2));
        }
        def.contact_day = days;
        def.thin = thin;
        const std::uint8_t none = 0, attacking = 3;
        for (int fi : x.first) engaged_[static_cast<std::size_t>(fi) * na + a] = forms_[static_cast<std::size_t>(fi)].spent ? none : attacking;
        if (x.second_committed) {
            for (int fi : x.second) engaged_[static_cast<std::size_t>(fi) * na + a] = forms_[static_cast<std::size_t>(fi)].spent ? none : attacking;
        }
        SectorConditions c;
        const double step_m = 1000.0 * t.terrain_step_km;
        c.terrain = x.ground.empty() ? Terrain::Open : x.ground[std::min(x.ground.size() - 1, static_cast<std::size_t>(std::max(0.0, x.depth_m) / step_m))];
        if (env.theatre != nullptr) c.night = airwar::TheatrePartAt(*env.theatre, start_hour_ + h0 + 0.5) == airwar::DayPart::Night;
        const int day = static_cast<int>(std::floor((start_hour_ + h0) / 24.0));
        if (env.weather != nullptr && !env.weather->empty()) {
            const std::size_t w = static_cast<std::size_t>(std::min(std::max(day, 0), static_cast<int>(env.weather->size()) - 1));
            c.poor_visibility = (*env.weather)[w] == airwar::Weather::Poor;
        }
        double ahead_m = 1e18;
        if (x.next_river < x.rivers.size()) ahead_m = std::max(0.0, x.rivers[x.next_river].depth_m - x.depth_m);
        c.river_line = ahead_m <= 1000.0 * t.river_line_km;
        c.river_ahead_km = ahead_m / 1000.0;
        // The sector's draw for the day: lognormal, mean 1, keyed by (sector, day).
        if (x.draw_day != day) {
            airwar::Rng r = root_.Fork((static_cast<std::uint64_t>(x.sector) + 1u) * 0x100000001B3ull + static_cast<std::uint64_t>(day + 100000));
            const double sigma = t.draw_sigma;
            x.draw = std::exp(sigma * r.Normal() - 0.5 * sigma * sigma);
            x.draw_day = day;
        }
        results_[a] = ResolveSector(t, att, def, c, data_->enemy.doctrine.caps, x.draw, 1.0);
        if (ahead_m < 1e17) results_[a].advance_km = std::min(results_[a].advance_km, ahead_m / 1000.0);
        x.last_ratio = results_[a].ratio;
        x.last_opposition = results_[a].opposition;
    }

    // Phase 2: one formation's losses over the hour, from every axis it fought on, in the axes' order.
    void ApplyHour(std::size_t i, double h0) {
        const std::size_t na = axes_.size();
        Formation& f = forms_[i];
        if (f.spent) return;
        double share = 0.0;
        double tank_share = -1.0;
        bool fought = false;
        int zone = -1;
        double ratio = 0.0;
        for (std::size_t a = 0; a < na; ++a) {
            const std::uint8_t z = engaged_[i * na + a];
            if (z == 0) continue;
            fought = true;
            const SectorResult& r = results_[a];
            if (z == 3) {
                share += r.attacker_loss;
            } else {
                const double part = (1.0 / shares_[i]) * (z == 1 ? data_->tables.security_zone_share : 1.0);
                share += r.defender_loss * part;
                zone = std::max(zone, static_cast<int>(z) - 1);
                ratio = std::max(ratio, r.ratio);
            }
        }
        if (!fought) return;
        Settle(i, h0);
        if (f.unit.side == Side::Ours && zone >= 0) {
            if (f.contact_from_h < 0.0) f.contact_from_h = h0;
            // FM 101-10-1 table 7-7 for its tanks over its commitment's first three days (a SAM3D mapping of the ratio to
            // how heavily it is committed); the proportional share after them.
            const TankCommitment c = zone == 0 ? (ratio >= 4.0 ? TankCommitment::DelayHeavy : ratio >= 2.5 ? TankCommitment::DelayModerate : TankCommitment::DelayLight)
                                               : (ratio >= 4.0 ? TankCommitment::DefendHeavy : TankCommitment::DefendModerate);
            if (f.zone != zone) {
                f.zone = zone;
                f.commitment = static_cast<int>(c);
                f.commit_h = h0;
                f.tanks_at_commit = f.Tanks();
            }
            const int day = static_cast<int>(std::floor((h0 - f.commit_h) / 24.0)) + 1;
            const double daily = TankLossShare(data_->tables, static_cast<TankCommitment>(f.commitment), day);
            const double tanks = f.Tanks();
            if (daily > 0.0 && tanks > 0.0) {
                const double part = zone == 0 ? data_->tables.security_zone_share : 1.0;
                tank_share = std::min(1.0, f.tanks_at_commit * daily / 24.0 * part / tanks);
            }
        }
        ApplyLoss(f, share, tank_share);
        f.settled_h = h0 + 1.0;
    }

    // The attacker's advance over the hour, stopped at a river's bank until it has crossed.
    void Advance(std::size_t a, double h0, const GroundEnv& env) {
        Axis& x = axes_[a];
        const SectorResult& r = results_[a];
        if (x.crossing_until_h >= 0.0) return;  // crossing: the bank holds it
        double adv = std::max(0.0, r.advance_km * 1000.0);
        adv = std::min(adv, x.objective_m - x.depth_m);
        if (x.next_river < x.rivers.size()) adv = std::min(adv, std::max(0.0, x.rivers[x.next_river].depth_m - x.depth_m));
        if (adv > 1.0) {
            x.depth_m += adv;
            x.last_advance_h = h0 + 1.0;
            moved_[a] = 1;
        }
        if (x.next_river < x.rivers.size() && x.depth_m >= x.rivers[x.next_river].depth_m - 1.0) {
            const RiverOnAxis& rv = x.rivers[x.next_river];
            bool intact = rv.road_bridge;
            if (rv.bridge_object >= 0 && env.truth != nullptr && static_cast<std::size_t>(rv.bridge_object) < env.truth->size()) {
                intact = (*env.truth)[static_cast<std::size_t>(rv.bridge_object)].function >= 0.5;
            }
            x.crossing_until_h = h0 + 1.0 + CrossingHours(data_->tables, intact, false);
            Push(x.crossing_until_h, EventKind::Crossed, static_cast<int>(a));
            Note(h0 + 1.0, GroundNote::RiverReached, static_cast<int>(a), -1, x.sector, rv.depth_m, HaltReason::None, intact);
        }
    }

    bool Spent(const Formation& f) const {
        const EffectivenessDef& e = Doctrine(f.unit.side).effectiveness;
        if (e.kind == EffectivenessKind::Lines) return EffectivenessByLines(e, f.unit.Fraction(), false) == Effectiveness::Ineffective;
        return 1.0 - f.unit.Fraction() >= f.breakpoint;
    }

    // After the hour: who is spent, and whether the axis goes on.
    void Check(std::size_t a, double h1) {
        Axis& x = axes_[a];
        const std::size_t na = axes_.size();
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Formation& f = forms_[i];
            if (f.spent || engaged_[i * na + a] == 0 || !Spent(f)) continue;
            f.spent = true;
            f.unit.state = UnitState::Reforming;
            f.rest = LossAction::Reserve;
            f.settled_h = h1;
            Note(h1, GroundNote::Spent, static_cast<int>(a), static_cast<int>(i), x.sector, f.unit.Fraction());
        }
        bool first_left = false, second_left = false;
        for (int fi : x.first) first_left = first_left || !forms_[static_cast<std::size_t>(fi)].spent;
        for (int fi : x.second) second_left = second_left || !forms_[static_cast<std::size_t>(fi)].spent;
        if (!first_left && !x.second_committed && second_left) Commit(a, h1);  // the first is spent: the second goes in
        const bool attackers = first_left || (x.second_committed && second_left);
        const bool echelon_left = !x.second_committed && !x.second_held && second_left;
        HaltReason why = HaltReason::None;
        if (x.depth_m >= x.objective_m - 1.0) {
            why = HaltReason::Objective;
        } else if (!attackers) {
            why = HaltReason::Spent;
        } else if (!echelon_left && x.crossing_until_h < 0.0 && h1 - x.last_advance_h >= data_->tables.stall_halt_h) {
            why = HaltReason::Stalled;
        }
        if (why == HaltReason::None) return;
        x.status = AxisStatus::Halted;
        x.halted = why;
        // The sector goes quiet: its attackers hold where they are, in contact.
        for (int fi : x.first) Hold(static_cast<std::size_t>(fi), h1);
        if (x.second_committed) {
            for (int fi : x.second) Hold(static_cast<std::size_t>(fi), h1);
        }
        for (int fi : x.defenders) SetRest(static_cast<std::size_t>(fi), LossAction::Inactive, h1);
        for (int fi : x.depth_defenders) {
            if (forms_[static_cast<std::size_t>(fi)].contact_from_h >= 0.0) SetRest(static_cast<std::size_t>(fi), LossAction::Inactive, h1);
        }
        Note(h1, GroundNote::AxisHalted, static_cast<int>(a), -1, x.sector, x.depth_m, why);
    }

    void Hold(std::size_t i, double h1) {
        Formation& f = forms_[i];
        if (f.spent) return;
        f.unit.posture = Posture::HastyDefence;
        f.rest = LossAction::Inactive;
        f.settled_h = std::max(f.settled_h, h1);
    }

    // The penetration's shape at a distance along the line: its depth over its breakthrough frontage, tapering to 0.
    static double PushedIn(const Axis& x, double along_m) {
        if (x.depth_m <= 0.0) return 0.0;
        const double off = std::fabs(along_m - x.along_m);
        if (off <= x.half_width_m) return x.depth_m;
        if (x.taper_m <= 0.0 || off >= x.half_width_m + x.taper_m) return 0.0;
        return x.depth_m * (1.0 - (off - x.half_width_m) / x.taper_m);
    }

    // The stations an axis reaches: each the deepest of the axes there (never back past the border).
    void DrawLine(std::size_t a, airwar::LineOfContact& line) {
        const Axis& x = axes_[a];
        const double lo = x.along_m - x.half_width_m - x.taper_m, hi = x.along_m + x.half_width_m + x.taper_m;
        for (std::size_t i = 0; i < line.StationCount(); ++i) {
            const double s = line.StationAlong(i);
            if (s < lo || s > hi) continue;
            double off = 0.0;
            for (const Axis& y : axes_) off = std::max(off, PushedIn(y, s));
            line.SetStationOffset(i, off);
        }
    }
};

}  // namespace groundwar
}  // namespace simcore
