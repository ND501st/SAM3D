// simcore/air_war/officers.hpp — the enemy's air staff as people (docs/air_war/02_AIR_STAFF.md).
//
// Every raid the player meets was planned by someone. The staff of an enemy air command: a commander who sets the
// objectives and approves the plan, a planner who builds the packages, an intelligence officer who keeps the picture,
// an EW/SEAD officer (where the faction has one), a political officer (in the Warsaw Pact), and a leader for every
// squadron, who flies. Each has eight traits and six skills (0..1), quirks with named effects on his choices, and a
// state that changes with the war: stress, confidence, reputation with his superiors, grudges against the units that
// shot his men down, and the tactics he remembers with how they went. Officers are relieved, promoted, killed or
// captured, and replaced by someone different, and the player sees all of it only through the raids and through what
// his own side's intelligence learns.
#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace airwar {

// What an officer remembers about one way of doing a mission (entry point, height, time of day, mission kind), as he
// believes it went: the share of aircraft lost and the success claimed, smoothed over uses.
struct TacticMemory {
    double loss_rate = 0.0;
    double success = 0.5;
    int uses = 0;
    double last_used_h = -1e9;
};

inline std::string TacticKey(const std::string& entry, Profile p, DayPart d, MissionKind k) {
    return entry + "|" + ProfileName(p) + "|" + DayPartName(d) + "|" + MissionKindName(k);
}

struct Officer {
    OfficerDef def;
    std::vector<QuirkDef> quirks;
    // In post, or what became of him.
    bool active = true;
    std::string fate;             // "", "relieved", "promoted", "killed", "captured"
    double appointed_h = 0.0;
    double left_h = -1.0;
    // State.
    double stress = 0.1;
    double confidence = 0.5;
    double reputation = 0.6;      // with his superiors
    std::map<std::string, double> grudges;      // what he believes shot his men down (intel entry key) -> weight
    std::map<std::string, TacticMemory> tactics;
    std::string last_entry;       // the entry point of his last package
    double lesson_sead = 0.0;     // more SEAD per threat, learnt from losses to radar defences
    int packages_planned = 0;
    int aircraft_lost = 0;        // under his plans (planner) or his command (leader)
    double damage_claimed = 0.0;
    std::vector<std::string> history;  // career and character notes, for the debrief and the dossier

    // The sum of an effect over his quirks.
    double Quirk(const std::string& key) const {
        double s = 0.0;
        for (const QuirkDef& q : quirks) s += q.Effect(key);
        return s;
    }
    bool HasQuirk(const std::string& id) const {
        for (const QuirkDef& q : quirks) {
            if (q.id == id) return true;
        }
        return false;
    }
    // Stress makes everyone worse; composure holds it back.
    double Effective(double skill) const { return Clamp01(skill * (1.0 - 0.5 * stress * (1.0 - def.traits.composure))); }
    // How fast he learns from what happened: the rigid learn slowly, the methodical carefully.
    double LearningRate(double base) const {
        return base * (1.0 - 0.8 * def.traits.rigidity) * (0.6 + 0.4 * def.traits.methodical);
    }
    std::string Title() const { return (def.rank.empty() ? std::string() : def.rank + " ") + def.name; }
    void Note(double h, double start_hour, const std::string& what) { history.push_back(FormatHour(h, start_hour) + " " + what); }
};

// The staff, past and present. Indices into `officers` are stable for the whole campaign.
class AirStaff {
public:
    std::vector<Officer> officers;

    int Holder(StaffRole role) const {
        const auto it = holders_.find(static_cast<int>(role));
        return it == holders_.end() ? -1 : it->second;
    }
    const Officer* HolderPtr(StaffRole role) const {
        const int i = Holder(role);
        return i < 0 ? nullptr : &officers[static_cast<std::size_t>(i)];
    }
    Officer* HolderPtr(StaffRole role) {
        const int i = Holder(role);
        return i < 0 ? nullptr : &officers[static_cast<std::size_t>(i)];
    }
    int LeaderOf(const std::string& squadron) const {
        const auto it = leaders_.find(squadron);
        return it == leaders_.end() ? -1 : it->second;
    }
    std::vector<int> Active() const {
        std::vector<int> out;
        for (std::size_t i = 0; i < officers.size(); ++i) {
            if (officers[i].active) out.push_back(static_cast<int>(i));
        }
        return out;
    }

