// simcore/campaign/ad_strategy.hpp — our air defence commanders' sector priorities (track F, F4.1;
// docs/fronts/04_AIR_DEFENCE_STRATEGY.md section 2).
//
// Each of our air defence commanders who holds a part of the defence keeps a priority per sector of the front: defend,
// take the offence, or economy. The national sector's commander decides for the national PVO's objects, the army's chief
// of air defence for the army's troop air defence (data/campaign/ad_strategy.json: who they are, at which posts of the
// chain, their traits, their rhythm and the rule table's values).
//
// First person and the written record (docs/command/01_PRINCIPLES.md): a commander decides only on what reached his post
// through our pipeline by the hour he decides (command/info_net.hpp), never on the ground war's or the air campaign's
// state:
//   * the axes our units reported (their depth and pace, the enemy in contact as they believed it, our counterattack on
//     them): the army's summaries (GroundFeed::Summaries, F3.8) and any unit's report that passed through his post;
//   * our commanders' counterattack orders (GroundFeed::Orders);
//   * the army's air defence staff's estimates of the enemy's air front (AirFrontWatch's conclusions, F2.5);
//   * the wrecks of our SAM sites reported (the feed's OwnDamage facts);
// beside his own side's plan: where our national objects and our SAM fire units stand in the theatre's data (the pre-war
// order of battle he holds) and the front's base line and sectors.
//
// He decides on his rhythm (a cycle from the data, moved by his traits), and only when a report has reached his post
// since his last decision. A decision reads the rule table (RulePriorities, pure) and changes at most one sector
// (DecideChange): a methodical man waits for a second report, a rigid one holds the pre-war plan one decision longer, a
// bold one takes the offence on a shallower estimate. Each change is an entry in his post's records (Decisions) and an
// order, a fact of kind Order from his post to the posts the data names (the SAM regiment's commander among the army
// chief's), which the pipeline carries with its delays. F4.1 only decides and orders: F4.2 moves units on the orders,
// F4.3 shares the fighters by sector (PriorityShares), F4.4 gives the regiment's tasking a trigger (PriorityKnownBy).
//
// Only at ad_strategy.mode 1 (data/gameplay/air_war.json), on a front with a line, with the commanders' posts in the
// chain; at 0 nothing is loaded, read or added. Cheap: nothing per step; at a decision hour each source is read from a
// cursor (what is new) and a short list of what has not yet reached the post; the picture is a pass over the sectors'
// axes and the plan's objects. Deterministic: a decision hour is taken only below the watermark (the earliest hour a
// package not yet resolved could still add a fact), so a campaign pumped every step and one pumped once after its load
// make the same decisions and orders.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/command/air_front_watch.hpp"
#include "simcore/command/ground_feed.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/console_data.hpp"
#include "simcore/ground_war/commanders.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace campaign {

// ------------------------------------------------------------------------------------------------------------- the data

enum class SectorPriority : std::uint8_t { Economy, Defend, Offence };

inline const char* SectorPriorityName(SectorPriority p) {
    switch (p) {
        case SectorPriority::Economy: return "economy";
        case SectorPriority::Defend: return "defend";
        case SectorPriority::Offence: return "offence";
    }
    return "economy";
}

// The national sector's commander (the national PVO's objects) or the army's chief of air defence (the troops).
enum class AdRole : std::uint8_t { National, Troops };

struct AdCommanderDef {
    std::string id;
    std::string post;  // his post in the chain: he decides on what reached it, his orders leave from it
    std::string name;  // "" the post's holder
    std::string rank;
    std::string bio;
    AdRole role = AdRole::Troops;
    std::vector<std::string> to;               // the posts his orders are for
    groundwar::ValueRange rhythm_h{2.0, 4.0};  // his decision cycle's range, his traits place it
    double order_work_h = 0.5;                 // from his decision to the order sent
    airwar::Traits traits;
};

struct AdRules {
    double trait_shift = 0.2;
    double object_value = 0.7;                            // a national object of high importance is worth this or more
    double object_reach_m = 30000.0;                      // within the enemy's reach: this far beyond the believed line
    groundwar::ValueRange offence_depth_m{10000.0, 20000.0};  // the air front's estimate that moves him to the offence
    double offence_fresh_h = 48.0;                        // an estimate older than this no longer does
    int thin_cover_units = 2;                             // fewer SAM fire units in a sector is thin cover
    double main_share = 0.6;                              // the defended sectors' share of the air defence
    double offence_weight = 2.0;                          // the rest by these weights
    double economy_weight = 1.0;
    double methodical_above = 0.6;                        // waits for a second report
    double rigid_above = 0.6;                             // holds the pre-war plan a decision longer
    double stale_h = 72.0;                                // a report not arrived this long after it was compiled: not waited for
    std::vector<airwar::ObjectKind> national_kinds;

    bool National(airwar::ObjectKind k) const { return std::find(national_kinds.begin(), national_kinds.end(), k) != national_kinds.end(); }
};

struct AdStrategyData {
    AdRules rules;
    std::vector<AdCommanderDef> commanders;
};

namespace detail {

class AdStrategyLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, AdStrategyData& out) {
        if (!Keys(root, "", {"strategy_id", "name", "status", "source", "confidence", "notes", "rules", "commanders"}) || !Provenance(root, "")) {
            return false;
        }
        const json::Value* rules = root.Find("rules");
        if (rules == nullptr) return Fail("rules", "required");
        if (!LoadRules(*rules, out.rules)) return false;
        const json::Value* list = OptionalArray(root, "", "commanders");
        if (list == nullptr || list->items.empty()) return Fail("commanders", "required: at least one commander");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            AdCommanderDef c;
            if (!LoadCommander(list->items[i], Index("commanders", i), c)) return false;
            for (const AdCommanderDef& o : out.commanders) {
                if (o.id == c.id) return Fail(Index("commanders", i), "duplicate id '" + c.id + "'");
            }
            out.commanders.push_back(c);
        }
        return true;
    }

