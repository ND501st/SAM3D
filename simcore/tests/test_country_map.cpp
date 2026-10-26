// The nation on the map (roadmap N0-N1): the country's map (simcore/country_map.hpp, data/maps/pannonia.json), our
// order of battle (simcore/command/oob.hpp, data/command/oob_pannonia.json), and the air war's new object kinds and
// air defence systems. The country must agree with everything already built on it: the Kesmark range's ground and
// lines, the theatre's objects and fronts, the chain of command's places and posts.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
#include "simcore/country_map.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::shared_ptr<const CountryMap> Country() {
    static std::shared_ptr<const CountryMap> map;
    if (!map) {
        const auto t0 = std::chrono::steady_clock::now();
        std::string e;
        map = LoadCountryMap(kData, "maps/pannonia.json", &e);
        if (!map) {
            std::printf("    %s\n", e.c_str());
        } else {
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::printf("    pannonia built in %.0f ms: %dx%d grid, %zu road, %zu rail, %zu oil nodes, %zu crossings\n", ms,
                        map->Grid().cols, map->Grid().rows, map->Network(CountryNet::Road).Nodes().size(),
                        map->Network(CountryNet::Rail).Nodes().size(), map->Network(CountryNet::Oil).Nodes().size(),
                        map->Crossings().size());
        }
    }
    return map;
}

std::shared_ptr<const MapData> Range() {
    static std::shared_ptr<const MapData> map;
    if (!map) {
        std::string e;
        map = LoadMap(kData, "maps/kesmark_range.json", &e);
        if (!map) std::printf("    %s\n", e.c_str());
    }
    return map;
}

airwar::TheatreDef LoadTheatre(const std::string& file) {
    airwar::TheatreDef t;
    std::string e;
    if (!airwar::LoadTheatreFile(kData + "/air_war/" + file, t, &e)) std::printf("    %s\n", e.c_str());
    return t;
}

// The theatre the balance pass tuned, and the nation's theatre that extends it with the briefing's assets (N2).
airwar::TheatreDef BaseTheatre() { return LoadTheatre("theatre_pannonia.json"); }
airwar::TheatreDef Theatre() { return LoadTheatre("theatre_pannonia_nation.json"); }

MapPoint At(const Vec3& p) { return {p.x, p.y}; }
MapPoint Km(double e, double n) { return {e * 1000.0, n * 1000.0}; }

bool RefusedCountry(const std::string& text, const std::string& expect) {
    CountryDef d;
    std::string e;
    if (LoadCountryDefJson(text, d, &e)) return false;
    if (e.find(expect) == std::string::npos) std::printf("    unexpected refusal: %s\n", e.c_str());
    return e.find(expect) != std::string::npos;
}

const char* kMinimalCountry = R"({"map_id": "x", "kind": "country", "name": "x", "source": "test", "confidence": "estimate",
  "origin": {"lat_deg": 47, "lon_deg": 19, "alt_m": 100}, "extent_km": {"west": -10, "east": 10, "south": -10, "north": 10},
  "terrain": {"base_m": 100}, "outline": {"points": [[-10, -10], [10, -10], [10, 10], [-10, 10]]},
  "settlements": [{"id": "c", "name": "C", "kind": "capital", "east_km": 0, "north_km": 0, "radius_km": 1, "population": 1000}]})";

}  // namespace

// ---------------------------------------------------------------------------------------------- the country's map

SC_TEST("country map: it loads, and the ground of the Kesmark range inside it is the range's own") {
    std::shared_ptr<const CountryMap> c = Country();
    std::shared_ptr<const MapData> r = Range();
    SC_EXPECT(c != nullptr);
    SC_EXPECT(r != nullptr);
    if (!c || !r) return;
    SC_EXPECT(c->Def().district == "maps/kesmark_range.json");
    // The same ground function over the same terrain: equal everywhere in the range's own map, to the last bit.
    double worst = 0.0;
    const MapDef& rd = r->Def();
    for (double n = rd.south_m; n <= rd.north_m; n += 500.0) {
        for (double e = rd.west_m; e <= rd.east_m; e += 500.0) worst = std::max(worst, std::fabs(c->Natural(e, n) - NaturalGround(rd, e, n)));
    }
    SC_EXPECT(worst < 1e-9);
    // The 1 km grid follows it: within a few metres of the analytic ground on the plain.
    SC_EXPECT_NEAR(c->GroundHeight(3000.0, -4000.0), c->Natural(3000.0, -4000.0), 5.0);
    // The country's hills are where the briefing puts them: the northern hills well above the plain.
    SC_EXPECT(c->GroundHeight(85000.0, 118000.0) > c->GroundHeight(0.0, 0.0) + 250.0);
}

SC_TEST("country map: the grid is the same every time it is built") {
    std::string text;
    SC_EXPECT(detail::ReadTextFile(kData + "/maps/pannonia.json", text));
    CountryDef a, b;
    SC_EXPECT(LoadCountryDefJson(text, a));
    SC_EXPECT(LoadCountryDefJson(text, b));
    std::shared_ptr<const CountryMap> ma = CountryMap::Build(a);
    std::shared_ptr<const CountryMap> mb = CountryMap::Build(b);
    SC_EXPECT(ma->Grid().values == mb->Grid().values);
    SC_EXPECT(ma->Crossings().size() == mb->Crossings().size());
}

