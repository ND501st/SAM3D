// simcore/command/district.hpp — the chain of command's places and people drawn in the world (roadmap P7,
// docs/command/06_INTEGRATION.md section 6).
//
// The places of data/command/places_pannonia.json (the posts, the offices, the station, the works, the villages) as basic
// models where they stand (data/scenery/basic_models.json "places" and "place_kinds"), and each post's holder standing at
// the place he is at the hour: the chain of command seen. What is drawn otherwise is left out: a place that is a theatre
// object (the nation's assets: the battery, the radar companies, the airfield), a city or a town (the country map's
// towns), and the vehicles' crews (in their vehicles). Pure layout in theatre ENU metres; Unreal draws it (ASimMapActor).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "simcore/command/info_net.hpp"
#include "simcore/scenery/basic_models.hpp"

namespace simcore {
namespace command {

struct DistrictPiece {
    std::string place;     // the place's id
    std::string model;     // a basic model's id
    Vec3 pos;              // theatre ENU [m], on the ground (z 0)
    Vec3 facing{0.0, 1.0, 0.0};  // horizontal, the way it faces
    std::string position;  // a person: his post's id ("" a building or a vehicle)
};

// Whether a place is drawn as the district's (not as a theatre object, a town or nothing).
inline bool DistrictDraws(const Place& p) {
    return p.theatre_object.empty() && p.kind != PlaceKind::City && p.kind != PlaceKind::Town;
}

// The places' buildings and vehicles: each laid out round its point, facing out from it.
inline std::vector<DistrictPiece> DistrictPlaces(const ChainData& data, const scenery::ModelLibrary& lib) {
    std::vector<DistrictPiece> out;
    for (const Place& p : data.places) {
        if (!DistrictDraws(p)) continue;
        for (const std::pair<std::string, Vec3>& m : lib.PlaceLayout(p.id, PlaceKindName(p.kind))) {
            DistrictPiece piece;
            piece.place = p.id;
            piece.model = m.first;
            piece.pos = Vec3{p.pos.x + m.second.x, p.pos.y + m.second.y, 0.0};
            const double len = m.second.HorizontalLength();
            if (len > 1.0) piece.facing = Vec3{m.second.x / len, m.second.y / len, 0.0};
            out.push_back(piece);
        }
    }
    return out;
}

// The people of the chain where they are at campaign hour h (their routines take them to briefings and visits): each a
// `person_model` in a row beside his place's point, facing north. Crews and those at a place drawn otherwise are left out.
inline std::vector<DistrictPiece> DistrictPeople(const InfoNet& net, double h, const std::string& person_model = "soldier") {
    std::vector<DistrictPiece> out;
    const ChainData& data = net.Data();
    std::vector<int> at_place(data.places.size(), 0);
    for (std::size_t i = 0; i < data.positions.size(); ++i) {
        const Position& p = data.positions[i];
        if (p.kind == "crew") continue;
        const int place = net.PlaceOf(static_cast<int>(i), h);
        if (place < 0 || !DistrictDraws(data.places[static_cast<std::size_t>(place)])) continue;
        const Vec3 at = net.PlacePos(place, h);
        const int k = at_place[static_cast<std::size_t>(place)]++;
        DistrictPiece piece;
        piece.place = data.places[static_cast<std::size_t>(place)].id;
        piece.model = person_model;
        piece.pos = Vec3{at.x + 12.0 + 1.2 * k, at.y - 12.0, 0.0};
        piece.position = p.id;
        out.push_back(piece);
    }
    return out;
}

}  // namespace command
}  // namespace simcore
