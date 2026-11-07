// The weather (simcore/weather.hpp, data/weather/presets.json) and the generated map (simcore/terrain_map.hpp,
// data/maps/kesmark_range.json). Hand calculations in the comments.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/aircraft.hpp"
#include "simcore/em/optical.hpp"
#include "simcore/json.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/solar.hpp"
#include "simcore/terrain_map.hpp"
#include "simcore/weather.hpp"
#include "simcore/weather_data.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

WeatherPreset Preset(const std::string& id) {
    WeatherPreset p;
    std::string e;
    if (!FindWeatherPreset(kData, id, p, &e)) std::printf("    %s\n", e.c_str());
    return p;
}

std::shared_ptr<const MapData> Kesmark() {
    static std::shared_ptr<const MapData> map;
    if (!map) {
        const auto t0 = std::chrono::steady_clock::now();
        std::string e;
        map = LoadMap(kData, "maps/kesmark_range.json", &e);
        if (!map) {
            std::printf("    %s\n", e.c_str());
        } else {
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::printf("    kesmark_range built in %.0f ms: %zu boxes, %zu strips, %zu crossings\n", ms, map->Layout().boxes.size(),
                        map->Layout().strips.size(), map->Layout().crossings.size());
        }
    }
    return map;
}

const MapSiteDef* Site(const MapData& m, const std::string& id) {
    for (const MapSiteDef& s : m.Def().sites) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------------------------- weather

SC_TEST("weather: without a preset the air is the standard atmosphere, still and clear") {
    const Weather w;
    for (double h : {0.0, 1000.0, 5000.0, 11000.0, 15000.0}) {
        SC_EXPECT_NEAR(w.Air(h).pressure_pa, StandardAtmosphere(h).pressure_pa, 1e-6);
        SC_EXPECT_NEAR(w.Air(h).temperature_k, StandardAtmosphere(h).temperature_k, 1e-9);
    }
    SC_EXPECT(w.WindAt(500.0, 400.0).Length() == 0.0);
    SC_EXPECT(!w.HasClouds());
    SC_EXPECT_NEAR(w.CloudTransmission({0, 0, 0}, 100.0, {0, 20000, 3000}, 3100.0), 1.0, 1e-12);
}

SC_TEST("weather: the presets load, each with a source, and the air war's three weathers are among them") {
    std::vector<WeatherPreset> all;
    std::string e;
    SC_EXPECT(LoadWeatherPresets(kData, all, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(all.size() >= 3);
    for (const char* aw : {"clear", "cloudy", "poor"}) {
        WeatherPreset p;
        SC_EXPECT(FindWeatherPreset(kData, aw, p, &e) && p.air_war == aw);
    }
    std::vector<WeatherPreset> bad;
    SC_EXPECT(!LoadWeatherPresetsJson(R"({"presets":[{"id":"x","name":"x","source":"sam3d","confidence":"estimate",
        "surface":{"temperature_c":5,"dewpoint_c":9,"visibility_km":10}}]})", bad, &e));
    SC_EXPECT(e.find("dewpoint_c") != std::string::npos);
}

SC_TEST("weather: the day's temperature and QNH shift the standard atmosphere (hydrostatic, standard lapse)") {
    // clear: 12 C at 110 m, QNH 1022 hPa. ISA at 110 m is 288.15 - 0.0065*110 = 287.435 K, so the offset is
    // 285.15 - 287.435 = -2.285 K. At sea level p = QNH; at 3000 m p = 102200 (T(3000)/T0)^5.2559 with T0 = 285.865 K.
    const Weather w(Preset("clear"), 110.0, 7);
    SC_EXPECT_NEAR(w.TemperatureOffsetK(), -2.285, 1e-3);
    SC_EXPECT_NEAR(w.Air(110.0).temperature_k, 285.15, 1e-6);
    SC_EXPECT_NEAR(w.Air(0.0).pressure_pa, 102200.0, 1e-6);
    const double t0 = 288.15 - 2.285, t3 = t0 - 0.0065 * 3000.0;
    SC_EXPECT_NEAR(w.Air(3000.0).pressure_pa, 102200.0 * std::pow(t3 / t0, 9.80665 / (0.0065 * 287.05287)), 1e-3);
    // Colder air is denser at the same pressure.
    SC_EXPECT(w.Air(3000.0).density_kg_m3 > StandardAtmosphere(3000.0).density_kg_m3);
}

SC_TEST("weather: a log wind near the ground, veering to the winds aloft above the boundary layer") {
    // clear: 3 m/s at 10 m from 330 over z0 = 0.1 m. At 50 m: 3 ln(500)/ln(100) = 4.048 m/s, still from 330, so
    // blowing toward 150: east 4.048 sin 150 = 2.024, north 4.048 cos 150 = -3.506.
    const Weather w(Preset("clear"), 110.0, 7);
    const Vec3 at10 = w.WindAt(120.0, 10.0);
    SC_EXPECT_NEAR(at10.Length(), 3.0, 1e-9);
    const Vec3 at50 = w.WindAt(160.0, 50.0);
    SC_EXPECT_NEAR(at50.Length(), 4.048, 1e-3);
    SC_EXPECT_NEAR(at50.x, 2.024, 1e-3);
    SC_EXPECT_NEAR(at50.y, -3.506, 1e-3);
    // Aloft: the table (5500 m: 18 m/s from 295), linear between rows.
    const Vec3 hi = w.WindAt(5500.0, 5390.0);
    SC_EXPECT_NEAR(hi.Length(), 18.0, 1e-9);
    SC_EXPECT_NEAR(RadToDeg(AzimuthOf(hi)), 295.0 - 180.0, 1e-6);
    SC_EXPECT_NEAR(w.WindAt(7250.0, 7140.0).Length(), 24.0, 1e-9);
    // The same 10 m wind over rougher ground (z0 = 1 m) shears more above it: 3 ln(50)/ln(10) = 5.097 m/s at 50 m.
    Weather rough(Preset("clear"), 110.0, 7);
    rough.SetRoughnessM(1.0);
    SC_EXPECT_NEAR(rough.WindAt(160.0, 50.0).Length(), 5.097, 1e-3);
}

SC_TEST("weather: each cloud layer covers its share of the sky; cumulus bases at the condensation level") {
    const Weather cloudy(Preset("cloudy"), 110.0, 11);
    SC_EXPECT(cloudy.LayerCount() == 2);
    const double sc = cloudy.MeasuredCover(0, 60000.0, 120);
    std::printf("    stratocumulus 5/8: measured %.3f\n", sc);
    SC_EXPECT_NEAR(sc, 5.0 / 8.0, 0.08);
    // fair_cumulus: 17 C over a 7 C dewpoint: base 125 m x 10 = 1250 m above the 110 m ground.
    const Weather cu(Preset("fair_cumulus"), 110.0, 11);
    SC_EXPECT_NEAR(cu.LayerDef(0).base_m, 1360.0, 1e-9);
    SC_EXPECT_NEAR(cu.MeasuredCover(0, 30000.0, 120), 3.0 / 8.0, 0.08);
    // An overcast covers it all.
    const Weather poor(Preset("poor"), 110.0, 11);
    SC_EXPECT_NEAR(poor.MeasuredCover(1, 30000.0, 40), 1.0, 1e-12);
    // Cumulus tops vary; bases are flat.
    double lo = 1e9, hi = 0.0;
    for (const CloudCell& c : cu.Cells(0, -20000, -20000, 20000, 20000, 400.0)) {
        SC_EXPECT_NEAR(c.base_m, 1360.0, 1e-9);
        lo = std::min(lo, c.top_m);
        hi = std::max(hi, c.top_m);
    }
    SC_EXPECT(hi - lo > 500.0);
}

SC_TEST("weather: the TV cannot see through an overcast; under it, and through gaps, it can") {
    // poor: nimbostratus 500-3100 m, LWC 0.4 g/m3: extinction 144.7 x 0.4^0.88 = 64.6 per km. Straight up through
    // 2.6 km of it: optical depth 168, nothing gets through.
    SC_EXPECT_NEAR(CloudExtinctionPerKm(0.4), 64.6, 0.1);
    const Weather poor(Preset("poor"), 110.0, 3);
    SC_EXPECT(poor.CloudTransmission({0, 0, 3}, 113.0, {100, 100, 5000}, 5113.0) < 1e-6);
    SC_EXPECT(poor.InCloud(0.0, 0.0, 2000.0));
    // Both ends under the scud (below 280 m): clear.
    SC_EXPECT_NEAR(poor.CloudTransmission({0, 0, 3}, 113.0, {0, 8000, 100}, 210.0), 1.0, 1e-12);
    // Broken stratocumulus: some lines through the layer find a gap, others do not.
    const Weather cloudy(Preset("cloudy"), 110.0, 5);
    int clear = 0, blocked = 0;
    for (int i = 0; i < 40; ++i) {
        const double az = kTwoPi * i / 40.0;
        const double t = cloudy.CloudTransmission({0, 0, 3}, 113.0, {12000 * std::sin(az), 12000 * std::cos(az), 2500}, 2600.0);
        clear += t > 0.5 ? 1 : 0;
        blocked += t < 0.01 ? 1 : 0;
    }
    std::printf("    40 looks up through 5/8 stratocumulus: %d clear, %d blocked\n", clear, blocked);
    SC_EXPECT(clear > 0 && blocked > 0);
}

SC_TEST("weather: the clouds drift with the wind at their height") {
    Weather w(Preset("cloudy"), 110.0, 9);
    double b0 = 0.0, t0 = 0.0, b1 = 0.0, t1 = 0.0;
    int checked = 0;
    for (const CloudCell& c : w.Cells(0, -5000, -5000, 5000, 5000, 500.0)) {
        w.SetTime(600.0);
        const Vec3 d = w.LayerDrift(0);
        SC_EXPECT(w.CloudColumnAt(0, c.east_m + d.x, c.north_m + d.y, b1, t1));
        w.SetTime(0.0);
        SC_EXPECT(w.CloudColumnAt(0, c.east_m, c.north_m, b0, t0));
        SC_EXPECT_NEAR(t1, t0, 1e-6);
        ++checked;
    }
    SC_EXPECT(checked > 10);
    // At 1100-1600 m the cloudy wind is about 11-12 m/s from about 290: ten minutes moves the field ~7 km east-ish.
    w.SetTime(600.0);
    const Vec3 d = w.LayerDrift(0);
    SC_EXPECT(d.Length() > 5000.0 && d.Length() < 9000.0 && d.x > 0.0);
}

// ---------------------------------------------------------------------------------------------- the map

SC_TEST("map: the Kesmark range loads and lays out the same every time") {
    const auto map = Kesmark();
    SC_EXPECT(map != nullptr);
    if (!map) return;
    const MapDef& d = map->Def();
    SC_EXPECT_NEAR(d.east_m - d.west_m, 40000.0, 1e-9);
    SC_EXPECT_NEAR(d.north_m - d.south_m, 40000.0, 1e-9);
    SC_EXPECT(map->CoreGrid().cols == 801 && map->FarGrid().cols == 601);
    std::string e;
    const auto again = LoadMap(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(again && again->Layout().boxes.size() == map->Layout().boxes.size());
    if (again && !again->Layout().boxes.empty()) {
        const MapBox& a = map->Layout().boxes.back();
        const MapBox& b = again->Layout().boxes.back();
        SC_EXPECT(a.east_m == b.east_m && a.north_m == b.north_m && a.height_m == b.height_m);
    }
    for (int p = 0; p < static_cast<int>(MapPart::Count); ++p) {
        const std::size_t n = map->Layout().CountOf(static_cast<MapPart>(p));
        if (n > 0) std::printf("    %-14s %zu\n", MapPartName(static_cast<MapPart>(p)), n);
    }
    SC_EXPECT(map->Layout().CountOf(MapPart::Canopy) > 5000);
    SC_EXPECT(map->Layout().CountOf(MapPart::Farm) >= 140);
    SC_EXPECT(map->Layout().CountOf(MapPart::Shelter) == 22);
}

SC_TEST("map: towns and villages hold most of their buildings, inside their radius") {
    const auto map = Kesmark();
    if (!map) return;
    std::vector<MapSettlementDef> all = map->Def().settlements;
    all.insert(all.end(), map->Def().detail_settlements.begin(), map->Def().detail_settlements.end());
    SC_EXPECT(!map->Def().detail_settlements.empty());
    for (const MapSettlementDef& s : all) {
        int n = 0;
        for (const MapBox& b : map->Layout().boxes) {
            if ((b.part == MapPart::House || b.part == MapPart::Flats) &&
                std::hypot(b.east_m - s.centre.east_m, b.north_m - s.centre.north_m) <= s.radius_m + 60.0) {
                ++n;
            }
        }
        std::printf("    %-12s %4d of %4d\n", s.id.c_str(), n, s.buildings);
        SC_EXPECT(n >= s.buildings * 6 / 10);
    }
}

SC_TEST("map: places agree with the theatre and the chain of command's places") {
    const auto map = Kesmark();
    if (!map) return;
    std::string text, perr;
    json::Value places, theatre;
    SC_EXPECT(detail::ReadTextFile(kData + "/command/places_pannonia.json", text) && json::Parse(text, places, &perr));
    SC_EXPECT(detail::ReadTextFile(kData + "/air_war/theatre_pannonia.json", text) && json::Parse(text, theatre, &perr));
    int compared = 0;
    const auto compare = [&](const std::string& id, const MapPoint& at) {
        for (const json::Value* list : {places.Find("places"), theatre.Find("objects")}) {
            if (list == nullptr) continue;
            for (const json::Value& p : list->items) {
                const json::Value* pid = p.Find("id");
                if (pid == nullptr || pid->string != id) continue;
                const double e = p.Find("east_km")->number * 1000.0, n = p.Find("north_km")->number * 1000.0;
                if (std::hypot(e - at.east_m, n - at.north_m) > 10.0) std::printf("    %s is %.0f m off\n", id.c_str(), std::hypot(e - at.east_m, n - at.north_m));
                SC_EXPECT(std::hypot(e - at.east_m, n - at.north_m) <= 10.0);
                ++compared;
            }
        }
    };
    for (const MapSettlementDef& s : map->Def().settlements) compare(s.id, s.centre);
    for (const MapSiteDef& s : map->Def().sites) compare(s.id, s.centre);
    std::printf("    %d positions compared\n", compared);
    SC_EXPECT(compared >= 25);
}

SC_TEST("map: both Kesmark bridges stand where the theatre has them, over the river") {
    const auto map = Kesmark();
    if (!map) return;
    for (const MapPoint at : {MapPoint{12000, -6000}, MapPoint{14000, -4000}}) {
        bool found = false;
        for (const MapCrossing& c : map->Layout().crossings) {
            if (std::hypot(c.at.east_m - at.east_m, c.at.north_m - at.north_m) < 50.0) {
                found = true;
                // The deck above the water, the water below the banks 400 m off.
                SC_EXPECT(c.deck_m > map->WaterHeight(at.east_m, at.north_m) + 3.0);
                const double across = c.heading_rad;  // along the line: off to both banks
                for (double side : {-1.0, 1.0}) {
                    const double e = at.east_m + side * 600.0 * std::sin(across), n = at.north_m + side * 600.0 * std::cos(across);
                    SC_EXPECT(map->WaterHeight(at.east_m, at.north_m) < map->GroundHeight(e, n));
                }
            }
        }
        SC_EXPECT(found);
    }
    SC_EXPECT(map->Layout().CountOf(MapPart::Bridge) >= 2);
}

SC_TEST("map: the ground is a plain with hills, a river valley, levelled sites and woods standing on it") {
    const auto map = Kesmark();
    if (!map) return;
    const MapGrid& g = map->CoreGrid();
    double lo = 1e9, hi = -1e9;
    for (double v : g.values) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    std::printf("    ground in the map %.1f to %.1f m\n", lo, hi);
    SC_EXPECT(lo > 80.0 && hi < 270.0);  // the hill country (terrain.landform, roadmap L11) to 70 m over the plain's hills
    SC_EXPECT(map->GroundHeight(2000, 14500) > 150.0);  // Polany hill
    // The air base's runway is level end to end.
    const MapSiteDef* kecel = Site(*map, "kecel_airbase");
    SC_EXPECT(kecel != nullptr);
    if (kecel) {
        const double h = kecel->heading_rad, half = 0.5 * kecel->runway_length_m;
        const double a = map->GroundHeight(kecel->centre.east_m + half * std::sin(h), kecel->centre.north_m + half * std::cos(h));
        const double b = map->GroundHeight(kecel->centre.east_m - half * std::sin(h), kecel->centre.north_m - half * std::cos(h));
        SC_EXPECT_NEAR(a, b, 0.5);
    }
    // Inside the Lazne woods the canopy stands 14-24 m over the ground; in the battery's position nothing does.
    SC_EXPECT(map->ObstacleHeight(-11000, -3000) >= 14.0);
    SC_EXPECT(map->ObstacleHeight(6400, -9600) < 3.0);
    // Far beyond the map the ground carries on.
    SC_EXPECT(map->GroundHeight(-140000, 100000) > 50.0);
}

SC_TEST("map: the map proper's layout is the same with the detail areas (counts per part, the last box)") {
    const auto map = Kesmark();
    if (!map) return;
    // The core's boxes as laid before the detail areas (roadmap R6, recorded from r-track at acea88a): the detail
    // areas' towns draw on their own random streams, so nothing in the map proper moves. Since R6's review, 9 fewer wood
    // blocks: a site's clearance is found however far a block is from the site's centre (mapgeo::Occupancy), and r-track
    // had 9 blocks inside the 140 m clearance of two field posts and the P-40 post (8842 canopy, 12038 boxes). Since the
    // hill country's land use (terrain roadmap A.3.17, 7 Nov 2026), 3554 more: the woods on its steep sides and crests.
    const std::pair<MapPart, std::size_t> before[] = {
        {MapPart::Bridge, 3},  {MapPart::Canopy, 12387}, {MapPart::Chimney, 1},   {MapPart::Farm, 204},     {MapPart::Flats, 77},
        {MapPart::House, 1836}, {MapPart::Industrial, 6}, {MapPart::Launcher, 3}, {MapPart::Marsh, 951},   {MapPart::Mast, 7},
        {MapPart::Military, 21}, {MapPart::Pad, 6},      {MapPart::Revetment, 54}, {MapPart::Station, 1}, {MapPart::Tank, 8},
        {MapPart::Tent, 17},   {MapPart::Tower, 1}};
    std::size_t counts[static_cast<int>(MapPart::Count)] = {};
    const MapBox* last = nullptr;
    std::size_t total = 0;
    for (const MapBox& b : map->Layout().boxes) {
        if (!map->Def().InCore(b.east_m, b.north_m)) continue;
        if (b.part == MapPart::TreeRow) continue;  // laid after, along the fields (roadmap T4): not part of the layout this pins
        ++counts[static_cast<int>(b.part)];
        ++total;
        last = &b;
    }
    std::size_t expected = 0;
    for (const auto& p : before) {
        if (counts[static_cast<int>(p.first)] != p.second) std::printf("    %s: %zu, was %zu\n", MapPartName(p.first), counts[static_cast<int>(p.first)], p.second);
        SC_EXPECT(counts[static_cast<int>(p.first)] == p.second);
        expected += p.second;
    }
    SC_EXPECT(total == expected && total == 15583);
    SC_EXPECT(last != nullptr && last->part == MapPart::Marsh && last->east_m == 16250.0 && last->north_m == -8450.0);
}

SC_TEST("map: the detail areas are levels of the ground: their edges the coarse ground's, inside it within the micro relief") {
    const auto map = Kesmark();
    if (!map) return;
    const MapDef& d = map->Def();
    SC_EXPECT(d.detail.size() == 2);
    SC_EXPECT(map->DetailGrids().size() == d.detail.size() && map->DetailObstacleGrids().size() == d.detail.size());
    for (std::size_t i = 0; i < d.detail.size(); ++i) {
        const MapDetailDef& a = d.detail[i];
        const MapGrid& coarse = d.InCore(a.centre.east_m, a.centre.north_m) ? map->CoreGrid() : map->FarGrid();
        SC_EXPECT(map->LevelAt(a.centre.east_m, a.centre.north_m) == static_cast<int>(map->DetailGrids()[i].spacing_m < 15.0 ? 0 : 1));
        // Every 100 m along the edge: the coarse level's triangles (its cells beyond the edge meet these nodes).
        double edge = 0.0;
        for (double t = -a.half_m; t <= a.half_m; t += 100.0) {
            const double pts[4][2] = {{a.centre.east_m + t, a.centre.north_m - a.half_m}, {a.centre.east_m + t, a.centre.north_m + a.half_m},
                                      {a.centre.east_m - a.half_m, a.centre.north_m + t}, {a.centre.east_m + a.half_m, a.centre.north_m + t}};
            for (const auto& p : pts) edge = std::max(edge, std::fabs(map->GroundHeight(p[0], p[1]) - coarse.SampleTri(p[0], p[1])));
        }
        // Inside: never further from the coarse ground than the micro relief, and some relief where nothing is near.
        double inside = 0.0;
        for (double n = -a.half_m + 37.0; n < a.half_m; n += 211.0) {
            for (double e = -a.half_m + 53.0; e < a.half_m; e += 197.0) {
                const double x = a.centre.east_m + e, y = a.centre.north_m + n;
                inside = std::max(inside, std::fabs(map->GroundHeight(x, y) - coarse.SampleTri(x, y)));
            }
        }
        std::printf("    %s: %d x %d at %.0f m; edge off the coarse ground by %.4f m, inside by up to %.2f m (micro %.1f m, meso %.1f m)\n", a.id.c_str(),
                    map->DetailGrids()[i].cols, map->DetailGrids()[i].rows, a.spacing_m, edge, inside, a.micro_m, a.MesoTotalM());
        SC_EXPECT(edge < 0.02);
        SC_EXPECT(inside <= a.micro_m + a.MesoTotalM() + 1e-9);
        SC_EXPECT(inside > 0.1 * (a.micro_m + a.MesoTotalM()));
    }
    // A loader refuses an area whose edges do not fall on the coarse ground's nodes.
    std::string text, e;
    SC_EXPECT(detail::ReadTextFile(kData + "/maps/kesmark_range.json", text));
    const std::string from = "\"east_km\": 9.5, \"north_km\": -6.5";
    SC_EXPECT(text.find(from) != std::string::npos);
    text.replace(text.find(from), from.size(), "\"east_km\": 9.51, \"north_km\": -6.5");
    MapDef bad;
    SC_EXPECT(!LoadMapDefJson(text, bad, &e));
    SC_EXPECT(e.find("nodes") != std::string::npos);
}

SC_TEST("map: the capital is laid out on the country's roads and river, its sites clear, its blocks standing in the way") {
    const auto map = Kesmark();
    if (!map) return;
    const MapDef& d = map->Def();
    const MapData::DetailLines& lines = map->ImportedLines();
    SC_EXPECT(lines.rivers.size() >= 1 && lines.roads.size() >= 4 && lines.railways.size() >= 1);
    // No building of the capital on its roads (the rule: its middle half its length and 4 m clear of a road's edge) or
    // within 30 m of the Rava's edge.
    const MapSettlementDef& cap = d.detail_settlements.front();
    SC_EXPECT(cap.id == "capital");
    int buildings = 0, on_road = 0, wet = 0;
    for (const MapBox& b : map->Layout().boxes) {
        if (b.part != MapPart::House && b.part != MapPart::Flats) continue;
        if (std::hypot(b.east_m - cap.centre.east_m, b.north_m - cap.centre.north_m) > cap.radius_m + 60.0) continue;
        ++buildings;
        for (const MapLineDef& r : lines.roads) on_road += mapgeo::DistToLine(r.points, b.east_m, b.north_m) < 0.5 * r.width_m + 4.0 ? 1 : 0;
        for (const MapLineDef& r : lines.railways) on_road += mapgeo::DistToLine(r.points, b.east_m, b.north_m) < 0.5 * r.width_m + 4.0 ? 1 : 0;
        for (const MapLineDef& r : lines.rivers) wet += mapgeo::DistToLine(r.points, b.east_m, b.north_m) < 0.5 * r.width_m + 30.0 ? 1 : 0;
    }
    std::printf("    the capital: %d buildings, %d on a road, %d in the river; %zu roads, %zu railways, %zu rivers imported\n", buildings, on_road, wet,
                lines.roads.size(), lines.railways.size(), lines.rivers.size());
    SC_EXPECT(on_road == 0 && wet == 0);
    // The Rava's bridges at the capital, and the strips of its lines on the detail ground (every midpoint between two
    // strip points within 0.3 m of the ground plus the lift, but on a bridge's ramps).
    int bridges = 0;
    for (const MapCrossing& c : map->Layout().crossings) bridges += d.InDetail(c.at.east_m, c.at.north_m) && !d.InCore(c.at.east_m, c.at.north_m) ? 1 : 0;
    SC_EXPECT(bridges >= 2);
    double worst = 0.0, worst_e = 0.0, worst_n = 0.0, worst_step = 0.0;
    for (const MapStrip& s : map->Layout().strips) {
        if (s.surface == MapSurface::Water) continue;
        const double lift = s.surface == MapSurface::Rail ? 0.45 : 0.25;
        for (std::size_t i = 1; i < s.points.size(); ++i) {
            const double e = 0.5 * (s.points[i - 1].x + s.points[i].x), n = 0.5 * (s.points[i - 1].y + s.points[i].y);
            if (!d.InDetail(e, n)) continue;
            bool ramp = false;
            for (const MapCrossing& c : map->Layout().crossings) ramp = ramp || std::hypot(e - c.at.east_m, n - c.at.north_m) < c.half_length_m + 150.0;
            if (ramp) continue;
            const double off = std::fabs(0.5 * (s.points[i - 1].z + s.points[i].z) - map->GroundHeight(e, n) - lift);
            if (off > worst) {
                worst = off;
                worst_e = e;
                worst_n = n;
                worst_step = std::hypot(s.points[i].x - s.points[i - 1].x, s.points[i].y - s.points[i - 1].y);
            }
        }
    }
    std::printf("    strips in the detail areas: at most %.3f m off the ground between their points (at %.0f, %.0f, points %.0f m apart)\n", worst,
                worst_e, worst_n, worst_step);
    SC_EXPECT(worst < 0.3);
    // The blocks of flats round its middle stand in the way; a site of the dynamic campaign test is clear.
    double tallest = 0.0;
    for (double n = -300.0; n <= 300.0; n += 25.0) {
        for (double e = -300.0; e <= 300.0; e += 25.0) tallest = std::max(tallest, map->ObstacleHeight(cap.centre.east_m + e, cap.centre.north_m + n));
    }
    SC_EXPECT(tallest > 10.0);
    const MapSiteDef* s60 = Site(*map, "capital_s60_1");
    SC_EXPECT(s60 != nullptr);
    if (s60 != nullptr) {
        std::printf("    the capital's middle: %.1f m tall at most; S-60 battery 1's position: %.1f m\n", tallest,
                    map->ObstacleHeight(s60->centre.east_m, s60->centre.north_m));
        SC_EXPECT(map->ObstacleHeight(s60->centre.east_m, s60->centre.north_m) < 1.0);
    }
}

SC_TEST("map: a scenario anywhere sees the same ground, and the ground falls away with the Earth's curvature") {
    const auto map = Kesmark();
    if (!map) return;
    // A scenario with its origin on battery 21's command post (6.4 km east, 9.6 km south of the map's origin).
    const LocalFrame mf(map->Def().origin);
    GeoPoint at = mf.EnuToGeodetic({6400.0, -9600.0, 0.0});
    at.alt_m = 110.0;
    const MapTerrain t(map, at);
    SC_EXPECT_NEAR(t.HeightAt(0.0, 0.0), map->GroundHeight(6400.0, -9600.0), 0.05);
    SC_EXPECT_NEAR(t.HeightAt(5600.0, 3600.0), map->GroundHeight(12000.0, -6000.0), 0.3);  // the road bridge
    // 12.6 km west-north-west of the battery: about (-5600, -5600) on the map, turned by the meridians' convergence
    // (6.4 km of longitude at 46.9 N: 0.00147 rad x sin 46.9 = 0.0011 rad, 13 m at this distance). Against the geodesy:
    const MapPoint p = t.Link().FrameToMap(-12000.0, 4000.0);
    GeoPoint g = LocalFrame(at).EnuToGeodetic({-12000.0, 4000.0, 0.0});
    g.alt_m = 110.0;
    const Vec3 exact = mf.GeodeticToEnu(g);
    SC_EXPECT_NEAR(p.east_m, exact.x, 1.0);
    SC_EXPECT_NEAR(p.north_m, exact.y, 1.0);
    SC_EXPECT(std::hypot(p.east_m + 5600.0, p.north_m + 5600.0) < 20.0);
    // In the map's own frame, sea-level ground 150 km east lies 150000^2 / 2R = 1.77 km below the horizontal plane
    // (R about 6.37e6 m); the sim's own geodesy agrees to a metre.
    const MapFrameLink same(map->Def().origin, map->Def().origin);
    const Vec3 far = same.MapToFrame(150000.0, 0.0, 110.0);
    SC_EXPECT_NEAR(far.z, -150000.0 * 150000.0 / (2.0 * 6.37e6), 15.0);
    SC_EXPECT_NEAR(mf.AltitudeOf(far), 110.0, 1.0);
    Vec3 ex, nx, up;
    same.AxesAt(150000.0, 0.0, ex, nx, up);
    SC_EXPECT_NEAR(RadToDeg(std::acos(up.z)), RadToDeg(150000.0 / 6.37e6), 0.02);
}

// ---------------------------------------------------------------------------------------------- in the sim

SC_TEST("wind: an aircraft drifts with the air, and crabs into it to hold its track to the next waypoint") {
    // No waypoint: it flies its heading through the air, so over the ground it moves at air + wind (velocity is over
    // the ground: nose north at 200 m/s in a 20 m/s westerly).
    Aircraft a;
    a.position = {0, 0, 1000};
    a.velocity = {20, 200, 0};
    a.speed_mps = 200.0;
    const Vec3 wind{20, 0, 0};
    StepAircraft(a, 1.0, wind);
    SC_EXPECT_NEAR(a.velocity.x, 20.0, 1e-9);
    SC_EXPECT_NEAR(a.velocity.y, 200.0, 1e-9);
    // With a waypoint due north 20 km away and a 20 m/s wind from the west, the nose turns asin(20/200) = 5.74 deg into
    // it and the track runs straight up the north line: ground speed sqrt(200^2 - 20^2) = 199.0 m/s.
    Aircraft b;
    b.position = {0, 0, 1000};
    b.velocity = {0, 200, 0};
    b.speed_mps = 200.0;
    b.max_turn_rate_rad_s = DegToRad(10.0);
    b.waypoints = {{0, 20000, 1000}};
    for (int i = 0; i < 200; ++i) StepAircraft(b, 0.05, wind);
    SC_EXPECT_NEAR(b.velocity.x, 0.0, 0.05);
    SC_EXPECT_NEAR(b.velocity.y, 199.0, 0.05);
    SC_EXPECT_NEAR(RadToDeg(std::atan2(-(b.velocity - wind).x, (b.velocity - wind).y)), 5.74, 0.05);
    SC_EXPECT(std::fabs(b.position.x) < 25.0);
}

SC_TEST("TV: a cloud between the sight and the target hides it, whatever the haze") {
    em::OpticalSensorDef d;
    const em::OpticalConditions c;
    const em::OpticalLook clear = em::Look(d, 0.02, c, 5000.0, 10.0, DegToRad(1.0), true, 1.0);
    SC_EXPECT(clear.seen);
    const em::OpticalLook hidden = em::Look(d, 0.02, c, 5000.0, 10.0, DegToRad(1.0), true, 1e-6);
    SC_EXPECT(!hidden.seen && std::string(hidden.why_not) == "in cloud");
}

SC_TEST("scenario on the map: the test range plays on the map's ground, in its weather") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/kesmark_range_test.json", kData, def, console, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(def.map != nullptr && def.has_weather && def.weather.id == "cloudy");
    SC_EXPECT_NEAR(def.optics.visibility_m, 20000.0, 1e-9);  // the preset's, no "environment"
    ScenarioRunner run(std::move(def), std::move(console));
    SC_EXPECT(run.MapGround() != nullptr && run.GetWeather().HasClouds());
    // The origin is vehicle 21-1's position: the map's ground there.
    SC_EXPECT_NEAR(run.GetWorld().GetTerrain().HeightAt(0.0, 0.0), Kesmark()->GroundHeight(6000.0, -9200.0), 0.05);
    // The strike pair holds 120 m over the map's ground on the way in, a wind on its beam.
    for (int i = 0; i < 20 * 330; ++i) run.Step(0.05);
    int seen = 0;
    for (const Aircraft& a : run.GetWorld().AircraftList()) {
        if (a.name.find("Strike") == std::string::npos || !a.alive) continue;
        const double agl = run.GetWorld().Frame().AltitudeOf(a.position) - run.GetWorld().GetTerrain().HeightAt(a.position.x, a.position.y);
        std::printf("    strike aircraft at %.1f km, %.0f m over the ground\n", a.position.HorizontalLength() / 1000.0, agl);
        SC_EXPECT(agl > 60.0 && agl < 200.0);
        ++seen;
    }
    SC_EXPECT(seen >= 1);
    // Flat scenarios are unchanged: still air, no map.
    ScenarioDef flat;
    ConsoleData fc;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, flat, fc, &e) && !flat.map && !flat.has_weather);
    std::vector<WeatherPreset> unused;
    SC_EXPECT(!LoadScenario(R"({"scenario_id":"x","name":"x","source":"sam3d","confidence":"estimate","origin":{"lat_deg":46.9,"lon_deg":19.7,"alt_m":110},
        "terrain":{"type":"map","map":"maps/kesmark_range.json","height_m":110}})", kData, flat, fc, &e));
    SC_EXPECT(e.find("own ground") != std::string::npos);
}

