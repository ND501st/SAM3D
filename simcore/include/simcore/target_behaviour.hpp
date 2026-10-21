// simcore/target_behaviour.hpp — how a scenario target flies beyond following its waypoints (roadmap 9.3).
//
// The Aircraft (aircraft.hpp) steers toward its next waypoint at constant speed. A TargetPilot sits on top of it:
//   * Terrain following: the pilot holds a height above the ground (the behaviour's agl_m, or each waypoint's own),
//     climbing early for the highest ground `lookahead_s` ahead, within a vertical rate. The aircraft then steers in
//     the horizontal only. A low strike run is such a route: low ingress, a pull-up point, the attack, a low egress.
//   * Pop-up (helicopters): low to its first waypoint (the hide), hovering there behind the cover at hide_agl_m, rising
//     to popup_agl_m for expose_s, sinking back for hidden_s, `cycles` times, then on along the rest of the route low.
//   * Evasion, on a missile launch at it or when the tracking radar locks it (a radar warning receiver hears the lock;
//     a TV track gives no warning): break onto a beam heading, break and dive to dive_agl_m, or jink (hard turns left
//     and right of the course) for evade_s and then back onto the route. A hiding helicopter drops behind its cover.
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
    enum class On { Launch, Lock };
    enum class Kind { Break, Dive, Jink };
    bool present = false;
    On on = On::Launch;
    Kind kind = Kind::Break;
    double turn_rate_rad_s = DegToRad(12.0);
    double dive_agl_m = 30.0;
    double jink_period_s = 4.0;
    double evade_s = 20.0;  // a jink lasts this long, then the route is resumed
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
    bool Evading() const { return evading_; }

    // When the aircraft enters the world: a hovering helicopter needs a tight capture circle at its hide.
    void OnSpawn(Aircraft& a) {
        if (b_.kind == TargetBehaviourDef::Kind::PopUp) a.waypoint_capture_radius_m = 100.0;
    }

    // Before the world moves it: with the height in the pilot's hands the aircraft steers and moves in the horizontal
    // only (AfterStep then sets the height and the vertical speed).
    void BeforeStep(Aircraft& a) const {
        if (!ControlsHeight()) return;
        for (Vec3& w : a.waypoints) w.z = a.position.z;
        a.velocity.z = 0.0;
    }

    // After the world moved it: the pop-up's phases, the jink's turns and the height. Returns what happened, for the
    // debrief ("" if nothing).
    std::string AfterStep(Aircraft& a, const Terrain& terrain, const LocalFrame& frame, double dt, double t) {
        if (!a.alive) return std::string();
        std::string event = StepPopUp(a, t);
        if (evading_ && e_.kind == EvadeDef::Kind::Jink) {
            const std::string j = StepJink(a, t);
            if (!j.empty()) event = j;
        }
        if (ControlsHeight() && dt > 0.0) HoldHeight(a, terrain, frame, dt);
        return event;
    }

    // A missile was launched at it, or the tracking radar holds it (`radar` false: a TV track, which it can't hear).
    std::string OnLaunch(Aircraft& a, const Vec3& unit, double t) {
        return e_.present && e_.on == EvadeDef::On::Launch ? Evade(a, unit, t) : std::string();
    }
    std::string OnLocked(Aircraft& a, const Vec3& unit, double t) {
        return e_.present && e_.on == EvadeDef::On::Lock ? Evade(a, unit, t) : std::string();
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

    std::string Evade(Aircraft& a, const Vec3& unit, double t) {
        if (evading_ || !a.alive) return std::string();
        evading_ = true;
        if (b_.kind == TargetBehaviourDef::Kind::PopUp && (phase_ == Phase::Hidden || phase_ == Phase::Exposed)) {
            if (phase_ == Phase::Exposed) ++cycles_done_;
            phase_ = Phase::Hidden;
            until_s_ = t + b_.hidden_s;
            return "drops behind cover (warned)";
        }
        saved_turn_rad_s_ = a.max_turn_rate_rad_s;
        a.max_turn_rate_rad_s = e_.turn_rate_rad_s;
        Vec3 dir = a.velocity;
        dir.z = 0.0;
        dir = dir.LengthSquared() > 0.0 ? dir.Normalized() : Vec3{0.0, 1.0, 0.0};
        if (e_.kind == EvadeDef::Kind::Jink) {
            saved_waypoints_ = a.waypoints;
            saved_next_ = a.next_waypoint;
            base_dir_ = dir;
            jink_sign_ = 1.0;
            jink_flip_s_ = t + e_.jink_period_s;
            jink_end_s_ = t + e_.evade_s;
            SetJinkPoint(a);
            return "jinks (hard turns left and right)";
        }
        // Break onto a beam heading, away from the missile's line of sight.
        const Vec3 los = (a.position - unit).Normalized();
        Vec3 beam{-los.y, los.x, 0.0};
        if (Dot(beam, a.velocity) < 0.0) beam = -beam;
        a.waypoints = {a.position + beam.Normalized() * 30000.0};
        a.next_waypoint = 0;
        if (e_.kind == EvadeDef::Kind::Dive) {
            diving_ = true;
            return "breaks onto a beam heading and dives for the ground";
        }
        return "breaks away onto a beam heading";
    }

    std::string StepJink(Aircraft& a, double t) {
        if (t >= jink_end_s_) {
            a.waypoints = saved_waypoints_;
            a.next_waypoint = saved_next_;
            a.max_turn_rate_rad_s = saved_turn_rad_s_;
            evading_ = false;
            e_.present = false;  // once
            return "back on its route";
        }
        if (t >= jink_flip_s_) {
            jink_sign_ = -jink_sign_;
            jink_flip_s_ = t + e_.jink_period_s;
            SetJinkPoint(a);
        }
        return std::string();
    }

    void SetJinkPoint(Aircraft& a) const {
        const double ang = jink_sign_ * DegToRad(60.0);
        const Vec3 d{base_dir_.x * std::cos(ang) + base_dir_.y * std::sin(ang),
                     -base_dir_.x * std::sin(ang) + base_dir_.y * std::cos(ang), 0.0};
        a.waypoints = {a.position + d * 20000.0};
        a.next_waypoint = 0;
    }

    // Climb or sink toward the wanted height over the highest ground ahead, within the vertical rate; never into the ground.
    void HoldHeight(Aircraft& a, const Terrain& terrain, const LocalFrame& frame, double dt) {
        const double here = terrain.HeightAt(a.position.x, a.position.y);
        double ground = here;
        const double vh = std::hypot(a.velocity.x, a.velocity.y);
        if (vh > 1.0 && b_.lookahead_s > 0.0) {
            const double reach = vh * b_.lookahead_s;
            const int samples = 2 + static_cast<int>(reach / 100.0);
            for (int i = 1; i <= samples; ++i) {
                const double f = reach * i / samples / vh;
                const double h = terrain.HeightAt(a.position.x + a.velocity.x * f, a.position.y + a.velocity.y * f);
                if (h > ground) ground = h;
            }
        }
        const double alt = frame.AltitudeOf(a.position);
        const double want = ground + WantedAglM(a);
        const double rate = diving_ && b_.max_climb_mps < 50.0 ? 50.0 : b_.max_climb_mps;  // a dive is steeper
        // The vertical speed changes no faster than a pull-up allows, and eases off in time to level at the height
        // wanted (a sudden climb would read as a hard manoeuvre in the outcome).
        const double gap = want - alt;
        double vz_want = std::min({rate, std::sqrt(2.0 * kVerticalAccelMps2 * std::fabs(gap)), std::fabs(gap) / kLevelOffS});
        if (gap < 0.0) vz_want = -vz_want;
        const double dv = kVerticalAccelMps2 * dt;
        vz_mps_ += std::max(-dv, std::min(dv, vz_want - vz_mps_));
        double dz = vz_mps_ * dt;
        if (alt + dz < here + 2.0) {  // no flying into the ground
            dz = here + 2.0 - alt;
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
    double saved_turn_rad_s_ = 0.0;
    std::vector<Vec3> saved_waypoints_;
    std::size_t saved_next_ = 0;
    Vec3 base_dir_;
    double jink_sign_ = 1.0;
    double jink_flip_s_ = 0.0;
    double jink_end_s_ = 0.0;
    double vz_mps_ = 0.0;  // the vertical speed the pilot holds (the world moves the aircraft in the horizontal)
};

}  // namespace simcore
