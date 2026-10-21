// simcore/world.hpp — the simulation world: entities, fixed-step update, events.
//
// The engine (Unreal) owns one World, calls Step() at a fixed rate, and reads
// entity state back for rendering. Nothing in here knows about rendering.
#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "simcore/aircraft.hpp"
#include "simcore/geo.hpp"
#include "simcore/missile.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"
#include "simcore/weather.hpp"

namespace simcore {

enum class EventType {
    MissileLaunched,
    MissileDetonated,
    MissileMissed,
    MissileSelfDestructed,
    MissileHitGround,
    AircraftDestroyed
};

struct SimEvent {
    EventType type = EventType::MissileLaunched;
    double time_s = 0.0;
    int missile_id = 0;
    int aircraft_id = 0;
    double miss_distance_m = 0.0;
};

class World {
public:
    // `terrain` may be null (flat ground at 0 m). If given, it must outlive the World.
    explicit World(const GeoPoint& origin, const Terrain* terrain = nullptr)
        : frame_(origin), terrain_(terrain) {}

    const LocalFrame& Frame() const { return frame_; }
    const Terrain& GetTerrain() const { return terrain_ != nullptr ? *terrain_ : flat_; }

    // Swap the terrain (e.g. once Cesium heights have been sampled). Null = flat ground at 0 m.
    // It must outlive the World or be replaced before it goes away.
    void SetTerrain(const Terrain* terrain) { terrain_ = terrain; }

    // The weather the aircraft fly in (their wind) and the sensors look through (its cloud). Null: still, clear air.
    // It must outlive the World or be replaced before it goes away.
    void SetWeather(const Weather* weather) { weather_ = weather; }
    const Weather& GetWeather() const { return weather_ != nullptr ? *weather_ : still_; }
    double Time() const { return time_s_; }

    int AddAircraft(Aircraft a) {
        a.id = next_id_++;
        if (a.velocity.LengthSquared() == 0.0 && !a.waypoints.empty()) {
            a.velocity = (a.waypoints.front() - a.position).Normalized() * a.speed_mps;
        }
        aircraft_.push_back(std::move(a));
        return aircraft_.back().id;
    }

    // Launch a missile from `launch_pos` along azimuth/elevation toward `target_id`.
    int LaunchMissile(const MissileParams& params, const Vec3& launch_pos, double azimuth_rad,
                      double elevation_rad, int target_id) {
        Missile m;
        m.id = next_id_++;
        m.target_id = target_id;
        m.params = params;
        m.position = launch_pos;
        m.velocity = DirectionFromAzEl(azimuth_rad, elevation_rad) * params.launch_speed_mps;
        missiles_.push_back(m);
        events_.push_back({EventType::MissileLaunched, time_s_, m.id, target_id, 0.0});
        return m.id;
    }

    void Step(double dt) {
        // Remember target positions at the start of the step for fuzing.
        start_positions_.clear();
        for (const Aircraft& a : aircraft_) start_positions_.push_back(a.position);

        for (Aircraft& a : aircraft_) {
            if (weather_ == nullptr || !a.alive) {
                StepAircraft(a, dt);
                continue;
            }
            const double alt = frame_.AltitudeOf(a.position);
            StepAircraft(a, dt, weather_->WindAt(alt, alt - GetTerrain().HeightAt(a.position.x, a.position.y)));
        }

        for (Missile& m : missiles_) {
            if (!m.Active()) continue;
            const std::size_t ti = IndexOfAircraft(m.target_id);
            if (ti == kNotFound || !aircraft_[ti].alive) {
                m.outcome = MissileOutcome::SelfDestructed;
                events_.push_back({EventType::MissileSelfDestructed, time_s_ + dt, m.id, m.target_id, 0.0});
                continue;
            }
            const Aircraft& tgt = aircraft_[ti];
            StepMissile(m, start_positions_[ti], tgt.position, tgt.velocity, frame_.AltitudeOf(m.position), dt);
            if (m.Active() && frame_.AltitudeOf(m.position) < GetTerrain().HeightAt(m.position.x, m.position.y)) {
                m.outcome = MissileOutcome::HitGround;
            }
            RecordOutcome(m, ti, time_s_ + dt);
        }
        time_s_ += dt;
    }

    std::vector<Aircraft>& AircraftList() { return aircraft_; }
    const std::vector<Aircraft>& AircraftList() const { return aircraft_; }
    const std::vector<Missile>& MissileList() const { return missiles_; }

    Aircraft* FindAircraft(int id) {
        const std::size_t i = IndexOfAircraft(id);
        return i == kNotFound ? nullptr : &aircraft_[i];
    }
    const Aircraft* FindAircraft(int id) const {
        const std::size_t i = IndexOfAircraft(id);
        return i == kNotFound ? nullptr : &aircraft_[i];
    }
    const Missile* FindMissile(int id) const {
        for (const Missile& m : missiles_) if (m.id == id) return &m;
        return nullptr;
    }

    const std::vector<SimEvent>& Events() const { return events_; }
    std::vector<SimEvent> DrainEvents() {
        std::vector<SimEvent> out;
        out.swap(events_);
        return out;
    }

private:
    static constexpr std::size_t kNotFound = static_cast<std::size_t>(-1);

    std::size_t IndexOfAircraft(int id) const {
        for (std::size_t i = 0; i < aircraft_.size(); ++i) if (aircraft_[i].id == id) return i;
        return kNotFound;
    }

    void RecordOutcome(const Missile& m, std::size_t target_index, double t) {
        switch (m.outcome) {
            case MissileOutcome::InFlight:
                return;
            case MissileOutcome::Detonated: {
                events_.push_back({EventType::MissileDetonated, t, m.id, m.target_id, m.closest_approach_m});
                Aircraft& tgt = aircraft_[target_index];
                if (tgt.alive && m.closest_approach_m <= m.params.lethal_radius_m) {
                    tgt.alive = false;
                    events_.push_back({EventType::AircraftDestroyed, t, m.id, tgt.id, m.closest_approach_m});
                }
                return;
            }
            case MissileOutcome::Missed:
                events_.push_back({EventType::MissileMissed, t, m.id, m.target_id, m.closest_approach_m});
                return;
            case MissileOutcome::SelfDestructed:
                events_.push_back({EventType::MissileSelfDestructed, t, m.id, m.target_id, m.closest_approach_m});
                return;
            case MissileOutcome::HitGround:
                events_.push_back({EventType::MissileHitGround, t, m.id, m.target_id, m.closest_approach_m});
                return;
        }
    }

    LocalFrame frame_;
    const Terrain* terrain_;
    FlatTerrain flat_{0.0};
    const Weather* weather_ = nullptr;
    Weather still_;
    double time_s_ = 0.0;
    int next_id_ = 1;
    std::vector<Aircraft> aircraft_;
    std::vector<Missile> missiles_;
    std::vector<Vec3> start_positions_;
    std::vector<SimEvent> events_;
};

}  // namespace simcore
