// Our supply lines and the enemy's armed reconnaissance (simcore/air_war/logistics.hpp, docs/air_war/14_LOGISTICS.md).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

AirWarData LoadOn(const char* theatre, const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, std::string("air_war/") + theatre, "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

std::vector<ObjectState> TruthOf(const AirWarData& d) {
    std::vector<ObjectState> truth;
    for (const TheatreObject& o : d.theatre.objects) {
        ObjectState s;
        s.id = o.id;
        s.pos = o.pos;
        s.missiles = o.defence.present ? o.defence.missiles : -1;
        truth.push_back(s);
    }
    return truth;
}

int LineIndex(const AirWarData& d, const std::string& id) {
    for (std::size_t i = 0; i < d.theatre.supply_lines.size(); ++i) {
        if (d.theatre.supply_lines[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

// Hours of the supply lines alone (no enemy), day and night as the theatre's clock has them.
void Run(const AirWarData& d, SupplyNetwork& net, std::vector<ObjectState>& truth, double& h, double hours, std::vector<std::string>* events) {
    for (double end = h + hours; h < end; h += 1.0) {
        net.Step(d, truth, 1.0, DayPartAt(d.theatre.start_hour + h) != DayPart::Day, events);
    }
}

bool Has(const std::vector<std::string>& events, const std::string& what) {
    for (const std::string& e : events) {
        if (e.find(what) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

SC_TEST("logistics: the supply lines load and their columns are theatre objects at their depots; the loader refuses a bad line") {
    const AirWarData d = LoadOn("theatre_pannonia.json", "rebels");
    SC_EXPECT(d.theatre.supply_lines.size() == 5);
    int columns = 0, made = 0;
    for (const SupplyLineDef& l : d.theatre.supply_lines) columns += l.columns;
    for (const TheatreObject& o : d.theatre.objects) {
        if (o.supply_line < 0) continue;
        ++made;
        const SupplyLineDef& l = d.theatre.supply_lines[static_cast<std::size_t>(o.supply_line)];
        SC_EXPECT(o.kind == l.column && IsColumn(o.kind) && o.mobile && o.move_mps == l.speed_mps);
        SC_EXPECT(Dist2d(o.pos, d.theatre.Find(l.from)->pos) < 1.0);
        SC_EXPECT(d.TargetOf(o).aim_points > 0);
    }
    SC_EXPECT(made == columns);
    const SupplyLineDef* march = nullptr;
    for (const SupplyLineDef& l : d.theatre.supply_lines) march = l.id == "north_march" ? &l : march;
    SC_EXPECT(march != nullptr && march->column == ObjectKind::Column && march->night_only && march->defence.present);
    std::printf("    %d lines, %d columns\n", static_cast<int>(d.theatre.supply_lines.size()), made);

    const auto refused = [](const std::string& line, const std::string& why) {
        const std::string text = R"({"theatre_id": "t", "name": "t", "source": "test", "confidence": "estimate",
            "origin": {"lat_deg": 47, "lon_deg": 19, "alt_m": 100},
            "fronts": [{"faction": "nato", "name": "f", "line": [{"east_km": -10, "north_km": 50}, {"east_km": 10, "north_km": 50}],
                        "entries": [{"id": "W1", "east_km": 0, "north_km": 60}], "bases": {"east_km": 0, "north_km": 100}}],
            "objects": [{"id": "bat", "name": "b", "kind": "sam_site", "east_km": 0, "north_km": 0, "player": true,
                         "defence": {"system": "osa", "radius_km": 10, "lethality": 0.2}},
                        {"id": "dep", "name": "d", "kind": "depot", "east_km": 0, "north_km": -20},
                        {"id": "tr", "name": "t", "kind": "troops", "east_km": 0, "north_km": 30}],
            "supply_lines": [)" + line + "]}";
        TheatreDef t;
        std::string e;
        const bool ok = LoadTheatreJson(text, t, &e);
        if (ok || e.find(why) == std::string::npos) std::printf("    expected a refusal with '%s', got '%s'\n", why.c_str(), e.c_str());
        return !ok && e.find(why) != std::string::npos;
    };
    const std::string good = R"({"id": "l", "name": "l", "from": "dep", "to": "tr", "source": "test", "confidence": "estimate"})";
    {
        TheatreDef t;
        std::string e;
        const std::string text = R"({"theatre_id": "t", "name": "t", "source": "test", "confidence": "estimate",
            "origin": {"lat_deg": 47, "lon_deg": 19, "alt_m": 100},
            "fronts": [{"faction": "nato", "name": "f", "line": [{"east_km": -10, "north_km": 50}, {"east_km": 10, "north_km": 50}],
                        "entries": [{"id": "W1", "east_km": 0, "north_km": 60}], "bases": {"east_km": 0, "north_km": 100}}],
            "objects": [{"id": "bat", "name": "b", "kind": "sam_site", "east_km": 0, "north_km": 0, "player": true,
                         "defence": {"system": "osa", "radius_km": 10, "lethality": 0.2}},
                        {"id": "dep", "name": "d", "kind": "depot", "east_km": 0, "north_km": -20},
                        {"id": "tr", "name": "t", "kind": "troops", "east_km": 0, "north_km": 30}],
            "supply_lines": [)" + good + "]}";
        SC_EXPECT(LoadTheatreJson(text, t, &e));
        if (!e.empty()) std::printf("    %s\n", e.c_str());
        SC_EXPECT(t.objects.size() == 5 && t.Find("l_1") != nullptr && t.Find("l_2") != nullptr);
    }
    SC_EXPECT(refused(R"({"id": "l", "name": "l", "from": "nowhere", "to": "tr", "source": "test", "confidence": "estimate"})", "supply_lines[0].from"));
    SC_EXPECT(refused(R"({"id": "l", "name": "l", "from": "dep", "to": "dep", "source": "test", "confidence": "estimate"})", "is not troops"));
    SC_EXPECT(refused(R"({"id": "l", "name": "l", "from": "dep", "to": "tr", "column": "train", "source": "test", "confidence": "estimate"})", "a train runs by rail"));
    SC_EXPECT(refused(R"({"id": "l", "name": "l", "from": "dep", "to": "tr", "column": "barge", "source": "test", "confidence": "estimate"})", "convoy, train or column"));
    SC_EXPECT(refused(R"({"id": "l", "name": "l", "from": "dep", "to": "tr", "speed_kmh": 500, "source": "test", "confidence": "estimate"})", "speed_kmh"));
}

SC_TEST("logistics: the columns go round and keep the troops supplied; a bridge down cuts the line, the troops' stocks run out and they weaken; mended, it opens") {
    const AirWarData d = LoadOn("theatre_pannonia.json", "rebels");
    std::vector<ObjectState> truth = TruthOf(d);
    SupplyNetwork net;
    net.Init(d, truth);
    const int north = LineIndex(d, "north_road");
    const int group = d.theatre.IndexOf("group_north");
    const int bridge = d.theatre.IndexOf("kesmark_road_bridge");
    SC_EXPECT(north >= 0 && group >= 0 && bridge >= 0);
    if (north < 0 || group < 0 || bridge < 0) return;
    const SupplyPath& path = net.Path(static_cast<std::size_t>(north));
    SC_EXPECT(!path.cut && path.crossings.size() == 1 && path.crossings[0] == "kesmark_road_bridge");
    // The columns leave one after another, not all at once.
    int at_depot = 0;
    for (std::size_t i = 0; i < truth.size(); ++i) at_depot += d.theatre.objects[i].supply_line == north && truth[i].wait_h < 0.0 ? 1 : 0;
    SC_EXPECT(at_depot >= 3);
    double h = 0.0;
    std::vector<std::string> events;
    Run(d, net, truth, h, 96.0, &events);
    SC_EXPECT(net.Ledger().delivered > 1.0);
    for (int j : net.Served()) SC_EXPECT(truth[static_cast<std::size_t>(j)].supply >= 0.8);
    // The columns are spread along the road, some on their way.
    int moving = 0;
    for (std::size_t i = 0; i < truth.size(); ++i) moving += d.theatre.objects[i].supply_line >= 0 && (truth[i].leg == 1 || truth[i].leg == 3) ? 1 : 0;
    SC_EXPECT(moving >= 3);
    std::printf("    after four days: %.2f delivered, %d columns on the move, the northern group's stocks %.0f%%\n", net.Ledger().delivered, moving,
                100.0 * truth[static_cast<std::size_t>(group)].supply);

    // The Kesmark bridge down: the northern road is cut; its columns stop where they are.
    truth[static_cast<std::size_t>(bridge)].function = 0.0;
    std::vector<Vec3> held;
    Run(d, net, truth, h, 1.0, &events);
    SC_EXPECT(net.Path(static_cast<std::size_t>(north)).cut && Has(events, "is cut"));
    for (std::size_t i = 0; i < truth.size(); ++i) {
        if (d.theatre.objects[i].supply_line == north) held.push_back(truth[i].pos);
    }
    Run(d, net, truth, h, 120.0, &events);
    std::size_t k = 0;
    bool still = true;
    for (std::size_t i = 0; i < truth.size(); ++i) {
        if (d.theatre.objects[i].supply_line != north) continue;
        if (truth[i].leg == 1 || truth[i].leg == 3) still = still && Dist2d(truth[i].pos, held[k]) < 1.0;
        ++k;
    }
    SC_EXPECT(still);
    const ObjectState& g = truth[static_cast<std::size_t>(group)];
    SC_EXPECT(g.supply <= 0.0 && g.function < 1.0 && g.function >= d.tuning.Get("logistics.unsupplied_floor"));
    SC_EXPECT(net.Ledger().line_hours_cut >= 120.0);
    std::printf("    five days cut: the northern group's stocks %.0f%%, its strength %.0f%%\n", 100.0 * g.supply, 100.0 * g.function);

    // Mended: open again, and the stocks come back.
    truth[static_cast<std::size_t>(bridge)].function = 1.0;
    Run(d, net, truth, h, 48.0, &events);
    SC_EXPECT(!net.Path(static_cast<std::size_t>(north)).cut && Has(events, "is open again"));
    SC_EXPECT(truth[static_cast<std::size_t>(group)].supply > 0.0);
}

SC_TEST("logistics on the nation's map: every line finds its way on its network; a bridge down sends a road round or cuts it, and the southern railway is cut at Horka") {
    const AirWarData d = LoadOn("theatre_pannonia_nation.json", "nato");
    SC_EXPECT(d.country != nullptr && d.theatre.supply_lines.size() >= 12);
    if (d.country == nullptr) return;
    std::vector<ObjectState> truth = TruthOf(d);
    SupplyNetwork net;
    net.Init(d, truth);
    int on_network = 0;
    for (std::size_t li = 0; li < d.theatre.supply_lines.size(); ++li) {
        const SupplyLineDef& l = d.theatre.supply_lines[li];
        const SupplyPath& p = net.Path(li);
        const double straight = Dist2d(d.theatre.Find(l.from)->pos, d.theatre.Find(l.to)->pos);
        SC_EXPECT(!p.cut && p.Length() >= straight - 1.0 && p.Length() <= 3.0 * straight + 30000.0);
        on_network += p.pts.size() > 3 ? 1 : 0;
        std::printf("    %s: %zu points, %.0f km (straight %.0f km), %zu bridges\n", l.id.c_str(), p.pts.size(), p.Length() / 1000.0,
                    straight / 1000.0, p.crossings.size());
    }
    SC_EXPECT(on_network >= 12);  // the rest run on local roads, their troops or depots off the map's network
    const int rail = LineIndex(d, "south_rail");
    SC_EXPECT(rail >= 0);
    if (rail < 0) return;
    SC_EXPECT(net.Path(static_cast<std::size_t>(rail)).Length() > 150000.0);

    // Every bridge one at a time: each line over it goes round by another crossing or is cut.
    int reroutes = 0, cuts = 0;
    for (std::size_t b = 0; b < d.theatre.objects.size(); ++b) {
        if (d.theatre.objects[b].kind != ObjectKind::Bridge) continue;
        std::vector<ObjectState> t = TruthOf(d);
        SupplyNetwork n;
        n.Init(d, t);
        std::vector<SupplyPath> before;
        for (std::size_t li = 0; li < n.Lines(); ++li) before.push_back(n.Path(li));
        t[b].function = 0.0;
        n.Step(d, t, 1.0, false, nullptr);
        for (std::size_t li = 0; li < n.Lines(); ++li) {
            const SupplyPath& after = n.Path(li);
            if (after.crossings == before[li].crossings && after.cut == before[li].cut) continue;
            if (after.cut) {
                ++cuts;
                continue;
            }
            ++reroutes;
            SC_EXPECT(after.Length() >= before[li].Length() - 1.0);
        }
    }
    SC_EXPECT(reroutes >= 1 && cuts >= 1);
    // The Horka rail bridge: no other railway south.
    {
        std::vector<ObjectState> t = TruthOf(d);
        SupplyNetwork n;
        n.Init(d, t);
        t[static_cast<std::size_t>(d.theatre.IndexOf("horka_rail_bridge"))].function = 0.2;
        n.Step(d, t, 1.0, false, nullptr);
        SC_EXPECT(n.Path(static_cast<std::size_t>(rail)).cut);
    }
    std::printf("    %d of %d lines on the network; one bridge down at a time: %d lines sent round, %d cut\n", on_network,
                static_cast<int>(d.theatre.supply_lines.size()), reroutes, cuts);
}

SC_TEST("logistics: an armed reconnaissance finds a column on the move on its stretch, by day more than by night, and never one in its depot") {
    AirWarData d = LoadOn("theatre_pannonia.json", "nato");
    for (TheatreObject& o : d.theatre.objects) o.defence.present = false;  // the search alone
    for (FrontDef& f : d.theatre.fronts) f.friendly_fighters = 0.0;
    const int line = LineIndex(d, "south_road");
    SC_EXPECT(line >= 0);
    if (line < 0) return;
    AirStaff staff;
    Rng srng(3);
    staff.Build(d, srng, 0.0);
    BatteryPosture posture;
    Rng rng(9);
    const AircraftType& a10 = *d.faction.Aircraft("a10a");
    const auto search = [&](DayPart part, int leg, int runs) {
        int found = 0;
        for (int r = 0; r < runs; ++r) {
            std::vector<ObjectState> truth = TruthOf(d);
            SupplyNetwork net;
            net.Init(d, truth);
            const SupplyPath& path = net.Path(static_cast<std::size_t>(line));
            const Vec3 mid = path.At(0.5 * path.Length());
            int first = -1;
            for (std::size_t i = 0; i < truth.size(); ++i) {
                if (d.theatre.objects[i].supply_line != line) continue;
                truth[i].leg = 0;
                truth[i].pos = path.At(0.0);
                if (first < 0) first = static_cast<int>(i);
            }
            truth[static_cast<std::size_t>(first)].leg = leg;
            if (leg == 1) truth[static_cast<std::size_t>(first)].pos = mid;
            Package p;
            p.kind = MissionKind::ArmedRecce;
            p.part = part;
            p.target_object = d.theatre.objects[static_cast<std::size_t>(first)].id;
            Flight f;
            f.callsign = "Hog";
            f.role = FlightRole::Strike;
            f.aircraft = a10.id;
            f.count = 2;
            f.profile = Profile::Low;
            f.weapon = "rockeye";
            f.weapons = 4;
            f.delivery = Delivery::Dive;
            f.aim_pos = mid + Vec3{4000.0, 3000.0, 0.0};
            f.route = {{mid + Vec3{0.0, -60000.0, 0.0}, 100.0, 0.0, "entry"},
                       {f.aim_pos, 100.0, 0.0, "target"},
                       {mid + Vec3{20000.0, -60000.0, 0.0}, 100.0, 0.0, "exit"}};
            TimeRoute(f.route, 1, 1.0, a10.attack_mps);
            f.release_index = 1;
            p.flights.push_back(f);
            ResolveContext rc;
            rc.data = &d;
            rc.truth = &truth;
            rc.posture = &posture;
            rc.staff = &staff;
            rc.rng = &rng;
            Resolver res;
            res.Resolve(p, rc);
            found += p.found.empty() ? 0 : 1;
        }
        return static_cast<double>(found) / runs;
    };
    const double day = search(DayPart::Day, 1, 300), night = search(DayPart::Night, 1, 300), depot = search(DayPart::Day, 0, 100);
    std::printf("    a column on the road found by day %.0f%%, by night %.0f%%; one loading in its depot %.0f%%\n", 100.0 * day, 100.0 * night,
                100.0 * depot);
    SC_EXPECT(std::fabs(day - d.tuning.Get("logistics.find_day")) < 0.08);
    SC_EXPECT(std::fabs(night - d.tuning.Get("logistics.find_night")) < 0.07);
    SC_EXPECT(depot == 0.0);
    // Where a column was seen spreads with its speed.
    IntelPicture intel;
    IntelReport r;
    r.object_id = "south_road_1";
    r.kind = ObjectKind::Convoy;
    r.sigma_m = 300.0;
    r.move_mps = 7.0;
    intel.Queue(r);
    intel.Deliver(1.0, 1.0);
    intel.Decay(1.0, d.tuning);
    SC_EXPECT(intel.entries.size() == 1 && intel.entries[0].mobile && intel.entries[0].sigma_m > 300.0 + 0.4 * 7.0 * 3600.0);
}

SC_TEST("logistics: NATO and the Pact search our lines by armed reconnaissance and find columns about as often as their planners expect") {
    for (const char* faction : {"warsaw_pact", "nato"}) {
        const AirWarData d = LoadOn("theatre_pannonia.json", faction);
        int searches = 0, found = 0;
        for (unsigned seed = 1; seed <= 2; ++seed) {
            AirCampaign c;
            SC_EXPECT(c.Init(d, seed));
            while (c.Now() < 7.0 * 24.0) {
                if (c.Advance(7.0 * 24.0)) c.SkipPlayerMission();
            }
            for (const Package& p : c.Packages()) {
                if (p.kind != MissionKind::ArmedRecce || !p.resolved) continue;
                ++searches;
                found += p.found.empty() ? 0 : 1;
                SC_EXPECT(p.Has(FlightRole::Strike));
            }
            SC_EXPECT(c.Supply().Ledger().delivered > 0.0);
        }
        std::printf("    %s: %d armed reconnaissance searches in two weeks of war, a column found in %d\n", faction, searches, found);
        SC_EXPECT(searches >= 4 && found >= 1 && found < searches);
    }
}