    // Build the staff the faction starts the war with: the authored officers in their posts, generated ones where the
    // faction names none (the EW/SEAD post only where the doctrine has SEAD; the political officer in the Pact only).
    void Build(const AirWarData& data, Rng& rng, double now_h) {
        officers.clear();
        holders_.clear();
        leaders_.clear();
        const FactionDef& f = data.faction;
        for (const OfficerDef& od : f.officers) {
            Officer o = Make(od, data);
            o.appointed_h = now_h;
            const int idx = static_cast<int>(officers.size());
            officers.push_back(std::move(o));
            if (od.role == StaffRole::SquadronLeader) {
                if (leaders_.count(od.squadron) == 0) leaders_[od.squadron] = idx;
            } else if (holders_.count(static_cast<int>(od.role)) == 0) {
                holders_[static_cast<int>(od.role)] = idx;
            }
        }
        for (StaffRole r : {StaffRole::Commander, StaffRole::Planner, StaffRole::Intelligence}) {
            if (Holder(r) < 0) Appoint(r, std::string(), data, rng, now_h, "appointed at the start of the war");
        }
        if (Holder(StaffRole::Sead) < 0 && HasSead(f)) {
            Appoint(StaffRole::Sead, std::string(), data, rng, now_h, "appointed at the start of the war");
        }
        if (Holder(StaffRole::Political) < 0 && f.pool.ranks.count("political") > 0) {
            Appoint(StaffRole::Political, std::string(), data, rng, now_h, "appointed at the start of the war");
        }
        for (const SquadronDef& s : f.squadrons) {
            if (LeaderOf(s.id) >= 0) continue;
            const AircraftType* at = f.Aircraft(s.aircraft);
            if (at != nullptr && at->drone && s.leader.empty()) continue;  // a drone detachment has no flying leader
            int found = -1;
            if (!s.leader.empty()) {
                for (std::size_t i = 0; i < officers.size(); ++i) {
                    if (officers[i].def.id == s.leader) found = static_cast<int>(i);
                }
            }
            if (found >= 0) {
                leaders_[s.id] = found;
            } else {
                Appoint(StaffRole::SquadronLeader, s.id, data, rng, now_h, "leads the squadron at the start of the war");
            }
        }
    }

    // Put a new officer in a post (generated from the faction's pool). Returns his index.
    int Appoint(StaffRole role, const std::string& squadron, const AirWarData& data, Rng& rng, double now_h,
                const std::string& why, const Officer* predecessor = nullptr) {
        OfficerDef od = Generate(role, squadron, data, rng, predecessor);
        Officer o = Make(od, data);
        o.appointed_h = now_h;
        o.Note(now_h, data.theatre.start_hour, why);
        const int idx = static_cast<int>(officers.size());
        officers.push_back(std::move(o));
        if (role == StaffRole::SquadronLeader) {
            leaders_[squadron] = idx;
        } else {
            holders_[static_cast<int>(role)] = idx;
        }
        return idx;
    }

    // Take an officer out of his post (relieved, promoted, killed, captured) and appoint his successor. A relieved
    // officer's successor leans the other way on the trait that failed (the staff learns too).
    int Replace(int idx, const std::string& fate, const std::string& why, const AirWarData& data, Rng& rng, double now_h) {
        Officer& o = officers[static_cast<std::size_t>(idx)];
        if (!o.active) return -1;
        o.active = false;
        o.fate = fate;
        o.left_h = now_h;
        o.Note(now_h, data.theatre.start_hour, fate + ": " + why);
        const StaffRole role = o.def.role;
        const std::string squadron = o.def.squadron;
        const Officer predecessor = o;  // copy: officers may reallocate below
        const std::string reason = "replaces " + predecessor.Title() + " (" + fate + ")";
        return Appoint(role, squadron, data, rng, now_h, reason, fate == "relieved" ? &predecessor : nullptr);
    }

    static bool HasSead(const FactionDef& f) {
        return f.doctrine.packages[static_cast<int>(MissionKind::Dead)].allowed ||
               f.doctrine.packages[static_cast<int>(MissionKind::Hunter)].allowed;
    }

private:
    std::map<int, int> holders_;
    std::map<std::string, int> leaders_;

    static Officer Make(const OfficerDef& od, const AirWarData& data) {
        Officer o;
        o.def = od;
        for (const std::string& q : od.quirks) {
            if (const QuirkDef* qd = data.Quirk(q)) o.quirks.push_back(*qd);
        }
        o.confidence = 0.4 + 0.3 * od.traits.boldness;
        return o;
    }

    static double Draw(Rng& rng, double mean, double sd) { return Clamp01(rng.Normal(mean, sd)); }

