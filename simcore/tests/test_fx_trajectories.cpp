// Drawn paths (simcore/fx/trajectories.hpp) and the humidity aloft for contrails (simcore/fx/air.hpp), docs/fx/README.md
// 3.3: motor phases; our missile's drawn launch gathering onto the simulation's line, continuous; bomb paths that start
// and end exactly where and when the simulation says, at the end angle each kind has and never under the ground; a
// chute's descent, a glide's dive, a dispenser's opening; a wreck's fall against the closed form for quadratic drag;
// rounds against the flat-fire hand calculation and a numerical integration, lead aim meeting its target; humidity in
// and out of cloud layers, and contrails at 10.5 km in a standard atmosphere. Numbers are generic placeholders.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "simcore/fx/air.hpp"
#include "simcore/fx/trajectories.hpp"
#include "simcore/weather_data.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::fx;

namespace {

double LowestZ(const BombPath& path, int samples = 2000) {
    const BombPathParams& p = path.Params();
    double lowest = 1e300;
    for (int i = 0; i <= samples; ++i) {
        const double t = p.release_t_s + (p.impact_t_s - p.release_t_s) * i / samples;
        lowest = std::min(lowest, path.Position(t).z);
    }
    return lowest;
}

double AngleBelowDeg(const Vec3& v) { return RadToDeg(std::atan2(-v.z, v.HorizontalLength())); }

BombPathParams Drop(BombPathParams::Kind kind, const Vec3& pos, const Vec3& vel, const Vec3& impact, double t0, double t1) {
    BombPathParams p;
    p.kind = kind;
    p.release_pos = pos;
    p.release_vel = vel;
    p.impact_pos = impact;
    p.release_t_s = t0;
    p.impact_t_s = t1;
    return p;
}

// The full equation of a round's flight, v' = -k |v| v - g, by RK4.
Vec3 IntegrateRound(const Vec3& muzzle, const Vec3& dir, double v0, double k, double t_end, double dt = 1e-3) {
    Vec3 p = muzzle, v = dir.Normalized() * v0;
    const auto accel = [&](const Vec3& u) { return Vec3{0.0, 0.0, -kStandardGravity} - u * (k * u.Length()); };
    const int n = static_cast<int>(std::lround(t_end / dt));
    for (int i = 0; i < n; ++i) {
        const Vec3 a1 = accel(v), v1 = v;
        const Vec3 a2 = accel(v + a1 * (0.5 * dt)), v2 = v + a1 * (0.5 * dt);
        const Vec3 a3 = accel(v + a2 * (0.5 * dt)), v3 = v + a2 * (0.5 * dt);
        const Vec3 a4 = accel(v + a3 * dt), v4 = v + a3 * dt;
        p += (v1 + v2 * 2.0 + v3 * 2.0 + v4) * (dt / 6.0);
        v += (a1 + a2 * 2.0 + a3 * 2.0 + a4) * (dt / 6.0);
    }
    return p;
}

}  // namespace

// ---------------------------------------------------------------------------------------------- motors and our missile

SC_TEST("fx: motor phases from boost and sustain times") {
    SC_EXPECT(MotorPhaseAt(-0.1, 2.0, 10.0) == MotorPhase::None);
    SC_EXPECT(MotorPhaseAt(0.0, 2.0, 10.0) == MotorPhase::Boost);
    SC_EXPECT(MotorPhaseAt(1.99, 2.0, 10.0) == MotorPhase::Boost);
    SC_EXPECT(MotorPhaseAt(2.0, 2.0, 10.0) == MotorPhase::Sustain);
    SC_EXPECT(MotorPhaseAt(11.99, 2.0, 10.0) == MotorPhase::Sustain);
    SC_EXPECT(MotorPhaseAt(12.0, 2.0, 10.0) == MotorPhase::Coast);
    SC_EXPECT(MotorPhaseAt(3.0, 4.0, 0.0) == MotorPhase::Boost);
    SC_EXPECT(MotorPhaseAt(4.5, 4.0, 0.0) == MotorPhase::Coast);
    SC_EXPECT(MotorPhaseAt(0.0, 0.0, 0.0) == MotorPhase::Coast);  // no motor: a bomb
}

SC_TEST("fx: our missile leaves along its launch line and gathers onto the simulation's") {
    const Vec3 launch{100.0, 200.0, 3.5};
    const Vec3 dir = DirectionFromAzEl(0.6, DegToRad(27.0));
    // The simulation's missile: a straight line from the launcher toward a target elsewhere, at 650 m/s.
    const Vec3 sim_dir = DirectionFromAzEl(0.9, DegToRad(10.0));
    const auto sim_pos = [&](double t) { return launch + sim_dir * (650.0 * std::max(t, 0.0)); };
    const Vec3 sim_vel = sim_dir * 650.0;
    // Its own run: 0.5 x 200 m/s^2 x t^2 until 2.5 s.
    SC_EXPECT_NEAR(MissileDisplayRunM(1.0), 100.0, 1e-9);
    SC_EXPECT_NEAR(MissileDisplayRunM(3.0), 625.0 + 250.0, 1e-9);
    SC_EXPECT_NEAR(MissileDisplaySpeedMps(1.25), 250.0, 1e-9);
    // At launch it is on the rail; until 0.3 s on its own line; from 2 s exactly the simulation's.
    const Vec3 at0 = MissileDisplayPosition(launch, dir, 0.0, sim_pos(0.0));
    SC_EXPECT((at0 - launch).Length() < 1e-12);
    const Vec3 at02 = MissileDisplayPosition(launch, dir, 0.2, sim_pos(0.2));
    SC_EXPECT((at02 - (launch + dir * 4.0)).Length() < 1e-9);
    for (double t : {2.0, 2.5, 4.0}) {
        const Vec3 drawn = MissileDisplayPosition(launch, dir, t, sim_pos(t));
        SC_EXPECT(drawn.x == sim_pos(t).x && drawn.y == sim_pos(t).y && drawn.z == sim_pos(t).z);
    }
    // Continuous: between 1 ms samples it moves no more than its own speed allows, and the velocity is the derivative.
    const double dt = 1e-3;
    double worst = 0.0;
    for (int i = 0; i < 3000; ++i) {
        const double t = i * dt;
        const Vec3 a = MissileDisplayPosition(launch, dir, t, sim_pos(t));
        const Vec3 b = MissileDisplayPosition(launch, dir, t + dt, sim_pos(t + dt));
        const double va = MissileDisplayVelocity(launch, dir, t, sim_pos(t), sim_vel).Length();
        const double vb = MissileDisplayVelocity(launch, dir, t + dt, sim_pos(t + dt), sim_vel).Length();
        SC_EXPECT((b - a).Length() <= std::max(va, vb) * dt * 1.01 + 1e-9);
        if (i > 0) {
            const Vec3 back = MissileDisplayPosition(launch, dir, t - dt, sim_pos(t - dt));
            const Vec3 fd = (b - back) / (2.0 * dt);
            worst = std::max(worst, (fd - MissileDisplayVelocity(launch, dir, t, sim_pos(t), sim_vel)).Length());
        }
    }
    SC_EXPECT(worst < 1.0);  // m/s; the boost's corner at 2.5 s is past the gather
}