SC_TEST("country map: the theatre's objects and our places are inside it; the enemy's entries and bases outside") {
    std::shared_ptr<const CountryMap> c = Country();
    if (!c) return;
    const airwar::TheatreDef t = Theatre();
    for (const airwar::TheatreObject& o : t.objects) {
        if (!c->Inside(o.pos.x, o.pos.y)) std::printf("    outside: %s\n", o.id.c_str());
        SC_EXPECT(c->Inside(o.pos.x, o.pos.y));
        for (const Vec3& q : o.prepared) SC_EXPECT(c->Inside(q.x, q.y));
    }
    for (const airwar::FrontDef& f : t.fronts) {
        const bool civil_war = f.faction == "rebels";
        for (const airwar::EntryPoint& en : f.entries) SC_EXPECT(c->Inside(en.pos.x, en.pos.y) == civil_war);
        SC_EXPECT(c->Inside(f.bases.x, f.bases.y) == civil_war);
        if (f.has_battery_position) SC_EXPECT(c->Inside(f.battery_position.x, f.battery_position.y));
        // Each adversary's entries lie across its own stretch of border.
        if (!civil_war) {
            for (const airwar::EntryPoint& en : f.entries) {
                const CountryBorderDef* b = nullptr;
                c->DistanceToBorder(en.pos.x, en.pos.y, &b);
                SC_EXPECT(b != nullptr && b->neighbour == f.faction);
            }
        }
    }
    command::ChainData chain;
    std::string e;
    SC_EXPECT(command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e));
    for (const command::Place& p : chain.places) SC_EXPECT(c->Inside(p.pos.x, p.pos.y));
}

SC_TEST("country map: the theatre's towns and the range's settlements are the country's") {
    std::shared_ptr<const CountryMap> c = Country();
    std::shared_ptr<const MapData> r = Range();
    if (!c || !r) return;
    const airwar::TheatreDef t = Theatre();
    for (const airwar::TheatreObject& o : t.objects) {
        if (o.kind != airwar::ObjectKind::City) continue;
        const CountrySettlementDef* s = c->Settlement(o.id);
        SC_EXPECT(s != nullptr);
        if (s != nullptr) SC_EXPECT(std::hypot(s->centre.east_m - o.pos.x, s->centre.north_m - o.pos.y) < 500.0);
    }
    for (const MapSettlementDef& rs : r->Def().settlements) {
        const CountrySettlementDef* s = c->Settlement(rs.id);
        SC_EXPECT(s != nullptr);
        if (s != nullptr) SC_EXPECT(std::hypot(s->centre.east_m - rs.centre.east_m, s->centre.north_m - rs.centre.north_m) < 1.0);
    }
    // The capital: on the Rava about 60 km north of Kesmark (decision 5).
    const CountrySettlementDef* cap = c->Capital();
    SC_EXPECT(cap != nullptr);
    if (cap != nullptr) {
        std::string river;
        SC_EXPECT(c->DistanceToRiver(cap->centre.east_m, cap->centre.north_m, &river) < 1500.0);
        SC_EXPECT(river == "rava");
        const CountrySettlementDef* kes = c->Settlement("kesmark");
        SC_EXPECT_NEAR(cap->centre.north_m - kes->centre.north_m, 60000.0, 5000.0);
        const MapPolygonDef* reg = c->RegionAt(cap->centre.east_m, cap->centre.north_m);
        SC_EXPECT(reg != nullptr && reg->id == "capital_region");
    }
    const MapPolygonDef* plain = c->RegionAt(9000.0, -1000.0);
    SC_EXPECT(plain != nullptr && plain->id == "central_plain");
}

SC_TEST("country map: the range's roads and railways are lines of the country's networks, point for point") {
    std::shared_ptr<const CountryMap> c = Country();
    std::shared_ptr<const MapData> r = Range();
    if (!c || !r) return;
    const auto same = [](const MapLineDef& a, const MapLineDef* b) {
        if (b == nullptr || a.points.size() != b->points.size()) return false;
        for (std::size_t i = 0; i < a.points.size(); ++i) {
            if (std::fabs(a.points[i].east_m - b->points[i].east_m) > 0.5 || std::fabs(a.points[i].north_m - b->points[i].north_m) > 0.5) return false;
        }
        return true;
    };
    for (const MapLineDef& l : r->Def().roads) {
        if (l.line_class == "track") continue;  // the range's own tracks to its sites
        SC_EXPECT(same(l, c->Line(CountryNet::Road, l.id)));
    }
    for (const MapLineDef& l : r->Def().railways) SC_EXPECT(same(l, c->Line(CountryNet::Rail, l.id)));
}

SC_TEST("country map: the theatre's bridges are crossings of its networks") {
    std::shared_ptr<const CountryMap> c = Country();
    if (!c) return;
    const airwar::TheatreDef t = Theatre();
    const auto check_bridge = [&](const std::string& id, CountryNet net, const std::string& river) {
        const airwar::TheatreObject* o = t.Find(id);
        SC_EXPECT(o != nullptr);
        if (o == nullptr) return;
        double d = 0.0;
        const CountryCrossing* x = c->NearestCrossing(o->pos.x, o->pos.y, net, &d);
        SC_EXPECT(x != nullptr && d < 300.0);
        if (x != nullptr) SC_EXPECT(x->river_id == river);
    };
    check_bridge("kesmark_road_bridge", CountryNet::Road, "rava");
    check_bridge("kesmark_rail_bridge", CountryNet::Rail, "rava");
    check_bridge("lenta_bridge", CountryNet::Road, "lenta");
    check_bridge("capital_rail_bridge", CountryNet::Rail, "rava");
    check_bridge("mezovar_road_bridge", CountryNet::Road, "tisava");
    // Every bridge of the nation's theatre stands on a crossing of the road or rail network.
    int bridges = 0;
    for (const airwar::TheatreObject& o : t.objects) {
        if (o.kind != airwar::ObjectKind::Bridge) continue;
        ++bridges;
        double dr = 0.0, dl = 0.0;
        const CountryCrossing* xr = c->NearestCrossing(o.pos.x, o.pos.y, CountryNet::Road, &dr);
        const CountryCrossing* xl = c->NearestCrossing(o.pos.x, o.pos.y, CountryNet::Rail, &dl);
        const bool on = (xr != nullptr && dr < 300.0) || (xl != nullptr && dl < 300.0);
        if (!on) std::printf("    bridge %s is on no crossing\n", o.id.c_str());
        SC_EXPECT(on);
    }
    SC_EXPECT(bridges >= 13);
    // The Rava's road bridges: Kesmark, two in the capital, Ralje, Dravec.
    int rava_road = 0;
    for (const CountryCrossing& x : c->Crossings()) rava_road += x.net == CountryNet::Road && x.river_id == "rava" ? 1 : 0;
    SC_EXPECT(rava_road >= 5);
}

