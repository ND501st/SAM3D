// simcore/air_war/intel.hpp — what the enemy believes about the player's side (docs/air_war/03_INTELLIGENCE.md).
//
// The enemy never plans on the truth. Its intelligence officer keeps a picture: one entry per thing it believes is
// there, with a believed kind, a position and its uncertainty, how far it believes the thing still works, how sure it
// is that the thing exists at all, when it was last seen and by what. Reports from the collection disciplines arrive
// late (film has to be processed, agents' reports travel) and are merged into it. Entries about mobile units age: the
// position spreads and the confidence fades, because a battery that was seen yesterday may be anywhere today (Dugman 5,
// docs/research/ENEMY_AIR_RESEARCH_2026-10.md §3). Ghost entries are things that aren't there: an ELINT fix between a
// radar and its decoy, an agent's wrong or planted report. Dummies are real objects the enemy may take for the real
// thing. Correlation of reports about the same real object is assumed right (the enemy's analysts tie a battery's
// fixes together by its signature); ghosts are matched by place.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace airwar {

struct IntelEntry {
    std::string key;            // the truth object id, or "ghost-<n>" for something that isn't there
    std::string object_id;      // "" for a ghost
    bool ghost = false;
    std::string label;          // how the enemy names it
    ObjectKind kind = ObjectKind::SamSite;  // what it believes it is
    Vec3 pos;
    double sigma_m = 0.0;       // 1-sigma horizontal uncertainty of the position
    double exists = 1.0;        // belief that something is there
    double function = 1.0;      // believed working share
    double observed_h = -1e9;   // when it was last observed (not when the report arrived)
    IntelSource source = IntelSource::None;
    bool mobile = false;
    bool emitter = false;
    double value = 0.5;         // its worth to the enemy's targeteers (before weights)
    double suspected_decoy = 0.0;
    // The air defence the enemy believes it has.
    double threat_radius_m = 0.0;
    double threat_lethality = 0.0;
    bool threat_low = true;
    bool threat_medium = true;
    bool threat_radar = true;
    int threat_channels = 0;    // aircraft it engages at once (DefenceDef::channels; 0: all in reach)
    double threat_max_target_mps = 0.0;  // the fastest target it can engage (0: any)
    // Its record with the enemy.
    int strikes = 0;
    double last_struck_h = -1e9;
    double claimed_destroyed_h = -1.0;  // pilots claimed it destroyed then
    bool claim_was_false = false;       // (truth, for the story) it was not destroyed when they claimed it
    double losses_credited = 0.0;       // aircraft the enemy believes it shot down

    bool IsThreat() const { return threat_radius_m > 0.0 && threat_lethality > 0.0; }
    // A radar SAM site: what a rollback hunts and counts. Guns, radar-laid or not, are flown round or over and left to the
    // strike's escort.
    bool IsSamRadar() const { return kind == ObjectKind::SamSite && threat_radar; }
    // How much it is worth acting on now: believed there, believed working.
    double Live() const { return Clamp01(exists) * Clamp01(function) * (1.0 - 0.7 * suspected_decoy); }
};

// Something the enemy will learn at `arrive_h`.
struct IntelReport {
    double arrive_h = 0.0;
    double observed_h = 0.0;
    IntelSource source = IntelSource::None;
    std::string object_id;      // the real object it is about; "" for a ghost
    bool ghost = false;
    ObjectKind kind = ObjectKind::SamSite;
    Vec3 pos;
    double sigma_m = 1000.0;
    double function = -1.0;     // observed working share; < 0: not observed
    double exists = 0.8;        // how much the report itself is believed
    bool claim_destroyed = false;
    bool claim_false = false;   // (truth) the claimed destruction did not happen
    bool decoy_spotted = false; // an analyst saw through a dummy
    double losses = 0.0;        // aircraft it shot down (credited to it)
    // The air defence it is believed to have (from its signature or what it did).
    double threat_radius_m = 0.0;
    double threat_lethality = 0.0;
    bool threat_low = true;
    bool threat_medium = true;
    bool threat_radar = true;
    int threat_channels = 0;
    double threat_max_target_mps = 0.0;
    std::string label;
    std::string note;
};