private:
    // {"value": x, "source": ..., "confidence": ...}
    bool Valued(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        const std::string p = Join(path, key);
        if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p) || !NumberOf(*v, p, "value", out)) return false;
        return true;
    }
    // {"min": a, "max": b, "source": ..., "confidence": ...}
    bool Ranged(const json::Value& obj, const std::string& path, const char* key, groundwar::ValueRange& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        const std::string p = Join(path, key);
        if (!Keys(*v, p, {"min", "max", "source", "confidence", "notes"}) || !Provenance(*v, p) || !NumberOf(*v, p, "min", out.lo) ||
            !NumberOf(*v, p, "max", out.hi)) {
            return false;
        }
        if (!(out.lo >= 0.0) || out.hi < out.lo) return Fail(p, "min >= 0 and max >= min");
        return true;
    }

    bool LoadRules(const json::Value& r, AdRules& out) {
        const std::string p = "rules";
        if (!Keys(r, p, {"trait_shift", "object_value", "object_reach_m", "offence_depth_m", "offence_fresh_h", "thin_cover_units", "main_share",
                         "offence_weight", "economy_weight", "methodical_above", "rigid_above", "stale_h", "national_kinds", "notes"})) {
            return false;
        }
        double thin = 0.0;
        if (!Valued(r, p, "trait_shift", out.trait_shift) || !Valued(r, p, "object_value", out.object_value) ||
            !Valued(r, p, "object_reach_m", out.object_reach_m) || !Ranged(r, p, "offence_depth_m", out.offence_depth_m) ||
            !Valued(r, p, "offence_fresh_h", out.offence_fresh_h) || !Valued(r, p, "thin_cover_units", thin) ||
            !Valued(r, p, "main_share", out.main_share) || !Valued(r, p, "offence_weight", out.offence_weight) ||
            !Valued(r, p, "economy_weight", out.economy_weight) || !Valued(r, p, "methodical_above", out.methodical_above) ||
            !Valued(r, p, "rigid_above", out.rigid_above) || !Valued(r, p, "stale_h", out.stale_h)) {
            return false;
        }
        out.thin_cover_units = static_cast<int>(thin);
        if (static_cast<double>(out.thin_cover_units) != thin || thin < 0.0) return Fail(Join(p, "thin_cover_units"), "a whole number >= 0");
        if (out.main_share < 0.0 || out.main_share > 1.0) return Fail(Join(p, "main_share"), "a share, 0 to 1");
        if (out.offence_weight < 0.0 || out.economy_weight < 0.0) return Fail(p, "weights >= 0");
        const json::Value* kinds = r.Find("national_kinds");
        if (kinds == nullptr) return Fail(Join(p, "national_kinds"), "required");
        const std::string kp = Join(p, "national_kinds");
        if (!Keys(*kinds, kp, {"value", "source", "confidence", "notes"}) || !Provenance(*kinds, kp)) return false;
        const json::Value* list = OptionalArray(*kinds, kp, "value");
        if (list == nullptr) return Fail(Join(kp, "value"), "required: a list of object kinds");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            airwar::ObjectKind k = airwar::ObjectKind::Depot;
            if (!list->items[i].Is(json::Type::String) || !airwar::ParseObjectKind(list->items[i].string, k)) {
                return Fail(Index(Join(kp, "value"), i), "an object kind");
            }
            out.national_kinds.push_back(k);
        }
        return true;
    }

    bool LoadTraits(const json::Value& t, const std::string& p, airwar::Traits& out) {
        if (!Keys(t, p, {"boldness", "methodical", "creativity", "rigidity", "ambition", "skepticism", "vindictiveness", "composure"})) return false;
        const std::pair<const char*, double*> fields[] = {{"boldness", &out.boldness},   {"methodical", &out.methodical}, {"creativity", &out.creativity},
                                                          {"rigidity", &out.rigidity},   {"ambition", &out.ambition},     {"skepticism", &out.skepticism},
                                                          {"vindictiveness", &out.vindictiveness}, {"composure", &out.composure}};
        for (const auto& f : fields) {
            if (!OptionalNumber(t, p, f.first, *f.second)) return false;
            if (*f.second < 0.0 || *f.second > 1.0) return Fail(Join(p, f.first), "0 to 1");
        }
        return true;
    }

    bool LoadCommander(const json::Value& c, const std::string& p, AdCommanderDef& out) {
        std::string role;
        if (!c.Is(json::Type::Object) || !Keys(c, p, {"id", "post", "name", "rank", "role", "bio", "to", "rhythm_h", "order_work_h", "traits", "source",
                                                      "confidence", "notes"}) ||
            !Provenance(c, p) || !RequireString(c, p, "id", out.id) || !RequireString(c, p, "post", out.post) ||
            !OptionalString(c, p, "name", out.name) || !OptionalString(c, p, "rank", out.rank) || !OptionalString(c, p, "bio", out.bio) ||
            !RequireString(c, p, "role", role) || !Ranged(c, p, "rhythm_h", out.rhythm_h) || !Valued(c, p, "order_work_h", out.order_work_h)) {
            return false;
        }
        if (role == "national") {
            out.role = AdRole::National;
        } else if (role == "troops") {
            out.role = AdRole::Troops;
        } else {
            return Fail(Join(p, "role"), "national or troops");
        }
        if (!(out.rhythm_h.lo > 0.0)) return Fail(Join(p, "rhythm_h"), "min > 0");
        if (out.order_work_h < 0.0) return Fail(Join(p, "order_work_h"), ">= 0");
        const json::Value* to = OptionalArray(c, p, "to");
        if (to == nullptr || to->items.empty()) return Fail(Join(p, "to"), "required: the posts his orders are for");
        for (std::size_t i = 0; i < to->items.size(); ++i) {
            if (!to->items[i].Is(json::Type::String) || to->items[i].string.empty()) return Fail(Index(Join(p, "to"), i), "a post id");
            out.to.push_back(to->items[i].string);
        }
        const json::Value* traits = c.Find("traits");
        if (traits == nullptr) return Fail(Join(p, "traits"), "required");
        return LoadTraits(*traits, Join(p, "traits"), out.traits);
    }
};

}  // namespace detail

