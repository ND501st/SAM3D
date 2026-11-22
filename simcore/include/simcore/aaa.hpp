// simcore/aaa.hpp — the guns' fire at an aircraft, and how its accuracy falls with range (roadmap R2, docs/DAMAGE.md).
//
// A gun fires where its fire control predicts the aircraft will be: what the director or sight measures of the
// aircraft's place and velocity, carried along a straight line over the round's time of flight (fx::LeadAim, the
// picture's own ballistics). Its errors:
//   * the aim's, an angle across the line of sight: the miss grows with the range;
//   * the measured velocity's, carried over the time of flight: the miss grows with the time of flight;
//   * the measured range's, along the line of sight (a time fuze bursts short or long by it);
//   * the aircraft's own turns while the round flies, which no straight-line prediction follows: they grow with the
//     square of the time of flight. Nothing here models them: the round is judged against where the aircraft really is;
//   * each round's own scatter about the line of fire (the gun's dispersion, an angle);
//   * a time fuze's, in seconds.
// The fire control's errors are drawn once for a pass (a bias the crew does not see), the rest round by round. A layer
// who sees his tracers walks them onto the aircraft through the pass (the errors fall as exp(-c t)). An optical sight by
// night fires with its errors times its night factor.
//
// The light guns' rounds (23 to 57 mm) burst on what they strike or destroy themselves: they are worked out in groups
// (group_s of fire), each round's chance of striking the aircraft from where the group's pattern passes it. The heavy
// guns' shells (the KS-30's) carry a time fuze set for the predicted meeting: each is worked out on its own, where it
// bursts and how far from the aircraft, and the damage model (damage.hpp) draws what the burst did. Every number is in
// data/gameplay/aaa.json with its source; the rounds' own from data/scenery/munitions.json.
#pragma once

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "simcore/damage.hpp"
#include "simcore/fx/trajectories.hpp"
#include "simcore/json.hpp"
#include "simcore/scenery/munitions.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct FireControlDef {
    std::string id;
    bool optical = false;       // a sight, not a radar: by night its errors are night_factor times
    double aim_mrad = 2.0;      // sigma of the aim's error, each way across the line of sight
    double speed_error = 0.1;   // sigma of the measured velocity's error, each way, a share of the aircraft's speed
    double range_error = 0.02;  // sigma of the measured range's error, a share of the range
    double night_factor = 3.0;
    double correction_per_s = 0.0;  // a layer watching his tracers walks them on: the errors fall as exp(-c t) through a pass
};

struct GunDef {
    std::string system;          // air defence system id (air_war/data.hpp AirDefenceSystems)
    std::string munition;        // munitions.json id
    std::string fire_control;
    double dispersion_mrad = 1.5;  // sigma of a round's own scatter, each way across its line of fire
    double fuze_error_s = 0.0;     // sigma of a time fuze's error: this much, plus
    double fuze_error_share = 0.0;  // this share of its running time (a clockwork fuze drifts the longer it runs)
    double group_s = 0.25;         // a light gun's rounds fired in this span are worked out together
    // The round (from munitions.json).
    double calibre_mm = 23.0;
    double muzzle_mps = 980.0;
    double rate_rpm = 2000.0;
    double mass_kg = 0.2;
    double charge_kg = 0.02;
    double sd_lo_s = 0.0;          // self-destruct window (0: none)
    double sd_hi_s = 0.0;
    double drag_k = fx::kDragKTypical23mm;
    bool time_fuze = false;

    fx::RoundParams Round() const {
        fx::RoundParams p;
        p.muzzle_mps = muzzle_mps;
        p.drag_k = drag_k;
        return p;
    }
};

struct AaaTable {
    std::vector<FireControlDef> controls;
    std::vector<GunDef> guns;
    scenery::MunitionLibrary munitions;  // the file the guns' rounds came from (a missile's warhead for the damage model)

