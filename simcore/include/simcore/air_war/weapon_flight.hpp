// simcore/air_war/weapon_flight.hpp — how a raid's weapons get from release to impact: how long they take and, for the
// ones that fall, how far they go over the ground (docs/fx/README.md 3.1, docs/air_war/05_EXECUTION.md "release"). Game
// logic: the mission bridge releases a falling load where its throw from the aircraft's state puts it on the aim, and
// lands it this long after (it released every load at a fixed 2 km capture and landed it a fixed `scenario.weapon_fall_s`,
// 15 s, later before), so a laydown at 60 m lands in under 4 s about 0.9 km on and a level release from 4 km in half a
// minute about 7 km on. Where the weapon is unknown the caller keeps `scenario.weapon_fall_s`.
//
//   * a free-fall bomb or dispenser: a point mass from the aircraft's speed and its path angle at release, under gravity
//     and drag (Cd·A/m of a 500 lb bomb, the air's density by its height), integrated numerically;
//   * a retarded bomb with blades (Snakeye): the same with the blades' drag;
//   * a parachute bomb (FAB-500ShN; the runway bombs dropped from low level): free fall until its chute opens at 1.2 s,
//     then down at the chute's 47 m/s, and over the ground slowed by the same chute;
//   * a guided bomb: released where a free fall would land it on the aim and taking about 1.3 times a free fall's time
//     (it glides and steers), and no faster over its range than the aircraft that launched it;
//   * rockets, air-to-ground missiles, anti-tank missiles and guns: their range over their average speed (they fly to
//     their aim: no throw).
// Each time is clamped to 1-60 s. The numbers are estimates (sam3d) except where a source is named.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include "simcore/air_war/data.hpp"
#include "simcore/atmosphere.hpp"
#include "simcore/geo.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

namespace weapon_flight {
// A free-fall bomb: the Mk 82 LDGP's mass and diameter (wiki_weapons: 241 kg, 0.273 m), a subsonic drag coefficient of
// 0.25 for a low-drag bomb (sam3d estimate).
constexpr double kBombMassKg = 241.0;
constexpr double kBombCdA_m2 = 0.25 * 0.25 * 3.141592653589793 * 0.273 * 0.273;
// A Snakeye's four blades open: their drag area (sam3d estimate: it sheds most of its speed in a couple of seconds).
constexpr double kRetardedCdA_m2 = 0.35;
// A parachute bomb (FAB-500ShN, wiki_weapons: airwar.ru): the chute opens 1.2 s after release and lets it down at about
// 47 m/s.
constexpr double kChuteOpenS = 1.2;
constexpr double kChuteDescentMps = 47.0;
// A guided bomb takes about 1.3 times a free fall's time: it glides and steers to its aim (sam3d estimate).
constexpr double kGuidedFactor = 1.3;
// Average speeds over their flight (sam3d estimates from the public top speeds): unguided rockets (S-5, Hydra 70: 600-700
// m/s at burn-out), air-to-ground missiles (the Maverick about Mach 1; the Kh-25ML and Kh-29L faster), wire-guided
// anti-tank missiles (TOW: about 20 s to 3.75 km), cannon rounds (GAU-8 1010 m/s, GSh-30 about 870 m/s at the muzzle).
constexpr double kRocketMps = 600.0;
constexpr double kMaverickMps = 320.0;
constexpr double kSovietAgmMps = 500.0;
constexpr double kMissileMps = 400.0;
constexpr double kAtgmMps = 200.0;
constexpr double kGunMps = 900.0;
constexpr double kMinS = 1.0;
constexpr double kMaxS = 60.0;
constexpr double kStepS = 0.01;  // the free fall's integration step
}  // namespace weapon_flight

// How a weapon gets from the aircraft to the ground.
enum class WeaponPath { Unknown, FreeFall, Retarded, Chute, Runway, Guided, Rocket, Missile, Atgm, Gun };

