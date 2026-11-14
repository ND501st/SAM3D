// simcore/fx/events.hpp — the presentation feed (docs/fx/README.md section 3.1): what happened, for show. A missile
// leaving its launcher and where it ends, an aircraft going down, bombs and rockets released and where they land, a
// neighbour's missiles and guns, an anti-radiation missile's burst: each one event, with where it starts, how it moves,
// where it ends and when.
//
// The simulation fills it where its models already decide (ScenarioRunner, air_war/mission_bridge.hpp) and never reads
// it back: nothing here changes what happens, and the crew still learn what happened only through their instruments,
// their eyes and ears, and the people who tell them (first person). Unreal reads it with its own cursor
// (Feed::Since(seq)) and turns each event into effects and sounds on the simulation's clock.
//
// What a field means for each kind (unused fields keep their defaults):
//   MissileLaunch  position: the launcher's rails (3.5 m over the ground); velocity: the launch direction x 30 m/s;
//                  target; number: the missile's (a launch's count from 1; a missile thrown off unguided, АВАРИЙНЫЙ ПУСК,
//                  1000 + its count); end_t_s: the planned meeting (its self-destruction with no target); weapon: our
//                  system's id.
//   MissileEnd     position, velocity: the missile's last display state; aim: the burst point now (Hit, NearBurst), or
//                  where it destroys itself at end_t_s, always after t_s (Wide, Early: it flies on, 25-28 s from its
//                  launch); end; number; target; fire/falls: a hit's wreck.
//   SamLaunch      position: the site; velocity: the launch direction; aim: the target where it was; end_t_s: the
//                  planned meeting; world_id/target: the aircraft; source: the site; number: its launch; weapon: its
//                  system id.
//   SamEnd         position: the site; aim: the burst point (Hit), or where it destroys itself at end_t_s
//                  (SelfDestruct); world_id/target: the aircraft; source; number; weapon.
//   AircraftDown   position, velocity: the aircraft's; world_id; target; fire, falls; model.
//   WeaponRelease  position, velocity: the aircraft's; aim: the impact point (on the ground); end_t_s: the impact time;
//                  world_id; source: the aircraft; weapon, number (count), delivery.
//   WeaponImpact   position = aim: the impact point; weapon, number, source; charge_kg (0: munitions.json's). t_s: when
//                  it lands (a load still falling when a raid's session ends lands at its own time, pushed at the end).
//   CruiseImpact   position: where it arrives; velocity: its last; aim: the impact point (on the ground); world_id,
//                  source: the missile; weapon: its type. No AircraftDown follows: its warhead is the end of it.
//   ArmLaunch      position, velocity: the carrier's; aim: our radar's antenna (4 m over the ground where the vehicle
//                  stands); end_t_s: its arrival; world_id, source: the carrier; weapon.
//   ArmBurst       position = aim: the burst point; end (Hit, or Ground for a miss); source; weapon.
//   GunPass        position: the site (on the ground); world_id/target: the aircraft; t_s..end_t_s: the pass; number:
//                  the rounds; source: the site; weapon: its system id; aim: the aircraft where the pass began. Its fire
//                  control's errors through the pass (simcore/aaa.hpp), so the picture's rounds miss as the
//                  simulation's: aim_across_rad, aim_up_rad (across the line of sight), range_share, lead_error (the
//                  measured velocity's, m/s), each times exp(-correction_per_s x the time into the pass), and
//                  spread_mrad (a round's scatter). All zero: a perfect lead and the picture's own scatter.
//   FlakBurst      a time-fuzed shell (the simulation's, aaa.hpp): t_s its firing, end_t_s its burst; position: the
//                  muzzle; velocity: its line of fire x its muzzle velocity; aim: where it bursts (fx::RoundAt along that
//                  line at end_t_s - t_s); world_id/target: the aircraft; source: the site; weapon: its system id.
//   AircraftDamaged world_id/target: the aircraft; position, velocity: its; number: its damage level now (2 moderate,
//                  3 heavy: simcore::DamageLevel); fire: it burns; falls: it trails smoke; source: who did it.
// `weapon` is a munitions.json id, a data/air_war/weapons.json id, or an air defence system id (air_war/data.hpp
// AirDefenceSystems: "osa", "zsu234", ...): the munitions library resolves any of them.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {
namespace fx {

enum class EventKind {
    MissileLaunch,   // our missile leaves the launcher (number, launch position, direction, target)
    MissileEnd,      // our missile's end: burst now (Hit, NearBurst) or flies on to self-destruct (Wide, Early)
    SamLaunch,       // a neighbour's or the bridge's defence missile leaves its site (system, site, target)
    SamEnd,          // that missile bursts (hit or not)
    AircraftDown,    // an aircraft (or helicopter, drone) destroyed: the wreck's start (fire, falls)
    WeaponRelease,   // bombs, rockets, missiles, gun bursts leave an aircraft (weapon id, count, delivery, impact)
    WeaponImpact,    // they arrive (weapon id, count, where, charge)
    CruiseImpact,    // a cruise missile's warhead where it arrives
    ArmLaunch,       // an anti-radiation missile fired (bridge model; EM ARMs are entities)
    ArmBurst,        // an ARM bursts (EM results and the bridge model)
    GunPass,         // a defence's guns fire at an aircraft through a pass (system, site, target, duration, rounds)
    FlakBurst,       // a time-fuzed shell fired, and where and when it bursts
    AircraftDamaged, // an aircraft damaged and flying on: smoke, fire
};

inline const char* ToString(EventKind k) {
    switch (k) {
        case EventKind::MissileLaunch: return "missile_launch";
        case EventKind::MissileEnd: return "missile_end";
        case EventKind::SamLaunch: return "sam_launch";
        case EventKind::SamEnd: return "sam_end";
        case EventKind::AircraftDown: return "aircraft_down";
        case EventKind::WeaponRelease: return "weapon_release";
        case EventKind::WeaponImpact: return "weapon_impact";
        case EventKind::CruiseImpact: return "cruise_impact";
        case EventKind::ArmLaunch: return "arm_launch";
        case EventKind::ArmBurst: return "arm_burst";
        case EventKind::GunPass: return "gun_pass";
        case EventKind::FlakBurst: return "flak_burst";
        case EventKind::AircraftDamaged: return "aircraft_damaged";
    }
    return "?";
}

// How a missile or a munition ends.
enum class BurstEnd {
    Hit,           // on the target (inside the warhead's reach)
    NearBurst,     // the fuse fired beside it (10-25 m off)
    Wide,          // it passed by and flies on to its self-destruction
    Early,         // the missile tracker lost it before the meeting; it flies on to destroy itself
    SelfDestruct,  // it destroyed itself (a neighbour's missile that missed)
    Ground,        // on the ground (an ARM that missed)
};

inline const char* ToString(BurstEnd e) {
    switch (e) {
        case BurstEnd::Hit: return "hit";
        case BurstEnd::NearBurst: return "near_burst";
        case BurstEnd::Wide: return "wide";
        case BurstEnd::Early: return "early";
        case BurstEnd::SelfDestruct: return "self_destruct";
        case BurstEnd::Ground: return "ground";
    }
    return "?";
}

// When a missile that missed and flies on (a MissileEnd that is Wide or Early) destroys itself: `u` (0..1, the
// presentation's draw) of the way through its window, `min_s`..`max_s` after its launch at `launch_t_s`, and never sooner
// than `fly_on_min_s` after `now_s`, when its end is decided. An "out of reach" miss is decided at the end of its flight
// time, which may be inside the window: without the floor it would end now, which the game shows as a burst on an
// aircraft that flies on. Past the window's end it destroys itself `fly_on_min_s` after now.
inline double SelfDestructTime(double launch_t_s, double now_s, double u, double min_s, double max_s, double fly_on_min_s) {
    const double lo = std::max(min_s, now_s - launch_t_s + fly_on_min_s);
    const double hi = std::max(lo, max_s);
    return launch_t_s + lo + (hi - lo) * std::min(1.0, std::max(0.0, u));
}

struct Event {
    std::uint64_t seq = 0;      // set by Feed::Push, rising from 1
    EventKind kind = EventKind::MissileLaunch;
    double t_s = 0.0;           // simulation time it happens (a future impact's time is in `end_t_s`)
    Vec3 position, velocity;    // ENU m, m/s: where it starts and how it moves
    Vec3 aim;                   // ENU: impact point, burst point, the target's position at the time
    double end_t_s = 0.0;       // when it ends (impact, burst, end of a gun pass)
    std::string source;         // who: the target id of an aircraft, a neighbour id, "own" for our launcher
    std::string target;         // at whom/what: a target id
    int world_id = -1;          // the aircraft's World id (= Unreal entity id) for AircraftDown, WeaponRelease, GunPass,
                                // SamLaunch, SamEnd (the target), WeaponImpact, ArmLaunch, CruiseImpact (the aircraft)
    int number = 0;             // our missile's number (MissileLaunch/End; 1000 + n thrown off), a site's launch, or the
                                // count (bombs, rounds)
    std::string weapon;         // munitions.json id (weapons.json id, "9m33", a system's missile, "zu23" for guns)
    std::string delivery;       // weapons.json delivery for WeaponRelease (laydown, loft, popup, dive, level, standoff, hover_fire)
    BurstEnd end = BurstEnd::Hit;
    double charge_kg = 0.0;     // TNT-equivalent of one warhead (0: munitions.json's)
    bool fire = false;          // AircraftDown: it burns; WeaponImpact: it set something alight
    bool falls = false;         // AircraftDown: it drops out of the sky at once
    std::string model;          // AircraftDown: the basic model id of the wreck
    // GunPass: the fire control's errors through the pass (aaa.hpp).
    double aim_across_rad = 0.0;
    double aim_up_rad = 0.0;
    double range_share = 0.0;
    Vec3 lead_error;            // m/s
    double correction_per_s = 0.0;
    double spread_mrad = 0.0;
};

// Bounded: the oldest are dropped past `capacity` (default 4096); a reader that fell behind skips them. The sequence
// numbers keep rising across Clear(), so a reader's cursor stays valid.
class Feed {
public:
    explicit Feed(std::size_t capacity = 4096) : capacity_(capacity > 0 ? capacity : 1) {}

    // Adds an event; returns its seq.
    std::uint64_t Push(Event e) {
        e.seq = ++last_;
        events_.push_back(std::move(e));
        while (events_.size() > capacity_) events_.pop_front();
        return last_;
    }

    // Every event with seq > `seq`, in order (those still held).
    std::vector<Event> Since(std::uint64_t seq) const {
        std::vector<Event> out;
        if (events_.empty() || seq >= last_) return out;
        const std::uint64_t first = events_.front().seq;  // the held events' seqs are consecutive
        const std::size_t from = seq < first ? 0 : static_cast<std::size_t>(seq - first + 1);
        out.assign(events_.begin() + static_cast<std::ptrdiff_t>(from), events_.end());
        return out;
    }

    std::uint64_t Last() const { return last_; }  // the newest seq (0: none)
    void Clear() { events_.clear(); }
    std::size_t Size() const { return events_.size(); }
    std::size_t Capacity() const { return capacity_; }

private:
    std::size_t capacity_;
    std::deque<Event> events_;
    std::uint64_t last_ = 0;
};

}  // namespace fx
}  // namespace simcore