class IntelPicture {
public:
    std::vector<IntelEntry> entries;
    std::vector<IntelReport> pending;

    IntelEntry* Find(const std::string& key) {
        for (IntelEntry& e : entries) {
            if (e.key == key) return &e;
        }
        return nullptr;
    }
    const IntelEntry* Find(const std::string& key) const {
        for (const IntelEntry& e : entries) {
            if (e.key == key) return &e;
        }
        return nullptr;
    }

    void Queue(IntelReport r) { pending.push_back(std::move(r)); }

    // Merge every report that has arrived by `now_h`. Returns the reports merged (for the journal).
    std::vector<IntelReport> Deliver(double now_h, double analyst_skill) {
        std::vector<IntelReport> arrived;
        std::vector<IntelReport> later;
        for (IntelReport& r : pending) {
            if (r.arrive_h <= now_h + 1e-9) {
                arrived.push_back(std::move(r));
            } else {
                later.push_back(std::move(r));
            }
        }
        pending.swap(later);
        std::sort(arrived.begin(), arrived.end(),
                  [](const IntelReport& a, const IntelReport& b) { return a.arrive_h < b.arrive_h; });
        for (const IntelReport& r : arrived) Merge(r, analyst_skill);
        return arrived;
    }

    // Age the picture by dt hours: mobile positions spread and fade; believed damage of fixed targets heals at the
    // rate the enemy assumes repair goes.
    void Decay(double dt_h, const TuningTable& tuning) {
        const double growth_m = tuning.Get("intel.mobile_sigma_growth_mps") * 3600.0 * dt_h;
        const double half = tuning.Get("intel.mobile_confidence_half_life_h");
        const double recovery = tuning.Get("intel.fixed_function_recovery");
        for (IntelEntry& e : entries) {
            if (e.mobile || e.ghost) {
                e.sigma_m += growth_m;  // it may have driven off in any direction at any time since
                if (half > 0.0) e.exists *= std::pow(0.5, dt_h / half);
            } else if (e.function < 1.0) {
                e.function = std::min(1.0, e.function + recovery * dt_h / 48.0);
            }
        }
        entries.erase(std::remove_if(entries.begin(), entries.end(), [](const IntelEntry& e) { return e.ghost && e.exists < 0.05; }),
                      entries.end());
    }

    // The share of the radar air defences it believes are there that it believes still work (the NATO rollback phase
    // ends on this): each weighed by the belief that it is there and real. A site found later counts like one known
    // from the start, so the share falls only as fast as the sites it knows of are destroyed, not as fast as it finds
    // new ones (the "percentage of known SAM sites destroyed or suppressed" a staff reports).
    // The whole country's: a battery that survives anywhere keeps the rollback from ending (the enemy's freedom of action,
    // docs/campaign/02_WHAT_WE_DEFEND.md §5).
    double BelievedIadsShare() const {
        double working = 0.0;
        double there = 0.0;
        for (const IntelEntry& e : entries) {
            if (!e.IsSamRadar()) continue;
            const double w = Clamp01(e.exists) * (1.0 - 0.7 * e.suspected_decoy);
            working += w * Clamp01(e.function);
            there += w;
        }
        return there > 0.0 ? Clamp01(working / there) : 1.0;
    }

    // Believed threats: (entry, effective lethality) for the planner.
    std::vector<const IntelEntry*> Threats(double min_live = 0.15) const {
        std::vector<const IntelEntry*> out;
        for (const IntelEntry& e : entries) {
            if (e.IsThreat() && e.Live() >= min_live) out.push_back(&e);
        }
        return out;
    }

