// simcore/missile.hpp — generic point-mass missile with proportional navigation.
//
// References (see docs/SOURCES.md):
//   * Zarchan, "Tactical and Strategic Missile Guidance" (proportional navigation, miss distance).
//   * Fleeman, "Missile Design and System Engineering" (point-mass flyout, drag, thrust).
//
// This is a generic placeholder flyout model. Historical Soviet SAMs mostly
// used radio command guidance (e.g. three-point / half-lead methods computed at
// the fire-control station). That will be added as a separate guidance law driven
// by the radar tracker; PN is here as the textbook baseline and for testing.
#pragma once

#include <cmath>

#include "simcore/atmosphere.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct MissileParams {
    double launch_mass_kg = 0.0;
    double burnout_mass_kg = 0.0;
    double boost_thrust_n = 0.0;
    double boost_time_s = 0.0;
    double sustain_thrust_n = 0.0;
    double sustain_time_s = 0.0;
    double reference_area_m2 = 0.0;
    double drag_coefficient = 0.0;        // zero-lift Cd (Mach dependence: TODO)
    double max_lateral_accel_mps2 = 0.0;  // structural / aerodynamic g-limit
    double navigation_constant = 4.0;
    double launch_speed_mps = 30.0;       // speed on leaving the rail
    double guidance_start_s = 0.5;        // unguided period after launch
    double fuze_arming_time_s = 1.0;
    double proximity_fuze_radius_m = 0.0;
    double lethal_radius_m = 0.0;         // deterministic kill radius (Pk model: TODO)
    double max_flight_time_s = 0.0;
    double min_guided_speed_mps = 0.0;    // self-destruct below this speed after burnout
};

enum class MissileOutcome { InFlight, Detonated, Missed, SelfDestructed, HitGround };

struct Missile {
    int id = 0;
    int target_id = 0;
    MissileParams params;
    Vec3 position;
    Vec3 velocity;
    double time_since_launch_s = 0.0;
    double closest_approach_m = 1e30;
    MissileOutcome outcome = MissileOutcome::InFlight;
    bool Active() const { return outcome == MissileOutcome::InFlight; }
};

inline double MissileBurnTime(const MissileParams& p) { return p.boost_time_s + p.sustain_time_s; }

inline double MissileMass(const MissileParams& p, double t) {
    const double burn = MissileBurnTime(p);
    if (burn <= 0.0 || t >= burn) return p.burnout_mass_kg;
    return p.launch_mass_kg - (p.launch_mass_kg - p.burnout_mass_kg) * (t / burn);
}

inline double MissileThrust(const MissileParams& p, double t) {
    if (t < p.boost_time_s) return p.boost_thrust_n;
    if (t < p.boost_time_s + p.sustain_time_s) return p.sustain_thrust_n;
    return 0.0;
}

// True proportional navigation: a = N * Vc * (Omega x r_hat), with LOS rate Omega = (r x v_rel) / |r|^2.
inline Vec3 ProportionalNavigationAccel(const Vec3& missile_pos, const Vec3& missile_vel,
                                        const Vec3& target_pos, const Vec3& target_vel, double nav_constant) {
    const Vec3 r = target_pos - missile_pos;
    const Vec3 v_rel = target_vel - missile_vel;
    const double r2 = r.LengthSquared();
    if (r2 < 1e-6) return {};
    const double range = std::sqrt(r2);
    const Vec3 omega = Cross(r, v_rel) / r2;
    const double closing_speed = -Dot(r, v_rel) / range;
    return Cross(omega, r / range) * (nav_constant * closing_speed);
}

// Remove the component of `a` along unit vector `axis`.
inline Vec3 PerpendicularPart(const Vec3& a, const Vec3& unit_axis) { return a - unit_axis * Dot(a, unit_axis); }

struct StepClosestApproach {
    double distance_m;  // minimum separation during the step
    double fraction;    // where in the step it occurred, [0, 1]; 1 = still closing at step end
};

// Minimum distance between two points moving linearly over one step.
// rel0/rel1 = target - missile at the start/end of the step.
inline StepClosestApproach ClosestApproachOverStep(const Vec3& rel0, const Vec3& rel1) {
    const Vec3 d = rel1 - rel0;
    const double dd = d.LengthSquared();
    double s = dd > 1e-12 ? -Dot(rel0, d) / dd : 0.0;
    s = Clamp(s, 0.0, 1.0);
    return {(rel0 + d * s).Length(), s};
}

inline double MinDistanceOverStep(const Vec3& rel0, const Vec3& rel1) {
    return ClosestApproachOverStep(rel0, rel1).distance_m;
}

// Advance one missile by dt against a target moving from target_pos0 to target_pos1.
// `missile_altitude_m` is the missile's current altitude (for the atmosphere model).
// Ground impact is checked by the caller (World), which knows the terrain.
inline void StepMissile(Missile& m, const Vec3& target_pos0, const Vec3& target_pos1, const Vec3& target_vel,
                        double missile_altitude_m, double dt) {
    if (!m.Active()) return;
    const MissileParams& p = m.params;
    const double t = m.time_since_launch_s;

    const double speed = m.velocity.Length();
    const Vec3 v_hat = speed > 1e-6 ? m.velocity / speed : Vec3{0.0, 0.0, 1.0};
    const double mass = MissileMass(p, t);
    const AtmosphereState atm = StandardAtmosphere(missile_altitude_m);
    const double drag = 0.5 * atm.density_kg_m3 * speed * speed * p.drag_coefficient * p.reference_area_m2;

    Vec3 accel = v_hat * ((MissileThrust(p, t) - drag) / mass);
    accel += Vec3{0.0, 0.0, -kStandardGravity};

    if (t >= p.guidance_start_s) {
        Vec3 cmd = ProportionalNavigationAccel(m.position, m.velocity, target_pos0, target_vel, p.navigation_constant);
        cmd += Vec3{0.0, 0.0, kStandardGravity};  // gravity bias
        cmd = PerpendicularPart(cmd, v_hat);       // airframe can only pull lateral g
        const double mag = cmd.Length();
        if (mag > p.max_lateral_accel_mps2 && mag > 0.0) cmd *= p.max_lateral_accel_mps2 / mag;
        accel += cmd;
    }

    const Vec3 pos0 = m.position;
    m.velocity += accel * dt;  // semi-implicit Euler
    m.position += m.velocity * dt;
    m.time_since_launch_s += dt;

    const Vec3 rel0 = target_pos0 - pos0;
    const Vec3 rel1 = target_pos1 - m.position;
    const StepClosestApproach ca = ClosestApproachOverStep(rel0, rel1);
    if (ca.distance_m < m.closest_approach_m) m.closest_approach_m = ca.distance_m;

    // Idealised proximity fuze: inside the fuze radius, detonate at the point of closest
    // approach (i.e. once the separation stops shrinking). Real fuze delay/geometry: TODO.
    const bool armed = m.time_since_launch_s >= p.fuze_arming_time_s;
    if (armed && ca.distance_m <= p.proximity_fuze_radius_m && ca.fraction < 1.0) {
        m.outcome = MissileOutcome::Detonated;
    } else if (armed && rel1.Length() > rel0.Length() && m.closest_approach_m < 1000.0) {
        m.outcome = MissileOutcome::Missed;  // opening after a close pass
    } else if (m.time_since_launch_s >= p.max_flight_time_s) {
        m.outcome = MissileOutcome::SelfDestructed;
    } else if (m.time_since_launch_s > MissileBurnTime(p) && m.velocity.Length() < p.min_guided_speed_mps) {
        m.outcome = MissileOutcome::SelfDestructed;
    }
}

}  // namespace simcore
