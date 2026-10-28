// simcore/battery_fire.hpp — the battery's other launchers in the fight, and the battery commander's fire distribution
// (roadmap 8.9, docs/crew/07_COMMANDER_MODE.md §3).
//
// The player's own vehicle is simulated switch by switch (console, crew, engagement). The battery's other launchers are
// not: each is a Shooter with an abstract crew that works through the textbook's network chart (voenizdat_osa_1991
// Fig. 7.2: the tracking radar onto the target 10 s, capture 6-7 s, computer 3 s, missile preparation 15 s, launch
// 1.2 s; the result watched 10 s), every time scaled by its crew's tier (docs/crew/05_SKILL.md: the tier's reaction
// against the reference tier's). Its sensors are geometric (the target within the tracking radar's reach); it fires
// only inside the firing zones of the gameplay tuning, from its own position; each launch is resolved with the same
// hit chance as the player's (the class's chance and its low-altitude factor; no crew mistakes, no manoeuvre). Six
// missiles; when empty it asks for the TZM and is reloaded one missile a minute alongside it (flak11).
//
// Fire distribution: the battery commander (an NPC, or the player through Assign) gives each shooter a target on the
// battery net. A shooter with no target takes the nearest hostile in its reach itself only if `autonomous` is set
// (free fire, or the PU-12 lost).
#pragma once

#include <cmath>
#include <cstddef>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "simcore/battery.hpp"
#include "simcore/engagement.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// The network chart's times (voenizdat_osa_1991 Fig. 7.2), seconds.
struct NetworkChart {
    double radar_on_s = 10.0;
    double capture_s = 6.5;
    double computer_s = 3.0;
    double prep_s = 15.0;
    double launch_s = 1.2;
    double result_s = 10.0;
};

// A target as the battery's other vehicles see it (their sensors are geometric: the truth, within reach).
struct BatteryFireTarget {
    std::string id;
    int number = 0;  // its number on the PU-12 board (0: not plotted)
    Vec3 position;
    Vec3 velocity;
    double agl_m = 0.0;
    std::string target_class = "default";
    bool alive = true;
    bool friendly = false;
};

class BatteryFire {
public:
    enum class Phase { Idle, Acquiring, Tracking, Preparing, InFlight };
    struct Shooter {
        std::string callsign;  // the battery vehicle's id
        Vec3 position;
        bool deployed = false;
        int missiles = 6;
        std::string target;
        Phase phase = Phase::Idle;
        double next_s = 0.0;
        double chance = 0.0;
        double flight_s = 0.0;
        int launches = 0;
        int hits = 0;
        double time_scale = 1.0;  // its crew's tier against the reference
        bool asked_tzm = false;
        double reload_clock_s = 0.0;
        bool external = false;      // simulated elsewhere (the player's own Osa, battery_link.hpp): set, not stepped
        bool on_own = false;        // off the battery net (jammed, a missed frequency change): searches and fires on its own
        bool has_sector = false;    // its sector of fire (true azimuth from its position, clockwise from..to)
        double sector_from_deg = 0.0;
        double sector_to_deg = 0.0;
    };

    // Whether a point is in a shooter's sector of fire (any point when it has none).
    static bool InSector(const Shooter& s, const Vec3& point) {
        if (!s.has_sector) return true;
        const double az = RadToDeg(AzimuthOf(point - s.position));
        double span = s.sector_to_deg - s.sector_from_deg;
        while (span < 0.0) span += 360.0;
        double off = az - s.sector_from_deg;
        while (off < 0.0) off += 360.0;
        while (off >= 360.0) off -= 360.0;
        return off <= span + 1e-9;
    }

    // The battery commander sets (or clears, from == to) a launcher's sector of fire. False for an unknown launcher.
    bool SetSector(const std::string& callsign, double from_deg, double to_deg) {
        Shooter* s = Find(callsign);
        if (s == nullptr) return false;
        s->has_sector = from_deg != to_deg;
        s->sector_from_deg = from_deg;
        s->sector_to_deg = to_deg;
        return true;
    }
    struct Event {
        // Ready: a launcher's к бою готов (said by the runner, or a linked Osa; the abstract shooters report readiness
        // when they deploy).
        enum class Kind { Assigned, Captured, Launch, Hit, Miss, Empty, Reloaded, Ready };
        Kind kind = Kind::Assigned;
        double t = 0.0;
        std::string callsign;
        std::string target;
        int number = 0;
    };

    BatteryFire(EngagementTuning tuning, double reach_m, NetworkChart chart = NetworkChart())
        : tuning_(std::move(tuning)), reach_m_(reach_m), chart_(chart) {}