inline bool LoadAdStrategyData(const std::string& data_dir, const std::string& rel, AdStrategyData& out, std::string* error = nullptr) {
    std::string text;
    json::Value root;
    std::string e;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text) || !json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "ad_strategy: cannot read " + rel + (e.empty() ? "" : ": " + e);
        return false;
    }
    detail::AdStrategyLoader loader;
    AdStrategyData d;
    if (!loader.Load(root, d)) {
        if (error != nullptr) *error = "ad_strategy: " + loader.Message();
        return false;
    }
    out = d;
    return true;
}

// ------------------------------------------------------------------------------------------------- his traits' thresholds

// His decision cycle inside his rhythm's range: a methodical man takes longer.
inline double CycleHoursOf(const AdCommanderDef& c, const AdRules& r) {
    return groundwar::ShiftedThreshold(c.rhythm_h.Mid(), c.rhythm_h, groundwar::TraitPull(c.traits.methodical), r.trait_shift);
}
// The estimate of the enemy's air front beyond the line that moves him to the offence: a bold man acts on less.
inline double OffenceDepthOf(const AdRules& r, const airwar::Traits& t) {
    return groundwar::ShiftedThreshold(r.offence_depth_m.Mid(), r.offence_depth_m, -groundwar::TraitPull(t.boldness), r.trait_shift);
}
// The decisions in a row the rule must give a sector's new priority before he changes it: one; two for a methodical man
// (he waits for a second report); one more for a rigid man while the sector still has its pre-war priority.
inline int ConfirmationsOf(const AdRules& r, const airwar::Traits& t, bool leaving_prewar) {
    return 1 + (t.methodical >= r.methodical_above ? 1 : 0) + (leaving_prewar && t.rigidity >= r.rigid_above ? 1 : 0);
}

// -------------------------------------------------------------------------------------------- the picture and the rules

// What a commander holds of a sector when he decides: all of it from facts that reached his post, and his side's plan.
struct AdSectorPicture {
    double thrust_m = 0.0;       // the deepest axis reported in it
    double pace_km_day = 0.0;    // that axis's pace
    double enemy = 0.0;          // the enemy reported in contact on its axes (formations x their believed strength)
    bool counterattack = false;  // our counterattack on one of its axes, reported or ordered since its last report
    double air_front_m = 0.0;    // the air defence staff's estimate of the enemy's air front there (F2.5; 0 none)
    double air_front_h = -1.0;   // when it was stated (-1 none)
    int objects_in_reach = 0;    // national objects of high importance within the enemy's reach of the line as reported
    int fire_units = 0;          // our SAM fire units there by our plan, less those reported wrecked and not yet repaired
};

struct AdPicture {
    double h = 0.0;
    std::vector<AdSectorPicture> sectors;
};

// The sector with the enemy's main thrust: the most enemy reported in contact (the deeper on a tie, then the first); -1
// none reported.
inline int MainThrustOf(const AdPicture& p) {
    int best = -1;
    for (std::size_t i = 0; i < p.sectors.size(); ++i) {
        const AdSectorPicture& s = p.sectors[i];
        if (!(s.enemy > 0.0)) continue;
        if (best >= 0) {
            const AdSectorPicture& b = p.sectors[static_cast<std::size_t>(best)];
            if (s.enemy < b.enemy - 1e-12 || (std::fabs(s.enemy - b.enemy) <= 1e-12 && s.thrust_m <= b.thrust_m)) continue;
        }
        best = static_cast<int>(i);
    }
    return best;
}

struct AdRuleResult {
    std::vector<SectorPriority> priority;
    std::vector<std::string> why;
};

// The rule table (04 section 2) on a picture: defend the main thrust's sector, a sector with national objects of high
// importance within the enemy's reach and one where our troops counterattack; take the offence where the staff reckons
// the enemy's air front has moved forward (recently, by at least his threshold) and our cover there is thin; economy
// everywhere else.
inline AdRuleResult RulePriorities(const AdPicture& p, const AdRules& r, const airwar::Traits& t) {
    AdRuleResult out;
    const int thrust = MainThrustOf(p);
    const double offence_m = OffenceDepthOf(r, t);
    char buf[256];
    for (std::size_t i = 0; i < p.sectors.size(); ++i) {
        const AdSectorPicture& s = p.sectors[i];
        if (static_cast<int>(i) == thrust) {
            if (s.thrust_m < 500.0) {
                std::snprintf(buf, sizeof(buf), "the enemy's main thrust is there (his formations in contact at about %.1f of full strength together, held at the line)",
                              s.enemy);
            } else {
                std::snprintf(buf, sizeof(buf),
                              "the enemy's main thrust is there (his formations in contact at about %.1f of full strength together, %.0f km in at %.0f km a day)", s.enemy,
                              s.thrust_m / 1000.0, s.pace_km_day);
            }
            out.priority.push_back(SectorPriority::Defend);
        } else if (s.objects_in_reach > 0) {
            std::snprintf(buf, sizeof(buf), "%d national object%s of high importance within the enemy's reach there", s.objects_in_reach,
                          s.objects_in_reach == 1 ? "" : "s");
            out.priority.push_back(SectorPriority::Defend);
        } else if (s.counterattack) {
            std::snprintf(buf, sizeof(buf), "our troops counterattack there");
            out.priority.push_back(SectorPriority::Defend);
        } else if (s.air_front_h >= 0.0 && s.air_front_m >= offence_m - 1e-9 && p.h - s.air_front_h <= r.offence_fresh_h && s.fire_units < r.thin_cover_units) {
            std::snprintf(buf, sizeof(buf), "the air defence staff reckons the enemy's close support works %.0f km deeper there, and we have %d fire unit%s there",
                          s.air_front_m / 1000.0, s.fire_units, s.fire_units == 1 ? "" : "s");
            out.priority.push_back(SectorPriority::Offence);
        } else {
            std::snprintf(buf, sizeof(buf), "quiet: no thrust, no national object within the enemy's reach, no air front reported moved");
            out.priority.push_back(SectorPriority::Economy);
        }
        out.why.push_back(buf);
    }
    return out;
}

