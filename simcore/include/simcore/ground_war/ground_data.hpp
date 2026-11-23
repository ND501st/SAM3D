// simcore/ground_war/ground_data.hpp — the ground war's data (track F, F3.1; docs/fronts/03_GROUND_WAR.md sections 3, 5,
// 8 and 11): the sector's combat tables (data/gameplay/ground_war.json), each enemy faction's ground forces, doctrine and
// plan (data/air_war/ground_<faction>.json), and our side's defence by adversary (data/campaign/ground_defence.json).
//
// Orders of battle are in command::OrderOfBattle's format, read by its loader (command/oob.hpp), so one loader reads both
// sides. Every point is a place of the country map the file names ("place_map", F1: settlements, landmarks and sites),
// resolved to its point at load, and the loader refuses a place the map lacks; every stretch of the front is a run of
// the sectors of the theatre the file names ("theatre", F2.1). No exceptions: a loader returns false with a message.
//
// Nothing here runs the ground war (F3.3) and no campaign reads these files yet: loading them changes nothing.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/crew_data.hpp"  // detail::ReadTextFile, which command/oob.hpp uses without including it
#include "simcore/command/oob.hpp"
#include "simcore/console_data.hpp"
#include "simcore/country_map.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace groundwar {

// ------------------------------------------------------------------------------------------------------------ the enums

// Dupuy's postures (ciano_1988_qjm fig. 24): a defender's multiplies its power; an attacker's is Attack.
enum class Posture : std::uint8_t { Attack, HastyDefence, PreparedDefence, FortifiedDefence, Delay, Withdrawal };
constexpr std::size_t kPostureCount = 6;

// FM 101-10-1 table 5-8's actions (fm_101_10_1_1976 p. 5-8), plus a division in reserve.
enum class LossAction : std::uint8_t {
    Covering,
    AttackMeeting,
    AttackPosition,
    AttackFortified,
    DefenceMeeting,
    DefencePosition,
    DefenceSector,
    Inactive,
    Pursuit,
    Delay,
    Reserve
};
constexpr std::size_t kLossActionCount = 11;

enum class Terrain : std::uint8_t { Open, Forest, Hills, Mountains, Marsh, Urban };
constexpr std::size_t kTerrainCount = 6;

// HERO's oppositions (hero_1972_oralfore p. IV-2), Stalled below the first threshold, Unopposed with no defender.
enum class Opposition : std::uint8_t { Stalled, Intense, Moderate, Slight, Negligible, Unopposed };
constexpr std::size_t kAdvanceClassCount = 5;  // Stalled to Negligible: the table's rows

// FM 101-10-1 table 7-7's commitments (fm_101_10_1_1976 p. 7-16).
enum class TankCommitment : std::uint8_t { DelayLight, DelayModerate, DelayHeavy, DefendModerate, DefendHeavy };
constexpr std::size_t kTankCommitmentCount = 5;

// The echelons FM 100-2-1 gives reaction and planning times for (fm_100_2_1_1984 p. 5-14).
enum class Echelon : std::uint8_t { Division, Regiment, Battalion };
constexpr std::size_t kEchelonCount = 3;

enum class DeployRole : std::uint8_t { FirstEchelon, SecondEchelon, FlankScreen, Reserve, Support };
enum class PlanKind : std::uint8_t { Offensive, LimitedIncursion, Raids };
enum class EffectivenessKind : std::uint8_t { Lines, Breakpoints };

inline const char* PostureName(Posture p) {
    static const char* const names[kPostureCount] = {"attack", "hasty_defence", "prepared_defence", "fortified_defence", "delay", "withdrawal"};
    return names[static_cast<std::size_t>(p)];
}
inline const char* LossActionName(LossAction a) {
    static const char* const names[kLossActionCount] = {"covering", "attack_meeting", "attack_position", "attack_fortified",
                                                        "defence_meeting", "defence_position", "defence_sector", "inactive",
                                                        "pursuit", "delay", "reserve"};
    return names[static_cast<std::size_t>(a)];
}
inline const char* TerrainName(Terrain t) {
    static const char* const names[kTerrainCount] = {"open", "forest", "hills", "mountains", "marsh", "urban"};
    return names[static_cast<std::size_t>(t)];
}
inline const char* OppositionName(Opposition o) {
    static const char* const names[] = {"stalled", "intense", "moderate", "slight", "negligible", "unopposed"};
    return names[static_cast<std::size_t>(o)];
}
inline const char* TankCommitmentName(TankCommitment c) {
    static const char* const names[kTankCommitmentCount] = {"delay_light", "delay_moderate", "delay_heavy", "defend_moderate", "defend_heavy"};
    return names[static_cast<std::size_t>(c)];
}
inline const char* EchelonName(Echelon e) {
    static const char* const names[kEchelonCount] = {"division", "regiment", "battalion"};
    return names[static_cast<std::size_t>(e)];
}
inline const char* DeployRoleName(DeployRole r) {
    static const char* const names[] = {"first_echelon", "second_echelon", "flank_screen", "reserve", "support"};
    return names[static_cast<std::size_t>(r)];
}
inline const char* PlanKindName(PlanKind k) {
    static const char* const names[] = {"offensive", "limited_incursion", "raids"};
    return names[static_cast<std::size_t>(k)];
}

// A name back to its enum; false if unknown. `count` names from `name_of`.
template <typename E, typename NameOf>
bool ParseEnum(const std::string& s, std::size_t count, NameOf name_of, E& out) {
    for (std::size_t i = 0; i < count; ++i) {
        if (s == name_of(static_cast<E>(i))) {
            out = static_cast<E>(i);
            return true;
        }
    }
    return false;
}

// A range from a table: a norm's low and high ends.
struct ValueRange {
    double lo = 0.0;
    double hi = 0.0;
    double At(double u) const { return lo + (hi - lo) * u; }
    double Mid() const { return 0.5 * (lo + hi); }
};

// --------------------------------------------------------------------------------- data/gameplay/ground_war.json

struct LossRow {
    double battle_first = 0.0;  // a day's battle losses on the action's first day, a fraction of strength
    double battle_later = 0.0;  // on later days
    double nonbattle = 0.0;     // every day
};

// Each side's picture of the ground war (F3.5, picture.hpp; docs/fronts/03_GROUND_WAR.md section 9): the reports' delays
// and the spreads of the believed strengths by source (ground_war.json's "picture" block, game values).
struct PictureTables {
    bool truth = false;              // a test's switch, never in the file: the commanders read the truth (F3.4's hand cases)
    double report_first_h = 6.0;     // the local hour of the day's first scheduled report (then every 24 / division_per_day)
    double own_delay_h = 2.0;        // a unit's report to its side's staff (its own strength, the enemy in contact)
    double own_sigma = 0.05;         // the spread of a unit's own strength return
    double contact_sigma = 0.25;     // of its estimate of the enemy in contact
    double prior_sigma = 0.3;        // of the other side's strengths known at the war's start (peacetime intelligence)
    int army_intel_per_day = 1;      // our army's intelligence summaries of the enemy not in contact a day
    double army_intel_delay_h = 6.0;
    double army_intel_sigma = 0.4;
    double air_sigma = 0.2;          // the enemy's air reconnaissance of our troops (its IntelPicture's Troops entries)
    double air_fresh_h = 24.0;       // an entry of a troops object tied to the formation (F3.7) observed within this counts
    double ground_recon_delay_h = 3.0;  // the enemy's ground reconnaissance at the line
    double ground_recon_sigma = 0.35;
    double crisis_below = 0.5;       // a formation of the enemy's own believed below this: its front collapsing (02 section 3)
};

// The air war's troops objects and our units (F3.7, ground_war.hpp's ties; docs/fronts/03_GROUND_WAR.md section 6): where a
// unit out of the line re-forms and how its stocks become the supply its combat reads (ground_war.json's "troops" block,
// game values; without the block these defaults).
struct TroopsTables {
    double reform_depth_km = 25.0;   // a unit withdrawn, dispersed or re-forming stands this far behind the line of contact
    double full_supply_days = 1.0;   // a formation with this many days of stocks (the air war's consumption) fights at full supply
    double strike_loss_share = 1.0;  // of a strike's damage to a tied object, the share its unit's strength loses (F6.1)
};

struct GroundTables {
    // Strength: weighted equipment plus personnel, in T-55 equivalents.
    double personnel_per_man = 0.0;
    std::vector<std::pair<std::string, double>> weights;  // by type, sorted

    // Power.
    double supply_floor = 0.5;
    double cohesion_floor = 0.3;
    std::array<double, kPostureCount> posture{};
    std::array<double, kTerrainCount> terrain_defender{};
    std::array<double, kTerrainCount> terrain_advance{};
    double night_attacker = 1.0;
    double poor_visibility_attacker = 1.0;
    double river_line_defender = 1.0;

    // Losses.
    std::array<LossRow, kLossActionCount> losses{};
    double reference_ratio = 3.0;
    double ratio_exponent = 0.5;
    double scale_lo = 0.25;
    double scale_hi = 4.0;
    std::array<std::array<double, 3>, kTankCommitmentCount> tank_losses{};  // days 1-3, fractions
    double reparable_share = 0.8;

    // Advance.
    std::array<double, kAdvanceClassCount> class_from_ratio{};
    std::array<double, kAdvanceClassCount> class_km_per_day{};
    double unopposed_km_per_day = 0.0;
    double draw_sigma = 0.0;

    // River crossings.
    double crossing_intact_h = 0.0;
    double crossing_cut_h = 0.0;
    double pontoon_factor = 1.0;

    // Commanders.
    std::array<ValueRange, kEchelonCount> reaction_h{};
    std::array<ValueRange, kEchelonCount> planning_h{};
    double trait_shift = 0.2;

    // Cohesion and dispersal.
    double cohesion_loss_per_strength_loss = 0.0;
    double cohesion_air_attack_loss = 0.0;
    double cohesion_recovery_per_day = 0.0;
    double cohesion_break_below = 0.0;
    ValueRange reassemble_days;
    double straggler_loss = 0.0;

    // The march.
    ValueRange road_day_kmh;
    ValueRange road_night_kmh;
    ValueRange cross_country_kmh;
    ValueRange halt_h;
    ValueRange halt_every_h;

    // Reports and levels.
    int division_reports_per_day = 2;
    double levels_down_after_h = 6.0;
    int levels_neighbours = 1;            // U1 (F3.6): the player's sector and this many either side
    double levels_hit_weight = 2.0;       // a regiment the axis hits takes this many times its share of the formation's losses
    double levels_support_weight = 0.5;   // a support regiment (no tanks: artillery) this many times its share

