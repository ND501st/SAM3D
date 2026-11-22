// simcore/damage.hpp — what a burst or a hit does to an aircraft (roadmap R1, docs/DAMAGE.md).
//
// A gameplay-level model drawn by chance, not a physics of fragments and structures (the roadmap's decision on how far
// the physics goes). An aircraft is undamaged, lightly damaged (largely unaffected), moderately damaged (it may trail
// smoke, jettison its tanks or its load, break away and abandon its mission), heavily damaged (it trails smoke and may
// burn; it crashes 10 s to 5 min later unless it limps home), or destroyed outright.
//
// Two ways to be struck:
//   * A burst near it (a time-fuzed shell, a missile's warhead) at a miss distance d. Inside the blast's reach
//     (k W^(1/3), W the charge's TNT kg) it is destroyed or heavily damaged. Beyond, its fragments: N of them, spread
//     over the sphere, strike it Poisson(lambda) times, lambda = N A / (4 pi d^2) exp(-d / R), A its presented area and R
//     the fragments' reach (they slow and fall away: R grows with W^(1/3)). Most bursts 30-50 m off do nothing.
//   * A contact round that hits it (the 23, 37 and 57 mm guns' rounds burst only on what they strike): each hit by its
//     calibre's chances.
// Each strike is critical (engine, fuel, controls, crew: heavy, or destroyed outright), serious (moderate) or light.
// Light hits add up (three make a moderate one) and moderate ones too (two make heavy): the owner's rule (2 Nov 2026).
// Every number is in data/gameplay/damage.json with its source.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"