// ---------------------------------------------------------------------------------------------- bombs

SC_TEST("fx: a free-fall bomb starts and ends where the simulation says and arrives steeply") {
    // Level at 1000 m and 250 m/s: about 15 s and 3.4 km. At impact 0.8 x 250 = 200 m/s across and
    // 0.9 sqrt(2 g 1000) = 126.04 m/s down: 32.2° below the horizon.
    const BombPath path(Drop(BombPathParams::FreeFall, {0, 0, 1000}, {0, 250, 0}, {0, 3375, 0}, 100.0, 115.0));
    SC_EXPECT((path.Position(100.0) - Vec3{0, 0, 1000}).Length() < 1e-12);
    SC_EXPECT((path.Position(115.0) - Vec3{0, 3375, 0}).Length() < 1e-12);
    SC_EXPECT((path.Position(90.0) - Vec3{0, 0, 1000}).Length() < 1e-12);  // waiting on the rack before
    SC_EXPECT((path.Position(130.0) - Vec3{0, 3375, 0}).Length() < 1e-12);
    SC_EXPECT((path.Velocity(100.0) - Vec3{0, 250, 0}).Length() < 1e-9);
    const Vec3 end = path.ImpactVelocity();
    SC_EXPECT_NEAR(end.y, 200.0, 1e-9);
    SC_EXPECT_NEAR(end.z, -0.9 * std::sqrt(2.0 * kStandardGravity * 1000.0), 1e-9);
    SC_EXPECT_NEAR(AngleBelowDeg(end), 32.21, 0.01);
    SC_EXPECT((path.Velocity(115.0 - 1e-7) - end).Length() < 1e-3);
    SC_EXPECT(!path.Done(114.99) && path.Done(115.0));
    SC_EXPECT(LowestZ(path) >= -1.0);
    // It points along its velocity: level at release, nose down at the end.
    SC_EXPECT(std::fabs(path.Forward(100.0).z) < 1e-12);
    SC_EXPECT(path.Forward(114.9).z < -0.4);
    // Velocity is the derivative of the position.
    for (double t : {101.0, 105.5, 112.0}) {
        const Vec3 fd = (path.Position(t + 1e-4) - path.Position(t - 1e-4)) / 2e-4;
        SC_EXPECT((fd - path.Velocity(t)).Length() < 1e-3);
    }
    // From 300 m, 0.9 x 76.7 m/s down against 200 m/s across is under 30°, but 30° (115.5 m/s down) is more than
    // gravity gives from that height: it arrives at the drag-free sqrt(2 g 300) = 76.71 m/s, 20.98° below.
    const BombPath low(Drop(BombPathParams::FreeFall, {0, 0, 300}, {0, 250, 0}, {0, 1800, 0}, 0.0, 8.0));
    SC_EXPECT_NEAR(low.ImpactVelocity().z, -std::sqrt(2.0 * kStandardGravity * 300.0), 1e-9);
    SC_EXPECT_NEAR(AngleBelowDeg(low.ImpactVelocity()), 20.9836, 1e-3);
    SC_EXPECT(LowestZ(low) >= -1.0);
}

SC_TEST("fx: a laydown arrives no faster than gravity allows, without a hook into the ground") {
    // Level at 60 m and 250 m/s, 3.51 s to impact: the drag-free fall from 60 m is sqrt(2 g 60) = 34.30 m/s, the most it
    // can arrive with (9.73° below 200 m/s across); the 30° floor would ask 115.5 m/s. The cubic's vertical acceleration
    // at the end, (6 h + 2 T vz0 + 4 T vz1) / T^2 = (360 - 4 x 3.51 x 34.30) / 3.51^2 = -9.87 m/s^2: about gravity, where
    // the floor's -115.5 m/s gave -102 m/s^2 (a 10 g hook in the last second).
    const double g = kStandardGravity, fall = std::sqrt(2.0 * g * 60.0), t1 = 3.51;
    const BombPath path(Drop(BombPathParams::FreeFall, {0, 0, 60}, {0, 250, 0}, {0, 800, 0}, 0.0, t1));
    SC_EXPECT_NEAR(path.ImpactVelocity().z, -fall, 1e-9);
    SC_EXPECT_NEAR(AngleBelowDeg(path.ImpactVelocity()), 9.7328, 1e-3);
    const double dt = 1e-3;
    const double zpp = (path.Position(t1 - 2.0 * dt).z - 2.0 * path.Position(t1 - dt).z + path.Position(t1).z) / (dt * dt);
    SC_EXPECT_NEAR(zpp, (6.0 * 60.0 - 4.0 * t1 * fall) / (t1 * t1), 0.05);
    SC_EXPECT_NEAR(zpp, -9.873, 0.05);
    SC_EXPECT(LowestZ(path) >= -1.0);
    // A Snakeye from the same pass: 0.3 x 250 = 75 m/s across, and down the 34.30 m/s gravity gives (the 30° floor's
    // 43.3 m/s capped): 24.58° below.
    const BombPath snake(Drop(BombPathParams::Retarded, {0, 0, 60}, {0, 250, 0}, {0, 400, 0}, 0.0, 3.85));
    SC_EXPECT_NEAR(snake.ImpactVelocity().z, -fall, 1e-9);
    SC_EXPECT_NEAR(AngleBelowDeg(snake.ImpactVelocity()), RadToDeg(std::atan2(fall, 75.0)), 1e-9);
    SC_EXPECT(LowestZ(snake) >= -1.0);
}