    // The air war's troops objects (F3.7).
    TroopsTables troops;

    // The clock and the line at U0 (F3.3, ground_war.hpp).
    std::vector<std::string> tank_types;  // the weights' types marked "tank" (FM 101-10-1 table 7-7's), sorted
    ValueRange breakthrough_width_km;
    ValueRange taper_km;
    double security_zone_share = 0.25;
    double second_echelon_after_h = 24.0;
    double stall_halt_h = 24.0;
    double terrain_step_km = 1.0;
    double hills_above_m = 150.0;
    double mountains_above_m = 300.0;
    double bridge_reach_km = 5.0;
    double river_line_km = 3.0;

    // The commanders (F3.4, commanders.hpp): the ranges their traits move thresholds in, and the clock of their orders.
    ValueRange attack_ratio_range{2.0, 4.0};
    ValueRange counterattack_ratio{1.5, 2.5};
    ValueRange stall_h{12.0, 36.0};
    ValueRange hold_without_order_h{3.0, 9.0};
    double moving_km_per_day = 2.8;
    double pace_window_h = 24.0;
    ValueRange reform_days{1.0, 2.0};
    double repair_return_per_day = 0.25;
    double march_road_factor = 1.3;

    // Each side's picture (F3.5).
    PictureTables picture;

    bool IsTank(const std::string& type) const { return std::binary_search(tank_types.begin(), tank_types.end(), type); }

    // A type's weight (0 for a type not in the table: support and air defence do not count); `known` says if listed.
    double WeightOf(const std::string& type, bool* known = nullptr) const {
        const auto it = std::lower_bound(weights.begin(), weights.end(), type,
                                         [](const std::pair<std::string, double>& w, const std::string& t) { return w.first < t; });
        const bool found = it != weights.end() && it->first == type;
        if (known != nullptr) *known = found;
        return found ? it->second : 0.0;
    }
};

// ------------------------------------------------------------------------------------- doctrine, plans, commanders

struct EffectivenessDef {
    EffectivenessKind kind = EffectivenessKind::Lines;
    // Lines (the Pact and our side): a strength fraction below which the unit is degraded or ineffective.
    double degraded_below = 0.0;
    double ineffective_below = 0.0;
    double hq_hit_ineffective_below = 0.0;  // with its command, rockets and artillery hit
    // Breakpoints (NATO and the small forces): cumulative losses at which an attack stops or a defence breaks, drawn per unit.
    ValueRange attack_stop_losses;
    ValueRange defence_break_losses;
};

struct AdvanceCaps {
    double breakthrough_km_per_day = 0.0;  // through a hasty, prepared or fortified defence
    double depth_km_per_day = 0.0;         // against a delay, a withdrawal or a meeting engagement
    double broken_km_per_day = 0.0;        // against a broken enemy or none
};

struct FrontageDef {
    std::string echelon;  // army, division, brigade, regiment, battalion
    std::string posture;  // attack, breakthrough, defence
    ValueRange width_km;
    ValueRange depth_km;  // {0, 0} when the norm gives none
};

struct GroundDoctrineDef {
    double attack_ratio = 3.0;
    ValueRange breakthrough_ratio;
    ValueRange defend_ratio;  // {0, 0}: the doctrine gives none
    bool withdraw_needs_order = true;
    bool counterattack_by_senior = true;
    bool second_echelon_to_success = false;
    bool disperse_rather_than_retreat = false;
    bool counterattack_stops_at_border = false;
    int echelons_against_deep_defence = 1;
    double reserve_share = 0.0;
    ValueRange covering_force_share;
    EffectivenessDef effectiveness;
    AdvanceCaps caps;
    std::vector<FrontageDef> frontages;
    // Our side only (security_zone and prepared_positions present).
    bool has_security_zone = false;
    ValueRange security_zone_depth_km;
    ValueRange security_line_spacing_km;
    ValueRange security_delay_per_line_h;
    ValueRange division_prepared_positions;
    ValueRange regiment_prepared_positions;
    ValueRange restore_control_h;
    // NATO's fire support coordination line (F3.7, fm_101_5_1_1980 p. 1-49, fm_6_20_1977 p. D-5, ota_1987_fofa pp. 78-79):
    // drawn per sector this far beyond the line of contact in defence; in attack on the next terrain line (a river on the
    // axis) beyond the day's objective, looked for this far beyond it. Absent: the faction draws none (C2 keeps
    // c2.bai_depth_m).
    bool has_fscl = false;
    ValueRange fscl_defence_km;
    double fscl_terrain_reach_km = 30.0;

    const FrontageDef* Frontage(const std::string& echelon, const std::string& posture) const {
        for (const FrontageDef& f : frontages) {
            if (f.echelon == echelon && f.posture == posture) return &f;
        }
        return nullptr;
    }
};

// A place of the country map a file names (F1) and its point, resolved at load ("" and the origin: none).
struct GroundPlace {
    std::string at;
    Vec3 pos;
};

struct GroundCommanderDef {
    std::string id;
    std::string name;
    std::string unit;  // the order of battle's unit he commands
    std::string post;  // our chain of command's position (chain_<id>.json), or ""
    int age = 45;
    std::string background;
    std::string bio;
    airwar::Traits traits;
};

struct GroundCommandersDef {
    airwar::OfficerPoolDef pool;  // ranks by echelon (army, division, brigade, regiment, battalion)
    std::vector<GroundCommanderDef> authored;
};

struct ObjectiveDef {
    std::string id;
    std::string name;
    GroundPlace place;
    int by_day = 0;  // the plan's day for it (whole days after the offensive's start)
    std::string notes;
};

struct AxisDef {
    std::string id;
    std::string name;
    bool main = false;
    std::vector<GroundPlace> route;  // places, the start first
    std::vector<std::string> first_echelon;
    std::vector<std::string> second_echelon;
    std::string objective;  // an ObjectiveDef id
    std::string sector;     // the sector of the faction's front (the theatre's, F2.1) it attacks in
    int sector_index = -1;  // its index in the front's sectors
};

struct RaidsDef {
    bool night_only = true;
    double min_ratio = 3.0;
    double max_depth_km = 0.0;
    int per_night = 0;
    std::vector<std::string> lines;  // the units holding the line that raid from it
};

struct GroundPlanDef {
    PlanKind kind = PlanKind::Offensive;
    double start_after_h = 0.0;  // hours after the air war's start
    double max_depth_km = 0.0;   // 0: no political limit
    std::vector<ObjectiveDef> objectives;
    std::vector<AxisDef> axes;
    std::vector<std::string> reserve;
    std::vector<std::string> covering_force;  // first-echelon units that screen the line when the defender counterattacks (NATO's)
    bool has_raids = false;
    RaidsDef raids;

    const ObjectiveDef* Objective(const std::string& id) const {
        for (const ObjectiveDef& o : objectives) {
            if (o.id == id) return &o;
        }
        return nullptr;
    }
    const AxisDef* MainAxis() const {
        for (const AxisDef& a : axes) {
            if (a.main) return &a;
        }
        return axes.empty() ? nullptr : &axes.front();
    }
};

// An enemy faction's ground forces (data/air_war/ground_<faction>.json).
struct GroundForceDef {
    std::string ground_id;
    std::string faction;
    std::string name;
    std::string place_map;  // the country map its places are on (data/maps/)
    std::string theatre;    // the theatre whose front of this faction its sectors are (data/air_war/)
    command::OrderOfBattle oob;
    std::vector<GroundPlace> garrisons;  // oob.units' garrisons in their order, resolved ("" where a unit names none)
    GroundCommandersDef commanders;
    GroundDoctrineDef doctrine;
    GroundPlanDef plan;

    // Every place the file names with its point, once each: the units' garrisons, the axes' routes, the objectives.
    std::vector<GroundPlace> NamedPlaces() const {
        std::vector<GroundPlace> out;
        const auto add = [&out](const GroundPlace& p) {
            if (p.at.empty()) return;
            for (const GroundPlace& o : out) {
                if (o.at == p.at) return;
            }
            out.push_back(p);
        };
        for (const GroundPlace& g : garrisons) add(g);
        for (const AxisDef& a : plan.axes) {
            for (const GroundPlace& p : a.route) add(p);
        }
        for (const ObjectiveDef& o : plan.objectives) add(o.place);
        return out;
    }
};

struct DeployedUnitDef {
    std::string unit;
    DeployRole role = DeployRole::FirstEchelon;
    std::vector<std::string> sectors;  // on the line: a run of the adversary's front's sectors, in order (F2.1)
    int first_sector = -1;             // their indices in the front's sectors
    int last_sector = -1;
    double from_km = 0.0;  // the run along the front's line, from its sectors (FrontDef::sectors)
    double to_km = 0.0;
    GroundPlace place;  // where it stands or assembles ("at"; required off the line)
};

struct DeploymentDef {
    std::string adversary;  // a faction id
    double security_zone_km = 0.0;
    std::vector<DeployedUnitDef> units;

    const DeployedUnitDef* Find(const std::string& unit) const {
        for (const DeployedUnitDef& d : units) {
            if (d.unit == unit) return &d;
        }
        return nullptr;
    }
};

// Our side's defence (data/campaign/ground_defence.json): the order of battle it names, with its equipment filled.
struct GroundDefenceDef {
    std::string defence_id;
    std::string name;
    std::string oob_file;
    std::string place_map;  // the country map its places are on
    std::string theatre;    // the theatre whose fronts its deployments' sectors are
    command::OrderOfBattle oob;  // oob_file's
    GroundCommandersDef commanders;
    GroundDoctrineDef doctrine;
    std::vector<DeploymentDef> deployments;

    const DeploymentDef* For(const std::string& adversary) const {
        for (const DeploymentDef& d : deployments) {
            if (d.adversary == adversary) return &d;
        }
        return nullptr;
    }
};

// What a ground file's places and sectors resolve against: the country map's places (F1) and a theatre's fronts and
// their sectors (F2.1), the theatre read on the same places. Loaded once for a file (LoadGroundWorld); a test may move a
// place before it loads the theatre and the ground files, and everything anchored to the place moves.
struct GroundWorld {
    std::string place_map;  // data/<place_map>
    std::string theatre;    // data/<theatre>
    MapPlaces places;
    airwar::TheatreDef theatre_def;

    const airwar::FrontDef* FrontOf(const std::string& faction) const {
        for (const airwar::FrontDef& f : theatre_def.fronts) {
            if (f.faction == faction) return &f;
        }
        return nullptr;
    }
};

