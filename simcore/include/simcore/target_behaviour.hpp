// simcore/target_behaviour.hpp — how a scenario target flies beyond following its waypoints (roadmap 9.3).
//
// The Aircraft (aircraft.hpp) steers toward its next waypoint (bank-to-turn with its type's limits, roadmap R3). A
// TargetPilot sits on top of it:
//   * Terrain following: the pilot holds a height above the ground (the behaviour's agl_m, or each waypoint's own),
//     climbing early for the highest ground `lookahead_s` ahead, within a vertical rate. The aircraft then steers in
//     the horizontal only. A low strike run is such a route: low ingress, a pull-up point, the attack, a low egress.
//   * Pop-up (helicopters): low to its first waypoint (the hide), hovering there behind the cover at hide_agl_m, rising
//     to popup_agl_m for expose_s, sinking back for hidden_s, `cycles` times, then on along the rest of the route low.
//   * Evasion, on a missile launch at it or when the tracking radar locks it (a radar warning receiver hears the lock;
//     a TV track gives no warning): break onto a beam heading, break and dive to dive_agl_m, or jink (hard turns left
//     and right of the course) for evade_s, and then back to its flight's slot or its route. A hiding helicopter drops
//     behind its cover. The pilot does not steer the aircraft himself: the evasion is a PilotOverride the command
//     layer flies and ends (flights.hpp, roadmap B1); the pilot keeps the height layer (the dive's height until it ends).
// Terrain masking is not a behaviour of its own: a target flying low behind a ridge is out of every sensor's line of
// sight (the gameplay model's and the EM layer's).
//
// All numbers come from the scenario (PLACEHOLDER flight profiles, not data of a real aircraft).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "simcore/aircraft.hpp"
#include "simcore/flights.hpp"
#include "simcore/geo.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct TargetBehaviourDef {
    enum class Kind { None, TerrainFollowing, PopUp };
    Kind kind = Kind::None;
    // Terrain following (and the pop-up's transit and egress, at hide_agl_m).
    double agl_m = 50.0;
    double max_climb_mps = 30.0;  // vertical rate limit, up and down
    double lookahead_s = 6.0;     // clear the highest ground this far ahead
    std::vector<double> waypoint_agl_m;  // per waypoint; < 0: agl_m
    // Pop-up.
    double hide_agl_m = 5.0;
    double popup_agl_m = 25.0;
    double first_hold_s = 30.0;  // hover hidden this long after arriving at the hide
    double expose_s = 30.0;
    double hidden_s = 60.0;
    int cycles = 1;
};

struct EvadeDef {
    enum class On { Launch, Lock, Order };  // Order: only when the instructor orders it (instructor.hpp)
    enum class Kind { Break, Dive, Jink };
    bool present = false;
    On on = On::Launch;
    Kind kind = Kind::Break;
    double turn_rate_rad_s = DegToRad(12.0);
    double dive_agl_m = 30.0;
    double jink_period_s = 4.0;
    double evade_s = 20.0;  // a break, dive or jink lasts this long, then the flight's slot or the route is resumed
};

// What a pilot's evasion asks of the aircraft: said for the debrief, and the manoeuvre the command layer flies
// (`steer` false: none, a helicopter dropping behind its cover).
struct Evasion {
    std::string what;
    bool steer = false;
    PilotOverride ov;
};

inline const char* ToString(EvadeDef::Kind k) {
    switch (k) {
        case EvadeDef::Kind::Break: return "break";
        case EvadeDef::Kind::Dive: return "dive";
        case EvadeDef::Kind::Jink: return "jink";
    }
    return "?";
}

class TargetPilot {
public:
    TargetPilot() = default;
    TargetPilot(TargetBehaviourDef behaviour, EvadeDef evade) : b_(std::move(behaviour)), e_(evade) {}

    bool ControlsHeight() const { return b_.kind != TargetBehaviourDef::Kind::None || diving_; }
    // A weapon flies into the ground on purpose: no ground floor (GroundFloor) for it.
    void SetWeapon(bool weapon) { weapon_ = weapon; }
    bool Evading() const { return evading_; }
    // The height above the ground it holds from now on (the instructor's order: a terrain-following pilot).
    void SetAgl(double agl_m) {
        b_.agl_m = agl_m;
        b_.waypoint_agl_m.clear();
    }
    double Agl() const { return b_.agl_m; }

    // When the aircraft enters the world: a hovering helicopter needs a tight capture circle at its hide.
    void OnSpawn(Aircraft& a) {
        if (b_.kind == TargetBehaviourDef::Kind::PopUp) a.waypoint_capture_radius_m = 100.0;
    }

