// simcore/ground_check.hpp — roadmap R5, "nothing under the map": everything the game draws, listed where it is under
// the ground (or a ground item floating over it), from the simulation itself.
//
// The game draws what the runner gives it on the ground the map draws (MapData::DrawnLevels, the triangles of
// MapGrid::SampleTri), and that is the ground the sim samples (one ground: terrain_map.hpp). So what is under the
// sim's ground is under the drawn ground:
//   * an aircraft whose lowest point (its centre less Aircraft::bottom_m) is under the ground;
//   * a weapon in flight (a bomb's or cruise missile's body), our missiles and the enemy's ARMs, by their centre (their
//     own kinds, "weapon", "missile", "arm": a missile skimming the ground is triaged, not taken for an aircraft);
//   * a ground item off its ground point: the battery's vehicles and dummies and the works (at the ground point each
//     row carries: the data's z is an antenna's, roadmap R5 step 2b), the visitors and the world's movers (at their
//     positions); our neighbours' sites, their launchers, the scenery and our own vehicle are drawn on the ground
//     under their data's east and north (GroundEnu), and that point is checked.
// Listed when more than `tolerance_m` under the ground, a ground item also when more than `float_m` over it.
// simcore/examples/ground_check.cpp runs it over the testbed's hour and every map mission (docs/TEST_RANGE.md).
#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "simcore/scenario_runner.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct BelowGround {
    double t_s = 0.0;
    std::string id;
    std::string kind;     // aircraft, weapon, missile, arm, battery, work, visitor, mover, neighbour, launcher, scenery, own
    Vec3 position;        // ENU, the point judged (an aircraft's centre, a ground item's ground point)
    double below_m = 0.0;  // how far its lowest point is under the ground (negative: over it, floating)
    bool floating = false;

    std::string Line() const {
        char text[256];
        std::snprintf(text, sizeof(text), "%8.1f s  %-9s %-28s %s %.2f m at (%.0f, %.0f)", t_s, kind.c_str(), id.c_str(),
                      floating ? "floats" : "below ground by", floating ? -below_m : below_m, position.x, position.y);
        return text;
    }
};

// Everything the runner draws, judged against its ground now (see above).
inline std::vector<BelowGround> EntitiesOffGround(ScenarioRunner& r, double tolerance_m = 0.05, double float_m = 0.5) {
    std::vector<BelowGround> out;
    const LocalFrame& frame = r.GetWorld().Frame();
    const double t = r.Time();
    const auto under = [&](const Vec3& p, double bottom_m) { return r.GroundHeightAt(p.x, p.y) - (frame.AltitudeOf(p) - bottom_m); };
    const auto air = [&](const std::string& id, const std::string& kind, const Vec3& p, double bottom_m) {
        const double below = under(p, bottom_m);
        if (below > tolerance_m) out.push_back({t, id, kind, p, below, false});
    };
    const auto ground = [&](const std::string& id, const std::string& kind, const Vec3& p) {
        const double below = under(p, 0.0);
        if (below > tolerance_m) out.push_back({t, id, kind, p, below, false});
        else if (-below > float_m) out.push_back({t, id, kind, p, below, true});
    };
    for (const Aircraft& a : r.GetWorld().AircraftList()) {
        if (!a.alive) continue;
        const bool weapon = a.type.rfind("Weapon", 0) == 0;
        air(a.name, weapon ? "weapon" : "aircraft", a.position, weapon ? 0.0 : a.bottom_m);
    }
    for (const Missile& m : r.GetWorld().MissileList()) {
        if (m.Active()) air("missile " + std::to_string(m.id), "missile", m.position, 0.0);
    }
    for (const EngagementModel::MissileView& m : r.MissilesInFlight()) air("missile " + std::to_string(m.number), "missile", m.position, 0.0);
    for (const EmSensorLink::ArmView& a : r.ArmsInFlight()) air(a.name, "arm", a.position, 0.0);
    const em::ViewExtras x = r.ViewExtrasNow(0);
    for (const em::ViewVehicle& v : x.battery) ground(v.id, "battery", v.ground);
    for (const em::ViewWork& k : x.works) ground("works", "work", k.ground);
    for (const ScenarioRunner::VisitorState& v : r.Visitors()) ground(v.id, "visitor", v.position);
    for (const ScenarioRunner::WorldMover& m : r.MoversInView()) ground(m.mover, "mover", m.position);
    for (const ScenarioNeighbour& nb : r.Neighbours()) ground(nb.id, "neighbour", r.GroundEnu(nb.position.x, nb.position.y));
    for (const std::pair<std::string, Vec3>& l : r.NeighbourLaunchers()) ground(l.first, "launcher", r.GroundEnu(l.second.x, l.second.y));
    for (const ScenarioScenery& s : r.Scenery()) ground(s.id, "scenery", r.GroundEnu(s.position.x, s.position.y));
    ground("own", "own", r.GroundEnu(r.UnitPosition().x, r.UnitPosition().y));
    return out;
}

}  // namespace simcore