SC_TEST("country map: a route goes round a cut bridge, and none is left when every Rava bridge is down") {
    std::shared_ptr<const CountryMap> c = Country();
    if (!c) return;
    const CountryNetwork& roads = c->Network(CountryNet::Road);
    const MapPoint kesmark = Km(9, -1), tiszavar = Km(68, 12);
    const CountryRoute direct = roads.Route(kesmark, tiszavar);
    SC_EXPECT(direct.found);
    SC_EXPECT(direct.length_m > 60000.0 && direct.length_m < 1.4 * 60500.0);  // 60.4 km as the crow flies
    const std::string kesmark_bridge = "b_ralje@rava";
    SC_EXPECT(std::find(direct.crossings.begin(), direct.crossings.end(), kesmark_bridge) != direct.crossings.end());
    // The Kesmark road bridge down: the way goes round by the capital, longer.
    const CountryRoute round = roads.Route(kesmark, tiszavar, {kesmark_bridge});
    SC_EXPECT(round.found);
    SC_EXPECT(round.length_m > direct.length_m + 20000.0);
    SC_EXPECT(std::find(round.crossings.begin(), round.crossings.end(), kesmark_bridge) == round.crossings.end());
    // Every road bridge over the Rava down: Tiszavar, across it, cannot be reached from Kesmark.
    std::set<std::string> all_rava;
    for (const CountryCrossing& x : c->Crossings()) {
        if (x.net == CountryNet::Road && x.river_id == "rava") all_rava.insert(x.id);
    }
    SC_EXPECT(!roads.Route(kesmark, tiszavar, all_rava).found);
    // The missiles' railway: from the technical battery's siding at Horka to the capital, over the Kesmark rail bridge;
    // with it down the siding has no railway to the capital at all.
    const CountryNetwork& rail = c->Network(CountryNet::Rail);
    const MapPoint horka_siding = Km(19.4, -14.2), capital = Km(36, 58);
    const CountryRoute by_rail = rail.Route(horka_siding, capital);
    SC_EXPECT(by_rail.found);
    SC_EXPECT(std::find(by_rail.crossings.begin(), by_rail.crossings.end(), "rail_main@rava") != by_rail.crossings.end());
    SC_EXPECT(!rail.Route(horka_siding, capital, {"rail_main@rava"}).found);
    // The oil: both pipelines reach the refinery, the smaller one over the Lenta and the Rava.
    const CountryNetwork& oil = c->Network(CountryNet::Oil);
    const CountryLandmarkDef* refinery = c->Landmark("refinery");
    SC_EXPECT(refinery != nullptr);
    if (refinery == nullptr) return;
    SC_EXPECT(oil.Route(c->Landmark("ps_main")->at, refinery->at).found);
    const CountryRoute small = oil.Route(c->Landmark("ps_neutral")->at, refinery->at);
    SC_EXPECT(small.found);
    SC_EXPECT(small.crossings.size() == 2);
}

SC_TEST("country map: the loader refuses what it should") {
    CountryDef d;
    SC_EXPECT(LoadCountryDefJson(kMinimalCountry, d));
    std::string t = kMinimalCountry;
    SC_EXPECT(RefusedCountry(std::string(t).replace(t.find("\"country\""), 9, "\"district\""), "must be \"country\""));
    SC_EXPECT(RefusedCountry(std::string(t).replace(t.find("\"capital\""), 9, "\"city\""), "exactly one capital"));
    SC_EXPECT(RefusedCountry(std::string(t).replace(t.find("\"map_id\""), 8, "\"mapid\""), "mapid"));
    std::string lm = t;
    lm.insert(lm.rfind('}'), R"(, "landmarks": [{"id": "l", "name": "L", "kind": "castle", "east_km": 0, "north_km": 0}])");
    SC_EXPECT(RefusedCountry(lm, "unknown landmark kind"));
}

// ---------------------------------------------------------------------------------------------- the order of battle