    // Before the world moves it: with the height in the pilot's hands the aircraft steers and moves in the horizontal
    // only (AfterStep then sets the height and the vertical speed).
    void BeforeStep(Aircraft& a) const {
        a.height_held = ControlsHeight();  // a bank-to-turn aircraft then flies the horizontal only (aircraft.hpp)
        if (!a.height_held) return;
        for (Vec3& w : a.waypoints) w.z = a.position.z;
        if (a.demand.active) a.demand.point.z = a.position.z;
        a.velocity.z = 0.0;
    }

    // After the world moved it: the pop-up's phases and the height. Returns what happened, for the debrief ("" if
    // nothing).
    std::string AfterStep(Aircraft& a, const Terrain& terrain, const LocalFrame& frame, double dt, double t) {
        if (!a.alive) return std::string();
        std::string event = StepPopUp(a, t);
        if (ControlsHeight() && dt > 0.0) HoldHeight(a, terrain, frame, dt);
        if (dt > 0.0) {
            const std::string g = GroundFloor(a, terrain, frame, dt, ControlsHeight());
            if (!g.empty() && event.empty()) event = g;
        }
        SyncPathAngle(a);  // the attitude follows the height it flew (roadmap R3)
        return event;
    }

    // A missile was launched at it, or the tracking radar holds it (`radar` false: a TV track, which it can't hear).
    Evasion OnLaunch(const Aircraft& a, const Vec3& unit, double t) {
        return e_.present && e_.on == EvadeDef::On::Launch ? Evade(a, unit, t) : Evasion();
    }
    Evasion OnLocked(const Aircraft& a, const Vec3& unit, double t) {
        return e_.present && e_.on == EvadeDef::On::Lock ? Evade(a, unit, t) : Evasion();
    }
    // The instructor's order to manoeuvre now (and again later, if he likes), whatever would set it off otherwise.
    Evasion EvadeNow(const Aircraft& a, const Vec3& unit, double t) {
        if (evading_) return Evasion();
        e_.present = true;
        return Evade(a, unit, t);
    }
    // The command layer ended his manoeuvre: he is no longer evading, holds his own height again, and evades only once
    // (as before roadmap B1; the instructor may order it again).
    void OverrideEnded() {
        if (!evading_) return;
        evading_ = false;
        diving_ = false;
        e_.present = false;
    }
    // Only the ground floor (roadmap R5): an aircraft past its last waypoint, flying on until it leaves the world.
    void HoldOverGround(Aircraft& a, const Terrain& terrain, const LocalFrame& frame, double dt) {
        if (a.alive && dt > 0.0) {
            GroundFloor(a, terrain, frame, dt, false);
            SyncPathAngle(a);
        }
    }
    // The height above the ground the pilot is holding now.
    double WantedAglM(const Aircraft& a) const {
        if (diving_) return e_.dive_agl_m;
        if (b_.kind == TargetBehaviourDef::Kind::PopUp) return phase_ == Phase::Exposed ? b_.popup_agl_m : b_.hide_agl_m;
        if (b_.waypoint_agl_m.empty()) return b_.agl_m;
        // The height of the waypoint it is flying to (past the last one: the last one's).
        const std::size_t i = a.next_waypoint < b_.waypoint_agl_m.size() ? a.next_waypoint : b_.waypoint_agl_m.size() - 1;
        return b_.waypoint_agl_m[i] >= 0.0 ? b_.waypoint_agl_m[i] : b_.agl_m;
    }

    enum class Phase { Transit, Hidden, Exposed, Egress };
    Phase PopUpPhase() const { return phase_; }

private:
    std::string StepPopUp(Aircraft& a, double t) {
        if (b_.kind != TargetBehaviourDef::Kind::PopUp) return std::string();
        switch (phase_) {
            case Phase::Transit:
                if (a.next_waypoint < 1) return std::string();
                cruise_mps_ = a.speed_mps;
                a.speed_mps = 0.0;
                phase_ = Phase::Hidden;
                until_s_ = t + b_.first_hold_s;
                return "hovers behind cover";
            case Phase::Hidden:
                if (t < until_s_) return std::string();
                if (cycles_done_ >= b_.cycles) {
                    a.speed_mps = cruise_mps_;
                    phase_ = Phase::Egress;
                    return "flies on, low";
                }
                phase_ = Phase::Exposed;
                until_s_ = t + b_.expose_s;
                return "pops up";
            case Phase::Exposed:
                if (t < until_s_) return std::string();
                ++cycles_done_;
                phase_ = Phase::Hidden;
                until_s_ = t + b_.hidden_s;
                return "drops back behind cover";
            case Phase::Egress:
                return std::string();
        }
        return std::string();
    }

