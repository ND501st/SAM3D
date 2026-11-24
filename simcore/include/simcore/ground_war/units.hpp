// simcore/ground_war/units.hpp — a ground unit's strength, power and effectiveness (track F, F3.2;
// docs/fronts/03_GROUND_WAR.md sections 3.1 and 5): strength is the unit's equipment by type times a weight per type
// (Perventsev and Sokolov's method, mt_perventsev_balance, the weights recomputed for 1985 as SAM3D estimates) plus its
// people's share; its strength fraction is that over its war establishment; its power is strength times its supply and
// cohesion. Effectiveness is the Pact's and our lines (2/3 and 1/2, mt_novoselov_replacement, mt_stashek_losses) or a
// breakpoint drawn per unit (NATO and the small forces, clark_1954_oro).
//
// Pure functions over the tables (ground_data.hpp). Weights are looked up by type when a unit is built, never per step.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "simcore/command/oob.hpp"
#include "simcore/ground_war/ground_data.hpp"

namespace simcore {
namespace groundwar {

enum class Side : std::uint8_t { Ours, Enemy };
enum class UnitState : std::uint8_t { InLine, Reserve, March, Dispersed, Reforming };
enum class Effectiveness : std::uint8_t { Effective, Degraded, Ineffective };

inline const char* EffectivenessName(Effectiveness e) {
    static const char* const names[] = {"effective", "degraded", "ineffective"};
    return names[static_cast<std::size_t>(e)];
}

// The weighted strength of equipment and people, in T-55 equivalents. Types not in the table weigh nothing.
inline double StrengthOf(const GroundTables& t, const std::vector<std::pair<std::string, int>>& equipment, int personnel) {
    double s = t.personnel_per_man * static_cast<double>(personnel);
    for (const std::pair<std::string, int>& e : equipment) s += t.WeightOf(e.first) * static_cast<double>(e.second);
    return s;
}

// A unit's own strength (its equipment and people, not its children's).
inline double StrengthOfUnit(const GroundTables& t, const command::OobUnit& u) { return StrengthOf(t, u.equipment, u.personnel.Total()); }

// A unit's strength with everything under it.
inline double StrengthOfTree(const GroundTables& t, const command::OrderOfBattle& oob, const std::string& id) {
    double s = 0.0;
    for (const command::OobUnit& u : oob.units) s += oob.Under(u.id, id) ? StrengthOfUnit(t, u) : 0.0;
    return s;
}

// The equipment types of an order of battle the weights do not list (support and air defence, or a misspelt id).
inline std::vector<std::string> UnweightedTypes(const GroundTables& t, const command::OrderOfBattle& oob) {
    std::vector<std::string> out;
    for (const command::OobUnit& u : oob.units) {
        for (const std::pair<std::string, int>& e : u.equipment) {
            bool known = false;
            t.WeightOf(e.first, &known);
            if (!known && std::find(out.begin(), out.end(), e.first) == out.end()) out.push_back(e.first);
        }
    }
    return out;
}

// Supply and cohesion's share of a unit's power: each scales linearly from its floor (none) to 1 (full).
inline double SupplyCohesionFactor(const GroundTables& t, double supply, double cohesion) {
    const double s = std::min(1.0, std::max(0.0, supply));
    const double c = std::min(1.0, std::max(0.0, cohesion));
    return (t.supply_floor + (1.0 - t.supply_floor) * s) * (t.cohesion_floor + (1.0 - t.cohesion_floor) * c);
}

// A unit's power before its posture and the ground: strength x supply x cohesion (docs/fronts/03_GROUND_WAR.md §4).
inline double UnitPower(const GroundTables& t, double strength, double supply, double cohesion) {
    return strength * SupplyCohesionFactor(t, supply, cohesion);
}

// The Pact's and our lines: degraded below 2/3, ineffective below 1/2, or below 3/4 with its command hit.
inline Effectiveness EffectivenessByLines(const EffectivenessDef& e, double strength_fraction, bool hq_hit) {
    if (strength_fraction < e.ineffective_below || (hq_hit && strength_fraction < e.hq_hit_ineffective_below)) return Effectiveness::Ineffective;
    if (strength_fraction < e.degraded_below) return Effectiveness::Degraded;
    return Effectiveness::Effective;
}

// A threshold moved by a trait, by at most `share` of its range (docs/fronts/README.md decision 6): `pull` in [-1, 1]
// (a trait's 0..1 as 2 x trait - 1), the result kept inside the range.
inline double ShiftedThreshold(double base, const ValueRange& range, double pull, double share) {
    const double p = std::min(1.0, std::max(-1.0, pull));
    const double v = base + p * share * (range.hi - range.lo);
    return std::min(range.hi, std::max(range.lo, v));
}

// A unit's breakpoint (cumulative losses at which an attack stops or a defence breaks), drawn at `u` in [0, 1] from its
// mission's range and moved by `pull` (ShiftedThreshold).
inline double DrawBreakpoint(const EffectivenessDef& e, bool attacking, double u, double pull, double share) {
    const ValueRange& r = attacking ? e.attack_stop_losses : e.defence_break_losses;
    return ShiftedThreshold(r.At(std::min(1.0, std::max(0.0, u))), r, pull, share);
}

// A ground unit as the ground war holds it (F3.3 steps them; F3.2 only builds them and keeps their strength).
struct GroundUnit {
    std::string id;
    Side side = Side::Ours;
    int parent = -1;  // an index into the same list
    std::vector<int> children;
    double establishment = 0.0;  // its own strength at war establishment
    double strength = 0.0;       // its own strength now
    double cohesion = 1.0;
    double supply = 1.0;
    Posture posture = Posture::HastyDefence;
    UnitState state = UnitState::Reserve;