SC_TEST("order of battle: every military object of the theatre has one unit, every post at most one") {
    command::OrderOfBattle oob;
    std::string e;
    SC_EXPECT(command::LoadOrderOfBattle(kData, "command/oob_pannonia.json", oob, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    command::ChainData chain;
    SC_EXPECT(command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e));
    const airwar::TheatreDef t = Theatre();
    for (const command::OobUnit& u : oob.units) {
        for (const std::string& o : u.objects) {
            if (t.Find(o) == nullptr) std::printf("    %s: no theatre object %s\n", u.id.c_str(), o.c_str());
            SC_EXPECT(t.Find(o) != nullptr);
        }
        for (const std::string& p : u.posts) {
            bool known = false;
            for (const command::Position& pos : chain.positions) known = known || pos.id == p;
            if (!known) std::printf("    %s: no post %s\n", u.id.c_str(), p.c_str());
            SC_EXPECT(known);
            int owners = 0;
            for (const command::OobUnit& v : oob.units) owners += std::count(v.posts.begin(), v.posts.end(), p) > 0 ? 1 : 0;
            SC_EXPECT(owners == 1);
        }
    }
    using airwar::ObjectKind;
    for (const airwar::TheatreObject& o : t.objects) {
        const bool military = o.kind == ObjectKind::SamSite || o.kind == ObjectKind::Aaa || o.kind == ObjectKind::EwRadar ||
                              o.kind == ObjectKind::CommandPost || o.kind == ObjectKind::Airfield || o.kind == ObjectKind::Troops ||
                              o.kind == ObjectKind::Depot || o.kind == ObjectKind::MaterialsDepot || o.kind == ObjectKind::Headquarters;
        if (!military) continue;
        int owners = 0;
        for (const command::OobUnit& u : oob.units) owners += std::count(u.objects.begin(), u.objects.end(), o.id) > 0 ? 1 : 0;
        if (owners != 1) std::printf("    %s has %d units\n", o.id.c_str(), owners);
        SC_EXPECT(owners == 1);
    }
    // The tree: battery 21 under the regiment, the division, the army; the regiment's five batteries and twenty launchers.
    SC_EXPECT(oob.Under("bty_21", "army_3"));
    SC_EXPECT(oob.OwnerOfObject("osa_21") == oob.Find("bty_21"));
    SC_EXPECT(oob.UnitOfPost("bty21_commander") == oob.Find("bty_21"));
    int batteries = 0;
    for (const command::OobUnit* u : oob.Children("sam_15")) batteries += u->kind == "battery" && u->EquipmentCount("osa_9a33") == 4 ? 1 : 0;
    SC_EXPECT(batteries == 5);
    SC_EXPECT(oob.TotalEquipment("sam_15", "osa_9a33") == 20);
    SC_EXPECT(oob.Find("bty_21")->personnel.Total() == 48);
    // The briefing's air defence (§5, §7.1, decisions 3, 4, 6 and 8), each fire unit, battery and company with its object.
    const auto count_under = [&](const std::string& top, const std::string& kind, const std::string& system) {
        int n = 0;
        for (const command::OobUnit& u : oob.units) {
            if (u.kind != kind || !oob.Under(u.id, top)) continue;
            bool has = system.empty();
            for (const std::string& id : u.objects) {
                const airwar::TheatreObject* o = t.Find(id);
                has = has || (o != nullptr && o->defence.present && o->defence.system == system);
            }
            n += has ? 1 : 0;
        }
        return n;
    };
    SC_EXPECT(count_under("s75_brigade", "fire_unit", "s75m2") == 6);
    SC_EXPECT(count_under("s125_regiment", "fire_unit", "s125m1") == 10);
    SC_EXPECT(count_under("ks30_regiment", "battery", "ks30") == 4);
    SC_EXPECT(count_under("mrd_9", "battery", "s60") == 4);
    SC_EXPECT(count_under("sam_15", "battery", "osa") == 5);
    SC_EXPECT(count_under("rtb_brigade", "company", "") >= 10);
    // Six regimental AA batteries, each a Strela-1 platoon and a Shilka platoon; no Strela-10 is left (decision 3).
    int regimental = 0;
    for (const command::OobUnit& u : oob.units) {
        const command::OobUnit* parent = oob.Find(u.parent);
        if (u.kind != "battery" || parent == nullptr || parent->kind != "regiment" || !oob.Under(u.id, "army_3")) continue;
        int strela = 0, shilka = 0;
        for (const std::string& id : u.objects) {
            const airwar::TheatreObject* o = t.Find(id);
            strela += o != nullptr && o->defence.system == "strela1" ? 1 : 0;
            shilka += o != nullptr && o->defence.system == "zsu234" ? 1 : 0;
        }
        regimental += strela == 1 && shilka == 1 ? 1 : 0;
    }
    SC_EXPECT(regimental == 6);
    for (const airwar::TheatreObject& o : t.objects) SC_EXPECT(o.defence.system != "strela10");
    // The Kub is the tank brigade's air defence (decision 6); the MiG-23MLs fly with the 3rd Fighter Regiment (decision 2).
    const command::OobUnit* kub = oob.OwnerOfObject("tiszavar_kub");
    SC_EXPECT(kub != nullptr && oob.Under(kub->id, "tank_brigade"));
    SC_EXPECT(oob.TotalEquipment("fighter_rgt_3", "mig23ml") == 12 && oob.TotalEquipment("national_air_defence", "mig29") == 0);
}

SC_TEST("order of battle: the loader refuses a loop, an unknown parent, service or kind") {
    const auto refused = [](const std::string& units, const std::string& expect) {
        const std::string text = R"({"oob_id": "x", "name": "x", "source": "test", "confidence": "estimate",
          "services": [{"id": "s", "name": "S"}], "units": [)" + units + "]}";
        command::OrderOfBattle o;
        std::string e;
        const bool ok = command::LoadOrderOfBattleJson(text, o, &e);
        if (!ok && e.find(expect) == std::string::npos) std::printf("    unexpected refusal: %s\n", e.c_str());
        return !ok && e.find(expect) != std::string::npos;
    };
    SC_EXPECT(refused(R"({"id": "a", "name": "A", "service": "s", "kind": "army", "parent": "b"},
                         {"id": "b", "name": "B", "service": "s", "kind": "army", "parent": "a"})", "own ancestor"));
    SC_EXPECT(refused(R"({"id": "a", "name": "A", "service": "s", "kind": "army", "parent": "z"})", "unknown unit"));
    SC_EXPECT(refused(R"({"id": "a", "name": "A", "service": "t", "kind": "army"})", "unknown service"));
    SC_EXPECT(refused(R"({"id": "a", "name": "A", "service": "s", "kind": "legion"})", "unknown unit kind"));
    SC_EXPECT(refused(R"({"id": "a", "name": "A", "service": "s", "kind": "army", "equipment": {"t": -1}})", "whole number"));
}

// ---------------------------------------------------------------------------------------------- the air war's new kinds

