// simcore/air_war/types.hpp — the enemy air command's shared vocabulary (docs/air_war/).
//
// The enemy air command plans and flies the other side's air war at campaign scale: officers with characters plan raids
// on what their intelligence believes, the raids are flown (as scenarios the player plays, or resolved abstractly), and
// the staff learns from what it believes happened. This header holds what every part shares: the kinds of things on
// the map, mission and flight roles, the campaign clock, and a small portable random generator (every draw comes from
// the campaign seed, so a campaign replays exactly).
//
// Positions are in the theatre's own ENU frame (metres, x east, y north) at the theatre origin; heights are separate.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// The four kinds of adversary (docs/research/ENEMY_AIR_RESEARCH_2026-10.md §2).
enum class Tier { Inferior, NearPeer, Peer, Superior };

inline const char* TierName(Tier t) {
    switch (t) {
        case Tier::Inferior: return "inferior";
        case Tier::NearPeer: return "near_peer";
        case Tier::Peer: return "peer";
        case Tier::Superior: return "superior";
    }
    return "?";
}

inline bool ParseTier(const std::string& s, Tier& out) {
    for (Tier t : {Tier::Inferior, Tier::NearPeer, Tier::Peer, Tier::Superior}) {
        if (s == TierName(t)) {
            out = t;
            return true;
        }
    }
    return false;
}

// What a friendly (the player's side's) object is, as a target.
enum class ObjectKind {
    SamSite,      // a SAM battery or site (the player's Osa battery is one)
    Aaa,          // guns around a target
    EwRadar,      // an early-warning radar (P-19, P-40): feeds the PU-12 (c2.regiment) and the warning nets
    CommandPost,  // a command post (brigade, division)
    Airfield,
    Bridge,
    RailJunction,
    Depot,
    PowerPlant,
    Headquarters,
    Troops,  // a formation in the field near the front
    Factory,
    City,  // protected by politics more often than by guns
    // The nation's infrastructure (the nation on the map, N0; docs/campaign/10_BRIEFING_FORCES_AND_NATION.md §3).
    Refinery,         // crude oil into fuel: the fuel system's heart
    PipelineStation,  // a pipeline's border or pumping station: where the oil comes in
    Substation,       // the grid between the power stations and the consumers (power, like PowerPlant)
    Dam,              // a hydroelectric dam (power, like PowerPlant)
    MaterialsDepot,   // spans, pontoons, rails, cable, spares: what rebuilds need (docs/campaign/02_WHAT_WE_DEFEND.md §3)
    Port,             // a river port
    Government,       // the ministries, the General Staff
    Broadcast,        // state radio and television
    Exchange,         // a telephone exchange or trunk node: the wire the command pipeline runs on
    Count
};

inline const char* ObjectKindName(ObjectKind k) {
    switch (k) {
        case ObjectKind::SamSite: return "sam_site";
        case ObjectKind::Aaa: return "aaa";
        case ObjectKind::EwRadar: return "ew_radar";
        case ObjectKind::CommandPost: return "command_post";
        case ObjectKind::Airfield: return "airfield";
        case ObjectKind::Bridge: return "bridge";
        case ObjectKind::RailJunction: return "rail_junction";
        case ObjectKind::Depot: return "depot";
        case ObjectKind::PowerPlant: return "power_plant";
        case ObjectKind::Headquarters: return "headquarters";
        case ObjectKind::Troops: return "troops";
        case ObjectKind::Factory: return "factory";
        case ObjectKind::City: return "city";
        case ObjectKind::Refinery: return "refinery";
        case ObjectKind::PipelineStation: return "pipeline_station";
        case ObjectKind::Substation: return "substation";
        case ObjectKind::Dam: return "dam";
        case ObjectKind::MaterialsDepot: return "materials_depot";
        case ObjectKind::Port: return "port";
        case ObjectKind::Government: return "government";
        case ObjectKind::Broadcast: return "broadcast";
        case ObjectKind::Exchange: return "exchange";
        case ObjectKind::Count: break;
    }
    return "?";
}

