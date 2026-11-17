// perception_bench — what the pilots' perception costs (docs/air_battle/03_PERCEPTION.md 5.7, roadmap B3).
//
// Part 1, the hub alone: N aircraft of two sides flying across a 60 x 60 km hilly range, a battery launching, flak
// bursting and tracers, ground objects and a receiver on a third of them; 60 s of simulated time at 0.05 s a step.
// It prints the mean and the worst time per step spent in perception (the due pilots' ticks), against the budget of
// 0.15 ms mean for about 100 aircraft.
//
// Part 2 (--runner [seconds]): the dynamic campaign testbed's hour in the scenario runner, with perception off and on
// (ScenarioRunner::SetPerception), the mean and worst wall time per step of each and the difference.
//
// Usage: perception_bench [data_dir] [aircraft] [--runner [seconds]]
// Generic placeholder aircraft and numbers; no real system's data.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war/perception_feed.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

using Clock = std::chrono::steady_clock;

double Ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

class Hills : public Terrain {
public:
    double HeightAt(double e, double n) const override { return 250.0 + 120.0 * std::sin(e * 0.0013) * std::cos(n * 0.0011) + 60.0 * std::sin((e + 2.0 * n) * 0.0031); }
};

struct Stats {
    double total_ms = 0.0;
    double worst_ms = 0.0;
    int steps = 0;
    void Add(double ms) {
        total_ms += ms;
        worst_ms = std::max(worst_ms, ms);
        ++steps;
    }
    double Mean() const { return steps > 0 ? total_ms / steps : 0.0; }
};

int HubBench(const std::string& data_dir, int n) {
    PerceptionData data;
    std::string error;
    if (!LoadPerceptionData(data_dir, data, &error)) {
        std::fprintf(stderr, "perception_bench: %s\n", error.c_str());
        return 2;
    }
    PerceptionFeed feed(data, 1);
    PerceptionHub& hub = feed.hub;
    Hills terrain;
    LocalFrame frame;
    EnvIn env;
    env.terrain = &terrain;
    env.frame = &frame;
    env.light.sun_elevation_deg = 35.0;
    env.light.sun_azimuth_deg = 180.0;
    env.visibility_m = 20000.0;
    const RwrSetDisplay* nato = data.RwrSet("nato_alr46_class_rwr");

    struct Ac {
        AirObject o;
        ObserverIn ob;
    };
    std::vector<Ac> fleet;
    for (int i = 0; i < n; ++i) {
        Ac a;
        a.o.id = "ac" + std::to_string(i);
        a.o.key = ObjectKeyOf(a.o.id);
        a.o.side = i % 2;
        const double ang = 6.2831853 * i / n;
        a.o.pos = Vec3{25000.0 * std::cos(ang), 25000.0 * std::sin(ang), 300.0 + 100.0 * (i % 7)};
        a.o.vel = Vec3{-220.0 * std::cos(ang), -220.0 * std::sin(ang), 0.0};
        a.o.alt_m = a.o.pos.z;
        a.o.size_m = 4.5;
        a.ob.id = a.o.id;
        a.ob.key = a.o.key;
        a.ob.side = a.o.side;
        a.ob.skill = 0.7;
        a.ob.two_seat = i % 5 == 0;
        a.ob.rwr_set = i % 3 == 0 ? nato : nullptr;
        fleet.push_back(a);
    }
    for (int k = 0; k < 10; ++k) hub.AddGround("osa_battery_silent", Vec3{-4000.0 + 900.0 * k, 3000.0 - 600.0 * k, 250.0}, "site");
    Stats cost;
    std::uint64_t sightings = 0;
    const double dt = 0.05;
    int launch = 0;
    for (double t = 0.0; t < 60.0; t += dt) {
        // The world's events this step: a launch every 8 s, flak bursts and tracer.
        if (std::fmod(t, 8.0) < dt) {
            ++launch;
            feed.Launch(launch, "own", Vec3{0.0, 0.0, 250.0}, t, false);
            feed.Synthetic("own", launch, Vec3{8000.0, 8000.0, 1500.0}, 25.0, 400.0);
        }
        if (std::fmod(t, 0.2) < dt) {
            feed.Burst(Vec3{3000.0 * std::cos(t), 3000.0 * std::sin(t), 800.0}, t, false);
            feed.Tracer("gun1", Vec3{1000.0, 1000.0, 260.0}, t);
        }
        feed.Update(t);
        const auto t0 = Clock::now();
        std::vector<std::size_t> due;
        for (std::size_t i = 0; i < fleet.size(); ++i) {
            if (hub.Due(fleet[i].ob.key, t)) due.push_back(i);
        }
        if (!due.empty()) {
            std::vector<AirObject> air;
            air.reserve(fleet.size());
            for (Ac& a : fleet) air.push_back(a.o);
            std::vector<Sighting> fresh;
            for (std::size_t i : due) {
                Ac& a = fleet[i];
                a.ob.pos = a.o.pos;
                a.ob.vel = a.o.vel;
                a.ob.alt_m = a.o.pos.z;
                a.ob.agl_m = a.o.pos.z - terrain.HeightAt(a.o.pos.x, a.o.pos.y);
                hub.Tick(a.ob, env, t, air, {}, &fresh);
            }
            sightings += fresh.size();
        }
        cost.Add(Ms(Clock::now() - t0));
        for (Ac& a : fleet) a.o.pos += a.o.vel * dt;
    }
    std::printf("hub alone: %d aircraft, 60 s at %.2f s a step: perception %.3f ms mean, %.3f ms worst a step (budget 0.15 ms mean for about 100)\n", n, dt,
                cost.Mean(), cost.worst_ms);
    std::printf("  %llu pilot ticks, %llu looks, %llu sight lines, %llu new sightings\n", static_cast<unsigned long long>(hub.Ticks()),
                static_cast<unsigned long long>(hub.Looks()), static_cast<unsigned long long>(hub.SightLines()), static_cast<unsigned long long>(sightings));
    return 0;
}

Stats RunnerHour(const std::string& data_dir, double seconds, bool perception) {
    Stats st;
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenarioFile(data_dir + "/scenarios/dynamic_campaign_test.json", data_dir, def, console, &error)) {
        std::fprintf(stderr, "perception_bench: %s\n", error.c_str());
        return st;
    }
    ScenarioRunner runner(std::move(def), std::move(console));
    runner.SetPerception(perception);
    const double dt = 0.05;
    while (runner.Time() < seconds && !runner.Finished()) {
        const auto t0 = Clock::now();
        runner.Step(dt);
        st.Add(Ms(Clock::now() - t0));
    }
    return st;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data_dir = "data";
    int n = 100;
    bool runner = false;
    double seconds = 600.0;
    int pos = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--runner") == 0) {
            runner = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') seconds = std::atof(argv[++i]);
        } else if (pos == 0) {
            data_dir = argv[i];
            ++pos;
        } else if (pos == 1) {
            n = std::atoi(argv[i]);
            ++pos;
        }
    }
    const int rc = HubBench(data_dir, n);
    if (rc != 0 || !runner) return rc;
    const Stats off = RunnerHour(data_dir, seconds, false);
    const Stats on = RunnerHour(data_dir, seconds, true);
    std::printf("testbed, %.0f s: perception off %.3f ms mean, %.2f ms worst a step; on %.3f ms mean, %.2f ms worst: %.3f ms mean for perception\n",
                seconds, off.Mean(), off.worst_ms, on.Mean(), on.worst_ms, on.Mean() - off.Mean());
    return 0;
}
