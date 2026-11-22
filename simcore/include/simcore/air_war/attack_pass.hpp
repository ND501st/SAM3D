// simcore/air_war/attack_pass.hpp — an attack pass flown (roadmap B6, docs/air_battle/06_OPPORTUNITY.md §5.2).
//
// A gun run, a rocket salvo or a re-attack is a manoeuvre on the flights' override layer (flights.hpp's
// PilotOverride::Kind::Attack), not a release at a waypoint:
//   setup    from too close or too far off its heading he first flies out to a point opportunity.run_in_m from the aim
//            on the side he came from (a re-attack: opportunity.reattack_separation_m out, opportunity.reattack_heading_deg
//            off his first heading);
//   run      he flies at the aim; he opens fire when the aim is within the weapon's pass.open_fire_m slant range and
//            within kBoresightDeg of his nose (the A-10's test: a mean 1,115 m slant in a 3.3 degree dive,
//            cdat_array20_1980; rockets from a 10-25 degree dive, mt_1970_bukhantsev);
//   recover  past the aim and off to the side, opportunity.recover_m out, then back to his flight or his route.
// The dive is flown (roadmap S4): through the setup and the run the override carries the weapon's dive and open-fire range,
// and the pilot's height layer (TargetPilot::SetPass) climbs to the roll-in height and dives along the line to the aim;
// at the recovery he pulls out to his own height again. What one pass does to what is really at the aim is GunPassEffect: the
// weapon's per_pass for the target's class (what a pass does to a found, stationary, undefended aim point), times its
// moving_factor on a moving target and opportunity.gun_burst_under_fire when the pass is flown under fire.
#pragma once

#include <algorithm>
#include <cmath>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/flights.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// What one pass does to the thing at its aim: the chance it is put out of action (immobilised, a launcher or radar
// wrecked), and of those the share burnt out.
struct PassEffect {
    double kill = 0.0;
    double catastrophic = 0.0;
};
inline PassEffect GunPassEffect(const GunPassDef& def, TargetClass cls, bool moving, bool under_fire, bool night_visual,
                                const TuningTable& tune) {
    PassEffect e;
    const int c = static_cast<int>(cls);
    if (!def.present || c < 0 || c >= GunPassDef::kClasses) return e;
    double k = def.per_pass[c];
    if (moving) k *= def.moving_factor;
    if (under_fire) k *= tune.Get("opportunity.gun_burst_under_fire");
    if (night_visual) k /= tune.Get("delivery.night_visual_factor");  // a pass by eye at night (the CEP's own factor)
    e.kill = Clamp01(k);
    e.catastrophic = Clamp01(def.catastrophic_share);
    return e;
}

class AttackPass {
public:
    enum class Phase { Setup, Run, Recover, Done };
    static constexpr double kBoresightDeg = 15.0;  // sam3d: the aim this near his nose to open fire
    static constexpr double kTurnDegS = 12.0;      // sam3d: his hard turn in the pass, like a break (target_behaviour.hpp)
    static constexpr double kRunCaptureM = 300.0;  // sam3d: the run ends this near the aim if he never opened fire

    // Begin a pass from `pos` flying `vel` at `aim` (on the ground). `run_in_m`: the setup's distance out (a re-attack's
    // separation), `heading_off_rad`: a re-attack comes in this far off the line he flew (0: the line he is on);
    // `delay_s`: a wingman behind his leader in trail.
    void Begin(const Vec3& pos, const Vec3& vel, const Vec3& aim, const GunPassDef& def, double run_in_m, double recover_m,
               double heading_off_rad, double t, double delay_s = 0.0) {
        aim_ = aim;
        def_ = def;
        recover_m_ = recover_m;
        fired_ = false;
        start_s_ = t + std::max(0.0, delay_s);
        Vec3 from = Vec3{pos.x - aim.x, pos.y - aim.y, 0.0};
        const double d = from.HorizontalLength();
        from = d > 1.0 ? from * (1.0 / d) : Vec3{-vel.x, -vel.y, 0.0}.Normalized();
        if (heading_off_rad != 0.0) {
            const double c = std::cos(heading_off_rad), s = std::sin(heading_off_rad);
            from = Vec3{from.x * c - from.y * s, from.x * s + from.y * c, 0.0};
        }
        const Vec3 fwd = Vec3{vel.x, vel.y, 0.0};
        const double off = fwd.HorizontalLength() > 1.0 ? AngleBetween(fwd, Vec3{-from.x, -from.y, 0.0}) : 0.0;
        // Room and heading for a run from here: else out to the setup point first.
        const bool room = d >= 0.8 * std::max(run_in_m, def.open_fire_m * 1.5) && off < DegToRad(60.0) && heading_off_rad == 0.0;
        setup_ = aim + from * std::max(run_in_m, def.open_fire_m * 1.5);
        phase_ = room ? Phase::Run : Phase::Setup;
        in_dir_ = Vec3{-from.x, -from.y, 0.0};
    }

