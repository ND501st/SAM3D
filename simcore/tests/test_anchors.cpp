// Anchors (roadmap F1, docs/fronts/F1_ANCHORS_AS_BUILT.md): the country map is the only source of truth for where
// things are. The theatre's objects, their prepared positions, the fronts' bases, battery positions and missile launch
// points, and our chain of command's places name a place of the map ("at") and take its point from there, to the bit.
// Moving a place on the map moves everything anchored to it, with no other edit; a file naming a place the map lacks is
// refused.
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
#include "simcore/country_map.hpp"
#include "simcore/json.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
const char* const kMap = "maps/pannonia.json";

json::Value ReadJson(const std::string& rel) {
    std::string text;
    json::Value v;
    if (!detail::ReadTextFile(kData + "/" + rel, text) || !json::Parse(text, v, nullptr)) std::printf("    cannot read %s\n", rel.c_str());
    return v;
}

std::string AtOf(const json::Value& v) {
    const json::Value* a = v.Find("at");
    return a != nullptr && a->Is(json::Type::String) ? a->string : std::string();
}

// Every point a loaded theatre holds, by a label: its objects, their prepared positions, its fronts' points.
std::map<std::string, Vec3> Points(const airwar::TheatreDef& t) {
    std::map<std::string, Vec3> out;
    for (const airwar::TheatreObject& o : t.objects) {
        out["obj:" + o.id] = o.pos;
        for (std::size_t k = 0; k < o.prepared.size(); ++k) out["obj:" + o.id + "#" + std::to_string(k)] = o.prepared[k];
    }
    for (const airwar::FrontDef& f : t.fronts) {
        for (std::size_t k = 0; k < f.line.size(); ++k) out["front:" + f.faction + ".line#" + std::to_string(k)] = f.line[k];
        for (const airwar::EntryPoint& en : f.entries) out["front:" + f.faction + ".entry." + en.id] = en.pos;
        out["front:" + f.faction + ".bases"] = f.bases;
        if (f.has_battery_position) out["front:" + f.faction + ".battery_position"] = f.battery_position;
        if (f.has_missile_launch) out["front:" + f.faction + ".missile_launch"] = f.missile_launch;
    }
    return out;
}

// What the theatre files name, read from their text the way LoadTheatreFile combines them (an extending file's objects
// replace the base's of the same id, its fronts the base's of the same faction): the place each label is anchored to.
void AnchorsOf(const json::Value& file, std::map<std::string, std::string>& at) {
    if (const json::Value* objs = file.Find("objects")) {
        for (const json::Value& o : objs->items) {
            const std::string id = o.Find("id")->string;
            for (auto it = at.lower_bound("obj:" + id); it != at.end() && it->first.rfind("obj:" + id, 0) == 0;) {
                const std::string rest = it->first.substr(4 + id.size());
                it = rest.empty() || rest[0] == '#' ? at.erase(it) : std::next(it);
            }
            at["obj:" + id] = AtOf(o);
            if (const json::Value* ps = o.Find("positions")) {
                at["obj:" + id + "#0"] = AtOf(o);
                for (std::size_t k = 0; k < ps->items.size(); ++k) at["obj:" + id + "#" + std::to_string(k + 1)] = AtOf(ps->items[k]);
            }
        }
    }
    if (const json::Value* fronts = file.Find("fronts")) {
        for (const json::Value& f : fronts->items) {
            const std::string faction = f.Find("faction")->string;
            for (const char* key : {"bases", "battery_position", "missile_launch"}) {
                const json::Value* p = f.Find(key);
                if (p != nullptr) {
                    at["front:" + faction + "." + key] = AtOf(*p);
                } else {
                    at.erase("front:" + faction + "." + key);
                }
            }
        }
    }
}

std::map<std::string, Vec3> ChainPoints(const command::ChainData& c) {
    std::map<std::string, Vec3> out;
    for (const command::Place& p : c.places) out["place:" + p.id] = p.pos;
    return out;
}

