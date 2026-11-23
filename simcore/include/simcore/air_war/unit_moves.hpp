// simcore/air_war/unit_moves.hpp — our SAM units' and radars' moves on orders (track F, F4.2;
// docs/fronts/04_AIR_DEFENCE_STRATEGY.md section 3).
//
// Under ad_strategy.mode 1 the national SAM units (S-75 and S-125 class), the army's Kub and the radar companies move
// only on an order their commander heard through our pipeline (campaign/ad_strategy.hpp plans and sends them; the
// unit's post hears them; the campaign runs the move). A move is whole, by the norms of its class
// (data/campaign/ad_strategy.json "moves"): packed, marched on the roads (the route service, nav/routes.hpp, cached), set
// up and checked; off the air from the first hour of packing to the end of its checks, and at no place while on the road.
// Here: the classes and their norms, a move as the campaign ran it, an order as the unit heard it, and the loader.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// A class of unit and the norms it moves by.
struct MoveClassDef {
    std::string id;
    std::string name;
    std::vector<std::string> systems;  // the defence systems it takes (a SAM site's defence.system)
    std::vector<ObjectKind> kinds;     // the object kinds it takes besides (the radar companies)
    std::vector<std::string> except;   // objects it does not take (the SAM regiment's own radar: the regiment moves it)
    bool national = true;              // whose orders move it: the national sector's commander, else the army's chief
    double pack_h = 2.5;               // packing up before the march
    double setup_h = 3.0;              // setting up at the new position
    double checks_h = 2.25;            // checking and adjusting the equipment after it (the range's middle)
    double road_kmh = 7.4;             // the march on the roads, by day
    double night_kmh = 7.4;            // by night (the march's start decides)
    double cross_kmh = 4.0;            // off the road, to and from it
    int max_moves_day = 3;             // orders to move in any 24 h at most
    bool leapfrog = false;             // a unit of the class waits while another of it in its sector is on the move
    bool cover = false;                // sent to cover a sector its commander gives the defence (or the offence's sector)
    bool offence = false;              // sent forward into a sector its commander gives the offence (an ambush)
    double withdraw_within_m = 20000.0;  // ordered back when the line as reported is this close
    double withdraw_to_m = 40000.0;      // to this far behind it
    double forward_behind_m = 30000.0;   // sent into a sector, it stands this far behind the line as reported

    double FixedHours() const { return pack_h + setup_h + checks_h; }
};

struct UnitMoveRules {
    std::vector<MoveClassDef> classes;
    int moves_per_review = 1;   // cover and offence moves a commander orders at one decision hour (withdrawals besides)
    double spacing_m = 10000.0; // units sent into one sector stand this far apart along it
    double road_factor = 1.3;   // no way on the roads: the straight line this much longer, at the road speed
    double no_road_near_m = 6000.0;  // a way off the country's roads longer than this is on local roads it does not draw
    double detour_max = 3.0;    // a way on the roads longer than this many straight lines is not taken

    // The class an object moves by (-1: it does not move on orders). Not decoys or the player's battery.
    int ClassOf(const TheatreObject& o) const {
        if (o.decoy || o.player) return -1;
        for (std::size_t i = 0; i < classes.size(); ++i) {
            const MoveClassDef& c = classes[i];
            bool except = false;
            for (const std::string& x : c.except) except = except || x == o.id;
            if (except) continue;
            for (const std::string& s : c.systems) {
                if (o.kind == ObjectKind::SamSite && o.defence.present && o.defence.system == s) return static_cast<int>(i);
            }
            for (ObjectKind k : c.kinds) {
                if (o.kind == k) return static_cast<int>(i);
            }
        }
        return -1;
    }
};