    double Fraction() const { return establishment > 0.0 ? strength / establishment : 0.0; }
};

// The level a formation is stepped at (docs/fronts/03_GROUND_WAR.md section 3.3; F3.6): U0, the formation as the actor
// with its regiments a list of strengths inside it; U1, in the player's sector and its neighbours, its regiments actors
// with their own stations, posture, cohesion, commander and reports. U2 (movers in a session) is F5.2's (session_units.hpp).
enum class UnitLevel : std::uint8_t { U0, U1 };

inline const char* UnitLevelName(UnitLevel l) {
    switch (l) {
        case UnitLevel::U0: return "U0";
        case UnitLevel::U1: return "U1";
    }
    return "?";
}

// A formation's regiment, brigade or battalion (its order of battle's child with weighted equipment), an actor at U1
// (F3.6). Its strength is its share of its formation's: `part` of the formation's strength now (the parts sum to 1) and
// `est_part` of its establishment, so the formation's strength is always the sum of its regiments' and a level change
// keeps it to the bit. At U0 the parts stand still (the formation's losses come off every regiment in proportion); at U1
// the formation's losses are shared out by where each regiment stands (ground_war.hpp).
struct Regiment {
    std::string id;
    std::string name;
    std::string kind;
    int formation = -1;   // its formation (GroundWar::Formations())
    int cmdr = -1;        // its commander (GroundWar::RegimentCommanders())
    double est_part = 0.0;
    double part = 0.0;
    double cohesion = 1.0;
    bool support = false;  // no tanks (the artillery): fights with the formation from behind, never in an echelon of its own
    bool first = false;    // in its formation's first echelon
    double from_m = 0.0;   // the stations it holds along the line (first echelon; the second echelon behind the same run)
    double to_m = 0.0;
    Posture posture = Posture::HastyDefence;
    UnitState state = UnitState::Reserve;
    std::string anchor;    // the map's anchor it stands at (the route service asks between anchors), "" none
    Vec3 pos;
    // Its commander's dealings over its strength.
    std::uint8_t crossed = 0;  // the lines it has crossed (GroundWar's bits)
    bool reported = false;
    bool asked = false;        // asked its formation's commander to be relieved
    double asked_h = -1.0;
    int relief_for = -1;       // marching to relieve this regiment of its formation
    int relieved_by = -1;      // the regiment marching to relieve it
    int rotations = 0;
};

// One GroundUnit per unit of the order of battle, in its order, at full strength.
inline std::vector<GroundUnit> UnitsOf(const GroundTables& t, const command::OrderOfBattle& oob, Side side) {
    std::vector<GroundUnit> out;
    out.reserve(oob.units.size());
    for (const command::OobUnit& u : oob.units) {
        GroundUnit g;
        g.id = u.id;
        g.side = side;
        g.establishment = g.strength = StrengthOfUnit(t, u);
        out.push_back(std::move(g));
    }
    for (std::size_t i = 0; i < oob.units.size(); ++i) {
        const std::string& parent = oob.units[i].parent;
        if (parent.empty()) continue;
        for (std::size_t k = 0; k < oob.units.size(); ++k) {
            if (oob.units[k].id == parent) {
                out[i].parent = static_cast<int>(k);
                out[k].children.push_back(static_cast<int>(i));
                break;
            }
        }
    }
    return out;
}

// A unit's strength (or establishment) with everything under it, over the list UnitsOf built.
inline double TreeStrength(const std::vector<GroundUnit>& units, int index, bool establishment = false) {
    if (index < 0 || static_cast<std::size_t>(index) >= units.size()) return 0.0;
    const GroundUnit& g = units[static_cast<std::size_t>(index)];
    double s = establishment ? g.establishment : g.strength;
    for (int c : g.children) s += TreeStrength(units, c, establishment);
    return s;
}

}  // namespace groundwar
}  // namespace simcore
