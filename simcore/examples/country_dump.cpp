// country_dump: build the country's map (data/maps/pannonia.json, simcore/country_map.hpp) and write it as JSON for
// tools/map_view.py, or ask it the way between two points with bridges cut, or the ground's height.
//
//   country_dump <data dir> maps/pannonia.json out/pannonia_map.json
//   country_dump <data dir> maps/pannonia.json --route road 9 -1 68 12 [b_ralje@rava ...]
//   country_dump <data dir> maps/pannonia.json --height 36 58 [east_km north_km ...]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <string>

#include "simcore/country_map.hpp"

using namespace simcore;

namespace {

void Points(std::ofstream& out, const std::vector<MapPoint>& pts) {
    out << "[";
    for (std::size_t i = 0; i < pts.size(); ++i) {
        char buf[48];
        std::snprintf(buf, sizeof buf, "%s[%.3f,%.3f]", i ? "," : "", pts[i].east_m / 1000.0, pts[i].north_m / 1000.0);
        out << buf;
    }
    out << "]";
}

std::string Quote(const std::string& s) {
    std::string q = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') q += '\\';
        q += c;
    }
    return q + "\"";
}

void Lines(std::ofstream& out, const char* key, const std::vector<MapLineDef>& lines) {
    out << "\"" << key << "\":[";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out << (i ? ",\n" : "\n") << "{\"id\":" << Quote(lines[i].id) << ",\"name\":" << Quote(lines[i].name) << ",\"class\":"
            << Quote(lines[i].line_class) << ",\"points\":";
        Points(out, lines[i].points);
        out << "}";
    }
    out << "],\n";
}