// A move as the campaign ran it (AirCampaign::UnitMoves).
struct UnitMove {
    std::string key;       // the order's (its fact in our pipeline), or the caller's
    int object = -1;       // the theatre object
    int cls = -1;          // its class
    Vec3 from;
    Vec3 to;
    double heard_h = 0.0;  // when its commander had the order
    double start_h = 0.0;  // packing from here (the campaign's whole hour at or after heard_h)
    double depart_h = 0.0; // on the road
    double arrive_h = 0.0; // at the new position: setting up, then the checks
    double ready_h = 0.0;  // ready to fire (to radiate) again
    double road_km = 0.0;  // the march on the roads, and off them
    double cross_km = 0.0;
    bool on_roads = false; // the route service found a way (else the straight line x road_factor)
    int stage = 0;         // 0 packing, 1 on the road, 2 setting up and checks, 3 ready; -1 overrun before it got away; -2 wrecked on the way
    bool escaped = false;  // the line of contact later passed where it had stood, after it had left
    double end_h = -1.0;   // when it ended (ready, overrun or wrecked)

    double MarchHours() const { return arrive_h - depart_h; }
    // Off the air between from_h and to_h: from packing to ready (or its end).
    double OfflineHours(double now_h) const {
        const double end = end_h >= 0.0 ? end_h : now_h;
        return end > start_h ? end - start_h : 0.0;
    }
};

// An order to move as the unit's commander heard it: our pipeline's (campaign/ad_strategy.hpp), or a caller's. With
// fighter_shares (track F, F4.3) it is no move: the fighter regiment's duty by sector as its post now holds its commander's
// priorities, the share of the front's fighters each sector is given (`unit` the regiment's post).
struct UnitOrder {
    std::string key;
    std::string unit;   // the theatre object's id
    Vec3 to;
    double heard_h = 0.0;
    std::vector<double> fighter_shares;
};

// Our fighters by sector (track F, F4.3; docs/fronts/04_AIR_DEFENCE_STRATEGY.md section 5; data/campaign/ad_strategy.json
// "fighters"): whose priorities share them, the post where the fighter regiment hears his orders, and the campaign's values.
struct FighterCoverRules {
    bool present = false;
    std::string post = "fighter_rgt_duty";      // the fighter regiment's duty officer's post
    std::string commander = "sector_commander"; // the commander whose orders it takes (campaign/ad_strategy.hpp's id)
    bool prewar_held = true;                    // it holds his pre-war plan from the war's start (the plan of cooperation)
    double cap = 1.0;                           // a sector's cover at most (the front's number is a threat, 0..1)
    double belief_learn = 0.5;                  // the enemy's belief of a sector's cover moves this share of the way to it
                                                // with each of its packages debriefed there
};

class AirCampaign;
// Who tells the campaign the orders its units heard (command::CampaignFeed::OrderSource): called at the campaign's whole
// hours from `*next_h` on, it lists the orders heard by then and not listed before, and sets when to ask it next.
using UnitOrderSource = std::function<void(const AirCampaign&, std::vector<UnitOrder>&, double* next_h)>;

namespace detail {

class UnitMoveLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& m, UnitMoveRules& out) {
        const std::string p = "moves";
        if (!Keys(m, p, {"moves_per_review", "spacing_m", "road_factor", "no_road_near_m", "detour_max", "classes", "notes"})) return false;
        double per = 0.0;
        if (!Valued(m, p, "moves_per_review", per) || !Valued(m, p, "spacing_m", out.spacing_m) || !Valued(m, p, "road_factor", out.road_factor) ||
            !Valued(m, p, "no_road_near_m", out.no_road_near_m) || !Valued(m, p, "detour_max", out.detour_max)) {
            return false;
        }
        out.moves_per_review = static_cast<int>(per);
        if (static_cast<double>(out.moves_per_review) != per || per < 0.0) return Fail(Join(p, "moves_per_review"), "a whole number >= 0");
        if (out.road_factor < 1.0) return Fail(Join(p, "road_factor"), ">= 1");
        if (out.detour_max < 1.0) return Fail(Join(p, "detour_max"), ">= 1");
        const json::Value* list = OptionalArray(m, p, "classes");
        if (list == nullptr || list->items.empty()) return Fail(Join(p, "classes"), "required: at least one class");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            MoveClassDef c;
            if (!LoadClass(list->items[i], Index(Join(p, "classes"), i), c)) return false;
            for (const MoveClassDef& o : out.classes) {
                if (o.id == c.id) return Fail(Index(Join(p, "classes"), i), "duplicate id '" + c.id + "'");
            }
            out.classes.push_back(c);
        }
        return true;
    }