SC_TEST("fx: every kind of drop stays above the ground until it arrives") {
    struct Release {
        Vec3 pos, vel, impact;
        double t1;
    };
    // Level high, level low, a 30° dive at 300 m/s from 2 km, a loft at 120 m/s up from 200 m (apex ~ 930 m).
    const std::vector<Release> releases = {
        {{0, 0, 1000}, {0, 250, 0}, {0, 3375, 0}, 15.0},
        {{0, 0, 150}, {0, 230, 0}, {0, 1100, 0}, 6.0},
        {{0, 0, 2000}, {0, 260, -150}, {0, 2340, 0}, 10.04},
        {{0, 0, 200}, {0, 220, 120}, {0, 4860, 0}, 26.0},
    };
    for (const auto kind : {BombPathParams::FreeFall, BombPathParams::Retarded, BombPathParams::Parachute,
                            BombPathParams::Glide, BombPathParams::Rocket, BombPathParams::Missile}) {
        for (const Release& r : releases) {
            const BombPath path(Drop(kind, r.pos, r.vel, r.impact, 50.0, 50.0 + r.t1));
            SC_EXPECT((path.Position(50.0) - r.pos).Length() < 1e-9);
            SC_EXPECT((path.Position(50.0 + r.t1) - r.impact).Length() < 1e-9);
            SC_EXPECT(LowestZ(path) >= r.impact.z - 1.0);
            if (kind != BombPathParams::Rocket && kind != BombPathParams::Missile) {
                SC_EXPECT(path.ImpactVelocity().z < 0.0);
            }
            // No jumps: between 1 ms samples it moves no more than its speed allows.
            double worst = 0.0;
            for (double t = 50.0; t < 50.0 + r.t1; t += 1e-3) {
                const double step = (path.Position(t + 1e-3) - path.Position(t)).Length();
                const double speed = std::max({path.Velocity(t).Length(), path.Velocity(t + 5e-4).Length(),
                                               path.Velocity(t + 1e-3).Length()});
                worst = std::max(worst, step - speed * 1e-3);
            }
            SC_EXPECT(worst < 1e-3);
        }
    }
    // A release diving faster than the height and time allow is drawn at the rate they allow.
    const BombPath steep(Drop(BombPathParams::FreeFall, {0, 0, 500}, {0, 100, -300}, {0, 600, 0}, 0.0, 5.0));
    SC_EXPECT_NEAR(steep.Velocity(0.0).z, -100.0, 1e-9);
    SC_EXPECT(LowestZ(steep) >= -1.0);
}

SC_TEST("fx: a retarded bomb slows and falls steeply") {
    // 230 m/s at 150 m: at the end 0.3 x 230 = 69 m/s across and sqrt(2 g 150) = 54.2 m/s down (the drag-free fall from
    // that height is under 60 m/s): 38.2° below the horizon.
    const BombPath path(Drop(BombPathParams::Retarded, {0, 0, 150}, {0, 230, 0}, {0, 700, 0}, 0.0, 6.0));
    const Vec3 end = path.ImpactVelocity();
    SC_EXPECT_NEAR(end.y, 69.0, 1e-9);
    SC_EXPECT_NEAR(end.z, -std::sqrt(2.0 * kStandardGravity * 150.0), 1e-9);
    SC_EXPECT_NEAR(AngleBelowDeg(end), 38.17, 0.01);
    SC_EXPECT(LowestZ(path) >= -1.0);
    // From higher, the full 60 m/s.
    const BombPath high(Drop(BombPathParams::Retarded, {0, 0, 600}, {0, 230, 0}, {0, 1500, 0}, 0.0, 13.0));
    SC_EXPECT_NEAR(high.ImpactVelocity().z, -60.0, 1e-9);
    SC_EXPECT_NEAR(high.ImpactVelocity().y, 69.0, 1e-9);
}

SC_TEST("fx: a parachute bomb falls free until its chute opens then descends at its rate") {
    // 250 m/s level at 300 m; the chute opens at 1.2 s, the bomb settles over 2 s, then 4.8 s of steady descent at
    // 47 m/s (the times leave room: 292.9 m over 4.8 + 1 s allows 50.5 m/s).
    const BombPath path(Drop(BombPathParams::Parachute, {0, 0, 300}, {0, 250, 0}, {0, 550, 0}, 0.0, 8.0));
    // Before the chute: a drag-free fall.
    const Vec3 at1 = path.Position(1.0);
    SC_EXPECT((at1 - Vec3{0, 250, 300 - 0.5 * kStandardGravity}).Length() < 1e-9);
    // Under the chute: 47 m/s down, nearly vertical.
    for (double t : {3.3, 5.0, 7.9}) {
        const Vec3 v = path.Velocity(t);
        SC_EXPECT_NEAR(v.z, -47.0, 1e-9);
        SC_EXPECT(v.HorizontalLength() <= std::tan(DegToRad(12.0)) * 47.0 + 1e-9);
    }
    SC_EXPECT((path.Position(8.0) - Vec3{0, 550, 0}).Length() < 1e-9);
    SC_EXPECT(LowestZ(path) >= -1.0);
    // Its own descent rate when the caller sets one (munitions.json's retarder "descent_mps").
    BombPathParams slow = Drop(BombPathParams::Parachute, {0, 0, 300}, {0, 250, 0}, {0, 550, 0}, 0.0, 10.0);
    slow.chute_descent_mps = 30.0;
    SC_EXPECT_NEAR(BombPath(slow).Velocity(9.0).z, -30.0, 1e-9);
    // Released too low for that rate in the time given: it descends slower and still lands on time.
    const BombPath low(Drop(BombPathParams::Parachute, {0, 0, 100}, {0, 200, 0}, {0, 350, 0}, 0.0, 8.0));
    SC_EXPECT(low.Velocity(6.0).z > -47.0 && low.Velocity(6.0).z < 0.0);
    SC_EXPECT((low.Position(8.0) - Vec3{0, 350, 0}).Length() < 1e-9);
    SC_EXPECT(LowestZ(low) >= -1.0);
}