namespace simcore {

enum class DamageLevel { None, Light, Moderate, Heavy, Destroyed };

inline const char* ToString(DamageLevel l) {
    switch (l) {
        case DamageLevel::None: return "none";
        case DamageLevel::Light: return "light";
        case DamageLevel::Moderate: return "moderate";
        case DamageLevel::Heavy: return "heavy";
        case DamageLevel::Destroyed: return "destroyed";
    }
    return "?";
}

// A contact round's hit by its calibre: the chance it is critical, and serious.
struct DamageRoundHit {
    double calibre_mm = 0.0;
    double critical = 0.0;
    double serious = 0.0;
};

struct DamageTable {
    // A burst's fragments.
    double fragments_per_kg = 70.0;        // lethal fragments per kg of a shell's or warhead's casing (its mass less its charge)
    double fragment_reach_m = 26.0;        // R per cube root of the charge's kg
    double fragment_critical = 0.03;       // a fragment's strike: critical
    double fragment_serious = 0.08;        // serious (else light)
    double blast_destroy_m = 2.0;          // per cube root of the charge's kg: destroyed outright inside
    double blast_heavy_m = 3.5;            // heavily damaged inside
    double area_per_size2 = 0.2;           // its presented area per square of its size (ScenarioTarget::size_m)
    std::vector<DamageRoundHit> rounds;    // a contact round's hit, by calibre (ascending; between two, interpolated)
    // Adding up.
    int lights_per_moderate = 3;
    int moderates_per_heavy = 2;
    double critical_destroys = 0.4;        // a critical strike destroys it outright (else heavy)
    // What follows.
    double heavy_crash = 0.85;             // a heavily damaged aircraft crashes (else it limps home)
    double crash_min_s = 10.0;
    double crash_max_s = 300.0;
    double heavy_fire = 0.4;
    double heavy_jettison = 0.9;
    double moderate_smoke = 0.6;
    double moderate_jettison = 0.5;
    double moderate_abort = 0.6;
    // A missile whose kill a calibrated chance decides (the Osa's resolver, the raids' per pass): a kill is heavy
    // damage this share of the time (else destroyed outright); a miss bursts near enough to strike it this share of the
    // time, missile_near_min_m..max_m off, and does at most moderate damage (the chance already counts every aircraft
    // brought down).
    double missile_heavy_share = 0.25;
    double missile_near_burst = 0.5;
    double missile_near_min_m = 15.0;
    double missile_near_max_m = 60.0;
    double missile_fragments_per_kg = 50.0;  // a missile warhead's lethal fragments per kg of its charge
    // What the damage leaves him able to do (roadmap S4; optional keys, 1 and 0 when absent: as before): his g limit and
    // his engine's thrust times these at moderate and heavy damage (DamagedLimits); a moderate or heavy hit holes his
    // fuel this share of the time, and then he has fuel_leak_min_s..max_s left (it crashes then if still flying in the
    // session); a damaged aircraft turning for home has a wingman sent with it as escort escort_share of the time.
    double moderate_g_factor = 1.0;
    double heavy_g_factor = 1.0;
    double moderate_thrust_factor = 1.0;
    double heavy_thrust_factor = 1.0;
    double moderate_fuel_leak = 0.0;
    double heavy_fuel_leak = 0.0;
    double fuel_leak_min_s = 300.0;
    double fuel_leak_max_s = 1200.0;
    double escort_share = 0.0;
};

// The limits a damaged aircraft flies by (roadmap S4): its g limit and thrust (and with the thrust its top speed, the drag
// rising with the square of the speed) at its level, from its type's undamaged ones.
struct DamagedLimitsOut {
    double g = 1.0;       // times the g limit (and the turn rates)
    double thrust = 1.0;  // times the acceleration
    double top = 1.0;     // times the top speed: sqrt(thrust)
};
inline DamagedLimitsOut DamagedLimits(const DamageTable& t, DamageLevel level) {
    DamagedLimitsOut o;
    if (level == DamageLevel::Moderate) {
        o.g = t.moderate_g_factor;
        o.thrust = t.moderate_thrust_factor;
    } else if (level == DamageLevel::Heavy) {
        o.g = t.heavy_g_factor;
        o.thrust = t.heavy_thrust_factor;
    }
    o.top = std::sqrt(std::max(0.0, o.thrust));
    return o;
}

// One burst's or hits' strikes on an aircraft.
struct DamageDraw {
    int light = 0;
    int serious = 0;
    int critical = 0;       // of them, `destroying` destroy it outright
    int destroying = 0;
    bool blast_destroy = false;
    bool blast_heavy = false;
    bool Any() const { return light + serious + critical > 0 || blast_destroy || blast_heavy; }
};

// An aircraft's damage so far.
struct AircraftDamage {
    DamageLevel level = DamageLevel::None;
    int light = 0;           // light strikes not yet added up into a moderate one
    int moderate = 0;        // moderate damage (strikes and added-up lights)
    bool smoke = false;
    bool fire = false;
    bool jettison = false;   // it has thrown off its tanks and its load
    bool abort = false;      // it has abandoned its mission and turns for home
    double crash_at_s = -1.0;  // heavily damaged and going down then (-1: not)
    bool fuel_leak = false;  // its tanks holed (roadmap S4): it falls at crash_at_s if still flying
    std::string by;          // who did the damage that counts most (the last that raised its level)
};

// What a draw changed.
struct DamageChange {
    DamageLevel before = DamageLevel::None;
    DamageLevel after = DamageLevel::None;
    bool smoke = false, fire = false, jettison = false, abort = false;  // started now
    bool crashes = false;                                               // heavy now, and going down
    bool Raised() const { return after > before; }
};

namespace detail {

inline double Draw01(std::mt19937& rng) { return static_cast<double>(rng()) / 4294967296.0; }

inline bool ReadDataFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream text;
    text << file.rdbuf();
    out = text.str();
    return true;
}

// A standard normal draw (Box-Muller on two of the generator's draws: the same on every compiler, unlike
// std::normal_distribution).
inline double DrawNormal(std::mt19937& rng) {
    const double u1 = std::max(Draw01(rng), 1e-300);
    const double u2 = Draw01(rng);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
}

// A Poisson draw: by counting for a small mean, a rounded normal for a large one (portable, as above).
inline int DrawPoisson(double mean, std::mt19937& rng) {
    if (!(mean > 0.0)) return 0;
    if (mean > 40.0) return std::max(0, static_cast<int>(std::lround(mean + std::sqrt(mean) * DrawNormal(rng))));
    const double limit = std::exp(-mean);
    double p = 1.0;
    int k = -1;
    do {
        ++k;
        p *= Draw01(rng);
    } while (p > limit);
    return k;
}

}  // namespace detail

// The presented area of an aircraft of `size_m` (its ScenarioTarget::size_m), m^2.
inline double PresentedAreaM2(const DamageTable& t, double size_m) { return std::max(1.0, t.area_per_size2 * size_m * size_m); }

