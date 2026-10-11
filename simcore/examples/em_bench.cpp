// em_bench — how the electromagnetic layer scales (docs/EM_ARCHITECTURE.md, "Performance").
//
// Random theatre 300 x 300 km on hilly synthetic terrain: N aircraft (a quarter with their radars
// on, a few jammers), ground SAM sites with rotating search radars, ESM sites, command posts and
// voice nets. Runs 60 s of simulated time at 10 Hz and reports wall-clock cost per tick and the
// work counters. Generic placeholder equipment only.
//
// Usage: em_bench [data_dir] [aircraft] [sam_sites] [esm_sites]
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "simcore/em.hpp"

using namespace simcore;
using namespace simcore::em;

int main(int argc, char** argv) {
    const std::string data_dir = argc > 1 ? argv[1] : "data";
    const int n_air = argc > 2 ? std::atoi(argv[2]) : 200;
    const int n_sam = argc > 3 ? std::atoi(argv[3]) : 30;
    const int n_esm = argc > 4 ? std::atoi(argv[4]) : 12;

    EmLibrary lib;
    const EmLoadResult load = LoadEmLibraryFile(data_dir + "/em/generic.json", lib);
    if (!load.ok) {
        std::fprintf(stderr, "em_bench: %s\n", load.error.c_str());
        return 2;
    }
    const int cols = 1501, rows = 1501;  // 200 m grid over 300 km
    std::vector<double> h(static_cast<std::size_t>(cols) * rows);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            h[static_cast<std::size_t>(r) * cols + c] =
                250.0 + 120.0 * std::sin(c * 0.013) * std::cos(r * 0.011) + 60.0 * std::sin((c + 2 * r) * 0.031);
    const GridTerrain terrain(-150000.0, -150000.0, 200.0, cols, rows, h);
    EmWorld w(GeoPoint::FromDegrees(49.5, 17.0, 0.0), &terrain, 99);
    w.SetLibrary(lib);
    EmScene& s = w.Scene();
    Random rng(5);
    auto uni = [&](std::uint64_t k, double lo, double hi) { return lo + (hi - lo) * rng.Uniform(Stream::Doctrine, k); };

    std::vector<int> nodes;
    for (int i = 0; i < n_sam; ++i) {
        const int p = w.AddGroundPlatform("SAM", Side::Red, uni(3 * i, -120000, 120000), uni(3 * i + 1, -120000, 0), 5.0);
        w.AttachSensor(w.AddNode("SAM", p, Side::Red, false, 0), w.AddRadar(p, "generic_search_c", true));
        nodes.push_back(s.nodes.back().id);
    }
    // One command post per ten SAMs, each on its own voice net with them.
    for (int c = 0; c * 10 < n_sam; ++c) {
        const int p = w.AddGroundPlatform("CP", Side::Red, uni(900 + c, -100000, 100000), uni(950 + c, -140000, -60000), 10.0);
        const int cp = w.AddNode("CP", p, Side::Red, true);
        std::vector<int> members{cp};
        for (int i = c * 10; i < n_sam && i < c * 10 + 10; ++i) {
            s.FindNode(nodes[static_cast<std::size_t>(i)])->parent = cp;
            members.push_back(nodes[static_cast<std::size_t>(i)]);
        }
        w.AddNet("net", "generic_vhf_fm", 40e6 + 0.1e6 * c, members);
        for (int e = 0; e < n_esm / 3 + 1 && c * (n_esm / 3 + 1) + e < n_esm; ++e) {
            const int ep = w.AddGroundPlatform("DF", Side::Red, uni(700 + 10 * c + e, -120000, 120000), uni(800 + 10 * c + e, -140000, -40000), 20.0);
            w.AttachSensor(cp, w.AddEsm(ep, "generic_esm_site"));
        }
    }
    const EmitterDef fcr = lib.emitters[static_cast<std::size_t>(lib.Emitter("generic_airborne_fcr_x"))];
    std::vector<int> air;
    for (int i = 0; i < n_air; ++i) {
        Platform p;
        p.name = "air";
        p.side = Side::Blue;
        p.kind = PlatformKind::FixedWing;
        const double e = uni(5000 + i, -150000, 150000), n = uni(6000 + i, -50000, 150000), alt = uni(7000 + i, 100, 9000);
        p.position = {e, n, alt - (e * e + n * n) / (2.0 * wgs84::kMeanRadius)};
        const double hdg = uni(8000 + i, 0, kTwoPi), v = uni(9000 + i, 150, 280);
        p.velocity = {v * std::sin(hdg), v * std::cos(hdg), 0.0};
        const int id = w.AddPlatform(p);
        air.push_back(id);
        if (i % 4 == 0) s.FindEmitter(w.AddEmitter(id, fcr))->radiating = true;
        if (i % 40 == 0) {
            const int j = w.AddJammer(id, "generic_soj_barrage_c", true);
            s.FindJammer(j)->aim_at_point = true;
            s.FindJammer(j)->aim_point = {0.0, -60000.0, 0.0};
        }
    }

    const int ticks = 600;
    double worst_ms = 0.0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < ticks; ++k) {
        for (int id : air) {
            Platform* p = s.FindPlatform(id);
            p->position += p->velocity * 0.1;
        }
        const auto a = std::chrono::steady_clock::now();
        w.Step(0.1);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
        if (ms > worst_ms) worst_ms = ms;
    }
    const double total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const EmStats& st = s.stats;
    std::size_t tracks = 0, contacts = 0;
    for (const RadarState& r : s.radars) tracks += r.tracker.Tracks().size();
    for (const EsmState& e : s.esm) contacts += e.contacts.size();
    std::printf("em_bench: %d aircraft (%d radiating, %d jammers), %d SAM search radars, %d ESM sites, %zu nodes, %zu nets\n", n_air,
                (n_air + 3) / 4, (n_air + 39) / 40, n_sam, n_esm, s.nodes.size(), s.nets.size());
    std::printf("  60 s simulated at 10 Hz: mean %.2f ms per tick, worst %.2f ms (one core)\n", total_ms / ticks, worst_ms);
    std::printf("  per tick: %.0f radar looks, %.0f ESM evaluations, %.0f link evaluations\n",
                static_cast<double>(st.radar_looks) / ticks, static_cast<double>(st.esm_evaluations) / ticks,
                static_cast<double>(st.link_evaluations) / ticks);
    std::printf("  culled: %.0f radar candidates and %.0f ESM pairs per tick; path cache %.1f %% hits (%llu profiles)\n",
                static_cast<double>(st.radar_candidates_culled) / ticks, static_cast<double>(st.esm_culled) / ticks,
                100.0 * static_cast<double>(s.paths.Hits()) / static_cast<double>(s.paths.Hits() + s.paths.Misses() + 1),
                static_cast<unsigned long long>(s.paths.Misses()));
    std::printf("  at the end: %zu sensor tracks, %zu ESM contacts\n", tracks, contacts);
    return 0;
}