    OfficerDef Generate(StaffRole role, const std::string& squadron, const AirWarData& data, Rng& rng,
                        const Officer* predecessor) const {
        const FactionDef& f = data.faction;
        const OfficerPoolDef& pool = f.pool;
        OfficerDef od;
        od.role = role;
        od.squadron = squadron;
        od.authored = false;
        // A name not yet used in this staff.
        for (int attempt = 0; attempt < 40; ++attempt) {
            const std::string first = pool.first_names[static_cast<std::size_t>(rng.Index(static_cast<int>(pool.first_names.size())))];
            const std::string last = pool.last_names[static_cast<std::size_t>(rng.Index(static_cast<int>(pool.last_names.size())))];
            od.name = first + " " + last;
            bool used = false;
            for (const Officer& o : officers) used = used || o.def.name == od.name;
            if (!used) break;
        }
        od.id = f.id + "_gen_" + std::to_string(officers.size() + 1);
        const auto rk = pool.ranks.find(StaffRoleName(role));
        od.rank = rk == pool.ranks.end() ? std::string() : rk->second;
        od.age = 30 + rng.Index(20) + (role == StaffRole::Commander ? 8 : 0);
        od.background = pool.backgrounds[static_cast<std::size_t>(rng.Index(static_cast<int>(pool.backgrounds.size())))];
        const Traits& m = pool.trait_mean;
        const double sd = pool.trait_sd;
        Traits& t = od.traits;
        t.boldness = Draw(rng, m.boldness, sd);
        t.methodical = Draw(rng, m.methodical, sd);
        t.creativity = Draw(rng, m.creativity, sd);
        t.rigidity = Draw(rng, m.rigidity, sd);
        t.ambition = Draw(rng, m.ambition, sd);
        t.skepticism = Draw(rng, m.skepticism, sd);
        t.vindictiveness = Draw(rng, m.vindictiveness, sd);
        t.composure = Draw(rng, m.composure, sd);
        if (predecessor != nullptr) {
            // The man who replaces a relieved officer is chosen against his failing: more careful after a reckless
            // one, bolder after a timid one, more flexible after a rigid one.
            const Traits& pt = predecessor->def.traits;
            if (pt.boldness > 0.6) t.boldness = Clamp01(t.boldness - 0.25);
            if (pt.boldness < 0.4) t.boldness = Clamp01(t.boldness + 0.25);
            if (pt.rigidity > 0.6) t.rigidity = Clamp01(t.rigidity - 0.25);
            t.methodical = Clamp01(t.methodical + 0.1);
        }
        const Skills& sm = pool.skill_mean;
        const double ss = pool.skill_sd;
        od.skills.planning = Draw(rng, sm.planning, ss);
        od.skills.intelligence = Draw(rng, sm.intelligence, ss);
        od.skills.sead = Draw(rng, sm.sead, ss);
        od.skills.airmanship = Draw(rng, sm.airmanship, ss);
        od.skills.leadership = Draw(rng, sm.leadership, ss);
        od.skills.politics = Draw(rng, sm.politics, ss);
        // Quirks, weighted by tier, allowed for the role, no repeats.
        const int n = pool.quirks_min + rng.Index(pool.quirks_max - pool.quirks_min + 1);
        std::vector<double> w;
        for (const QuirkDef& q : data.quirks) {
            double x = q.tier_weight[static_cast<int>(f.tier)];
            if (!q.roles.empty()) {
                bool ok = false;
                for (StaffRole r : q.roles) ok = ok || r == role;
                if (!ok) x = 0.0;
            }
            w.push_back(x);
        }
        for (int k = 0; k < n; ++k) {
            const int qi = rng.Weighted(w);
            if (qi < 0 || w[static_cast<std::size_t>(qi)] <= 0.0) break;
            od.quirks.push_back(data.quirks[static_cast<std::size_t>(qi)].id);
            w[static_cast<std::size_t>(qi)] = 0.0;
        }
        od.flies = role == StaffRole::SquadronLeader;
        for (const std::string& q : od.quirks) {
            const QuirkDef* qd = data.Quirk(q);
            if (qd != nullptr && qd->Effect("flies") > 0.0) od.flies = true;
        }
        od.bio = od.background + ", " + std::to_string(od.age) + ".";
        return od;
    }
};

// How an officer's character reads in plain words (for dossiers and the debrief).
inline std::string DescribeTraits(const Traits& t) {
    std::vector<std::string> w;
    if (t.boldness > 0.65) w.push_back("bold");
    if (t.boldness < 0.35) w.push_back("cautious");
    if (t.methodical > 0.65) w.push_back("methodical");
    if (t.methodical < 0.35) w.push_back("slapdash");
    if (t.creativity > 0.65) w.push_back("inventive");
    if (t.rigidity > 0.65) w.push_back("rigid");
    if (t.rigidity < 0.3) w.push_back("flexible");
    if (t.ambition > 0.7) w.push_back("ambitious");
    if (t.skepticism > 0.65) w.push_back("sceptical of claims");
    if (t.skepticism < 0.3) w.push_back("credulous");
    if (t.vindictiveness > 0.7) w.push_back("vindictive");
    if (t.composure < 0.35) w.push_back("brittle under losses");
    if (t.composure > 0.7) w.push_back("unflappable");
    std::string s;
    for (std::size_t i = 0; i < w.size(); ++i) s += (i == 0 ? "" : ", ") + w[i];
    return s.empty() ? std::string("unremarkable") : s;
}

}  // namespace airwar
}  // namespace simcore
