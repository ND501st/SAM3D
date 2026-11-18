// simcore/air_war/package_control.hpp — who may order an enemy flight in a session (roadmap B5,
// docs/air_battle/05_C2_AND_RETASKING.md §4.2 and §4.6).
//
// A session's flights (flights.hpp) take orders from their own leader, their package's commander, a pilot of their own,
// or the instructor; and from the faction's command posts on the ground. Each post's order is an act of the authority
// table (C2Authority::May): a divert, a hold or an orbit is "divert_airborne", a recall "recall", an attack on a new aim
// "designate", an evade "warn". Following the route again or rejoining is always allowed. The rule is set on the session
// with ScenarioRunner::SetFlightAuthority; LivePackages sets it for the campaign's posts.
#pragma once

#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/c2.hpp"
#include "simcore/flights.hpp"

namespace simcore {
namespace airwar {

// The authority table's act an order is (05 §4.2); "" for an order anyone may give (the route again, rejoin).
inline const char* C2ActOf(OrderKind k) {
    switch (k) {
        case OrderKind::Divert:
        case OrderKind::Hold:
        case OrderKind::Orbit: return "divert_airborne";
        case OrderKind::ReturnHome: return "recall";
        case OrderKind::Attack: return "designate";
        case OrderKind::Evade: return "warn";
        case OrderKind::FollowRoute:
        case OrderKind::Rejoin: return "";
    }
    return "";
}

// An order from inside the package: its leader, its commander, a pilot, the instructor (or a session's own test call).
inline bool OwnCommand(const std::string& by) {
    return by.empty() || by == "leader" || by == "package" || by == "pilot" || by == "instructor" || by == "the session";
}

// The package id a session flight's package names ("p12" -> 12), or -1.
inline int SessionPackageId(const std::string& package) {
    if (package.size() < 2 || package[0] != 'p') return -1;
    for (std::size_t i = 1; i < package.size(); ++i) {
        if (package[i] < '0' || package[i] > '9') return -1;
    }
    return std::atoi(package.c_str() + 1);
}

struct PackageControl {
    // A post's kind by its id (a post on each entry is "<id>@<entry>"); "" if it is no post.
    using PostKind = std::function<std::string(const std::string& by)>;
    // The flight's mission as the authority table names it (C2MissionOf).
    using MissionOf = std::function<std::string(const FlightState&)>;
    // Where the flight (or the order's aim) lies for the post.
    using WhereOf = std::function<C2Where(const std::string& post, const FlightState&, const FlightOrder&)>;

    // May the order `o` be given to `f`? Our own side's flights and the package's own command always may; a post may by
    // its line in the table; anyone else may not.
    static bool May(const FactionC2& c2, int year, const FlightState& f, const FlightOrder& o, const PostKind& kind_of, const MissionOf& mission_of,
                    const WhereOf& where_of) {
        if (f.side != "enemy" || OwnCommand(o.by)) return true;
        const std::string act = C2ActOf(o.kind);
        if (act.empty()) return true;
        const std::string kind = kind_of ? kind_of(o.by) : std::string();
        if (kind.empty()) return false;
        const std::string mission = mission_of ? mission_of(f) : std::string("any");
        const C2Where where = where_of ? where_of(o.by, f, o) : C2Where{true, false};
        return C2Authority::May(c2, kind, act, mission, where, year);
    }

    // The rule for ScenarioRunner::SetFlightAuthority, over the faction's table and the posts the campaign placed.
    static Flights::Authority Rule(const AirWarData& d, PostKind kind_of, MissionOf mission_of, WhereOf where_of = WhereOf()) {
        const AirWarData* dp = &d;
        return [dp, kind_of, mission_of, where_of](const FlightState& f, const FlightOrder& o) {
            return May(dp->faction.c2, dp->theatre.year, f, o, kind_of, mission_of, where_of);
        };
    }

    // A post kind lookup over a campaign's posts.
    static PostKind KindsOf(const std::vector<C2PostState>& posts) {
        return [posts](const std::string& by) {
            for (const C2PostState& p : posts) {
                if (p.id == by) return p.kind;
            }
            return std::string();
        };
    }
};

}  // namespace airwar
}  // namespace simcore