    // The battery's launchers other than the player's, where they are and whether they are set up.
    void Sync(const Battery& battery) {
        for (const Battery::Vehicle& v : battery.Vehicles()) {
            if (v.def.own || v.def.kind != BatteryVehicleKind::Launcher) continue;
            Shooter* s = Find(v.def.id);
            if (s == nullptr) {
                shooters_.push_back(Shooter{});
                s = &shooters_.back();
                s->callsign = v.def.id;
                s->time_scale = time_scale_;
                s->has_sector = v.def.has_sector;  // the scenario's sector; the battery commander may change it later
                s->sector_from_deg = v.def.sector_from_deg;
                s->sector_to_deg = v.def.sector_to_deg;
            }
            s->position = v.position;
            s->deployed = v.deployed;
        }
    }

    // A launcher of no Battery here: another battery of the regiment beside the scenario (the runner's fighting
    // neighbours). Set up where it stands, with its missiles; no TZM comes to it.
    Shooter& AddShooter(const std::string& callsign, const Vec3& position, int missiles) {
        shooters_.push_back(Shooter{});
        Shooter& s = shooters_.back();
        s.callsign = callsign;
        s.position = position;
        s.deployed = true;
        s.missiles = missiles;
        s.time_scale = time_scale_;
        return s;
    }

    // Every shooter's crew at one tier: its times are the chart's times this much longer (1 = the reference crew).
    void SetCrewTimeScale(double scale) {
        time_scale_ = scale > 0.1 ? scale : 0.1;
        for (Shooter& s : shooters_) s.time_scale = time_scale_;
    }
    // Free fire: shooters with no target take the nearest hostile in reach (and in their sector) themselves.
    void SetAutonomous(bool on) { autonomous_ = on; }
    // A launcher that has lost the battery net searches and fires on its own (pvo_bu_ix_1984: independently; wrk:
    // sudden targets engaged at once and reported afterwards).
    void SetOnOwn(const std::string& callsign, bool on) {
        for (Shooter& s : shooters_) {
            if (s.callsign == callsign) s.on_own = on;
        }
    }

    // A launcher simulated elsewhere (the player's own Osa when its battery's post is a linked PU-12M, battery_link.hpp):
    // it keeps its place in the battery and in the fire distribution, but its missiles, target and phase are set from
    // what its crew does. False for an unknown launcher.
    bool SetExternal(const std::string& callsign) {
        Shooter* s = Find(callsign);
        if (s == nullptr) return false;
        s->external = true;
        return true;
    }
    bool SetExternalState(const std::string& callsign, int missiles, const std::string& target, Phase phase) {
        Shooter* s = Find(callsign);
        if (s == nullptr || !s->external) return false;
        if (missiles >= 0) s->missiles = missiles;
        s->target = phase == Phase::Idle ? std::string() : target;
        s->phase = s->target.empty() ? Phase::Idle : phase;
        return true;
    }

    // The battery commander gives a shooter a target. False for an unknown shooter, or one with no missiles.
    bool Assign(const std::string& callsign, const std::string& target, int number, double t) {
        Shooter* s = Find(callsign);
        if (s == nullptr || s->missiles <= 0) return false;
        if (s->target == target && s->phase != Phase::Idle) return true;
        s->target = target;
        s->phase = Phase::Acquiring;
        s->next_s = t + (chart_.radar_on_s + chart_.capture_s) * s->time_scale;
        events_.push_back({Event::Kind::Assigned, t, callsign, target, number});
        return true;
    }

