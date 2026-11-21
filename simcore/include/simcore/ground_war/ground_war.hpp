// simcore/ground_war/ground_war.hpp — the ground war's clock, its line and its commanders (track F, F3.3 and F3.4;
// docs/fronts/03_GROUND_WAR.md sections 2, 3.3, 4, 5, 8 and 12, docs/fronts/README.md "Efficiency").
//
// GroundWar holds both sides' formations at U0 (the enemy's divisions, brigades and regiments its plan names, ours by the
// deployment against this adversary), each with its equipment by type summed over its tree, the enemy plan's axes (and,
// for the rebels, one raid axis per line group), and a commander for every formation and every headquarters above them
// (commanders.hpp). AirCampaign steps it with the campaign's hour (StepTo, after the hour's air results and before the
// next plan reads the line); it moves the line of contact's stations (airwar::LineOfContact::SetStationOffset) from its
// axes' penetrations, so the air war's distances to the front, the air front's bands and their readers follow the line.
//
// What is stepped and what is not:
//   * an event queue keyed by (hour, sequence) (a binary heap on a vector sized at Build) holds what happens at a set
//     hour: an offensive's start, a river crossed, the day's end, and the commanders' decisions and the orders that
//     follow them (an echelon or a counterattack arriving, a covering force ordered back a line, a unit re-formed, a raid
//     ending at dawn); a quiet hour costs a look at its top;
//   * each hour, only the axes attacking or counterattacked are resolved, one F3.2 ResolveSector each: both sides'
//     power from their formations' strength and cohesion, the defender's posture (the security zone's covering share in
//     delay, then the main defensive zone prepared; an attacker hit in the flank by our counterattack in a meeting
//     engagement, or NATO's covering force screening in delay), the ground, the night, the weather and a river line;
//     losses, cohesion and the advance applied;
//   * everything else is closed form: a formation out of the fight takes its rest rate (FM 101-10-1's "inactive" in
//     contact, "reserve" out of it) and regains cohesion over the hours since it was last touched, when it is next touched;
//   * an axis's ground and rivers are sampled along its route once, at Build (CountryMap lookups never run per hour);
//   * no allocation in the hourly step once built: the vectors are sized at Build, the log's and queue's capacity reserved.
// Two phases an hour (every axis's power read, then every loss applied by formation in a fixed order), so the order the
// axes are resolved in changes nothing; every draw is a fork keyed by (sector, day), the formation or (commander,
// decision).
//
// Who decides (F3.4, docs/fronts/03_GROUND_WAR.md section 5): commanders, each at his decision time (FM 100-2-1's
// reaction and planning times) or woken by an event (a breakthrough, a request from below, his unit crossing an
// effectiveness line or a breakpoint, an axis halted). The enemy's top (the Pact's army, NATO's corps, the neutral's
// division) commits its second echelon and reserve to the axis that moves (the Pact) or to their own (NATO, the neutral),
// stops an axis that has stalled with no echelon coming, resumes one our counterattack has stopped when the ratio is back
// at its attack ratio, and permits its ineffective units' withdrawal. Our security zone holds each line until the
// division orders the covering force back; the main defensive zone holds; our army commander answers a request to
// withdraw (behind the next, a relief marched in, or refused) and counterattacks a penetration into the main zone with
// his second echelon and reserve when his force over the penetration's reaches his counterattack ratio, back to the
// border at most. A unit retreats as a unit (ordered, or NATO's decided at its breakpoint) or disperses (under its line
// with no order while in contact, cohesion gone, or the rebels); re-forms smaller behind. The rebels raid by night where
// we are thin. Every decision goes into the log (the campaign's journal, kind "ground", hidden).
//
// The commanders' picture (F3.5, picture.hpp, docs/fronts/03_GROUND_WAR.md section 9): every read a commander makes of
// the war's strengths, depths and paces goes through the Seen* accessors below, which read his side's picture: its own
// units from their reports (late by the reporting delay), the other side's with an age and an error, updated only when a
// report arrives (events in the queue). A formation's own commander knows his own unit's state (OwnFraction). The enemy's
// picture also says where its own ground front is collapsing (EnemyCrisis: the air front's crisis, air_front.hpp).
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
#include "simcore/air_war/intel.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/country_map.hpp"
#include "simcore/ground_war/combat.hpp"
#include "simcore/ground_war/commanders.hpp"
#include "simcore/ground_war/ground_data.hpp"
#include "simcore/ground_war/picture.hpp"
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
    double repair = 0.0;  // tanks of this type lost and in repair, returned over the days (F3.4)
};

// What a formation is doing for its commander's orders.
enum class Task : std::uint8_t { None, Echelon, Counter, Relief };

// A formation at U0: the actor, with its equipment summed over its tree (F3.2's GroundUnit for its strength and state).
struct Formation {
    GroundUnit unit;
    std::string name;
    std::string kind;        // the order of battle's: division, brigade, regiment...
    bool spent = false;      // out of the fight: withdrawn to re-form, or dispersed (unit.state says which)
    int cmdr = -1;           // its commander (GroundWar::Commanders())
    // The enemy's: its axis now (-1 none), the plan's axis for it, and whether it is an echelon not yet committed.
    int axis = -1;
    int plan_axis = -1;
    bool second = false;
    int raid_axis = -1;      // a rebel line group's raid axis
    // Ours: its role and run of sectors (-1 off the line) and along the line; thin when wider than its doctrine's frontage.
    DeployRole role = DeployRole::Reserve;
    int first_sector = -1;
    int last_sector = -1;
    bool thin = false;
    bool free = false;       // ours: free for the army commander to send (a reserve, or re-formed)
    // Breakpoints (NATO and the small forces): cumulative losses at which an attack stops and a defence breaks.
    double attack_stop = 1.0;
    double defence_break = 1.0;
    bool attack_stopped = false;
    // Its commander's dealings over its strength (commanders.hpp DecideUnit).
    bool reported = false;   // under the degraded line, reported
    bool asked = false;      // has asked to withdraw
    double asked_h = -1.0;
    bool refused = false;    // the senior refused (journaled once)
    bool held_noted = false; // an echelon held back (journaled once)
    std::uint8_t crossed = 0;  // the lines it has crossed (bits: degraded, ask, attack stop, defence break)
    double last_fought_h = -1.0;
    Task task = Task::None;
    int task_axis = -1;
    int relief_for = -1;     // a relief: the formation it relieves
    Vec3 pos;                // where it is, for a march's length
    double base_establishment = 0.0;  // its war establishment before any re-forming
    int withdrawals = 0;
    int dispersals = 0;
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
    double tanks_reparable = 0.0;  // of those lost, the table's reparable share
    double tanks_returned = 0.0;   // of those, back from repair
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

// Counterattacked: our counterattack is in progress on it (we attack, the enemy defends).
enum class AxisStatus : std::uint8_t { Waiting, Attacking, Halted, Counterattacked };
// Ordered: the senior stopped it (stalled with no echelon coming); Breakpoint: its attackers stopped on their own;
// Counterattacked: held where our counterattack left it, until the senior resumes it.
enum class HaltReason : std::uint8_t { None, Objective, Spent, Stalled, Breakpoint, Counterattacked };

inline const char* HaltReasonName(HaltReason r) {
    switch (r) {
        case HaltReason::None: return "none";
        case HaltReason::Objective: return "objective";
        case HaltReason::Spent: return "spent";
        case HaltReason::Stalled: return "stalled";
        case HaltReason::Breakpoint: return "breakpoint";
        case HaltReason::Counterattacked: return "counterattacked";
    }
    return "?";
}

struct Axis {
    std::string id;
    std::string name;
    int sector = -1;
    bool raid = false;         // a rebel raid axis: attacks for a night, then back to the line
    double along_m = 0.0;      // the penetration's middle along the base line (its border crossing's projection)
    double objective_m = 0.0;  // how deep its objective is (or the plan's political depth)
    double depth_m = 0.0;      // how far it has pushed the line in
    double half_width_m = 4000.0;
    double taper_m = 12500.0;
    AxisStatus status = AxisStatus::Waiting;
    HaltReason halted = HaltReason::None;
    double start_h = 0.0;
    double last_advance_h = 0.0;
    double pace_km_day = 0.0;  // its advance averaged over about a day (tables' pace_window_h)
    int incoming = 0;          // echelons marching to it
    std::vector<int> first;    // formation indices: its attackers (the plan's first echelon and those committed since)
    std::vector<int> second;   // the plan's second echelon for it, not yet committed
    std::vector<int> defenders;  // ours on the line in its sector (first echelon, flank screens, reliefs)
    std::vector<int> depth_defenders;  // ours of the sector's second echelon, engaged at the main defensive zone
    std::vector<int> counter;  // ours counterattacking on it
    int counter_incoming = 0;  // our counterattack's formations marching to it
    double counter_start_h = 0.0;
    double counter_advance_h = 0.0;  // when the counterattack last gained ground
    double counter_end_h = -1.0;     // when the last one ended (another only once the enemy has advanced since)
    // The security zone: its lines' depths, the one the covering force holds, and whether the division's order is due.
    std::vector<double> lines_m;
    std::size_t next_line = 0;
    bool line_pending = false;
    bool through_zone = false;  // the enemy has reached the main defensive zone (it does not come back)
    int zone_cdr = -1;          // our commander who runs its security zone
    double raid_share = 1.0;    // a raid: the share of each defender's frontage it meets
    std::vector<Terrain> ground;  // the defender's ground by depth, every clock.terrain_step_km
    std::vector<RiverOnAxis> rivers;
    std::size_t next_river = 0;
    double crossing_until_h = -1.0;  // crossing its next river until then (-1: not crossing)
    int draw_day = -1000000;
    double draw = 1.0;
    double last_ratio = 0.0;
    Opposition last_opposition = Opposition::Stalled;
};

// The commanders' decisions as the log records them (F3.4).
enum class GroundOrder : std::uint8_t {
    EchelonCommitted,      // formation to axis (other: the plan's axis for it, when switched)
    EchelonHeld,           // formation held back: no axis moves
    AxisStopped,           // the senior stops a stalled axis
    AttackResumed,         // the senior resumes an axis
    AttackStopped,         // a formation past its attack breakpoint stops and holds
    Reported,              // a formation under its degraded line
    WithdrawAsked,         // a formation asks to withdraw
    WithdrawOrdered,       // its senior orders it back
    WithdrawDecided,       // it withdraws on its own (NATO at its breakpoint, or the top's own)
    WithdrawRefused,       // the senior refuses: hold
    Dispersed,             // breaks and disperses (value: why, 0 no order, 1 cohesion, 2 a rebel's breakpoint)
    Reformed,              // re-formed behind, smaller (value: its strength fraction of its old establishment)
    ReliefOrdered,         // formation sent to relieve other
    Relieved,              // formation relieved by other, withdraws
    CounterattackOrdered,  // our army commander's counterattack on axis (value: his ratio)
    CounterattackStarts,
    CounterattackEnds,     // value: the axis's depth left
    CoveringBack,          // the division orders the covering force back to the next line (value: its depth)
    RaidStarts,
    RaidEnds,              // value: how deep it got
};
constexpr std::size_t kGroundOrderCount = 20;

inline const char* GroundOrderName(GroundOrder o) {
    switch (o) {
        case GroundOrder::EchelonCommitted: return "echelon_committed";
        case GroundOrder::EchelonHeld: return "echelon_held";
        case GroundOrder::AxisStopped: return "axis_stopped";
        case GroundOrder::AttackResumed: return "attack_resumed";
        case GroundOrder::AttackStopped: return "attack_stopped";
        case GroundOrder::Reported: return "reported";
        case GroundOrder::WithdrawAsked: return "withdraw_asked";
        case GroundOrder::WithdrawOrdered: return "withdraw_ordered";
        case GroundOrder::WithdrawDecided: return "withdraw_decided";
        case GroundOrder::WithdrawRefused: return "withdraw_refused";
        case GroundOrder::Dispersed: return "dispersed";
        case GroundOrder::Reformed: return "reformed";
        case GroundOrder::ReliefOrdered: return "relief_ordered";
        case GroundOrder::Relieved: return "relieved";
        case GroundOrder::CounterattackOrdered: return "counterattack_ordered";
        case GroundOrder::CounterattackStarts: return "counterattack_starts";
        case GroundOrder::CounterattackEnds: return "counterattack_ends";
        case GroundOrder::CoveringBack: return "covering_back";
        case GroundOrder::RaidStarts: return "raid_starts";
        case GroundOrder::RaidEnds: return "raid_ends";
    }
    return "?";
}

// What the ground war says happened, for the campaign's journal (never read by the simulation).
enum class GroundNote : std::uint8_t { AxisStarts, RiverReached, RiverCrossed, AxisHalted, DayLine, Order };
struct GroundLogEntry {
    double h = 0.0;
    GroundNote what = GroundNote::AxisStarts;
    int axis = -1;
    int formation = -1;
    int sector = -1;
    double value = 0.0;
    HaltReason reason = HaltReason::None;
    bool intact = false;  // RiverReached: the bridge was standing
    GroundOrder order = GroundOrder::EchelonCommitted;  // Order: which
    int cmdr = -1;        // Order: the commander who decided
    int other = -1;       // Order: another formation (a relief's) or axis (an echelon's planned one)
};

// Where the hour goes, by part (docs/fronts/README.md's budget lines), when profiling: steady_clock, summed.
struct GroundProfile {
    enum Part : int { Combat, Decisions, Movement, Line, Picture, Reports, kParts };
    std::uint64_t ns[kParts] = {};
    std::uint64_t hours = 0;
    std::uint64_t decisions = 0;  // commanders' decisions made
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
    const airwar::IntelPicture* intel = nullptr;  // the enemy's picture of our side (its troops entries: its air's sightings); null none
};

// ------------------------------------------------------------------------------------------------------- the ground war

class GroundWar {
public:
    // Builds both sides at U0 on the campaign's line (its sectors by id), their commanders, samples each axis's ground and
    // rivers on the map (`country`, may be null: open ground, no rivers) and queues the plan's start. False with a message
    // when the data name a sector the line lacks.
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
            f.free = u.role == DeployRole::Reserve;
            f.pos = u.place.pos;
            if (!u.sectors.empty()) {
                f.first_sector = line.SectorIndex(u.sectors.front());
                f.last_sector = line.SectorIndex(u.sectors.back());
                if (f.first_sector < 0 || f.last_sector < 0) return Fail(error, "our " + u.unit + "'s sectors are not the line's");
                const double from_m = line.Sector(static_cast<std::size_t>(f.first_sector)).from_m;
                const double to_m = line.Sector(static_cast<std::size_t>(f.last_sector)).to_m;
                const double width_km = (to_m - from_m) / 1000.0;
                const FrontageDef* fr = d.ours.doctrine.Frontage(f.kind, "defence");
                f.thin = fr != nullptr && fr->width_km.hi > 0.0 && width_km > fr->width_km.hi;
                if (u.place.at.empty()) f.pos = line.PointAt(0.5 * (from_m + to_m));
            }
        }
        // Each formation's tree, its equipment and people summed, stopping at another actor's tree.
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Equip(i);
            forms_[i].base_establishment = forms_[i].unit.establishment;
            if (forms_[i].unit.side == Side::Enemy) {
                const GroundPlace* g = GarrisonOf(forms_[i].unit.id);
                if (g != nullptr) forms_[i].pos = g->pos;
            }
        }
        // The commanders: one per formation and per headquarters between them and the side's top.
        BuildCommanders(Side::Enemy);
        BuildCommanders(Side::Ours);
        // Breakpoints drawn per formation (NATO, the small forces), moved by its commander's composure; the Pact and ours
        // go by their lines.
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Formation& f = forms_[i];
            const EffectivenessDef& e = Doctrine(f.unit.side).effectiveness;
            if (e.kind != EffectivenessKind::Breakpoints) continue;
            airwar::Rng r = root_.Fork(0xB4EA0000ull + i);
            const double pull = f.cmdr >= 0 ? BreakpointPullOf(cmdrs_[static_cast<std::size_t>(f.cmdr)].traits) : 0.0;
            f.attack_stop = DrawBreakpoint(e, true, r.Uniform(), pull, t.trait_shift);
            f.defence_break = DrawBreakpoint(e, false, r.Uniform(), pull, t.trait_shift);
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
                        f.axis = f.plan_axis = static_cast<int>(a);
                        x.first.push_back(static_cast<int>(i));
                    } else if (std::find(def.second_echelon.begin(), def.second_echelon.end(), f.unit.id) != def.second_echelon.end()) {
                        f.plan_axis = static_cast<int>(a);
                        f.second = true;
                        x.second.push_back(static_cast<int>(i));
                    }
                } else if (f.InSector(x.sector)) {
                    if (f.role == DeployRole::FirstEchelon || f.role == DeployRole::FlankScreen) x.defenders.push_back(static_cast<int>(i));
                    if (f.role == DeployRole::SecondEchelon) x.depth_defenders.push_back(static_cast<int>(i));
                }
            }
            SampleAxis(x, def, line, theatre, country);
            // The security zone's lines (our doctrine's spacing), from the forward edge to the main defensive zone.
            const GroundDoctrineDef& od = d.ours.doctrine;
            const double spacing_m = 1000.0 * od.security_line_spacing_km.Mid();
            if (od.has_security_zone && security_zone_m_ > 0.0 && spacing_m > 0.0) {
                for (double m = 0.0; m < security_zone_m_ - 1.0; m += spacing_m) x.lines_m.push_back(m);
            }
            x.zone_cdr = ZoneCommander(x);
        }
        // The plan's reserve: echelons for the top to send.
        for (const std::string& id : plan.reserve) {
            const int fi = FindFormation(id, Side::Enemy);
            if (fi >= 0) forms_[static_cast<std::size_t>(fi)].second = true;
        }
        // The enemy's line formations off the axes (the raids' lines) hold the sector their garrison projects into; each
        // raids on its own axis there (03 section 8).
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Formation& f = forms_[i];
            if (f.unit.side != Side::Enemy || f.axis >= 0 || !plan.has_raids) continue;
            if (std::find(plan.raids.lines.begin(), plan.raids.lines.end(), f.unit.id) == plan.raids.lines.end()) continue;
            const GroundPlace* g = GarrisonOf(f.unit.id);
            if (g == nullptr || g->at.empty()) continue;
            f.first_sector = f.last_sector = line.SectorOf(g->pos);
            if (f.first_sector < 0) continue;
            AddRaidAxis(i, *g, line, theatre, country);
        }
        // Capacities: no list grows past these in the hour.
        const std::size_t nf = forms_.size();
        for (Axis& x : axes_) {
            x.first.reserve(nf);
            x.second.reserve(nf);
            x.defenders.reserve(nf);
            x.depth_defenders.reserve(nf);
            x.counter.reserve(nf);
        }
        const std::size_t na = axes_.empty() ? 1 : axes_.size();
        shares_.assign(nf, 0.0);
        loss_.assign(nf * na, 0.0);
        part_.assign(nf * na, 0.0);
        results_.assign(axes_.size(), SectorResult());
        engaged_.assign(nf * na, 0);
        fought_.assign(nf, 0);
        moved_.assign(axes_.size(), 0);
        choices_.assign(axes_.size(), AxisChoice());
        log_.reserve(512);
        queue_.reserve(64 + 8 * (nf + cmdrs_.size() + axes_.size()) + 6 * nf + 4 * axes_.size());
        // The plan's start; the days' ends.
        const double start = std::max(0.0, plan.start_after_h);
        Push(start, EventKind::Start);
        Push(NextMidnight(0.0), EventKind::DayEnd);
        // Each side's picture (F3.5): its own units as reported, the other's as known in peacetime; then the reports.
        BuildPictures(line);
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
    const std::vector<GroundCommander>& Commanders() const { return cmdrs_; }
    const GroundWarData& Data() const { return *data_; }
    // The side's top commander (-1 none).
    int TopOf(Side s) const { return s == Side::Ours ? top_[0] : top_[1]; }
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
    std::size_t QueueCapacity() const { return queue_.capacity(); }

    // ---- the commanders' picture (F3.5, picture.hpp)
    //
    // Every read a commander makes of the war's strengths, depths and paces goes through these four: his side's picture,
    // as its reports left it (a test's tables.picture.truth: the truth now, F3.4's hand cases). Who is on which axis and
    // whether an axis attacks are known to both sides (their orders and the contact say so); how strong, how deep and
    // how fast are believed.

    // A formation's power (strength x supply and cohesion) as `by`'s side believes it. Its own side knows at once which of
    // its units are out of the fight (it ordered them out, or was told).
    double SeenPower(Side by, std::size_t i) const {
        const Formation& f = forms_[i];
        if (data_->tables.picture.truth) return f.spent ? 0.0 : UnitPower(data_->tables, f.unit.strength, f.unit.supply, f.unit.cohesion);
        const UnitBelief& b = pictures_[PictureIndex(by)].units[i];
        if (f.unit.side == by) return f.spent ? 0.0 : b.power;
        return b.out ? 0.0 : b.power;
    }
    // A formation's strength fraction as `by`'s side believes it.
    double SeenFraction(Side by, std::size_t i) const {
        if (data_->tables.picture.truth) return forms_[i].unit.Fraction();
        return pictures_[PictureIndex(by)].units[i].fraction;
    }
    // An axis's depth and pace as `by`'s side believes them.
    double SeenDepth(Side by, std::size_t a) const {
        if (data_->tables.picture.truth) return axes_[a].depth_m;
        return pictures_[PictureIndex(by)].axes[a].depth_m;
    }
    double SeenPace(Side by, std::size_t a) const {
        if (data_->tables.picture.truth) return axes_[a].pace_km_day;
        return pictures_[PictureIndex(by)].axes[a].pace_km_day;
    }
    // A formation's strength fraction as its own commander knows it (his staff's count: his reaction time is its delay).
    double OwnFraction(std::size_t i) const { return forms_[i].unit.Fraction(); }

    // A side's picture (F3.5): what its staffs believe, by formation and axis, the line by station.
    const GroundPicture& Picture(Side by) const { return pictures_[PictureIndex(by)]; }
    // The sectors where the enemy's own ground front is collapsing in its picture (a formation of its own believed under
    // picture.crisis_below, or our counterattack on an axis there): the air front's crisis input (air_front.hpp), by the
    // line's sector index.
    const std::vector<char>& EnemyCrisis() const { return pictures_[1].crisis; }
    // A test's: takes a share off a formation's strength now (the truth moves; a picture moves only when a report comes).
    void TestLoss(std::size_t i, double share) {
        Settle(i, now_h_);
        ApplyLoss(forms_[i], share, -1.0);
    }

