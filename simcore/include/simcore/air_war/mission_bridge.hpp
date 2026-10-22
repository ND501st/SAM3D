// simcore/air_war/mission_bridge.hpp — a raid played as a scenario, brought back into the campaign
// (docs/air_war/07_INTEGRATION.md).
//
// The runner (scenario_runner.hpp) flies the raid's aircraft, the crew fights them and the engagement model decides
// the shots. What the runner doesn't do, the bridge does, from the runner's public state each step:
//   * weapon release: a strike, DEAD or helicopter aircraft alive at its release point drops its load; it lands after
//     `scenario.weapon_fall_s` where it was aimed (where the enemy believed the target was) and hurts what is really
//     there: our vehicle, the battery's other vehicles, a dummy, the defended object (game values in
//     data/gameplay/air_war.json);
//   * anti-radiation missiles: with the EM layer a SEAD or hunter aircraft with a warning receiver carries EM ARMs
//     (the target's "arms", em/arm.hpp): the runner fires and flies them, and the bridge reads their ends (the vehicle
//     hit, a dummy taken, a miss) from ScenarioRunner::ArmResults (roadmap A11). Without (no EM layer, no warning
//     receiver) the gameplay model: in range while our search or tracking radar radiates it fires one; it flies at
//     `scenario.arm_speed_mps` and hits if the radar still radiates when it arrives, or radiated within the seeker's
//     memory if it has one;
//   * the neighbours: our other air defences whose zones reach into the scenario (RaidManifest::neighbours) take their
//     kill chances at the aircraft passing through, by the abstract resolver's rule (its pass chance, fire channels,
//     rounds, the package's SEAD and jamming, night and weather for the optical ones); an aircraft they hit goes down in
//     the scenario, half way through their zone (17.1's first form);
//   * what the enemy learns: every second our radars radiate (ELINT), every launch an enemy aircraft sees.
// Every event goes into the runner's log, so the debrief shows it. Finish() turns it all into a PackageOutcome for
// AirCampaign::CompletePlayerMission.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {
namespace airwar {

class MissionBridge {
public:
    MissionBridge(const RaidManifest& m, const AirWarData& d, unsigned seed) : m_(m), d_(&d), rng_(seed * 2654435761u + 99u) {
        state_.assign(m_.aircraft.size(), AircraftState{});
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) state_[i].arms_left = m_.aircraft[i].arms;
        passes_.assign(m_.aircraft.size() * m_.neighbours.size(), Pass{});
        neighbours_.assign(m_.neighbours.size(), NeighbourState{});
        for (std::size_t j = 0; j < m_.neighbours.size(); ++j) neighbours_[j].missiles = m_.neighbours[j].missiles;
        kill_by_.assign(m_.aircraft.size(), std::string());
    }

