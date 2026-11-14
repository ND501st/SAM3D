// aaa_bench: the guns' fire by range and the damage it does (simcore/aaa.hpp, simcore/damage.hpp; roadmap R1, R2),
// as rounds fired for each aircraft brought down, to set beside what history recorded (docs/DAMAGE.md,
// docs/research/AAA_DAMAGE_2026-11.md).
//
// Usage: aaa_bench [data_dir] [passes]
//
// Each line: one gun of data/gameplay/aaa.json against a fighter (size 10 m) crossing its front at 250 m/s, straight
// and level or turning at 4 g across the line of fire from the moment the gun opens, at a slant range: `passes` passes
// of 3 s (30 s for a heavy time-fuzed gun, one gun of its battery), each with the fire control's errors drawn afresh and a fresh aircraft, its rounds worked out as the runner
// does (groups of contact rounds, time-fuzed shells one by one). Brought down: destroyed, or heavily damaged and going
// down. The figures are the model's against this one geometry, not a claim about a battle.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "simcore/aaa.hpp"
#include "simcore/damage.hpp"

using namespace simcore;

namespace {

struct Result {
    long rounds = 0;
    int down = 0;
    int damaged = 0;  // moderate or heavy and flying home
};

// The aircraft `t` seconds after the pass opens: from `p0`, `v0`, turning at `g_turn` g toward `across` (a unit vector
// across its path).
Vec3 Where(const Vec3& p0, const Vec3& v0, const Vec3& across, double g_turn, double t) {
    if (g_turn <= 0.0) return p0 + v0 * t;
    const double speed = v0.Length();
    const double w = g_turn * kStandardGravity / speed;  // turn rate
    const double r = speed / w;
    const Vec3 fwd = v0 / speed;
    return p0 + fwd * (r * std::sin(w * t)) + across * (r * (1.0 - std::cos(w * t)));
}

Result Run(const GunDef& g, const FireControlDef& fc, const DamageTable& dt, double range_m, double height_m, double g_turn, int passes,
           unsigned seed) {
    std::mt19937 rng(seed);
    Result res;
    const Vec3 muzzle{0.0, 0.0, 2.0};
    const double ground = std::sqrt(std::max(range_m * range_m - height_m * height_m, 100.0));
    const Vec3 p0{0.0, ground, height_m};
    const Vec3 v0{250.0, 0.0, 0.0};
    Vec3 right, up;
    AcrossBasis((p0 - muzzle).Normalized(), right, up);
    const Vec3 across = Cross(Cross(v0.Normalized(), up), v0.Normalized()).Normalized();  // the turn across the line of fire
    const double area = PresentedAreaM2(dt, 10.0);
    const double pass_s = g.time_fuze ? 30.0 : 3.0;  // a light gun's burst; a heavy battery's engagement
    for (int k = 0; k < passes; ++k) {
        const PassErrors e0 = DrawPassErrors(fc, v0, false, rng);
        AircraftDamage a;
        double due = 0.0;
        for (double t = 0.0; t < pass_s && a.level != DamageLevel::Destroyed; t += g.group_s) {
            due += g.rate_rpm / 60.0 * g.group_s;
            const int n = static_cast<int>(std::floor(due + 1e-9));
            if (n <= 0) continue;
            due -= n;
            res.rounds += n;
            const Vec3 pos = Where(p0, v0, across, g_turn, t);
            const Vec3 vel = (Where(p0, v0, across, g_turn, t + 0.01) - pos) / 0.01;  // what the director sees now
            const GunAim aim = AimGun(g, muzzle, pos, vel, Corrected(e0, fc, t));
            if (g.time_fuze) {
                for (int i = 0; i < n; ++i) {
                    const ShellBurst b = TimeShellBurst(g, muzzle, aim, rng);
                    const double miss = (b.position - Where(p0, v0, across, g_turn, t + b.after_s)).Length();
                    ApplyDamage(dt, a, DrawBurst(dt, g.mass_kg, g.charge_kg, 0.0, miss, area, rng), t, rng, g.system);
                }
            } else {
                const int hits = GroupHits(g, muzzle, aim, Where(p0, v0, across, g_turn, t + aim.tof_s), n, area, rng);
                if (hits > 0) ApplyDamage(dt, a, DrawRoundHits(dt, g.calibre_mm, hits, rng), t, rng, g.system);
            }
        }
        if (a.level == DamageLevel::Destroyed || a.crash_at_s >= 0.0) {
            ++res.down;
        } else if (a.level >= DamageLevel::Moderate) {
            ++res.damaged;
        }
    }
    return res;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string data = argc > 1 ? argv[1] : "data";
    const int passes = argc > 2 ? std::max(1, std::atoi(argv[2])) : 2000;
    DamageTable dt;
    AaaTable aaa;
    std::string e;
    if (!LoadDamageFile(data + "/gameplay/damage.json", dt, &e) || !LoadAaaFile(data, "gameplay/aaa.json", aaa, &e)) {
        std::fprintf(stderr, "aaa_bench: %s\n", e.c_str());
        return 1;
    }
    std::printf("aaa_bench: %d passes of 3 s (heavy guns 30 s) at each range; a fighter (20 m2) crossing at 250 m/s\n", passes);
    std::printf("%-8s %-14s %8s %8s | %12s %8s %8s | %12s %8s\n", "gun", "fire control", "range", "height", "rounds/down", "down %",
                "damaged%", "4 g turn r/d", "down %");
    unsigned seed = 1u;
    for (const GunDef& g : aaa.guns) {
        if (g.system == "aaa") continue;  // the generic light guns are the ZU-23
        const FireControlDef* fc = aaa.Control(g.fire_control);
        if (fc == nullptr) continue;
        const bool heavy = g.time_fuze;
        const std::vector<double> ranges = heavy ? std::vector<double>{6000.0, 9000.0, 12000.0, 15000.0}
                                                 : std::vector<double>{800.0, 1500.0, 2500.0, 3500.0};
        const double height = heavy ? 5000.0 : 300.0;
        for (double r : ranges) {
            const Result s = Run(g, *fc, dt, r, height, 0.0, passes, seed++);
            const Result t = Run(g, *fc, dt, r, height, 4.0, passes, seed++);
            const auto per = [](const Result& x) { return x.down > 0 ? static_cast<double>(x.rounds) / x.down : -1.0; };
            std::printf("%-8s %-14s %7.0fm %7.0fm | %12.0f %7.2f%% %7.2f%% | %12.0f %7.2f%%\n", g.system.c_str(), fc->id.c_str(), r, height, per(s),
                        100.0 * s.down / passes, 100.0 * s.damaged / passes, per(t), 100.0 * t.down / passes);
        }
    }
    std::printf("(-1: none brought down)\n");
    return 0;
}