private:
    bool Valued(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        const std::string q = Join(path, key);
        return Keys(*v, q, {"value", "source", "confidence", "notes"}) && Provenance(*v, q) && NumberOf(*v, q, "value", out);
    }
    // {"min": a, "max": b, ...} as its middle, or {"value": x, ...}.
    bool ValuedOrRange(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        if (v->Find("value") != nullptr) return Valued(obj, path, key, out);
        const std::string q = Join(path, key);
        double lo = 0.0, hi = 0.0;
        if (!Keys(*v, q, {"min", "max", "source", "confidence", "notes"}) || !Provenance(*v, q) || !NumberOf(*v, q, "min", lo) || !NumberOf(*v, q, "max", hi)) {
            return false;
        }
        if (hi < lo) return Fail(q, "max >= min");
        out = 0.5 * (lo + hi);
        return true;
    }
    bool Strings(const json::Value& obj, const std::string& path, const char* key, std::vector<std::string>& out) {
        const json::Value* a = OptionalArray(obj, path, key);
        if (a == nullptr) return true;
        for (std::size_t i = 0; i < a->items.size(); ++i) {
            if (!a->items[i].Is(json::Type::String) || a->items[i].string.empty()) return Fail(Index(Join(path, key), i), "a string");
            out.push_back(a->items[i].string);
        }
        return true;
    }

    bool LoadClass(const json::Value& c, const std::string& p, MoveClassDef& out) {
        std::string commander;
        std::vector<std::string> kinds;
        double max_day = 0.0;
        if (!c.Is(json::Type::Object) ||
            !Keys(c, p, {"id", "name", "systems", "kinds", "except", "commander", "pack_h", "setup_h", "checks_h", "road_kmh", "night_kmh", "cross_kmh",
                         "max_moves_day", "leapfrog", "cover", "offence", "withdraw_within_m", "withdraw_to_m", "forward_behind_m", "source",
                         "confidence", "notes"}) ||
            !Provenance(c, p) || !RequireString(c, p, "id", out.id) || !OptionalString(c, p, "name", out.name) || !Strings(c, p, "systems", out.systems) ||
            !Strings(c, p, "kinds", kinds) || !Strings(c, p, "except", out.except) || !RequireString(c, p, "commander", commander) ||
            !ValuedOrRange(c, p, "pack_h", out.pack_h) || !ValuedOrRange(c, p, "setup_h", out.setup_h) || !ValuedOrRange(c, p, "checks_h", out.checks_h) ||
            !ValuedOrRange(c, p, "road_kmh", out.road_kmh) || !ValuedOrRange(c, p, "night_kmh", out.night_kmh) ||
            !ValuedOrRange(c, p, "cross_kmh", out.cross_kmh) || !Valued(c, p, "max_moves_day", max_day) || !OptionalBool(c, p, "leapfrog", out.leapfrog) ||
            !OptionalBool(c, p, "cover", out.cover) || !OptionalBool(c, p, "offence", out.offence) ||
            !Valued(c, p, "withdraw_within_m", out.withdraw_within_m) || !Valued(c, p, "withdraw_to_m", out.withdraw_to_m) ||
            !Valued(c, p, "forward_behind_m", out.forward_behind_m)) {
            return false;
        }
        if (commander == "national") {
            out.national = true;
        } else if (commander == "troops") {
            out.national = false;
        } else {
            return Fail(Join(p, "commander"), "national or troops");
        }
        for (std::size_t i = 0; i < kinds.size(); ++i) {
            ObjectKind k = ObjectKind::Depot;
            if (!ParseObjectKind(kinds[i], k)) return Fail(Index(Join(p, "kinds"), i), "an object kind");
            out.kinds.push_back(k);
        }
        if (out.systems.empty() && out.kinds.empty()) return Fail(p, "systems or kinds: what the class takes");
        out.max_moves_day = static_cast<int>(max_day);
        if (static_cast<double>(out.max_moves_day) != max_day || max_day < 1.0) return Fail(Join(p, "max_moves_day"), "a whole number >= 1");
        if (out.pack_h < 0.0 || out.setup_h < 0.0 || out.checks_h < 0.0) return Fail(p, "pack_h, setup_h and checks_h >= 0");
        if (!(out.road_kmh > 0.0) || !(out.night_kmh > 0.0) || !(out.cross_kmh > 0.0)) return Fail(p, "the speeds > 0");
        if (!(out.withdraw_to_m > out.withdraw_within_m) || !(out.forward_behind_m > out.withdraw_within_m)) {
            return Fail(p, "withdraw_to_m and forward_behind_m beyond withdraw_within_m (else a unit sent there is withdrawn at once)");
        }
        return true;
    }
};

class FighterCoverLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& f, FighterCoverRules& out) {
        const std::string p = "fighters";
        if (!f.Is(json::Type::Object) || !Keys(f, p, {"post", "commander", "prewar_held", "cap", "belief_learn", "source", "confidence", "notes"}) ||
            !Provenance(f, p) || !RequireString(f, p, "post", out.post) || !RequireString(f, p, "commander", out.commander) ||
            !OptionalBool(f, p, "prewar_held", out.prewar_held) || !Valued(f, p, "cap", out.cap) || !Valued(f, p, "belief_learn", out.belief_learn)) {
            return false;
        }
        if (!(out.cap > 0.0) || out.cap > 1.0) return Fail(Join(p, "cap"), "in (0, 1]");
        if (out.belief_learn < 0.0 || out.belief_learn > 1.0) return Fail(Join(p, "belief_learn"), "in [0, 1]");
        out.present = true;
        return true;
    }

private:
    bool Valued(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        const std::string q = Join(path, key);
        return Keys(*v, q, {"value", "source", "confidence", "notes"}) && Provenance(*v, q) && NumberOf(*v, q, "value", out);
    }
};

}  // namespace detail

// The "fighters" block of data/campaign/ad_strategy.json (track F, F4.3): absent, `present` false and true returned.
inline bool LoadFighterCoverBlock(const json::Value& root, FighterCoverRules& out, std::string* error = nullptr) {
    out = FighterCoverRules();
    const json::Value* f = root.Find("fighters");
    if (f == nullptr) return true;
    detail::FighterCoverLoader loader;
    FighterCoverRules r;
    if (!loader.Load(*f, r)) {
        if (error != nullptr) *error = "fighters: " + loader.Message();
        return false;
    }
    out = r;
    return true;
}
inline bool LoadFighterCoverRules(const std::string& data_dir, const std::string& rel, FighterCoverRules& out, std::string* error = nullptr) {
    std::string text;
    json::Value root;
    std::string e;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text) || !json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "fighters: cannot read " + rel + (e.empty() ? "" : ": " + e);
        return false;
    }
    return LoadFighterCoverBlock(root, out, error);
}

// The "moves" block of data/campaign/ad_strategy.json (the file's other keys are campaign/ad_strategy.hpp's).
inline bool LoadUnitMoveRules(const std::string& data_dir, const std::string& rel, UnitMoveRules& out, std::string* error = nullptr) {
    std::string text;
    json::Value root;
    std::string e;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text) || !json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "unit moves: cannot read " + rel + (e.empty() ? "" : ": " + e);
        return false;
    }
    const json::Value* m = root.Find("moves");
    if (m == nullptr) {
        if (error != nullptr) *error = "unit moves: " + rel + " has no \"moves\" block";
        return false;
    }
    detail::UnitMoveLoader loader;
    UnitMoveRules r;
    if (!loader.Load(*m, r)) {
        if (error != nullptr) *error = "unit moves: " + loader.Message();
        return false;
    }
    out = r;
    return true;
}

}  // namespace airwar
}  // namespace simcore