    const GunDef* Gun(const std::string& system) const {
        for (const GunDef& g : guns) {
            if (g.system == system) return &g;
        }
        return nullptr;
    }
    const FireControlDef* Control(const std::string& id) const {
        for (const FireControlDef& c : controls) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }
};

// The fire control's errors through one pass.
struct PassErrors {
    double across_rad = 0.0;  // the aim's, to the right of the line of sight
    double up_rad = 0.0;      // and up
    double range_share = 0.0; // the measured range's, a share (> 0: long)
    Vec3 velocity_mps;        // the measured velocity's
};

inline PassErrors DrawPassErrors(const FireControlDef& fc, const Vec3& target_velocity, bool night, std::mt19937& rng) {
    const double f = fc.optical && night ? fc.night_factor : 1.0;
    PassErrors e;
    e.across_rad = f * fc.aim_mrad * 1e-3 * detail::DrawNormal(rng);
    e.up_rad = f * fc.aim_mrad * 1e-3 * detail::DrawNormal(rng);
    e.range_share = f * fc.range_error * detail::DrawNormal(rng);
    const double sv = f * fc.speed_error * target_velocity.Length();
    e.velocity_mps = Vec3{sv * detail::DrawNormal(rng), sv * detail::DrawNormal(rng), sv * detail::DrawNormal(rng)};
    return e;
}

// The pass's errors `into_s` seconds into its fire, as the layer has corrected them by his tracers.
inline PassErrors Corrected(const PassErrors& e, const FireControlDef& fc, double into_s) {
    const double f = std::exp(-std::max(fc.correction_per_s, 0.0) * std::max(into_s, 0.0));
    PassErrors c = e;
    c.across_rad *= f;
    c.up_rad *= f;
    c.range_share *= f;
    c.velocity_mps = e.velocity_mps * f;
    return c;
}

// Right (horizontal) and up across a line of unit direction `los`.
inline void AcrossBasis(const Vec3& los, Vec3& right, Vec3& up) {
    right = Cross(los, Vec3{0.0, 0.0, 1.0});
    right = right.LengthSquared() > 1e-12 ? right.Normalized() : Vec3{1.0, 0.0, 0.0};
    up = Cross(right, los).Normalized();
}

// Where a group of rounds is aimed: its line of fire, the predicted time of flight, the predicted meeting point.
struct GunAim {
    Vec3 dir;
    double tof_s = 0.0;
    Vec3 point;
};

// The fire control's solution against an aircraft at `position` flying `velocity`, with the pass's errors.
inline GunAim AimGun(const GunDef& g, const Vec3& muzzle, const Vec3& position, const Vec3& velocity, const PassErrors& e) {
    const Vec3 los = position - muzzle;
    const double range = los.Length();
    Vec3 right, up;
    const Vec3 u = range > 1e-9 ? los / range : Vec3{0.0, 0.0, 1.0};
    AcrossBasis(u, right, up);
    const Vec3 measured = position + right * (e.across_rad * range) + up * (e.up_rad * range) + u * (e.range_share * range);
    GunAim a;
    a.dir = fx::LeadAim(muzzle, measured, velocity + e.velocity_mps, g.muzzle_mps, g.drag_k, &a.tof_s);
    a.point = fx::RoundAt(muzzle, a.dir, g.Round(), a.tof_s).position;
    return a;
}

// One round's chance of striking an aircraft of `area_m2` when its group's pattern (a round-to-round scatter of sigma
// `spread_m` each way) passes `miss_m` from it: the pattern's share inside the aircraft's area (a disc of the same
// area), 1 - exp(-r^2 / 2 s^2) at the centre, falling off as a normal of variance s^2 + r^2 / 2 with the miss.
inline double RoundHitChance(double miss_m, double spread_m, double area_m2) {
    const double r2 = std::max(area_m2, 0.0) / 3.141592653589793;
    const double s2 = std::max(spread_m * spread_m, 1e-6);
    const double centre = 1.0 - std::exp(-r2 / (2.0 * s2));
    return centre * std::exp(-miss_m * miss_m / (2.0 * (s2 + 0.5 * r2)));
}