// The world a ground file names: its place map's places (unless `places` is given) and its theatre read on them.
inline bool LoadGroundWorld(const std::string& data_dir, const std::string& place_map, const std::string& theatre, GroundWorld& out,
                            std::string* error = nullptr, const MapPlaces* places = nullptr) {
    out.place_map = place_map;
    out.theatre = theatre;
    if (places != nullptr) {
        out.places = *places;
    } else if (!LoadMapPlaces(data_dir, place_map, out.places, error)) {
        return false;
    }
    std::string e;
    if (!airwar::LoadTheatreFile(data_dir + "/" + theatre, out.theatre_def, &e, &out.places)) {
        if (error) *error = "theatre " + theatre + ": " + e;
        return false;
    }
    if (out.theatre_def.place_map != place_map) {
        if (error) *error = "theatre " + theatre + ": its places are on '" + out.theatre_def.place_map + "', not '" + place_map + "'";
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------------------- the loader

namespace detail {

class GroundLoader : public simcore::detail::ConsoleLoader {
public:
    bool LoadTables(const json::Value& root, GroundTables& out) {
        if (!Keys(root, "", {"ground_war_id", "status", "source", "confidence", "notes", "strength", "power", "posture", "ground",
                             "conditions", "losses", "tank_losses", "advance", "crossing", "decision_times_h", "trait_shift",
                             "cohesion", "dispersal", "march", "reports", "levels", "clock", "commanders", "picture", "troops"}) ||
            !Provenance(root, "")) {
            return false;
        }
        const json::Value* st = Object(root, "", "strength");
        if (st == nullptr || !Keys(*st, "strength", {"unit", "personnel_per_man", "weights", "notes"}) ||
            !Sourced(*st, "strength", "personnel_per_man", out.personnel_per_man, 0.0, 1.0)) {
            return false;
        }
        const json::Value* w = OptionalArray(*st, "strength", "weights");
        if (w == nullptr || w->items.empty()) return message_.empty() ? Fail("strength.weights", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < w->items.size(); ++i) {
            const std::string p = Index("strength.weights", i);
            std::string type;
            double weight = 0.0;
            if (!Keys(w->items[i], p, {"type", "tank", "weight", "source", "confidence", "notes"}) || !Provenance(w->items[i], p) ||
                !RequireString(w->items[i], p, "type", type) || !Num(w->items[i], p, "weight", weight, 0.0, 10.0)) {
                return false;
            }
            if (const json::Value* tank = w->items[i].Find("tank")) {  // a tank: table 7-7's losses (F3.3)
                if (!tank->Is(json::Type::Bool)) return Fail(Join(p, "tank"), "true or false");
                if (tank->boolean) out.tank_types.push_back(type);
            }
            for (const auto& e : out.weights) {
                if (e.first == type) return Fail(Join(p, "type"), "'" + type + "' is listed twice");
            }
            out.weights.push_back({type, weight});
        }
        std::stable_sort(out.weights.begin(), out.weights.end(),
                         [](const std::pair<std::string, double>& a, const std::pair<std::string, double>& b) { return a.first < b.first; });
        std::stable_sort(out.tank_types.begin(), out.tank_types.end());

        const json::Value* pw = Object(root, "", "power");
        if (pw == nullptr || !Keys(*pw, "power", {"supply_floor", "cohesion_floor", "notes"}) ||
            !Sourced(*pw, "power", "supply_floor", out.supply_floor, 0.0, 1.0) ||
            !Sourced(*pw, "power", "cohesion_floor", out.cohesion_floor, 0.0, 1.0)) {
            return false;
        }
        const json::Value* po = Object(root, "", "posture");
        if (po == nullptr || !Keys(*po, "posture", {"attack", "hasty_defence", "prepared_defence", "fortified_defence", "delay", "withdrawal", "notes"})) {
            return false;
        }
        for (std::size_t i = 0; i < kPostureCount; ++i) {
            if (!Sourced(*po, "posture", PostureName(static_cast<Posture>(i)), out.posture[i], 0.1, 5.0)) return false;
        }
        if (!LoadGround(root, out) || !LoadLosses(root, out) || !LoadTankLosses(root, out) || !LoadAdvance(root, out) ||
            !LoadDecisionTimes(root, out) || !LoadRest(root, out) || !LoadClock(root, out) || !LoadCommanders(root, out) ||
            !LoadPicture(root, out) || !LoadTroops(root, out)) {
            return false;
        }
        return true;
    }

    // The air war's troops objects and our units (F3.7): optional, the defaults without it.
    bool LoadTroops(const json::Value& root, GroundTables& out) {
        const json::Value* c = root.Find("troops");
        if (c == nullptr) return true;
        const std::string p = "troops";
        if (!c->Is(json::Type::Object)) return Fail(p, "expected an object");
        if (!Keys(*c, p, {"reform_depth_km", "full_supply_days", "strike_loss_share", "notes"}) ||
            !Sourced(*c, p, "reform_depth_km", out.troops.reform_depth_km, 0.0, 200.0) ||
            !Sourced(*c, p, "full_supply_days", out.troops.full_supply_days, 0.01, 30.0))
            return false;
        return c->Find("strike_loss_share") == nullptr || Sourced(*c, p, "strike_loss_share", out.troops.strike_loss_share, 0.0, 1.0);
    }

    // `world`: the places and theatre the file names (LoadGroundWorld).
    bool LoadForce(const json::Value& root, const GroundWorld& world, GroundForceDef& out) {
        world_ = &world;
        if (!Keys(root, "", {"ground_id", "faction", "name", "status", "source", "confidence", "notes", "place_map", "theatre", "oob",
                             "commanders", "doctrine", "plan"}) ||
            !Provenance(root, "") || !RequireString(root, "", "ground_id", out.ground_id) ||
            !RequireString(root, "", "faction", out.faction) || !RequireString(root, "", "name", out.name) ||
            !WorldOf(root, out.place_map, out.theatre) || !Oob(root, out.oob) || !Garrisons(out) ||
            !Commanders(root, out.oob, out.commanders) || !Doctrine(root, out.doctrine)) {
            return false;
        }
        front_ = world.FrontOf(out.faction);
        if (front_ == nullptr) return Fail("faction", "the theatre " + world.theatre + " has no front of '" + out.faction + "'");
        return Plan(root, out);
    }

    // `oob` is the order of battle the file names, already loaded; `world` the places and theatre it names.
    bool LoadDefence(const json::Value& root, const command::OrderOfBattle& oob, const GroundWorld& world, GroundDefenceDef& out) {
        world_ = &world;
        if (!Keys(root, "", {"defence_id", "name", "status", "source", "confidence", "notes", "oob", "place_map", "theatre",
                             "commanders", "doctrine", "deployments"}) ||
            !Provenance(root, "") || !RequireString(root, "", "defence_id", out.defence_id) ||
            !RequireString(root, "", "name", out.name) || !RequireString(root, "", "oob", out.oob_file) ||
            !WorldOf(root, out.place_map, out.theatre)) {
            return false;
        }
        out.oob = oob;
        if (!Commanders(root, out.oob, out.commanders) || !Doctrine(root, out.doctrine)) return false;
        if (!out.doctrine.has_security_zone) return Fail("doctrine.security_zone", "required for our side");
        return Deployments(root, out);
    }

    // A file's key naming a file, before the file is loaded ("oob", "place_map", "theatre"); false if missing.
    static bool FileOf(const json::Value& root, const char* key, std::string& file) {
        const json::Value* v = root.Find(key);
        if (v == nullptr || !v->Is(json::Type::String) || v->string.empty()) return false;
        file = v->string;
        return true;
    }

private:
    // ---- the helpers

    const json::Value* Object(const json::Value& obj, const std::string& path, const char* key) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) {
            Fail(Join(path, key), "required");
            return nullptr;
        }
        if (!v->Is(json::Type::Object)) {
            Fail(Join(path, key), "expected an object");
            return nullptr;
        }
        return v;
    }

    // A required number in [lo, hi].
    bool Num(const json::Value& obj, const std::string& path, const char* key, double& out, double lo, double hi) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        if (!v->Is(json::Type::Number) || v->number < lo || v->number > hi) {
            return Fail(Join(path, key), "a number in [" + Fmt(lo) + ", " + Fmt(hi) + "]");
        }
        out = v->number;
        return true;
    }

    bool WholeNum(const json::Value& obj, const std::string& path, const char* key, int& out, int lo, int hi) {
        double d = 0.0;
        if (!Num(obj, path, key, d, static_cast<double>(lo), static_cast<double>(hi))) return false;
        if (d != static_cast<double>(static_cast<int>(d))) return Fail(Join(path, key), "a whole number");
        out = static_cast<int>(d);
        return true;
    }

    // A sourced value: {"value": n, "source": ..., "confidence": ...}, n in [lo, hi].
    bool Sourced(const json::Value& obj, const std::string& path, const char* key, double& out, double lo, double hi) {
        const json::Value* v = Object(obj, path, key);
        const std::string p = Join(path, key);
        return v != nullptr && Keys(*v, p, {"value", "source", "confidence", "notes"}) && Provenance(*v, p) && Num(*v, p, "value", out, lo, hi);
    }

    bool WholeSourced(const json::Value& obj, const std::string& path, const char* key, int& out, int lo, int hi) {
        double d = 0.0;
        if (!Sourced(obj, path, key, d, static_cast<double>(lo), static_cast<double>(hi))) return false;
        if (d != static_cast<double>(static_cast<int>(d))) return Fail(Join(Join(path, key), "value"), "a whole number");
        out = static_cast<int>(d);
        return true;
    }

    // A sourced flag: {"value": true, "source": ..., "confidence": ...}.
    bool Flag(const json::Value& obj, const std::string& path, const char* key, bool& out) {
        const json::Value* v = Object(obj, path, key);
        const std::string p = Join(path, key);
        if (v == nullptr || !Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
        const json::Value* b = v->Find("value");
        if (b == nullptr || !b->Is(json::Type::Bool)) return Fail(Join(p, "value"), "true or false");
        out = b->boolean;
        return true;
    }

    // A sourced range: {"min": a, "max": b, "source": ..., "confidence": ...}, lo <= a <= b <= hi.
    bool RangeOf(const json::Value& obj, const std::string& path, const char* key, ValueRange& out, double lo, double hi) {
        const json::Value* v = Object(obj, path, key);
        const std::string p = Join(path, key);
        if (v == nullptr || !Keys(*v, p, {"min", "max", "source", "confidence", "notes"}) || !Provenance(*v, p) ||
            !Num(*v, p, "min", out.lo, lo, hi) || !Num(*v, p, "max", out.hi, lo, hi)) {
            return false;
        }
        if (out.hi < out.lo) return Fail(Join(p, "max"), "at least min");
        return true;
    }

    static std::string Fmt(double d) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", d);
        return buf;
    }

    bool Ids(const json::Value& obj, const std::string& path, const char* key, std::vector<std::string>& out) {
        return StringList(obj, path, key, out, [](const std::string& s) { return s.empty() ? std::string("an empty id") : std::string(); });
    }

    // ---- the tables

    bool LoadGround(const json::Value& root, GroundTables& out) {
        const json::Value* g = OptionalArray(root, "", "ground");
        if (g == nullptr) return message_.empty() ? Fail("ground", "required") : false;
        std::array<bool, kTerrainCount> seen{};
        for (std::size_t i = 0; i < g->items.size(); ++i) {
            const std::string p = Index("ground", i);
            const json::Value& r = g->items[i];
            std::string name;
            Terrain t = Terrain::Open;
            if (!Keys(r, p, {"terrain", "defender", "advance", "source", "confidence", "notes"}) || !Provenance(r, p) ||
                !RequireString(r, p, "terrain", name)) {
                return false;
            }
            if (!ParseEnum(name, kTerrainCount, TerrainName, t)) return Fail(Join(p, "terrain"), "unknown terrain '" + name + "'");
            const std::size_t k = static_cast<std::size_t>(t);
            if (seen[k]) return Fail(Join(p, "terrain"), "'" + name + "' is listed twice");
            seen[k] = true;
            if (!Num(r, p, "defender", out.terrain_defender[k], 0.5, 5.0) || !Num(r, p, "advance", out.terrain_advance[k], 0.0, 1.0)) return false;
        }
        for (std::size_t k = 0; k < kTerrainCount; ++k) {
            if (!seen[k]) return Fail("ground", std::string("no row for ") + TerrainName(static_cast<Terrain>(k)));
        }
        const json::Value* c = Object(root, "", "conditions");
        return c != nullptr && Keys(*c, "conditions", {"night_attacker", "poor_visibility_attacker", "river_line_defender", "notes"}) &&
               Sourced(*c, "conditions", "night_attacker", out.night_attacker, 0.1, 1.0) &&
               Sourced(*c, "conditions", "poor_visibility_attacker", out.poor_visibility_attacker, 0.1, 1.0) &&
               Sourced(*c, "conditions", "river_line_defender", out.river_line_defender, 1.0, 5.0);
    }

    bool LoadLosses(const json::Value& root, GroundTables& out) {
        const json::Value* l = Object(root, "", "losses");
        if (l == nullptr || !Keys(*l, "losses", {"notes", "actions", "reference_ratio", "ratio_exponent", "scale_min", "scale_max"})) return false;
        const json::Value* a = OptionalArray(*l, "losses", "actions");
        if (a == nullptr) return message_.empty() ? Fail("losses.actions", "required") : false;
        std::array<bool, kLossActionCount> seen{};
        for (std::size_t i = 0; i < a->items.size(); ++i) {
            const std::string p = Index("losses.actions", i);
            const json::Value& r = a->items[i];
            std::string name;
            LossAction act = LossAction::Inactive;
            double first = 0.0;
            double later = 0.0;
            double non = 0.0;
            if (!Keys(r, p, {"action", "battle_first_pct", "battle_later_pct", "nonbattle_pct", "source", "confidence", "notes"}) ||
                !Provenance(r, p) || !RequireString(r, p, "action", name) || !Num(r, p, "battle_first_pct", first, 0.0, 50.0) ||
                !Num(r, p, "battle_later_pct", later, 0.0, 50.0) || !Num(r, p, "nonbattle_pct", non, 0.0, 50.0)) {
                return false;
            }
            if (!ParseEnum(name, kLossActionCount, LossActionName, act)) return Fail(Join(p, "action"), "unknown action '" + name + "'");
            const std::size_t k = static_cast<std::size_t>(act);
            if (seen[k]) return Fail(Join(p, "action"), "'" + name + "' is listed twice");
            seen[k] = true;
            out.losses[k] = {first / 100.0, later / 100.0, non / 100.0};
        }
        for (std::size_t k = 0; k < kLossActionCount; ++k) {
            if (!seen[k]) return Fail("losses.actions", std::string("no row for ") + LossActionName(static_cast<LossAction>(k)));
        }
        if (!Sourced(*l, "losses", "reference_ratio", out.reference_ratio, 0.5, 10.0) ||
            !Sourced(*l, "losses", "ratio_exponent", out.ratio_exponent, 0.0, 3.0) || !Sourced(*l, "losses", "scale_min", out.scale_lo, 0.0, 1.0) ||
            !Sourced(*l, "losses", "scale_max", out.scale_hi, 1.0, 20.0)) {
            return false;
        }
        return true;
    }

    bool LoadTankLosses(const json::Value& root, GroundTables& out) {
        const json::Value* t = Object(root, "", "tank_losses");
        if (t == nullptr || !Keys(*t, "tank_losses", {"notes", "commitments", "reparable_share"}) ||
            !Sourced(*t, "tank_losses", "reparable_share", out.reparable_share, 0.0, 1.0)) {
            return false;
        }
        const json::Value* c = OptionalArray(*t, "tank_losses", "commitments");
        if (c == nullptr) return message_.empty() ? Fail("tank_losses.commitments", "required") : false;
        std::array<bool, kTankCommitmentCount> seen{};
        for (std::size_t i = 0; i < c->items.size(); ++i) {
            const std::string p = Index("tank_losses.commitments", i);
            const json::Value& r = c->items[i];
            std::string name;
            TankCommitment tc = TankCommitment::DelayLight;
            std::array<double, 3> pct{};
            if (!Keys(r, p, {"commitment", "day1_pct", "day2_pct", "day3_pct", "source", "confidence", "notes"}) || !Provenance(r, p) ||
                !RequireString(r, p, "commitment", name) || !Num(r, p, "day1_pct", pct[0], 0.0, 100.0) ||
                !Num(r, p, "day2_pct", pct[1], 0.0, 100.0) || !Num(r, p, "day3_pct", pct[2], 0.0, 100.0)) {
                return false;
            }
            if (!ParseEnum(name, kTankCommitmentCount, TankCommitmentName, tc)) return Fail(Join(p, "commitment"), "unknown commitment '" + name + "'");
            const std::size_t k = static_cast<std::size_t>(tc);
            if (seen[k]) return Fail(Join(p, "commitment"), "'" + name + "' is listed twice");
            seen[k] = true;
            if (pct[0] + pct[1] + pct[2] > 100.0) return Fail(p, "the three days' losses add up to more than all the tanks");
            for (std::size_t d = 0; d < 3; ++d) out.tank_losses[k][d] = pct[d] / 100.0;
        }
        for (std::size_t k = 0; k < kTankCommitmentCount; ++k) {
            if (!seen[k]) return Fail("tank_losses.commitments", std::string("no row for ") + TankCommitmentName(static_cast<TankCommitment>(k)));
        }
        return true;
    }

    bool LoadAdvance(const json::Value& root, GroundTables& out) {
        const json::Value* a = Object(root, "", "advance");
        if (a == nullptr || !Keys(*a, "advance", {"notes", "classes", "unopposed_km_per_day", "draw_sigma"}) ||
            !Sourced(*a, "advance", "unopposed_km_per_day", out.unopposed_km_per_day, 0.0, 300.0) ||
            !Sourced(*a, "advance", "draw_sigma", out.draw_sigma, 0.0, 2.0)) {
            return false;
        }
        const json::Value* c = OptionalArray(*a, "advance", "classes");
        if (c == nullptr) return message_.empty() ? Fail("advance.classes", "required") : false;
        if (c->items.size() != kAdvanceClassCount) return Fail("advance.classes", "five rows: stalled, intense, moderate, slight, negligible");
        for (std::size_t i = 0; i < c->items.size(); ++i) {
            const std::string p = Index("advance.classes", i);
            const json::Value& r = c->items[i];
            std::string name;
            if (!Keys(r, p, {"opposition", "from_ratio", "km_per_day", "source", "confidence", "notes"}) || !Provenance(r, p) ||
                !RequireString(r, p, "opposition", name) || !Num(r, p, "from_ratio", out.class_from_ratio[i], 0.0, 100.0) ||
                !Num(r, p, "km_per_day", out.class_km_per_day[i], 0.0, 300.0)) {
                return false;
            }
            if (name != OppositionName(static_cast<Opposition>(i))) {
                return Fail(Join(p, "opposition"), std::string("expected '") + OppositionName(static_cast<Opposition>(i)) + "' in this row");
            }
            if (i == 0 && out.class_from_ratio[0] != 0.0) return Fail(Join(p, "from_ratio"), "the first class starts at 0");
            if (i > 0 && !(out.class_from_ratio[i] > out.class_from_ratio[i - 1])) return Fail(Join(p, "from_ratio"), "rising");
            if (i > 0 && out.class_km_per_day[i] < out.class_km_per_day[i - 1]) return Fail(Join(p, "km_per_day"), "not below the class before");
        }
        if (out.unopposed_km_per_day < out.class_km_per_day[kAdvanceClassCount - 1]) {
            return Fail("advance.unopposed_km_per_day", "not below the negligible class");
        }
        return true;
    }

    bool LoadDecisionTimes(const json::Value& root, GroundTables& out) {
        const json::Value* d = OptionalArray(root, "", "decision_times_h");
        if (d == nullptr) return message_.empty() ? Fail("decision_times_h", "required") : false;
        std::array<bool, kEchelonCount> seen{};
        for (std::size_t i = 0; i < d->items.size(); ++i) {
            const std::string p = Index("decision_times_h", i);
            const json::Value& r = d->items[i];
            std::string name;
            Echelon e = Echelon::Division;
            ValueRange re;
            ValueRange pl;
            if (!Keys(r, p, {"echelon", "reaction_min", "reaction_max", "planning_min", "planning_max", "source", "confidence", "notes"}) ||
                !Provenance(r, p) || !RequireString(r, p, "echelon", name) || !Num(r, p, "reaction_min", re.lo, 0.0, 48.0) ||
                !Num(r, p, "reaction_max", re.hi, 0.0, 48.0) || !Num(r, p, "planning_min", pl.lo, 0.0, 48.0) ||
                !Num(r, p, "planning_max", pl.hi, 0.0, 48.0)) {
                return false;
            }
            if (!ParseEnum(name, kEchelonCount, EchelonName, e)) return Fail(Join(p, "echelon"), "unknown echelon '" + name + "'");
            if (re.hi < re.lo || pl.hi < pl.lo) return Fail(p, "each max at least its min");
            const std::size_t k = static_cast<std::size_t>(e);
            if (seen[k]) return Fail(Join(p, "echelon"), "'" + name + "' is listed twice");
            seen[k] = true;
            out.reaction_h[k] = re;
            out.planning_h[k] = pl;
        }
        for (std::size_t k = 0; k < kEchelonCount; ++k) {
            if (!seen[k]) return Fail("decision_times_h", std::string("no row for ") + EchelonName(static_cast<Echelon>(k)));
        }
        return true;
    }

    bool LoadRest(const json::Value& root, GroundTables& out) {
        const json::Value* cr = Object(root, "", "crossing");
        if (cr == nullptr || !Keys(*cr, "crossing", {"intact_bridge_h", "cut_bridge_h", "pontoon_factor", "notes"}) ||
            !Sourced(*cr, "crossing", "intact_bridge_h", out.crossing_intact_h, 0.0, 240.0) ||
            !Sourced(*cr, "crossing", "cut_bridge_h", out.crossing_cut_h, 0.0, 240.0) ||
            !Sourced(*cr, "crossing", "pontoon_factor", out.pontoon_factor, 0.0, 1.0) || !Sourced(root, "", "trait_shift", out.trait_shift, 0.0, 0.5)) {
            return false;
        }
        if (out.crossing_cut_h < out.crossing_intact_h) return Fail("crossing.cut_bridge_h", "at least intact_bridge_h");
        const json::Value* co = Object(root, "", "cohesion");
        if (co == nullptr || !Keys(*co, "cohesion", {"loss_per_strength_loss", "air_attack_loss", "recovery_per_day", "break_below", "notes"}) ||
            !Sourced(*co, "cohesion", "loss_per_strength_loss", out.cohesion_loss_per_strength_loss, 0.0, 20.0) ||
            !Sourced(*co, "cohesion", "air_attack_loss", out.cohesion_air_attack_loss, 0.0, 1.0) ||
            !Sourced(*co, "cohesion", "recovery_per_day", out.cohesion_recovery_per_day, 0.0, 1.0) ||
            !Sourced(*co, "cohesion", "break_below", out.cohesion_break_below, 0.0, 1.0)) {
            return false;
        }
        const json::Value* di = Object(root, "", "dispersal");
        if (di == nullptr || !Keys(*di, "dispersal", {"reassemble_min_days", "reassemble_max_days", "straggler_loss", "notes"}) ||
            !Sourced(*di, "dispersal", "reassemble_min_days", out.reassemble_days.lo, 0.0, 30.0) ||
            !Sourced(*di, "dispersal", "reassemble_max_days", out.reassemble_days.hi, 0.0, 30.0) ||
            !Sourced(*di, "dispersal", "straggler_loss", out.straggler_loss, 0.0, 1.0)) {
            return false;
        }
        if (out.reassemble_days.hi < out.reassemble_days.lo) return Fail("dispersal.reassemble_max_days", "at least reassemble_min_days");
        const json::Value* m = Object(root, "", "march");
        if (m == nullptr || !Keys(*m, "march", {"road_day_kmh", "road_night_kmh", "cross_country_kmh", "halt_h", "halt_every_h", "notes"}) ||
            !RangeOf(*m, "march", "road_day_kmh", out.road_day_kmh, 1.0, 120.0) ||
            !RangeOf(*m, "march", "road_night_kmh", out.road_night_kmh, 1.0, 120.0) ||
            !RangeOf(*m, "march", "cross_country_kmh", out.cross_country_kmh, 1.0, 120.0) || !RangeOf(*m, "march", "halt_h", out.halt_h, 0.0, 24.0) ||
            !RangeOf(*m, "march", "halt_every_h", out.halt_every_h, 0.1, 48.0)) {
            return false;
        }
        const json::Value* r = Object(root, "", "reports");
        const json::Value* lv = Object(root, "", "levels");
        return r != nullptr && lv != nullptr && Keys(*r, "reports", {"division_per_day", "notes"}) &&
               WholeSourced(*r, "reports", "division_per_day", out.division_reports_per_day, 1, 24) &&
               Keys(*lv, "levels", {"down_after_h", "neighbours", "hit_weight", "support_weight", "notes"}) &&
               Sourced(*lv, "levels", "down_after_h", out.levels_down_after_h, 0.0, 72.0) &&
               WholeSourced(*lv, "levels", "neighbours", out.levels_neighbours, 0, 8) &&
               Sourced(*lv, "levels", "hit_weight", out.levels_hit_weight, 1.0, 10.0) &&
               Sourced(*lv, "levels", "support_weight", out.levels_support_weight, 0.0, 1.0);
    }

    // The commanders' ranges and clock (F3.4).
    bool LoadCommanders(const json::Value& root, GroundTables& out) {
        const json::Value* c = Object(root, "", "commanders");
        const std::string p = "commanders";
        if (c == nullptr ||
            !Keys(*c, p, {"attack_ratio_range", "counterattack_ratio", "stall_h", "hold_without_order_h", "moving_km_per_day", "pace_window_h",
                          "reform_days", "repair_return_per_day", "march_road_factor", "notes"}) ||
            !RangeOf(*c, p, "attack_ratio_range", out.attack_ratio_range, 1.0, 20.0) ||
            !RangeOf(*c, p, "counterattack_ratio", out.counterattack_ratio, 0.5, 20.0) || !RangeOf(*c, p, "stall_h", out.stall_h, 1.0, 240.0) ||
            !RangeOf(*c, p, "hold_without_order_h", out.hold_without_order_h, 0.0, 72.0) ||
            !Sourced(*c, p, "moving_km_per_day", out.moving_km_per_day, 0.0, 100.0) ||
            !Sourced(*c, p, "pace_window_h", out.pace_window_h, 1.0, 240.0) || !RangeOf(*c, p, "reform_days", out.reform_days, 0.0, 30.0) ||
            !Sourced(*c, p, "repair_return_per_day", out.repair_return_per_day, 0.0, 1.0) ||
            !Sourced(*c, p, "march_road_factor", out.march_road_factor, 1.0, 3.0)) {
            return false;
        }
        return true;
    }

    // Each side's picture (F3.5): the reports' delays and the spreads by source.
    bool LoadPicture(const json::Value& root, GroundTables& out) {
        const json::Value* c = Object(root, "", "picture");
        const std::string p = "picture";
        PictureTables& t = out.picture;
        if (c == nullptr ||
            !Keys(*c, p, {"report_first_h", "own_delay_h", "own_sigma", "contact_sigma", "prior_sigma", "army_intel_per_day", "army_intel_delay_h",
                          "army_intel_sigma", "air_sigma", "air_fresh_h", "ground_recon_delay_h",
                          "ground_recon_sigma", "crisis_below", "notes"}) ||
            !Sourced(*c, p, "report_first_h", t.report_first_h, 0.0, 23.99) || !Sourced(*c, p, "own_delay_h", t.own_delay_h, 0.0, 24.0) ||
            !Sourced(*c, p, "own_sigma", t.own_sigma, 0.0, 2.0) || !Sourced(*c, p, "contact_sigma", t.contact_sigma, 0.0, 2.0) ||
            !Sourced(*c, p, "prior_sigma", t.prior_sigma, 0.0, 2.0) || !WholeSourced(*c, p, "army_intel_per_day", t.army_intel_per_day, 1, 24) ||
            !Sourced(*c, p, "army_intel_delay_h", t.army_intel_delay_h, 0.0, 48.0) || !Sourced(*c, p, "army_intel_sigma", t.army_intel_sigma, 0.0, 2.0) ||
            !Sourced(*c, p, "air_sigma", t.air_sigma, 0.0, 2.0) || !Sourced(*c, p, "air_fresh_h", t.air_fresh_h, 0.0, 240.0) ||
            !Sourced(*c, p, "ground_recon_delay_h", t.ground_recon_delay_h, 0.0, 48.0) ||
            !Sourced(*c, p, "ground_recon_sigma", t.ground_recon_sigma, 0.0, 2.0) || !Sourced(*c, p, "crisis_below", t.crisis_below, 0.0, 1.0)) {
            return false;
        }
        // One scheduled batch of the line in flight at a time (GroundWar keeps one pending line a side).
        if (t.own_delay_h >= 24.0 / static_cast<double>(out.division_reports_per_day)) {
            return Fail("picture.own_delay_h", "under the reports' period (24 / reports.division_per_day)");
        }
        return true;
    }

    // The clock and the line at U0 (F3.3).
    bool LoadClock(const json::Value& root, GroundTables& out) {
        const json::Value* c = Object(root, "", "clock");
        if (c == nullptr ||
            !Keys(*c, "clock", {"breakthrough_width_km", "taper_km", "security_zone_share", "second_echelon_after_h", "stall_halt_h",
                                "terrain_step_km", "hills_above_m", "mountains_above_m", "bridge_reach_km", "river_line_km", "notes"}) ||
            !RangeOf(*c, "clock", "breakthrough_width_km", out.breakthrough_width_km, 0.5, 50.0) ||
            !RangeOf(*c, "clock", "taper_km", out.taper_km, 0.0, 50.0) ||
            !Sourced(*c, "clock", "security_zone_share", out.security_zone_share, 0.0, 1.0) ||
            !Sourced(*c, "clock", "second_echelon_after_h", out.second_echelon_after_h, 0.0, 240.0) ||
            !Sourced(*c, "clock", "stall_halt_h", out.stall_halt_h, 1.0, 240.0) ||
            !Sourced(*c, "clock", "terrain_step_km", out.terrain_step_km, 0.1, 10.0) ||
            !Sourced(*c, "clock", "hills_above_m", out.hills_above_m, 0.0, 3000.0) ||
            !Sourced(*c, "clock", "mountains_above_m", out.mountains_above_m, 0.0, 5000.0) ||
            !Sourced(*c, "clock", "bridge_reach_km", out.bridge_reach_km, 0.0, 50.0) || !Sourced(*c, "clock", "river_line_km", out.river_line_km, 0.0, 50.0)) {
            return false;
        }
        if (out.mountains_above_m < out.hills_above_m) return Fail("clock.mountains_above_m", "at least hills_above_m");
        return true;
    }

    // ---- the force files

    // The file's place map and theatre: required, and the ones the world was loaded from.
    bool WorldOf(const json::Value& root, std::string& place_map, std::string& theatre) {
        if (!RequireString(root, "", "place_map", place_map) || !RequireString(root, "", "theatre", theatre)) return false;
        if (place_map != world_->place_map) return Fail("place_map", "'" + place_map + "', but its places were read from '" + world_->place_map + "'");
        if (theatre != world_->theatre) return Fail("theatre", "'" + theatre + "', but the theatre read is '" + world_->theatre + "'");
        return true;
    }

    // A place of the map by id, to its point (F1: the loader refuses a place the map lacks).
    bool Place(const std::string& path, const std::string& id, GroundPlace& out) {
        const MapPoint* p = world_->places.Find(id);
        if (p == nullptr) return Fail(path, "no place '" + id + "' on the map " + world_->place_map + " (add it to its sites)");
        out.at = id;
        out.pos = Vec3{p->east_m, p->north_m, 0.0};
        return true;
    }

    bool Garrisons(GroundForceDef& out) {
        out.garrisons.assign(out.oob.units.size(), GroundPlace());
        for (std::size_t i = 0; i < out.oob.units.size(); ++i) {
            const std::string& g = out.oob.units[i].garrison;
            if (!g.empty() && !Place(Join(Index("oob.units", i), "garrison"), g, out.garrisons[i])) return false;
        }
        return true;
    }

    // A sector of the front by id: its index, or a refusal.
    bool SectorIndex(const airwar::FrontDef& front, const std::string& path, const std::string& id, int& out) {
        for (std::size_t i = 0; i < front.sectors.size(); ++i) {
            if (front.sectors[i].id == id) {
                out = static_cast<int>(i);
                return true;
            }
        }
        return Fail(path, "no sector '" + id + "' on the " + front.faction + " front of " + world_->theatre);
    }

    bool Oob(const json::Value& root, command::OrderOfBattle& out) {
        const json::Value* o = Object(root, "", "oob");
        if (o == nullptr) return false;
        command::detail::OobLoader loader;
        if (!loader.Load(*o, out)) return Fail("oob", loader.Message());
        return true;
    }

    bool TraitsOf(const json::Value& v, const std::string& p, airwar::Traits& t) {
        return Keys(v, p, {"boldness", "methodical", "creativity", "rigidity", "ambition", "skepticism", "vindictiveness", "composure"}) &&
               Num(v, p, "boldness", t.boldness, 0.0, 1.0) && Num(v, p, "methodical", t.methodical, 0.0, 1.0) &&
               Num(v, p, "creativity", t.creativity, 0.0, 1.0) && Num(v, p, "rigidity", t.rigidity, 0.0, 1.0) &&
               Num(v, p, "ambition", t.ambition, 0.0, 1.0) && Num(v, p, "skepticism", t.skepticism, 0.0, 1.0) &&
               Num(v, p, "vindictiveness", t.vindictiveness, 0.0, 1.0) && Num(v, p, "composure", t.composure, 0.0, 1.0);
    }

    bool Commanders(const json::Value& root, const command::OrderOfBattle& oob, GroundCommandersDef& out) {
        const json::Value* c = Object(root, "", "commanders");
        if (c == nullptr || !Keys(*c, "commanders", {"pool", "authored", "notes"})) return false;
        const json::Value* pl = Object(*c, "commanders", "pool");
        const std::string pp = "commanders.pool";
        airwar::OfficerPoolDef& pool = out.pool;
        const auto name = [](const std::string& s) { return s.empty() ? std::string("an empty name") : std::string(); };
        if (pl == nullptr ||
            !Keys(*pl, pp, {"ranks", "first_names", "last_names", "backgrounds", "trait_mean", "trait_sd", "source", "confidence", "notes"}) ||
            !Provenance(*pl, pp) || !StringList(*pl, pp, "first_names", pool.first_names, name) ||
            !StringList(*pl, pp, "last_names", pool.last_names, name) || !StringList(*pl, pp, "backgrounds", pool.backgrounds, name) ||
            !Num(*pl, pp, "trait_sd", pool.trait_sd, 0.0, 0.5)) {
            return false;
        }
        if (pool.first_names.empty() || pool.last_names.empty()) return Fail(pp, "needs first_names and last_names");
        pool.quirks_min = pool.quirks_max = 0;  // the ground's commanders have no quirks yet (F3.4)
        const json::Value* tm = Object(*pl, pp, "trait_mean");
        if (tm == nullptr || !TraitsOf(*tm, Join(pp, "trait_mean"), pool.trait_mean)) return false;
        const json::Value* r = Object(*pl, pp, "ranks");
        if (r == nullptr) return false;
        static const char* const echelons[] = {"army", "division", "brigade", "regiment", "battalion"};
        for (std::size_t k = 0; k < r->keys.size(); ++k) {
            bool ok = false;
            for (const char* e : echelons) ok = ok || r->keys[k] == e;
            if (!ok) return Fail(Join(Join(pp, "ranks"), r->keys[k]), "unknown echelon (army, division, brigade, regiment, battalion)");
            if (!r->items[k].Is(json::Type::String) || r->items[k].string.empty()) return Fail(Join(Join(pp, "ranks"), r->keys[k]), "a rank title");
            pool.ranks[r->keys[k]] = r->items[k].string;
        }
        const json::Value* a = OptionalArray(*c, "commanders", "authored");
        if (a == nullptr) return message_.empty();
        for (std::size_t i = 0; i < a->items.size(); ++i) {
            const std::string p = Index("commanders.authored", i);
            const json::Value& v = a->items[i];
            GroundCommanderDef d;
            if (!Keys(v, p, {"id", "name", "unit", "post", "age", "background", "bio", "traits", "notes"}) || !RequireString(v, p, "id", d.id) ||
                !RequireString(v, p, "name", d.name) || !RequireString(v, p, "unit", d.unit) || !OptionalString(v, p, "post", d.post) ||
                !WholeNum(v, p, "age", d.age, 25, 75) || !OptionalString(v, p, "background", d.background) ||
                !OptionalString(v, p, "bio", d.bio)) {
                return false;
            }
            const json::Value* t = Object(v, p, "traits");
            if (t == nullptr || !TraitsOf(*t, Join(p, "traits"), d.traits)) return false;
            if (oob.Find(d.unit) == nullptr) return Fail(Join(p, "unit"), "unknown unit '" + d.unit + "'");
            for (const GroundCommanderDef& o : out.authored) {
                if (o.id == d.id) return Fail(Join(p, "id"), "'" + d.id + "' is listed twice");
                if (o.unit == d.unit) return Fail(Join(p, "unit"), "'" + d.unit + "' has two commanders");
            }
            out.authored.push_back(std::move(d));
        }
        return true;
    }

    bool Doctrine(const json::Value& root, GroundDoctrineDef& out) {
        const json::Value* d = Object(root, "", "doctrine");
        const std::string p = "doctrine";
        if (d == nullptr ||
            !Keys(*d, p, {"source", "confidence", "notes", "attack_ratio", "breakthrough_ratio", "defend_ratio", "withdraw_needs_order",
                          "counterattack_by_senior", "second_echelon_to_success", "disperse_rather_than_retreat",
                          "counterattack_stops_at_border", "echelons_against_deep_defence", "reserve_share", "covering_force_share",
                          "effectiveness", "advance_caps_km_per_day", "frontages_km", "security_zone", "prepared_positions",
                          "restore_control_h", "fscl"}) ||
            !Provenance(*d, p) || !Sourced(*d, p, "attack_ratio", out.attack_ratio, 1.0, 20.0) ||
            !RangeOf(*d, p, "breakthrough_ratio", out.breakthrough_ratio, 1.0, 20.0) || !Flag(*d, p, "withdraw_needs_order", out.withdraw_needs_order) ||
            !Flag(*d, p, "counterattack_by_senior", out.counterattack_by_senior) ||
            !Flag(*d, p, "second_echelon_to_success", out.second_echelon_to_success)) {
            return false;
        }
        if (d->Find("defend_ratio") != nullptr && !RangeOf(*d, p, "defend_ratio", out.defend_ratio, 1.0, 20.0)) return false;
        if (d->Find("disperse_rather_than_retreat") != nullptr && !Flag(*d, p, "disperse_rather_than_retreat", out.disperse_rather_than_retreat)) return false;
        if (d->Find("counterattack_stops_at_border") != nullptr && !Flag(*d, p, "counterattack_stops_at_border", out.counterattack_stops_at_border)) return false;
        if (d->Find("echelons_against_deep_defence") != nullptr && !WholeSourced(*d, p, "echelons_against_deep_defence", out.echelons_against_deep_defence, 1, 3)) return false;
        if (d->Find("reserve_share") != nullptr && !Sourced(*d, p, "reserve_share", out.reserve_share, 0.0, 0.5)) return false;
        if (d->Find("covering_force_share") != nullptr && !RangeOf(*d, p, "covering_force_share", out.covering_force_share, 0.0, 1.0)) return false;
        if (!Effectiveness(*d, out.effectiveness) || !Caps(*d, out.caps) || !Frontages(*d, out.frontages)) return false;
        if (const json::Value* fs = d->Find("fscl")) {
            const std::string fp = Join(p, "fscl");
            if (!fs->Is(json::Type::Object) || !Keys(*fs, fp, {"defence_km", "terrain_reach_km", "notes"}) ||
                !RangeOf(*fs, fp, "defence_km", out.fscl_defence_km, 0.0, 200.0) ||
                !Sourced(*fs, fp, "terrain_reach_km", out.fscl_terrain_reach_km, 0.0, 200.0)) {
                return message_.empty() ? Fail(fp, "expected an object") : false;
            }
            out.has_fscl = true;
        }
        if (const json::Value* sz = d->Find("security_zone")) {
            const std::string sp = Join(p, "security_zone");
            if (!sz->Is(json::Type::Object) || !Keys(*sz, sp, {"depth_km", "line_spacing_km", "delay_per_line_h", "notes"}) ||
                !RangeOf(*sz, sp, "depth_km", out.security_zone_depth_km, 0.0, 100.0) ||
                !RangeOf(*sz, sp, "line_spacing_km", out.security_line_spacing_km, 0.1, 50.0) ||
                !RangeOf(*sz, sp, "delay_per_line_h", out.security_delay_per_line_h, 0.0, 48.0)) {
                return message_.empty() ? Fail(sp, "expected an object") : false;
            }
            const json::Value* pp = Object(*d, p, "prepared_positions");
            const std::string ppp = Join(p, "prepared_positions");
            if (pp == nullptr || !Keys(*pp, ppp, {"division", "regiment", "notes"}) ||
                !RangeOf(*pp, ppp, "division", out.division_prepared_positions, 1.0, 10.0) ||
                !RangeOf(*pp, ppp, "regiment", out.regiment_prepared_positions, 1.0, 10.0) ||
                !RangeOf(*d, p, "restore_control_h", out.restore_control_h, 0.0, 48.0)) {
                return false;
            }
            out.has_security_zone = true;
        }
        return true;
    }

    bool Effectiveness(const json::Value& d, EffectivenessDef& out) {
        const std::string p = "doctrine.effectiveness";
        const json::Value* e = Object(d, "doctrine", "effectiveness");
        std::string kind;
        if (e == nullptr || !RequireString(*e, p, "kind", kind)) return false;
        if (kind == "lines") {
            out.kind = EffectivenessKind::Lines;
            if (!Keys(*e, p, {"kind", "degraded_below", "ineffective_below", "hq_hit_ineffective_below", "notes"}) ||
                !Sourced(*e, p, "degraded_below", out.degraded_below, 0.0, 1.0) || !Sourced(*e, p, "ineffective_below", out.ineffective_below, 0.0, 1.0) ||
                !Sourced(*e, p, "hq_hit_ineffective_below", out.hq_hit_ineffective_below, 0.0, 1.0)) {
                return false;
            }
            if (!(out.ineffective_below < out.degraded_below)) return Fail(Join(p, "ineffective_below"), "below degraded_below");
            if (out.hq_hit_ineffective_below < out.ineffective_below) return Fail(Join(p, "hq_hit_ineffective_below"), "at least ineffective_below");
            return true;
        }
        if (kind == "breakpoints") {
            out.kind = EffectivenessKind::Breakpoints;
            return Keys(*e, p, {"kind", "attack_stop_losses", "defence_break_losses", "notes"}) &&
                   RangeOf(*e, p, "attack_stop_losses", out.attack_stop_losses, 0.0, 1.0) &&
                   RangeOf(*e, p, "defence_break_losses", out.defence_break_losses, 0.0, 1.0);
        }
        return Fail(Join(p, "kind"), "lines or breakpoints");
    }

    bool Caps(const json::Value& d, AdvanceCaps& out) {
        const std::string p = "doctrine.advance_caps_km_per_day";
        const json::Value* c = Object(d, "doctrine", "advance_caps_km_per_day");
        if (c == nullptr || !Keys(*c, p, {"breakthrough", "depth", "broken", "notes"}) ||
            !Sourced(*c, p, "breakthrough", out.breakthrough_km_per_day, 0.0, 300.0) || !Sourced(*c, p, "depth", out.depth_km_per_day, 0.0, 300.0) ||
            !Sourced(*c, p, "broken", out.broken_km_per_day, 0.0, 300.0)) {
            return false;
        }
        if (out.depth_km_per_day < out.breakthrough_km_per_day || out.broken_km_per_day < out.depth_km_per_day) {
            return Fail(p, "breakthrough <= depth <= broken");
        }
        return true;
    }

    bool Frontages(const json::Value& d, std::vector<FrontageDef>& out) {
        const json::Value* a = OptionalArray(d, "doctrine", "frontages_km");
        if (a == nullptr) return message_.empty() ? Fail("doctrine.frontages_km", "required") : false;
        static const char* const echelons[] = {"army", "division", "brigade", "regiment", "battalion"};
        static const char* const postures[] = {"attack", "breakthrough", "defence"};
        for (std::size_t i = 0; i < a->items.size(); ++i) {
            const std::string p = Index("doctrine.frontages_km", i);
            const json::Value& r = a->items[i];
            FrontageDef f;
            if (!Keys(r, p, {"echelon", "posture", "width_min", "width_max", "depth_min", "depth_max", "source", "confidence", "notes"}) ||
                !Provenance(r, p) || !RequireString(r, p, "echelon", f.echelon) || !RequireString(r, p, "posture", f.posture) ||
                !Num(r, p, "width_min", f.width_km.lo, 0.0, 300.0) || !Num(r, p, "width_max", f.width_km.hi, 0.0, 300.0)) {
                return false;
            }
            bool ok_e = false;
            bool ok_p = false;
            for (const char* e : echelons) ok_e = ok_e || f.echelon == e;
            for (const char* q : postures) ok_p = ok_p || f.posture == q;
            if (!ok_e) return Fail(Join(p, "echelon"), "unknown echelon '" + f.echelon + "'");
            if (!ok_p) return Fail(Join(p, "posture"), "attack, breakthrough or defence");
            if (r.Find("depth_min") != nullptr || r.Find("depth_max") != nullptr) {
                if (!Num(r, p, "depth_min", f.depth_km.lo, 0.0, 300.0) || !Num(r, p, "depth_max", f.depth_km.hi, 0.0, 300.0)) return false;
            }
            if (f.width_km.hi < f.width_km.lo || f.depth_km.hi < f.depth_km.lo) return Fail(p, "each max at least its min");
            for (const FrontageDef& o : out) {
                if (o.echelon == f.echelon && o.posture == f.posture) return Fail(p, "'" + f.echelon + " " + f.posture + "' is listed twice");
            }
            out.push_back(std::move(f));
        }
        return true;
    }

    bool Plan(const json::Value& root, GroundForceDef& out) {
        const json::Value* pl = Object(root, "", "plan");
        const std::string p = "plan";
        GroundPlanDef& plan = out.plan;
        std::string kind;
        if (pl == nullptr ||
            !Keys(*pl, p, {"kind", "start_after_h", "max_depth_km", "source", "confidence", "notes", "objectives", "axes", "reserve", "raids",
                           "covering_force"}) ||
            !Provenance(*pl, p) || !RequireString(*pl, p, "kind", kind) || !Sourced(*pl, p, "start_after_h", plan.start_after_h, 0.0, 24.0 * 60.0) ||
            !Ids(*pl, p, "reserve", plan.reserve)) {
            return false;
        }
        if (!ParseEnum(kind, 3, PlanKindName, plan.kind)) return Fail(Join(p, "kind"), "offensive, limited_incursion or raids");
        if (pl->Find("max_depth_km") != nullptr && !Sourced(*pl, p, "max_depth_km", plan.max_depth_km, 0.0, 1000.0)) return false;
        if (plan.kind == PlanKind::LimitedIncursion && !(plan.max_depth_km > 0.0)) return Fail(Join(p, "max_depth_km"), "required for a limited incursion");
        const json::Value* obs = OptionalArray(*pl, p, "objectives");
        if (obs == nullptr && !message_.empty()) return false;
        for (std::size_t i = 0; obs != nullptr && i < obs->items.size(); ++i) {
            const std::string op = Index("plan.objectives", i);
            const json::Value& r = obs->items[i];
            ObjectiveDef o;
            std::string at;
            if (!Keys(r, op, {"id", "name", "at", "by_day", "notes"}) || !RequireString(r, op, "id", o.id) ||
                !RequireString(r, op, "name", o.name) || !RequireString(r, op, "at", at) || !Place(Join(op, "at"), at, o.place) ||
                !WholeNum(r, op, "by_day", o.by_day, 0, 60) || !OptionalString(r, op, "notes", o.notes)) {
                return false;
            }
            if (plan.Objective(o.id) != nullptr) return Fail(Join(op, "id"), "'" + o.id + "' is listed twice");
            plan.objectives.push_back(std::move(o));
        }
        // Every unit the plan names is in the order of battle, and in one role only.
        std::vector<std::string> placed;
        const auto unit_once = [&](const std::string& path, const std::string& id) {
            if (out.oob.Find(id) == nullptr) return Fail(path, "unknown unit '" + id + "'");
            for (const std::string& u : placed) {
                if (u == id || out.oob.Under(id, u) || out.oob.Under(u, id)) return Fail(path, "'" + id + "' is placed twice in the plan (itself or with its parent)");
            }
            placed.push_back(id);
            return true;
        };
        const json::Value* axes = OptionalArray(*pl, p, "axes");
        if (axes == nullptr && !message_.empty()) return false;
        for (std::size_t i = 0; axes != nullptr && i < axes->items.size(); ++i) {
            const std::string ap = Index("plan.axes", i);
            const json::Value& r = axes->items[i];
            AxisDef a;
            std::vector<std::string> route;
            if (!Keys(r, ap, {"id", "name", "main", "route", "first_echelon", "second_echelon", "objective", "sector", "notes"}) ||
                !RequireString(r, ap, "id", a.id) || !RequireString(r, ap, "name", a.name) || !OptionalBool(r, ap, "main", a.main) ||
                !Ids(r, ap, "route", route) || !Ids(r, ap, "first_echelon", a.first_echelon) || !Ids(r, ap, "second_echelon", a.second_echelon) ||
                !RequireString(r, ap, "objective", a.objective) || !RequireString(r, ap, "sector", a.sector) ||
                !SectorIndex(*front_, Join(ap, "sector"), a.sector, a.sector_index)) {
                return false;
            }
            if (route.size() < 2) return Fail(Join(ap, "route"), "at least a start and an end");
            a.route.resize(route.size());
            for (std::size_t k = 0; k < route.size(); ++k) {
                if (!Place(Index(Join(ap, "route"), k), route[k], a.route[k])) return false;
            }
            if (a.first_echelon.empty()) return Fail(Join(ap, "first_echelon"), "at least one unit");
            if (plan.Objective(a.objective) == nullptr) return Fail(Join(ap, "objective"), "unknown objective '" + a.objective + "'");
            for (const AxisDef& o : plan.axes) {
                if (o.id == a.id) return Fail(Join(ap, "id"), "'" + a.id + "' is listed twice");
                if (o.main && a.main) return Fail(Join(ap, "main"), "only one main axis");
            }
            for (std::size_t k = 0; k < a.first_echelon.size(); ++k) {
                if (!unit_once(Index(Join(ap, "first_echelon"), k), a.first_echelon[k])) return false;
            }
            for (std::size_t k = 0; k < a.second_echelon.size(); ++k) {
                if (!unit_once(Index(Join(ap, "second_echelon"), k), a.second_echelon[k])) return false;
            }
            plan.axes.push_back(std::move(a));
        }
        for (std::size_t k = 0; k < plan.reserve.size(); ++k) {
            if (!unit_once(Index("plan.reserve", k), plan.reserve[k])) return false;
        }
        if (plan.kind != PlanKind::Raids && plan.axes.empty()) return Fail("plan.axes", "an offensive or an incursion needs an axis");
        if (pl->Find("covering_force") != nullptr && !Ids(*pl, p, "covering_force", plan.covering_force)) return false;
        for (std::size_t k = 0; k < plan.covering_force.size(); ++k) {
            bool first = false;
            for (const AxisDef& a : plan.axes) first = first || std::find(a.first_echelon.begin(), a.first_echelon.end(), plan.covering_force[k]) != a.first_echelon.end();
            if (!first) return Fail(Index("plan.covering_force", k), "'" + plan.covering_force[k] + "' is not in an axis's first echelon");
        }
        if (const json::Value* rd = pl->Find("raids")) {
            const std::string rp = "plan.raids";
            RaidsDef& raids = plan.raids;
            if (!rd->Is(json::Type::Object) || !Keys(*rd, rp, {"night_only", "min_ratio", "max_depth_km", "per_night", "lines", "notes"}) ||
                !Flag(*rd, rp, "night_only", raids.night_only) || !Sourced(*rd, rp, "min_ratio", raids.min_ratio, 1.0, 20.0) ||
                !Sourced(*rd, rp, "max_depth_km", raids.max_depth_km, 0.0, 200.0) || !WholeSourced(*rd, rp, "per_night", raids.per_night, 0, 50) ||
                !Ids(*rd, rp, "lines", raids.lines)) {
                return message_.empty() ? Fail(rp, "expected an object") : false;
            }
            if (raids.lines.empty()) return Fail(Join(rp, "lines"), "at least one unit");
            for (std::size_t k = 0; k < raids.lines.size(); ++k) {
                if (!unit_once(Index(Join(rp, "lines"), k), raids.lines[k])) return false;
            }
            plan.has_raids = true;
        }
        if (plan.kind == PlanKind::Raids && !plan.has_raids) return Fail("plan.raids", "required for raids");
        return true;
    }

    // ---- our side

    bool Deployments(const json::Value& root, GroundDefenceDef& out) {
        const json::Value* a = OptionalArray(root, "", "deployments");
        if (a == nullptr || a->items.empty()) return message_.empty() ? Fail("deployments", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < a->items.size(); ++i) {
            const std::string p = Index("deployments", i);
            const json::Value& r = a->items[i];
            DeploymentDef d;
            if (!Keys(r, p, {"adversary", "security_zone_km", "source", "confidence", "notes", "units"}) || !Provenance(r, p) ||
                !RequireString(r, p, "adversary", d.adversary) || !Sourced(r, p, "security_zone_km", d.security_zone_km, 0.0, 100.0)) {
                return false;
            }
            if (out.For(d.adversary) != nullptr) return Fail(Join(p, "adversary"), "'" + d.adversary + "' is listed twice");
            const airwar::FrontDef* front = world_->FrontOf(d.adversary);
            if (front == nullptr) return Fail(Join(p, "adversary"), "the theatre " + world_->theatre + " has no front of '" + d.adversary + "'");
            const json::Value* us = OptionalArray(r, p, "units");
            if (us == nullptr || us->items.empty()) return message_.empty() ? Fail(Join(p, "units"), "required, a non-empty list") : false;
            for (std::size_t k = 0; k < us->items.size(); ++k) {
                const std::string up = Index(Join(p, "units"), k);
                const json::Value& v = us->items[k];
                DeployedUnitDef du;
                std::string role;
                std::string at;
                if (!Keys(v, up, {"unit", "role", "sectors", "at", "notes"}) || !RequireString(v, up, "unit", du.unit) ||
                    !RequireString(v, up, "role", role) || !OptionalString(v, up, "at", at) || !Ids(v, up, "sectors", du.sectors)) {
                    return false;
                }
                if (!at.empty() && !Place(Join(up, "at"), at, du.place)) return false;
                if (!ParseEnum(role, 5, DeployRoleName, du.role)) {
                    return Fail(Join(up, "role"), "first_echelon, second_echelon, flank_screen, reserve or support");
                }
                if (out.oob.Find(du.unit) == nullptr) return Fail(Join(up, "unit"), "unknown unit '" + du.unit + "' in " + out.oob_file);
                for (const DeployedUnitDef& o : d.units) {
                    if (o.unit == du.unit || out.oob.Under(du.unit, o.unit) || out.oob.Under(o.unit, du.unit)) {
                        return Fail(Join(up, "unit"), "'" + du.unit + "' is deployed twice (itself or with its parent)");
                    }
                }
                const bool on_line = du.role == DeployRole::FirstEchelon || du.role == DeployRole::SecondEchelon || du.role == DeployRole::FlankScreen;
                if (on_line) {
                    // A run of the front's sectors, each the next along the line.
                    if (du.sectors.empty()) return Fail(Join(up, "sectors"), "a unit on the line holds at least one sector");
                    for (std::size_t s = 0; s < du.sectors.size(); ++s) {
                        int idx = -1;
                        if (!SectorIndex(*front, Index(Join(up, "sectors"), s), du.sectors[s], idx)) return false;
                        if (s > 0 && idx != du.last_sector + 1) return Fail(Index(Join(up, "sectors"), s), "not the next sector along the line after '" + du.sectors[s - 1] + "'");
                        if (s == 0) du.first_sector = idx;
                        du.last_sector = idx;
                    }
                    du.from_km = front->sectors[static_cast<std::size_t>(du.first_sector)].from_m / 1000.0;
                    du.to_km = front->sectors[static_cast<std::size_t>(du.last_sector)].to_m / 1000.0;
                } else {
                    if (!du.sectors.empty()) return Fail(Join(up, "sectors"), "a reserve or support unit holds no stretch of the line");
                    if (du.place.at.empty()) return Fail(Join(up, "at"), "required for a reserve or support unit");
                }
                d.units.push_back(std::move(du));
            }
            out.deployments.push_back(std::move(d));
        }
        return true;
    }

    const GroundWorld* world_ = nullptr;
    const airwar::FrontDef* front_ = nullptr;  // a force file's faction's front
};