// Does it fall from the aircraft (so its throw from the release decides where it lands), rather than fly to its aim?
inline bool Falls(WeaponPath p) {
    return p == WeaponPath::FreeFall || p == WeaponPath::Retarded || p == WeaponPath::Chute || p == WeaponPath::Runway ||
           p == WeaponPath::Guided;
}

// The nominal path angle of each delivery at release, degrees up (sam3d estimates: a toss pulls up 20-45°, a pop-up attack
// dives shallowly, a dive-bombing run steeply; level, laydown, stand-off and a hovering helicopter release level). The
// bridge uses the aircraft's own path angle (ReleaseStateOf); this only when it has no speed to have one.
inline double DeliveryPathDeg(const std::string& delivery) {
    if (delivery == "loft") return 30.0;
    if (delivery == "popup") return -20.0;
    if (delivery == "dive") return -40.0;
    if (delivery == "strafe") return -3.3;        // the A-10's mean dive in the 1979 test (cdat_array20_1980)
    if (delivery == "gentle_dive") return -15.0;  // cannon and rockets from 10-25 degrees (mt_1970_bukhantsev)
    return 0.0;
}

// The path of a weapon by its weapons.json kind (bomb, retarded_bomb, cluster, runway, guided_bomb, missile, rockets,
// atgm, gun), or by its id looked up in `weapons` (null: kinds only). A retarded bomb whose id ends in "sh" or "shn"
// (the Soviet штурмовая, low-level, bombs) has a chute; the others blades.
inline WeaponPath WeaponPathOf(const std::string& kind_or_id, const WeaponsDef* weapons = nullptr) {
    std::string kind = kind_or_id;
    std::string id;
    if (weapons != nullptr) {
        if (const WeaponDef* w = weapons->Find(kind_or_id)) {
            kind = w->kind;
            id = w->id;
        }
    }
    const auto ends = [&id](const char* tail) {
        const std::string t(tail);
        return id.size() >= t.size() && id.compare(id.size() - t.size(), t.size(), t) == 0;
    };
    if (kind == "bomb" || kind == "cluster") return WeaponPath::FreeFall;
    if (kind == "retarded_bomb") return ends("sh") || ends("shn") ? WeaponPath::Chute : WeaponPath::Retarded;
    if (kind == "runway") return WeaponPath::Runway;
    if (kind == "guided_bomb") return WeaponPath::Guided;
    if (kind == "rockets") return WeaponPath::Rocket;
    if (kind == "missile") return WeaponPath::Missile;
    if (kind == "atgm") return WeaponPath::Atgm;
    if (kind == "gun") return WeaponPath::Gun;
    return WeaponPath::Unknown;
}

// A body's fall: how long it takes and how far it goes over the ground.
struct Fall {
    double t_s = 0.0;
    double x_m = 0.0;
};

// A body's fall from `agl_m` above the ground, released at `speed_mps` along a path `path_deg` above the horizontal,
// under gravity and drag (`cda_m2` the drag area, `mass_kg`; the air's density by its height above the ground, the
// ground taken at sea level): the seconds until it reaches the ground (at most kMaxS) and the metres it went over the
// ground by then.
inline Fall FallPath(double agl_m, double speed_mps, double path_deg, double cda_m2, double mass_kg) {
    using namespace weapon_flight;
    Fall out;
    double z = std::max(0.0, agl_m);
    if (z <= 0.0) return out;
    const double gamma = DegToRad(path_deg);
    double vx = std::max(0.0, speed_mps) * std::cos(gamma);
    double vz = std::max(0.0, speed_mps) * std::sin(gamma);
    const double k0 = 0.5 * std::max(0.0, cda_m2) / std::max(1.0, mass_kg);  // times the density: the drag per speed²
    double t = 0.0, x = 0.0;
    while (t < kMaxS) {
        const double v = std::sqrt(vx * vx + vz * vz);
        const double k = k0 * StandardAtmosphere(z).density_kg_m3 * v;
        vx += -k * vx * kStepS;
        vz += (-kStandardGravity - k * vz) * kStepS;
        const double z_next = z + vz * kStepS;
        if (z_next <= 0.0) {  // the ground crossed within this step
            const double f = z / (z - z_next);
            out.t_s = t + kStepS * f;
            out.x_m = x + vx * kStepS * f;
            return out;
        }
        z = z_next;
        x += vx * kStepS;
        t += kStepS;
    }
    out.t_s = kMaxS;
    out.x_m = x;
    return out;
}
inline double FallTime(double agl_m, double speed_mps, double path_deg, double cda_m2, double mass_kg) {
    return FallPath(agl_m, speed_mps, path_deg, cda_m2, mass_kg).t_s;
}

