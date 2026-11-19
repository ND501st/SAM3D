// simcore/air_war/test_waves_data.hpp — the dynamic campaign testbed's waves as data (docs/DYNAMIC_CAMPAIGN_TEST.md).
//
// A scenario's "waves": "gameplay/test_waves.json" keeps aircraft coming over it for as long as it runs: random waves
// of every adversary's aircraft (data/air_war/faction_*.json) and of our own side's, each flying a profile of its role,
// the strike aircraft releasing the weapons of their loads (data/air_war/weapons.json) as objects that fall or fly to
// where they were aimed. It is a testbed for the game's look, sound and play, not a model of a war: the enemy air
// command's planned raids are simcore/air_war/ (the campaign); these waves are random. This file holds the data and
// its loader (read by the scenario loader, scenario.hpp); the generator is test_waves.hpp.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/aaa.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/console_data.hpp"
#include "simcore/damage.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace airwar {

// How a kind of weapon of weapons.json is drawn and flown once released (game values).
struct WaveWeaponKind {
    std::string kind;          // bomb, retarded_bomb, cluster, runway, guided_bomb, missile, rockets, atgm, gun
    double release_m = 1500.0; // released this far (horizontally) from its aim point
    double scatter_m = 30.0;   // about the aim point
};

// One of our own side's aircraft types in the waves (game values: our side has no faction file).
struct WaveFriendlyType {
    std::string id;
    std::string name;
    std::string model;
    std::string target_class = "fighter";
    std::vector<std::string> roles;  // cap, intercept, ground_attack, transport, helicopter, trainer
    bool helicopter = false;
    double speed_mps = 220.0;
    double alt_m = 4000.0;     // above the ground (a helicopter's and a low flier's: its low height)
    double rcs_m2 = 3.0;
    int count_min = 1;
    int count_max = 2;
    std::string fit;  // its fit in air_combat.json for the air fight (S1; "": its id)
};

struct WavesDef {
    std::string id;
    TheatreDef theatre;                 // its fronts: where each adversary's waves come from
    WeaponsDef weapons;
    std::vector<FactionDef> factions;   // every adversary, every aircraft type of each in the waves
    std::vector<WaveFriendlyType> friendly;
    std::vector<std::string> friendly_callsigns;
    std::vector<WaveWeaponKind> weapon_kinds;
    // Our air defence round the testbed (its scenario's neighbours, by system): each system's figures as the enemy air
    // command's theatre has them (the first object of that system in "defences"), and the raids' tuning ("tuning"),
    // so a neighbour shoots on the pass as the raids' resolver has it (test_waves.hpp, WavePassKillChance).
    std::map<std::string, DefenceDef> defences;
    TuningTable tuning;
    // What their bursts and hits do ("damage": data/gameplay/damage.json) and how the guns fire by range ("aaa":
    // data/gameplay/aaa.json): roadmap R1 and R2. Without them a gun's pass is decided by the chance per pass too.
    bool has_damage = false;
    DamageTable damage;
    AaaTable aaa;
    // Timing.
    double first_wave_s = 45.0;
    double interval_min_s = 100.0;
    double interval_max_s = 200.0;
    int flights_min = 3;
    int flights_max = 6;
    int max_airborne = 60;      // no new wave while this many of its aircraft are in the sky or about to enter it
    double friendly_share = 0.35;
    double remove_after_s = 60.0;  // a wave's aircraft gone this long leaves the world (it no longer costs a frame)
    // Geometry (from the scenario's centre).
    double entry_min_m = 55000.0;
    double entry_max_m = 85000.0;
    double helicopter_min_m = 18000.0;
    double helicopter_max_m = 28000.0;
    double jammer_orbit_m = 45000.0;
    double support_orbit_m = 105000.0;
    double city_radius_m = 6000.0;

    const DefenceDef* Defence(const std::string& system) const {
        const auto it = defences.find(system);
        return it == defences.end() ? nullptr : &it->second;
    }

