// simcore/step_profile.hpp — where a session's step goes (roadmap B12, docs/air_battle/02_FLIGHTS_AND_PILOTS.md §5.4).
//
// The runner times the parts of its step when asked (ScenarioRunner::SetProfile): wall time per part, summed over the
// steps since, and the mean and worst whole step. Off, it costs a branch per part. Nothing in the simulation reads it.
#pragma once

#include <chrono>
#include <cstdint>

namespace simcore {

struct StepProfile {
    enum Part : int {
        Levels,      // the flights' levels of detail (air_levels.hpp)
        Flights,     // the command layer's steering and its after-move, the A1 wingmen placed
        World,       // the flight model and the missiles (World::Step)
        Pilots,      // the pilots' height layers, the ground floor, the gone and the lost
        Radio,       // the enemy's calls and our intercept (air_war/air_radio.hpp)
        Waves,       // the testbed's waves, their loads and the testbed's defences
        Crew,        // orders, deliveries, voice, nets, the battery, the crew, the commander and the rest of the step
        Nav,         // the session's roads and movers
        Defences,    // emission, the battery's and the neighbours' fire models
        Post,        // C2 sources, radios, the post and its EM world
        Engagement,  // the engagement model and the unit's EM world (radars, the aircraft's warning receivers)
        Guidance,    // the launcher, the guidance channels in EM, the receivers' warnings
        Perception,  // the pilots' eyes and receivers' displays (air_war/perception_feed.hpp)
        Reactions,   // damage, the reaction chain (reaction_chain.hpp)
        AirFight,    // the live air combat (air_war/air_fight_session.hpp)
        kParts
    };
    std::uint64_t ns[kParts] = {};
    std::uint64_t steps = 0;
    double total_ms = 0.0;
    double worst_ms = 0.0;

    static const char* Name(int p) {
        static const char* const kNames[kParts] = {"levels", "flights", "world", "pilots", "radio", "waves", "crew", "nav",
                                                   "defences", "post", "engagement+EM", "guidance", "perception", "reactions", "air fight"};
        return p >= 0 && p < kParts ? kNames[p] : "?";
    }
    double MeanMs(int p) const { return steps > 0 ? static_cast<double>(ns[p]) * 1e-6 / static_cast<double>(steps) : 0.0; }
    double MeanStepMs() const { return steps > 0 ? total_ms / static_cast<double>(steps) : 0.0; }
};

// The clock the runner laps through its step.
class StepClock {
public:
    using Clock = std::chrono::steady_clock;
    bool On() const { return on_; }
    void SetOn(bool on) { on_ = on; }
    void Start() {
        if (!on_) return;
        step0_ = t0_ = Clock::now();
    }
    // The time since the last lap goes to `part`.
    void Lap(StepProfile::Part part) {
        if (!on_) return;
        const Clock::time_point t1 = Clock::now();
        p_.ns[part] += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0_).count());
        t0_ = t1;
    }
    void End() {
        if (!on_) return;
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - step0_).count();
        p_.total_ms += ms;
        if (ms > p_.worst_ms) p_.worst_ms = ms;
        ++p_.steps;
    }
    const StepProfile& Profile() const { return p_; }
    void Reset() { p_ = StepProfile(); }

private:
    bool on_ = false;
    Clock::time_point t0_, step0_;
    StepProfile p_;
};

}  // namespace simcore