bool LoadAll(const MapPlaces* places, airwar::TheatreDef& district, airwar::TheatreDef& nation, command::ChainData& chain) {
    std::string e;
    const bool ok = airwar::LoadTheatreFile(kData + "/air_war/theatre_pannonia.json", district, &e, places) &&
                    airwar::LoadTheatreFile(kData + "/air_war/theatre_pannonia_nation.json", nation, &e, places) &&
                    command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e, places);
    if (!ok) std::printf("    %s\n", e.c_str());
    return ok;
}

// A theatre on a place map, small enough to break one piece at a time.
std::string SmallTheatre(const std::string& object_point, const std::string& more = "", const std::string& bases = R"("at": "kesmark")") {
    return R"({"theatre_id": "t", "name": "t", "source": "test", "confidence": "estimate", "place_map": "maps/pannonia.json",
      "origin": {"lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 100},
      "fronts": [{"faction": "rebels", "name": "f", "line": [{"east_km": -5, "north_km": -20}, {"east_km": 5, "north_km": -20}],
                  "entries": [{"id": "S1", "east_km": 0, "north_km": -30}], "bases": {)" + bases + R"(}}],
      "objects": [{"id": "b", "name": "b", "kind": "sam_site", )" + object_point + R"(, "player": true,
                   "defence": {"system": "osa", "radius_km": 10, "lethality": 0.2}})" + more + "]}";
}

}  // namespace