// How many lethal fragments a shell or warhead of `mass_kg` with `charge_kg` of TNT throws.
inline double BurstFragments(const DamageTable& t, double mass_kg, double charge_kg) {
    return std::max(0.0, t.fragments_per_kg * std::max(0.0, mass_kg - charge_kg));
}

// The mean number of fragments that strike an aircraft of `area_m2` from a burst `miss_m` off.
inline double ExpectedFragmentStrikes(const DamageTable& t, double fragments, double charge_kg, double miss_m, double area_m2) {
    const double d = std::max(miss_m, 0.5);
    const double reach = t.fragment_reach_m * std::cbrt(std::max(charge_kg, 1e-6));
    return fragments * area_m2 / (4.0 * 3.141592653589793 * d * d) * std::exp(-d / std::max(reach, 1e-6));
}

// A contact round's hit chances at `calibre_mm` (between the table's calibres, interpolated; beyond them, the end's).
inline DamageRoundHit RoundHitFor(const DamageTable& t, double calibre_mm) {
    if (t.rounds.empty()) return DamageRoundHit{calibre_mm, 0.0, 0.0};
    if (calibre_mm <= t.rounds.front().calibre_mm) return t.rounds.front();
    if (calibre_mm >= t.rounds.back().calibre_mm) return t.rounds.back();
    for (std::size_t i = 1; i < t.rounds.size(); ++i) {
        const DamageRoundHit& a = t.rounds[i - 1];
        const DamageRoundHit& b = t.rounds[i];
        if (calibre_mm > b.calibre_mm) continue;
        const double u = (calibre_mm - a.calibre_mm) / std::max(b.calibre_mm - a.calibre_mm, 1e-9);
        return DamageRoundHit{calibre_mm, a.critical + (b.critical - a.critical) * u, a.serious + (b.serious - a.serious) * u};
    }
    return t.rounds.back();
}

// Sorts `n` strikes into critical, serious and light by the chances, and the critical ones into destroying or not.
inline void SortStrikes(const DamageTable& t, int n, double critical, double serious, DamageDraw& d, std::mt19937& rng) {
    for (int i = 0; i < n; ++i) {
        const double u = detail::Draw01(rng);
        if (u < critical) {
            ++d.critical;
            if (detail::Draw01(rng) < t.critical_destroys) ++d.destroying;
        } else if (u < critical + serious) {
            ++d.serious;
        } else {
            ++d.light;
        }
    }
}

// A burst `miss_m` from an aircraft of `area_m2`: a shell or warhead of `mass_kg` with `charge_kg` of TNT, throwing
// `fragments` (<= 0: BurstFragments).
inline DamageDraw DrawBurst(const DamageTable& t, double mass_kg, double charge_kg, double fragments, double miss_m, double area_m2,
                            std::mt19937& rng) {
    DamageDraw d;
    const double w = std::cbrt(std::max(charge_kg, 1e-6));
    if (miss_m <= t.blast_destroy_m * w) {
        d.blast_destroy = true;
        return d;
    }
    if (miss_m <= t.blast_heavy_m * w) d.blast_heavy = true;
    const double n = fragments > 0.0 ? fragments : BurstFragments(t, mass_kg, charge_kg);
    SortStrikes(t, detail::DrawPoisson(ExpectedFragmentStrikes(t, n, charge_kg, miss_m, area_m2), rng), t.fragment_critical,
                t.fragment_serious, d, rng);
    return d;
}

// `hits` contact rounds of `calibre_mm` that struck it.
inline DamageDraw DrawRoundHits(const DamageTable& t, double calibre_mm, int hits, std::mt19937& rng) {
    DamageDraw d;
    const DamageRoundHit h = RoundHitFor(t, calibre_mm);
    SortStrikes(t, std::max(hits, 0), h.critical, h.serious, d, rng);
    return d;
}