SC_TEST("fx: a glide bomb arrives in a shallow dive and a rocket flies straight") {
    // 30 km from 6 km in 150 s: the chord is 11.3° down, the end dive 25°, at the mean speed 30594 / 150 = 204 m/s.
    const BombPath distant(Drop(BombPathParams::Glide, {0, 0, 6000}, {0, 220, 0}, {0, 30000, 0}, 0.0, 150.0));
    SC_EXPECT_NEAR(AngleBelowDeg(distant.ImpactVelocity()), 25.0, 1e-9);
    SC_EXPECT_NEAR(distant.ImpactVelocity().Length(), std::sqrt(30000.0 * 30000.0 + 6000.0 * 6000.0) / 150.0, 1e-9);
    SC_EXPECT(LowestZ(distant) >= -1.0);
    // 6 km from 4 km: the chord's own 33.7°.
    const BombPath near_drop(Drop(BombPathParams::Glide, {0, 0, 4000}, {0, 220, 0}, {0, 6000, 0}, 0.0, 40.0));
    SC_EXPECT_NEAR(AngleBelowDeg(near_drop.ImpactVelocity()), RadToDeg(std::atan2(4000.0, 6000.0)), 1e-9);
    // A rocket fired along its line from a dive: it stays on the chord and arrives at the mean speed.
    const Vec3 from{0, 0, 1000}, to{0, 2000, 0};
    const Vec3 line = (to - from).Normalized();
    const BombPath rocket(Drop(BombPathParams::Rocket, from, line * 250.0, to, 0.0, 3.0));
    double off = 0.0;
    for (double t = 0.0; t <= 3.0; t += 0.01) {
        const Vec3 d = rocket.Position(t) - from;
        off = std::max(off, (d - line * Dot(d, line)).Length());
    }
    SC_EXPECT(off < 1e-6);
    SC_EXPECT_NEAR(rocket.ImpactVelocity().Length(), (to - from).Length() / 3.0, 1e-9);
}

SC_TEST("fx: a cluster dispenser opens at its height on the way down") {
    // Level from 1000 m (the path above): it is 500 m up about 10.4 s after release (the drag-free fall: 10.1 s).
    const BombPath path(Drop(BombPathParams::FreeFall, {0, 0, 1000}, {0, 250, 0}, {0, 3375, 0}, 100.0, 115.0));
    const double t = ClusterOpenTime(path, 500.0);
    SC_EXPECT(t - 100.0 > 9.5 && t - 100.0 < 11.5);
    SC_EXPECT_NEAR(path.Position(t).z, 500.0, 1e-3);
    for (double u = t + 0.01; u < 115.0; u += 0.05) SC_EXPECT(path.Position(u).z < 500.0);
    // Lofted from 200 m, it passes 500 m going up and opens coming down.
    const BombPath loft(Drop(BombPathParams::FreeFall, {0, 0, 200}, {0, 220, 120}, {0, 4860, 0}, 0.0, 26.0));
    const double tl = ClusterOpenTime(loft, 500.0);
    SC_EXPECT_NEAR(loft.Position(tl).z, 500.0, 1e-3);
    SC_EXPECT(loft.Velocity(tl).z < 0.0);
    // Released under its height: at once; a height of 0: at impact.
    SC_EXPECT_NEAR(ClusterOpenTime(BombPath(Drop(BombPathParams::FreeFall, {0, 0, 150}, {0, 230, 0}, {0, 1100, 0}, 7.0, 13.0)),
                                   500.0),
                   7.0, 1e-12);
    SC_EXPECT_NEAR(ClusterOpenTime(path, 0.0), 115.0, 1e-12);
}

// ---------------------------------------------------------------------------------------------- wrecks

SC_TEST("fx: a wreck falls the same way from the same seed") {
    for (const bool falls : {true, false}) {
        WreckFallParams p;
        p.pos = {1000.0, -500.0, 3100.0};
        p.vel = {0.0, 200.0, 0.0};
        p.ground_z_m = 100.0;
        p.seed = 42;
        p.falls = falls;
        const WreckFall a(p), b(p);
        SC_EXPECT(a.GroundTime() == b.GroundTime() && a.TerminalMps() == b.TerminalMps());
        for (double t : {0.0, 0.05, 5.55, 20.0, 31.27}) {
            const Vec3 pa = a.Position(t), pb = b.Position(t);
            SC_EXPECT(pa.x == pb.x && pa.y == pb.y && pa.z == pb.z);
            SC_EXPECT(a.Rotation(t).roll_rad == b.Rotation(t).roll_rad);
        }
        p.seed = 43;
        const WreckFall c(p);
        SC_EXPECT(c.TerminalMps() != a.TerminalMps() || (c.Position(20.0) - a.Position(20.0)).Length() > 1.0);
        // It starts where the aircraft was and moves as it did.
        SC_EXPECT((a.Position(0.0) - p.pos).Length() < 1e-12);
        SC_EXPECT((a.Velocity(0.0) - p.vel).Length() < 1e-12);
    }
}

