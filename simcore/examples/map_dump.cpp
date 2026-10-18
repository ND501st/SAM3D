// map_dump: build a generated map (data/maps/<id>.json, simcore/terrain_map.hpp) and write what it built as JSON for
// tools/map_view.py, or print the ground's height at given points (to place a scenario's units on it).
//
//   map_dump <data dir> maps/kesmark_range.json out/kesmark_range_map.json
//   map_dump <data dir> maps/kesmark_range.json --height 6.0 -9.2 [east_km north_km ...]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "simcore/terrain_map.hpp"

using namespace simcore;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: map_dump <data dir> <map file> <out.json> | --height e_km n_km ...\n");
        return 2;
    }
    const auto t0 = std::chrono::steady_clock::now();
    std::string error;
    const std::shared_ptr<const MapData> map = LoadMap(argv[1], argv[2], &error);
    if (!map) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "%s built in %.0f ms: %zu boxes, %zu strips, %zu crossings\n", map->Def().map_id.c_str(), ms,
                 map->Layout().boxes.size(), map->Layout().strips.size(), map->Layout().crossings.size());
    if (std::strcmp(argv[3], "--height") == 0) {
        for (int i = 4; i + 1 < argc; i += 2) {
            const double e = std::atof(argv[i]) * 1000.0, n = std::atof(argv[i + 1]) * 1000.0;
            std::printf("%.3f %.3f ground %.2f m obstacle %.1f m\n", e / 1000.0, n / 1000.0, map->GroundHeight(e, n), map->ObstacleHeight(e, n));
        }
        return 0;
    }
    std::ofstream out(argv[3]);
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", argv[3]);
        return 1;
    }
    const MapDef& d = map->Def();
    out << "{\"map_id\":\"" << d.map_id << "\",\"name\":\"" << d.name << "\",\"extent_m\":[" << d.west_m << "," << d.south_m << ","
        << d.east_m << "," << d.north_m << "],\"far_m\":" << d.far_m << ",\n";
    // The ground every 500 m inside the map, every 5 km beyond it.
    const auto grid = [&](const char* key, double west, double south, double east, double north, double step) {
        out << "\"" << key << "\":{\"west\":" << west << ",\"south\":" << south << ",\"step\":" << step << ",\"rows\":[";
        bool first_row = true;
        for (double n = south; n <= north + 1e-6; n += step) {
            out << (first_row ? "" : ",") << "[";
            first_row = false;
            bool first = true;
            for (double e = west; e <= east + 1e-6; e += step) {
                char buf[32];
                std::snprintf(buf, sizeof buf, "%.1f", map->GroundHeight(e, n));
                out << (first ? "" : ",") << buf;
                first = false;
            }
            out << "]";
        }
        out << "]},\n";
    };
    grid("ground", d.west_m, d.south_m, d.east_m, d.north_m, 500.0);
    grid("far_ground", -d.far_m, -d.far_m, d.far_m, d.far_m, 5000.0);
    out << "\"boxes\":[";
    bool first = true;
    for (const MapBox& b : map->Layout().boxes) {
        char buf[200];
        std::snprintf(buf, sizeof buf, "%s[\"%s\",%.1f,%.1f,%.1f,%.1f,%.1f,%.3f]", first ? "" : ",", MapPartName(b.part), b.east_m, b.north_m,
                      b.length_m, b.width_m, b.height_m, b.heading_rad);
        out << buf;
        first = false;
    }
    out << "],\n\"strips\":[";
    first = true;
    for (const MapStrip& s : map->Layout().strips) {
        out << (first ? "" : ",") << "{\"id\":\"" << s.id << "\",\"surface\":\"" << MapSurfaceName(s.surface) << "\",\"width_m\":" << s.width_m
            << ",\"points\":[";
        first = false;
        for (std::size_t i = 0; i < s.points.size(); i += 4) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%s[%.0f,%.0f]", i == 0 ? "" : ",", s.points[i].x, s.points[i].y);
            out << buf;
        }
        out << "]}";
    }
    out << "],\n\"labels\":[";
    first = true;
    for (const MapLabel& l : map->Layout().labels) {
        out << (first ? "" : ",") << "{\"id\":\"" << l.id << "\",\"name\":\"" << l.name << "\",\"kind\":\"" << l.kind << "\",\"at\":["
            << l.at.east_m << "," << l.at.north_m << "]}";
        first = false;
    }
    out << "]}\n";
    std::fprintf(stderr, "wrote %s\n", argv[3]);
    return 0;
}