SC_TEST("anchors (F1): the map's places are one namespace, built once; the theatres and the chain stand on them to the bit") {
    MapPlaces places;
    std::string e;
    SC_EXPECT(LoadMapPlaces(kData, kMap, places, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    CountryDef def;
    std::string text;
    SC_EXPECT(detail::ReadTextFile(kData + "/" + kMap, text) && LoadCountryDefJson(text, def, &e));
    SC_EXPECT(places.Size() == def.settlements.size() + def.landmarks.size() + def.sites.size());
    std::printf("    %zu places: %zu settlements, %zu landmarks, %zu sites\n", places.Size(), def.settlements.size(), def.landmarks.size(),
                def.sites.size());
    // A place's point is its list's, exactly (km * 1000.0, as a theatre's own east_km and north_km were read).
    for (const CountrySiteDef& s : def.sites) {
        const MapPoint* p = places.Find(s.id);
        SC_EXPECT(p != nullptr && p->east_m == s.at.east_m && p->north_m == s.at.north_m);
    }
    SC_EXPECT(places.Find("kesmark") != nullptr && places.Find("refinery") != nullptr && places.Find("nowhere") == nullptr);

    // Every placed object of both theatres and every place of the chain names a place, and stands at its point.
    airwar::TheatreDef district, nation;
    command::ChainData chain;
    SC_EXPECT(LoadAll(nullptr, district, nation, chain));
    SC_EXPECT(district.place_map == kMap && nation.place_map == kMap);
    std::map<std::string, std::string> at;
    AnchorsOf(ReadJson("air_war/theatre_pannonia.json"), at);
    const std::map<std::string, std::string> district_at = at;
    AnchorsOf(ReadJson("air_war/theatre_pannonia_nation.json"), at);
    int anchored = 0;
    const auto check = [&](const airwar::TheatreDef& t, const std::map<std::string, std::string>& names) {
        for (const auto& [label, pos] : Points(t)) {
            const auto it = names.find(label);
            if (it == names.end()) continue;  // a front's line and entries (tracks F2, F3), a supply line's columns
            const MapPoint* p = places.Find(it->second);
            SC_EXPECT(p != nullptr);
            if (p == nullptr) {
                std::printf("    %s names '%s'\n", label.c_str(), it->second.c_str());
                continue;
            }
            SC_EXPECT(pos.x == p->east_m && pos.y == p->north_m && pos.z == 0.0);
            ++anchored;
        }
    };
    check(district, district_at);
    check(nation, at);
    const json::Value pf = ReadJson("command/places_pannonia.json");
    for (const json::Value& v : pf.Find("places")->items) {
        const command::Place* pl = nullptr;
        for (const command::Place& q : chain.places) pl = q.id == v.Find("id")->string ? &q : pl;
        const MapPoint* p = places.Find(AtOf(v));
        SC_EXPECT(pl != nullptr && p != nullptr && pl->pos.x == p->east_m && pl->pos.y == p->north_m);
        ++anchored;
    }
    std::printf("    %d points anchored (the district's, the nation's and the chain's)\n", anchored);
    SC_EXPECT(anchored > 300);
}

SC_TEST("anchors (F1): a place moved 2 km on the map moves everything anchored to it, and nothing else") {
    MapPlaces places;
    std::string e;
    SC_EXPECT(LoadMapPlaces(kData, kMap, places, &e));
    airwar::TheatreDef d0, n0;
    command::ChainData c0;
    SC_EXPECT(LoadAll(&places, d0, n0, c0));
    std::map<std::string, std::string> at_d, at_n, at_c;
    AnchorsOf(ReadJson("air_war/theatre_pannonia.json"), at_d);
    at_n = at_d;
    AnchorsOf(ReadJson("air_war/theatre_pannonia_nation.json"), at_n);
    const json::Value pf = ReadJson("command/places_pannonia.json");  // kept: a range over a temporary's member dangles
    for (const json::Value& v : pf.Find("places")->items) at_c["place:" + v.Find("id")->string] = AtOf(v);

    // The owner's case: a factory shifted 2 km; then a town (a settlement of the map) and a prepared position.
    struct Case {
        const char* place;
        double de_m, dn_m;
        std::vector<std::string> must_move;
    };
    const std::vector<Case> cases = {
        {"csepely_works", 2000.0, 0.0, {"obj:csepely_works", "place:nv_works"}},
        {"kesmark", 0.0, -2000.0, {"obj:kesmark", "place:kesmark"}},
        {"s75_capital_nw_pos1", 1414.0, 1414.0, {"obj:s75_capital_nw#1"}},
        {"nato_bases", 0.0, 2000.0, {"front:nato.bases"}},
    };
    for (const Case& k : cases) {
        MapPlaces moved = places;
        SC_EXPECT(moved.Move(k.place, k.de_m, k.dn_m));
        airwar::TheatreDef d1, n1;
        command::ChainData c1;
        SC_EXPECT(LoadAll(&moved, d1, n1, c1));
        std::set<std::string> seen;
        int moved_n = 0, still = 0;
        const auto compare = [&](const std::map<std::string, Vec3>& before, const std::map<std::string, Vec3>& after,
                                 const std::map<std::string, std::string>& names, const airwar::TheatreDef* t) {
            SC_EXPECT(before.size() == after.size());
            for (const auto& [label, p0] : before) {
                const auto a = after.find(label);
                SC_EXPECT(a != after.end());
                if (a == after.end()) continue;
                std::string anchor;
                const auto it = names.find(label);
                if (it != names.end()) anchor = it->second;
                // A supply line's columns start at their depot: anchored where it is.
                if (t != nullptr && label.rfind("obj:", 0) == 0 && label.find('#') == std::string::npos) {
                    const airwar::TheatreObject* o = t->Find(label.substr(4));
                    if (o != nullptr && o->supply_line >= 0) {
                        const auto dep = names.find("obj:" + t->supply_lines[static_cast<std::size_t>(o->supply_line)].from);
                        if (dep != names.end()) anchor = dep->second;
                    }
                }
                const Vec3& p1 = a->second;
                if (anchor == k.place) {
                    SC_EXPECT(p1.x == p0.x + k.de_m && p1.y == p0.y + k.dn_m && p1.z == p0.z);
                    seen.insert(label);
                    ++moved_n;
                } else {
                    SC_EXPECT(p1.x == p0.x && p1.y == p0.y && p1.z == p0.z);  // to the bit
                    ++still;
                }
            }
        };
        compare(Points(d0), Points(d1), at_d, &d0);
        compare(Points(n0), Points(n1), at_n, &n0);
        compare(ChainPoints(c0), ChainPoints(c1), at_c, nullptr);
        for (const std::string& m : k.must_move) {
            SC_EXPECT(seen.count(m) == 1);
            if (seen.count(m) == 0) std::printf("    %s did not move with %s\n", m.c_str(), k.place);
        }
        std::printf("    %s moved (%.0f, %.0f) m: %d points moved with it, %d stayed to the bit\n", k.place, k.de_m, k.dn_m, moved_n, still);
    }
    SC_EXPECT(!places.Move("nowhere", 1.0, 0.0));
}

SC_TEST("anchors (F1): a file naming a place the map lacks is refused, and so are coordinates where a place belongs") {
    MapPlaces places;
    std::string e;
    SC_EXPECT(LoadMapPlaces(kData, kMap, places, &e));
    airwar::TheatreDef t;
    const auto refused = [&](const std::string& text, const MapPlaces* p, const std::string& why) {
        std::string err;
        const bool ok = airwar::LoadTheatreJson(text, t, &err, p);
        if (ok || err.find(why) == std::string::npos) std::printf("    expected a refusal with '%s', got '%s'\n", why.c_str(), err.c_str());
        return !ok && err.find(why) != std::string::npos;
    };
    std::string ok_err;
    SC_EXPECT(airwar::LoadTheatreJson(SmallTheatre(R"("at": "osa_21")"), t, &ok_err, &places));
    if (!ok_err.empty()) std::printf("    %s\n", ok_err.c_str());
    SC_EXPECT(t.objects.size() == 1 && t.objects[0].pos.x == places.Find("osa_21")->east_m);
    SC_EXPECT(refused(SmallTheatre(R"("at": "nowhere")"), &places, "objects[0].at: no place 'nowhere' on the map maps/pannonia.json"));
    SC_EXPECT(refused(SmallTheatre(R"("east_km": 6, "north_km": -10)"), &places, "takes its points from the map"));
    SC_EXPECT(refused(SmallTheatre(R"("at": "osa_21", "east_km": 6)"), &places, "takes its points from the map"));
    SC_EXPECT(refused(SmallTheatre(R"("at": "osa_21")"), nullptr, "place_map: its places are read from the map"));
    SC_EXPECT(refused(SmallTheatre(R"("at": "osa_21")", "", R"("at": "rebel_strips")"), &places, "fronts[0].bases.at: no place 'rebel_strips'"));
    SC_EXPECT(refused(SmallTheatre(R"("at": "osa_21")", "", R"("east_km": 0, "north_km": -40)"), &places, "fronts[0].bases: a theatre on a place map"));
    SC_EXPECT(refused(SmallTheatre(R"("at": "osa_21")", R"(, {"id": "s", "name": "s", "kind": "sam_site", "at": "s75_capital_nw", "mobile": true,
                      "relocate_h": 24, "positions": [{"at": "s75_capital_nw_pos1"}, {"at": "s75_capital_nw_pos9"}]})"),
                      &places, "objects[1].positions[1].at: no place 's75_capital_nw_pos9'"));
    // A theatre of coordinates (tests' inline theatres) has no places to name.
    std::string coords = SmallTheatre(R"("at": "osa_21")", "", R"("east_km": 0, "north_km": -40)");
    coords.replace(coords.find("\"place_map\": \"maps/pannonia.json\","), 34, "");
    SC_EXPECT(refused(coords, &places, "objects[0].at: a place needs the theatre's \"place_map\""));

    // Our chain's places file: the same rule.
    std::string chain_text, places_text, means_text;
    SC_EXPECT(detail::ReadTextFile(kData + "/command/chain_pannonia.json", chain_text) &&
              detail::ReadTextFile(kData + "/command/places_pannonia.json", places_text) &&
              detail::ReadTextFile(kData + "/command/signal_means.json", means_text));
    command::ChainData chain;
    SC_EXPECT(command::LoadChainJson(chain_text, places_text, means_text, chain, &e, &places));
    const std::size_t army = places_text.find("\"at\": \"army_hq\"");
    SC_EXPECT(army != std::string::npos);
    std::string bad = places_text;
    bad.replace(army, 15, "\"at\": \"nowhere\"");
    SC_EXPECT(!command::LoadChainJson(chain_text, bad, means_text, chain, &e, &places) && e.find("no place 'nowhere' on the map") != std::string::npos);
    bad = places_text;
    bad.replace(army, 15, "\"east_km\": -28, \"north_km\": 42");
    SC_EXPECT(!command::LoadChainJson(chain_text, bad, means_text, chain, &e, &places) && e.find("takes its points from the map") != std::string::npos);
    SC_EXPECT(!command::LoadChainJson(chain_text, places_text, means_text, chain, &e) && e.find("place_map") != std::string::npos);

    // The map: one namespace for its settlements, landmarks and sites.
    std::string map_text;
    SC_EXPECT(detail::ReadTextFile(kData + "/" + kMap, map_text));
    const std::size_t sites = map_text.find("\"sites\": [");
    SC_EXPECT(sites != std::string::npos);
    if (sites == std::string::npos) return;
    std::string twice = map_text;
    twice.insert(sites + 10, R"({"id": "kesmark", "name": "a second Kesmark", "east_km": 0, "north_km": 0}, )");
    CountryDef def;
    SC_EXPECT(!LoadCountryDefJson(twice, def, &e) && e.find("'kesmark' is already a place of this map") != std::string::npos);
    MapPlaces p2;
    SC_EXPECT(!LoadMapPlaces(kData, "maps/kesmark_range.json", p2, &e) && e.find("country") != std::string::npos);
}

SC_TEST("anchors (F1): a bridge's site names the crossing the supply network finds under it") {
    std::string e;
    const std::shared_ptr<const CountryMap> map = LoadCountryMap(kData, kMap, &e);
    SC_EXPECT(map != nullptr);
    if (map == nullptr) return;
    int linked = 0;
    for (const CountrySiteDef& s : map->Def().sites) {
        if (s.crossing.empty()) continue;
        const CountryCrossing* named = nullptr;
        for (const CountryCrossing& c : map->Crossings()) named = c.id == s.crossing ? &c : named;
        SC_EXPECT(named != nullptr);
        if (named == nullptr) continue;
        double dist = 0.0;
        const CountryCrossing* nearest = map->NearestCrossing(s.at.east_m, s.at.north_m, named->net, &dist);
        SC_EXPECT(nearest == named && dist < 800.0);  // air_war/logistics.hpp's rule for a bridge object
        if (nearest != named || dist >= 800.0) std::printf("    %s: %s is %.0f m away\n", s.id.c_str(), s.crossing.c_str(), dist);
        ++linked;
    }
    // Every bridge of the nation's theatre the supply network links to a crossing has its site say which.
    airwar::TheatreDef t;
    SC_EXPECT(airwar::LoadTheatreFile(kData + "/air_war/theatre_pannonia_nation.json", t, &e));
    std::map<std::string, std::string> at;
    AnchorsOf(ReadJson("air_war/theatre_pannonia.json"), at);
    AnchorsOf(ReadJson("air_war/theatre_pannonia_nation.json"), at);
    for (const airwar::TheatreObject& o : t.objects) {
        if (o.kind != airwar::ObjectKind::Bridge) continue;
        double dr = 0.0, dl = 0.0;
        map->NearestCrossing(o.pos.x, o.pos.y, CountryNet::Road, &dr);
        map->NearestCrossing(o.pos.x, o.pos.y, CountryNet::Rail, &dl);
        if (dr >= 800.0 && dl >= 800.0) continue;
        const CountrySiteDef* site = nullptr;
        for (const CountrySiteDef& s : map->Def().sites) site = s.id == at["obj:" + o.id] ? &s : site;
        SC_EXPECT(site != nullptr && !site->crossing.empty());
        if (site == nullptr || site->crossing.empty()) std::printf("    bridge %s: its site names no crossing\n", o.id.c_str());
    }
    std::printf("    %d bridge sites linked to their crossings\n", linked);
    SC_EXPECT(linked > 0);
}