SC_TEST("fx: a wreck's draws from a seed are the same on every compiler") {
    // std::mt19937(42)'s raw outputs (the standard fixes them; its first is 1608637542), mirrored by hand, taken one a
    // statement in Draw's order: the terminal speeds (falling, spiralling), the lift, its fade, the bank, the side, then
    // each tumble rate's sign before its size. Pinned here so a build that draws in another order fails.
    std::mt19937 rng(42);
    SC_EXPECT(rng() == 1608637542u);
    WreckFallParams p;
    p.pos = {0.0, 0.0, 3100.0};
    p.vel = {0.0, 200.0, 0.0};
    p.ground_z_m = 100.0;
    p.seed = 42;
    p.falls = true;
    const WreckFall fall(p);
    SC_EXPECT_NEAR(fall.TerminalMps(), 104.363502857741, 1e-9);
    const WreckRotation r0 = fall.Rotation(0.0), r1 = fall.Rotation(1.0);
    SC_EXPECT_NEAR(r1.yaw_rad - r0.yaw_rad, 0.837165145297, 1e-9);
    SC_EXPECT_NEAR(r1.pitch_rad - r0.pitch_rad, -1.224165860517, 1e-9);
    SC_EXPECT_NEAR(r1.roll_rad - r0.roll_rad, -1.749937301269, 1e-9);
    p.falls = false;
    SC_EXPECT_NEAR(WreckFall(p).TerminalMps(), 95.930859684013, 1e-9);
}

SC_TEST("fx: a wreck lands on the ground under where it comes down") {
    const double g = kStandardGravity, earth_r = 6371000.0;
    // The Earth's curve in ENU: the ground at sea level is (x^2 + y^2) / 2R below the frame's tangent plane.
    const auto curve = [&](double x, double y) { return -(x * x + y * y) / (2.0 * earth_r); };
    // A constant ground is the flat ground, to the bit.
    WreckFallParams p;
    p.pos = {1000.0, -500.0, 3100.0};
    p.vel = {0.0, 200.0, 0.0};
    p.ground_z_m = 100.0;
    p.seed = 7;
    p.falls = false;
    const WreckFall flat(p);
    p.ground_z_at = [](double, double) { return 100.0; };
    const WreckFall same(p);
    SC_EXPECT(same.GroundTime() == flat.GroundTime());
    SC_EXPECT(same.Position(same.GroundTime()).z == 100.0 && same.Position(31.27).y == flat.Position(31.27).y);
    SC_EXPECT(!same.Params().ground_z_at);  // the function is not kept
    // Dropped from rest 30 km east of the origin, 3100 m up: it falls straight down onto the curve under it,
    // 30000^2 / 2R = 70.6326 m below the plane, so from 3170.63 m: t = (vt / g) acosh(exp(g h / vt^2)).
    p = WreckFallParams{};
    p.pos = {30000.0, 0.0, 3100.0};
    p.seed = 7;
    p.falls = true;
    p.ground_z_at = curve;
    const WreckFall rest(p);
    const double vt = rest.TerminalMps(), h = 3100.0 + 30000.0 * 30000.0 / (2.0 * earth_r);
    SC_EXPECT_NEAR(curve(30000.0, 0.0), -70.6326, 1e-4);
    SC_EXPECT_NEAR(rest.GroundTime(), vt / g * std::acosh(std::exp(g * h / (vt * vt))), 0.01);
    SC_EXPECT_NEAR(rest.Position(rest.GroundTime()).z, curve(30000.0, 0.0), 1e-9);
    // Going on east at 250 m/s from 1100 m up: it drifts out over a kilometre, where the ground is metres lower than
    // under the kill; it lands on the ground there, and the flat plane under the kill would have stopped it above it.
    p.pos = {30000.0, 0.0, 1100.0};
    p.vel = {250.0, 0.0, 0.0};
    const WreckFall outward(p);
    const Vec3 land = outward.Position(outward.GroundTime());
    SC_EXPECT(land.x - 30000.0 > 1000.0);
    SC_EXPECT_NEAR(land.z, curve(land.x, land.y), 1e-9);
    SC_EXPECT(curve(30000.0, 0.0) - land.z > 3.0);
    p.ground_z_at = nullptr;
    p.ground_z_m = curve(30000.0, 0.0);
    const WreckFall plane(p);
    SC_EXPECT(plane.GroundTime() < outward.GroundTime());
    const Vec3 on_plane = plane.Position(plane.GroundTime());
    SC_EXPECT(on_plane.z - curve(on_plane.x, on_plane.y) > 3.0);
    // Over ground rising 20 m a kilometre to the north, a wreck going north lands sooner and higher than on the level,
    // on the slope; tumbling or spiralling, it is above the ground under it all the way down.
    const auto slope = [](double, double y) { return 100.0 + 0.02 * y; };
    for (const bool falls : {true, false}) {
        p = WreckFallParams{};
        p.pos = {0.0, 0.0, 3100.0};
        p.vel = {0.0, 200.0, 0.0};
        p.ground_z_m = 100.0;
        p.seed = 3;
        p.falls = falls;
        const WreckFall level(p);
        p.ground_z_at = slope;
        const WreckFall hill(p);
        const Vec3 down = hill.Position(hill.GroundTime());
        SC_EXPECT_NEAR(down.z, slope(down.x, down.y), 1e-9);
        if (falls) {
            SC_EXPECT(down.y > 1000.0 && down.z > 120.0);
            SC_EXPECT(hill.GroundTime() < level.GroundTime());
        }
        double lowest = 1e300;
        for (double t = 0.0; t < hill.GroundTime(); t += 0.01) {
            const Vec3 q = hill.Position(t);
            lowest = std::min(lowest, q.z - slope(q.x, q.y));
        }
        SC_EXPECT(lowest >= -1e-6);
    }
}