// Adds a draw to an aircraft's damage at `now_s`; `cap` limits what this draw may raise it to (a calibrated miss:
// Moderate). The consequences (smoke, fire, jettison, abort, the crash) are drawn as the level is reached.
inline DamageChange ApplyDamage(const DamageTable& t, AircraftDamage& a, const DamageDraw& d, double now_s, std::mt19937& rng,
                                const std::string& by, DamageLevel cap = DamageLevel::Destroyed) {
    DamageChange c;
    c.before = a.level;
    if (a.level == DamageLevel::Destroyed || !d.Any()) {
        c.after = a.level;
        return c;
    }
    DamageLevel to = a.level;
    const auto raise = [&to](DamageLevel l) { to = std::max(to, l); };
    if (d.blast_destroy || d.destroying > 0) raise(DamageLevel::Destroyed);
    if (d.blast_heavy || d.critical > 0) raise(DamageLevel::Heavy);
    a.light += d.light;
    a.moderate += d.serious;
    if (t.lights_per_moderate > 0) {
        while (a.light >= t.lights_per_moderate) {
            a.light -= t.lights_per_moderate;
            ++a.moderate;
        }
    }
    if (d.light > 0) raise(DamageLevel::Light);
    if (a.moderate > 0) raise(DamageLevel::Moderate);
    if (t.moderates_per_heavy > 0 && a.moderate >= t.moderates_per_heavy) raise(DamageLevel::Heavy);
    to = std::min(to, std::max(cap, a.level));
    if (to > a.level) a.by = by;
    a.level = to;
    c.after = to;
    const auto starts = [&rng](bool& state, double chance) {
        if (state || !(detail::Draw01(rng) < chance)) return false;
        state = true;
        return true;
    };
    if (c.before < DamageLevel::Moderate && to == DamageLevel::Moderate) {
        c.smoke = starts(a.smoke, t.moderate_smoke);
        c.jettison = starts(a.jettison, t.moderate_jettison);
        c.abort = starts(a.abort, t.moderate_abort);
    }
    if (c.before < DamageLevel::Heavy && to == DamageLevel::Heavy) {
        c.smoke = starts(a.smoke, 1.0);
        c.fire = starts(a.fire, t.heavy_fire);
        c.jettison = starts(a.jettison, t.heavy_jettison);
        c.abort = starts(a.abort, 1.0);
        if (detail::Draw01(rng) < t.heavy_crash) {
            a.crash_at_s = now_s + t.crash_min_s + (t.crash_max_s - t.crash_min_s) * detail::Draw01(rng);
            c.crashes = true;
        }
    }
    return c;
}

// A missile whose kill a calibrated chance has decided (`kill`): destroyed outright, or heavily damaged
// (missile_heavy_share); a miss that bursts near (missile_near_burst) strikes it as a burst of the warhead
// with `charge_kg` of TNT (missile_fragments_per_kg of it) missile_near_min_m..max_m off, capped at moderate damage.
// `near_miss_m`, if given, gets how far off a miss burst (-1: it flew on to destroy itself far away, or it was a kill).
inline DamageChange ApplyMissile(const DamageTable& t, AircraftDamage& a, bool kill, double charge_kg, double area_m2, double now_s,
                                 std::mt19937& rng, const std::string& by, double* near_miss_m = nullptr) {
    if (near_miss_m != nullptr) *near_miss_m = -1.0;
    if (kill) {
        DamageDraw d;
        if (detail::Draw01(rng) < t.missile_heavy_share) {
            d.blast_heavy = true;
        } else {
            d.blast_destroy = true;
        }
        return ApplyDamage(t, a, d, now_s, rng, by);
    }
    if (!(detail::Draw01(rng) < t.missile_near_burst)) {
        DamageChange c;
        c.before = c.after = a.level;
        return c;
    }
    const double miss = t.missile_near_min_m + (t.missile_near_max_m - t.missile_near_min_m) * detail::Draw01(rng);
    if (near_miss_m != nullptr) *near_miss_m = miss;
    const DamageDraw d = DrawBurst(t, 0.0, charge_kg, t.missile_fragments_per_kg * charge_kg, miss, area_m2, rng);
    DamageDraw capped = d;
    capped.blast_destroy = capped.blast_heavy = false;
    capped.serious += capped.critical;  // a critical strike from a missile that missed counts as serious
    capped.critical = capped.destroying = 0;
    return ApplyDamage(t, a, capped, now_s, rng, by, DamageLevel::Moderate);
}

// --------------------------------------------------------------------------------------------- data/gameplay/damage.json