SC_TEST("air war: the nation's new object kinds parse; the grid stays banned where power is, and NATO's escalation unbans it all") {
    using airwar::ObjectKind;
    for (const char* k : {"refinery", "pipeline_station", "substation", "dam", "materials_depot", "port", "government", "broadcast", "exchange"}) {
        ObjectKind out = ObjectKind::City;
        SC_EXPECT(airwar::ParseObjectKind(k, out));
        SC_EXPECT(std::string(airwar::ObjectKindName(out)) == k);
    }
    const auto banned = [](const airwar::FactionDef& f, ObjectKind k) {
        return std::find(f.politics.banned.begin(), f.politics.banned.end(), k) != f.politics.banned.end();
    };
    std::string e;
    airwar::FactionDef neutral, nato;
    SC_EXPECT(airwar::LoadFactionFile(kData + "/air_war/faction_neutral.json", neutral, &e));
    SC_EXPECT(airwar::LoadFactionFile(kData + "/air_war/faction_nato.json", nato, &e));
    for (ObjectKind k : {ObjectKind::PowerPlant, ObjectKind::Substation, ObjectKind::Dam, ObjectKind::Refinery, ObjectKind::Government,
                         ObjectKind::Broadcast}) {
        SC_EXPECT(banned(neutral, k));
    }
    SC_EXPECT(banned(nato, ObjectKind::Substation) && banned(nato, ObjectKind::Dam));
    bool unbans_grid = false;
    for (const airwar::PoliticalEventDef& ev : nato.politics.events) {
        const auto has = [&](ObjectKind k) { return std::find(ev.unban.begin(), ev.unban.end(), k) != ev.unban.end(); };
        unbans_grid = unbans_grid || (has(ObjectKind::PowerPlant) && has(ObjectKind::Substation) && has(ObjectKind::Dam));
    }
    SC_EXPECT(unbans_grid);
    // Each faction weighs the command and communications kinds.
    for (const char* f : {"rebels", "neutral", "warsaw_pact", "nato"}) {
        airwar::FactionDef fd;
        SC_EXPECT(airwar::LoadFactionFile(kData + "/air_war/faction_" + std::string(f) + ".json", fd, &e));
        SC_EXPECT(fd.politics.target_weight[static_cast<int>(ObjectKind::Exchange)] > 0.0);
    }
}

SC_TEST("air war: the theatre refuses an unknown air defence system; the new systems have the enemy's names") {
    std::string text;
    SC_EXPECT(detail::ReadTextFile(kData + "/air_war/theatre_pannonia.json", text));
    const std::size_t at = text.find("\"system\": \"kub\"");
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    std::string bad = text;
    bad.replace(at, 15, "\"system\": \"sa99\"");
    airwar::TheatreDef t;
    std::string e;
    SC_EXPECT(!airwar::LoadTheatreJson(bad, t, &e));
    SC_EXPECT(e.find("unknown air defence system") != std::string::npos);
    SC_EXPECT(std::string(airwar::FindAirDefenceSystem("strela1")->label) == "SA-9 platoon");
    SC_EXPECT(std::string(airwar::FindAirDefenceSystem("s75m2")->label) == "SA-2 site");
    SC_EXPECT(std::string(airwar::FindAirDefenceSystem("ks30")->label) == "130 mm AAA battery");
    // And ours, as the command post's board writes them.
    SC_EXPECT(std::string(airwar::FindAirDefenceSystem("s125m1")->own) == "С-125М1");
    for (const airwar::AirDefenceSystem& s : airwar::AirDefenceSystems()) SC_EXPECT(std::string(s.own).size() > 0);
    SC_EXPECT(airwar::FindAirDefenceSystem("osa") != nullptr);
}

// ---------------------------------------------------------------------------------------------- the nation's theatre (N2)

SC_TEST("the nation's theatre: the briefing's assets stand on open ground, each mobile unit with its prepared positions") {
    std::shared_ptr<const CountryMap> c = Country();
    std::shared_ptr<const MapData> r = Range();
    if (!c || !r) return;
    using airwar::ObjectKind;
    const airwar::TheatreDef base = BaseTheatre();
    const airwar::TheatreDef t = Theatre();
    const auto field = [](ObjectKind k) {
        return k == ObjectKind::SamSite || k == ObjectKind::EwRadar || k == ObjectKind::Airfield || k == ObjectKind::Troops ||
               k == ObjectKind::CommandPost || k == ObjectKind::Depot || k == ObjectKind::MaterialsDepot;
    };
    const auto clear = [](ObjectKind k) { return k == ObjectKind::SamSite || k == ObjectKind::EwRadar || k == ObjectKind::Airfield; };
    // Open ground: not in a river or a town; what needs a clear horizon or a field not in a wood either, and inside the
    // Kesmark range, whose own map has the woods, villages and farmsteads, nothing standing where it is.
    const auto open = [&](ObjectKind k, const Vec3& p) -> std::string {
        if (!c->Inside(p.x, p.y)) return "outside the country";
        if (!field(k)) return "";
        if (c->DistanceToRiver(p.x, p.y) < 500.0) return "in a river";
        for (const CountrySettlementDef& s : c->Def().settlements) {
            if (std::hypot(s.centre.east_m - p.x, s.centre.north_m - p.y) < s.radius_m + 300.0) return "in " + s.name;
        }
        if (!clear(k)) return "";
        if (c->InForest(p.x, p.y)) return "in a forest";
        if (std::fabs(p.x) < 20000.0 && std::fabs(p.y) < 20000.0 && r->ObstacleHeight(p.x, p.y) > 3.0) return "not open on the range";
        return "";
    };
    int placed = 0, positions = 0;
    for (const airwar::TheatreObject& o : t.objects) {
        const bool own = base.Find(o.id) == nullptr;
        std::string why = own ? open(o.kind, o.pos) : "";
        if (!why.empty()) std::printf("    %s: %s\n", o.id.c_str(), why.c_str());
        SC_EXPECT(why.empty());
        placed += own ? 1 : 0;
        for (std::size_t i = 1; i < o.prepared.size(); ++i) {
            why = open(o.kind, o.prepared[i]);
            if (!why.empty()) std::printf("    %s position %zu: %s\n", o.id.c_str(), i, why.c_str());
            SC_EXPECT(why.empty());
            ++positions;
        }
        if (!o.prepared.empty()) SC_EXPECT(airwar::Dist2d(o.prepared[0], o.pos) == 0.0);
        // Every unit that moves on its own moves between prepared positions (the player's battery: where the player says).
        if (o.relocate_h > 0.0 && !o.player) {
            if (o.prepared.size() < 3) std::printf("    %s has %zu prepared positions\n", o.id.c_str(), o.prepared.size());
            SC_EXPECT(o.prepared.size() >= 3);
        }
        // The nation's works stand at the country's landmarks of their kind.
        if (o.kind == ObjectKind::Refinery || o.kind == ObjectKind::Dam || o.kind == ObjectKind::PipelineStation || o.kind == ObjectKind::Port) {
            double best = 1e12;
            for (const CountryLandmarkDef& l : c->Def().landmarks) {
                if (l.kind == airwar::ObjectKindName(o.kind)) best = std::min(best, std::hypot(l.at.east_m - o.pos.x, l.at.north_m - o.pos.y));
            }
            SC_EXPECT(best < 1000.0);
        }
    }
    // Where battery 21 deploys in each war: open ground too.
    for (const airwar::FrontDef& f : t.fronts) {
        if (!f.has_battery_position) continue;
        const std::string why = open(ObjectKind::SamSite, f.battery_position);
        if (!why.empty()) std::printf("    battery 21's position against %s: %s\n", f.faction.c_str(), why.c_str());
        SC_EXPECT(why.empty());
    }
    std::printf("    %d assets placed, %d prepared positions\n", placed, positions);
    SC_EXPECT(placed >= 120 && positions >= 100);
}