    // Call after every runner.Step().
    void Step(ScenarioRunner& r) {
        const TuningTable& tune = d_->tuning;
        const double t = r.Time();
        const double dt = t - last_t_;
        last_t_ = t;
        if (unit_destroyed_) return;
        const ConsoleData& con = r.Console();
        const bool search = con.equipment.IsReady(con.search.antenna) && con.equipment.IsReady(con.search.transmitter);
        const bool track = con.equipment.IsReady(con.track.transmitter);
        const bool radiating = search || track;
        if (radiating && dt > 0.0) {
            emitted_s_ += dt;
            last_radiating_t_ = t;
        }
        // Launches the enemy sees.
        if (r.Launches() > launches_seen_) {
            launches_seen_ = r.Launches();
            for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
                if (!r.TargetAlive(m_.aircraft[i].target_id)) continue;
                if (Dist2d(r.TargetPosition(m_.aircraft[i].target_id), m_.own_enu) < tune.Get("scenario.revealed_by_launch_m")) {
                    launch_seen_ = true;
                    break;
                }
            }
        }
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
            const RaidAircraft& a = m_.aircraft[i];
            AircraftState& s = state_[i];
            if (!r.TargetAlive(a.target_id)) continue;
            const Vec3 pos = r.TargetPosition(a.target_id);
            s.prev_pos = s.seen ? s.last_pos : pos;
            s.last_pos = pos;
            s.seen = true;
            // Weapons at the release point.
            if (a.has_release && !s.released && Dist2d(pos, a.release_enu) < tune.Get("scenario.release_capture_m")) {
                s.released = true;
                if (a.role == FlightRole::Recce) {
                    photographed_ = true;
                    Log(r, a.target_id, "photographs the area");
                } else {
                    Impact imp;
                    imp.t = t + tune.Get("scenario.weapon_fall_s");
                    imp.aircraft = static_cast<int>(i);
                    const double scatter = 120.0 + 200.0 * rng_.Uniform();
                    const double ang = rng_.Uniform(0.0, 6.283185307179586);
                    imp.where = a.aim_enu + Vec3{std::cos(ang) * scatter, std::sin(ang) * scatter, 0.0};
                    impacts_.push_back(imp);
                    Log(r, a.target_id, a.helicopter ? "fires its rockets" : "releases its bombs");
                }
            }
            // Anti-radiation missiles at a radiating radar.
            if (!a.em_arms && s.arms_left > 0 && radiating && t - s.last_arm_t > 10.0) {
                const double range = Dist2d(pos, m_.own_enu);
                if (range < a.arm_range_m) {
                    --s.arms_left;
                    s.last_arm_t = t;
                    Arm arm;
                    arm.t = t + range / tune.Get("scenario.arm_speed_mps");
                    arm.aircraft = static_cast<int>(i);
                    arm.memory = a.arm_memory;
                    arms_.push_back(arm);
                    ++arms_fired_;
                    Log(r, a.target_id, "fires an anti-radiation missile at the radar (" + std::to_string(static_cast<int>(range / 1000.0 + 0.5)) + " km)");
                }
            }
        }
        Neighbours(r);
        ReadEmArms(r);
        if (unit_destroyed_) return;
        // Weapons arriving.
        for (Impact& imp : impacts_) {
            if (imp.done || imp.t > t) continue;
            imp.done = true;
            Explode(r, imp);
            if (unit_destroyed_) return;
        }
        for (Arm& arm : arms_) {
            if (arm.done || arm.t > t) continue;
            arm.done = true;
            const bool on = radiating || (arm.memory && t - last_radiating_t_ < tune.Get("scenario.arm_memory_s"));
            const std::string who = m_.aircraft[static_cast<std::size_t>(arm.aircraft)].target_id;
            if (!on) {
                radar_silenced_ = true;
                Log(r, who, "ARM: the radar went silent, the missile lost it");
                continue;
            }
            if (rng_.Chance(tune.Get("scenario.arm_pk"))) {
                radar_killed_ = true;
                unit_destroyed_ = true;
                Log(r, who, "ARM: hit the vehicle's radar - our vehicle is out of action");
            } else {
                Log(r, who, "ARM: missed");
            }
        }
    }

    bool UnitDestroyed() const { return unit_destroyed_; }
    double EmittedSeconds() const { return emitted_s_; }
    int ArmsFired() const { return arms_fired_ + em_arms_fired_; }

    // What happened, for AirCampaign::CompletePlayerMission.
    PackageOutcome Finish(ScenarioRunner& r) {
        PackageOutcome o;
        o.package_id = m_.package_id;
        o.played = true;
        o.truth_applied = false;
        // Which aircraft went down: our missiles' hits (the engagement model's HIT), the battery's other launchers
        // ("destroyed by <launcher>") and anything else the world destroyed ("destroyed").
        std::vector<bool> down(m_.aircraft.size(), false);
        for (const DebriefEntry& e : r.Log().Entries()) {
            const bool hit = e.kind == DebriefEntry::Kind::Hit;
            const bool destroyed = e.kind == DebriefEntry::Kind::Scenario && e.detail.compare(0, 9, "destroyed") == 0;
            if (!hit && !destroyed) continue;
            for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
                if (m_.aircraft[i].target_id == e.what) down[i] = true;
            }
        }
        const std::size_t nf = m_.flight_in.size();
        std::vector<bool> flight_released(nf, false), flight_has_release(nf, false), flight_alive(nf, false);
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
            const RaidAircraft& a = m_.aircraft[i];
            const std::size_t fi = static_cast<std::size_t>(std::max(0, a.flight));
            if (fi >= flight_released.size()) continue;
            if (a.has_release) flight_has_release[fi] = true;
            if (state_[i].released) flight_released[fi] = true;
            if (!down[i]) flight_alive[fi] = true;
            if (!down[i] || a.drone) continue;
            LossRecord l;
            l.flight = a.flight;
            l.by = kill_by_[i].empty() ? m_.own_object : kill_by_[i];
            l.where = state_[i].last_pos;
            l.officer = a.officer;
            l.captured = rng_.Chance(0.5);
            o.losses.push_back(l);
        }
        for (std::size_t fi = 0; fi < flight_released.size(); ++fi) {
            if (flight_released[fi]) o.released_flights.push_back(static_cast<int>(fi));
            else if (flight_has_release[fi] && flight_alive[fi]) o.aborted_flights.push_back(static_cast<int>(fi));
        }
        o.delivered = delivered_;
        o.hit_nothing = delivered_ && !hit_something_;
        o.damage = damage_;
        o.decoys_hit = decoys_hit_;
        o.destroyed = destroyed_;
        o.missiles_spent = r.Launches();
        ReadEmArms(r);
        o.arms_fired = arms_fired_ + em_arms_fired_;
        if (emitted_s_ > 0.0) o.AddEmission(m_.own_object, emitted_s_);
        if (launch_seen_) o.fired.push_back(m_.own_object);
        if (radar_killed_) {
            o.radars_killed.push_back(m_.own_object);
        } else if (radar_silenced_) {
            o.radars_suppressed.push_back(m_.own_object);
        }
        if (photographed_) o.recce_seen.push_back(m_.own_object);
        for (std::size_t j = 0; j < m_.neighbours.size(); ++j) {
            const NeighbourState& ns = neighbours_[j];
            if (!ns.fired) continue;
            o.fired.push_back(m_.neighbours[j].id);
            if (ns.emitted_s > 0.0) o.AddEmission(m_.neighbours[j].id, ns.emitted_s);
            if (ns.spent > 0) o.missiles_by.push_back({m_.neighbours[j].id, ns.spent});
        }
        o.notes = notes_;
        return o;
    }

    // Aircraft the neighbours have shot down so far.
    int NeighbourKills() const {
        int n = 0;
        for (const std::string& k : kill_by_) n += k.empty() ? 0 : 1;
        return n;
    }