    // The manoeuvre, as an override for the command layer to fly (flights.hpp): a jink's turns 60 degrees off its course,
    // 20 km out, flipped every jink_period_s; a break or a dive for a point 30 km along the beam to the line of sight from
    // the unit; each at the evasion's turn rate, for evade_s.
    Evasion Evade(const Aircraft& a, const Vec3& unit, double t) {
        Evasion out;
        if (evading_ || !a.alive) return out;
        evading_ = true;
        if (b_.kind == TargetBehaviourDef::Kind::PopUp && (phase_ == Phase::Hidden || phase_ == Phase::Exposed)) {
            if (phase_ == Phase::Exposed) ++cycles_done_;
            phase_ = Phase::Hidden;
            until_s_ = t + b_.hidden_s;
            out.what = "drops behind cover (warned)";
            return out;
        }
        Vec3 dir = a.velocity;
        dir.z = 0.0;
        dir = dir.LengthSquared() > 0.0 ? dir.Normalized() : Vec3{0.0, 1.0, 0.0};
        out.steer = true;
        PilotOverride& o = out.ov;
        o.turn_rate_rad_s = e_.turn_rate_rad_s;
        o.until_s = t + e_.evade_s;
        if (e_.kind == EvadeDef::Kind::Jink) {
            o.kind = PilotOverride::Kind::Jink;
            o.base_dir = dir;
            o.sign = 1.0;
            o.period_s = e_.jink_period_s;
            o.flip_s = t + e_.jink_period_s;
            o.point = Flights::JinkPoint(o, a.position);
            out.what = "jinks (hard turns left and right)";
            return out;
        }
        // Break onto a beam heading, away from the missile's line of sight.
        const Vec3 los = (a.position - unit).Normalized();
        Vec3 beam{-los.y, los.x, 0.0};
        if (Dot(beam, a.velocity) < 0.0) beam = -beam;
        o.point = a.position + beam.Normalized() * 30000.0;
        if (e_.kind == EvadeDef::Kind::Dive) {
            o.kind = PilotOverride::Kind::Dive;
            diving_ = true;
            out.what = "breaks onto a beam heading and dives for the ground";
            return out;
        }
        o.kind = PilotOverride::Kind::Break;
        out.what = "breaks away onto a beam heading";
        return out;
    }

    // The floor every pilot keeps, whatever he is doing (roadmap R5): his belly (Aircraft::bottom_m below his centre) a
    // metre over the highest ground of the next kFloorLookS; under it he pulls up at his climb limit, and never lets
    // his belly under the ground here. A weapon has none (it ends on the ground). Returns "pulled up over the ground"
    // when a pull-up starts.
    // A pilot who held his own height this step (`held`: HoldHeight ran, for terrain following, a pop-up, a dive) climbed
    // for the ground ahead within his limit and keeps his belly over the ground here: for him the floor is only a
    // limit, never a second climb on top of his own in the same step, and no event (roadmap R5's review: TF over a
    // ridge climbed at twice its rate).
    std::string GroundFloor(Aircraft& a, const Terrain& terrain, const LocalFrame& frame, double dt, bool held) {
        if (weapon_ || !a.alive || dt <= 0.0) return std::string();
        const double here = terrain.HeightAt(a.position.x, a.position.y);
        if (held) {
            const double under = here + a.bottom_m - frame.AltitudeOf(a.position);
            if (under > 0.0) a.position.z += under;  // never the belly under the ground here (HoldHeight keeps it over)
            pulled_up_ = false;
            return std::string();
        }
        double ground = here;
        const double vh = std::hypot(a.velocity.x, a.velocity.y);
        if (vh > 1.0) {
            const double reach = vh * kFloorLookS;
            const int samples = std::min(60, 2 + static_cast<int>(reach / 100.0));
            for (int i = 1; i <= samples; ++i) {
                const double f = reach * i / samples / vh;
                ground = std::max(ground, terrain.HeightAt(a.position.x + a.velocity.x * f, a.position.y + a.velocity.y * f));
            }
        }
        const double alt = frame.AltitudeOf(a.position);
        const double floor_m = ground + a.bottom_m + kFloorClearM;
        if (alt >= floor_m) {
            pulled_up_ = false;
            return std::string();
        }
        double dz = std::min(floor_m - alt, std::max(1.0, b_.max_climb_mps) * dt);
        dz = std::max(dz, here + a.bottom_m - alt);  // never the belly under the ground here
        a.position.z += dz;
        a.velocity.z = std::max(a.velocity.z, dz / dt);
        vz_mps_ = std::max(vz_mps_, dz / dt);
        const bool started = !pulled_up_;
        pulled_up_ = true;
        return started ? "pulled up over the ground" : std::string();
    }
    static constexpr double kFloorLookS = 3.0;   // game values
    static constexpr double kFloorClearM = 1.0;

