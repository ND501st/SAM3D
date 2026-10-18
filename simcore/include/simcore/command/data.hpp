// simcore/command/data.hpp — our side's chain of command, where its people sit, and the means of signal between them
// (docs/command/; data/command/chain_pannonia.json, places_pannonia.json, signal_means.json).
//
// The first-person rule (docs/command/01_PRINCIPLES.md): the player's character knows only what reaches them through a
// person or an instrument. This file is the data the information pipeline (info_net.hpp) runs on:
//   * places: the city, the town, the villages, the works, the posts in the countryside (theatre ENU metres);
//   * means of signal: in person, messengers, despatch riders, courier runs, field and town telephones, radio, the
//     early-warning net's receivers, teleprinters, sirens, the field post, the state radio (utin_2018);
//   * positions: who holds each post, whom they answer to and deal with, where they are, what means they have;
//   * routines: the scheduled contacts (briefings, visits, courier runs, the news) that carry what isn't urgent.
// Loaded strictly, like every other data file: unknown keys and dangling references fail the load.
#pragma once

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace command {

enum class PlaceKind { City, Town, Village, Industrial, Civil, MilitaryPost, Infrastructure, Countryside };

inline const char* PlaceKindName(PlaceKind k) {
    switch (k) {
        case PlaceKind::City: return "city";
        case PlaceKind::Town: return "town";
        case PlaceKind::Village: return "village";
        case PlaceKind::Industrial: return "industrial";
        case PlaceKind::Civil: return "civil";
        case PlaceKind::MilitaryPost: return "military_post";
        case PlaceKind::Infrastructure: return "infrastructure";
        case PlaceKind::Countryside: return "countryside";
    }
    return "?";
}

inline bool ParsePlaceKind(const std::string& s, PlaceKind& out) {
    for (PlaceKind k : {PlaceKind::City, PlaceKind::Town, PlaceKind::Village, PlaceKind::Industrial, PlaceKind::Civil,
                        PlaceKind::MilitaryPost, PlaceKind::Infrastructure, PlaceKind::Countryside}) {
        if (s == PlaceKindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

struct Place {
    std::string id;
    std::string name;
    PlaceKind kind = PlaceKind::Countryside;
    Vec3 pos;                    // theatre ENU [m]
    std::string settlement;      // the city, town or village it is in ("" countryside)
    std::string theatre_object;  // a theatre object it stands at ("" none)
    bool urban = false;          // in a city or town (utin_2018 §9.3: radio and wire suffer there)
    std::string notes;
};

enum class MeanKind {
    InPerson,
    Messenger,
    DespatchRider,
    Courier,
    FieldTelephone,
    CityTelephone,
    RadioVoice,
    WarningReceiver,
    Teleprinter,
    Siren,
    FieldPost,
    Broadcast
};

inline const char* MeanKindName(MeanKind k) {
    switch (k) {
        case MeanKind::InPerson: return "in_person";
        case MeanKind::Messenger: return "messenger";
        case MeanKind::DespatchRider: return "despatch_rider";
        case MeanKind::Courier: return "courier";
        case MeanKind::FieldTelephone: return "field_telephone";
        case MeanKind::CityTelephone: return "city_telephone";
        case MeanKind::RadioVoice: return "radio_voice";
        case MeanKind::WarningReceiver: return "warning_receiver";
        case MeanKind::Teleprinter: return "teleprinter";
        case MeanKind::Siren: return "siren";
        case MeanKind::FieldPost: return "field_post";
        case MeanKind::Broadcast: return "broadcast";
    }
    return "?";
}

inline bool ParseMeanKind(const std::string& s, MeanKind& out) {
    for (MeanKind k : {MeanKind::InPerson, MeanKind::Messenger, MeanKind::DespatchRider, MeanKind::Courier,
                       MeanKind::FieldTelephone, MeanKind::CityTelephone, MeanKind::RadioVoice, MeanKind::WarningReceiver,
                       MeanKind::Teleprinter, MeanKind::Siren, MeanKind::FieldPost, MeanKind::Broadcast}) {
        if (s == MeanKindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

// A carrier of papers (it can hand over a document in full) rather than a voice.
inline bool CarriesPaper(MeanKind k) {
    return k == MeanKind::InPerson || k == MeanKind::Messenger || k == MeanKind::DespatchRider || k == MeanKind::Courier ||
           k == MeanKind::FieldPost || k == MeanKind::Teleprinter;
}

// Something that travels the roads (and can be caught by bombs on the way).
inline bool Travels(MeanKind k) {
    return k == MeanKind::Messenger || k == MeanKind::DespatchRider || k == MeanKind::Courier || k == MeanKind::FieldPost;
}

struct SignalMean {
    std::string id;
    std::string name;
    MeanKind kind = MeanKind::InPerson;
    double speed_mps = 0.0;          // a carrier's road speed
    double handling_s = 0.0;         // writing, encoding, handing over, reading
    double connect_s = 0.0;          // telephone: an urgent connection, per exchange
    double connect_routine_s = 0.0;  // telephone: a routine connection
    double range_m = 0.0;            // 0: no limit
    double loss = 0.0;               // chance a message is lost or garbled beyond use
    double intercept = 0.0;          // how much one message tells the enemy's intercept service
    bool documents = false;
    int precedence = 2;              // the highest precedence it is used for
};

struct RelayRules {
    double urgent_s = 30.0;
    double priority_s = 180.0;
    double routine_s = 600.0;
    double max_wait_h = 30.0;
    double retry_h = 1.0;
    double road_factor = 1.3;
    double night_speed_factor = 0.7;
    double urgent_handling_factor = 0.25;  // an urgent word in person or by voice: a shout, not a talk
    std::map<std::string, double> skill_factor;

    double SkillFactor(const std::string& tier) const {
        const auto it = skill_factor.find(tier);
        return it == skill_factor.end() ? 1.0 : it->second;
    }
};

struct Holder {
    std::string name;
    std::string rank;
    std::string skill = "class2";
};

struct Position {
    std::string id;
    std::string title;
    std::string unit;
    std::string kind;       // command, staff, signals, crew, territorial, civil
    int level = 2;          // 0 army and national .. 5 crews: how far down a fact must reach to get here
    std::string superior;   // "" at the top of a tree
    std::vector<std::string> links;
    std::string place;
    Holder holder;
    std::vector<std::string> means;
    bool player = false;    // the player's character may hold it
    std::string duties;

    bool Has(const std::string& mean) const { return std::find(means.begin(), means.end(), mean) != means.end(); }
    // "Major Imre Sallai (chief of staff (first deputy), 15th SAM Regiment)"
    std::string Label() const { return holder.rank + " " + holder.name + " (" + title + ")"; }
};

enum class RoutineKind { Briefing, Visit, CourierRun, Broadcast };

inline const char* RoutineKindName(RoutineKind k) {
    switch (k) {
        case RoutineKind::Briefing: return "briefing";
        case RoutineKind::Visit: return "visit";
        case RoutineKind::CourierRun: return "courier_run";
        case RoutineKind::Broadcast: return "broadcast";
    }
    return "?";
}

struct Routine {
    std::string id;
    std::string name;
    RoutineKind kind = RoutineKind::Briefing;
    std::string by;                  // who holds it (a briefing) or travels (a visit)
    std::string place;               // where it is held (briefings, visits)
    std::string mean = "in_person";  // a courier run's or broadcast's means
    std::string carrier;             // a courier run made by a person of the chain ("" a despatch rider)
    std::vector<double> at_local_h;  // local hours it starts (each day of its cycle)
    double every_h = 24.0;
    double duration_h = 1.0;
    std::vector<std::string> with;   // who attends (briefings, visits)
    std::vector<std::string> stops;  // a courier run's places in order
};

struct ChainData {
    std::string chain_id;
    std::vector<Place> places;
    std::vector<SignalMean> means;
    RelayRules relay;
    std::vector<Position> positions;
    std::vector<Routine> routines;
    std::string unit_regiment;   // the regiment's commander: the subtree most facts are for
    std::string player_default;  // the post the player's character holds unless the game says otherwise

    int PlaceIndex(const std::string& id) const {
        for (std::size_t i = 0; i < places.size(); ++i) {
            if (places[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }
    int MeanIndex(const std::string& id) const {
        for (std::size_t i = 0; i < means.size(); ++i) {
            if (means[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }
    int PositionIndex(const std::string& id) const {
        for (std::size_t i = 0; i < positions.size(); ++i) {
            if (positions[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }
    const Place* FindPlace(const std::string& id) const {
        const int i = PlaceIndex(id);
        return i < 0 ? nullptr : &places[static_cast<std::size_t>(i)];
    }
    const SignalMean* FindMean(const std::string& id) const {
        const int i = MeanIndex(id);
        return i < 0 ? nullptr : &means[static_cast<std::size_t>(i)];
    }
    const Position* FindPosition(const std::string& id) const {
        const int i = PositionIndex(id);
        return i < 0 ? nullptr : &positions[static_cast<std::size_t>(i)];
    }
    // Is `id` inside the tree under `root` (itself included)?
    bool InSubtree(const std::string& id, const std::string& root) const {
        std::string at = id;
        for (std::size_t guard = 0; guard <= positions.size() && !at.empty(); ++guard) {
            if (at == root) return true;
            const Position* p = FindPosition(at);
            if (p == nullptr) return false;
            at = p->superior;
        }
        return false;
    }
};

// ------------------------------------------------------------------------------------------------ loading

namespace detail {

class CommandLoader : public simcore::detail::ConsoleLoader {
public:
    bool LoadPlaces(const json::Value& root, std::vector<Place>& out) {
        if (!Keys(root, "", {"places_id", "display_name", "status", "theatre", "source", "confidence", "notes", "places"})) return false;
        const json::Value* arr = OptionalArray(root, "", "places");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("places", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& v = arr->items[i];
            const std::string p = Index("places", i);
            Place pl;
            std::string kind;
            double e = 0.0, n = 0.0;
            if (!Keys(v, p, {"id", "name", "kind", "east_km", "north_km", "settlement", "theatre_object", "notes"}) ||
                !RequireString(v, p, "id", pl.id) || !RequireString(v, p, "name", pl.name) || !RequireString(v, p, "kind", kind) ||
                !OptionalString(v, p, "settlement", pl.settlement) || !OptionalString(v, p, "theatre_object", pl.theatre_object) ||
                !OptionalString(v, p, "notes", pl.notes)) {
                return false;
            }
            if (!ParsePlaceKind(kind, pl.kind)) return Fail(Join(p, "kind"), "unknown place kind");
            if (v.Find("east_km") == nullptr || v.Find("north_km") == nullptr) return Fail(p, "needs east_km and north_km");
            if (!OptionalNumber(v, p, "east_km", e) || !OptionalNumber(v, p, "north_km", n)) return false;
            pl.pos = Vec3{e * 1000.0, n * 1000.0, 0.0};
            for (const Place& q : out) {
                if (q.id == pl.id) return Fail(Join(p, "id"), "duplicate place");
            }
            out.push_back(pl);
        }
        // Settlements exist and are settlements; a place in a city or town is urban.
        for (std::size_t i = 0; i < out.size(); ++i) {
            Place& pl = out[i];
            pl.urban = pl.kind == PlaceKind::City || pl.kind == PlaceKind::Town;
            if (pl.settlement.empty()) continue;
            const Place* s = nullptr;
            for (const Place& q : out) {
                if (q.id == pl.settlement) s = &q;
            }
            if (s == nullptr || (s->kind != PlaceKind::City && s->kind != PlaceKind::Town && s->kind != PlaceKind::Village)) {
                return Fail(Join(Index("places", i), "settlement"), "not a city, town or village of this file");
            }
            pl.urban = s->kind == PlaceKind::City || s->kind == PlaceKind::Town;
        }
        return true;
    }

    bool LoadMeans(const json::Value& root, std::vector<SignalMean>& out, RelayRules& relay) {
        if (!Keys(root, "", {"means_id", "display_name", "status", "source", "confidence", "notes", "relay", "means"})) return false;
        if (const json::Value* r = root.Find("relay")) {
            if (!Keys(*r, "relay", {"urgent_s", "priority_s", "routine_s", "max_wait_h", "retry_h", "road_factor", "night_speed_factor",
                                     "urgent_handling_factor", "skill_factor", "source", "confidence", "notes"}) ||
                !Num(*r, "relay", "urgent_s", relay.urgent_s, 0.0, 3600.0) || !Num(*r, "relay", "priority_s", relay.priority_s, 0.0, 36000.0) ||
                !Num(*r, "relay", "routine_s", relay.routine_s, 0.0, 86400.0) || !Num(*r, "relay", "max_wait_h", relay.max_wait_h, 0.0, 240.0) ||
                !Num(*r, "relay", "retry_h", relay.retry_h, 0.0, 48.0) || !Num(*r, "relay", "road_factor", relay.road_factor, 1.0, 3.0) ||
                !Num(*r, "relay", "night_speed_factor", relay.night_speed_factor, 0.1, 1.0) ||
                !Num(*r, "relay", "urgent_handling_factor", relay.urgent_handling_factor, 0.0, 1.0)) {
                return false;
            }
            if (const json::Value* sf = r->Find("skill_factor")) {
                if (!sf->Is(json::Type::Object)) return Fail("relay.skill_factor", "expected {tier: factor}");
                for (std::size_t k = 0; k < sf->keys.size(); ++k) {
                    const json::Value& x = sf->items[k];
                    if (!x.Is(json::Type::Number) || x.number <= 0.0) return Fail(Join("relay.skill_factor", sf->keys[k]), "a positive number");
                    relay.skill_factor[sf->keys[k]] = x.number;
                }
            }
        }
        const json::Value* arr = OptionalArray(root, "", "means");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("means", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& v = arr->items[i];
            const std::string p = Index("means", i);
            SignalMean m;
            std::string kind;
            double speed_kmh = 0.0, range_km = 0.0;
            if (!Keys(v, p, {"id", "name", "kind", "speed_kmh", "handling_s", "connect_s", "connect_routine_s", "range_km", "loss",
                              "intercept", "documents", "precedence", "source", "confidence", "notes"}) ||
                !RequireString(v, p, "id", m.id) || !RequireString(v, p, "name", m.name) || !RequireString(v, p, "kind", kind) ||
                !Num(v, p, "speed_kmh", speed_kmh, 0.0, 1000.0) || !Num(v, p, "handling_s", m.handling_s, 0.0, 1.0e6) ||
                !Num(v, p, "connect_s", m.connect_s, 0.0, 86400.0) || !Num(v, p, "connect_routine_s", m.connect_routine_s, 0.0, 86400.0) ||
                !Num(v, p, "range_km", range_km, 0.0, 10000.0) || !Num(v, p, "loss", m.loss, 0.0, 1.0) ||
                !Num(v, p, "intercept", m.intercept, 0.0, 1.0) || !OptionalBool(v, p, "documents", m.documents) ||
                !Int(v, p, "precedence", m.precedence, 0, 2)) {
                return false;
            }
            if (!ParseMeanKind(kind, m.kind)) return Fail(Join(p, "kind"), "unknown means kind");
            if (Travels(m.kind) && speed_kmh <= 0.0) return Fail(Join(p, "speed_kmh"), "a carrier needs a speed");
            m.speed_mps = speed_kmh / 3.6;
            m.range_m = range_km * 1000.0;
            if (m.connect_routine_s <= 0.0) m.connect_routine_s = m.connect_s;
            for (const SignalMean& q : out) {
                if (q.id == m.id) return Fail(Join(p, "id"), "duplicate means");
            }
            out.push_back(m);
        }
        return true;
    }

    bool LoadChain(const json::Value& root, ChainData& out, std::string& places_file, std::string& means_file) {
        if (!Keys(root, "", {"chain_id", "display_name", "status", "places", "means", "source", "confidence", "notes", "unit_regiment",
                             "player_default", "positions", "routines"}) ||
            !RequireString(root, "", "chain_id", out.chain_id) || !RequireString(root, "", "places", places_file) ||
            !RequireString(root, "", "means", means_file) || !RequireString(root, "", "unit_regiment", out.unit_regiment) ||
            !RequireString(root, "", "player_default", out.player_default)) {
            return false;
        }
        const json::Value* arr = OptionalArray(root, "", "positions");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("positions", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& v = arr->items[i];
            const std::string p = Index("positions", i);
            Position pos;
            if (!Keys(v, p, {"id", "title", "unit", "kind", "level", "superior", "links", "place", "holder", "means", "player", "duties",
                             "source", "confidence", "notes"}) ||
                !RequireString(v, p, "id", pos.id) || !RequireString(v, p, "title", pos.title) || !RequireString(v, p, "unit", pos.unit) ||
                !RequireString(v, p, "kind", pos.kind) || !Int(v, p, "level", pos.level, 0, 5) ||
                !OptionalString(v, p, "superior", pos.superior) || !Strings(v, p, "links", pos.links) ||
                !RequireString(v, p, "place", pos.place) || !Strings(v, p, "means", pos.means) ||
                !OptionalBool(v, p, "player", pos.player) || !OptionalString(v, p, "duties", pos.duties)) {
                return false;
            }
            if (v.Find("level") == nullptr) return Fail(Join(p, "level"), "required");
            const json::Value* h = v.Find("holder");
            if (h == nullptr) return Fail(Join(p, "holder"), "required");
            if (!Keys(*h, Join(p, "holder"), {"name", "rank", "skill"}) || !RequireString(*h, Join(p, "holder"), "name", pos.holder.name) ||
                !RequireString(*h, Join(p, "holder"), "rank", pos.holder.rank) ||
                !OptionalString(*h, Join(p, "holder"), "skill", pos.holder.skill)) {
                return false;
            }
            if (pos.means.empty()) return Fail(Join(p, "means"), "at least one");
            for (const Position& q : out.positions) {
                if (q.id == pos.id) return Fail(Join(p, "id"), "duplicate position");
            }
            out.positions.push_back(pos);
        }
        const json::Value* rarr = OptionalArray(root, "", "routines");
        if (rarr == nullptr && !message_.empty()) return false;
        if (rarr != nullptr) {
            for (std::size_t i = 0; i < rarr->items.size(); ++i) {
                const json::Value& v = rarr->items[i];
                const std::string p = Index("routines", i);
                Routine r;
                std::string kind;
                if (!Keys(v, p, {"id", "name", "kind", "by", "place", "mean", "carrier", "at_local_h", "every_h", "duration_h", "with", "stops",
                                 "source", "confidence", "notes"}) ||
                    !RequireString(v, p, "id", r.id) || !RequireString(v, p, "name", r.name) || !RequireString(v, p, "kind", kind) ||
                    !OptionalString(v, p, "by", r.by) || !OptionalString(v, p, "place", r.place) || !OptionalString(v, p, "mean", r.mean) ||
                    !OptionalString(v, p, "carrier", r.carrier) || !Num(v, p, "every_h", r.every_h, 1.0, 24.0 * 30.0) ||
                    !Num(v, p, "duration_h", r.duration_h, 0.0, 24.0) || !Strings(v, p, "with", r.with) || !Strings(v, p, "stops", r.stops)) {
                    return false;
                }
                bool known = false;
                for (RoutineKind k : {RoutineKind::Briefing, RoutineKind::Visit, RoutineKind::CourierRun, RoutineKind::Broadcast}) {
                    if (kind == RoutineKindName(k)) {
                        r.kind = k;
                        known = true;
                    }
                }
                if (!known) return Fail(Join(p, "kind"), "briefing, visit, courier_run or broadcast");
                const json::Value* at = OptionalArray(v, p, "at_local_h");
                if (at == nullptr || at->items.empty()) return message_.empty() ? Fail(Join(p, "at_local_h"), "required, local hours") : false;
                for (std::size_t k = 0; k < at->items.size(); ++k) {
                    const json::Value& x = at->items[k];
                    if (!x.Is(json::Type::Number) || x.number < 0.0 || x.number >= 24.0) return Fail(Index(Join(p, "at_local_h"), k), "an hour 0..24");
                    r.at_local_h.push_back(x.number);
                }
                if ((r.kind == RoutineKind::Briefing || r.kind == RoutineKind::Visit) && (r.by.empty() || r.place.empty())) {
                    return Fail(p, "a briefing or visit needs 'by' and 'place'");
                }
                if (r.kind == RoutineKind::CourierRun && r.stops.size() < 2) return Fail(Join(p, "stops"), "a courier run needs two stops or more");
                out.routines.push_back(r);
            }
        }
        return true;
    }

    // Everything refers to something that exists; the chain is a forest; the player's post is a player's.
    bool Check(ChainData& d) {
        for (std::size_t i = 0; i < d.positions.size(); ++i) {
            const Position& pos = d.positions[i];
            const std::string p = Index("positions", i);
            if (!pos.superior.empty() && d.FindPosition(pos.superior) == nullptr) return Fail(Join(p, "superior"), "no such position");
            for (const std::string& l : pos.links) {
                if (d.FindPosition(l) == nullptr) return Fail(Join(p, "links"), "no such position: " + l);
            }
            if (d.FindPlace(pos.place) == nullptr) return Fail(Join(p, "place"), "no such place: " + pos.place);
            for (const std::string& m : pos.means) {
                if (d.FindMean(m) == nullptr) return Fail(Join(p, "means"), "no such means: " + m);
            }
            if (!pos.holder.skill.empty() && !d.relay.skill_factor.empty() && d.relay.skill_factor.count(pos.holder.skill) == 0) {
                return Fail(Join(Join(p, "holder"), "skill"), "no such skill tier: " + pos.holder.skill);
            }
            // No cycles up the tree, and a subordinate is never above its superior.
            std::set<std::string> seen;
            std::string at = pos.id;
            while (!at.empty()) {
                if (!seen.insert(at).second) return Fail(Join(p, "superior"), "the chain loops");
                const Position* q = d.FindPosition(at);
                at = q != nullptr ? q->superior : std::string();
            }
            if (!pos.superior.empty() && d.FindPosition(pos.superior)->level > pos.level) {
                return Fail(Join(p, "level"), "below its superior's level");
            }
        }
        if (d.FindPosition(d.unit_regiment) == nullptr) return Fail("unit_regiment", "no such position");
        const Position* pd = d.FindPosition(d.player_default);
        if (pd == nullptr || !pd->player) return Fail("player_default", "not a post the player may hold");
        for (std::size_t i = 0; i < d.routines.size(); ++i) {
            const Routine& r = d.routines[i];
            const std::string p = Index("routines", i);
            if (!r.by.empty() && d.FindPosition(r.by) == nullptr) return Fail(Join(p, "by"), "no such position");
            if (!r.carrier.empty() && d.FindPosition(r.carrier) == nullptr) return Fail(Join(p, "carrier"), "no such position");
            if (!r.place.empty() && d.FindPlace(r.place) == nullptr) return Fail(Join(p, "place"), "no such place");
            if (d.FindMean(r.mean) == nullptr) return Fail(Join(p, "mean"), "no such means");
            for (const std::string& w : r.with) {
                if (d.FindPosition(w) == nullptr) return Fail(Join(p, "with"), "no such position: " + w);
            }
            for (const std::string& s : r.stops) {
                if (d.FindPlace(s) == nullptr) return Fail(Join(p, "stops"), "no such place: " + s);
            }
        }
        return true;
    }

private:
    bool Num(const json::Value& obj, const std::string& path, const char* key, double& out, double lo, double hi) {
        if (!OptionalNumber(obj, path, key, out)) return false;
        if (obj.Find(key) != nullptr && (out < lo || out > hi)) {
            std::ostringstream s;
            s << "must be in " << lo << " .. " << hi;
            return Fail(Join(path, key), s.str());
        }
        return true;
    }
    bool Int(const json::Value& obj, const std::string& path, const char* key, int& out, int lo, int hi) {
        if (!OptionalInt(obj, path, key, out)) return false;
        if (obj.Find(key) != nullptr && (out < lo || out > hi)) {
            return Fail(Join(path, key), "must be in " + std::to_string(lo) + " .. " + std::to_string(hi));
        }
        return true;
    }
    bool Strings(const json::Value& obj, const std::string& path, const char* key, std::vector<std::string>& out) {
        const json::Value* arr = OptionalArray(obj, path, key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            if (!arr->items[i].Is(json::Type::String) || arr->items[i].string.empty()) return Fail(Index(Join(path, key), i), "expected a string");
            out.push_back(arr->items[i].string);
        }
        return true;
    }
};

inline bool ReadText(const std::string& path, std::string& out, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream s;
    s << f.rdbuf();
    out = s.str();
    return true;
}

}  // namespace detail

// Load a chain from its three texts (tests).
inline bool LoadChainJson(const std::string& chain_text, const std::string& places_text, const std::string& means_text, ChainData& out,
                          std::string* error = nullptr) {
    auto fail = [&](const std::string& what, const std::string& msg) {
        if (error != nullptr) *error = what + ": " + msg;
        return false;
    };
    json::Value chain, places, means;
    std::string perr;
    if (!json::Parse(chain_text, chain, &perr)) return fail("chain", perr);
    if (!json::Parse(places_text, places, &perr)) return fail("places", perr);
    if (!json::Parse(means_text, means, &perr)) return fail("means", perr);
    ChainData d;
    std::string places_file, means_file;
    {
        detail::CommandLoader l;
        if (!l.LoadChain(chain, d, places_file, means_file)) return fail("chain", l.Message());
    }
    {
        detail::CommandLoader l;
        if (!l.LoadPlaces(places, d.places)) return fail("places", l.Message());
    }
    {
        detail::CommandLoader l;
        if (!l.LoadMeans(means, d.means, d.relay)) return fail("means", l.Message());
    }
    detail::CommandLoader l;
    if (!l.Check(d)) return fail("chain", l.Message());
    out = std::move(d);
    return true;
}

// Load a chain file and the places and means files it names (paths relative to the data directory).
inline bool LoadChainData(const std::string& data_dir, const std::string& chain_file, ChainData& out, std::string* error = nullptr) {
    const std::string base = data_dir.empty() ? std::string() : data_dir + "/";
    std::string chain_text, places_text, means_text;
    if (!detail::ReadText(base + chain_file, chain_text, error)) return false;
    json::Value chain;
    std::string perr;
    if (!json::Parse(chain_text, chain, &perr)) {
        if (error != nullptr) *error = chain_file + ": " + perr;
        return false;
    }
    const json::Value* pf = chain.Find("places");
    const json::Value* mf = chain.Find("means");
    if (pf == nullptr || mf == nullptr || !pf->Is(json::Type::String) || !mf->Is(json::Type::String)) {
        if (error != nullptr) *error = chain_file + ": needs 'places' and 'means' file names";
        return false;
    }
    if (!detail::ReadText(base + pf->string, places_text, error) || !detail::ReadText(base + mf->string, means_text, error)) return false;
    std::string e;
    if (!LoadChainJson(chain_text, places_text, means_text, out, &e)) {
        if (error != nullptr) *error = chain_file + ": " + e;
        return false;
    }
    return true;
}

}  // namespace command
}  // namespace simcore
