// simcore/nav/anchors.hpp — the points routes are asked between (roadmap G1; docs/campaign/11_BATTERY_LIFE_PROPOSAL.md
// §5.1, "Anchors"): our chain's places, the theatre's objects and their prepared positions, the country's towns and
// landmarks, by id. Saved routes keep anchors and plan again; nothing refers to the road graph's node numbers.
#pragma once

#include <string>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/command/data.hpp"
#include "simcore/country_map.hpp"

namespace simcore {
namespace nav {

struct Anchor {
    std::string id;
    std::string kind;  // "place", "object", "position", "settlement", "landmark", "point"
    MapPoint at;
};

class Anchors {
public:
    // The first one with an id holds it: a place of the chain at a theatre object keeps the chain's id and point.
    void Add(const std::string& id, const std::string& kind, const MapPoint& at) {
        if (Find(id) == nullptr) all_.push_back({id, kind, at});
    }
    const Anchor* Find(const std::string& id) const {
        for (const Anchor& a : all_) {
            if (a.id == id) return &a;
        }
        return nullptr;
    }
    const std::vector<Anchor>& All() const { return all_; }

    // Our chain's places (data/command/places_pannonia.json).
    void AddChain(const command::ChainData& chain) {
        for (const command::Place& p : chain.places) Add(p.id, "place", {p.pos.x, p.pos.y});
    }
    // The theatre's objects, and each one's prepared positions besides where it starts ("<id>#2", "<id>#3", ...).
    void AddTheatre(const airwar::TheatreDef& theatre) {
        for (const airwar::TheatreObject& o : theatre.objects) {
            Add(o.id, "object", {o.pos.x, o.pos.y});
            for (std::size_t k = 1; k < o.prepared.size(); ++k) {
                Add(o.id + "#" + std::to_string(k + 1), "position", {o.prepared[k].x, o.prepared[k].y});
            }
        }
    }
    // The country's towns and villages, and its landmarks (the dam, the refinery, the border crossings).
    void AddCountry(const CountryDef& country) {
        for (const CountrySettlementDef& s : country.settlements) Add(s.id, "settlement", s.centre);
        for (const CountryLandmarkDef& l : country.landmarks) Add(l.id, "landmark", l.at);
    }

private:
    std::vector<Anchor> all_;
};

}  // namespace nav
}  // namespace simcore