    Phase GetPhase() const { return phase_; }
    bool Fired() const { return fired_; }
    bool Done() const { return phase_ == Phase::Done; }
    const Vec3& Aim() const { return aim_; }
    double StartS() const { return start_s_; }

    // The override for the phase he is in (Kind::Attack): the point he flies for and how it ends.
    PilotOverride Override(double t) const {
        PilotOverride o;
        o.kind = PilotOverride::Kind::Attack;
        o.turn_rate_rad_s = DegToRad(kTurnDegS);
        o.after = PilotOverride::After::Rejoin;
        o.end = PilotOverride::End::Point;
        o.until_s = t + 600.0;
        if (phase_ == Phase::Setup || phase_ == Phase::Run) {  // flown in its dive (roadmap S4)
            o.dive_rad = DegToRad(std::max(0.0, def_.dive_deg));
            o.open_fire_m = def_.open_fire_m;
            o.pass_aim = aim_;
        }
        switch (phase_) {
            case Phase::Setup: o.point = setup_; break;
            case Phase::Run: o.point = aim_; break;
            case Phase::Recover:
            case Phase::Done: o.point = recover_; break;
        }
        return o;
    }

    // The open-fire gate: within the weapon's slant range and near his nose.
    bool Gate(const Vec3& pos, const Vec3& vel, const Vec3& aim_ground) const {
        const Vec3 to = aim_ground - pos;
        if (to.Length() > def_.open_fire_m) return false;
        if (vel.Length() < 1.0) return true;
        return AngleBetween(vel, to) <= DegToRad(kBoresightDeg);
    }

    // After each step: the phase he is now in. Returns true at the step he opens fire. `reached`: the override's point
    // was passed (flights.hpp ended it); `aim_ground`: the aim on the ground (its height).
    bool Step(const Vec3& pos, const Vec3& vel, const Vec3& aim_ground, bool reached, double t) {
        if (t < start_s_ || phase_ == Phase::Done) return false;
        switch (phase_) {
            case Phase::Setup:
                if (reached || Dist2d(pos, setup_) < 600.0) phase_ = Phase::Run;
                return false;
            case Phase::Run: {
                if (!fired_ && Gate(pos, vel, aim_ground)) {
                    fired_ = true;
                    BeginRecover(pos, vel);
                    return true;
                }
                if (reached || Dist2d(pos, aim_) < kRunCaptureM) BeginRecover(pos, vel);  // never lined up: off without firing
                return false;
            }
            case Phase::Recover:
                if (reached || Dist2d(pos, recover_) < 600.0) phase_ = Phase::Done;
                return false;
            case Phase::Done: break;
        }
        return false;
    }
    void Abandon() { phase_ = Phase::Done; }

private:
    static double AngleBetween(const Vec3& a, const Vec3& b) {
        const double la = a.Length(), lb = b.Length();
        if (la < 1e-9 || lb < 1e-9) return 0.0;
        return std::acos(Clamp(Dot(a, b) / (la * lb), -1.0, 1.0));
    }
    // Off past the aim and to the side he breaks to (right, a fixed choice).
    void BeginRecover(const Vec3& pos, const Vec3& vel) {
        Vec3 dir = Vec3{vel.x, vel.y, 0.0};
        dir = dir.HorizontalLength() > 1.0 ? dir.Normalized() : in_dir_;
        const Vec3 side{dir.y, -dir.x, 0.0};
        recover_ = Vec3{aim_.x, aim_.y, pos.z} + dir * recover_m_ + side * (0.6 * recover_m_);
        phase_ = Phase::Recover;
    }

    Phase phase_ = Phase::Done;
    GunPassDef def_;
    Vec3 aim_;
    Vec3 setup_;
    Vec3 recover_;
    Vec3 in_dir_;
    double recover_m_ = 2500.0;
    double start_s_ = 0.0;
    bool fired_ = false;
};

}  // namespace airwar
}  // namespace simcore
