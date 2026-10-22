// simcore/command/oob.hpp — our side's order of battle (data/command/oob_<id>.json): the services and every formation
// and unit under them as one tree, each with the posts of the chain of command that lead it (chain_<id>.json), the
// theatre objects it appears as to the enemy (data/air_war/theatre_<id>.json), its equipment and its establishment.
//
// It is what the war is simulated with when no player is there: who owns which battery, which radar, which depot; how
// many people and vehicles each unit has; who replaces whom (roadmap: the nation on the map, N0 and N2;
// docs/campaign/10_BRIEFING_FORCES_AND_NATION.md §4-9). This file reads it and keeps the tree whole; the cross-checks
// against the chain and the theatre are in tests/test_country_map.cpp.
#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace command {

inline const std::vector<std::string>& OobUnitKinds() {
    static const std::vector<std::string> kinds = {"ministry", "service", "army", "division", "brigade", "regiment", "group",
                                                   "battalion", "battery", "fire_unit", "company", "squadron", "sector",
                                                   "post", "platoon", "section"};
    return kinds;
}

struct OobService {
    std::string id;
    std::string name;
    std::string notes;
};

// People in a unit at war strength, by kind (docs/campaign/10_BRIEFING_FORCES_AND_NATION.md §8).
struct OobPersonnel {
    int officers = 0;
    int ncos = 0;  // career NCOs and warrant officers
    int conscripts = 0;
    std::string basis;  // where the numbers come from: a source, or "estimate"

    int Total() const { return officers + ncos + conscripts; }
};

struct OobUnit {
    std::string id;
    std::string name;
    std::string service;  // an OobService id
    std::string kind;     // OobUnitKinds()
    std::string parent;   // "" at the top of a tree
    std::vector<std::string> posts;    // chain positions that lead it (commander first)
    std::vector<std::string> objects;  // theatre objects it appears as
    std::string garrison;              // a place (data/command/places_<id>.json) or a theatre object, or ""
    std::vector<std::pair<std::string, int>> equipment;  // type (a basic model id where one exists) and count
    OobPersonnel personnel;
    std::string notes;

    int EquipmentCount(const std::string& type) const {
        int n = 0;
        for (const std::pair<std::string, int>& e : equipment) n += e.first == type ? e.second : 0;
        return n;
    }
};

struct OrderOfBattle {
    std::string oob_id;
    std::string name;
    std::vector<OobService> services;
    std::vector<OobUnit> units;

    const OobUnit* Find(const std::string& id) const {
        for (const OobUnit& u : units) {
            if (u.id == id) return &u;
        }
        return nullptr;
    }
    const OobService* Service(const std::string& id) const {
        for (const OobService& s : services) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }
    std::vector<const OobUnit*> Children(const std::string& id) const {
        std::vector<const OobUnit*> out;
        for (const OobUnit& u : units) {
            if (u.parent == id) out.push_back(&u);
        }
        return out;
    }
    // The unit that has `object` among its theatre objects, or null.
    const OobUnit* OwnerOfObject(const std::string& object) const {
        for (const OobUnit& u : units) {
            if (std::find(u.objects.begin(), u.objects.end(), object) != u.objects.end()) return &u;
        }
        return nullptr;
    }
    // The unit a chain position leads, or null.
    const OobUnit* UnitOfPost(const std::string& post) const {
        for (const OobUnit& u : units) {
            if (std::find(u.posts.begin(), u.posts.end(), post) != u.posts.end()) return &u;
        }
        return nullptr;
    }
    // Is `id` the unit `ancestor` or under it?
    bool Under(const std::string& id, const std::string& ancestor) const {
        for (const OobUnit* u = Find(id); u != nullptr; u = u->parent.empty() ? nullptr : Find(u->parent)) {
            if (u->id == ancestor) return true;
        }
        return false;
    }
    // The people of a unit and everything under it.
    int TotalPersonnel(const std::string& id) const {
        int n = 0;
        for (const OobUnit& u : units) n += Under(u.id, id) ? u.personnel.Total() : 0;
        return n;
    }
    int TotalEquipment(const std::string& id, const std::string& type) const {
        int n = 0;
        for (const OobUnit& u : units) n += Under(u.id, id) ? u.EquipmentCount(type) : 0;
        return n;
    }
};