SC_TEST("fx: a falling wreck reaches the ground when the closed form says") {
    // From rest, quadratic drag: h = (vt^2 / g) ln cosh(g t / vt), so t = (vt / g) acosh(exp(g h / vt^2)): 3000 m at
    // vt = 100 m/s is 37.1 s (33.4 s at 120, 39.7 s at 90).
    WreckFallParams p;
    p.pos = {0.0, 0.0, 3100.0};
    p.ground_z_m = 100.0;
    p.seed = 7;
    p.falls = true;
    const WreckFall rest(p);
    const double vt = rest.TerminalMps();
    SC_EXPECT(vt >= 95.0 && vt <= 120.0);
    const double g = kStandardGravity;
    SC_EXPECT_NEAR(rest.GroundTime(), vt / g * std::acosh(std::exp(g * 3000.0 / (vt * vt))), 0.01);
    SC_EXPECT(rest.GroundTime() > 30.0 && rest.GroundTime() < 40.0);
    SC_EXPECT_NEAR(rest.Position(rest.GroundTime()).z, 100.0, 1e-9);
    SC_EXPECT((rest.Position(rest.GroundTime() + 10.0) - rest.Position(rest.GroundTime())).Length() < 1e-9);
    SC_EXPECT(rest.Velocity(rest.GroundTime() + 1.0).Length() == 0.0);
    // Near the ground it falls at nearly its terminal speed: tanh(g t / vt).
    SC_EXPECT_NEAR(rest.Velocity(30.0).z, -vt * std::tanh(g * 30.0 / vt), 0.01);
    // Thrown forward at 200 m/s it falls slower at first (the drag of its speed holds its fall back): later, but in 30-45 s.
    p.vel = {0.0, 200.0, 0.0};
    const WreckFall moving(p);
    SC_EXPECT(moving.GroundTime() > rest.GroundTime());
    SC_EXPECT(moving.GroundTime() > 30.0 && moving.GroundTime() < 45.0);
    // It tumbles at its drawn rates.
    const WreckRotation r0 = moving.Rotation(0.0), r1 = moving.Rotation(1.0);
    SC_EXPECT(std::fabs(r1.roll_rad - r0.roll_rad) >= 1.5 && std::fabs(r1.roll_rad - r0.roll_rad) <= 4.0);
    SC_EXPECT(std::fabs(r1.pitch_rad - r0.pitch_rad) >= 0.6 && std::fabs(r1.pitch_rad - r0.pitch_rad) <= 2.0);
    SC_EXPECT_NEAR(r0.yaw_rad, 0.0, 1e-12);  // heading north
}

SC_TEST("fx: a burning wreck that does not simply fall spirals down") {
    for (std::uint32_t seed = 1; seed <= 10; ++seed) {
        WreckFallParams p;
        p.pos = {0.0, 0.0, 3000.0};
        p.vel = {0.0, 200.0, 0.0};
        p.ground_z_m = 0.0;
        p.seed = seed;
        p.falls = false;
        const WreckFall spiral(p);
        p.falls = true;
        const WreckFall drop(p);
        // Its lift keeps it up longer than a tumbling wreck, and it turns more than half round on the way down.
        SC_EXPECT(spiral.GroundTime() > drop.GroundTime());
        SC_EXPECT(spiral.GroundTime() < 150.0);
        const double turned = spiral.Rotation(spiral.GroundTime()).yaw_rad - spiral.Rotation(0.0).yaw_rad;
        SC_EXPECT(std::fabs(turned) > kPi);
        // Banked into its turn.
        const double roll = spiral.Rotation(10.0).roll_rad;
        SC_EXPECT(std::fabs(roll) >= DegToRad(55.0) - 0.15 && std::fabs(roll) <= DegToRad(70.0) + 0.15);
        SC_EXPECT(roll * turned > 0.0);
        // Smooth: between 10 ms samples no more than its speed allows.
        double worst = 0.0;
        for (double t = 0.0; t < spiral.GroundTime(); t += 0.01) {
            const double step = (spiral.Position(t + 0.01) - spiral.Position(t)).Length();
            const double speed = std::max(spiral.Velocity(t).Length(), spiral.Velocity(t + 0.01).Length());
            worst = std::max(worst, step - speed * 0.01);
        }
        SC_EXPECT(worst < 1e-3);
        SC_EXPECT_NEAR(spiral.Position(spiral.GroundTime()).z, 0.0, 1e-9);
    }
}

// ---------------------------------------------------------------------------------------------- rounds