// How far across its line of flight the group's pattern passes an aircraft that is at `target` when the group arrives
// (the pattern's centre at the predicted time of flight), and the pattern's spread there. False if its rounds destroy
// themselves before they get there.
inline bool GroupMiss(const GunDef& g, const Vec3& muzzle, const GunAim& aim, const Vec3& target, double& miss_m, double& spread_m) {
    if (g.sd_lo_s > 0.0 && aim.tof_s >= g.sd_lo_s) return false;
    const fx::RoundState c = fx::RoundAt(muzzle, aim.dir, g.Round(), aim.tof_s);
    const Vec3 v = c.velocity.LengthSquared() > 1e-9 ? c.velocity.Normalized() : aim.dir;
    const Vec3 d = target - c.position;
    const Vec3 across = d - v * Dot(d, v);
    miss_m = across.Length();
    spread_m = g.dispersion_mrad * 1e-3 * (c.position - muzzle).Length();
    return true;
}

// The hits of `rounds` contact rounds of a group (each drawn).
inline int GroupHits(const GunDef& g, const Vec3& muzzle, const GunAim& aim, const Vec3& target, int rounds, double area_m2, std::mt19937& rng) {
    double miss = 0.0, spread = 0.0;
    if (rounds <= 0 || !GroupMiss(g, muzzle, aim, target, miss, spread)) return 0;
    const double p = RoundHitChance(miss, spread, area_m2);
    int hits = 0;
    for (int i = 0; i < rounds; ++i) hits += detail::Draw01(rng) < p ? 1 : 0;
    return hits;
}

// A time-fuzed shell's burst: its own scatter about the line of fire and its fuze's error. `after_s`: from firing.
struct ShellBurst {
    Vec3 position;
    double after_s = 0.0;
    Vec3 dir;  // its own line of fire
};

inline ShellBurst TimeShellBurst(const GunDef& g, const Vec3& muzzle, const GunAim& aim, std::mt19937& rng) {
    Vec3 right, up;
    AcrossBasis(aim.dir, right, up);
    const double s = g.dispersion_mrad * 1e-3;
    const Vec3 dir = (aim.dir + right * (s * detail::DrawNormal(rng)) + up * (s * detail::DrawNormal(rng))).Normalized();
    ShellBurst b;
    b.after_s = std::max(0.5, aim.tof_s + (g.fuze_error_s + g.fuze_error_share * aim.tof_s) * detail::DrawNormal(rng));
    b.position = fx::RoundAt(muzzle, dir, g.Round(), b.after_s).position;
    b.dir = dir;
    return b;
}

// --------------------------------------------------------------------------------------------- data/gameplay/aaa.json

