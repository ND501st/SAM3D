// nav_check: the route service against the map's data (roadmap G1.6, docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.1).
// Builds the road graph from the country's roads and the district's tracks, then lists every anchor (the chain's places,
// the theatre's objects and prepared positions, the country's towns and landmarks) that stands farther than 3 km from a
// road, or by a piece of the road net cut off from the rest; and times the build and a sample of routes (nav_bench's
// first form). A changed map needs no edit to the navigation: run this and read the list.
//
//   nav_check <data dir> [routes] [--json out.json]   (routes: how many to time, default 200)
//
// --json writes battery 21's routes to every place of the chain, each point and stretch, for
// `python tools/map_view.py out/pannonia_map.json --routes out.json` to draw over the country.
//
// Exit 0 with the lists printed; 1 if the data does not load, or a route between two places of the chain is not found.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <cstdlib>
#include <string>
#include <vector>

#include "simcore/nav.hpp"

using namespace simcore;
using namespace simcore::nav;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: nav_check <data dir> [routes] [--json out.json]\n");
        return 1;
    }
    const std::string data = argv[1];
    int sample = 200;
    std::string json_out;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--json" && i + 1 < argc) {
            json_out = argv[++i];
        } else {
            sample = std::atoi(argv[i]);
        }
    }
    std::string text, e;
    CountryDef country;
    MapDef district;
    MobilityLibrary mobility;
    command::ChainData chain;
    airwar::TheatreDef theatre;
    if (!simcore::detail::ReadTextFile(data + "/maps/pannonia.json", text) || !LoadCountryDefJson(text, country, &e)) {
        std::printf("country: %s\n", e.c_str());
        return 1;
    }
    if (!simcore::detail::ReadTextFile(data + "/maps/kesmark_range.json", text) || !LoadMapDefJson(text, district, &e)) {
        std::printf("district: %s\n", e.c_str());
        return 1;
    }
    if (!LoadMobility(data, "vehicles/mobility.json", mobility, &e) || !command::LoadChainData(data, "command/chain_pannonia.json", chain, &e) ||
        !airwar::LoadTheatreFile(data + "/air_war/theatre_pannonia_nation.json", theatre, &e)) {
        std::printf("%s\n", e.c_str());
        return 1;
    }
    using Clock = std::chrono::steady_clock;
    const Clock::time_point t0 = Clock::now();
    const RoadGraph g = RoadGraph::ForCountry(country, &district);
    const double build_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    Anchors anchors;
    anchors.AddChain(chain);
    anchors.AddTheatre(theatre);
    anchors.AddCountry(country);
    std::printf("road graph: %d nodes, %d edges, %d crossings, built in %.1f ms\n", static_cast<int>(g.Nodes().size()),
                static_cast<int>(g.Edges().size()), static_cast<int>(g.Crossings().size()), build_ms);

    // The road net's pieces: the largest is the country's; an anchor nearest a road of another piece is cut off from it.
    std::vector<int> piece(g.Nodes().size(), -1);
    std::vector<int> sizes;
    for (std::size_t s = 0; s < g.Nodes().size(); ++s) {
        if (piece[s] >= 0) continue;
        const int id = static_cast<int>(sizes.size());
        sizes.push_back(0);
        std::vector<std::size_t> stack{s};
        piece[s] = id;
        while (!stack.empty()) {
            const std::size_t u = stack.back();
            stack.pop_back();
            ++sizes.back();
            for (int ei : g.Nodes()[u].edges) {
                const NavEdge& ed = g.Edges()[static_cast<std::size_t>(ei)];
                const std::size_t v = static_cast<std::size_t>(ed.a == static_cast<int>(u) ? ed.b : ed.a);
                if (piece[v] < 0) {
                    piece[v] = id;
                    stack.push_back(v);
                }
            }
        }
    }
    int main_piece = 0;
    for (std::size_t i = 1; i < sizes.size(); ++i) {
        if (sizes[i] > sizes[static_cast<std::size_t>(main_piece)]) main_piece = static_cast<int>(i);
    }
    std::printf("road net pieces: %d (the largest %d nodes)\n", static_cast<int>(sizes.size()), sizes[static_cast<std::size_t>(main_piece)]);

    const Mobility* osa = mobility.Find("osa_9a33");
    const Anchor* home = anchors.Find("bty21_cp");
    if (osa == nullptr || home == nullptr) {
        std::printf("no osa_9a33 class or no bty21_cp place\n");
        return 1;
    }
    int far = 0, cut_off = 0, failed = 0;
    const char* kinds[] = {"place", "position", "object", "settlement", "landmark"};
    for (const char* kind : kinds) {
        int count = 0, kind_far = 0;
        for (const Anchor& a : anchors.All()) {
            if (a.kind != kind) continue;
            ++count;
            const double d = g.DistanceToRoad(a.at);
            // Which piece the anchor's way onto the road joins: the route from battery 21 tells (a cut-off piece makes
            // it go straight across country the whole way).
            RouteRequest q;
            q.from = home->at;
            q.to = a.at;
            q.mobility = osa;
            const Route r = g.Find(q);
            if (!r.found) {
                ++failed;
                std::printf("  NO ROUTE %-10s %s (%s): %s\n", kind, a.id.c_str(), a.name.c_str(), r.why.c_str());
                continue;
            }
            bool road = false;
            for (const RouteLeg& l : r.legs) road = road || l.kind == "road";
            const int node = r.nodes.size() > 2 ? r.nodes[r.nodes.size() - 2] : -1;
            if (d > 3000.0) {
                ++far;
                ++kind_far;
                if (std::string(kind) == "place" || std::string(kind) == "position") {
                    std::printf("  off the road net  %-10s %-28s %5.1f km from a road\n", kind, a.id.c_str(), d / 1000.0);
                }
            } else if (node >= 0 && piece[static_cast<std::size_t>(node)] != main_piece) {
                ++cut_off;
                std::printf("  cut off           %-10s %-28s on a piece of %d nodes\n", kind, a.id.c_str(),
                            sizes[static_cast<std::size_t>(piece[static_cast<std::size_t>(node)])]);
            } else if (!road && r.length_m > 6000.0) {
                std::printf("  no road taken     %-10s %-28s %.1f km straight\n", kind, a.id.c_str(), r.length_m / 1000.0);
            }
        }
        std::printf("%-10s %4d, %3d beyond 3 km of a road\n", kind, count, kind_far);
    }

    // Timing: routes between anchors taken in a fixed order (every k-th pair).
    const std::vector<Anchor>& all = anchors.All();
    const Clock::time_point t1 = Clock::now();
    int routes = 0;
    double hours = 0.0;
    for (int i = 0; i < sample && !all.empty(); ++i) {
        const Anchor& a = all[static_cast<std::size_t>(i * 7) % all.size()];
        const Anchor& b = all[static_cast<std::size_t>(i * 13 + 5) % all.size()];
        RouteRequest q;
        q.from = a.at;
        q.to = b.at;
        q.mobility = osa;
        const Route r = g.Find(q);
        if (r.found) {
            ++routes;
            hours += r.time_s / 3600.0;
        }
    }
    const double route_ms = std::chrono::duration<double, std::milli>(Clock::now() - t1).count();
    std::printf("routes: %d of %d found, %.3f ms each (%.0f h of driving)\n", routes, sample, sample > 0 ? route_ms / sample : 0.0, hours);
    std::printf("anchors: %d beyond 3 km of a road, %d by a cut-off piece, %d with no route\n", far, cut_off, failed);

    // Every place of the chain reaches every other; with --json, the routes as points and stretches (km).
    std::ofstream js;
    if (!json_out.empty()) {
        js.open(json_out, std::ios::binary);
        if (!js) {
            std::printf("cannot write %s\n", json_out.c_str());
            return 1;
        }
        js << "{\"from\": \"bty21_cp\", \"mobility\": \"truck\", \"routes\": [";
    }
    bool first = true;
    for (const Anchor& a : all) {
        if (a.kind != "place") continue;
        RouteRequest q;
        q.from = home->at;
        q.to = a.at;
        q.mobility = mobility.Find("truck");
        const Route r = g.Find(q);
        if (!r.found) {
            std::printf("FAIL: no truck route from bty21_cp to %s\n", a.id.c_str());
            return 1;
        }
        if (!js.is_open() || r.path.size() < 2) continue;
        char buf[96];
        js << (first ? "\n" : ",\n") << "  {\"to\": \"" << a.id << "\", \"time_s\": " << std::llround(r.time_s)
           << ", \"length_m\": " << std::llround(r.length_m) << ", \"points_km\": [";
        for (std::size_t i = 0; i < r.path.size(); ++i) {
            std::snprintf(buf, sizeof(buf), "%s[%.3f, %.3f]", i == 0 ? "" : ", ", r.path[i].east_m / 1000.0, r.path[i].north_m / 1000.0);
            js << buf;
        }
        js << "], \"stretches\": [";
        for (std::size_t i = 0; i < r.stretches.size(); ++i) js << (i == 0 ? "\"" : ", \"") << r.stretches[i].kind << "\"";
        js << "]}";
        first = false;
    }
    if (js.is_open()) {
        js << "\n]}\n";
        std::printf("routes written to %s\n", json_out.c_str());
    }
    return failed > 0 ? 1 : 0;
}