SC_TEST("the nation's theatre extends the tuned one: the base's objects keep their places; the loader refuses what it should") {
    const airwar::TheatreDef base = BaseTheatre();
    const airwar::TheatreDef t = Theatre();
    SC_EXPECT(t.id == "pannonia_nation" && t.fronts.size() == base.fronts.size() && t.country_name == base.country_name);
    SC_EXPECT(t.objects.size() > base.objects.size() + 100);
    bool in_place = t.objects.size() > base.objects.size();
    // The written objects (a supply line's columns are made again after them, docs/air_war/14_LOGISTICS.md).
    for (std::size_t i = 0; in_place && i < base.objects.size(); ++i) {
        if (base.objects[i].supply_line < 0) in_place = t.objects[i].id == base.objects[i].id;
    }
    SC_EXPECT(in_place);
    // A replaced object: the S-75 at Novo Varosd is the S-75M2 (decision 1), where it stood, with its prepared sites.
    const airwar::TheatreObject* v = t.Find("varosd_s75");
    const airwar::TheatreObject* vb = base.Find("varosd_s75");
    SC_EXPECT(v != nullptr && vb != nullptr);
    if (v != nullptr && vb != nullptr) {
        SC_EXPECT(v->defence.system == "s75m2" && vb->defence.system == "s75");
        SC_EXPECT(airwar::Dist2d(v->pos, vb->pos) == 0.0 && v->prepared.size() == 3 && vb->prepared.empty());
    }
    SC_EXPECT(t.PlayerBattery() != nullptr && t.PlayerBattery()->id == "osa_21");

    const std::string head = R"({"theatre_id": "t", "name": "t", "source": "sam3d", "confidence": "estimate",
      "origin": {"lat_deg": 47, "lon_deg": 19, "alt_m": 100},
      "fronts": [{"faction": "rebels", "name": "f", "line": [{"east_km": -5, "north_km": -20}, {"east_km": 5, "north_km": -20}],
                  "entries": [{"id": "S1", "east_km": 0, "north_km": -30}], "bases": {"east_km": 0, "north_km": -40}}],
      "objects": [{"id": "b", "name": "b", "kind": "sam_site", "east_km": 0, "north_km": 5, "player": true,
                   "defence": {"system": "osa", "radius_km": 10, "lethality": 0.2}})";
    const auto load = [&](const std::string& top, const std::string& more, std::string& e) {
        airwar::TheatreDef x;
        std::string text = head;
        text.insert(1, top);
        const bool ok = airwar::LoadTheatreJson(text + more + "]}", x, &e);
        return ok ? static_cast<int>(x.objects.back().prepared.size()) : -1;
    };
    std::string e;
    SC_EXPECT(load("", R"(, {"id": "s", "name": "s", "kind": "sam_site", "east_km": 3, "north_km": 5, "mobile": true,
                          "relocate_h": 24, "positions": [{"east_km": 3, "north_km": 9}, {"east_km": 6, "north_km": 5}]})", e) == 3);
    SC_EXPECT(load("\"extends\": \"theatre_pannonia.json\", ", "", e) < 0 && e.find("only a theatre file can extend") != std::string::npos);
    SC_EXPECT(load("\"remove\": [\"b\"], ", "", e) < 0 && e.find("only a theatre that extends") != std::string::npos);
    SC_EXPECT(load("", R"(, {"id": "s", "name": "s", "kind": "depot", "east_km": 3, "north_km": 5,
                          "positions": [{"east_km": 4, "north_km": 5}]})", e) < 0 && e.find("mobile unit") != std::string::npos);
    SC_EXPECT(load("", R"(, {"id": "s", "name": "s", "kind": "troops", "east_km": 3, "north_km": 5, "mobile": true, "relocate_h": 12,
                          "positions": [{"east_km": 3.1, "north_km": 5}]})", e) < 0 && e.find("within 300 m") != std::string::npos);
    std::string player = head;
    player.replace(player.find("\"player\": true"), 14, "\"player\": true, \"positions\": [{\"east_km\": 2, \"north_km\": 5}]");
    airwar::TheatreDef x;
    SC_EXPECT(!airwar::LoadTheatreJson(player + "]}", x, &e) && e.find("the player chooses") != std::string::npos);
}