// A commander's priorities: his pre-war plan, what he holds now, and the change each sector's rule has asked for and on
// how many decisions in a row.
struct AdCommanderState {
    std::vector<SectorPriority> now;
    std::vector<SectorPriority> prewar;
    std::vector<SectorPriority> pending_to;
    std::vector<int> pending_n;

    void Init(const std::vector<SectorPriority>& plan) {
        now = prewar = pending_to = plan;
        pending_n.assign(plan.size(), 0);
    }
};

struct AdChange {
    int sector = -1;  // -1 no change
    SectorPriority from = SectorPriority::Economy;
    SectorPriority to = SectorPriority::Economy;
    std::string why;
};

// One decision (04 section 2): the rule table on the picture; each sector whose rule differs from its priority counts a
// decision in a row asking for it (a different ask starts again), and may change once it has his traits' count
// (ConfirmationsOf). Of those, one changes: defend first, then the offence, then economy; the most enemy, then the most
// objects, then the first sector. Pure but for his own state.
inline AdChange DecideChange(const AdPicture& p, const AdRules& r, const airwar::Traits& t, AdCommanderState& s) {
    const AdRuleResult rule = RulePriorities(p, r, t);
    const auto rank = [](SectorPriority x) { return x == SectorPriority::Defend ? 0 : x == SectorPriority::Offence ? 1 : 2; };
    AdChange best;
    for (std::size_t i = 0; i < rule.priority.size() && i < s.now.size(); ++i) {
        const SectorPriority want = rule.priority[i];
        if (want == s.now[i]) {
            s.pending_n[i] = 0;
            s.pending_to[i] = want;
            continue;
        }
        if (s.pending_to[i] != want || s.pending_n[i] <= 0) {
            s.pending_to[i] = want;
            s.pending_n[i] = 1;
        } else {
            ++s.pending_n[i];
        }
        if (s.pending_n[i] < ConfirmationsOf(r, t, s.now[i] == s.prewar[i])) continue;
        if (best.sector >= 0) {
            const std::size_t b = static_cast<std::size_t>(best.sector);
            const AdSectorPicture& x = p.sectors[i];
            const AdSectorPicture& y = p.sectors[b];
            if (rank(want) > rank(best.to)) continue;
            if (rank(want) == rank(best.to)) {
                if (x.enemy < y.enemy - 1e-12) continue;
                if (std::fabs(x.enemy - y.enemy) <= 1e-12 && x.objects_in_reach <= y.objects_in_reach) continue;
            }
        }
        best.sector = static_cast<int>(i);
        best.from = s.now[i];
        best.to = want;
        best.why = rule.why[i];
    }
    if (best.sector >= 0) {
        const std::size_t b = static_cast<std::size_t>(best.sector);
        s.now[b] = best.to;
        s.pending_n[b] = 0;
    }
    return best;
}

// The air defence's share by sector from a commander's priorities (what F4.3's fighters and F4.2's units are given): the
// defended sectors main_share together (all of it when nothing else), the rest between the offence's and economy's
// sectors by their weights. Sums to 1 (empty for no sectors).
inline std::vector<double> PriorityShares(const std::vector<SectorPriority>& p, const AdRules& r) {
    std::vector<double> out(p.size(), 0.0);
    int defend = 0;
    double rest_weight = 0.0;
    for (SectorPriority x : p) {
        if (x == SectorPriority::Defend) {
            ++defend;
        } else {
            rest_weight += x == SectorPriority::Offence ? r.offence_weight : r.economy_weight;
        }
    }
    if (p.empty()) return out;
    const double defend_share = defend == 0 ? 0.0 : (rest_weight > 0.0 ? r.main_share : 1.0);
    const double rest = 1.0 - defend_share;
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (p[i] == SectorPriority::Defend) {
            out[i] = defend_share / static_cast<double>(defend);
        } else if (rest_weight > 0.0) {
            out[i] = rest * (p[i] == SectorPriority::Offence ? r.offence_weight : r.economy_weight) / rest_weight;
        } else {
            out[i] = rest / static_cast<double>(p.size() - static_cast<std::size_t>(defend));
        }
    }
    return out;
}

// ------------------------------------------------------------------------------------------------------- the commanders

// One change in a commander's post's records: what he decided, when, why, on which facts, and the order that carries it.
struct AdDecision {
    int commander = -1;
    double h = 0.0;  // decided (the order leaves order_work_h later)
    int sector = -1;
    SectorPriority from = SectorPriority::Economy;
    SectorPriority to = SectorPriority::Economy;
    std::string why;
    std::vector<int> basis;       // the facts it rested on (the net's indices), each held by his post by h
    std::vector<double> basis_h;  // when each reached his post
    std::string key;              // the order's fact
    int fact = -1;
};

class AdStrategy {
public:
    void Bind(command::InfoNet* net) {
        net_ = net;
        Reset();
    }
    // Other commanders or rules than data/campaign/ad_strategy.json's (call after Bind, before the first Pump).
    void Use(const AdStrategyData& d) {
        data_ = d;
        given_ = true;
        Reset();
    }