private:
    enum class EventKind : std::uint8_t {
        Start, Crossed, DayEnd, Decide, LineBack, Arrive, Reformed, RaidNight, RaidEnd,
        ReportsDue, IntelDue, UnitReport, AxisReport, LineReport  // the pictures' (F3.5)
    };
    static bool IsPictureEvent(EventKind k) {
        return k == EventKind::ReportsDue || k == EventKind::IntelDue || k == EventKind::UnitReport || k == EventKind::AxisReport ||
               k == EventKind::LineReport;
    }
    struct Event {
        double h = 0.0;
        std::uint32_t seq = 0;
        EventKind kind = EventKind::Start;
        int axis = -1;
        int who = -1;            // a commander (Decide, LineBack, RaidNight) or a formation (Arrive, Reformed)
        std::uint32_t gen = 0;   // Decide: the commander's decision it is (older ones are stale)
        Task task = Task::None;  // Arrive: what for
        // A report (F3.5): who = the formation (UnitReport) or the picture's side (AxisReport, LineReport, ReportsDue);
        // axis = the picture's side (UnitReport) or the axis (AxisReport); what it says, compiled at obs_h.
        double obs_h = 0.0;
        double v0 = 0.0;   // UnitReport: the believed fraction; AxisReport: the depth
        double v1 = 0.0;   // UnitReport: the believed power; AxisReport: the pace
        double factor = 1.0;
        int sector = -1;
        std::uint8_t src = 0;      // a PictureSource
        std::uint8_t posture = 0;  // UnitReport: a Posture
        std::uint8_t flag = 0;     // UnitReport: out of the fight; AxisReport: counterattacked
    };
    struct Later {
        bool operator()(const Event& a, const Event& b) const { return a.h != b.h ? a.h > b.h : a.seq > b.seq; }
    };
    using Clock = std::chrono::steady_clock;
    enum : std::uint8_t { kCrossDegraded = 1, kCrossAsk = 2, kCrossAttackStop = 4, kCrossDefenceBreak = 8 };

    std::shared_ptr<const GroundWarData> data_;
    std::vector<Formation> forms_;
    std::vector<Axis> axes_;
    std::vector<GroundCommander> cmdrs_;
    int top_[2] = {-1, -1};  // ours, the enemy's
    std::vector<Event> queue_;  // a min-heap by (hour, sequence)
    std::uint32_t seq_ = 0;
    std::vector<double> shares_;       // by formation: 1 / the axes it defends this hour
    std::vector<double> loss_;         // by formation x axis: this hour's loss share from that axis
    std::vector<double> part_;         // by formation x axis: the share of its strength engaged there (defenders)
    std::vector<std::uint8_t> engaged_;  // by formation x axis: engaged on it this hour (1 security zone, 2 main zone, 3 attacking)
    std::vector<std::uint8_t> fought_;   // by formation: fought this hour
    std::vector<SectorResult> results_;
    std::vector<std::uint8_t> moved_;
    std::vector<AxisChoice> choices_;
    std::vector<GroundLogEntry> log_;
    GroundPicture pictures_[2];  // ours, the enemy's (F3.5)
    bool crisis_dirty_ = false;
    airwar::Rng root_;
    double now_h_ = 0.0;
    double start_hour_ = 0.0;
    double security_zone_m_ = 0.0;
    double offensive_h_ = -1.0;  // when the plan's offensive started
    bool built_ = false;
    bool profiling_ = false;
    bool reverse_ = false;
    GroundProfile profile_;

    static bool Fail(std::string* error, const std::string& m) {
        if (error != nullptr) *error = "ground war: " + m;
        return false;
    }

    const GroundDoctrineDef& Doctrine(Side s) const { return s == Side::Ours ? data_->ours.doctrine : data_->enemy.doctrine; }
    const command::OrderOfBattle& OobOf(Side s) const { return s == Side::Ours ? data_->ours.oob : data_->enemy.oob; }

    const GroundPlace* GarrisonOf(const std::string& id) const {
        const command::OrderOfBattle& oob = data_->enemy.oob;
        for (std::size_t k = 0; k < oob.units.size(); ++k) {
            if (oob.units[k].id == id && k < data_->enemy.garrisons.size()) return &data_->enemy.garrisons[k];
        }
        return nullptr;
    }

    int FindFormation(const std::string& id, Side side) const {
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            if (forms_[i].unit.id == id && forms_[i].unit.side == side) return static_cast<int>(i);
        }
        return -1;
    }

    void AddEnemy(const std::string& id) { AddFormation(data_->enemy.oob, id, Side::Enemy); }

    int AddFormation(const command::OrderOfBattle& oob, const std::string& id, Side side) {
        const int have = FindFormation(id, side);
        if (have >= 0) return have;
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
        const command::OrderOfBattle& oob = OobOf(f.unit.side);
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

    // ---- the commanders, built once

    int CommanderOfUnit(const std::string& unit, Side side) const {
        for (std::size_t c = 0; c < cmdrs_.size(); ++c) {
            if (cmdrs_[c].side == side && cmdrs_[c].unit == unit) return static_cast<int>(c);
        }
        return -1;
    }

    // One commander per formation of `side` and per headquarters between them and the side's top (the lowest unit over
    // them all in its order of battle; with none, the first formation of the plan's main axis or of the deployment);
    // each answers to the nearest commander above him, the top to nobody. Written by hand where the data name him,
    // drawn from the pool otherwise.
    void BuildCommanders(Side side) {
        const command::OrderOfBattle& oob = OobOf(side);
        const GroundCommandersDef& defs = side == Side::Ours ? data_->ours.commanders : data_->enemy.commanders;
        std::vector<std::size_t> mine;
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            if (forms_[i].unit.side == side) mine.push_back(i);
        }
        if (mine.empty()) return;
        // The top: the lowest unit over every formation.
        std::string top;
        for (const command::OobUnit* p = oob.Find(forms_[mine.front()].unit.id); p != nullptr && top.empty();
             p = p->parent.empty() ? nullptr : oob.Find(p->parent)) {
            bool all = true;
            for (std::size_t i : mine) all = all && oob.Under(forms_[i].unit.id, p->id);
            if (all) top = p->id;
        }
        if (top.empty()) {
            const AxisDef* main = side == Side::Enemy ? data_->enemy.plan.MainAxis() : nullptr;
            top = main != nullptr && !main->first_echelon.empty() ? main->first_echelon.front() : forms_[mine.front()].unit.id;
        }
        const auto add = [&](const std::string& unit, int formation) {
            if (CommanderOfUnit(unit, side) >= 0) return;
            const command::OobUnit* u = oob.Find(unit);
            const std::string kind = u != nullptr ? u->kind : std::string("division");
            const std::size_t idx = cmdrs_.size();
            airwar::Rng r = root_.Fork(0xC0DE0000ull + (side == Side::Ours ? 0x10000ull : 0ull) + idx);
            GroundCommander c = DrawCommander(defs.pool, kind, r);
            for (const GroundCommanderDef& a : defs.authored) {
                if (a.unit != unit) continue;
                c.id = a.id;
                c.name = a.name;
                c.age = a.age;
                c.background = a.background;
                c.bio = a.bio;
                c.traits = a.traits;
                c.authored = true;
            }
            if (c.id.empty()) c.id = unit + "_cdr";
            c.unit = unit;
            c.side = side;
            c.formation = formation;
            cmdrs_.push_back(std::move(c));
        };
        for (std::size_t i : mine) {
            add(forms_[i].unit.id, static_cast<int>(i));
            forms_[i].cmdr = CommanderOfUnit(forms_[i].unit.id, side);
        }
        for (std::size_t i : mine) {
            if (!oob.Under(forms_[i].unit.id, top)) continue;
            const command::OobUnit* u = oob.Find(forms_[i].unit.id);
            for (const command::OobUnit* p = u == nullptr || u->parent.empty() ? nullptr : oob.Find(u->parent); p != nullptr;
                 p = p->parent.empty() ? nullptr : oob.Find(p->parent)) {
                add(p->id, -1);
                if (p->id == top) break;
            }
        }
        if (CommanderOfUnit(top, side) < 0) add(top, FindFormation(top, side));
        const int top_c = CommanderOfUnit(top, side);
        top_[side == Side::Ours ? 0 : 1] = top_c;
        for (std::size_t c = 0; c < cmdrs_.size(); ++c) {
            GroundCommander& g = cmdrs_[c];
            if (g.side != side) continue;
            g.top = static_cast<int>(c) == top_c;
            g.senior = -1;
            if (g.top) continue;
            const command::OobUnit* u = oob.Find(g.unit);
            for (const command::OobUnit* p = u == nullptr || u->parent.empty() ? nullptr : oob.Find(u->parent); p != nullptr && g.senior < 0;
                 p = p->parent.empty() ? nullptr : oob.Find(p->parent)) {
                g.senior = CommanderOfUnit(p->id, side);
            }
            if (g.senior < 0) g.senior = top_c;
        }
    }

    // Who runs an axis's security zone: the commander of the division (or brigade) holding its sector, the senior of a
    // regiment that holds it.
    int ZoneCommander(const Axis& x) const {
        for (int fi : x.defenders) {
            const Formation& f = forms_[static_cast<std::size_t>(fi)];
            if (f.cmdr < 0) continue;
            const GroundCommander& c = cmdrs_[static_cast<std::size_t>(f.cmdr)];
            if (f.kind == "regiment" && c.senior >= 0 && !cmdrs_[static_cast<std::size_t>(c.senior)].top) return c.senior;
            return f.cmdr;
        }
        return top_[0];
    }

    // A rebel line group's raid axis: straight in from its garrison's place on the line, as wide as the group's attack
    // frontage, as deep as the plan's raids (03 section 8).
    void AddRaidAxis(std::size_t fi, const GroundPlace& g, const airwar::LineOfContact& line, const airwar::TheatreDef& theatre,
                     const CountryMap* country) {
        const GroundTables& t = data_->tables;
        const GroundPlanDef& plan = data_->enemy.plan;
        Formation& f = forms_[fi];
        Axis x;
        x.id = "raid_" + f.unit.id;
        x.name = "the raid from " + f.name;
        x.raid = true;
        x.sector = f.first_sector;
        const std::size_t s = static_cast<std::size_t>(x.sector);
        x.along_m = std::min(std::max(line.Place(g.pos).along_m, line.Sector(s).from_m), line.Sector(s).to_m);
        x.objective_m = std::max(1000.0 * plan.raids.max_depth_km, 1000.0);
        const FrontageDef* fr = data_->enemy.doctrine.Frontage(f.kind, "attack");
        const double width_m = 1000.0 * (fr != nullptr && fr->width_km.hi > 0.0 ? fr->width_km.Mid() : t.breakthrough_width_km.Mid());
        x.half_width_m = 0.5 * width_m;
        x.taper_m = width_m;
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            const Formation& o = forms_[i];
            if (o.unit.side != Side::Ours || !o.InSector(x.sector)) continue;
            if (o.role != DeployRole::FirstEchelon && o.role != DeployRole::FlankScreen) continue;
            x.defenders.push_back(static_cast<int>(i));
        }
        // The share of a defender's frontage the raid meets: its own width over the defender's run of sectors.
        double held_m = line.Sector(s).to_m - line.Sector(s).from_m;
        for (int di : x.defenders) {
            const Formation& o = forms_[static_cast<std::size_t>(di)];
            held_m = std::max(held_m, line.Sector(static_cast<std::size_t>(o.last_sector)).to_m - line.Sector(static_cast<std::size_t>(o.first_sector)).from_m);
        }
        x.raid_share = held_m > 0.0 ? std::min(1.0, width_m / held_m) : 1.0;
        AxisDef def;
        def.id = x.id;
        SampleAxis(x, def, line, theatre, country);
        x.zone_cdr = ZoneCommander(x);
        f.raid_axis = static_cast<int>(axes_.size());
        axes_.push_back(std::move(x));
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

    void Push(double h, EventKind k, int axis = -1, int who = -1, std::uint32_t gen = 0, Task task = Task::None) {
        Event e;
        e.h = h;
        e.seq = seq_++;
        e.kind = k;
        e.axis = axis;
        e.who = who;
        e.gen = gen;
        e.task = task;
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

    void Order(double h, GroundOrder o, int cmdr, int formation, int axis, double value = 0.0, int other = -1) {
        GroundLogEntry e;
        e.h = h;
        e.what = GroundNote::Order;
        e.order = o;
        e.cmdr = cmdr;
        e.formation = formation;
        e.axis = axis;
        e.sector = axis >= 0 ? axes_[static_cast<std::size_t>(axis)].sector : -1;
        e.value = value;
        e.other = other;
        log_.push_back(e);
    }

    // ---- the commanders' clock

    // A keyed draw in [0, 1) for a commander's decision (order-free: the same whatever else is drawn).
    double DecisionDraw(int c, std::uint64_t salt) const {
        const std::uint64_t k = 0xDEC1500000000000ull ^ (static_cast<std::uint64_t>(c + 1) << 32) ^
                                (static_cast<std::uint64_t>(cmdrs_[static_cast<std::size_t>(c)].decisions) << 8) ^ salt;
        return root_.Fork(k).Uniform();
    }

    double ReactionOf(int c, std::uint64_t salt) const {
        const GroundCommander& g = cmdrs_[static_cast<std::size_t>(c)];
        return DecideHoursOf(data_->tables, g.echelon, false, g.traits, DecisionDraw(c, salt));
    }
    double PlanningOf(int c, std::uint64_t salt) const {
        const GroundCommander& g = cmdrs_[static_cast<std::size_t>(c)];
        return DecideHoursOf(data_->tables, g.echelon, true, g.traits, DecisionDraw(c, salt));
    }

    // Wakes a commander to decide at `h`: unless a decision of his is already due by then (it decides on everything).
    void Wake(int c, double h) {
        if (c < 0) return;
        GroundCommander& g = cmdrs_[static_cast<std::size_t>(c)];
        if (g.next_h >= now_h_ - 1e-9 && g.next_h <= h + 1e-9) return;
        g.next_h = h;
        Push(h, EventKind::Decide, -1, c, ++g.gen);
    }
    // Wakes him after his reaction time from `h`.
    void WakeAfterReaction(int c, double h, std::uint64_t salt) {
        if (c >= 0) Wake(c, h + ReactionOf(c, salt));
    }

    // ---- the closed form

    // A formation's rest rate over the hours since it was last touched, its cohesion regained, then its rest from now.
    void Settle(std::size_t i, double h) {
        Formation& f = forms_[i];
        if (!f.spent && h > f.settled_h) {
            ApplyLoss(f, LossOverHours(DailyLoss(data_->tables, f.rest, 1), h - f.settled_h), -1.0);
            f.unit.cohesion = std::min(1.0, f.unit.cohesion + data_->tables.cohesion_recovery_per_day * (h - f.settled_h) / 24.0);
        }
        f.settled_h = std::max(f.settled_h, h);
    }
    void SetRest(std::size_t i, LossAction rest, double h) {
        Settle(i, h);
        forms_[i].rest = rest;
    }

    // A share of strength lost: every holding and the people in proportion; `tank_share` >= 0 replaces the tanks' (table
    // 7-7). Tanks lost go to repair by the table's reparable share unless `repairable` is false (stragglers' left behind).
    void ApplyLoss(Formation& f, double share, double tank_share, bool repairable = true) {
        const double s = std::min(1.0, std::max(0.0, share));
        for (Holding& h : f.holdings) {
            const double ts = h.tank && tank_share >= 0.0 ? std::min(1.0, tank_share) : s;
            const double lost = h.count * ts;
            h.count -= lost;
            if (h.tank) {
                f.tanks_lost += lost;
                if (repairable) {
                    f.tanks_reparable += lost * data_->tables.reparable_share;
                    h.repair += lost * data_->tables.reparable_share;
                }
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
        // The hour's events: the clock's and the commanders'.
        while (!queue_.empty() && queue_.front().h <= h0 + 1e-9) {
            std::pop_heap(queue_.begin(), queue_.end(), Later());
            const Event e = queue_.back();
            queue_.pop_back();
            const bool report = IsPictureEvent(e.kind);
            if (report) Lap(GroundProfile::Decisions);
            Handle(e, h0, env, line);
            if (report) Lap(GroundProfile::Picture);
        }
        if (crisis_dirty_) {
            Lap(GroundProfile::Decisions);
            UpdateCrisis();
            Lap(GroundProfile::Picture);
        }
        Lap(GroundProfile::Decisions);
        // Phase 1: every axis's sides read from the hour's start.
        const std::size_t na = axes_.size();
        bool any = false;
        for (const Axis& x : axes_) any = any || x.status == AxisStatus::Attacking || x.status == AxisStatus::Counterattacked;
        if (any) {
            std::fill(shares_.begin(), shares_.end(), 0.0);
            std::fill(engaged_.begin(), engaged_.end(), static_cast<std::uint8_t>(0));
            std::fill(fought_.begin(), fought_.end(), static_cast<std::uint8_t>(0));
            for (std::size_t a = 0; a < na; ++a) {
                const Axis& x = axes_[a];
                if (x.status == AxisStatus::Attacking) {
                    for (int fi : x.defenders) shares_[static_cast<std::size_t>(fi)] += forms_[static_cast<std::size_t>(fi)].spent ? 0.0 : 1.0;
                    if (DepthEngaged(x)) {
                        for (int fi : x.depth_defenders) {
                            const Formation& f = forms_[static_cast<std::size_t>(fi)];
                            shares_[static_cast<std::size_t>(fi)] += f.spent || f.task == Task::Counter ? 0.0 : 1.0;
                        }
                    }
                } else if (x.status == AxisStatus::Counterattacked) {
                    for (int fi : x.first) shares_[static_cast<std::size_t>(fi)] += forms_[static_cast<std::size_t>(fi)].spent ? 0.0 : 1.0;
                }
            }
            for (std::size_t k = 0; k < na; ++k) {
                const std::size_t a = reverse_ ? na - 1 - k : k;
                if (axes_[a].status == AxisStatus::Attacking) Resolve(a, h0, env);
                if (axes_[a].status == AxisStatus::Counterattacked) ResolveCounter(a, h0, env);
            }
            Lap(GroundProfile::Combat);
            // Phase 2: every formation's losses, by formation in a fixed order, its axes' in theirs.
            for (std::size_t i = 0; i < forms_.size(); ++i) ApplyHour(i, h0);
            Lap(GroundProfile::Combat);
            for (std::size_t a = 0; a < na; ++a) {
                if (axes_[a].status == AxisStatus::Attacking) Advance(a, h0, env);
                if (axes_[a].status == AxisStatus::Counterattacked) PushBack(a, h0);
            }
            Lap(GroundProfile::Movement);
            AfterHour(h0 + 1.0);
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

    // The sector's second echelon fights once the enemy is through the security zone.
    bool DepthEngaged(const Axis& x) const { return x.through_zone || x.depth_m >= security_zone_m_; }
    bool InZone(const Axis& x) const { return !x.through_zone && x.depth_m < security_zone_m_; }

    void Handle(const Event& e, double h0, const GroundEnv& env, const airwar::LineOfContact& line) {
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
                offensive_h_ = h0;
                for (std::size_t a = 0; a < axes_.size(); ++a) {
                    Axis& x = axes_[a];
                    if (x.raid) continue;
                    x.status = AxisStatus::Attacking;
                    x.start_h = x.last_advance_h = h0;
                    for (int fi : x.first) StartAttack(static_cast<std::size_t>(fi), h0);
                    Note(h0, GroundNote::AxisStarts, static_cast<int>(a), -1, x.sector, x.objective_m);
                }
                // The tops follow their offensive and their defence from its start; the rebels' raids go by night.
                if (data_->enemy.plan.has_raids) {
                    const double night = NextNightStart(h0, env);
                    if (night >= 0.0 && top_[1] >= 0) Push(night, EventKind::RaidNight, -1, top_[1]);
                }
                bool offensive = false;
                for (const Axis& x : axes_) offensive = offensive || !x.raid;
                if (offensive) {
                    WakeAfterReaction(top_[1], h0, 1);
                    WakeAfterReaction(top_[0], h0, 1);
                }
                break;
            }
            case EventKind::Crossed: {
                Axis& x = axes_[static_cast<std::size_t>(e.axis)];
                if (x.crossing_until_h >= 0.0 && std::fabs(x.crossing_until_h - e.h) < 1e-6 && x.next_river < x.rivers.size()) {
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
                ReturnRepaired(h0);
                Push(NextMidnight(h0), EventKind::DayEnd);
                break;
            }
            case EventKind::Decide: {
                GroundCommander& g = cmdrs_[static_cast<std::size_t>(e.who)];
                if (e.gen != g.gen) break;  // superseded by an earlier wake
                g.next_h = -1.0;
                Decide(e.who, h0, line);
                break;
            }
            case EventKind::LineBack:
                LineBack(e.who, static_cast<std::size_t>(e.axis), h0);
                break;
            case EventKind::Arrive:
                Arrive(static_cast<std::size_t>(e.who), e.axis, e.task, h0, line);
                break;
            case EventKind::Reformed:
                Reform(static_cast<std::size_t>(e.who), h0);
                break;
            case EventKind::RaidNight:
                RaidNight(e.who, h0, env);
                break;
            case EventKind::RaidEnd:
                RaidEnd(h0, env);
                break;
            case EventKind::ReportsDue:
                ReportsDue(e.who == 0 ? Side::Ours : Side::Enemy, h0, env, line);
                break;
            case EventKind::IntelDue:
                IntelDue(h0);
                break;
            case EventKind::UnitReport: {
                UnitBelief b;
                b.fraction = e.v0;
                b.power = e.v1;
                b.observed_h = e.obs_h;
                b.factor = e.factor;
                b.sector = e.sector;
                b.posture = static_cast<Posture>(e.posture);
                b.source = static_cast<PictureSource>(e.src);
                b.out = e.flag != 0;
                if (pictures_[e.axis].Merge(static_cast<std::size_t>(e.who), b) && e.axis == 1) crisis_dirty_ = true;
                break;
            }
            case EventKind::AxisReport: {
                AxisBelief b;
                b.depth_m = e.v0;
                b.pace_km_day = e.v1;
                b.observed_h = e.obs_h;
                b.counterattacked = e.flag != 0;
                if (pictures_[e.who].MergeAxis(static_cast<std::size_t>(e.axis), b) && e.who == 1) crisis_dirty_ = true;
                break;
            }
            case EventKind::LineReport: {
                GroundPicture& p = pictures_[e.who];
                if (p.pending_line_h >= 0.0 && std::fabs(p.pending_line_h - e.obs_h) < 1e-6) {
                    std::copy(p.pending_line_m.begin(), p.pending_line_m.end(), p.line_m.begin());
                    p.line_observed_h = p.pending_line_h;
                    p.pending_line_h = -1.0;
                }
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

    void Hold(std::size_t i, double h1) {
        Formation& f = forms_[i];
        if (f.spent) return;
        Settle(i, h1);
        f.unit.posture = Posture::HastyDefence;
        f.rest = LossAction::Inactive;
    }

    // An attacker that attacks this hour: in the axis's list, not out, not stopped on its breakpoint.
    bool Attacks(const Formation& f) const { return !f.spent && !f.attack_stopped; }

    double AttackPower(const Axis& x) const {
        const GroundTables& t = data_->tables;
        double p = 0.0;
        for (int fi : x.first) {
            const Formation& f = forms_[static_cast<std::size_t>(fi)];
            if (Attacks(f)) p += UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion);
        }
        return p;
    }

    // Phase 1: one axis's hour, from the hour's start (nothing written but its own result and its engagements).
    void Resolve(std::size_t a, double h0, const GroundEnv& env) {
        const GroundTables& t = data_->tables;
        Axis& x = axes_[a];
        const std::size_t na = axes_.size();
        const bool in_zone = !x.raid && InZone(x);
        SectorSide att;
        att.power = AttackPower(x);
        att.posture = Posture::Attack;
        att.contact_day = static_cast<int>(std::floor((h0 - x.start_h) / 24.0));
        SectorSide def;
        def.posture = x.raid ? Posture::HastyDefence : in_zone ? Posture::Delay : Posture::PreparedDefence;
        def.covering = in_zone;
        int days = 0;
        bool thin = false;
        const auto add = [&](int fi, std::uint8_t zone) {
            const std::size_t i = static_cast<std::size_t>(fi);
            const Formation& f = forms_[i];
            if (f.spent || shares_[i] <= 0.0) return;
            const double share = (1.0 / shares_[i]) * (x.raid ? x.raid_share : zone == 1 ? t.security_zone_share : 1.0);
            def.power += UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion) * share;
            engaged_[i * na + a] = zone;
            part_[i * na + a] = share;
            thin = thin || f.thin;
            if (f.contact_from_h >= 0.0) days = std::max(days, static_cast<int>(std::floor((h0 - f.contact_from_h) / 24.0)));
        };
        for (int fi : x.defenders) add(fi, static_cast<std::uint8_t>(in_zone ? 1 : 2));
        if (!x.raid && DepthEngaged(x)) {
            for (int fi : x.depth_defenders) {
                if (forms_[static_cast<std::size_t>(fi)].task != Task::Counter) add(fi, static_cast<std::uint8_t>(2));
            }
        }
        def.contact_day = days;
        def.thin = thin;
        const std::uint8_t attacking = 3;
        for (int fi : x.first) {
            if (Attacks(forms_[static_cast<std::size_t>(fi)])) engaged_[static_cast<std::size_t>(fi) * na + a] = attacking;
        }
        SectorConditions c = Conditions(x, h0, env);
        double ahead_m = 1e18;
        if (x.next_river < x.rivers.size()) ahead_m = std::max(0.0, x.rivers[x.next_river].depth_m - x.depth_m);
        c.river_line = ahead_m <= 1000.0 * t.river_line_km;
        c.river_ahead_km = ahead_m / 1000.0;
        results_[a] = ResolveSector(t, att, def, c, data_->enemy.doctrine.caps, DayDraw(x, h0), 1.0);
        if (ahead_m < 1e17) results_[a].advance_km = std::min(results_[a].advance_km, ahead_m / 1000.0);
        x.last_ratio = results_[a].ratio;
        x.last_opposition = results_[a].opposition;
    }

    // Phase 1 under our counterattack: ours attack, the axis's attackers defend (in a meeting engagement while they were
    // attacking, in hasty defence once halted), NATO's covering force alone in delay while it screens.
    void ResolveCounter(std::size_t a, double h0, const GroundEnv& env) {
        const GroundTables& t = data_->tables;
        Axis& x = axes_[a];
        const std::size_t na = axes_.size();
        SectorSide att;
        att.posture = Posture::Attack;
        att.contact_day = static_cast<int>(std::floor((h0 - x.counter_start_h) / 24.0));
        for (int fi : x.counter) {
            const std::size_t i = static_cast<std::size_t>(fi);
            const Formation& f = forms_[i];
            if (f.spent) continue;
            att.power += UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion);
            engaged_[i * na + a] = 3;
        }
        SectorSide def;
        const int screen = Screen(x);
        def.posture = screen >= 0 ? Posture::Delay : (x.halted != HaltReason::None ? Posture::HastyDefence : Posture::Attack);
        def.covering = screen >= 0;
        for (int fi : x.first) {
            const std::size_t i = static_cast<std::size_t>(fi);
            const Formation& f = forms_[i];
            if (f.spent || shares_[i] <= 0.0 || (screen >= 0 && fi != screen)) continue;
            const double share = 1.0 / shares_[i];
            def.power += UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion) * share;
            engaged_[i * na + a] = 2;
            part_[i * na + a] = share;
        }
        def.contact_day = att.contact_day;
        const SectorConditions c = Conditions(x, h0, env);
        results_[a] = ResolveSector(t, att, def, c, data_->ours.doctrine.caps, DayDraw(x, h0), 1.0);
        x.last_ratio = results_[a].ratio;
        x.last_opposition = results_[a].opposition;
    }

    // NATO's covering force on an axis, screening while it holds (the plan's covering_force), or -1.
    int Screen(const Axis& x) const {
        const std::vector<std::string>& cf = data_->enemy.plan.covering_force;
        if (cf.empty()) return -1;
        for (int fi : x.first) {
            const Formation& f = forms_[static_cast<std::size_t>(fi)];
            if (!f.spent && std::find(cf.begin(), cf.end(), f.unit.id) != cf.end()) return fi;
        }
        return -1;
    }

    SectorConditions Conditions(const Axis& x, double h0, const GroundEnv& env) const {
        const GroundTables& t = data_->tables;
        SectorConditions c;
        const double step_m = 1000.0 * t.terrain_step_km;
        c.terrain = x.ground.empty() ? Terrain::Open : x.ground[std::min(x.ground.size() - 1, static_cast<std::size_t>(std::max(0.0, x.depth_m) / step_m))];
        if (env.theatre != nullptr) c.night = airwar::TheatrePartAt(*env.theatre, start_hour_ + h0 + 0.5) == airwar::DayPart::Night;
        const int day = static_cast<int>(std::floor((start_hour_ + h0) / 24.0));
        if (env.weather != nullptr && !env.weather->empty()) {
            const std::size_t w = static_cast<std::size_t>(std::min(std::max(day, 0), static_cast<int>(env.weather->size()) - 1));
            c.poor_visibility = (*env.weather)[w] == airwar::Weather::Poor;
        }
        return c;
    }

    // The sector's draw for the day: lognormal, mean 1, keyed by (sector, day).
    double DayDraw(Axis& x, double h0) const {
        const int day = static_cast<int>(std::floor((start_hour_ + h0) / 24.0));
        if (x.draw_day != day) {
            airwar::Rng r = root_.Fork((static_cast<std::uint64_t>(x.sector) + 1u) * 0x100000001B3ull + static_cast<std::uint64_t>(day + 100000));
            const double sigma = data_->tables.draw_sigma;
            x.draw = std::exp(sigma * r.Normal() - 0.5 * sigma * sigma);
            x.draw_day = day;
        }
        return x.draw;
    }

    // Phase 2: one formation's losses over the hour, from every axis it fought on, in the axes' order; its cohesion falls
    // with what it lost.
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
            const bool countered = axes_[a].status == AxisStatus::Counterattacked;
            if (z == 3) {
                share += r.attacker_loss;
            } else {
                share += r.defender_loss * part_[i * na + a];
                if (!countered && !axes_[a].raid) {
                    zone = std::max(zone, static_cast<int>(z) - 1);
                    ratio = std::max(ratio, r.ratio);
                }
            }
        }
        if (!fought) return;
        fought_[i] = 1;
        f.last_fought_h = h0;
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
        const double before = f.unit.strength;
        ApplyLoss(f, share, tank_share);
        const double lost = before > 0.0 ? 1.0 - f.unit.strength / before : 0.0;
        // Cohesion falls with the hour's losses and is regained at its daily rate in contact too: a steady grind wears
        // it slowly, heavy losses in hours break it (03 section 5.4).
        const GroundTables& t = data_->tables;
        f.unit.cohesion = std::min(1.0, std::max(0.0, f.unit.cohesion + t.cohesion_recovery_per_day / 24.0 - t.cohesion_loss_per_strength_loss * lost));
        f.settled_h = h0 + 1.0;
    }

    // The attacker's advance over the hour, stopped at a river's bank until it has crossed and at a security line until
    // our division orders the covering force back.
    void Advance(std::size_t a, double h0, const GroundEnv& env) {
        const GroundTables& t = data_->tables;
        Axis& x = axes_[a];
        const SectorResult& r = results_[a];
        double adv = 0.0;
        if (x.crossing_until_h < 0.0) {  // crossing: the bank holds it
            adv = std::max(0.0, r.advance_km * 1000.0);
            adv = std::min(adv, x.objective_m - x.depth_m);
            if (x.next_river < x.rivers.size()) adv = std::min(adv, std::max(0.0, x.rivers[x.next_river].depth_m - x.depth_m));
            const bool covering = !x.raid && InZone(x) && x.next_line < x.lines_m.size() && DefendersLeft(x);
            if (covering) adv = std::min(adv, std::max(0.0, x.lines_m[x.next_line] - x.depth_m));
            if (adv > 1.0) {
                x.depth_m += adv;
                x.last_advance_h = h0 + 1.0;
                moved_[a] = 1;
            }
            // The covering force's line reached: the division orders it back after the line's hours.
            if (covering && !x.line_pending && x.depth_m >= x.lines_m[x.next_line] - 1.0 && x.zone_cdr >= 0) {
                x.line_pending = true;
                const GroundCommander& g = cmdrs_[static_cast<std::size_t>(x.zone_cdr)];
                const double hold = SecurityLineHoursOf(t, data_->ours.doctrine, g.traits, DecisionDraw(x.zone_cdr, 0x11E0000ull + a * 64 + x.next_line));
                Push(h0 + 1.0 + hold, EventKind::LineBack, static_cast<int>(a), x.zone_cdr);
            }
            if (x.next_river < x.rivers.size() && x.depth_m >= x.rivers[x.next_river].depth_m - 1.0) {
                const RiverOnAxis& rv = x.rivers[x.next_river];
                bool intact = rv.road_bridge;
                if (rv.bridge_object >= 0 && env.truth != nullptr && static_cast<std::size_t>(rv.bridge_object) < env.truth->size()) {
                    intact = (*env.truth)[static_cast<std::size_t>(rv.bridge_object)].function >= 0.5;
                }
                x.crossing_until_h = h0 + 1.0 + CrossingHours(t, intact, false);
                Push(x.crossing_until_h, EventKind::Crossed, static_cast<int>(a));
                Note(h0 + 1.0, GroundNote::RiverReached, static_cast<int>(a), -1, x.sector, rv.depth_m, HaltReason::None, intact);
            }
        }
        Pace(x, adv);
    }

    // Our counterattack's hour: the axis pushed back, never past the border.
    void PushBack(std::size_t a, double h0) {
        Axis& x = axes_[a];
        const double back = std::min(std::max(0.0, results_[a].advance_km * 1000.0), x.depth_m);
        if (back > 1.0) {
            x.depth_m -= back;
            x.counter_advance_h = h0 + 1.0;
            moved_[a] = 1;
            // Rivers behind the new line are to cross again; a crossing under way is lost.
            while (x.next_river > 0 && x.rivers[x.next_river - 1].depth_m > x.depth_m + 1.0) --x.next_river;
            if (x.crossing_until_h >= 0.0) x.crossing_until_h = -1.0;
        }
        Pace(x, -back);
    }

    // The axis's pace: its advance averaged over about the pace window (an exponential average, km a day).
    void Pace(Axis& x, double adv_m) {
        const double w = std::max(1.0, data_->tables.pace_window_h);
        x.pace_km_day += (adv_m * 24.0 / 1000.0 - x.pace_km_day) / w;
    }

    bool DefendersLeft(const Axis& x) const {
        for (int fi : x.defenders) {
            if (!forms_[static_cast<std::size_t>(fi)].spent) return true;
        }
        return false;
    }

    // ---- after the hour: what crossed a line, what halts

    void AfterHour(double h1) {
        const GroundTables& t = data_->tables;
        // Formations that fought: cohesion gone breaks them; a line or breakpoint crossed wakes the commander.
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            if (fought_[i] == 0) continue;
            Formation& f = forms_[i];
            if (f.spent) continue;
            if (f.unit.cohesion < t.cohesion_break_below) {
                Disperse(i, h1, f.cmdr, 1.0);
                continue;
            }
            const std::uint8_t now = Crossings(i);
            if ((now & ~f.crossed) != 0) WakeAfterReaction(f.cmdr, h1, 0x5000ull + i);
            f.crossed = static_cast<std::uint8_t>(f.crossed | now);
            // Asked, no answer, held as long as he can: in contact again, he decides at once.
            if (f.asked && f.cmdr >= 0 && RelievedBy(i) < 0 && h1 - f.asked_h >= HoldWithoutOrderHoursOf(t, cmdrs_[static_cast<std::size_t>(f.cmdr)].traits) - 1e-9) Wake(f.cmdr, h1);
        }
        for (std::size_t a = 0; a < axes_.size(); ++a) {
            Axis& x = axes_[a];
            if (x.status == AxisStatus::Attacking) {
                if (!x.raid && !x.through_zone && x.depth_m >= security_zone_m_) {
                    x.through_zone = true;  // the penetration reaches the main defensive zone: our army hears of it
                    AxisAtOnce(Side::Ours, a, h1);
                    WakeAfterReaction(top_[0], h1, 0x6000ull + a);
                }
                bool attackers = false, stopped = false;
                for (int fi : x.first) {
                    const Formation& f = forms_[static_cast<std::size_t>(fi)];
                    attackers = attackers || Attacks(f);
                    stopped = stopped || (!f.spent && f.attack_stopped);
                }
                HaltReason why = HaltReason::None;
                if (x.depth_m >= x.objective_m - 1.0) {
                    why = HaltReason::Objective;
                } else if (!attackers && x.incoming == 0) {
                    why = stopped ? HaltReason::Breakpoint : HaltReason::Spent;
                }
                if (why != HaltReason::None && !x.raid) Halt(a, h1, why);
                if (why != HaltReason::None && x.raid && !attackers) EndRaid(a, h1);
            } else if (x.status == AxisStatus::Counterattacked) {
                bool ours = false;
                for (int fi : x.counter) ours = ours || !forms_[static_cast<std::size_t>(fi)].spent;
                if (!ours) {
                    EndCounter(a, h1);
                } else if (x.depth_m <= 1.0) {
                    EndCounter(a, h1);  // back at the border: we stop there
                }
            }
        }
    }

    // The lines and breakpoints a formation is under now (by its own commander's count).
    std::uint8_t Crossings(std::size_t i) const {
        const Formation& f = forms_[i];
        const EffectivenessDef& e = Doctrine(f.unit.side).effectiveness;
        const double frac = OwnFraction(i);  // its own commander's count
        std::uint8_t c = 0;
        if (e.kind == EffectivenessKind::Lines) {
            const double ask = f.cmdr >= 0 ? AskWithdrawLineOf(data_->tables, e, cmdrs_[static_cast<std::size_t>(f.cmdr)].traits) : e.ineffective_below;
            if (frac < e.degraded_below) c = static_cast<std::uint8_t>(c | kCrossDegraded);
            if (frac < ask) c = static_cast<std::uint8_t>(c | kCrossAsk);
        } else {
            if (1.0 - frac >= f.attack_stop) c = static_cast<std::uint8_t>(c | kCrossAttackStop);
            if (1.0 - frac >= f.defence_break) c = static_cast<std::uint8_t>(c | kCrossDefenceBreak);
        }
        return c;
    }

    void Halt(std::size_t a, double h1, HaltReason why) {
        Axis& x = axes_[a];
        x.status = AxisStatus::Halted;
        x.halted = why;
        // The sector goes quiet: its attackers hold where they are, in contact.
        for (int fi : x.first) Hold(static_cast<std::size_t>(fi), h1);
        for (int fi : x.defenders) {
            if (!forms_[static_cast<std::size_t>(fi)].spent) SetRest(static_cast<std::size_t>(fi), LossAction::Inactive, h1);
        }
        for (int fi : x.depth_defenders) {
            const Formation& f = forms_[static_cast<std::size_t>(fi)];
            if (!f.spent && f.contact_from_h >= 0.0) SetRest(static_cast<std::size_t>(fi), LossAction::Inactive, h1);
        }
        Note(h1, GroundNote::AxisHalted, static_cast<int>(a), -1, x.sector, x.depth_m, why);
        AxisAtOnce(Side::Enemy, a, h1);
        WakeAfterReaction(top_[1], h1, 0x7000ull + a);  // the senior hears of it: his echelons may go elsewhere
    }

    // ---- the commanders' decisions

    void Decide(int c, double h, const airwar::LineOfContact& line) {
        GroundCommander& g = cmdrs_[static_cast<std::size_t>(c)];
        ++g.decisions;
        ++profile_.decisions;
        if (g.formation >= 0) UnitDecision(c, h);
        if (g.side == Side::Enemy) {
            AnswerRequests(c, h, line);
            if (g.top) EnemyTop(c, h, line);
        } else {
            AnswerRequests(c, h, line);
            if (g.top) OurTop(c, h, line);
        }
    }

    // His own formation's state (commanders.hpp DecideUnit).
    void UnitDecision(int c, double h) {
        const GroundCommander& g = cmdrs_[static_cast<std::size_t>(c)];
        const std::size_t i = static_cast<std::size_t>(g.formation);
        Formation& f = forms_[i];
        if (f.spent) return;
        const GroundTables& t = data_->tables;
        const GroundDoctrineDef& d = Doctrine(f.unit.side);
        UnitSituation s;
        s.lines = d.effectiveness.kind == EffectivenessKind::Lines;
        s.fraction = OwnFraction(i);  // he knows his own unit
        s.ask_line = AskWithdrawLineOf(t, d.effectiveness, g.traits);
        s.degraded_line = d.effectiveness.degraded_below;
        s.attack_stop = f.attack_stop;
        s.defence_break = f.defence_break;
        const int on = f.axis >= 0 ? f.axis : f.raid_axis;
        s.attacking = f.unit.side == Side::Enemy && on >= 0 && !f.second && axes_[static_cast<std::size_t>(on)].status == AxisStatus::Attacking &&
                      std::find(axes_[static_cast<std::size_t>(on)].first.begin(), axes_[static_cast<std::size_t>(on)].first.end(), static_cast<int>(i)) !=
                          axes_[static_cast<std::size_t>(on)].first.end();
        s.attack_stopped = f.attack_stopped;
        s.engaged = f.last_fought_h >= h - 1.0 - 1e-9;
        s.asked = f.asked;
        s.asked_for_h = f.asked && RelievedBy(i) < 0 ? h - f.asked_h : 0.0;  // a relief on its way is his answer: hold until it comes
        s.hold_h = HoldWithoutOrderHoursOf(t, g.traits);
        s.needs_order = d.withdraw_needs_order;
        s.is_top = g.top;
        s.disperse_not_retreat = d.disperse_rather_than_retreat;
        s.reported = f.reported;
        const UnitAct act = DecideUnit(s);
        switch (act) {
            case UnitAct::None:
                break;
            case UnitAct::Report:
                f.reported = true;
                Order(h, GroundOrder::Reported, c, static_cast<int>(i), f.axis, s.fraction);
                ReportAtOnce(i, h, AxisOfFormation(i));
                WakeAfterReaction(g.senior, h, 0x8000ull + i);
                break;
            case UnitAct::Ask:
                f.asked = true;
                f.asked_h = h;
                Order(h, GroundOrder::WithdrawAsked, c, static_cast<int>(i), AxisOfFormation(i), s.fraction);
                ReportAtOnce(i, h, AxisOfFormation(i));
                WakeAfterReaction(g.senior, h, 0x9000ull + i);
                break;
            case UnitAct::Withdraw:
                Order(h, GroundOrder::WithdrawDecided, c, static_cast<int>(i), AxisOfFormation(i), s.fraction);
                Withdraw(i, h);
                break;
            case UnitAct::Disperse:
                Disperse(i, h, c, s.disperse_not_retreat ? 2.0 : 0.0);
                break;
            case UnitAct::StopAttack:
                f.attack_stopped = true;
                Hold(i, h);
                Order(h, GroundOrder::AttackStopped, c, static_cast<int>(i), f.axis, s.fraction);
                ReportAtOnce(i, h, f.axis);
                WakeAfterReaction(g.senior, h, 0xA000ull + i);
                break;
        }
        // Waiting on an answer under the line: he looks again when he can hold no longer (after that, when he next fights).
        if (!f.spent && f.asked && RelievedBy(i) < 0 && f.asked_h + s.hold_h > h + 1e-9) Wake(c, f.asked_h + s.hold_h);
    }

    // The axis a formation fights on (an attacker's, or the first an our formation defends), -1 none.
    int AxisOfFormation(std::size_t i) const {
        const Formation& f = forms_[i];
        if (f.unit.side == Side::Enemy) return f.axis >= 0 ? f.axis : f.raid_axis;
        for (std::size_t a = 0; a < axes_.size(); ++a) {
            const Axis& x = axes_[a];
            if (std::find(x.defenders.begin(), x.defenders.end(), static_cast<int>(i)) != x.defenders.end()) return static_cast<int>(a);
            if (std::find(x.depth_defenders.begin(), x.depth_defenders.end(), static_cast<int>(i)) != x.depth_defenders.end()) return static_cast<int>(a);
            if (std::find(x.counter.begin(), x.counter.end(), static_cast<int>(i)) != x.counter.end()) return static_cast<int>(a);
        }
        return -1;
    }

    // A senior's answers to his units' requests to withdraw (03 sections 5.2-5.4): the enemy's senior permits (its doctrine
    // withdraws and re-forms the ineffective); ours permits behind the next, sends a relief, or refuses.
    void AnswerRequests(int c, double h, const airwar::LineOfContact& line) {
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Formation& f = forms_[i];
            if (f.spent || !f.asked || f.cmdr < 0 || cmdrs_[static_cast<std::size_t>(f.cmdr)].senior != c || f.relief_for >= 0) continue;
            if (RelievedBy(i) >= 0) continue;  // a relief is on its way
            const bool ours = f.unit.side == Side::Ours;
            const int behind = ours ? NextBehind(i) : -1;
            const int relief = ours && behind < 0 ? FreeFormation() : -1;
            switch (AnswerRequest(behind >= 0, relief >= 0, !ours)) {
                case RequestAnswer::Permit:
                    Order(h, GroundOrder::WithdrawOrdered, c, static_cast<int>(i), AxisOfFormation(i), SeenFraction(f.unit.side, i), behind);
                    Withdraw(i, h);
                    break;
                case RequestAnswer::Relieve: {
                    Formation& r = forms_[static_cast<std::size_t>(relief)];
                    const int a = AxisOfFormation(i);
                    r.free = false;
                    r.task = Task::Relief;
                    r.task_axis = a;
                    r.relief_for = static_cast<int>(i);
                    r.unit.state = UnitState::March;
                    const double at = h + PlanningOf(c, 0xB000ull + i) + (a >= 0 ? MarchHours(r, static_cast<std::size_t>(a), line) : 0.0);
                    Push(at, EventKind::Arrive, a, relief, 0, Task::Relief);
                    Order(h, GroundOrder::ReliefOrdered, c, relief, a, at - h, static_cast<int>(i));
                    break;
                }
                case RequestAnswer::Refuse:
                    if (!f.refused) Order(h, GroundOrder::WithdrawRefused, c, static_cast<int>(i), AxisOfFormation(i), SeenFraction(f.unit.side, i));
                    f.refused = true;
                    break;
            }
        }
    }

    int RelievedBy(std::size_t i) const {
        for (std::size_t k = 0; k < forms_.size(); ++k) {
            if (forms_[k].relief_for == static_cast<int>(i) && forms_[k].task == Task::Relief) return static_cast<int>(k);
        }
        return -1;
    }

    // Another of ours holding the same ground behind or beside `i` (the sector's second echelon engaged in depth, or
    // another defender there), -1 none.
    int NextBehind(std::size_t i) const {
        for (const Axis& x : axes_) {
            if (x.status != AxisStatus::Attacking && x.status != AxisStatus::Halted) continue;
            const bool holds = std::find(x.defenders.begin(), x.defenders.end(), static_cast<int>(i)) != x.defenders.end();
            if (!holds) continue;
            for (int fi : x.defenders) {
                if (fi != static_cast<int>(i) && !forms_[static_cast<std::size_t>(fi)].spent) return fi;
            }
            if (DepthEngaged(x)) {
                for (int fi : x.depth_defenders) {
                    const Formation& o = forms_[static_cast<std::size_t>(fi)];
                    if (fi != static_cast<int>(i) && !o.spent && o.task != Task::Counter) return fi;
                }
            }
        }
        return -1;
    }

    // Our first formation free for the army commander to send (a reserve or re-formed), -1 none.
    int FreeFormation() const {
        for (std::size_t k = 0; k < forms_.size(); ++k) {
            const Formation& f = forms_[k];
            if (f.unit.side == Side::Ours && f.free && !f.spent && f.task == Task::None) return static_cast<int>(k);
        }
        return -1;
    }

    // Where an axis's fight is now.
    Vec3 AxisPoint(const Axis& x, const airwar::LineOfContact& line) const {
        return line.PointAt(x.along_m) + line.InwardNormal(x.along_m) * x.depth_m;
    }

    // A march's hours at U0: the straight line's length by the road factor at the column's day speed (F3.6 asks the roads).
    double MarchHours(const Formation& f, std::size_t a, const airwar::LineOfContact& line) const {
        const GroundTables& t = data_->tables;
        const double kmh = t.road_day_kmh.Mid();
        if (!(kmh > 0.0)) return 0.0;
        return airwar::Dist2d(f.pos, AxisPoint(axes_[a], line)) / 1000.0 * t.march_road_factor / kmh;
    }

    // Sum of the power of an axis's attackers (and an echelon to come) over its defenders', by `by`'s picture.
    double AxisRatio(Side by, std::size_t a, double extra_attack) const {
        const Axis& x = axes_[a];
        double att = extra_attack, def = 0.0;
        for (int fi : x.first) {
            if (Attacks(forms_[static_cast<std::size_t>(fi)])) att += SeenPower(by, static_cast<std::size_t>(fi));
        }
        for (int fi : x.defenders) def += SeenPower(by, static_cast<std::size_t>(fi));
        if (DepthEngaged(x)) {
            for (int fi : x.depth_defenders) def += SeenPower(by, static_cast<std::size_t>(fi));
        }
        for (int fi : x.counter) def += SeenPower(by, static_cast<std::size_t>(fi));
        return def > 0.0 ? att / def : (att > 0.0 ? 1e9 : 0.0);
    }

    // The enemy's top (the Pact's army, NATO's corps, the neutral's division): echelons, stalls, resumptions.
    void EnemyTop(int c, double h, const airwar::LineOfContact& line) {
        const GroundTables& t = data_->tables;
        const GroundDoctrineDef& d = data_->enemy.doctrine;
        const GroundCommander& g = cmdrs_[static_cast<std::size_t>(c)];
        const double attack_ratio = AttackRatioOf(t, d, g.traits);
        const std::size_t na = axes_.size();
        // The echelons: committed when due (the plan's hour, or their axis's first echelon worn or stopped).
        const bool time_due = offensive_h_ >= 0.0 && h - offensive_h_ >= t.second_echelon_after_h - 1e-9;
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Formation& f = forms_[i];
            if (f.unit.side != Side::Enemy || !f.second || f.spent || f.task != Task::None) continue;
            bool due = time_due;
            for (std::size_t a = 0; a < na && !due; ++a) due = !axes_[a].raid && axes_[a].status != AxisStatus::Waiting && AxisWorn(a);
            if (!due) continue;
            const double power = SeenPower(Side::Enemy, i);
            for (std::size_t a = 0; a < na; ++a) {
                const Axis& x = axes_[a];
                AxisChoice& ch = choices_[a];
                ch.attacking = !x.raid && x.status == AxisStatus::Attacking;
                ch.open = !x.raid && (x.status == AxisStatus::Attacking ||
                                      (x.status == AxisStatus::Halted && x.halted != HaltReason::Objective));
                ch.pace_km_day = SeenPace(Side::Enemy, a);
                ch.ratio_with = ch.open ? AxisRatio(Side::Enemy, a, power) : 0.0;
            }
            const int target = EchelonTarget(choices_.data(), na, f.plan_axis, d.second_echelon_to_success, t.moving_km_per_day, attack_ratio);
            if (target < 0) {
                if (!f.held_noted) Order(h, GroundOrder::EchelonHeld, c, static_cast<int>(i), f.plan_axis);
                f.held_noted = true;
                continue;
            }
            // Committed: it marches to the axis (from its plan's axis's list if it was there).
            if (f.plan_axis >= 0) {
                std::vector<int>& sec = axes_[static_cast<std::size_t>(f.plan_axis)].second;
                sec.erase(std::remove(sec.begin(), sec.end(), static_cast<int>(i)), sec.end());
            }
            f.task = Task::Echelon;
            f.task_axis = target;
            f.second = false;
            f.unit.state = UnitState::March;
            ++axes_[static_cast<std::size_t>(target)].incoming;
            const double at = h + PlanningOf(c, 0xC000ull + i) + MarchHours(f, static_cast<std::size_t>(target), line);
            Push(at, EventKind::Arrive, target, static_cast<int>(i), 0, Task::Echelon);
            Order(h, GroundOrder::EchelonCommitted, c, static_cast<int>(i), target, at - h, f.plan_axis);
        }
        // Stalled axes with nothing coming: stopped. Halted ones our counterattack left: resumed at the attack ratio.
        const double stall_h = StallHoursOf(t, g.traits);
        for (std::size_t a = 0; a < na; ++a) {
            Axis& x = axes_[a];
            if (x.raid) continue;
            if (x.status == AxisStatus::Attacking && x.incoming == 0 && x.crossing_until_h < 0.0 && h - x.last_advance_h >= stall_h - 1e-9) {
                Order(h, GroundOrder::AxisStopped, c, -1, static_cast<int>(a), x.depth_m);
                Halt(a, h, HaltReason::Stalled);
            } else if (x.status == AxisStatus::Halted && x.halted == HaltReason::Counterattacked && x.incoming == 0 &&
                       AxisRatio(Side::Enemy, a, 0.0) >= attack_ratio) {
                Resume(a, h);
                Order(h, GroundOrder::AttackResumed, c, -1, static_cast<int>(a), x.depth_m);
            }
        }
        if (EnemyActive()) WakeAfterReaction(c, h, 0xD000ull);
    }

    // An axis whose attackers are worn: one under its degraded line, stopped on its breakpoint, or none left.
    bool AxisWorn(std::size_t a) const {
        const Axis& x = axes_[a];
        bool any = false;
        for (int fi : x.first) {
            const Formation& f = forms_[static_cast<std::size_t>(fi)];
            if (f.spent || f.attack_stopped || (f.crossed & kCrossDegraded) != 0) return true;
            any = true;
        }
        return !any;
    }

    bool EnemyActive() const {
        for (const Axis& x : axes_) {
            if (x.raid) continue;
            if (x.status == AxisStatus::Attacking || x.status == AxisStatus::Counterattacked || x.incoming > 0) return true;
            if (x.status == AxisStatus::Halted && x.halted == HaltReason::Counterattacked) return true;
        }
        for (const Formation& f : forms_) {
            if (f.unit.side == Side::Enemy && f.second && !f.spent && f.task == Task::None && !f.held_noted) return true;
        }
        return false;
    }

    void Resume(std::size_t a, double h) {
        Axis& x = axes_[a];
        x.status = AxisStatus::Attacking;
        x.halted = HaltReason::None;
        x.last_advance_h = h;
        for (int fi : x.first) {
            if (Attacks(forms_[static_cast<std::size_t>(fi)])) StartAttack(static_cast<std::size_t>(fi), h);
        }
        for (int fi : x.defenders) {
            if (!forms_[static_cast<std::size_t>(fi)].spent) Settle(static_cast<std::size_t>(fi), h);
        }
        AxisAtOnce(Side::Enemy, a, h);
    }

    // Our army commander: counterattacks into a penetration of the main defensive zone, and stops one that no longer gains.
    void OurTop(int c, double h, const airwar::LineOfContact& line) {
        const GroundTables& t = data_->tables;
        const GroundCommander& g = cmdrs_[static_cast<std::size_t>(c)];
        const std::size_t na = axes_.size();
        // A counterattack that has not gained ground over his reaction time stops: its force holds where it is.
        const double gain_h = DecisionHours(t, g.echelon, false, 0.0);
        for (std::size_t a = 0; a < na; ++a) {
            Axis& x = axes_[a];
            if (x.status == AxisStatus::Counterattacked && h - x.counter_advance_h >= gain_h - 1e-9) EndCounter(a, h);
        }
        // The deepest penetration of the main zone with no counterattack on it, if his force is strong enough.
        int best = -1;
        for (std::size_t a = 0; a < na; ++a) {
            const Axis& x = axes_[a];
            if (x.raid || !x.through_zone || x.counter_incoming > 0 || !x.counter.empty()) continue;
            if (x.counter_end_h >= 0.0 && x.last_advance_h <= x.counter_end_h + 1e-9) continue;  // not again until he moves
            if (x.status != AxisStatus::Attacking && x.status != AxisStatus::Halted) continue;
            if (best < 0 || SeenDepth(Side::Ours, a) > SeenDepth(Side::Ours, static_cast<std::size_t>(best))) best = static_cast<int>(a);
        }
        if (best >= 0) {
            const std::size_t a = static_cast<std::size_t>(best);
            Axis& x = axes_[a];
            double ours = 0.0, theirs = 0.0;
            for (std::size_t i = 0; i < forms_.size(); ++i) {
                if (CounterCandidate(i, x)) ours += SeenPower(Side::Ours, i);
            }
            for (int fi : x.first) theirs += SeenPower(Side::Ours, static_cast<std::size_t>(fi));
            const double ratio = theirs > 0.0 ? ours / theirs : (ours > 0.0 ? 1e9 : 0.0);
            if (ours > 0.0 && WantsCounterattack(ratio, CounterattackRatioOf(t, g.traits))) {
                const double plan_h = PlanningOf(c, 0xE000ull + a);
                for (std::size_t i = 0; i < forms_.size(); ++i) {
                    if (!CounterCandidate(i, x)) continue;
                    Formation& f = forms_[i];
                    f.free = false;
                    f.task = Task::Counter;
                    f.task_axis = best;
                    f.unit.state = UnitState::March;
                    ++x.counter_incoming;
                    Push(h + plan_h + MarchHours(f, a, line), EventKind::Arrive, best, static_cast<int>(i), 0, Task::Counter);
                }
                Order(h, GroundOrder::CounterattackOrdered, c, -1, best, ratio);
            }
        }
        if (OursActive()) WakeAfterReaction(c, h, 0xF000ull);
    }

    // Ours that may counterattack on an axis: free (a reserve, re-formed), or the sector's second echelon.
    bool CounterCandidate(std::size_t i, const Axis& x) const {
        const Formation& f = forms_[i];
        if (f.unit.side != Side::Ours || f.spent || f.task != Task::None) return false;
        if (f.role == DeployRole::Support) return false;
        if (f.free) return true;
        return std::find(x.depth_defenders.begin(), x.depth_defenders.end(), static_cast<int>(i)) != x.depth_defenders.end();
    }

    bool OursActive() const {
        for (const Axis& x : axes_) {
            if (x.raid) continue;
            if (x.status == AxisStatus::Attacking || x.status == AxisStatus::Counterattacked || x.counter_incoming > 0) return true;
        }
        return false;
    }

    // Our counterattack ends: its force goes over to the defence on the axis; the enemy's senior decides whether to resume.
    void EndCounter(std::size_t a, double h) {
        Axis& x = axes_[a];
        for (int fi : x.counter) {
            Formation& f = forms_[static_cast<std::size_t>(fi)];
            f.task = Task::None;
            f.task_axis = -1;
            if (f.spent) continue;
            f.unit.posture = Posture::HastyDefence;
            f.unit.state = UnitState::InLine;
            if (std::find(x.defenders.begin(), x.defenders.end(), fi) == x.defenders.end() &&
                std::find(x.depth_defenders.begin(), x.depth_defenders.end(), fi) == x.depth_defenders.end()) {
                x.defenders.push_back(fi);
            }
        }
        x.counter.clear();
        x.counter_end_h = h;
        bool attackers = false;
        for (int fi : x.first) attackers = attackers || !forms_[static_cast<std::size_t>(fi)].spent;
        x.status = AxisStatus::Halted;
        x.halted = attackers ? HaltReason::Counterattacked : HaltReason::Spent;
        for (int fi : x.first) Hold(static_cast<std::size_t>(fi), h);
        Order(h, GroundOrder::CounterattackEnds, top_[0], -1, static_cast<int>(a), x.depth_m);
        AxisAtOnce(Side::Ours, a, h);
        AxisAtOnce(Side::Enemy, a, h);
        WakeAfterReaction(top_[1], h, 0x7100ull + a);
    }

    // ---- orders carried out

    // A march done: an echelon joins its axis's attack, a counterattack force goes in, a relief takes over.
    void Arrive(std::size_t i, int a, Task task, double h, const airwar::LineOfContact& line) {
        Formation& f = forms_[i];
        if (a < 0) {
            f.task = Task::None;
            return;
        }
        Axis& x = axes_[static_cast<std::size_t>(a)];
        switch (task) {
            case Task::None:
                break;
            case Task::Echelon:
                --x.incoming;
                f.task = Task::None;
                if (f.spent) break;
                f.axis = a;
                f.pos = AxisPoint(x, line);
                if (std::find(x.first.begin(), x.first.end(), static_cast<int>(i)) == x.first.end()) x.first.push_back(static_cast<int>(i));
                if (x.status == AxisStatus::Halted && x.halted != HaltReason::Objective) {
                    Resume(static_cast<std::size_t>(a), h);
                    Order(h, GroundOrder::AttackResumed, top_[1], static_cast<int>(i), a, x.depth_m);
                }
                if (x.status == AxisStatus::Attacking) {
                    StartAttack(i, h);
                } else {
                    Hold(i, h);
                    f.unit.state = UnitState::InLine;
                }
                break;
            case Task::Counter:
                --x.counter_incoming;
                if (f.spent || f.task != Task::Counter) break;
                f.pos = AxisPoint(x, line);
                if (x.status != AxisStatus::Attacking && x.status != AxisStatus::Halted && x.status != AxisStatus::Counterattacked) {
                    f.task = Task::None;
                    f.free = true;
                    break;
                }
                Settle(i, h);
                f.unit.posture = Posture::Attack;
                f.unit.state = UnitState::InLine;
                x.counter.push_back(static_cast<int>(i));
                if (x.status != AxisStatus::Counterattacked) {
                    for (int fi : x.first) Settle(static_cast<std::size_t>(fi), h);
                    x.status = AxisStatus::Counterattacked;
                    x.counter_start_h = x.counter_advance_h = h;
                    Order(h, GroundOrder::CounterattackStarts, top_[0], static_cast<int>(i), a, x.depth_m);
                    AxisAtOnce(Side::Ours, static_cast<std::size_t>(a), h);
                    AxisAtOnce(Side::Enemy, static_cast<std::size_t>(a), h);
                    WakeAfterReaction(top_[1], h, 0x7200ull + static_cast<std::uint64_t>(a));
                }
                break;
            case Task::Relief: {
                f.task = Task::None;
                const int old = f.relief_for;
                f.relief_for = -1;
                if (f.spent) break;
                f.pos = AxisPoint(x, line);
                f.unit.state = UnitState::InLine;
                f.unit.posture = Posture::PreparedDefence;
                Settle(i, h);
                f.rest = LossAction::Inactive;
                if (old >= 0) {
                    const Formation& o = forms_[static_cast<std::size_t>(old)];
                    f.first_sector = o.first_sector;
                    f.last_sector = o.last_sector;
                    for (Axis& y : axes_) {
                        const bool in_def = std::find(y.defenders.begin(), y.defenders.end(), old) != y.defenders.end();
                        const bool in_depth = std::find(y.depth_defenders.begin(), y.depth_defenders.end(), old) != y.depth_defenders.end();
                        if (in_def && std::find(y.defenders.begin(), y.defenders.end(), static_cast<int>(i)) == y.defenders.end()) {
                            y.defenders.push_back(static_cast<int>(i));
                        }
                        if (in_depth && std::find(y.depth_defenders.begin(), y.depth_defenders.end(), static_cast<int>(i)) == y.depth_defenders.end()) {
                            y.depth_defenders.push_back(static_cast<int>(i));
                        }
                    }
                    Order(h, GroundOrder::Relieved, f.cmdr, old, a, 0.0, static_cast<int>(i));
                    if (!o.spent) Withdraw(static_cast<std::size_t>(old), h);
                }
                if (std::find(x.defenders.begin(), x.defenders.end(), static_cast<int>(i)) == x.defenders.end() &&
                    std::find(x.depth_defenders.begin(), x.depth_defenders.end(), static_cast<int>(i)) == x.depth_defenders.end()) {
                    x.defenders.push_back(static_cast<int>(i));  // the unit it came for has gone: it holds the axis itself
                    if (f.first_sector < 0) f.first_sector = f.last_sector = x.sector;
                }
                break;
            }
        }
    }

    // Out of every axis's lists.
    void LeaveLists(int fi) {
        for (Axis& x : axes_) {
            x.first.erase(std::remove(x.first.begin(), x.first.end(), fi), x.first.end());
            x.second.erase(std::remove(x.second.begin(), x.second.end(), fi), x.second.end());
            x.defenders.erase(std::remove(x.defenders.begin(), x.defenders.end(), fi), x.defenders.end());
            x.depth_defenders.erase(std::remove(x.depth_defenders.begin(), x.depth_defenders.end(), fi), x.depth_defenders.end());
            x.counter.erase(std::remove(x.counter.begin(), x.counter.end(), fi), x.counter.end());
        }
    }

    // Retreat as a unit: ordered (or decided), cohesive, keeping its strength; out of the line to re-form behind.
    void Withdraw(std::size_t i, double h) {
        Formation& f = forms_[i];
        if (f.spent) return;
        const int on = AxisOfFormation(i);
        Settle(i, h);
        f.spent = true;
        f.unit.state = UnitState::Reforming;
        f.unit.posture = Posture::Withdrawal;
        f.rest = LossAction::Reserve;
        f.task = Task::None;
        f.relief_for = -1;
        ++f.withdrawals;
        ReportAtOnce(i, h, on);
        LeaveLists(static_cast<int>(i));
        const double days = data_->tables.reform_days.At(root_.Fork(0x2EF0000000ull + i * 64 + static_cast<std::size_t>(f.withdrawals)).Uniform());
        Push(h + 24.0 * days, EventKind::Reformed, -1, static_cast<int>(i));
    }

    // Retreat dispersed: it breaks, leaves the line, a share of what is left lost as stragglers; its remnants reassemble
    // behind in 1-3 days (03 section 5.4). `why`: 0 no order came, 1 its cohesion went, 2 a rebel group's way.
    void Disperse(std::size_t i, double h, int cmdr, double why) {
        Formation& f = forms_[i];
        if (f.spent) return;
        Settle(i, h);
        Order(h, GroundOrder::Dispersed, cmdr, static_cast<int>(i), AxisOfFormation(i), why);
        const int on = AxisOfFormation(i);
        ApplyLoss(f, data_->tables.straggler_loss, -1.0, false);
        f.spent = true;
        f.unit.state = UnitState::Dispersed;
        f.unit.posture = Posture::Withdrawal;
        f.rest = LossAction::Reserve;
        f.task = Task::None;
        f.relief_for = -1;
        ++f.dispersals;
        ReportAtOnce(i, h, on);
        LeaveLists(static_cast<int>(i));
        const double days = data_->tables.reassemble_days.At(root_.Fork(0xD15E000000ull + i * 64 + static_cast<std::size_t>(f.dispersals)).Uniform());
        Push(h + 24.0 * days, EventKind::Reformed, -1, static_cast<int>(i));
    }

    // Re-formed behind as a smaller unit: its establishment what it has now, its cohesion whole; free for its side's top.
    void Reform(std::size_t i, double h) {
        Formation& f = forms_[i];
        if (!f.spent) return;
        f.spent = false;
        f.settled_h = h;
        f.unit.state = UnitState::Reserve;
        f.unit.posture = Posture::HastyDefence;
        f.unit.establishment = std::min(f.base_establishment, std::max(f.unit.strength, 1e-9));
        f.unit.cohesion = 1.0;
        f.crossed = 0;
        f.reported = f.asked = f.refused = f.attack_stopped = f.held_noted = false;
        f.asked_h = -1.0;
        f.zone = -1;
        f.commitment = -1;
        Order(h, GroundOrder::Reformed, f.cmdr, static_cast<int>(i), -1, f.base_establishment > 0.0 ? f.unit.strength / f.base_establishment : 0.0);
        ReportAtOnce(i, h, -1);
        if (f.unit.side == Side::Enemy) {
            if (f.raid_axis >= 0) return;  // a rebel group back on its line
            f.axis = -1;
            f.second = true;
            WakeAfterReaction(top_[1], h, 0x2EF1000000ull + i);
        } else {
            f.free = true;
            f.first_sector = f.last_sector = -1;
            WakeAfterReaction(top_[0], h, 0x2EF2000000ull + i);
        }
    }

    // The division's order: the covering force back from its line to the next (or into the main zone).
    void LineBack(int c, std::size_t a, double h) {
        Axis& x = axes_[a];
        if (!x.line_pending) return;
        x.line_pending = false;
        ++x.next_line;
        const double to_m = x.next_line < x.lines_m.size() ? x.lines_m[x.next_line] : security_zone_m_;
        if (DefendersLeft(x) && to_m < x.objective_m) Order(h, GroundOrder::CoveringBack, c, -1, static_cast<int>(a), to_m);
    }

    // Tanks back from repair at the day's end (a share of those in repair), to formations not dispersed.
    void ReturnRepaired(double h) {
        const double rate = data_->tables.repair_return_per_day;
        if (!(rate > 0.0)) return;
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            Formation& f = forms_[i];
            if (f.unit.state == UnitState::Dispersed) continue;
            bool any = false;
            for (const Holding& hd : f.holdings) any = any || hd.repair > 1e-9;
            if (!any) continue;
            Settle(i, h);
            for (Holding& hd : f.holdings) {
                const double back = hd.repair * rate;
                hd.count += back;
                hd.repair -= back;
                f.tanks_returned += back;
            }
            f.unit.strength = Strength(f);
        }
    }

    // ---- the rebels' raids (03 section 8)

    bool NightAt(const GroundEnv& env, double h) const {
        return airwar::TheatrePartAt(*env.theatre, start_hour_ + h + 0.5) == airwar::DayPart::Night;
    }
    // The first hour from `h` a night begins (with night_only false: now); -1 if the theatre has no clock.
    double NextNightStart(double h, const GroundEnv& env) const {
        if (!data_->enemy.plan.raids.night_only) return h;
        if (env.theatre == nullptr) return -1.0;
        for (int k = 0; k < 48; ++k) {
            const double t = h + static_cast<double>(k);
            if (NightAt(env, t) && !NightAt(env, t - 1.0)) return t;
        }
        return -1.0;
    }
    double NightEnd(double h, const GroundEnv& env) const {
        if (!data_->enemy.plan.raids.night_only || env.theatre == nullptr) return h + 12.0;
        for (int k = 1; k < 48; ++k) {
            const double t = h + static_cast<double>(k);
            if (!NightAt(env, t)) return t;
        }
        return h + 12.0;
    }

    // The rebels' commander at nightfall: raids where his group's weight over the share of our line it meets reaches his
    // ratio, the best first, as many as a night's raids.
    void RaidNight(int c, double h, const GroundEnv& env) {
        const GroundTables& t = data_->tables;
        const RaidsDef& raids = data_->enemy.plan.raids;
        GroundCommander& g = cmdrs_[static_cast<std::size_t>(c)];
        ++g.decisions;
        ++profile_.decisions;
        ValueRange band = t.attack_ratio_range;
        band.lo = std::min(band.lo, raids.min_ratio);
        band.hi = std::max(band.hi, raids.min_ratio);
        const double need = ShiftedThreshold(raids.min_ratio, band, -TraitPull(g.traits.boldness), t.trait_shift);
        for (int n = 0; n < raids.per_night; ++n) {
            int best = -1;
            double best_r = need;
            for (std::size_t a = 0; a < axes_.size(); ++a) {
                const Axis& x = axes_[a];
                if (!x.raid || x.status != AxisStatus::Waiting) continue;
                const int fi = RaidGroup(a);
                if (fi < 0) continue;
                double def = 0.0;
                for (int di : x.defenders) def += SeenPower(Side::Enemy, static_cast<std::size_t>(di)) * x.raid_share;
                const double r = def > 0.0 ? SeenPower(Side::Enemy, static_cast<std::size_t>(fi)) / def : 1e9;
                if (r >= best_r) {
                    best_r = r;
                    best = static_cast<int>(a);
                }
            }
            if (best < 0) break;
            Axis& x = axes_[static_cast<std::size_t>(best)];
            const int fi = RaidGroup(static_cast<std::size_t>(best));
            x.status = AxisStatus::Attacking;
            x.halted = HaltReason::None;
            x.start_h = x.last_advance_h = h;
            x.first.push_back(fi);
            StartAttack(static_cast<std::size_t>(fi), h);
            for (int di : x.defenders) {
                if (!forms_[static_cast<std::size_t>(di)].spent) Settle(static_cast<std::size_t>(di), h);
            }
            Order(h, GroundOrder::RaidStarts, c, fi, best, best_r >= 1e8 ? 0.0 : best_r);  // 0: nobody holds there
        }
        Push(NightEnd(h, env), EventKind::RaidEnd);
    }

    // The raid group of a raid axis, if it is fit to raid (-1 otherwise).
    int RaidGroup(std::size_t a) const {
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            const Formation& f = forms_[i];
            if (f.raid_axis == static_cast<int>(a)) return f.spent ? -1 : static_cast<int>(i);
        }
        return -1;
    }

    // Dawn: the raids go back to their lines (raided, not held); the next night's are planned.
    void RaidEnd(double h, const GroundEnv& env) {
        for (std::size_t a = 0; a < axes_.size(); ++a) {
            if (axes_[a].raid && axes_[a].status == AxisStatus::Attacking) EndRaid(a, h);
        }
        const double night = NextNightStart(h, env);
        if (night >= 0.0 && top_[1] >= 0) Push(night, EventKind::RaidNight, -1, top_[1]);
    }

    void EndRaid(std::size_t a, double h) {
        Axis& x = axes_[a];
        Order(h, GroundOrder::RaidEnds, top_[1], x.first.empty() ? -1 : x.first.front(), static_cast<int>(a), x.depth_m);
        for (int fi : x.first) Hold(static_cast<std::size_t>(fi), h);
        for (int di : x.defenders) {
            if (!forms_[static_cast<std::size_t>(di)].spent) SetRest(static_cast<std::size_t>(di), LossAction::Inactive, h);
        }
        x.first.clear();
        x.status = AxisStatus::Waiting;
        x.depth_m = 0.0;
        x.pace_km_day = 0.0;
        moved_[a] = 1;
    }

    // ---- each side's picture (F3.5, picture.hpp)

    static std::size_t PictureIndex(Side s) { return s == Side::Ours ? 0u : 1u; }

    void PushEvent(Event e) {
        e.seq = seq_++;
        queue_.push_back(e);
        std::push_heap(queue_.begin(), queue_.end(), Later());
    }

    // Sized for the war; its first beliefs (its own units as they stand, the other side's as peacetime intelligence
    // estimated them, the line at the border); its scheduled reports queued. With the tables' truth switch (tests) no
    // report is ever queued: the commanders read the truth.
    void BuildPictures(const airwar::LineOfContact& line) {
        const PictureTables& pt = data_->tables.picture;
        for (std::size_t k = 0; k < 2; ++k) {
            const Side by = k == 0 ? Side::Ours : Side::Enemy;
            GroundPicture& p = pictures_[k];
            p.Size(by, forms_.size(), axes_.size(), line.StationCount(), line.SectorCount());
            for (std::size_t i = 0; i < forms_.size(); ++i) {
                p.units[i] = Observe(i, by, forms_[i].unit.side == by ? PictureSource::Own : PictureSource::Prior, 0.0);
            }
            for (std::size_t s = 0; s < line.StationCount(); ++s) p.line_m[s] = line.StationOffset(s);
        }
        if (pt.truth) return;
        const double first = NextReportHour(0.0, ReportPeriodHours());
        Push(first, EventKind::ReportsDue, -1, 0);
        Push(first, EventKind::ReportsDue, -1, 1);
        Push(NextReportHour(0.0, 24.0 / static_cast<double>(std::max(1, pt.army_intel_per_day))), EventKind::IntelDue);
    }

    double ReportPeriodHours() const { return 24.0 / static_cast<double>(std::max(1, data_->tables.division_reports_per_day)); }

    // The first scheduled report's hour at or after `h`: the local report_first_h, then every `period`.
    double NextReportHour(double h, double period) const {
        const double first = data_->tables.picture.report_first_h;
        const double abs_h = start_hour_ + h;
        const double k = std::ceil((abs_h - first) / period - 1e-9);
        return first + k * period - start_hour_;
    }

    // The sector a formation is seen in: its axis's, the sector it holds or its task's (-1 behind the line).
    int SectorOfFormation(std::size_t i) const {
        const Formation& f = forms_[i];
        if (f.unit.side == Side::Enemy) {
            if (f.axis >= 0) return axes_[static_cast<std::size_t>(f.axis)].sector;
            if (f.raid_axis >= 0) return axes_[static_cast<std::size_t>(f.raid_axis)].sector;
            return f.first_sector;
        }
        if (f.first_sector >= 0) return f.first_sector;
        if (f.task_axis >= 0) return axes_[static_cast<std::size_t>(f.task_axis)].sector;
        return -1;
    }

    // A report of formation `i` compiled at `h` by `by`'s source: the truth then times the source's factor.
    UnitBelief Observe(std::size_t i, Side by, PictureSource src, double h) const {
        const Formation& f = forms_[i];
        const GroundTables& t = data_->tables;
        UnitBelief b;
        b.sector = SectorOfFormation(i);
        b.factor = BeliefFactor(root_, by, src, b.sector, h, SourceSigma(t.picture, src));
        const double s = h > f.settled_h ? StrengthAt(i, h) : f.unit.strength;
        b.fraction = (f.unit.establishment > 0.0 ? s / f.unit.establishment : 0.0) * b.factor;
        b.power = f.spent ? 0.0 : UnitPower(t, s, f.unit.supply, f.unit.cohesion) * b.factor;
        b.observed_h = h;
        b.posture = f.unit.posture;
        b.source = src;
        b.out = f.spent;
        return b;
    }

    void PushUnitReport(Side by, std::size_t i, const UnitBelief& b, double at) {
        Event e;
        e.h = at;
        e.kind = EventKind::UnitReport;
        e.who = static_cast<int>(i);
        e.axis = static_cast<int>(PictureIndex(by));
        e.obs_h = b.observed_h;
        e.v0 = b.fraction;
        e.v1 = b.power;
        e.factor = b.factor;
        e.sector = b.sector;
        e.src = static_cast<std::uint8_t>(b.source);
        e.posture = static_cast<std::uint8_t>(b.posture);
        e.flag = b.out ? 1 : 0;
        PushEvent(e);
    }

    void PushAxisReport(Side by, std::size_t a, double obs_h, double at) {
        const Axis& x = axes_[a];
        Event e;
        e.h = at;
        e.kind = EventKind::AxisReport;
        e.who = static_cast<int>(PictureIndex(by));
        e.axis = static_cast<int>(a);
        e.obs_h = obs_h;
        e.v0 = x.depth_m;
        e.v1 = x.pace_km_day;
        e.flag = x.status == AxisStatus::Counterattacked ? 1 : 0;
        PushEvent(e);
    }

    // An axis's news reported at once (a breakthrough, a halt, our counterattack): into `by`'s picture after the delay.
    void AxisAtOnce(Side by, std::size_t a, double h) {
        if (data_->tables.picture.truth) return;
        PushAxisReport(by, a, h, h + data_->tables.picture.own_delay_h);
    }

    // A unit's report on an event (it reports, asks, stops, withdraws, breaks, re-forms): its own state into its side's
    // picture, with the enemy facing it on `axis` (contact for ours, the ground reconnaissance for the enemy's) and that
    // axis's depth.
    void ReportAtOnce(std::size_t i, double h, int axis) {
        const PictureTables& pt = data_->tables.picture;
        if (pt.truth) return;
        const Side by = forms_[i].unit.side;
        PushUnitReport(by, i, Observe(i, by, PictureSource::Own, h), h + pt.own_delay_h);
        if (axis < 0) return;
        const Axis& x = axes_[static_cast<std::size_t>(axis)];
        const PictureSource src = by == Side::Ours ? PictureSource::Contact : PictureSource::GroundRecon;
        const double at = h + SourceDelayHours(pt, src);
        if (by == Side::Ours) {
            for (int fi : x.first) PushUnitReport(by, static_cast<std::size_t>(fi), Observe(static_cast<std::size_t>(fi), by, src, h), at);
        } else {
            for (int fi : x.defenders) PushUnitReport(by, static_cast<std::size_t>(fi), Observe(static_cast<std::size_t>(fi), by, src, h), at);
            if (DepthEngaged(x)) {
                for (int fi : x.depth_defenders) PushUnitReport(by, static_cast<std::size_t>(fi), Observe(static_cast<std::size_t>(fi), by, src, h), at);
            }
            for (int fi : x.counter) PushUnitReport(by, static_cast<std::size_t>(fi), Observe(static_cast<std::size_t>(fi), by, src, h), at);
        }
        PushAxisReport(by, static_cast<std::size_t>(axis), h, h + pt.own_delay_h);
    }

    // The enemy's formations our units touch: on an axis's attack, or holding the line (a rebel group on its line).
    bool EnemyAtLine(std::size_t i) const {
        const Formation& f = forms_[i];
        if (f.unit.side != Side::Enemy || f.spent) return false;
        if (f.raid_axis >= 0 || f.first_sector >= 0) return true;
        if (f.axis < 0) return false;
        const Axis& x = axes_[static_cast<std::size_t>(f.axis)];
        return std::find(x.first.begin(), x.first.end(), static_cast<int>(i)) != x.first.end();
    }

    // Ours the enemy's ground reconnaissance sees at the line: holding a sector, or fighting on an axis.
    bool OursAtLine(std::size_t i) const {
        const Formation& f = forms_[i];
        if (f.unit.side != Side::Ours || f.spent) return false;
        if (f.first_sector >= 0 && (f.role == DeployRole::FirstEchelon || f.role == DeployRole::FlankScreen)) return true;
        const int fi = static_cast<int>(i);
        for (const Axis& x : axes_) {
            if (std::find(x.defenders.begin(), x.defenders.end(), fi) != x.defenders.end()) return true;
            if (std::find(x.counter.begin(), x.counter.end(), fi) != x.counter.end()) return true;
            if (DepthEngaged(x) && std::find(x.depth_defenders.begin(), x.depth_defenders.end(), fi) != x.depth_defenders.end()) return true;
        }
        return false;
    }

    // The enemy's air has seen one of ours lately: a troops entry of its picture (not a ghost) observed within
    // air_fresh_h, behind the line in a sector it holds (within air_depth_km) or, off the line, within air_reach_km of
    // it. A stand-in until F3.7 ties the theatre's troops objects to the formations.
    bool AirSees(std::size_t i, double h, const GroundEnv& env, const airwar::LineOfContact& line) const {
        if (env.intel == nullptr) return false;
        const PictureTables& pt = data_->tables.picture;
        const Formation& f = forms_[i];
        if (f.spent) return false;
        for (const airwar::IntelEntry& e : env.intel->entries) {
            if (e.ghost || e.kind != airwar::ObjectKind::Troops || h - e.observed_h > pt.air_fresh_h) continue;
            if (f.first_sector >= 0) {
                const airwar::FrontPlace pl = line.Place(e.pos);
                if (pl.side > 0.0 && pl.distance_m <= pt.air_depth_km * 1000.0 && f.InSector(line.SectorOf(e.pos))) return true;
            } else if (airwar::Dist2d(e.pos, f.pos) <= pt.air_reach_km * 1000.0) {
                return true;
            }
        }
        return false;
    }

    // A side's scheduled reports, compiled now: its own units' returns, the other side's units it sees (ours: those in
    // contact; the enemy's: those its air has seen, else those at the line), every axis's depth and pace, the line.
    void ReportsDue(Side by, double h, const GroundEnv& env, const airwar::LineOfContact& line) {
        const PictureTables& pt = data_->tables.picture;
        const std::size_t k = PictureIndex(by);
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            PictureSource src = PictureSource::None;
            if (forms_[i].unit.side == by) {
                src = PictureSource::Own;
            } else if (by == Side::Ours) {
                if (EnemyAtLine(i)) src = PictureSource::Contact;
            } else if (AirSees(i, h, env, line)) {
                src = PictureSource::Air;
            } else if (OursAtLine(i)) {
                src = PictureSource::GroundRecon;
            }
            if (src != PictureSource::None) PushUnitReport(by, i, Observe(i, by, src, h), h + SourceDelayHours(pt, src));
        }
        GroundPicture& p = pictures_[k];
        for (std::size_t a = 0; a < axes_.size(); ++a) {
            const Axis& x = axes_[a];
            const AxisBelief& b = p.axes[a];
            if (x.status == AxisStatus::Waiting && x.depth_m == 0.0 && b.depth_m == 0.0 && b.pace_km_day == 0.0 && !b.counterattacked) continue;
            PushAxisReport(by, a, h, h + pt.own_delay_h);
        }
        for (std::size_t s = 0; s < p.pending_line_m.size() && s < line.StationCount(); ++s) p.pending_line_m[s] = line.StationOffset(s);
        p.pending_line_h = h;
        Event e;
        e.h = h + pt.own_delay_h;
        e.kind = EventKind::LineReport;
        e.who = static_cast<int>(k);
        e.obs_h = h;
        PushEvent(e);
        Push(h + ReportPeriodHours(), EventKind::ReportsDue, -1, static_cast<int>(k));
    }

    // Our army's intelligence summary of the enemy's formations our units do not touch (its echelons and reserve).
    void IntelDue(double h) {
        const PictureTables& pt = data_->tables.picture;
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            if (forms_[i].unit.side != Side::Enemy || EnemyAtLine(i)) continue;
            PushUnitReport(Side::Ours, i, Observe(i, Side::Ours, PictureSource::ArmyIntel, h), h + pt.army_intel_delay_h);
        }
        Push(h + 24.0 / static_cast<double>(std::max(1, pt.army_intel_per_day)), EventKind::IntelDue);
    }

    // Where the enemy's own ground front is collapsing in its picture: a formation of its own on an axis (or its raid
    // line) believed under crisis_below, or our counterattack on an axis there.
    void UpdateCrisis() {
        crisis_dirty_ = false;
        GroundPicture& p = pictures_[1];
        std::fill(p.crisis.begin(), p.crisis.end(), static_cast<char>(0));
        const double below = data_->tables.picture.crisis_below;
        for (std::size_t i = 0; i < forms_.size(); ++i) {
            const Formation& f = forms_[i];
            if (f.unit.side != Side::Enemy) continue;
            const int on = f.axis >= 0 ? f.axis : f.raid_axis;
            if (on < 0) continue;
            const int s = axes_[static_cast<std::size_t>(on)].sector;
            if (s >= 0 && static_cast<std::size_t>(s) < p.crisis.size() && p.units[i].fraction < below) p.crisis[static_cast<std::size_t>(s)] = 1;
        }
        for (std::size_t a = 0; a < axes_.size(); ++a) {
            const int s = axes_[a].sector;
            if (p.axes[a].counterattacked && s >= 0 && static_cast<std::size_t>(s) < p.crisis.size()) p.crisis[static_cast<std::size_t>(s)] = 1;
        }
    }


    // ---- the line

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