SC_TEST("air war on the nation's theatre: a unit with prepared positions moves only between them") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_rebels.json", d, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(d, 7u));
    while (c.Now() < 72.0) {
        if (c.Advance(72.0)) c.SkipPlayerMission();
    }
    int with = 0, moved = 0, off = 0;
    for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
        const airwar::TheatreObject& o = d.theatre.objects[i];
        if (o.prepared.empty()) continue;
        ++with;
        const Vec3 at = c.Truth()[i].pos;
        bool on = false;
        for (const Vec3& q : o.prepared) on = on || airwar::Dist2d(q, at) < 1.0;
        off += on ? 0 : 1;
        moved += airwar::Dist2d(at, o.pos) > 1.0 ? 1 : 0;
    }
    std::printf("    %d units with prepared positions, %d moved in three days, %d off them\n", with, moved, off);
    SC_EXPECT(with > 50 && moved > with / 2 && off == 0);
}

SC_TEST("country map: its landmarks stand where the theatre has those objects; the chain's places where the country has its towns") {
    std::shared_ptr<const CountryMap> c = Country();
    if (!c) return;
    const airwar::TheatreDef t = Theatre();
    int compared = 0;
    // The refinery, the dam, the pipeline stations and the port: each has the theatre's object of its kind on it.
    for (const CountryLandmarkDef& l : c->Def().landmarks) {
        if (l.kind == "border_crossing") continue;
        double best = 1e18;
        for (const airwar::TheatreObject& o : t.objects) {
            if (std::string(airwar::ObjectKindName(o.kind)) != l.kind) continue;
            best = std::min(best, std::hypot(o.pos.x - l.at.east_m, o.pos.y - l.at.north_m));
        }
        if (best > 1000.0) std::printf("    no %s object at the landmark %s (nearest %.0f m)\n", l.kind.c_str(), l.id.c_str(), best);
        SC_EXPECT(best <= 1000.0);
        ++compared;
    }
    // The chain of command's places: a town or village of the country where the country has it, a post at its object.
    command::ChainData chain;
    std::string e;
    SC_EXPECT(command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e));
    for (const command::Place& p : chain.places) {
        if (const CountrySettlementDef* s = c->Settlement(p.id)) {
            const double d = std::hypot(s->centre.east_m - p.pos.x, s->centre.north_m - p.pos.y);
            if (d > 10.0) std::printf("    %s is %.0f m off the country's\n", p.id.c_str(), d);
            SC_EXPECT(d <= 10.0);
            ++compared;
        }
        if (!p.theatre_object.empty()) {
            const airwar::TheatreObject* o = t.Find(p.theatre_object);
            SC_EXPECT(o != nullptr);
            if (o != nullptr) SC_EXPECT(std::hypot(o->pos.x - p.pos.x, o->pos.y - p.pos.y) <= 1000.0);
            ++compared;
        }
    }
    std::printf("    %d positions compared\n", compared);
    SC_EXPECT(compared >= 15);
}

SC_TEST("the nation's theatre: a whole nation's war is fought by a larger force (the front's force_scale); the tuned theatre's is the faction's") {
    airwar::AirWarData tuned, nation;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", tuned, &e));
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", nation, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const airwar::FrontDef* f = nation.theatre.FrontFor("warsaw_pact");
    SC_EXPECT(f != nullptr && f->force_scale == 2.0);
    int a = 0, b = 0;
    for (const airwar::SquadronDef& s : tuned.faction.squadrons) a += s.count;
    for (const airwar::SquadronDef& s : nation.faction.squadrons) b += s.count;
    std::printf("    the Pact: %d aircraft against the district, %d against the nation\n", a, b);
    SC_EXPECT(b == 2 * a);
    SC_EXPECT_NEAR(nation.faction.doctrine.max_packages_per_cycle, 2.0 * tuned.faction.doctrine.max_packages_per_cycle, 1e-9);
    // The cruise missiles are the long-range aviation's, not the front's: their stocks stay.
    for (std::size_t i = 0; i < tuned.faction.aircraft.size(); ++i) {
        if (tuned.faction.aircraft[i].missile) SC_EXPECT(nation.faction.aircraft[i].stock == tuned.faction.aircraft[i].stock);
    }
    airwar::AirWarData nato;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_nato.json", nato, &e));
    SC_EXPECT(nato.theatre.FrontFor("nato") != nullptr && nato.theatre.FrontFor("nato")->force_scale == 1.5);
    // The loader keeps it within reason.
    airwar::TheatreDef t;
    std::string text;
    SC_EXPECT(detail::ReadTextFile(kData + "/air_war/theatre_pannonia.json", text));
    const std::string key = "\"friendly_fighters\": 0.35";
    const std::size_t at = text.find(key);
    SC_EXPECT(at != std::string::npos);
    if (at != std::string::npos) {
        std::string bad = text;
        bad.insert(at, "\"force_scale\": 9, ");
        airwar::detail::AirWarLoader loader;
        json::Value root;
        SC_EXPECT(json::Parse(bad, root, &e));
        SC_EXPECT(!loader.LoadTheatre(root, t));
        SC_EXPECT(loader.Message().find("force_scale") != std::string::npos);
    }
}

SC_TEST("air war on the nation's theatre: every adversary's war runs two days, the same every time for a seed") {
    for (const char* faction : {"rebels", "neutral", "warsaw_pact", "nato"}) {
        airwar::AirWarData d;
        std::string e;
        SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", std::string("air_war/faction_") + faction + ".json", d, &e));
        if (!e.empty()) std::printf("    %s\n", e.c_str());
        std::string runs[3];
        int packages = 0;
        for (int k = 0; k < 3; ++k) {
            airwar::AirCampaign c;
            SC_EXPECT(c.Init(d, k < 2 ? 7u : 8u));
            while (c.Now() < 48.0) {
                if (c.Advance(48.0)) c.SkipPlayerMission();
            }
            runs[k] = airwar::SaveCampaignJson(c, "air_war/theatre_pannonia_nation.json", std::string("air_war/faction_") + faction + ".json");
            if (k == 0) packages = static_cast<int>(c.Packages().size());
        }
        std::printf("    %s: %d packages in two days\n", faction, packages);
        SC_EXPECT(packages > 0);
        SC_EXPECT(runs[0] == runs[1]);  // the same seed, the same war
        SC_EXPECT(runs[0] != runs[2]);  // another seed, another
    }
}