private:
    struct AircraftState {
        bool released = false;
        bool seen = false;
        int arms_left = 0;
        double last_arm_t = -1e9;
        Vec3 last_pos;
        Vec3 prev_pos;  // where it was the step before
    };
    // One aircraft's pass through one neighbour's zone: rolled as it enters, the hit (if any) half way through.
    struct Pass {
        bool inside = false;
        bool done = false;
        bool hit = false;
        double flown_m = 0.0;
        double hit_at_m = 0.0;
    };
    struct NeighbourState {
        int missiles = -1;
        int spent = 0;
        bool fired = false;
        double emitted_s = 0.0;
        std::vector<std::pair<int, int>> engaged;  // (flight, its aircraft engaged in their pass)
    };
    struct Impact {
        double t = 0.0;
        int aircraft = -1;
        Vec3 where;
        bool done = false;
    };
    struct Arm {
        double t = 0.0;
        int aircraft = -1;
        bool memory = false;
        bool done = false;
    };

    RaidManifest m_;
    const AirWarData* d_;
    Rng rng_;
    std::vector<AircraftState> state_;
    std::vector<Impact> impacts_;
    std::vector<Arm> arms_;
    double last_t_ = 0.0;
    double emitted_s_ = 0.0;
    double last_radiating_t_ = -1e9;
    int launches_seen_ = 0;
    bool launch_seen_ = false;
    bool unit_destroyed_ = false;
    bool radar_killed_ = false;
    bool radar_silenced_ = false;
    bool photographed_ = false;
    bool delivered_ = false;
    bool hit_something_ = false;
    int arms_fired_ = 0;
    int em_arms_fired_ = 0;
    std::size_t em_arms_read_ = 0;
    std::vector<std::pair<std::string, double>> damage_;
    std::vector<std::string> decoys_hit_;
    std::vector<std::string> destroyed_;
    std::vector<std::string> notes_;
    std::vector<Pass> passes_;  // aircraft x neighbours
    std::vector<NeighbourState> neighbours_;
    std::vector<std::string> kill_by_;  // per aircraft: the neighbour that shot it down

    // The neighbours' fire (see the header): each aircraft's pass through each zone is rolled once, as it enters.
    void Neighbours(ScenarioRunner& r) {
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
            const RaidAircraft& a = m_.aircraft[i];
            const AircraftState& s = state_[i];
            if (!s.seen || !r.TargetAlive(a.target_id)) continue;
            const Vec3 step = s.last_pos - s.prev_pos;
            const double step_m = std::hypot(step.x, step.y);
            for (std::size_t j = 0; j < m_.neighbours.size() && r.TargetAlive(a.target_id); ++j) {
                const RaidNeighbour& nb = m_.neighbours[j];
                Pass& ps = passes_[i * m_.neighbours.size() + j];
                if (ps.done) continue;
                const bool inside = Dist2d(s.last_pos, nb.enu) < nb.defence.radius_m;
                if (inside && !ps.inside) {
                    ps.inside = true;
                    Engage(r, i, j, step, step_m);
                } else if (ps.inside) {
                    ps.flown_m += step_m;
                }
                if (!ps.inside || ps.done) continue;
                // The hit, half way through the zone, or as it leaves if it turned away sooner.
                if (ps.hit && (ps.flown_m >= ps.hit_at_m || !inside)) {
                    ps.done = true;
                    if (r.DestroyTarget(a.target_id, nb.name)) {
                        kill_by_[i] = nb.id;
                        notes_.push_back(a.target_id + " shot down by " + nb.name);
                    }
                    continue;
                }
                if (!inside) ps.done = true;
            }
        }
    }

    void Engage(ScenarioRunner& r, std::size_t i, std::size_t j, const Vec3& step, double step_m) {
        const TuningTable& tune = d_->tuning;
        const RaidAircraft& a = m_.aircraft[i];
        const RaidNeighbour& nb = m_.neighbours[j];
        NeighbourState& ns = neighbours_[j];
        Pass& ps = passes_[i * m_.neighbours.size() + j];
        if (nb.function <= 0.05 || ns.missiles == 0) return;
        // The chord it will fly through the zone on its present heading.
        const double radius = nb.defence.radius_m;
        double chord = radius;
        if (step_m > 1.0) {
            const Vec3 d = state_[i].last_pos - nb.enu;
            const double along = (d.x * step.x + d.y * step.y) / step_m;
            const double cross2 = std::max(0.0, d.x * d.x + d.y * d.y - along * along);
            chord = 2.0 * std::sqrt(std::max(0.0, radius * radius - cross2));
        }
        const bool radar = nb.defence.radar;
        if (radar) ns.emitted_s += chord / 250.0 + 30.0;
        ns.fired = true;
        if (a.drone) {
            // A decoy draws fire: the defence wastes rounds on it and shows itself.
            Spend(ns, 1 + rng_.Index(2));
            if (rng_.Chance(0.6)) r.DestroyTarget(a.target_id, nb.name);  // drones are meant to be lost: never a loss
            ps.done = true;
            return;
        }
        // Its fire channels: how many of this flight it can take on in their pass.
        std::size_t e = 0;
        while (e < ns.engaged.size() && ns.engaged[e].first != a.flight) ++e;
        if (e == ns.engaged.size()) ns.engaged.push_back({a.flight, 0});
        int& engaged = ns.engaged[e].second;
        if (EngagedAircraft(nb.defence.channels, nb.function, engaged + 1) <= engaged) return;
        ++engaged;
        double env = nb.function;
        if (!radar && m_.night) env *= tune.Get("attrition.night");
        if (!radar && m_.poor_weather) env *= tune.Get("attrition.poor_weather");
        const double k = PassKillChance(nb.defence.lethality, chord, radius, a.profile, a.helicopter, nb.defence.low_cover,
                                        nb.defence.medium_cover, radar, radar ? m_.sead_supp : 0.0, radar ? m_.jam : 0.0, env,
                                        Clamp01(a.evasion + 0.3 * (m_.airmanship - 0.5)), tune);
        if (k <= 0.0) return;
        Spend(ns, 1);
        if (rng_.Chance(k)) {
            ps.hit = true;
            ps.hit_at_m = 0.5 * chord;
        }
    }

    static void Spend(NeighbourState& ns, int n) {
        if (ns.missiles < 0) return;
        const int used = std::min(ns.missiles, n);
        ns.missiles -= used;
        ns.spent += used;
    }

    // The EM ARMs' ends (the runner logs each itself): on our vehicle, the radar is killed and the vehicle out of
    // action; on a dummy, that dummy is destroyed; a miss means the crew beat it (silent in time, or a decoy).
    void ReadEmArms(ScenarioRunner& r) {
        em_arms_fired_ = r.ArmsFired();
        const std::vector<EmSensorLink::ArmResult>& res = r.ArmResults();
        for (; em_arms_read_ < res.size(); ++em_arms_read_) {
            const EmSensorLink::ArmResult& a = res[em_arms_read_];
            if (a.hit && a.hit_what == "unit") {
                radar_killed_ = true;
                unit_destroyed_ = true;
                notes_.push_back(a.carrier + " ARM: hit the vehicle's radar - our vehicle is out of action");
                continue;
            }
            if (a.hit && a.hit_what == "dummy") {
                const std::string* best = nullptr;
                double best_m = 1e18;
                for (const auto& dm : m_.dummies) {
                    const double d = Dist2d(dm.second, a.burst_point);
                    if (d < best_m) {
                        best_m = d;
                        best = &dm.first;
                    }
                }
                if (best != nullptr && std::find(decoys_hit_.begin(), decoys_hit_.end(), *best) == decoys_hit_.end()) {
                    decoys_hit_.push_back(*best);
                    destroyed_.push_back(*best);
                }
                notes_.push_back(a.carrier + " ARM: a dummy position is hit");
                continue;
            }
            radar_silenced_ = true;
            notes_.push_back(a.carrier + " ARM: missed");
        }
    }

    void Log(ScenarioRunner& r, const std::string& who, const std::string& what) {
        r.Log().Record({r.Time(), DebriefEntry::Kind::Scenario, "enemy air", who, what});
        notes_.push_back(who + " " + what);
    }

    void AddDamage(const std::string& id, double x) {
        for (auto& e : damage_) {
            if (e.first == id) {
                e.second += x;
                return;
            }
        }
        damage_.push_back({id, x});
    }

    void Explode(ScenarioRunner& r, const Impact& imp) {
        const TuningTable& tune = d_->tuning;
        const RaidAircraft& a = m_.aircraft[static_cast<std::size_t>(imp.aircraft)];
        const double lethal = tune.Get("scenario.bomb_lethal_m") * (a.helicopter ? 0.6 : 1.0);
        const double pk = (a.helicopter ? tune.Get("scenario.rocket_pk_vehicle") : tune.Get("scenario.bomb_pk_vehicle")) *
                          std::min(1.5, std::max(0.2, a.strike_power * 2.0));
        delivered_ = true;
        bool hit = false;
        // Our vehicle.
        if (Dist2d(imp.where, m_.own_enu) < lethal) {
            hit = true;
            if (rng_.Chance(pk)) {
                unit_destroyed_ = true;
                AddDamage(m_.own_object, 0.25);
                Log(r, a.target_id, "bombs: our vehicle is hit and out of action");
                hit_something_ = true;
                return;
            }
            Log(r, a.target_id, "bombs: near miss on our vehicle");
        }
        // The battery's other vehicles: each one lost costs the battery a quarter of its strength.
        for (const auto& v : m_.battery) {
            if (Dist2d(imp.where, v.second) < lethal && rng_.Chance(pk)) {
                hit = true;
                hit_something_ = true;
                AddDamage(m_.own_object, v.first == "PU-12" ? 0.15 : 0.2);
                Log(r, a.target_id, "bombs: vehicle " + v.first + " of the battery is hit");
            }
        }
        // Dummies.
        for (const auto& dm : m_.dummies) {
            if (Dist2d(imp.where, dm.second) < tune.Get("strike.decoy_area_m") * 0.4) {
                hit = true;
                if (std::find(decoys_hit_.begin(), decoys_hit_.end(), dm.first) == decoys_hit_.end()) {
                    decoys_hit_.push_back(dm.first);
                    destroyed_.push_back(dm.first);
                    Log(r, a.target_id, "bombs: a dummy position is destroyed");
                }
            }
        }
        // Other objects (a bridge, a depot) inside the scenario.
        for (const auto& ob : m_.objects) {
            if (Dist2d(imp.where, ob.second) > tune.Get("strike.stale_miss_m") * 0.5) continue;
            const TheatreObject* def = d_->theatre.Find(ob.first);
            const double hard = def != nullptr ? def->hardness : 0.5;
            if (rng_.Chance(tune.Get("strike.hit_chance"))) {
                const double x = a.strike_power * (1.0 - 0.7 * hard);
                AddDamage(ob.first, x);
                hit = true;
                hit_something_ = true;
                Log(r, a.target_id, "bombs: " + (def != nullptr ? def->name : ob.first) + " hit");
            }
        }
        if (!hit) Log(r, a.target_id, "bombs fall on empty ground");
    }
};

}  // namespace airwar
}  // namespace simcore