SC_TEST("fx: a round's drop matches the flat-fire hand calculation") {
    // A placeholder round: 1000 m/s, its speed halving in 2 km (k = ln 2 / 2000 = 3.466e-4 1/m), tracer 5 s, SD 10 s.
    RoundParams round;
    round.muzzle_mps = 1000.0;
    round.drag_k = DragKForHalving(2000.0);
    round.tracer_s = 5.0;
    round.sd_s = 10.0;
    SC_EXPECT_NEAR(round.drag_k, 3.4657e-4, 1e-8);
    SC_EXPECT_NEAR(kDragKTypical23mm, round.drag_k, 1e-15);
    const Vec3 north{0.0, 1.0, 0.0};
    // Without drag: 2000 m and g t^2 / 2 = 19.61 m in 2 s.
    RoundParams vacuum = round;
    vacuum.drag_k = 0.0;
    const RoundState v2 = RoundAt({}, north, vacuum, 2.0);
    SC_EXPECT_NEAR(v2.position.y, 2000.0, 1e-9);
    SC_EXPECT_NEAR(v2.position.z, -0.5 * kStandardGravity * 4.0, 1e-9);
    // With drag at 5 s: a t = k v0 t = 1.7329; run ln(2.7329) / k = 2900.8 m; speed 1000 / 2.7329 = 365.9 m/s; drop
    // g (25/4 + 5/(2 x 0.34657) - ln(2.7329) / (2 x 0.34657^2)) = g (6.25 + 7.2135 - 4.1863) = 90.99 m; falling at
    // g (5 + 0.34657 x 12.5) / 2.7329 = 33.49 m/s.
    const RoundState s5 = RoundAt({}, north, round, 5.0);
    SC_EXPECT_NEAR(s5.position.y, 2900.83, 0.01);
    SC_EXPECT_NEAR(s5.position.z, -90.99, 0.01);
    SC_EXPECT_NEAR(s5.velocity.y, 365.92, 0.01);
    SC_EXPECT_NEAR(s5.velocity.z, -33.49, 0.01);
    // Against the full equation integrated: within a metre at 5 s, horizontal or at 45°.
    for (const Vec3& dir : {north, Vec3{0.0, std::sqrt(0.5), std::sqrt(0.5)}}) {
        for (double t : {2.0, 5.0}) {
            const Vec3 exact = IntegrateRound({}, dir, 1000.0, round.drag_k, t);
            SC_EXPECT((RoundAt({}, dir, round, t).position - exact).Length() < (t < 3.0 ? 0.2 : 1.0));
        }
    }
    // Its tracer burns 5 s; it destroys itself at 10 s.
    SC_EXPECT(RoundAt({}, north, round, 4.9).tracer && RoundAt({}, north, round, 4.9).alive);
    SC_EXPECT(!RoundAt({}, north, round, 5.1).tracer && RoundAt({}, north, round, 5.1).alive);
    SC_EXPECT(!RoundAt({}, north, round, 10.1).alive);
    SC_EXPECT(!RoundAt({}, north, round, -0.1).alive);
    // The time to run a distance inverts the run: level, climbing and diving.
    for (double sin_el : {0.0, 0.7, -0.5}) {
        const double run = RoundFlightAt(3.7, 1000.0, round.drag_k, sin_el).run_m;
        SC_EXPECT_NEAR(RoundTimeToRunS(run, 1000.0, round.drag_k, sin_el), 3.7, 1e-9);
    }
    SC_EXPECT_NEAR(RoundTimeToRunS(2000.0, 1000.0, 0.0, 0.0), 2.0, 1e-12);
}

SC_TEST("fx: a climbing round past its reach takes the time its speed runs out") {
    // The 23 mm placeholder (980 m/s, halving in 2 km) fired straight up: c = sqrt(g / k) = 168.21 m/s, p0 = atan(v0 / c),
    // its speed along the line runs out at p0 / (k c) = 24.0281 s, 7003.56 m up (v0 cos p0 ln(sec p0 + tan p0) / (k c)).
    // Any run beyond that gives that time, not one that grows with each step; at sin 0.9, 25.4828 s and 7161.58 m.
    const double v0 = 980.0, k = DragKForHalving(2000.0);
    SC_EXPECT_NEAR(RoundFlightAt(1e9, v0, k, 1.0).run_m, 7003.56, 0.01);
    for (const double run : {7003.6, 7074.0, 9004.0}) SC_EXPECT_NEAR(RoundTimeToRunS(run, v0, k, 1.0), 24.0281, 1e-4);
    for (const double run : {7161.6, 9004.0}) SC_EXPECT_NEAR(RoundTimeToRunS(run, v0, k, 0.9), 25.4828, 1e-4);
    // Short of its reach it still inverts the run, up to the run-out.
    for (const double t : {5.0, 20.0, 24.0}) {
        SC_EXPECT_NEAR(RoundTimeToRunS(RoundFlightAt(t, v0, k, 1.0).run_m, v0, k, 1.0), t, 1e-6);
    }
    // A target the round cannot catch (a decoy at 3 km going away at 200 m/s): the aim is a finite unit direction and
    // the time of flight the round's own run-out at that elevation (32.0 s), where it ran off to no number before.
    double tof = 0.0;
    const Vec3 dir = LeadAim({}, {0.0, 2000.0, 3000.0}, {0.0, 200.0, 0.0}, v0, k, &tof);
    SC_EXPECT(std::isfinite(dir.x) && std::isfinite(dir.y) && std::isfinite(dir.z) && std::isfinite(tof));
    SC_EXPECT_NEAR(dir.Length(), 1.0, 1e-12);
    const double c = std::sqrt(kStandardGravity * dir.z / k);
    SC_EXPECT_NEAR(tof, std::atan(v0 / c) / (k * c), 1e-3);
    SC_EXPECT(tof > 30.0 && tof < 34.0);
}

SC_TEST("fx: lead aim meets a target flying a straight line") {
    const double v0 = 1000.0, k = DragKForHalving(2000.0);
    RoundParams round;
    round.muzzle_mps = v0;
    round.drag_k = k;
    struct Case {
        Vec3 target, vel;
    };
    // Crossing at 1.7 km, coming head-on, going away and climbing.
    for (const Case& c : {Case{{1500.0, 0.0, 800.0}, {0.0, 220.0, 0.0}}, Case{{0.0, 2500.0, 300.0}, {0.0, -250.0, 0.0}},
                          Case{{-800.0, 900.0, 1200.0}, {-150.0, 150.0, 40.0}}}) {
        double tof = 0.0;
        const Vec3 dir = LeadAim({}, c.target, c.vel, v0, k, &tof);
        SC_EXPECT_NEAR(dir.Length(), 1.0, 1e-12);
        // Re-simulated, the round passes within a metre of the target at the time of flight.
        const Vec3 round_at = RoundAt({}, dir, round, tof).position;
        SC_EXPECT((round_at - (c.target + c.vel * tof)).Length() < 0.01);
        double miss = 1e300;
        for (double t = 0.5 * tof; t <= 1.5 * tof; t += 1e-4) {
            miss = std::min(miss, (RoundAt({}, dir, round, t).position - (c.target + c.vel * t)).Length());
        }
        SC_EXPECT(miss < 1.0);
        // And with the full equation of its flight.
        SC_EXPECT((IntegrateRound({}, dir, v0, k, tof, 1e-4) - (c.target + c.vel * tof)).Length() < 1.0);
    }
}

// ---------------------------------------------------------------------------------------------- the air aloft