bool ParseNet(const char* s, CountryNet& out) {
    for (int i = 0; i < static_cast<int>(CountryNet::Count); ++i) {
        if (std::strcmp(s, CountryNetName(static_cast<CountryNet>(i))) == 0) {
            out = static_cast<CountryNet>(i);
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: country_dump <data dir> <country map> <out.json> | --route road|rail|oil e1 n1 e2 n2 [cut ...] | --height e n ...\n");
        return 2;
    }
    const auto t0 = std::chrono::steady_clock::now();
    std::string error;
    const std::shared_ptr<const CountryMap> map = LoadCountryMap(argv[1], argv[2], &error);
    if (!map) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "%s built in %.0f ms: %dx%d grid, %zu crossings\n", map->Def().map_id.c_str(), ms, map->Grid().cols, map->Grid().rows,
                 map->Crossings().size());
    if (std::strcmp(argv[3], "--height") == 0) {
        for (int i = 4; i + 1 < argc; i += 2) {
            const double e = std::atof(argv[i]) * 1000.0, n = std::atof(argv[i + 1]) * 1000.0;
            const MapPolygonDef* region = map->RegionAt(e, n);
            std::printf("%.3f %.3f ground %.1f m, %s, %s\n", e / 1000.0, n / 1000.0, map->GroundHeight(e, n),
                        map->Inside(e, n) ? "inside" : "outside", region != nullptr ? region->name.c_str() : "no region");
        }
        return 0;
    }
    if (std::strcmp(argv[3], "--route") == 0) {
        CountryNet net = CountryNet::Road;
        if (argc < 9 || !ParseNet(argv[4], net)) {
            std::fprintf(stderr, "--route road|rail|oil e1 n1 e2 n2 [cut crossing ids ...]\n");
            return 2;
        }
        const MapPoint from{std::atof(argv[5]) * 1000.0, std::atof(argv[6]) * 1000.0};
        const MapPoint to{std::atof(argv[7]) * 1000.0, std::atof(argv[8]) * 1000.0};
        std::set<std::string> cut;
        for (int i = 9; i < argc; ++i) cut.insert(argv[i]);
        const CountryRoute r = map->Network(net).Route(from, to, cut);
        if (!r.found) {
            std::printf("no %s route\n", CountryNetName(net));
            return 0;
        }
        std::printf("%s route: %.1f km over %zu points\n  lines:", CountryNetName(net), r.length_m / 1000.0, r.path.size());
        for (const std::string& l : r.lines) std::printf(" %s", l.c_str());
        std::printf("\n  crossings:");
        for (const std::string& c : r.crossings) std::printf(" %s", c.c_str());
        std::printf("\n");
        return 0;
    }
    std::ofstream out(argv[3]);
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", argv[3]);
        return 1;
    }
    const CountryDef& d = map->Def();
    const MapGrid& g = map->Grid();
    out << "{\"kind\":\"country\",\"map_id\":" << Quote(d.map_id) << ",\"name\":" << Quote(d.name) << ",\"extent_km\":[" << g.west_m / 1000.0
        << "," << g.south_m / 1000.0 << "," << (g.west_m + g.spacing_m * (g.cols - 1)) / 1000.0 << ","
        << (g.south_m + g.spacing_m * (g.rows - 1)) / 1000.0 << "],\n";
    // The ground every 2 km.
    out << "\"ground\":{\"west\":" << g.west_m / 1000.0 << ",\"south\":" << g.south_m / 1000.0 << ",\"step\":2,\"rows\":[";
    for (int r = 0; r < g.rows; r += 2) {
        out << (r ? ",\n" : "\n") << "[";
        for (int c = 0; c < g.cols; c += 2) {
            char buf[24];
            std::snprintf(buf, sizeof buf, "%s%.0f", c ? "," : "", g.At(c, r));
            out << buf;
        }
        out << "]";
    }
    out << "]},\n\"outline\":";
    Points(out, d.outline);
    out << ",\n\"borders\":[";
    for (std::size_t i = 0; i < d.borders.size(); ++i) {
        out << (i ? ",\n" : "\n") << "{\"id\":" << Quote(d.borders[i].id) << ",\"neighbour\":" << Quote(d.borders[i].neighbour) << ",\"points\":";
        Points(out, d.borders[i].points);
        out << "}";
    }
    out << "],\n\"regions\":[";
    for (std::size_t i = 0; i < d.regions.size(); ++i) {
        out << (i ? ",\n" : "\n") << "{\"id\":" << Quote(d.regions[i].id) << ",\"name\":" << Quote(d.regions[i].name) << ",\"points\":";
        Points(out, d.regions[i].points);
        out << "}";
    }
    out << "],\n\"forests\":[";
    for (std::size_t i = 0; i < d.ground.forests.size(); ++i) {
        out << (i ? ",\n" : "\n") << "{\"id\":" << Quote(d.ground.forests[i].id) << ",\"points\":";
        Points(out, d.ground.forests[i].points);
        out << "}";
    }
    out << "],\n";
    Lines(out, "rivers", d.ground.rivers);
    Lines(out, "roads", d.ground.roads);
    Lines(out, "railways", d.ground.railways);
    Lines(out, "pipelines", d.pipelines);
    out << "\"settlements\":[";
    for (std::size_t i = 0; i < d.settlements.size(); ++i) {
        const CountrySettlementDef& s = d.settlements[i];
        char buf[96];
        std::snprintf(buf, sizeof buf, ",\"at\":[%.3f,%.3f],\"radius_km\":%.2f,\"population\":%.0f}", s.centre.east_m / 1000.0,
                      s.centre.north_m / 1000.0, s.radius_m / 1000.0, s.population);
        out << (i ? ",\n" : "\n") << "{\"id\":" << Quote(s.id) << ",\"name\":" << Quote(s.name) << ",\"kind\":" << Quote(s.kind) << buf;
    }
    out << "],\n\"landmarks\":[";
    for (std::size_t i = 0; i < d.landmarks.size(); ++i) {
        const CountryLandmarkDef& l = d.landmarks[i];
        char buf[48];
        std::snprintf(buf, sizeof buf, ",\"at\":[%.3f,%.3f]}", l.at.east_m / 1000.0, l.at.north_m / 1000.0);
        out << (i ? ",\n" : "\n") << "{\"id\":" << Quote(l.id) << ",\"name\":" << Quote(l.name) << ",\"kind\":" << Quote(l.kind) << buf;
    }
    out << "],\n\"crossings\":[";
    for (std::size_t i = 0; i < map->Crossings().size(); ++i) {
        const CountryCrossing& c = map->Crossings()[i];
        char buf[48];
        std::snprintf(buf, sizeof buf, ",\"at\":[%.3f,%.3f]}", c.at.east_m / 1000.0, c.at.north_m / 1000.0);
        out << (i ? ",\n" : "\n") << "{\"id\":" << Quote(c.id) << ",\"net\":" << Quote(CountryNetName(c.net)) << ",\"river\":" << Quote(c.river_id) << buf;
    }
    out << "]}\n";
    std::fprintf(stderr, "wrote %s\n", argv[3]);
    return 0;
}