    int ghosts_made = 0;
    // Entries the pilots had claimed destroyed and that a later report shows working ("the truth comes out").
    std::vector<std::string> resurrected;
    // Entries truly destroyed that a later report shows working again: repaired, faster than the enemy assumed.
    std::vector<std::string> repaired;

private:
    void Merge(const IntelReport& r, double analyst_skill) {
        IntelEntry* e = nullptr;
        if (!r.ghost) {
            e = Find(r.object_id);
        } else {
            // A ghost report: the nearest ghost within reach, else a new one.
            double best = 1e18;
            for (IntelEntry& g : entries) {
                if (!g.ghost) continue;
                const double d = Dist2d(g.pos, r.pos);
                if (d < 2.5 * std::max(g.sigma_m, r.sigma_m) && d < best) {
                    best = d;
                    e = &g;
                }
            }
        }
        if (e == nullptr) {
            IntelEntry n;
            n.key = r.ghost ? "ghost-" + std::to_string(++ghosts_made) : r.object_id;
            n.object_id = r.ghost ? std::string() : r.object_id;
            n.ghost = r.ghost;
            n.kind = r.kind;
            n.label = r.label.empty() ? std::string(ObjectKindName(r.kind)) : r.label;
            n.pos = r.pos;
            n.sigma_m = r.sigma_m;
            n.exists = r.exists;
            n.function = r.function >= 0.0 ? r.function : 1.0;
            n.observed_h = r.observed_h;
            n.source = r.source;
            n.mobile = r.kind == ObjectKind::SamSite || r.kind == ObjectKind::Troops || r.ghost;
            n.emitter = r.threat_radar && IsAirDefence(r.kind);
            n.threat_radius_m = r.threat_radius_m;
            n.threat_lethality = r.threat_lethality;
            n.threat_low = r.threat_low;
            n.threat_medium = r.threat_medium;
            n.threat_radar = r.threat_radar;
            n.threat_channels = r.threat_channels;
            n.threat_max_target_mps = r.threat_max_target_mps;
            n.losses_credited = r.losses;
            if (r.claim_destroyed) {
                n.function = 0.0;
                n.claimed_destroyed_h = r.observed_h;
                n.claim_was_false = r.claim_false;
            }
            if (r.decoy_spotted) n.suspected_decoy = 0.8;
            entries.push_back(std::move(n));
            return;
        }
        // Position: inverse-variance blend, the older estimate counting for less the older it is.
        if (r.sigma_m > 0.0 && r.observed_h >= e->observed_h - 1e-9) {
            const double w_new = 1.0 / (r.sigma_m * r.sigma_m);
            const double w_old = 1.0 / (e->sigma_m * e->sigma_m + 1.0);
            const double moved = Dist2d(e->pos, r.pos);
            if (moved > 3.0 * (e->sigma_m + r.sigma_m)) {
                e->pos = r.pos;  // it has moved: take the new place
                e->sigma_m = r.sigma_m;
            } else {
                e->pos = (e->pos * w_old + r.pos * w_new) * (1.0 / (w_old + w_new));
                e->sigma_m = std::sqrt(1.0 / (w_old + w_new));
            }
            e->observed_h = r.observed_h;
            e->source = r.source;
        }
        e->exists = Clamp01(std::max(e->exists, r.exists) + 0.1 * (1.0 - e->exists));
        if (r.function >= 0.0) {
            // A claim of destruction is believed in proportion to the analysts' credulity (skill doubts claims).
            if (r.claim_destroyed) {
                const double belief = 1.0 - 0.6 * analyst_skill;
                e->function = Clamp01(e->function * (1.0 - belief));
                e->claimed_destroyed_h = r.observed_h;
                e->claim_was_false = r.claim_false;
            } else {
                e->function = r.function;
                if (r.function > 0.5 && e->claimed_destroyed_h >= 0.0 && r.observed_h > e->claimed_destroyed_h) {
                    (e->claim_was_false ? resurrected : repaired).push_back(e->key);
                }
                if (r.function > 0.5) e->claimed_destroyed_h = -1.0;
            }
        }
        if (r.threat_radius_m > 0.0) {
            e->threat_radius_m = r.threat_radius_m;
            e->threat_lethality = r.threat_lethality;
            e->threat_low = r.threat_low;
            e->threat_medium = r.threat_medium;
            e->threat_radar = r.threat_radar;
            e->threat_channels = r.threat_channels;
            e->threat_max_target_mps = r.threat_max_target_mps;
            e->emitter = r.threat_radar;
        }
        e->losses_credited += r.losses;
        if (r.decoy_spotted) e->suspected_decoy = Clamp01(e->suspected_decoy + 0.6);
        if (!r.label.empty() && (e->label.empty() || e->label == ObjectKindName(e->kind))) e->label = r.label;
    }
};

}  // namespace airwar
}  // namespace simcore
