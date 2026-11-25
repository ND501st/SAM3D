// map_dump: build a generated map (data/maps/<id>.json, simcore/terrain_map.hpp) and write what it built as JSON for
// tools/map_view.py, or print the ground's height at given points (to place a scenario's units on it).
//
//   map_dump <data dir> maps/kesmark_range.json out/kesmark_range_map.json
//   map_dump <data dir> maps/kesmark_range.json --height 6.0 -9.2 [east_km north_km ...]
//   map_dump <data dir> maps/kesmark_range.json --horizon 6.4 -9.6 5   (east_km north_km antenna_m: the lowest target seen)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "simcore/radar.hpp"
#include "simcore/scenery/map_dressing.hpp"
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
    {
        // The map's dressing (roadmap R6, scenery/map_dressing.hpp): its trees, wood masses and land cover, and their time.
        const auto t1 = std::chrono::steady_clock::now();
        scenery::DressingRules rules;
        std::string rules_error;
        if (!scenery::LoadDressingRules(std::string(argv[1]) + "/scenery/map_dressing.json", rules, &rules_error)) {
            std::fprintf(stderr, "%s (the defaults instead)\n", rules_error.c_str());
        }
        const scenery::MapDressing dressing = scenery::PlaceTrees(*map, rules);
        const scenery::CoverRaster cover = scenery::BakeCover(*map, rules);
        std::size_t cells = 0;
        for (const scenery::CoverGrid& g : cover.grids) cells += g.cover.size();
        const double dress_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
        std::fprintf(stderr, "dressing in %.0f ms: %zu trees, %zu wood masses, %zu land cover cells\n", dress_ms, dressing.trees.size(),
                     dressing.masses.size(), cells);
    }
    if (std::strcmp(argv[3], "--horizon") == 0 && argc >= 7) {
        // The lowest target an antenna there sees, every degree round and at 5, 10, 15 and 20 km, by the sight rule the
        // battery fire model and the player's Osa use (engagement.hpp SearchAntennaSees: the ground, woods, buildings,
        // tree rows; 4/3 earth). Heights above the ground under the target.
        const double e0 = std::atof(argv[4]) * 1000.0, n0 = std::atof(argv[5]) * 1000.0, antenna_m = std::atof(argv[6]);
        const MapTerrain terrain(map, map->Def().origin);
        const double eye = map->GroundHeight(e0, n0) + antenna_m;
        const double ranges_m[4] = {5000.0, 10000.0, 15000.0, 20000.0};
        std::printf("horizon from %.3f %.3f km, antenna %.1f m (ground %.1f m)\naz", e0 / 1000.0, n0 / 1000.0, antenna_m, eye - antenna_m);
        for (double r : ranges_m) std::printf("  min_%.0fkm", r / 1000.0);
        std::printf("\n");
        for (int az = 0; az < 360; ++az) {
            std::printf("%3d", az);
            const double a = az * kPi / 180.0;
            for (double r : ranges_m) {
                const double e = e0 + r * std::sin(a), n = n0 + r * std::cos(a);
                const double ground = map->GroundHeight(e, n);
                const auto sees = [&](double h) {
                    return HasLineOfSight(terrain, Vec3{e0, n0, eye}, eye, Vec3{e, n, ground + h}, ground + h, 100.0);
                };
                double lo = 0.0, hi = 3000.0;
                if (sees(lo)) {
                    hi = 0.0;
                } else if (!sees(hi)) {
                    lo = hi;
                } else {
                    for (int i = 0; i < 14; ++i) (sees(0.5 * (lo + hi)) ? hi : lo) = 0.5 * (lo + hi);
                }
                std::printf("  %9.0f", hi);
            }
            std::printf("\n");
        }
        return 0;
    }
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
