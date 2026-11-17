// simcore/air_war/perception_feed.hpp — what the simulation made that a pilot may see (roadmap B3, 03_PERCEPTION.md 5.2).
//
// The runner records a visible event beside each place that makes one: a launch's dust and flash and its smoke trail,
// a burst of flak, tracer from a gun, a fire. Nothing here reads the fx feed (fx/events.hpp: the simulation never reads
// it); these come from the simulation's own sources, and the serial of each is given in the order the simulation makes
// them, which is deterministic. The hub (perception.hpp) turns them into sightings, observer by observer.
#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "simcore/air_war/perception.hpp"

namespace simcore {
namespace airwar {

class PerceptionFeed {
public:
    PerceptionFeed(const PerceptionData& d, std::uint64_t seed) : hub(d, seed) {}

    PerceptionHub hub;

    // A launch at `site` (the ground under the launcher), launch number `launch_id` of `owner`: the sign (dust and smoke,
    // or by night a flash) and the trail the missile will lay (extended by TrailHead or a synthetic flight). `night`: the
    // sign is a flash of a second or two.
    // `what`: the class the eye gives it by its size and smoke (roadmap B4): "SAM" (the Osa's size), "large SAM",
    // "shoulder SAM".
    void Launch(int launch_id, const std::string& owner, const Vec3& site, double now_s, bool night, const std::string& what = "SAM") {
        const PerceptionData& d = hub.Data();
        VisibleEvent sign;
        sign.kind = SightKind::LaunchSign;
        sign.what = what + " launch";
        sign.owner = owner;
        sign.pos = site;
        sign.start_s = now_s;
        sign.until_s = now_s + (night ? 2.0 : d.launch_sign_life_s);
        sign.ground = true;
        if (const GroundClassDef* c = d.Class("launch_sign")) {
            sign.pattern = c->pattern;
            sign.ground_contrast = c->contrast;
            sign.area_m2 = c->area_m2;
        }
        sign.shows_at_night = true;
        sign.night_range_m = d.launch_flash_night_range_m;
        sign.launch_id = owner == "own" ? launch_id : 0;  // only our own vehicle's launches are counted for the enemy's report
        hub.AddEvent(sign);
        VisibleEvent trail;
        trail.kind = SightKind::Trail;
        trail.what = what + " trail";
        trail.owner = owner;
        trail.pos = site;
        trail.start_s = now_s;
        trail.until_s = now_s + 1.0;
        trail.contrast_pct = d.trail.contrast_pct;
        trail.width_m = d.trail.width_m;
        trail.hang_s = kTrailHangS;
        trail.night_range_m = d.trail.tracer_night_range_m;
        trail.launch_id = owner == "own" ? launch_id : 0;
        const std::uint64_t serial = hub.AddEvent(trail);
        flown_[Key(owner, launch_id)] = Flown{serial, now_s, site};
        hub.ExtendTrail(serial, site, now_s, true);
    }

    // The missile's place now (the simulation flies it): lays the trail's next point while the motor burns and after.
    void TrailHead(const std::string& owner, int launch_id, const Vec3& head, double now_s) {
        const auto it = flown_.find(Key(owner, launch_id));
        if (it == flown_.end()) return;
        hub.ExtendTrail(it->second.serial, head, now_s, now_s - it->second.t0_s < kBurnS);
    }

    // A missile the simulation does not fly (a neighbour's, the testbed's): its trail runs from the site toward the place
    // its target was at `speed_mps` for `flight_s`, advanced by Update.
    void Synthetic(const std::string& owner, int launch_id, const Vec3& toward, double flight_s, double speed_mps) {
        const auto it = flown_.find(Key(owner, launch_id));
        if (it == flown_.end()) return;
        Synth s;
        s.key = it->first;
        s.from = it->second.site;
        s.dir = (toward - it->second.site).LengthSquared() > 1.0 ? (toward - it->second.site).Normalized() : Vec3{0.0, 0.0, 1.0};
        s.t0_s = it->second.t0_s;
        s.flight_s = flight_s;
        s.speed_mps = speed_mps;
        synth_.push_back(s);
    }

    // Advance the synthetic trails to `now_s` and drop what is over.
    void Update(double now_s) {
        for (std::size_t i = 0; i < synth_.size();) {
            Synth& s = synth_[i];
            const auto it = flown_.find(s.key);
            const double t = std::min(now_s - s.t0_s, s.flight_s);
            if (it != flown_.end()) {
                // The motor's climb out of the launcher (27 degrees: the 9M33's), then along the line to the target.
                const Vec3 head = s.from + s.dir * (s.speed_mps * t);
                hub.ExtendTrail(it->second.serial, head, now_s, now_s - s.t0_s < kBurnS && now_s - s.t0_s <= s.flight_s);
            }
            if (now_s - s.t0_s > s.flight_s) {
                synth_.erase(synth_.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
        hub.Prune(now_s);
    }

    // A flak burst: a puff by day, a flash by night.
    void Burst(const Vec3& pos, double now_s, bool night) {
        const PerceptionData& d = hub.Data();
        VisibleEvent e;
        e.kind = SightKind::Burst;
        e.what = "flak burst";
        e.pos = pos;
        e.start_s = now_s;
        e.until_s = now_s + (night ? d.burst.flash_life_s : d.burst.life_s);
        e.area_m2 = night ? d.burst.flash_area_m2 : d.burst.area_m2;
        e.contrast_pct = night ? d.burst.flash_contrast_pct : d.burst.contrast_pct;
        e.shows_at_night = true;
        e.night_range_m = d.burst.night_range_m;
        hub.AddEvent(e);
    }

    // Tracer from a gun site (at most one record each half second per site).
    void Tracer(const std::string& site_key, const Vec3& pos, double now_s) {
        double& last = tracer_last_[site_key];
        if (now_s - last < 0.5 && last > 0.0) return;
        last = now_s;
        const PerceptionData& d = hub.Data();
        VisibleEvent e;
        e.kind = SightKind::Tracer;
        e.what = "tracer";
        e.owner = site_key;
        e.pos = pos;
        e.start_s = now_s;
        e.until_s = now_s + d.tracer.life_s;
        e.area_m2 = d.tracer.area_m2;
        e.contrast_pct = d.tracer.contrast_pct;
        e.shows_at_night = true;
        e.night_range_m = d.tracer.night_range_m;
        hub.AddEvent(e);
    }

    // The 9M33's motor burns 15 s (a 2 s boost and 13 s sustain: data/scenery/munitions.json), and its smoke hangs 40 s.
    static constexpr double kBurnS = 15.0;
    static constexpr double kTrailHangS = 40.0;

private:
    struct Flown {
        std::uint64_t serial = 0;
        double t0_s = 0.0;
        Vec3 site;
    };
    struct Synth {
        std::string key;
        Vec3 from;
        Vec3 dir;
        double t0_s = 0.0;
        double flight_s = 0.0;
        double speed_mps = 0.0;
    };
    static std::string Key(const std::string& owner, int launch_id) { return owner + "#" + std::to_string(launch_id); }

    std::map<std::string, Flown> flown_;
    std::vector<Synth> synth_;
    std::map<std::string, double> tracer_last_;
};

}  // namespace airwar
}  // namespace simcore