SC_TEST("sun: noon on 15 October over the Kesmark district stands 34 deg up in the south") {
    // Day 288: declination about -8.9 deg, equation of time about +14 min. Noon at 19.7 E comes 78.8 + 14 min before
    // 12:00 UTC (10:27 UTC); its height is 90 - 46.9 - 8.9 = 34.2 deg, due south.
    const SunPosition noon = SunAt(46.9, 19.7, 288, 10.0 + 27.0 / 60.0);
    SC_EXPECT_NEAR(RadToDeg(noon.declination_rad), -8.9, 0.3);
    SC_EXPECT_NEAR(noon.equation_of_time_min, 14.2, 0.5);
    SC_EXPECT_NEAR(RadToDeg(noon.elevation_rad), 34.2, 0.3);
    SC_EXPECT_NEAR(RadToDeg(noon.azimuth_rad), 180.0, 1.0);
    // Morning in the east-south-east, night below the horizon.
    const SunPosition morning = SunAt(46.9, 19.7, 288, 6.0);
    SC_EXPECT(morning.elevation_rad > 0.0 && RadToDeg(morning.azimuth_rad) > 90.0 && RadToDeg(morning.azimuth_rad) < 135.0);
    SC_EXPECT(SunAt(46.9, 19.7, 288, 22.0).elevation_rad < DegToRad(-20.0));
}
