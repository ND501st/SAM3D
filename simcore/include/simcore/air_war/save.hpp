// simcore/air_war/save.hpp — saving and loading an air war campaign (docs/air_war/08_IMPLEMENTATION_PLAN.md, A13).
//
// A campaign is deterministic for its data and seed, so a save is small: the data files, the seed and the player's
// actions in order (each Advance with the posture then in force, each move of the battery, each raid let pass, each raid
// played with its outcome). Loading starts the campaign again from the seed and replays the actions; the result is the
// same campaign, to the bit. Numbers are written with 17 significant digits so that the replay sees exactly the times
// and outcomes the original saw.
//
//   {"save_version": 1, "theatre_file": "air_war/theatre_pannonia.json", "faction_file": "air_war/faction_nato.json",
//    "seed": 7, "actions": [ {"kind": "advance", "until_h": 24, "posture": {...}}, {"kind": "complete", ...} ]}
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace airwar {

namespace save_detail {

using namespace jsonb;

inline json::Value Strings(const std::vector<std::string>& v) {
    json::Value a = Arr();
    for (const std::string& x : v) Push(a, Str(x));
    return a;
}
inline json::Value Ints(const std::vector<int>& v) {
    json::Value a = Arr();
    for (int x : v) Push(a, Num(x));
    return a;
}
inline json::Value Pairs(const std::vector<std::pair<std::string, double>>& v) {
    json::Value a = Arr();
    for (const auto& x : v) {
        json::Value p = Arr();
        Push(p, Str(x.first));
        Push(p, Num(x.second));
        Push(a, p);
    }
    return a;
}

inline json::Value PostureJson(const BatteryPosture& p) {
    json::Value o = Obj();
    Put(o, "emission", Num(p.emission));
    Put(o, "emcon_discipline", Num(p.emcon_discipline));
    Put(o, "radio_discipline", Num(p.radio_discipline));
    Put(o, "dummy_traffic", Bool(p.dummy_traffic));
    Put(o, "dummies", Num(p.dummies));
    Put(o, "decoy_mode", Str(p.decoy_mode));
    Put(o, "nets", Bool(p.nets));
    Put(o, "emplacement", Bool(p.emplacement));
    Put(o, "relocate_every_h", Num(p.relocate_every_h));
    Put(o, "fight_silent", Bool(p.fight_silent));
    return o;
}

inline json::Value OutcomeJson(const PackageOutcome& o) {
    json::Value v = Obj();
    Put(v, "package_id", Num(o.package_id));
    json::Value losses = Arr();
    for (const LossRecord& l : o.losses) {
        json::Value x = Obj();
        Put(x, "flight", Num(l.flight));
        Put(x, "by", Str(l.by));
        Put(x, "east_m", Num(l.where.x));
        Put(x, "north_m", Num(l.where.y));
        Put(x, "officer", Bool(l.officer));
        Put(x, "captured", Bool(l.captured));
        Push(losses, x);
    }
    Put(v, "losses", losses);
    Put(v, "damaged", Num(o.damaged));
    Put(v, "damage", Pairs(o.damage));
    Put(v, "destroyed", Strings(o.destroyed));
    Put(v, "decoys_hit", Strings(o.decoys_hit));
    Put(v, "hit_nothing", Bool(o.hit_nothing));
    Put(v, "radars_killed", Strings(o.radars_killed));
    Put(v, "radars_suppressed", Strings(o.radars_suppressed));
    Put(v, "emissions", Pairs(o.emissions));
    Put(v, "fired", Strings(o.fired));
    Put(v, "recce_seen", Strings(o.recce_seen));
    Put(v, "missiles_spent", Num(o.missiles_spent));
    Put(v, "arms_fired", Num(o.arms_fired));
    Put(v, "aborted", Bool(o.aborted));
    Put(v, "delivered", Bool(o.delivered));
    Put(v, "collateral", Num(o.collateral));
    Put(v, "notes", Strings(o.notes));
    Put(v, "released_flights", Ints(o.released_flights));
    Put(v, "aborted_flights", Ints(o.aborted_flights));
    Put(v, "truth_applied", Bool(o.truth_applied));
    Put(v, "played", Bool(o.played));
    return v;
}

// Readers: lenient about missing keys (defaults), strict about types.
class Reader {
public:
    std::string error;

    double Number(const json::Value& o, const char* key, double fallback = 0.0) {
        const json::Value* v = o.Find(key);
        if (v == nullptr) return fallback;
        if (!v->Is(json::Type::Number)) Bad(key, "a number");
        return v->number;
    }
    int Int(const json::Value& o, const char* key, int fallback = 0) {
        return static_cast<int>(std::llround(Number(o, key, fallback)));
    }
    bool Boolean(const json::Value& o, const char* key, bool fallback = false) {
        const json::Value* v = o.Find(key);
        if (v == nullptr) return fallback;
        if (!v->Is(json::Type::Bool)) Bad(key, "true or false");
        return v->boolean;
    }
    std::string String(const json::Value& o, const char* key, const std::string& fallback = std::string()) {
        const json::Value* v = o.Find(key);
        if (v == nullptr) return fallback;
        if (!v->Is(json::Type::String)) Bad(key, "a string");
        return v->string;
    }
    std::vector<std::string> Strings(const json::Value& o, const char* key) {
        std::vector<std::string> out;
        const json::Value* v = o.Find(key);
        if (v == nullptr) return out;
        if (!v->Is(json::Type::Array)) Bad(key, "a list");
        for (const json::Value& x : v->items) {
            if (!x.Is(json::Type::String)) Bad(key, "a list of strings");
            out.push_back(x.string);
        }
        return out;
    }
    std::vector<int> Ints(const json::Value& o, const char* key) {
        std::vector<int> out;
        const json::Value* v = o.Find(key);
        if (v == nullptr) return out;
        if (!v->Is(json::Type::Array)) Bad(key, "a list");
        for (const json::Value& x : v->items) {
            if (!x.Is(json::Type::Number)) Bad(key, "a list of numbers");
            out.push_back(static_cast<int>(std::llround(x.number)));
        }
        return out;
    }
    std::vector<std::pair<std::string, double>> Pairs(const json::Value& o, const char* key) {
        std::vector<std::pair<std::string, double>> out;
        const json::Value* v = o.Find(key);
        if (v == nullptr) return out;
        if (!v->Is(json::Type::Array)) Bad(key, "a list");
        for (const json::Value& x : v->items) {
            if (!x.Is(json::Type::Array) || x.items.size() != 2 || !x.items[0].Is(json::Type::String) || !x.items[1].Is(json::Type::Number)) {
                Bad(key, "a list of [id, number]");
                continue;
            }
            out.push_back({x.items[0].string, x.items[1].number});
        }
        return out;
    }

    BatteryPosture Posture(const json::Value& p) {
        BatteryPosture b;
        if (!p.Is(json::Type::Object)) {
            Bad("posture", "an object");
            return b;
        }
        b.emission = Number(p, "emission", b.emission);
        b.emcon_discipline = Number(p, "emcon_discipline", b.emcon_discipline);
        b.radio_discipline = Number(p, "radio_discipline", b.radio_discipline);
        b.dummy_traffic = Boolean(p, "dummy_traffic", b.dummy_traffic);
        b.dummies = Int(p, "dummies", b.dummies);
        b.decoy_mode = String(p, "decoy_mode", b.decoy_mode);
        b.nets = Boolean(p, "nets", b.nets);
        b.emplacement = Boolean(p, "emplacement", b.emplacement);
        b.relocate_every_h = Number(p, "relocate_every_h", b.relocate_every_h);
        b.fight_silent = Boolean(p, "fight_silent", b.fight_silent);
        return b;
    }

    PackageOutcome Outcome(const json::Value& v) {
        PackageOutcome o;
        if (!v.Is(json::Type::Object)) {
            Bad("outcome", "an object");
            return o;
        }
        o.package_id = Int(v, "package_id");
        if (const json::Value* ls = v.Find("losses")) {
            for (const json::Value& x : ls->items) {
                LossRecord l;
                l.flight = Int(x, "flight", -1);
                l.by = String(x, "by");
                l.where = Vec3{Number(x, "east_m"), Number(x, "north_m"), 0.0};
                l.officer = Boolean(x, "officer");
                l.captured = Boolean(x, "captured");
                o.losses.push_back(l);
            }
        }
        o.damaged = Int(v, "damaged");
        o.damage = Pairs(v, "damage");
        o.destroyed = Strings(v, "destroyed");
        o.decoys_hit = Strings(v, "decoys_hit");
        o.hit_nothing = Boolean(v, "hit_nothing");
        o.radars_killed = Strings(v, "radars_killed");
        o.radars_suppressed = Strings(v, "radars_suppressed");
        o.emissions = Pairs(v, "emissions");
        o.fired = Strings(v, "fired");
        o.recce_seen = Strings(v, "recce_seen");
        o.missiles_spent = Int(v, "missiles_spent");
        o.arms_fired = Int(v, "arms_fired");
        o.aborted = Boolean(v, "aborted");
        o.delivered = Boolean(v, "delivered");
        o.collateral = Number(v, "collateral");
        o.notes = Strings(v, "notes");
        o.released_flights = Ints(v, "released_flights");
        o.aborted_flights = Ints(v, "aborted_flights");
        o.truth_applied = Boolean(v, "truth_applied");
        o.played = Boolean(v, "played", true);
        return o;
    }

private:
    void Bad(const char* key, const char* what) {
        if (error.empty()) error = std::string(key) + ": expected " + what;
    }
};

}  // namespace save_detail

// The campaign as save text. theatre_file and faction_file are relative to the data folder, as LoadAirWarData takes them.
inline std::string SaveCampaignJson(const AirCampaign& c, const std::string& theatre_file, const std::string& faction_file) {
    using namespace jsonb;
    json::Value root = Obj();
    Put(root, "save_version", Num(1));
    Put(root, "theatre_file", Str(theatre_file));
    Put(root, "faction_file", Str(faction_file));
    Put(root, "seed", Num(static_cast<double>(c.Seed())));
    Put(root, "clock", Str(c.Clock()));
    json::Value actions = Arr();
    for (const CampaignAction& a : c.Actions()) {
        json::Value x = Obj();
        switch (a.kind) {
            case CampaignAction::Kind::Advance:
                Put(x, "kind", Str("advance"));
                Put(x, "until_h", Num(a.until_h));
                break;
            case CampaignAction::Kind::Relocate:
                Put(x, "kind", Str("relocate"));
                Put(x, "east_m", Num(a.to.x));
                Put(x, "north_m", Num(a.to.y));
                break;
            case CampaignAction::Kind::Skip:
                Put(x, "kind", Str("skip"));
                break;
            case CampaignAction::Kind::Complete:
                Put(x, "kind", Str("complete"));
                Put(x, "end_h", Num(a.end_h));
                Put(x, "outcome", save_detail::OutcomeJson(a.outcome));
                break;
        }
        Put(x, "posture", save_detail::PostureJson(a.posture));
        Push(actions, x);
    }
    Put(root, "actions", actions);
    return Dump(root, 1, 17);
}

// Load a campaign from save text: its data from data_dir, started from its seed, its actions replayed.
inline bool LoadCampaignJson(const std::string& text, const std::string& data_dir, AirCampaign& out, std::string* error = nullptr,
                             std::string* theatre_file = nullptr, std::string* faction_file = nullptr) {
    json::Value root;
    std::string perr;
    if (!json::Parse(text, root, &perr)) {
        if (error != nullptr) *error = "the save doesn't parse: " + perr;
        return false;
    }
    save_detail::Reader r;
    if (r.Int(root, "save_version") != 1) {
        if (error != nullptr) *error = "save_version 1 expected";
        return false;
    }
    const std::string theatre = r.String(root, "theatre_file");
    const std::string faction = r.String(root, "faction_file");
    const unsigned seed = static_cast<unsigned>(r.Number(root, "seed", 1.0));
    std::vector<CampaignAction> actions;
    if (const json::Value* list = root.Find("actions")) {
        for (const json::Value& x : list->items) {
            CampaignAction a;
            const std::string kind = r.String(x, "kind");
            if (kind == "advance") {
                a.kind = CampaignAction::Kind::Advance;
                a.until_h = r.Number(x, "until_h");
            } else if (kind == "relocate") {
                a.kind = CampaignAction::Kind::Relocate;
                a.to = Vec3{r.Number(x, "east_m"), r.Number(x, "north_m"), 0.0};
            } else if (kind == "skip") {
                a.kind = CampaignAction::Kind::Skip;
            } else if (kind == "complete") {
                a.kind = CampaignAction::Kind::Complete;
                a.end_h = r.Number(x, "end_h");
                if (const json::Value* o = x.Find("outcome")) a.outcome = r.Outcome(*o);
            } else {
                if (error != nullptr) *error = "unknown action '" + kind + "'";
                return false;
            }
            if (const json::Value* p = x.Find("posture")) a.posture = r.Posture(*p);
            actions.push_back(a);
        }
    }
    if (!r.error.empty()) {
        if (error != nullptr) *error = r.error;
        return false;
    }
    AirWarData data;
    if (!LoadAirWarData(data_dir, theatre, faction, data, error)) return false;
    AirCampaign c;
    if (!c.Init(data, seed, error)) return false;
    if (!c.Replay(actions, error)) return false;
    out = c;
    if (theatre_file != nullptr) *theatre_file = theatre;
    if (faction_file != nullptr) *faction_file = faction;
    return true;
}

}  // namespace airwar
}  // namespace simcore