namespace detail {

// Reads {"name": {"value": n, "source": ..., "confidence": ...}} entries, each required, into doubles.
class SourcedValuesLoader : public ConsoleLoader {
public:
    bool Values(const json::Value& obj, const std::string& path, std::initializer_list<std::pair<const char*, double*>> each,
                bool positive_only = false) {
        for (const auto& e : each) {
            const std::string p = Join(path, e.first);
            const json::Value* v = obj.Find(e.first);
            if (v == nullptr) return Fail(p, "required");
            if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || n->number < 0.0 || (positive_only && !(n->number > 0.0))) {
                return Fail(Join(p, "value"), positive_only ? "a number > 0" : "a number >= 0");
            }
            *e.second = n->number;
        }
        return true;
    }
    using ConsoleLoader::Fail;
    using ConsoleLoader::Index;
    using ConsoleLoader::Join;
    using ConsoleLoader::Keys;
    using ConsoleLoader::Provenance;
};

class DamageLoader : public SourcedValuesLoader {
public:
    bool Load(const json::Value& root, DamageTable& out) {
        if (!Keys(root, "", {"damage_id", "status", "notes", "source", "confidence", "values", "round_hits"}) || !Provenance(root, "")) return false;
        const json::Value* v = root.Find("values");
        if (v == nullptr) return Fail("values", "required");
        static const char* const kNames[] = {"fragments_per_kg", "fragment_reach_m", "fragment_critical", "fragment_serious", "blast_destroy_m",
                                             "blast_heavy_m", "area_per_size2", "lights_per_moderate", "moderates_per_heavy",
                                             "critical_destroys", "heavy_crash", "crash_min_s", "crash_max_s", "heavy_fire", "heavy_jettison",
                                             "moderate_smoke", "moderate_jettison", "moderate_abort", "missile_heavy_share",
                                             "missile_near_burst", "missile_near_min_m", "missile_near_max_m", "missile_fragments_per_kg",
                                             "moderate_g_factor", "heavy_g_factor", "moderate_thrust_factor", "heavy_thrust_factor",
                                             "moderate_fuel_leak", "heavy_fuel_leak", "fuel_leak_min_s", "fuel_leak_max_s", "escort_share"};
        for (const std::string& k : v->keys) {
            bool known = false;
            for (const char* n : kNames) known = known || k == n;
            if (!known) return Fail(Join("values", k), "unknown key");
        }
        double lights = 3.0, moderates = 2.0;
        if (!Values(*v, "values", {{"fragments_per_kg", &out.fragments_per_kg}, {"fragment_reach_m", &out.fragment_reach_m},
                                   {"blast_destroy_m", &out.blast_destroy_m}, {"blast_heavy_m", &out.blast_heavy_m},
                                   {"area_per_size2", &out.area_per_size2}, {"lights_per_moderate", &lights},
                                   {"moderates_per_heavy", &moderates}, {"crash_min_s", &out.crash_min_s}, {"crash_max_s", &out.crash_max_s},
                                   {"missile_near_min_m", &out.missile_near_min_m}, {"missile_near_max_m", &out.missile_near_max_m},
                                   {"missile_fragments_per_kg", &out.missile_fragments_per_kg}},
                    true)) {
            return false;
        }
        if (!Values(*v, "values", {{"fragment_critical", &out.fragment_critical}, {"fragment_serious", &out.fragment_serious},
                                   {"critical_destroys", &out.critical_destroys}, {"heavy_crash", &out.heavy_crash}, {"heavy_fire", &out.heavy_fire},
                                   {"heavy_jettison", &out.heavy_jettison}, {"moderate_smoke", &out.moderate_smoke},
                                   {"moderate_jettison", &out.moderate_jettison}, {"moderate_abort", &out.moderate_abort},
                                   {"missile_heavy_share", &out.missile_heavy_share}, {"missile_near_burst", &out.missile_near_burst}})) {
            return false;
        }
        for (const std::pair<const char*, double> c : {std::make_pair("fragment_critical", out.fragment_critical + out.fragment_serious),
                                                       std::make_pair("critical_destroys", out.critical_destroys),
                                                       std::make_pair("heavy_crash", out.heavy_crash), std::make_pair("heavy_fire", out.heavy_fire),
                                                       std::make_pair("heavy_jettison", out.heavy_jettison),
                                                       std::make_pair("moderate_smoke", out.moderate_smoke),
                                                       std::make_pair("moderate_jettison", out.moderate_jettison),
                                                       std::make_pair("moderate_abort", out.moderate_abort),
                                                       std::make_pair("missile_heavy_share", out.missile_heavy_share),
                                                       std::make_pair("missile_near_burst", out.missile_near_burst)}) {
            if (c.second > 1.0) return Fail(Join("values", c.first), "a chance (with fragment_serious, at most 1)");
        }
        // Roadmap S4's, each optional (a table without them flies its damaged aircraft as before).
        for (const std::pair<const char*, double*> o :
             {std::make_pair("moderate_g_factor", &out.moderate_g_factor), std::make_pair("heavy_g_factor", &out.heavy_g_factor),
              std::make_pair("moderate_thrust_factor", &out.moderate_thrust_factor), std::make_pair("heavy_thrust_factor", &out.heavy_thrust_factor),
              std::make_pair("moderate_fuel_leak", &out.moderate_fuel_leak), std::make_pair("heavy_fuel_leak", &out.heavy_fuel_leak),
              std::make_pair("escort_share", &out.escort_share)}) {
            if (v->Find(o.first) == nullptr) continue;
            if (!Values(*v, "values", {{o.first, o.second}})) return false;
            if (*o.second > 1.0) return Fail(Join("values", o.first), "a share (at most 1)");
        }
        for (const std::pair<const char*, double*> o : {std::make_pair("fuel_leak_min_s", &out.fuel_leak_min_s), std::make_pair("fuel_leak_max_s", &out.fuel_leak_max_s)}) {
            if (v->Find(o.first) != nullptr && !Values(*v, "values", {{o.first, o.second}}, true)) return false;
        }
        if (out.fuel_leak_max_s < out.fuel_leak_min_s) return Fail("values.fuel_leak_max_s", "at least fuel_leak_min_s");
        if (out.heavy_g_factor <= 0.0 || out.moderate_g_factor <= 0.0) return Fail("values", "a g factor > 0");
        if (lights != std::floor(lights) || moderates != std::floor(moderates)) return Fail("values", "lights_per_moderate and moderates_per_heavy are counts");
        out.lights_per_moderate = static_cast<int>(lights);
        out.moderates_per_heavy = static_cast<int>(moderates);
        if (out.blast_heavy_m < out.blast_destroy_m) return Fail("values.blast_heavy_m", "at least blast_destroy_m");
        if (out.crash_max_s < out.crash_min_s) return Fail("values.crash_max_s", "at least crash_min_s");
        if (out.missile_near_max_m < out.missile_near_min_m) return Fail("values.missile_near_max_m", "at least missile_near_min_m");
        const json::Value* rs = root.Find("round_hits");
        if (rs == nullptr || !rs->Is(json::Type::Array) || rs->items.empty()) return Fail("round_hits", "required, a list by calibre");
        out.rounds.clear();
        for (std::size_t i = 0; i < rs->items.size(); ++i) {
            const std::string p = Index("round_hits", i);
            const json::Value& r = rs->items[i];
            if (!Keys(r, p, {"calibre_mm", "critical", "serious", "notes"})) return false;
            DamageRoundHit h;
            if (!Values(r, p, {{"calibre_mm", &h.calibre_mm}}, true) || !Values(r, p, {{"critical", &h.critical}, {"serious", &h.serious}})) return false;
            if (h.critical + h.serious > 1.0) return Fail(p, "critical and serious together at most 1");
            if (!out.rounds.empty() && h.calibre_mm <= out.rounds.back().calibre_mm) return Fail(Join(p, "calibre_mm"), "ascending");
            out.rounds.push_back(h);
        }
        return true;
    }
};

}  // namespace detail

// A file's path; false with the reason in `error`.
inline bool LoadDamageFile(const std::string& path, DamageTable& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadDataFile(path, text)) {
        if (error != nullptr) *error = "damage: cannot read " + path;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "damage: " + e;
        return false;
    }
    detail::DamageLoader loader;
    DamageTable t;
    if (!loader.Load(root, t)) {
        if (error != nullptr) *error = "damage: " + loader.Message();
        return false;
    }
    out = std::move(t);
    return true;
}

}  // namespace simcore