    // One step. `hit` is called for a target a shooter's missile destroys.
    template <typename OnHit>
    void Step(double t, const std::vector<BatteryFireTarget>& targets, std::mt19937& rng, Battery* battery, OnHit hit) {
        for (Shooter& s : shooters_) {
            if (s.external) continue;  // its own crew fights it
            Reload(s, t, battery);
            if (!s.deployed) continue;
            if (s.target.empty() && (autonomous_ || s.on_own) && s.missiles > 0) TakeNearest(s, targets, t);
            const BatteryFireTarget* tg = FindTarget(targets, s.target);
            if (s.phase == Phase::InFlight) {
                if (t < s.next_s) continue;
                const bool alive = tg != nullptr && tg->alive;
                const bool hit_it = alive && Uniform(rng) < s.chance;
                events_.push_back({hit_it ? Event::Kind::Hit : Event::Kind::Miss, t, s.callsign, s.target, tg != nullptr ? tg->number : 0});
                if (hit_it) {
                    ++s.hits;
                    hit(s.target);
                }
                s.phase = hit_it || !alive ? Phase::Idle : Phase::Tracking;
                if (s.phase == Phase::Idle) s.target.clear();
                s.next_s = t + chart_.result_s * s.time_scale;
                continue;
            }
            if (tg == nullptr || !tg->alive || tg->friendly) {
                s.target.clear();
                s.phase = Phase::Idle;
                continue;
            }
            const Vec3 rel = tg->position - s.position;
            const double range = rel.Length();
            if (s.phase == Phase::Acquiring) {
                if (t < s.next_s || range > reach_m_) continue;
                s.phase = Phase::Tracking;
                s.next_s = t + chart_.computer_s * s.time_scale;
                events_.push_back({Event::Kind::Captured, t, s.callsign, s.target, tg->number});
                continue;
            }
            if (range > reach_m_) {
                s.phase = Phase::Acquiring;
                continue;
            }
            const bool in_zone = tuning_.LaunchZoneFor(rel, tg->velocity, tg->agl_m) != EngagementTuning::ZoneKind::None;
            if (s.phase == Phase::Tracking) {
                if (t < s.next_s || !in_zone || s.missiles <= 0) continue;
                s.phase = Phase::Preparing;
                s.next_s = t + (chart_.prep_s + chart_.launch_s) * s.time_scale;
                continue;
            }
            if (s.phase == Phase::Preparing) {
                if (t < s.next_s) continue;
                if (!in_zone) {
                    s.phase = Phase::Tracking;
                    continue;
                }
                --s.missiles;
                ++s.launches;
                const EngagementTuning::Meeting meet = tuning_.MeetingFor(rel, tg->velocity, tg->agl_m);
                double flight = meet.ok ? meet.time_s : tuning_.missile_max_flight_time_s;
                if (flight > tuning_.missile_max_flight_time_s) flight = tuning_.missile_max_flight_time_s;
                s.chance = tuning_.HitChanceFor(tg->target_class) * tuning_.LowAltitudeFactor(tg->target_class, tg->agl_m);
                s.phase = Phase::InFlight;
                s.next_s = t + flight;
                events_.push_back({Event::Kind::Launch, t, s.callsign, s.target, tg->number});
                if (s.missiles == 0) events_.push_back({Event::Kind::Empty, t, s.callsign, std::string(), 0});
            }
        }
    }

    std::vector<Event> TakeEvents() {
        std::vector<Event> out;
        out.swap(events_);
        return out;
    }
    const std::vector<Shooter>& Shooters() const { return shooters_; }
    Shooter* Find(const std::string& callsign) {
        for (Shooter& s : shooters_) {
            if (s.callsign == callsign) return &s;
        }
        return nullptr;
    }
    // Whether some shooter has this target now.
    bool Engaged(const std::string& target) const {
        for (const Shooter& s : shooters_) {
            if (s.target == target && s.phase != Phase::Idle) return true;
        }
        return false;
    }
    double ReachM() const { return reach_m_; }

private:
    static double Uniform(std::mt19937& rng) { return static_cast<double>(rng()) / 4294967296.0; }

    static const BatteryFireTarget* FindTarget(const std::vector<BatteryFireTarget>& targets, const std::string& id) {
        for (const BatteryFireTarget& t : targets) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }

    void TakeNearest(Shooter& s, const std::vector<BatteryFireTarget>& targets, double t) {
        const BatteryFireTarget* best = nullptr;
        double best_r = reach_m_;
        for (const BatteryFireTarget& tg : targets) {
            if (!tg.alive || tg.friendly || Engaged(tg.id) || !InSector(s, tg.position)) continue;
            const double r = (tg.position - s.position).Length();
            if (r < best_r) {
                best_r = r;
                best = &tg;
            }
        }
        if (best != nullptr) Assign(s.callsign, best->id, best->number, t);
    }

    // Empty: ask for the TZM; alongside it, one missile a minute (flak11: six in 6 min).
    void Reload(Shooter& s, double t, Battery* battery) {
        if (battery == nullptr || s.missiles >= 6) {
            s.asked_tzm = false;
            return;
        }
        if (s.missiles == 0 && !s.asked_tzm && s.phase != Phase::InFlight) {
            s.asked_tzm = battery->RequestService(s.callsign, Battery::Service::Reload, t);
            s.reload_clock_s = 0.0;
        }
        if (!battery->Alongside(s.callsign, Battery::Service::Reload)) return;
        if (s.reload_clock_s == 0.0) s.reload_clock_s = t + 60.0;
        if (t < s.reload_clock_s) return;
        ++s.missiles;
        battery->Consume(s.callsign, Battery::Service::Reload, 1.0, t);
        s.reload_clock_s = t + 60.0;
        if (s.missiles >= 6) {
            battery->Done(s.callsign, Battery::Service::Reload, t);
            events_.push_back({Event::Kind::Reloaded, t, s.callsign, std::string(), 0});
            s.asked_tzm = false;
        }
    }

    EngagementTuning tuning_;
    double reach_m_;
    NetworkChart chart_;
    double time_scale_ = 1.0;
    bool autonomous_ = false;
    std::vector<Shooter> shooters_;
    std::vector<Event> events_;
};

}  // namespace simcore