template <typename Fn>
bool LoadGroundJsonWith(const std::string& text, const char* what, std::string* error, Fn&& fn) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) {
        if (error) *error = std::string(what) + ": " + parse_error;
        return false;
    }
    GroundLoader loader;
    if (!fn(loader, root)) {
        if (error) *error = std::string(what) + ": " + loader.Message();
        return false;
    }
    return true;
}

}  // namespace detail

// ------------------------------------------------------------------------------------------------------- the loaders

inline bool LoadGroundTablesJson(const std::string& text, GroundTables& out, std::string* error = nullptr) {
    return detail::LoadGroundJsonWith(text, "ground war tables", error,
                                      [&out](detail::GroundLoader& l, const json::Value& root) { return l.LoadTables(root, out); });
}

// data/gameplay/ground_war.json.
inline bool LoadGroundTables(const std::string& data_dir, GroundTables& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/gameplay/ground_war.json", text)) {
        if (error) *error = "cannot open gameplay/ground_war.json";
        return false;
    }
    return LoadGroundTablesJson(text, out, error);
}

inline bool LoadGroundForceJson(const std::string& text, const GroundWorld& world, GroundForceDef& out, std::string* error = nullptr) {
    return detail::LoadGroundJsonWith(text, "ground forces", error,
                                      [&](detail::GroundLoader& l, const json::Value& root) { return l.LoadForce(root, world, out); });
}