    const WaveWeaponKind* Kind(const std::string& kind) const {
        for (const WaveWeaponKind& k : weapon_kinds) {
            if (k.kind == kind) return &k;
        }
        return nullptr;
    }
};

namespace detail {

class WavesLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, const std::string& data_dir, WavesDef& out) {
        if (!Keys(root, "", {"waves_id", "status", "source", "confidence", "notes", "theatre", "weapons", "factions", "timing",
                             "geometry", "weapon_kinds", "friendly", "friendly_callsigns", "defences", "tuning", "damage", "aaa"}) ||
            !Provenance(root, "") || !RequireString(root, "", "waves_id", out.id)) {
            return false;
        }
        std::string theatre, weapons, e;
        if (!RequireString(root, "", "theatre", theatre) || !RequireString(root, "", "weapons", weapons)) return false;
        if (!LoadTheatreFile(data_dir + "/" + theatre, out.theatre, &e)) return Fail("theatre", e);
        if (!LoadWeaponsFile(data_dir + "/" + weapons, out.weapons, &e)) return Fail("weapons", e);
        // "defences": theatre files, the first object of each system in the first file that has it.
        std::string tuning;
        if (!OptionalString(root, "", "tuning", tuning)) return false;
        if (const json::Value* ds = root.Find("defences")) {
            if (!ds->Is(json::Type::Array)) return Fail("defences", "expected a list of theatre files");
            for (std::size_t i = 0; i < ds->items.size(); ++i) {
                if (!ds->items[i].Is(json::Type::String)) return Fail(Index("defences", i), "expected a theatre file");
                TheatreDef d;
                if (!LoadTheatreFile(data_dir + "/" + ds->items[i].string, d, &e)) return Fail(Index("defences", i), e);
                for (const TheatreObject& o : d.objects) {
                    if (o.defence.present && !o.defence.system.empty()) out.defences.emplace(o.defence.system, o.defence);
                }
            }
        }
        if (!tuning.empty() && !LoadTuningFile(data_dir + "/" + tuning, out.tuning, &e)) return Fail("tuning", e);
        std::string damage, aaa;
        if (!OptionalString(root, "", "damage", damage) || !OptionalString(root, "", "aaa", aaa)) return false;
        if (damage.empty() != aaa.empty()) return Fail(damage.empty() ? "damage" : "aaa", "damage and aaa come together");
        if (!damage.empty()) {
            if (!LoadDamageFile(data_dir + "/" + damage, out.damage, &e)) return Fail("damage", e);
            if (!LoadAaaFile(data_dir, aaa, out.aaa, &e)) return Fail("aaa", e);
            out.has_damage = true;
        }
        const json::Value* fs = OptionalArray(root, "", "factions");
        if (fs == nullptr || fs->items.empty()) return message_.empty() ? Fail("factions", "required, a list of faction files") : false;
        for (std::size_t i = 0; i < fs->items.size(); ++i) {
            const std::string p = Index("factions", i);
            if (!fs->items[i].Is(json::Type::String)) return Fail(p, "expected a faction file");
            FactionDef f;
            if (!LoadFactionFile(data_dir + "/" + fs->items[i].string, f, &e)) return Fail(p, e);
            if (out.theatre.FrontFor(f.id) == nullptr || out.theatre.FrontFor(f.id)->entries.empty()) {
                return Fail(p, "the theatre has no front with entries for '" + f.id + "'");
            }
            out.factions.push_back(std::move(f));
        }
        return LoadTiming(root, out) && LoadGeometry(root, out) && LoadKinds(root, out) && LoadFriendly(root, out);
    }