// ---------------------------------------------------------------------------------------------- the terrain

SC_TEST("country map: its terrain: ranges along the borders, the rivers' gorges through them, the reservoir above the dam, a lake and marshes") {
    const std::shared_ptr<const CountryMap> c = Country();
    SC_EXPECT(c != nullptr);
    if (c == nullptr) return;
    const MapDef& g = c->Def().ground;
    SC_EXPECT(g.ridges.size() >= 5 && g.valleys.size() >= 4 && g.lakes.size() >= 2 && g.marshes.size() >= 2);
    // The ranges stand high: the border range's crest some hundreds of metres above the plain.
    const auto crest = [&](const MapRidgeDef& r) {
        double best = 0.0;
        for (std::size_t i = 0; i + 1 < r.points.size(); ++i) {
            for (double f = 0.0; f <= 1.0; f += 0.05) {
                const double e = r.points[i].east_m + f * (r.points[i + 1].east_m - r.points[i].east_m);
                const double n = r.points[i].north_m + f * (r.points[i + 1].north_m - r.points[i].north_m);
                best = std::max(best, c->GroundHeight(e, n));
            }
        }
        return best;
    };
    for (const MapRidgeDef& r : g.ridges) SC_EXPECT(crest(r) > g.base_m + 0.6 * r.height_m);
    // A river crosses a range in its gorge: the floor near the plain, the shoulders high above it.
    const MapRidgeDef* border = nullptr;
    for (const MapRidgeDef& r : g.ridges) border = r.id == "pact_border_range" ? &r : border;
    SC_EXPECT(border != nullptr);
    const double floor = c->GroundHeight(Km(52.3, 120.0).east_m, Km(52.3, 120.0).north_m);
    const double shoulder = c->GroundHeight(Km(57.3, 120.0).east_m, Km(57.3, 120.0).north_m);
    SC_EXPECT(floor < g.base_m + 60.0 && shoulder > floor + 150.0);
    // Valleys never cut below the plain.
    for (const MapRidgeDef& v : g.valleys) {
        for (const MapPoint& p : v.points) SC_EXPECT(c->GroundHeight(p.east_m, p.north_m) > g.base_m - 20.0);
    }
    // The lakes: the ground under them below their level, their shores at it or above (within a grid cell's reach).
    for (const MapLakeDef& l : g.lakes) {
        double ce = 0.0, cn = 0.0;
        for (const MapPoint& p : l.points) {
            ce += p.east_m / static_cast<double>(l.points.size());
            cn += p.north_m / static_cast<double>(l.points.size());
        }
        SC_EXPECT(NaturalGround(g, ce, cn) < l.level_m);
        SC_EXPECT(std::string(c->LandCoverAt(ce, cn)) == "lake");
    }
    // The reservoir lies above the dam, on the Rava.
    const CountryLandmarkDef* dam = c->Landmark("rava_dam");
    const MapLakeDef* reservoir = nullptr;
    for (const MapLakeDef& l : g.lakes) reservoir = l.id == "rava_reservoir" ? &l : reservoir;
    SC_EXPECT(dam != nullptr && reservoir != nullptr);
    if (dam != nullptr && reservoir != nullptr) {
        double closest = 1e18;
        for (const MapPoint& p : reservoir->points) closest = std::min(closest, std::hypot(p.east_m - dam->at.east_m, p.north_m - dam->at.north_m));
        SC_EXPECT(closest < 1500.0);
        bool upstream = true;
        for (const MapPoint& p : reservoir->points) upstream = upstream && p.north_m >= dam->at.north_m - 500.0;
        SC_EXPECT(upstream);
    }
    // Land cover: the marshes are marsh, a forest is forest, the capital town, the plain open.
    SC_EXPECT(std::string(c->LandCoverAt(Km(40, -33).east_m, Km(40, -33).north_m)) == "marsh");
    SC_EXPECT(std::string(c->LandCoverAt(Km(-11, -3).east_m, Km(-11, -3).north_m)) == "forest");
    SC_EXPECT(std::string(c->LandCoverAt(Km(36, 58).east_m, Km(36, 58).north_m)) == "town");
    SC_EXPECT(std::string(c->LandCoverAt(Km(-60, 60).east_m, Km(-60, 60).north_m)) == "open");
    // No asset of the nation's theatre stands in a lake, and the secondary roads reach the western and south-western groups.
    const airwar::TheatreDef t = Theatre();
    for (const airwar::TheatreObject& o : t.objects) {
        if (o.supply_line >= 0) continue;
        const std::string cover = c->LandCoverAt(o.pos.x, o.pos.y);
        if (cover == "lake") std::printf("    %s stands in a lake\n", o.id.c_str());
        SC_EXPECT(cover != "lake");
    }
    const CountryNetwork& roads = c->Network(CountryNet::Road);
    for (const char* id : {"group_west", "group_southwest", "sw_depot"}) {
        const airwar::TheatreObject* o = t.Find(id);
        SC_EXPECT(o != nullptr && roads.NearestNode(At(o->pos), 12000.0) >= 0);
    }
    std::printf("    %zu ranges, %zu gorges, %zu lakes, %zu marshes; the border range's gorge: floor %.0f m, shoulder %.0f m\n", g.ridges.size(),
                g.valleys.size(), g.lakes.size(), g.marshes.size(), floor, shoulder);
}