namespace detail {

// A ground file's text and the world it names ("place_map", "theatre"), loaded; false with the file's message if it
// names none (the file's own loader then says what is missing).
inline bool ReadGroundFile(const std::string& data_dir, const std::string& file, std::string& text, GroundWorld& world, bool& named,
                           std::string* error, const MapPlaces* places) {
    named = false;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + file, text)) {
        if (error) *error = "cannot open " + file;
        return false;
    }
    json::Value root;
    std::string place_map;
    std::string theatre;
    if (!json::Parse(text, root, nullptr) || !GroundLoader::FileOf(root, "place_map", place_map) || !GroundLoader::FileOf(root, "theatre", theatre)) {
        return true;
    }
    named = true;
    std::string e;
    if (!LoadGroundWorld(data_dir, place_map, theatre, world, &e, places)) {
        if (error) *error = file + ": " + e;
        return false;
    }
    return true;
}

}  // namespace detail

// data/air_war/ground_<faction>.json, on the places and theatre it names (`places`: the map's, moved, for a test).
inline bool LoadGroundForce(const std::string& data_dir, const std::string& faction, GroundForceDef& out, std::string* error = nullptr,
                            const MapPlaces* places = nullptr) {
    const std::string file = "air_war/ground_" + faction + ".json";
    std::string text;
    GroundWorld world;
    bool named = false;
    if (!detail::ReadGroundFile(data_dir, file, text, world, named, error, places)) return false;
    if (!LoadGroundForceJson(text, world, out, error)) {
        if (error) *error = file + ": " + *error;
        return false;
    }
    if (out.faction != faction) {
        if (error) *error = file + ": faction '" + out.faction + "', expected '" + faction + "'";
        return false;
    }
    return true;
}