private:
    bool Range(const json::Value& obj, const std::string& path, const char* key, double& lo, double& hi) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return true;
        if (!v->Is(json::Type::Array) || v->items.size() != 2 || !v->items[0].Is(json::Type::Number) ||
            !v->items[1].Is(json::Type::Number) || v->items[0].number > v->items[1].number || v->items[0].number < 0.0) {
            return Fail(Join(path, key), "expected [least, most], 0 <= least <= most");
        }
        lo = v->items[0].number;
        hi = v->items[1].number;
        return true;
    }

    bool LoadTiming(const json::Value& root, WavesDef& out) {
        const json::Value* t = root.Find("timing");
        if (t == nullptr) return true;
        double fmin = out.flights_min, fmax = out.flights_max, cap = out.max_airborne;
        if (!Keys(*t, "timing", {"first_wave_s", "interval_s", "flights", "max_airborne", "friendly_share", "remove_after_s", "notes"}) ||
            !OptionalNumber(*t, "timing", "first_wave_s", out.first_wave_s) ||
            !Range(*t, "timing", "interval_s", out.interval_min_s, out.interval_max_s) || !Range(*t, "timing", "flights", fmin, fmax) ||
            !OptionalNumber(*t, "timing", "max_airborne", cap) || !OptionalNumber(*t, "timing", "friendly_share", out.friendly_share) ||
            !OptionalNumber(*t, "timing", "remove_after_s", out.remove_after_s)) {
            return false;
        }
        if (out.interval_min_s < 10.0) return Fail("timing.interval_s", "at least 10 s between waves");
        if (fmin < 1.0) return Fail("timing.flights", "at least one flight a wave");
        if (cap < 1.0) return Fail("timing.max_airborne", "at least 1");
        if (out.friendly_share < 0.0 || out.friendly_share > 1.0) return Fail("timing.friendly_share", "between 0 and 1");
        out.flights_min = static_cast<int>(fmin);
        out.flights_max = static_cast<int>(fmax);
        out.max_airborne = static_cast<int>(cap);
        return true;
    }

    bool LoadGeometry(const json::Value& root, WavesDef& out) {
        const json::Value* g = root.Find("geometry");
        if (g == nullptr) return true;
        double emin = out.entry_min_m / 1000.0, emax = out.entry_max_m / 1000.0;
        double hmin = out.helicopter_min_m / 1000.0, hmax = out.helicopter_max_m / 1000.0;
        double jam = out.jammer_orbit_m / 1000.0, sup = out.support_orbit_m / 1000.0, city = out.city_radius_m / 1000.0;
        if (!Keys(*g, "geometry", {"entry_km", "helicopter_km", "jammer_orbit_km", "support_orbit_km", "city_radius_km", "notes"}) ||
            !Range(*g, "geometry", "entry_km", emin, emax) || !Range(*g, "geometry", "helicopter_km", hmin, hmax) ||
            !OptionalNumber(*g, "geometry", "jammer_orbit_km", jam) || !OptionalNumber(*g, "geometry", "support_orbit_km", sup) ||
            !OptionalNumber(*g, "geometry", "city_radius_km", city)) {
            return false;
        }
        if (emin < 10.0) return Fail("geometry.entry_km", "the waves come in from 10 km out at least");
        out.entry_min_m = emin * 1000.0;
        out.entry_max_m = emax * 1000.0;
        out.helicopter_min_m = hmin * 1000.0;
        out.helicopter_max_m = hmax * 1000.0;
        out.jammer_orbit_m = jam * 1000.0;
        out.support_orbit_m = sup * 1000.0;
        out.city_radius_m = city * 1000.0;
        return true;
    }

    bool LoadKinds(const json::Value& root, WavesDef& out) {
        const json::Value* ks = OptionalArray(root, "", "weapon_kinds");
        if (ks == nullptr) return message_.empty() ? Fail("weapon_kinds", "required") : false;
        for (std::size_t i = 0; i < ks->items.size(); ++i) {
            const json::Value& v = ks->items[i];
            const std::string p = Index("weapon_kinds", i);
            WaveWeaponKind k;
            double release_km = k.release_m / 1000.0;
            if (!Keys(v, p, {"kind", "release_km", "scatter_m", "notes"}) || !RequireString(v, p, "kind", k.kind) ||
                !OptionalNumber(v, p, "release_km", release_km) || !OptionalNumber(v, p, "scatter_m", k.scatter_m)) {
                return false;
            }
            if (!(release_km > 0.0) || k.scatter_m < 0.0) {
                return Fail(p, "release_km > 0, scatter_m >= 0");
            }
            if (out.Kind(k.kind) != nullptr) return Fail(Join(p, "kind"), "twice: '" + k.kind + "'");
            k.release_m = release_km * 1000.0;
            out.weapon_kinds.push_back(std::move(k));
        }
        for (const WeaponDef& w : out.weapons.weapons) {
            if (out.Kind(w.kind) == nullptr) return Fail("weapon_kinds", "no entry for the weapons' kind '" + w.kind + "' (" + w.id + ")");
        }
        return true;
    }

    bool LoadFriendly(const json::Value& root, WavesDef& out) {
        const json::Value* fs = OptionalArray(root, "", "friendly");
        if (fs != nullptr) {
            for (std::size_t i = 0; i < fs->items.size(); ++i) {
                const json::Value& v = fs->items[i];
                const std::string p = Index("friendly", i);
                WaveFriendlyType t;
                double cmin = t.count_min, cmax = t.count_max;
                if (!Keys(v, p, {"id", "name", "model", "class", "roles", "helicopter", "speed_mps", "alt_m", "rcs_m2", "count", "fit", "notes"}) ||
                    !OptionalString(v, p, "fit", t.fit) ||
                    !RequireString(v, p, "id", t.id) || !RequireString(v, p, "name", t.name) || !RequireString(v, p, "model", t.model) ||
                    !OptionalString(v, p, "class", t.target_class) || !OptionalBool(v, p, "helicopter", t.helicopter) ||
                    !OptionalNumber(v, p, "speed_mps", t.speed_mps) || !OptionalNumber(v, p, "alt_m", t.alt_m) ||
                    !OptionalNumber(v, p, "rcs_m2", t.rcs_m2) || !Range(v, p, "count", cmin, cmax)) {
                    return false;
                }
                const json::Value* rs = OptionalArray(v, p, "roles");
                if (rs == nullptr || rs->items.empty()) return message_.empty() ? Fail(Join(p, "roles"), "required") : false;
                for (const json::Value& r : rs->items) {
                    static const char* const kRoles[] = {"cap", "intercept", "ground_attack", "transport", "helicopter", "trainer"};
                    bool known = false;
                    for (const char* k : kRoles) known = known || (r.Is(json::Type::String) && r.string == k);
                    if (!known) return Fail(Join(p, "roles"), "cap, intercept, ground_attack, transport, helicopter or trainer");
                    t.roles.push_back(r.string);
                }
                if (!(t.speed_mps > 0.0) || !(t.alt_m > 0.0) || !(t.rcs_m2 > 0.0) || cmin < 1.0) {
                    return Fail(p, "speed_mps, alt_m and rcs_m2 > 0, count at least 1");
                }
                t.count_min = static_cast<int>(cmin);
                t.count_max = static_cast<int>(cmax);
                out.friendly.push_back(std::move(t));
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* cs = OptionalArray(root, "", "friendly_callsigns")) {
            for (const json::Value& c : cs->items) {
                if (!c.Is(json::Type::String) || c.string.empty()) return Fail("friendly_callsigns", "expected names");
                out.friendly_callsigns.push_back(c.string);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (!out.friendly.empty() && out.friendly_callsigns.empty()) return Fail("friendly_callsigns", "our flights need call signs");
        return true;
    }
};

}  // namespace detail

// Loads the waves file `rel` (relative to the data folder) with the theatre, weapons and factions it names.
inline bool LoadWavesFile(const std::string& data_dir, const std::string& rel, WavesDef& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "waves: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "waves: " + e;
        return false;
    }
    detail::WavesLoader loader;
    if (!loader.Load(root, data_dir, out)) {
        if (error != nullptr) *error = "waves " + rel + ": " + loader.Message();
        return false;
    }
    return true;
}

}  // namespace airwar
}  // namespace simcore