    // Climb or sink toward the wanted height over the highest ground ahead, within the vertical rate; never into the ground.
    // The height wanted is over the woods and buildings standing there (Terrain::ObstacleHeightAt, roadmap R6/E9): a low
    // pilot clears the canopy he sees, he does not fly through it.
    void HoldHeight(Aircraft& a, const Terrain& terrain, const LocalFrame& frame, double dt) {
        const double here = terrain.HeightAt(a.position.x, a.position.y);
        double ground = here + terrain.ObstacleHeightAt(a.position.x, a.position.y);
        const double vh = std::hypot(a.velocity.x, a.velocity.y);
        if (vh > 1.0 && b_.lookahead_s > 0.0) {
            const double reach = vh * b_.lookahead_s;
            const int samples = 2 + static_cast<int>(reach / 100.0);
            for (int i = 1; i <= samples; ++i) {
                const double f = reach * i / samples / vh;
                const double x = a.position.x + a.velocity.x * f, y = a.position.y + a.velocity.y * f;
                const double h = terrain.HeightAt(x, y) + terrain.ObstacleHeightAt(x, y);
                if (h > ground) ground = h;
            }
            // The buildings between those points, at the terrain's own spacing where one may top what he holds over
            // already (ObstaclesAlong): a narrow block of flats ahead is not stepped over.
            const double ux = a.velocity.x / vh, uy = a.velocity.y / vh;
            for (int i = 1; i <= samples; ++i) {
                ObstaclesAlong(terrain, a.position.x, a.position.y, ux, uy, reach, reach * (i - 1) / samples, reach * i / samples, ground,
                               0.0, [&](double, double top) {
                                   if (top > ground) ground = top;
                                   return true;
                               });
            }
        }
        const double alt = frame.AltitudeOf(a.position);
        const double want = ground + WantedAglM(a);
        const double rate = diving_ && b_.max_climb_mps < 50.0 ? 50.0 : b_.max_climb_mps;  // a dive is steeper
        // The vertical speed changes no faster than a pull-up allows, and eases off in time to level at the height
        // wanted (a sudden climb would read as a hard manoeuvre in the outcome).
        const double gap = want - alt;
        // Up no faster than its type climbs (a bank-to-turn type's max_climb_mps, aircraft.hpp; 0: not given).
        const double up = gap > 0.0 && a.max_climb_mps > 0.0 ? std::min(rate, a.max_climb_mps) : rate;
        double vz_want = std::min({up, std::sqrt(2.0 * kVerticalAccelMps2 * std::fabs(gap)), std::fabs(gap) / kLevelOffS});
        if (gap < 0.0) vz_want = -vz_want;
        const double dv = kVerticalAccelMps2 * dt;
        vz_mps_ += std::max(-dv, std::min(dv, vz_want - vz_mps_));
        double dz = vz_mps_ * dt;
        if (alt + dz < here + a.bottom_m + kFloorClearM) {  // no flying into the ground: the belly a metre over it
            dz = here + a.bottom_m + kFloorClearM - alt;
            vz_mps_ = std::min(dz / dt, rate);
        }
        a.position.z += dz;
        a.velocity.z = dz / dt;
    }
    static constexpr double kVerticalAccelMps2 = 15.0;  // about 1.5 g beyond level flight: a game value
    static constexpr double kLevelOffS = 1.0;

    TargetBehaviourDef b_;
    EvadeDef e_;
    Phase phase_ = Phase::Transit;
    double until_s_ = 0.0;
    int cycles_done_ = 0;
    double cruise_mps_ = 0.0;
    bool evading_ = false;
    bool diving_ = false;
    double vz_mps_ = 0.0;  // the vertical speed the pilot holds (the world moves the aircraft in the horizontal)
    bool weapon_ = false;     // a weapon's "pilot": no ground floor
    bool pulled_up_ = false;  // under the ground floor last step
};

}  // namespace simcore