    // On: ad_strategy.mode 1, the front has a line and the chain the commanders' posts (decided once, at the first pump).
    bool On() const { return on_; }
    const std::string& Error() const { return error_; }
    const AdStrategyData& Data() const { return data_; }
    std::size_t Commanders() const { return runs_.size(); }
    int CommanderIndex(const std::string& id) const {
        for (std::size_t i = 0; i < data_.commanders.size() && i < runs_.size(); ++i) {
            if (data_.commanders[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }
    const AdCommanderDef& Def(int k) const { return data_.commanders[static_cast<std::size_t>(k)]; }
    const std::vector<SectorPriority>& Priorities(int k) const { return runs_[static_cast<std::size_t>(k)].state.now; }
    const std::vector<SectorPriority>& PreWar(int k) const { return runs_[static_cast<std::size_t>(k)].state.prewar; }
    double CycleH(int k) const { return runs_[static_cast<std::size_t>(k)].cycle_h; }
    // The decision hours at which something new had reached his post (each read the table; Decisions() are the changes).
    int Reviews(int k) const { return runs_[static_cast<std::size_t>(k)].reviews; }
    const std::vector<AdDecision>& Decisions() const { return decisions_; }
    std::vector<double> Shares(int k) const { return PriorityShares(Priorities(k), data_.rules); }
    // What commander k held at his last decision hour, as his picture at hour t (a debrief's, a bench's).
    AdPicture Held(int k, double t) const { return Picture(runs_[static_cast<std::size_t>(k)], t); }
    // A sector's priority as a post knows it from commander k now: the latest of his orders on it that reached the post
    // (its hour in *h), else his pre-war plan (*h -1). What the regiment's tasking reads (F4.4).
    SectorPriority PriorityKnownBy(int k, int position, int sector, double* h = nullptr) const {
        if (h != nullptr) *h = -1.0;
        if (net_ == nullptr || k < 0 || static_cast<std::size_t>(k) >= runs_.size() || sector < 0) return SectorPriority::Economy;
        for (std::size_t i = decisions_.size(); i-- > 0;) {
            const AdDecision& d = decisions_[i];
            if (d.commander != k || d.sector != sector || d.fact < 0) continue;
            const command::Delivery* got = net_->Known(position, d.fact);
            if (got == nullptr) continue;
            if (h != nullptr) *h = got->h;
            return d.to;
        }
        const std::vector<SectorPriority>& plan = PreWar(k);
        return static_cast<std::size_t>(sector) < plan.size() ? plan[static_cast<std::size_t>(sector)] : SectorPriority::Economy;
    }

    // Each commander's decisions due by now, on what reached his post: after the feed added the campaign's facts and
    // brought the net up to the campaign's hour. True when an order was added (the net is to be advanced again).
    bool Pump(const airwar::AirCampaign& c, const command::GroundFeed& g, const command::AirFrontWatch& w) {
        if (net_ == nullptr || !Ready(c)) return false;
        if (next_any_h_ > c.Now()) return false;
        double watermark_h = c.Now();
        for (const airwar::Package& p : c.Packages()) {
            if (!p.resolved) watermark_h = std::min(watermark_h, p.FirstH());
        }
        ScanWrecks();
        for (std::size_t i = conclusion_fact_.size(); i < w.Conclusions().size(); ++i) {
            conclusion_fact_.push_back(w.Conclusions()[i].key.empty() ? -1 : net_->FactIndex(w.Conclusions()[i].key));
        }
        bool added = false;
        for (;;) {
            std::size_t k = runs_.size();
            for (std::size_t i = 0; i < runs_.size(); ++i) {
                if (runs_[i].post < 0) continue;
                if (k == runs_.size() || runs_[i].next_h < runs_[k].next_h) k = i;
            }
            if (k == runs_.size() || !(runs_[k].next_h < watermark_h - 1e-9)) break;
            const double t = runs_[k].next_h;
            added = Review(static_cast<int>(k), t, c, g, w) || added;
            runs_[k].next_h = t + runs_[k].cycle_h;
        }
        next_any_h_ = 1e18;
        for (const Run& r : runs_) {
            if (r.post >= 0) next_any_h_ = std::min(next_any_h_, r.next_h);
        }
        return added;
    }

private:
    struct AxisHeld {
        bool has = false;
        double h = -1.0;  // compiled
        groundwar::SitAxis axis;
        int fact = -1;
        double arrived_h = 0.0;
        double counter_h = -1.0;  // our counterattack ordered on it (the order's hour), -1 none
        int counter_fact = -1;
        double counter_arrived_h = 0.0;
    };
    struct FrontHeld {
        double depth_m = 0.0;
        double h = -1.0;
        int fact = -1;
        double arrived_h = 0.0;
    };
    struct LostHeld {
        double until_h = -1.0;
        int fact = -1;
        double arrived_h = 0.0;
    };
    struct Run {
        int post = -1;
        double cycle_h = 3.0;
        double next_h = 0.0;
        AdCommanderState state;
        int reviews = 0;
        int orders = 0;
        // What has not reached his post yet, by source (indices into the source's list), and the cursors past what was listed.
        std::vector<int> reports, summaries, orders_in, conclusions, wrecks;
        std::size_t reports_seen = 0, summaries_seen = 0, orders_seen = 0, conclusions_seen = 0, wrecks_seen = 0;
        // What he holds.
        std::vector<AxisHeld> axes;      // by the ground war's axis index
        std::vector<FrontHeld> fronts;   // by sector
        std::vector<LostHeld> lost;      // by SAM fire unit (sams_)
        // The newest fact that reached him in this review.
        int prompt_fact = -1;
        double prompt_h = -1.0;
    };
    struct PlanObject {
        int sector = -1;
        double base_depth_m = 0.0;  // beyond the front's base line, into our country
    };
    struct SamUnit {
        int sector = -1;
        double repair_h = 24.0;
    };
    struct Wreck {
        int fact = -1;
        int sam = -1;
        double h = 0.0;
    };

    command::InfoNet* net_ = nullptr;
    AdStrategyData data_;
    bool given_ = false;
    bool ready_ = false;
    bool on_ = false;
    std::string error_;
    std::vector<Run> runs_;
    std::vector<AdDecision> decisions_;
    std::vector<PlanObject> objects_;
    std::vector<SamUnit> sams_;
    std::map<std::string, int> sam_by_id_;
    std::vector<Wreck> wrecks_;
    std::size_t facts_scanned_ = 0;
    std::vector<int> conclusion_fact_;
    std::vector<std::string> sector_ids_, sector_names_;
    double next_any_h_ = 0.0;

    void Reset() {
        ready_ = on_ = false;
        error_.clear();
        runs_.clear();
        decisions_.clear();
        objects_.clear();
        sams_.clear();
        sam_by_id_.clear();
        wrecks_.clear();
        facts_scanned_ = 0;
        conclusion_fact_.clear();
        sector_ids_.clear();
        sector_names_.clear();
        next_any_h_ = 0.0;
    }

    // On at ad_strategy.mode 1 on a front with a line, with the data and the commanders' posts; decided once. The
    // pre-war plan is the rule table on what each holds before the war: his side's plan alone.
    bool Ready(const airwar::AirCampaign& c) {
        if (ready_) return on_;
        ready_ = true;
        if (c.Data().tuning.Get("ad_strategy.mode") < 1.0 || c.FrontLine().Empty()) return false;
        if (!given_ && !LoadAdStrategyData(c.Data().data_dir, "campaign/ad_strategy.json", data_, &error_)) return false;
        const command::ChainData& d = net_->Data();
        const airwar::LineOfContact& line = c.FrontLine();
        const std::size_t n = line.SectorCount();
        for (std::size_t i = 0; i < n; ++i) {
            sector_ids_.push_back(line.Sector(i).id);
            sector_names_.push_back(line.Sector(i).name.empty() ? line.Sector(i).id : line.Sector(i).name);
        }
        const std::vector<airwar::TheatreObject>& objs = c.Data().theatre.objects;
        for (const airwar::TheatreObject& o : objs) {
            if (o.decoy) continue;
            if (o.kind == airwar::ObjectKind::SamSite) {
                SamUnit u;
                u.sector = line.SectorOf(o.pos);
                u.repair_h = o.repair_h;
                sam_by_id_[o.id] = static_cast<int>(sams_.size());
                sams_.push_back(u);
            } else if (data_.rules.National(o.kind) && o.value >= data_.rules.object_value) {
                const airwar::FrontPlace pl = line.Place(o.pos);
                PlanObject po;
                po.sector = line.SectorAt(pl.along_m);
                po.base_depth_m = pl.side * pl.distance_m;
                objects_.push_back(po);
            }
        }
        int posts = 0;
        for (const AdCommanderDef& def : data_.commanders) {
            Run r;
            r.post = d.PositionIndex(def.post);
            for (const std::string& to : def.to) {
                if (d.PositionIndex(to) < 0) r.post = -1;
            }
            r.cycle_h = CycleHoursOf(def, data_.rules);
            r.next_h = r.cycle_h;
            r.fronts.assign(n, FrontHeld());
            r.lost.assign(sams_.size(), LostHeld());
            AdPicture pre = Picture(r, 0.0);
            r.state.Init(RulePriorities(pre, data_.rules, def.traits).priority);
            posts += r.post >= 0 ? 1 : 0;
            runs_.push_back(std::move(r));
        }
        if (posts == 0) {
            error_ = "ad_strategy: none of the commanders' posts (or the posts their orders are for) is in the chain";
            return false;
        }
        on_ = true;
        return true;
    }

    // The wrecks of our SAM sites reported in the pipeline (the feed's OwnDamage facts), from the facts added since the last
    // scan.
    void ScanWrecks() {
        const std::vector<command::Fact>& facts = net_->Facts();
        for (; facts_scanned_ < facts.size(); ++facts_scanned_) {
            const command::Fact& f = facts[facts_scanned_];
            if (f.kind != command::FactKind::OwnDamage) continue;
            const auto it = sam_by_id_.find(f.subject);
            if (it == sam_by_id_.end()) continue;
            Wreck w;
            w.fact = static_cast<int>(facts_scanned_);
            w.sam = it->second;
            w.h = f.h;
            wrecks_.push_back(w);
        }
    }

    // Has his post the fact by hour t? Its arrival in *at.
    bool HeldBy(const Run& r, int fact, double t, double* at) const {
        if (fact < 0) return false;
        const command::Delivery* d = net_->Known(r.post, fact);
        if (d == nullptr || d->h > t + 1e-9) return false;
        *at = d->h;
        return true;
    }

    void Prompt(Run& r, int fact, double at) {
        if (at > r.prompt_h || (at == r.prompt_h && fact > r.prompt_fact)) {
            r.prompt_h = at;
            r.prompt_fact = fact;
        }
    }

    AxisHeld& AxisAt(Run& r, int axis) {
        if (static_cast<std::size_t>(axis) >= r.axes.size()) r.axes.resize(static_cast<std::size_t>(axis) + 1);
        return r.axes[static_cast<std::size_t>(axis)];
    }

    void HoldAxis(Run& r, const groundwar::SitAxis& a, double compiled_h, int fact, double arrived_h) {
        if (a.axis < 0) return;
        AxisHeld& x = AxisAt(r, a.axis);
        if (x.has && compiled_h < x.h) return;
        x.has = true;
        x.h = compiled_h;
        x.axis = a;
        x.fact = fact;
        x.arrived_h = arrived_h;
    }

    // Move what reached his post by t from his lists into what he holds; true when anything did.
    bool Take(Run& r, double t, const airwar::AirCampaign& c, const command::GroundFeed& g, const command::AirFrontWatch& w) {
        bool fresh = false;
        double at = 0.0;
        // Our units' reports (those that pass his post) and the army's summaries.
        const std::vector<command::SituationReport>& reps = g.Reports();
        for (; r.reports_seen < reps.size(); ++r.reports_seen) {
            if (!reps[r.reports_seen].axes.empty()) r.reports.push_back(static_cast<int>(r.reports_seen));
        }
        std::size_t keep = 0;
        for (int i : r.reports) {
            const command::SituationReport& s = reps[static_cast<std::size_t>(i)];
            if (HeldBy(r, s.fact, t, &at)) {
                for (const groundwar::SitAxis& a : s.axes) HoldAxis(r, a, s.h, s.fact, at);
                Prompt(r, s.fact, at);
                fresh = true;
            } else if (s.h >= t - data_.rules.stale_h) {
                r.reports[keep++] = i;
            }
        }
        r.reports.resize(keep);
        const std::vector<command::ArmySummary>& sums = g.Summaries();
        for (; r.summaries_seen < sums.size(); ++r.summaries_seen) r.summaries.push_back(static_cast<int>(r.summaries_seen));
        keep = 0;
        for (int i : r.summaries) {
            const command::ArmySummary& s = sums[static_cast<std::size_t>(i)];
            if (HeldBy(r, s.fact, t, &at)) {
                for (std::size_t k = 0; k < s.axes.size() && k < s.axes_h.size(); ++k) HoldAxis(r, s.axes[k], s.axes_h[k], s.fact, at);
                Prompt(r, s.fact, at);
                fresh = true;
            } else if (s.h >= t - data_.rules.stale_h) {
                r.summaries[keep++] = i;
            }
        }
        r.summaries.resize(keep);
        // Our counterattack orders.
        const std::vector<command::OrderRecord>& ords = g.Orders();
        for (; r.orders_seen < ords.size(); ++r.orders_seen) {
            const command::OrderRecord& o = ords[r.orders_seen];
            if (o.order == groundwar::GroundOrder::CounterattackOrdered && o.axis >= 0) r.orders_in.push_back(static_cast<int>(r.orders_seen));
        }
        keep = 0;
        for (int i : r.orders_in) {
            const command::OrderRecord& o = ords[static_cast<std::size_t>(i)];
            if (HeldBy(r, o.fact, t, &at)) {
                AxisHeld& x = AxisAt(r, o.axis);
                if (o.h >= x.counter_h) {
                    x.counter_h = o.h;
                    x.counter_fact = o.fact;
                    x.counter_arrived_h = at;
                    // The sector of the axis: the theatre's (the plan's), until a report on it says.
                    if (!x.has && c.Ground() != nullptr && static_cast<std::size_t>(o.axis) < c.Ground()->Axes().size()) {
                        x.axis.axis = o.axis;
                        x.axis.sector = c.Ground()->Axes()[static_cast<std::size_t>(o.axis)].sector;
                    }
                }
                Prompt(r, o.fact, at);
                fresh = true;
            } else if (o.h >= t - data_.rules.stale_h) {
                r.orders_in[keep++] = i;
            }
        }
        r.orders_in.resize(keep);
        // The air defence staff's estimates of the enemy's air front.
        for (; r.conclusions_seen < conclusion_fact_.size(); ++r.conclusions_seen) r.conclusions.push_back(static_cast<int>(r.conclusions_seen));
        keep = 0;
        for (int i : r.conclusions) {
            const command::AirFrontConclusion& k = w.Conclusions()[static_cast<std::size_t>(i)];
            const int fact = conclusion_fact_[static_cast<std::size_t>(i)];
            if (HeldBy(r, fact, t, &at)) {
                if (k.sector >= 0 && static_cast<std::size_t>(k.sector) < r.fronts.size() && k.h >= r.fronts[static_cast<std::size_t>(k.sector)].h) {
                    FrontHeld& f = r.fronts[static_cast<std::size_t>(k.sector)];
                    f.depth_m = k.depth_m;
                    f.h = k.h;
                    f.fact = fact;
                    f.arrived_h = at;
                }
                Prompt(r, fact, at);
                fresh = true;
            } else if (fact >= 0 && k.h >= t - data_.rules.stale_h) {
                r.conclusions[keep++] = i;
            }
        }
        r.conclusions.resize(keep);
        // Our SAM sites reported wrecked: out until their repair time is past.
        for (; r.wrecks_seen < wrecks_.size(); ++r.wrecks_seen) r.wrecks.push_back(static_cast<int>(r.wrecks_seen));
        keep = 0;
        for (int i : r.wrecks) {
            const Wreck& wk = wrecks_[static_cast<std::size_t>(i)];
            if (HeldBy(r, wk.fact, t, &at)) {
                LostHeld& l = r.lost[static_cast<std::size_t>(wk.sam)];
                const double until = wk.h + sams_[static_cast<std::size_t>(wk.sam)].repair_h;
                if (until > l.until_h) {
                    l.until_h = until;
                    l.fact = wk.fact;
                    l.arrived_h = at;
                }
                Prompt(r, wk.fact, at);
                fresh = true;
            } else if (wk.h >= t - data_.rules.stale_h) {
                r.wrecks[keep++] = i;
            }
        }
        r.wrecks.resize(keep);
        return fresh;
    }

    // His picture at hour t from what he holds and his side's plan.
    AdPicture Picture(const Run& r, double t) const {
        AdPicture p;
        p.h = t;
        p.sectors.assign(sector_ids_.size(), AdSectorPicture());
        const std::size_t n = p.sectors.size();
        for (const AxisHeld& x : r.axes) {
            const int s = x.axis.sector;
            if (s < 0 || static_cast<std::size_t>(s) >= n) continue;
            AdSectorPicture& sp = p.sectors[static_cast<std::size_t>(s)];
            if (x.has) {
                if (x.axis.depth_m > sp.thrust_m) {
                    sp.thrust_m = x.axis.depth_m;
                    sp.pace_km_day = x.axis.pace_km_day;
                }
                sp.enemy += static_cast<double>(x.axis.enemy_n) * x.axis.enemy_fraction;
                sp.counterattack = sp.counterattack || x.axis.counterattacked;
            }
            if (x.counter_h >= 0.0 && (!x.has || x.counter_h > x.h)) sp.counterattack = true;
        }
        for (std::size_t s = 0; s < n && s < r.fronts.size(); ++s) {
            if (r.fronts[s].h < 0.0) continue;
            p.sectors[s].air_front_m = r.fronts[s].depth_m;
            p.sectors[s].air_front_h = r.fronts[s].h;
        }
        for (const PlanObject& o : objects_) {
            if (o.sector < 0 || static_cast<std::size_t>(o.sector) >= n) continue;
            AdSectorPicture& sp = p.sectors[static_cast<std::size_t>(o.sector)];
            if (o.base_depth_m - sp.thrust_m <= data_.rules.object_reach_m) ++sp.objects_in_reach;
        }
        for (std::size_t i = 0; i < sams_.size(); ++i) {
            const int s = sams_[i].sector;
            if (s < 0 || static_cast<std::size_t>(s) >= n) continue;
            if (i < r.lost.size() && r.lost[i].until_h > t) continue;
            ++p.sectors[static_cast<std::size_t>(s)].fire_units;
        }
        return p;
    }

    // The facts a sector's picture rests on, with when each reached him, and the one that prompted this review.
    void Basis(const Run& r, int sector, double t, AdDecision& out) const {
        const auto add = [&out](int fact, double at) {
            if (fact < 0 || std::find(out.basis.begin(), out.basis.end(), fact) != out.basis.end()) return;
            out.basis.push_back(fact);
            out.basis_h.push_back(at);
        };
        for (const AxisHeld& x : r.axes) {
            if (x.axis.sector != sector) continue;
            if (x.has) add(x.fact, x.arrived_h);
            if (x.counter_h >= 0.0) add(x.counter_fact, x.counter_arrived_h);
        }
        if (static_cast<std::size_t>(sector) < r.fronts.size()) add(r.fronts[static_cast<std::size_t>(sector)].fact, r.fronts[static_cast<std::size_t>(sector)].arrived_h);
        for (std::size_t i = 0; i < sams_.size() && i < r.lost.size(); ++i) {
            if (sams_[i].sector == sector && r.lost[i].until_h > t) add(r.lost[i].fact, r.lost[i].arrived_h);
        }
        add(r.prompt_fact, r.prompt_h);
    }

    static const char* PriorityOrderText(SectorPriority p) {
        switch (p) {
            case SectorPriority::Defend:
                return "to be defended: the main effort of the air defence there, the main thrust and the national objects covered";
            case SectorPriority::Offence:
                return "for the offence: mobile SAMs forward without announcement and under emission discipline to where the enemy's low-level aircraft now fly, the fighters given the sector";
            case SectorPriority::Economy:
                return "an economy sector: at least one fire unit in each object's cover, the rest to the other sectors";
        }
        return "";
    }

    std::string Title(int k) const {
        const AdCommanderDef& def = Def(k);
        const command::Position& pos = net_->Data().positions[static_cast<std::size_t>(runs_[static_cast<std::size_t>(k)].post)];
        const std::string name = def.name.empty() ? pos.holder.name : def.name;
        const std::string rank = def.name.empty() ? pos.holder.rank : def.rank;
        const std::string who = def.role == AdRole::National ? "The national air defence sector's commander" : "The army's chief of air defence";
        return who + (name.empty() ? std::string() : ", " + (rank.empty() ? std::string() : rank + " ") + name);
    }

    // A decision hour of commander k: take what reached his post by t; with anything new, read the table, change at most
    // one sector and send the order. True when an order was added.
    bool Review(int k, double t, const airwar::AirCampaign& c, const command::GroundFeed& g, const command::AirFrontWatch& w) {
        Run& r = runs_[static_cast<std::size_t>(k)];
        r.prompt_fact = -1;
        r.prompt_h = -1.0;
        if (!Take(r, t, c, g, w)) return false;
        ++r.reviews;
        const AdCommanderDef& def = Def(k);
        const AdPicture p = Picture(r, t);
        const AdChange ch = DecideChange(p, data_.rules, def.traits, r.state);
        if (ch.sector < 0) return false;
        AdDecision dec;
        dec.commander = k;
        dec.h = t;
        dec.sector = ch.sector;
        dec.from = ch.from;
        dec.to = ch.to;
        dec.why = ch.why;
        Basis(r, ch.sector, t, dec);
        const std::string& sector = sector_names_[static_cast<std::size_t>(ch.sector)];
        command::Fact f;
        f.key = "ad_order:" + def.id + ":" + std::to_string(++r.orders);
        f.kind = command::FactKind::Order;
        f.h = t + def.order_work_h;
        f.origin = def.post;
        f.subject = "ad_priority:" + sector_ids_[static_cast<std::size_t>(ch.sector)];
        f.text = command::FormatClock(f.h, c.Data().theatre.start_hour) + ". " + Title(k) + ": " + sector + " is " + PriorityOrderText(ch.to) +
                 " (it was " + SectorPriorityName(ch.from) + "). Because " + ch.why + ".";
        f.summary = Title(k) + ": " + sector + " is now " + (ch.to == SectorPriority::Defend ? "to be defended" : ch.to == SectorPriority::Offence ? "for the offence" : "an economy sector") +
                    " (the order follows).";
        f.to = def.to;
        f.precedence = 1;
        f.document = true;
        dec.key = f.key;
        dec.fact = net_->AddFact(f);
        decisions_.push_back(std::move(dec));
        return true;
    }
};

}  // namespace campaign
}  // namespace simcore