inline bool ParseObjectKind(const std::string& s, ObjectKind& out) {
    for (int i = 0; i < static_cast<int>(ObjectKind::Count); ++i) {
        const ObjectKind k = static_cast<ObjectKind>(i);
        if (s == ObjectKindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

inline bool IsAirDefence(ObjectKind k) { return k == ObjectKind::SamSite || k == ObjectKind::Aaa; }

// What a package is for. The doctrine's apportionment shares sorties between these.
enum class MissionKind {
    Strike,      // interdiction or counter-air on a fixed target (bridge, depot, airfield, HQ, troops)
    Dead,        // destruction of a located air-defence site (or EW radar)
    Hunter,      // armed suppression patrol: hunter-killer teams, "the hunt" over a suspected battery; fires on emitters
    Recce,       // find: photograph a corridor or an area
    Bda,         // look again at what was struck
    Cas,         // support of the own troops near the line, on call
    Harassment,  // a night nuisance raid by a few light aircraft
    HeloRaid,    // attack or armed utility helicopters near the line, popping up from cover
    Deception,   // a decoy or demonstration wave to make the defences radiate and fire
    Count
};

inline const char* MissionKindName(MissionKind m) {
    switch (m) {
        case MissionKind::Strike: return "strike";
        case MissionKind::Dead: return "dead";
        case MissionKind::Hunter: return "hunter";
        case MissionKind::Recce: return "recce";
        case MissionKind::Bda: return "bda";
        case MissionKind::Cas: return "cas";
        case MissionKind::Harassment: return "harassment";
        case MissionKind::HeloRaid: return "helo_raid";
        case MissionKind::Deception: return "deception";
        case MissionKind::Count: break;
    }
    return "?";
}

inline bool ParseMissionKind(const std::string& s, MissionKind& out) {
    for (int i = 0; i < static_cast<int>(MissionKind::Count); ++i) {
        const MissionKind m = static_cast<MissionKind>(i);
        if (s == MissionKindName(m)) {
            out = m;
            return true;
        }
    }
    return false;
}

constexpr int kMissionKinds = static_cast<int>(MissionKind::Count);

// What one flight of a package does (the Soviet "tactical groups" and NATO's package roles: mai_tactics_vvs §3.3.3,
// afmag_1989_slam).
enum class FlightRole {
    Strike,      // the strike group: bombs or rockets on the target
    Dead,        // bombs on an air-defence site
    Sead,        // ARMs with the package: fires on what radiates
    Hunter,      // the hunter of a hunter-killer team (finds and fires on emitters)
    Escort,      // fighter escort / clearing the airspace
    Jammer,      // stand-off or escort jamming
    Recce,       // reconnaissance (and additional reconnaissance and marking)
    Decoy,       // decoy drones or a demonstration group
    Helicopter,  // armed helicopters
    Count
};

inline const char* FlightRoleName(FlightRole r) {
    switch (r) {
        case FlightRole::Strike: return "strike";
        case FlightRole::Dead: return "dead";
        case FlightRole::Sead: return "sead";
        case FlightRole::Hunter: return "hunter";
        case FlightRole::Escort: return "escort";
        case FlightRole::Jammer: return "jammer";
        case FlightRole::Recce: return "recce";
        case FlightRole::Decoy: return "decoy";
        case FlightRole::Helicopter: return "helicopter";
        case FlightRole::Count: break;
    }
    return "?";
}

inline bool ParseFlightRole(const std::string& s, FlightRole& out) {
    for (int i = 0; i < static_cast<int>(FlightRole::Count); ++i) {
        const FlightRole r = static_cast<FlightRole>(i);
        if (s == FlightRoleName(r)) {
            out = r;
            return true;
        }
    }
    return false;
}

// How a flight crosses the defended area.
enum class Profile { Low, Medium, High };

inline const char* ProfileName(Profile p) {
    switch (p) {
        case Profile::Low: return "low";
        case Profile::Medium: return "medium";
        case Profile::High: return "high";
    }
    return "?";
}

// Time-of-day buckets the planners think in.
enum class DayPart { Night, Dawn, Day, Dusk };
constexpr int kDayParts = 4;

inline const char* DayPartName(DayPart d) {
    switch (d) {
        case DayPart::Night: return "night";
        case DayPart::Dawn: return "dawn";
        case DayPart::Day: return "day";
        case DayPart::Dusk: return "dusk";
    }
    return "?";
}

inline bool ParseDayPart(const std::string& s, DayPart& out) {
    for (DayPart d : {DayPart::Night, DayPart::Dawn, DayPart::Day, DayPart::Dusk}) {
        if (s == DayPartName(d)) {
            out = d;
            return true;
        }
    }
    return false;
}

// Local hour 0-24 -> bucket: night 21-04, dawn 04-07, day 07-18, dusk 18-21 (sam3d).
inline DayPart DayPartAt(double local_hour) {
    double h = std::fmod(local_hour, 24.0);
    if (h < 0.0) h += 24.0;
    if (h < 4.0 || h >= 21.0) return DayPart::Night;
    if (h < 7.0) return DayPart::Dawn;
    if (h < 18.0) return DayPart::Day;
    return DayPart::Dusk;
}

// The hour a planner aims at inside a bucket (its middle).
inline double DayPartHour(DayPart d) {
    switch (d) {
        case DayPart::Night: return 1.0;
        case DayPart::Dawn: return 5.5;
        case DayPart::Day: return 12.0;
        case DayPart::Dusk: return 19.5;
    }
    return 12.0;
}

enum class Weather { Clear, Cloudy, Poor };

inline const char* WeatherName(Weather w) {
    switch (w) {
        case Weather::Clear: return "clear";
        case Weather::Cloudy: return "cloudy";
        case Weather::Poor: return "poor";
    }
    return "?";
}

// Where an item of the enemy's knowledge came from (docs/air_war/03_INTELLIGENCE.md).
enum class IntelSource { None, Satellite, AirRecce, Elint, Comint, Humint, PilotReport, LaunchSighting, Prior };

inline const char* IntelSourceName(IntelSource s) {
    switch (s) {
        case IntelSource::None: return "none";
        case IntelSource::Satellite: return "satellite";
        case IntelSource::AirRecce: return "air reconnaissance";
        case IntelSource::Elint: return "ELINT";
        case IntelSource::Comint: return "radio intercept";
        case IntelSource::Humint: return "agents";
        case IntelSource::PilotReport: return "pilot reports";
        case IntelSource::LaunchSighting: return "launch sighting";
        case IntelSource::Prior: return "pre-war survey";
    }
    return "?";
}

// A portable, sequential random generator: SplitMix64 (Steele, Lea & Flood, OOPSLA 2014). No std:: distributions,
// whose output differs between standard libraries.
class Rng {
public:
    explicit Rng(std::uint64_t seed = 1) : state_(seed) {}

    std::uint64_t Next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    // [0, 1)
    double Uniform() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }
    double Uniform(double lo, double hi) { return lo + (hi - lo) * Uniform(); }
    bool Chance(double p) { return Uniform() < p; }
    // Standard normal (Box-Muller).
    double Normal() {
        double u = Uniform();
        if (u < 1e-12) u = 1e-12;
        const double v = Uniform();
        return std::sqrt(-2.0 * std::log(u)) * std::cos(6.283185307179586 * v);
    }
    double Normal(double mean, double sd) { return mean + sd * Normal(); }
    // 0 .. n-1
    int Index(int n) { return n <= 1 ? 0 : static_cast<int>(Next() % static_cast<std::uint64_t>(n)); }
    // An index by weight (non-negative weights; all zero: uniform).
    int Weighted(const std::vector<double>& w) {
        double total = 0.0;
        for (double x : w) total += x > 0.0 ? x : 0.0;
        if (total <= 0.0) return Index(static_cast<int>(w.size()));
        double r = Uniform() * total;
        for (std::size_t i = 0; i < w.size(); ++i) {
            const double x = w[i] > 0.0 ? w[i] : 0.0;
            if (r < x) return static_cast<int>(i);
            r -= x;
        }
        return static_cast<int>(w.size()) - 1;
    }
    // A child generator for an independent stream (e.g. one per officer), stable whatever is drawn elsewhere.
    Rng Fork(std::uint64_t key) const {
        Rng r(state_ ^ (key * 0xD1B54A32D192ED03ull + 0x632BE59BD9B4E019ull));
        r.Next();
        return r;
    }
    std::uint64_t State() const { return state_; }

private:
    std::uint64_t state_;
};

inline double Clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
inline double ClampTo(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double Lerp(double a, double b, double t) { return a + (b - a) * t; }

// Horizontal distance (the z of theatre positions is ignored).
inline double Dist2d(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.y - b.y); }

// Distance from p to the segment a-b in the horizontal, and where along it (0..1) the closest point is.
inline double SegmentDistance2d(const Vec3& p, const Vec3& a, const Vec3& b, double* along = nullptr) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 0.0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0;
    t = Clamp01(t);
    if (along != nullptr) *along = t;
    return std::hypot(a.x + dx * t - p.x, a.y + dy * t - p.y);
}

// Length of the part of segment a-b inside a circle (centre c, radius r), horizontal.
inline double ChordInCircle2d(const Vec3& a, const Vec3& b, const Vec3& c, double r) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double fx = a.x - c.x, fy = a.y - c.y;
    const double qa = dx * dx + dy * dy;
    if (qa <= 0.0) return 0.0;
    const double qb = 2.0 * (fx * dx + fy * dy);
    const double qc = fx * fx + fy * fy - r * r;
    const double disc = qb * qb - 4.0 * qa * qc;
    if (disc <= 0.0) return 0.0;
    const double s = std::sqrt(disc);
    double t0 = (-qb - s) / (2.0 * qa);
    double t1 = (-qb + s) / (2.0 * qa);
    t0 = Clamp01(t0);
    t1 = Clamp01(t1);
    return t1 > t0 ? (t1 - t0) * std::sqrt(qa) : 0.0;
}

inline std::string FormatHour(double campaign_h, double start_hour) {
    const double abs_h = start_hour + campaign_h;
    const int day = static_cast<int>(std::floor(abs_h / 24.0)) + 1;
    double h = std::fmod(abs_h, 24.0);
    if (h < 0.0) h += 24.0;
    const int hh = static_cast<int>(std::floor(h));
    const int mm = static_cast<int>(std::floor((h - hh) * 60.0 + 1e-6));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "D%d %02d:%02d", day, hh, mm);
    return buf;
}

}  // namespace airwar
}  // namespace simcore