// A parachute bomb: free fall (no drag) until the chute opens, then down at the chute's descent rate. Over the ground it
// keeps the aircraft's speed until the chute opens, then the chute slows it: a drag that holds it at its descent rate
// (g = k·47²) acting on its speed over the ground, dv/dt = -k·v², goes ln(1 + k·v·τ)/k in the τ after opening (sam3d).
inline Fall ChuteFall(double agl_m, double speed_mps, double path_deg) {
    using namespace weapon_flight;
    Fall out;
    const double h = std::max(0.0, agl_m);
    const double gamma = DegToRad(path_deg);
    const double vz = std::max(0.0, speed_mps) * std::sin(gamma);  // up
    const double vh = std::max(0.0, speed_mps) * std::cos(gamma);
    const double g = kStandardGravity;
    const double fall_open = -vz * kChuteOpenS + 0.5 * g * kChuteOpenS * kChuteOpenS;
    if (h <= fall_open) {  // down before the chute opens
        out.t_s = (vz + std::sqrt(vz * vz + 2.0 * g * h)) / g;
        out.x_m = vh * out.t_s;
        return out;
    }
    const double tau = (h - fall_open) / kChuteDescentMps;
    const double k = g / (kChuteDescentMps * kChuteDescentMps);
    out.t_s = kChuteOpenS + tau;
    out.x_m = vh * kChuteOpenS + std::log1p(k * vh * tau) / k;
    return out;
}
inline double ChuteFallTime(double agl_m, double speed_mps, double path_deg) { return ChuteFall(agl_m, speed_mps, path_deg).t_s; }

// The farthest a body released so can go over the ground: in a vacuum (drag only shortens it). The bridge opens its
// release window with it before it works out the real throw.
inline double VacuumThrow(double agl_m, double speed_mps, double path_deg) {
    const double h = std::max(0.0, agl_m);
    const double gamma = DegToRad(path_deg);
    const double vz = std::max(0.0, speed_mps) * std::sin(gamma);
    const double vh = std::max(0.0, speed_mps) * std::cos(gamma);
    return vh * (vz + std::sqrt(vz * vz + 2.0 * kStandardGravity * h)) / kStandardGravity;
}

// A weapon's way from release to impact.
struct WeaponFlight {
    double time_s = -1.0;   // release to impact, clamped to 1-60 s (-1: the weapon is not known; the caller times it)
    double throw_m = -1.0;  // over the ground from the release to the impact, for the falling kinds (Falls); -1 for the
                            // ones that fly to their aim and for an unknown weapon
};