SC_TEST("fx: humidity aloft from the surface, between the clouds and inside them") {
    WeatherPreset p;
    p.surface_temperature_c = 15.0;
    p.surface_dewpoint_c = 5.0;
    p.boundary_layer_m = 1000.0;
    CloudLayerDef sc;
    sc.id = "sc";
    sc.kind = "stratocumulus";
    sc.base_m = 2000.0;
    sc.thickness_m = 600.0;
    sc.cover_oktas = 5.0;
    sc.lwc_g_m3 = 0.3;
    CloudLayerDef ci;
    ci.id = "ci";
    ci.kind = "cirrus";
    ci.base_m = 9000.0;
    ci.thickness_m = 1000.0;
    ci.cover_oktas = 2.0;
    ci.extinction_per_km = 0.4;
    p.layers = {sc, ci};
    const Weather w(p, 100.0, 7u);
    // At the ground, Magnus: e(5 °C) / e(15 °C) = 51.2 %. 500 m up the air is 11.75 °C and its dew point 4.1 °C: 59.4 %.
    SC_EXPECT_NEAR(RelativeHumidityAloft(w, 100.0), 0.5121, 0.001);
    SC_EXPECT_NEAR(RelativeHumidityAloft(w, 600.0), 0.5943, 0.001);
    // Above the boundary layer (its top 1100 m) and between the layers: the free troposphere's 40 %.
    SC_EXPECT_NEAR(RelativeHumidityAloft(w, 1600.0), 0.40, 1e-9);
    SC_EXPECT_NEAR(RelativeHumidityAloft(w, 5000.0), 0.40, 1e-9);
    // Inside the stratocumulus, saturated; at its base, halfway.
    SC_EXPECT_NEAR(RelativeHumidityAloft(w, 2300.0), 1.0, 1e-9);
    SC_EXPECT_NEAR(RelativeHumidityAloft(w, 2000.0), 0.70, 1e-9);
    // Inside the cirrus (227.05 K at 9.5 km): 10 % over ice, which is 69.9 % over water.
    const double t95 = w.Air(9500.0).temperature_k;
    SC_EXPECT_NEAR(t95, 227.05, 1e-6);
    SC_EXPECT_NEAR(RelativeHumidityAloft(w, 9500.0), 0.6986, 0.001);
    SC_EXPECT_NEAR(IceHumidity(RelativeHumidityAloft(w, 9500.0), t95), 1.1, 1e-9);
    // Smooth: no step over a metre.
    double worst = 0.0;
    for (double h = 0.0; h < 12000.0; h += 1.0) {
        worst = std::max(worst, std::fabs(RelativeHumidityAloft(w, h + 1.0) - RelativeHumidityAloft(w, h)));
    }
    SC_EXPECT(worst < 0.01);
    // The preset's own humidity aloft.
    p.upper_rh_pct = 70.0;
    const Weather moist(p, 100.0, 7u);
    SC_EXPECT_NEAR(moist.UpperRhPct(), 70.0, 1e-12);
    SC_EXPECT_NEAR(RelativeHumidityAloft(moist, 5000.0), 0.70, 1e-9);
    SC_EXPECT_NEAR(RelativeHumidityAloft(moist, 9500.0), 0.6986, 0.001);  // the cloud sets its own
}

SC_TEST("fx: contrails at 10.5 km in a standard atmosphere") {
    // ISA at 10.5 km: 219.9 K (-53.3 °C), 244.7 hPa. At 40 % over water (67.5 % over ice) a short trail forms; at 70 %
    // (118 % over ice) it persists. At 6 km (-24 °C) none.
    const Weather isa_day;
    SC_EXPECT_NEAR(RelativeHumidityAloft(isa_day, 10500.0), 0.40, 1e-9);
    const ContrailState dry = ContrailAt(isa_day, 10500.0, 0.3);
    SC_EXPECT(dry.kind == ContrailKind::Short);
    SC_EXPECT(dry.margin_k > 0.0);
    SC_EXPECT_NEAR(dry.rh_ice, 0.6755, 0.001);
    SC_EXPECT(ContrailAt(isa_day, 6000.0, 0.3).kind == ContrailKind::None);
    WeatherPreset moist;
    moist.upper_rh_pct = 70.0;
    const ContrailState wet = ContrailAt(Weather(moist, 0.0, 1u), 10500.0, 0.3);
    SC_EXPECT(wet.kind == ContrailKind::Persistent);
    SC_EXPECT_NEAR(wet.rh_ice, 1.182, 0.001);
}

SC_TEST("fx: a weather preset may give its humidity aloft") {
    std::vector<WeatherPreset> out;
    std::string e;
    const std::string head = R"({"presets":[{"id":"x","name":"x","source":"sam3d","confidence":"estimate",
        "surface":{"temperature_c":10,"dewpoint_c":5,"visibility_km":10})";
    SC_EXPECT(LoadWeatherPresetsJson(head + R"(,"upper_rh_pct":55}]})", out, &e));
    SC_EXPECT(out.size() == 1 && out[0].upper_rh_pct == 55.0);
    out.clear();
    SC_EXPECT(LoadWeatherPresetsJson(head + "}]}", out, &e));
    SC_EXPECT(out.size() == 1 && out[0].upper_rh_pct < 0.0);
    out.clear();
    SC_EXPECT(!LoadWeatherPresetsJson(head + R"(,"upper_rh_pct":120}]})", out, &e));
    SC_EXPECT(e.find("upper_rh_pct") != std::string::npos);
    // The shipped presets: dry under the high, moist ahead of the front.
    WeatherPreset clear, poor;
    SC_EXPECT(FindWeatherPreset(SIMCORE_DATA_DIR, "clear", clear, &e) && FindWeatherPreset(SIMCORE_DATA_DIR, "poor", poor, &e));
    SC_EXPECT(clear.upper_rh_pct == 30.0 && poor.upper_rh_pct == 70.0);
}