// Our side's defence; `oob` is the order of battle its "oob" names (command::LoadOrderOfBattle), `world` the places and
// theatre it names.
inline bool LoadGroundDefenceJson(const std::string& text, const command::OrderOfBattle& oob, const GroundWorld& world, GroundDefenceDef& out,
                                  std::string* error = nullptr) {
    return detail::LoadGroundJsonWith(text, "ground defence", error,
                                      [&](detail::GroundLoader& l, const json::Value& root) { return l.LoadDefence(root, oob, world, out); });
}

// data/<file> (default campaign/ground_defence.json) with the order of battle, places and theatre it names.
inline bool LoadGroundDefence(const std::string& data_dir, GroundDefenceDef& out, std::string* error = nullptr,
                              const std::string& file = "campaign/ground_defence.json", const MapPlaces* places = nullptr) {
    std::string text;
    GroundWorld world;
    bool named = false;
    if (!detail::ReadGroundFile(data_dir, file, text, world, named, error, places)) return false;
    json::Value root;
    std::string oob_file;
    command::OrderOfBattle oob;
    if (named && json::Parse(text, root, nullptr) && detail::GroundLoader::FileOf(root, "oob", oob_file) &&
        !command::LoadOrderOfBattle(data_dir, oob_file, oob, error)) {
        return false;
    }
    if (!LoadGroundDefenceJson(text, oob, world, out, error)) {
        if (error) *error = file + ": " + *error;
        return false;
    }
    return true;
}

}  // namespace groundwar
}  // namespace simcore