// `weapon_kind_or_id` (a weapons.json kind, or its id with `weapons`) released by `delivery` (weapons.json: laydown,
// loft, popup, dive, level, standoff, hover_fire; "" level) from `agl_m` above the ground it falls to, at `speed_mps`
// along a path `path_deg` above the horizontal, `range_m` from the impact point (the slant distance: the rockets',
// missiles' and guns' flight, and a guided bomb's).
inline WeaponFlight WeaponFlightOf(const std::string& delivery, const std::string& weapon_kind_or_id, double agl_m,
                                   double speed_mps, double path_deg, double range_m, const WeaponsDef* weapons = nullptr) {
    using namespace weapon_flight;
    WeaponFlight out;
    if (!std::isfinite(agl_m) || !std::isfinite(speed_mps) || !std::isfinite(path_deg) || !std::isfinite(range_m)) return out;
    const WeaponPath path = WeaponPathOf(weapon_kind_or_id, weapons);
    const double range = std::max(0.0, range_m);
    Fall f;
    double t = -1.0;
    switch (path) {
        case WeaponPath::Unknown: return out;
        case WeaponPath::FreeFall: f = FallPath(agl_m, speed_mps, path_deg, kBombCdA_m2, kBombMassKg); break;
        case WeaponPath::Retarded: f = FallPath(agl_m, speed_mps, path_deg, kRetardedCdA_m2, kBombMassKg); break;
        case WeaponPath::Chute: f = ChuteFall(agl_m, speed_mps, path_deg); break;
        case WeaponPath::Runway:
            // JP233's, Durandal's and BetAB's are let down by chutes at low level; BetAB from a dive or level flight
            // is driven down by its booster after the chute has pointed it: about a free fall.
            f = delivery == "laydown" ? ChuteFall(agl_m, speed_mps, path_deg) : FallPath(agl_m, speed_mps, path_deg, kBombCdA_m2, kBombMassKg);
            break;
        case WeaponPath::Guided:
            // Released where a free fall would land it on the aim (it steers the rest: sam3d).
            f = FallPath(agl_m, speed_mps, path_deg, kBombCdA_m2, kBombMassKg);
            t = std::max(kGuidedFactor * f.t_s, range / std::max(100.0, speed_mps));
            break;
        case WeaponPath::Rocket: t = range / kRocketMps; break;
        case WeaponPath::Missile: {
            const std::string& id = weapon_kind_or_id;
            const double v = id.compare(0, 5, "agm65") == 0 ? kMaverickMps : id.compare(0, 2, "kh") == 0 ? kSovietAgmMps : kMissileMps;
            t = range / v;
            break;
        }
        case WeaponPath::Atgm: t = range / kAtgmMps; break;
        case WeaponPath::Gun: t = range / kGunMps; break;
    }
    if (Falls(path)) {
        if (t < 0.0) t = f.t_s;
        out.throw_m = f.x_m;
    }
    out.time_s = Clamp(t, kMinS, kMaxS);
    return out;
}

// The seconds from release to impact at the delivery's nominal path angle (DeliveryPathDeg); -1 when the weapon is not
// known. The bridge times its loads by WeaponFlightOf from the aircraft's own path angle.
inline double WeaponFlightTime(const std::string& delivery, const std::string& weapon_kind_or_id, double agl_m,
                               double speed_mps, double range_m, const WeaponsDef* weapons = nullptr) {
    return WeaponFlightOf(delivery, weapon_kind_or_id, agl_m, speed_mps, DeliveryPathDeg(delivery), range_m, weapons).time_s;
}

// An aircraft's state at a release, over the ground: its speed, its path angle above the local horizontal, and the unit
// vectors of its track over the ground and of the local up. ENU z is not up far from the origin (the Earth curves away),
// so the local up is taken from the frame at `pos`. With no speed (under 1 m/s) the path angle is `delivery`'s nominal.
struct ReleaseState {
    double speed_mps = 0.0;
    double ground_mps = 0.0;  // its speed over the ground
    double path_deg = 0.0;
    Vec3 along;               // its track over the ground (zero with no speed over the ground)
    Vec3 up;                  // the local up at its place
};
inline ReleaseState ReleaseStateOf(const LocalFrame& frame, const Vec3& pos, const Vec3& vel, const std::string& delivery) {
    ReleaseState s;
    GeoPoint g = frame.EnuToGeodetic(pos);
    g.alt_m += 1.0;
    s.up = (frame.GeodeticToEnu(g) - pos).Normalized();
    s.speed_mps = vel.Length();
    const double vz = Dot(vel, s.up);
    const Vec3 vh = vel - s.up * vz;
    s.ground_mps = vh.Length();
    s.along = s.ground_mps > 1e-9 ? vh / s.ground_mps : Vec3{};
    s.path_deg = s.speed_mps < 1.0 ? DeliveryPathDeg(delivery) : RadToDeg(std::atan2(vz, s.ground_mps));
    return s;
}

}  // namespace airwar
}  // namespace simcore