namespace detail {

class AaaLoader : public SourcedValuesLoader {
public:
    bool Load(const json::Value& root, const std::string& data_dir, AaaTable& out) {
        if (!Keys(root, "", {"aaa_id", "status", "notes", "source", "confidence", "munitions", "fire_controls", "guns"}) || !Provenance(root, "")) {
            return false;
        }
        std::string munitions;
        if (!RequireString(root, "", "munitions", munitions)) return false;
        scenery::MunitionLibrary& lib = out.munitions;
        std::string e;
        if (!lib.Load(data_dir + "/" + munitions, &e)) return Fail("munitions", e);
        const json::Value* fcs = root.Find("fire_controls");
        if (fcs == nullptr || !fcs->Is(json::Type::Array) || fcs->items.empty()) return Fail("fire_controls", "required, a list");
        for (std::size_t i = 0; i < fcs->items.size(); ++i) {
            const std::string p = Index("fire_controls", i);
            const json::Value& v = fcs->items[i];
            if (!Keys(v, p, {"id", "optical", "aim_mrad", "speed_error", "range_error", "night_factor", "correction_per_s", "notes"})) return false;
            FireControlDef f;
            if (!RequireString(v, p, "id", f.id) || !OptionalBool(v, p, "optical", f.optical)) return false;
            if (!Values(v, p, {{"aim_mrad", &f.aim_mrad}, {"speed_error", &f.speed_error}, {"range_error", &f.range_error}}) ||
                !Values(v, p, {{"night_factor", &f.night_factor}}, true)) {
                return false;
            }
            if (v.Find("correction_per_s") != nullptr && !Values(v, p, {{"correction_per_s", &f.correction_per_s}})) return false;
            if (out.Control(f.id) != nullptr) return Fail(Join(p, "id"), "'" + f.id + "' twice");
            out.controls.push_back(f);
        }
        const json::Value* gs = root.Find("guns");
        if (gs == nullptr || !gs->Is(json::Type::Array) || gs->items.empty()) return Fail("guns", "required, a list");
        for (std::size_t i = 0; i < gs->items.size(); ++i) {
            const std::string p = Index("guns", i);
            const json::Value& v = gs->items[i];
            if (!Keys(v, p, {"system", "munition", "fire_control", "dispersion_mrad", "fuze_error_s", "fuze_error_share", "group_s", "notes"})) return false;
            GunDef g;
            if (!RequireString(v, p, "system", g.system) || !RequireString(v, p, "munition", g.munition) ||
                !RequireString(v, p, "fire_control", g.fire_control)) {
                return false;
            }
            if (out.Gun(g.system) != nullptr) return Fail(Join(p, "system"), "'" + g.system + "' twice");
            if (out.Control(g.fire_control) == nullptr) return Fail(Join(p, "fire_control"), "unknown '" + g.fire_control + "'");
            const scenery::MunitionSpec* m = lib.Find(g.munition);
            if (m == nullptr || !m->IsRound()) return Fail(Join(p, "munition"), "'" + g.munition + "' is not a round in " + munitions);
            if (!(m->muzzle_mps > 0.0) || !(m->rate_rpm > 0.0) || !(m->calibre_mm > 0.0)) {
                return Fail(Join(p, "munition"), "'" + g.munition + "' lacks its calibre, muzzle velocity or rate");
            }
            if (!Values(v, p, {{"dispersion_mrad", &g.dispersion_mrad}}, true)) return false;
            g.time_fuze = m->burst == "time";
            if (g.time_fuze && !Values(v, p, {{"fuze_error_s", &g.fuze_error_s}})) return false;
            if (g.time_fuze && v.Find("fuze_error_share") != nullptr && !Values(v, p, {{"fuze_error_share", &g.fuze_error_share}}, true)) {
                return false;
            }
            if (!g.time_fuze && v.Find("fuze_error_s") != nullptr) return Fail(Join(p, "fuze_error_s"), "only for a time fuze");
            if (v.Find("group_s") != nullptr && !Values(v, p, {{"group_s", &g.group_s}}, true)) return false;
            g.calibre_mm = m->calibre_mm;
            g.muzzle_mps = m->muzzle_mps;
            g.rate_rpm = m->rate_rpm;
            g.mass_kg = m->mass_kg;
            g.charge_kg = m->charge_kg;
            g.sd_lo_s = m->sd_lo_s;
            g.sd_hi_s = m->sd_hi_s;
            g.drag_k = fx::kDragKTypical23mm * 23.0 / g.calibre_mm;  // as the picture's rounds (SimFxEffects.cpp)
            out.guns.push_back(g);
        }
        return true;
    }
    using ConsoleLoader::OptionalBool;
    using ConsoleLoader::RequireString;
};

}  // namespace detail

inline bool LoadAaaFile(const std::string& data_dir, const std::string& rel, AaaTable& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadDataFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "aaa: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "aaa: " + e;
        return false;
    }
    detail::AaaLoader loader;
    AaaTable t;
    if (!loader.Load(root, data_dir, t)) {
        if (error != nullptr) *error = "aaa: " + loader.Message();
        return false;
    }
    out = std::move(t);
    return true;
}

}  // namespace simcore