namespace detail {

class OobLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, OrderOfBattle& out) {
        if (!Keys(root, "", {"oob_id", "name", "status", "source", "confidence", "notes", "services", "units"}) ||
            !RequireString(root, "", "oob_id", out.oob_id) || !RequireString(root, "", "name", out.name) || !Provenance(root, "")) {
            return false;
        }
        const json::Value* svc = OptionalArray(root, "", "services");
        if (svc == nullptr || svc->items.empty()) return message_.empty() ? Fail("services", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < svc->items.size(); ++i) {
            const std::string p = Index("services", i);
            OobService s;
            if (!Keys(svc->items[i], p, {"id", "name", "notes"}) || !RequireString(svc->items[i], p, "id", s.id) ||
                !RequireString(svc->items[i], p, "name", s.name) || !OptionalString(svc->items[i], p, "notes", s.notes)) {
                return false;
            }
            if (out.Service(s.id) != nullptr) return Fail(Join(p, "id"), "'" + s.id + "' is listed twice");
            out.services.push_back(std::move(s));
        }
        const json::Value* list = OptionalArray(root, "", "units");
        if (list == nullptr || list->items.empty()) return message_.empty() ? Fail("units", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            if (!LoadUnit(list->items[i], Index("units", i), out)) return false;
        }
        return CheckTree(out);
    }

private:
    bool LoadUnit(const json::Value& v, const std::string& p, OrderOfBattle& out) {
        OobUnit u;
        const auto any = [](const std::string&) { return std::string(); };
        if (!Keys(v, p, {"id", "name", "service", "kind", "parent", "posts", "objects", "garrison", "equipment", "personnel", "notes"}) ||
            !RequireString(v, p, "id", u.id) || !RequireString(v, p, "name", u.name) || !RequireString(v, p, "service", u.service) ||
            !RequireString(v, p, "kind", u.kind) || !OptionalString(v, p, "parent", u.parent) ||
            !StringList(v, p, "posts", u.posts, any) || !StringList(v, p, "objects", u.objects, any) ||
            !OptionalString(v, p, "garrison", u.garrison) || !OptionalString(v, p, "notes", u.notes)) {
            return false;
        }
        if (out.Service(u.service) == nullptr) return Fail(Join(p, "service"), "unknown service '" + u.service + "'");
        const std::vector<std::string>& kinds = OobUnitKinds();
        if (std::find(kinds.begin(), kinds.end(), u.kind) == kinds.end()) return Fail(Join(p, "kind"), "unknown unit kind '" + u.kind + "'");
        if (out.Find(u.id) != nullptr) return Fail(Join(p, "id"), "'" + u.id + "' is listed twice");
        if (const json::Value* eq = v.Find("equipment")) {
            if (!eq->Is(json::Type::Object)) return Fail(Join(p, "equipment"), "expected {type: count}");
            for (std::size_t k = 0; k < eq->keys.size(); ++k) {
                const json::Value& c = eq->items[k];
                if (!c.Is(json::Type::Number) || c.number < 0.0 || c.number != static_cast<double>(static_cast<int>(c.number))) {
                    return Fail(Join(Join(p, "equipment"), eq->keys[k]), "a whole number >= 0");
                }
                u.equipment.push_back({eq->keys[k], static_cast<int>(c.number)});
            }
        }
        if (const json::Value* pe = v.Find("personnel")) {
            const std::string pp = Join(p, "personnel");
            if (!Keys(*pe, pp, {"officers", "ncos", "conscripts", "basis", "notes"}) || !OptionalInt(*pe, pp, "officers", u.personnel.officers) ||
                !OptionalInt(*pe, pp, "ncos", u.personnel.ncos) || !OptionalInt(*pe, pp, "conscripts", u.personnel.conscripts) ||
                !RequireString(*pe, pp, "basis", u.personnel.basis)) {
                return false;
            }
            if (u.personnel.officers < 0 || u.personnel.ncos < 0 || u.personnel.conscripts < 0) return Fail(pp, "counts >= 0");
        }
        out.units.push_back(std::move(u));
        return true;
    }

    // Every parent exists, and no unit is its own ancestor.
    bool CheckTree(const OrderOfBattle& out) {
        for (std::size_t i = 0; i < out.units.size(); ++i) {
            const OobUnit& u = out.units[i];
            const std::string p = Index("units", i);
            if (!u.parent.empty() && out.Find(u.parent) == nullptr) return Fail(Join(p, "parent"), "unknown unit '" + u.parent + "'");
            std::size_t steps = 0;
            for (const OobUnit* a = &u; a != nullptr && !a->parent.empty(); a = out.Find(a->parent)) {
                if (++steps > out.units.size()) return Fail(Join(p, "parent"), "'" + u.id + "' is its own ancestor");
            }
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadOrderOfBattleJson(const std::string& text, OrderOfBattle& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) {
        if (error) *error = "order of battle: " + parse_error;
        return false;
    }
    detail::OobLoader loader;
    if (!loader.Load(root, out)) {
        if (error) *error = "order of battle: " + loader.Message();
        return false;
    }
    return true;
}

// Loads data/<file> (e.g. "command/oob_pannonia.json").
inline bool LoadOrderOfBattle(const std::string& data_dir, const std::string& file, OrderOfBattle& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + file, text)) {
        if (error) *error = "cannot open " + file;
        return false;
    }
    return LoadOrderOfBattleJson(text, out, error);
}

}  // namespace command
}  // namespace simcore
