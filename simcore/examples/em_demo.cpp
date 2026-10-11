// em_demo — the electromagnetic layer's minimum viable scenario, headless.
//
// Builds simcore/em/demo_scenario.hpp (a fictional Central-European border area with synthetic hills
// and a ridge; generic placeholder equipment from data/em/generic.json, no real system data), runs
// 480 s, prints the timeline and checks the eleven MVP behaviours (docs/EM_HANDOFF.md §6), and writes
// the debug overlays as JSON frames for tools/em_view.py plus a Tacview replay with Red's picture.
// The checks look at truth: they are the referee, not part of the simulation.
//
// Usage: em_demo [data_dir] [out_json] [out_acmi]
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "simcore/acmi.hpp"
#include "simcore/em.hpp"

using namespace simcore;
using namespace simcore::em;

namespace {

const char* kChecks[11] = {
    "1  aircraft emits a radar signal",           "2  SAM radar detects the aircraft",
    "3  detection probability rises with closure", "4  terrain masks the low raider",
    "5  radar forms a track",                     "6  track carries its uncertainty",
    "7  aircraft switches its radar off (EMCON)",  "8  passive sensors hear the emission",
    "9  three DF sites triangulate the emitter",  "10 jammer degrades the radar",
    "11 C2 spreads the picture; losing it matters"};

}  // namespace

int main(int argc, char** argv) {
    const std::string data_dir = argc > 1 ? argv[1] : "data";
    const std::string json_path = argc > 2 ? argv[2] : "out/em_demo.json";
    const std::string acmi_path = argc > 3 ? argv[3] : "out/em_demo.acmi";

    EmLibrary lib;
    const EmLoadResult load = LoadEmLibraryFile(data_dir + "/em/generic.json", lib);
    if (!load.ok) {
        std::fprintf(stderr, "em_demo: %s\n", load.error.c_str());
        return 2;
    }
    const GridTerrain terrain = MakeDemoTerrain();
    const GeoPoint origin = GeoPoint::FromDegrees(49.2, 16.6, 0.0);
    EmDemoScenario demo(origin, &terrain, 7);
    std::string error;
    if (!demo.Build(lib, &error)) {
        std::fprintf(stderr, "em_demo: %s\n", error.c_str());
        return 2;
    }
    EmWorld& w = demo.Layer();
    EmScene& s = w.Scene();

    bool ok[11] = {false, false, false, false, false, false, false, false, false, false, false};
    double first_esm_s = -1.0, first_radar_on_striker_s = -1.0, fix_err = -1.0, fix_cep = -1.0;
    double pd_far_sum = 0.0, pd_near_sum = 0.0, jn_max_db = -300.0, track_sigma_example = -1.0;
    int pd_far_n = 0, pd_near_n = 0, raider_masked_looks = 0, raider_hits_behind = 0, raider_hits_after = 0;
    bool sam2_network_before = false, sam2_autonomous_after = false, sam2_radar_after = false;
    AcmiWriter acmi(s.frame, "2026-10-07T08:00:00Z", "SAM3D EM layer MVP");
    std::string frames = "[";
    bool first_frame = true;
    std::size_t printed_events = 0;
    const double dt = 0.1;
    const int ticks = 4800;  // 480 s

    std::printf("SAM3D electromagnetic layer - MVP scenario (generic placeholder equipment)\n\n");
    for (int i = 0; i < ticks; ++i) {
        demo.Step(dt);

        const Platform& sp = *s.FindPlatform(demo.striker);
        const Platform& rp = *s.FindPlatform(demo.raider);
        if (s.FindEmitter(demo.striker_fcr)->InAir()) ok[0] = true;
        const RadarState& r1 = *s.FindRadar(demo.sam1_search);
        for (const LookRecord& l : r1.last_looks) {
            if (l.platform_id == demo.striker) {
                if (l.detected) {
                    ok[1] = true;
                    if (first_radar_on_striker_s < 0.0) first_radar_on_striker_s = w.Time();
                }
                if (l.range_m > 50000.0) {
                    pd_far_sum += l.pd;
                    ++pd_far_n;
                } else if (l.range_m < 40000.0 && w.Time() < 200.0) {
                    pd_near_sum += l.pd;
                    ++pd_near_n;
                }
                if (l.jn_db > jn_max_db) jn_max_db = l.jn_db;
            }
            if (l.platform_id == demo.raider) {
                // North of the ridge line (5, 30) km - (50, 24) km by more than its half-width.
                const bool behind = rp.position.y - (30000.0 - (rp.position.x - 5000.0) * 6.0 / 45.0) > 1500.0;
                if (!l.line_of_sight && behind) {
                    ++raider_masked_looks;
                    raider_hits_behind += l.detected ? 1 : 0;
                }
                if (!behind && l.detected) ++raider_hits_after;
            }
        }
        for (const SensorTrack& tr : r1.tracker.Tracks()) {
            if (tr.status == TrackStatus::Confirmed &&
                (tr.debug_truth_platform == demo.striker || tr.debug_truth_platform == demo.raider)) {
                ok[4] = true;
                if (track_sigma_example < 0.0) track_sigma_example = tr.PositionSigmaM();
            }
        }
        for (int id : demo.df_esm)
            if (!s.FindEsm(id)->contacts.empty() && first_esm_s < 0.0) first_esm_s = w.Time();
        if (w.Time() > 60.0 && w.Time() < 125.0) {
            for (const FusedTrack& f : s.FindNode(demo.cp)->kb.Picture(w.Time())) {
                if (f.from_fix && f.fused.emitter.library_id == "blue_fighter_fcr") {
                    fix_err = GroundRange(f.fused.position, sp.position);
                    fix_cep = CepM(Horizontal(f.fused.cov));
                }
            }
        }
        if (w.Time() > 200.0 && w.Time() < 320.0) {
            for (const KnowledgeTrack& k : s.FindNode(demo.sam2)->kb.Entries())
                sam2_network_before = sam2_network_before || (k.source == InfoSource::Network && k.position_valid);
        }
        if (w.Time() > 460.0) {
            sam2_autonomous_after = s.FindNode(demo.sam2)->status == C2Status::Autonomous;
            sam2_radar_after = s.FindRadar(demo.sam2_search)->on;
        }

        for (; printed_events < s.events.size(); ++printed_events) {
            const EmEvent& e = s.events[printed_events];
            if (e.kind == EmEventKind::MessageSent || e.kind == EmEventKind::MessageDelivered) continue;
            std::printf("%6.1f s  %-17s %s\n", e.time_s, EmEventKindName(e.kind), e.text.c_str());
        }

        if (i % 20 == 19) {  // outputs every 2 s
            acmi.BeginFrame(w.Time());
            for (const Platform& p : s.platforms) {
                if (p.kind == PlatformKind::Ground && !p.alive) continue;
                acmi.WriteObject(p.id, p.position, p.velocity,
                                 std::string(p.kind == PlatformKind::Ground ? "Type=Ground+Static" : "Type=Air+FixedWing") +
                                     ",Name=" + p.name + ",Color=" + (p.side == Side::Blue ? "Blue" : "Red"));
            }
            if (const NodeState* n = s.FindNode(demo.cp)) {
                if (n->alive) {
                    for (const FusedTrack& f : n->kb.Picture(w.Time())) {
                        if (!f.fused.position_valid) continue;
                        acmi.WriteObject(100000 + f.id, f.fused.position, f.fused.velocity,
                                         "Type=Navaid+Static,Name=CP picture #" + std::to_string(f.id) + ",Color=Orange");
                    }
                }
            }
            const std::vector<DebugPrim> prims = BuildDebugScene(s, kLayerAll, -1, 2.0);
            if (!first_frame) frames += ",\n";
            first_frame = false;
            char head[64];
            std::snprintf(head, sizeof(head), "{\"t\":%.1f,\"prims\":", w.Time());
            frames += head;
            frames += PrimsToJson(prims);
            frames += "}";
        }
    }
    frames += "]";

    ok[2] = pd_far_n > 0 && pd_near_n > 0 && pd_near_sum / pd_near_n > 2.0 * (pd_far_sum / pd_far_n) + 0.1;
    ok[3] = raider_masked_looks > 5 && raider_hits_behind == 0 && raider_hits_after > 0;
    ok[5] = track_sigma_example > 0.0;
    ok[6] = !s.FindEmitter(demo.striker_fcr)->radiating;
    ok[7] = first_esm_s >= 0.0 && (first_radar_on_striker_s < 0.0 || first_esm_s < first_radar_on_striker_s);
    ok[8] = fix_err >= 0.0 && fix_err < 4.0 * fix_cep + 1000.0;
    ok[9] = jn_max_db > 10.0;
    ok[10] = sam2_network_before && sam2_autonomous_after && sam2_radar_after;

    std::printf("\nMVP checks\n");
    int passed = 0;
    for (int k = 0; k < 11; ++k) {
        std::printf("  [%s] %s\n", ok[k] ? "PASS" : "FAIL", kChecks[k]);
        passed += ok[k] ? 1 : 0;
    }
    std::printf("\n  passive detection of the striker at %.1f s, first radar detection at %.1f s\n", first_esm_s,
                first_radar_on_striker_s);
    std::printf("  CP's triangulated fix on the striker's radar: error %.1f km, CEP %.1f km\n", fix_err / 1000.0,
                fix_cep / 1000.0);
    std::printf("  mean Pd beyond 50 km %.2f, inside 40 km (before jamming) %.2f; raider masked looks %d (hits %d), "
                "hits after crossing %d\n",
                pd_far_n ? pd_far_sum / pd_far_n : 0.0, pd_near_n ? pd_near_sum / pd_near_n : 0.0, raider_masked_looks,
                raider_hits_behind, raider_hits_after);
    std::printf("  strongest J/N seen by SAM-1 on the striker's looks: %.1f dB\n", jn_max_db);
    const EmStats& st = s.stats;
    std::printf("  work: %llu radar looks (%llu culled), %llu ESM evaluations (%llu culled), %llu link evaluations,\n"
                "        path cache %llu hits / %llu misses\n",
                static_cast<unsigned long long>(st.radar_looks), static_cast<unsigned long long>(st.radar_candidates_culled),
                static_cast<unsigned long long>(st.esm_evaluations), static_cast<unsigned long long>(st.esm_culled),
                static_cast<unsigned long long>(st.link_evaluations), static_cast<unsigned long long>(s.paths.Hits()),
                static_cast<unsigned long long>(s.paths.Misses()));

    // Scene metadata for the viewer: layers, node names, terrain (2 km grid), events, checks.
    std::string meta = "{\"title\":\"SAM3D EM layer - MVP scenario\",\"default_owner\":" + std::to_string(demo.cp) +
                       ",\"origin\":[";
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.6f,%.6f],\"layers\":{", origin.LatDeg(), origin.LonDeg());
    meta += buf;
    for (int b = 0; b < 15; ++b) {
        std::snprintf(buf, sizeof(buf), "%s\"%u\":\"%s\"", b == 0 ? "" : ",", 1u << b, DebugLayerName(1u << b));
        meta += buf;
    }
    meta += "},\"nodes\":{";
    for (std::size_t k = 0; k < s.nodes.size(); ++k) {
        std::snprintf(buf, sizeof(buf), "%s\"%d\":", k == 0 ? "" : ",", s.nodes[k].id);
        meta += buf;
        AppendJsonString(meta, s.nodes[k].name);
    }
    meta += "},\"terrain\":{\"x0\":-60000,\"y0\":-40000,\"step\":2000,\"cols\":81,\"rows\":71,\"h\":[";
    for (int r = 0; r < 71; ++r)
        for (int c = 0; c < 81; ++c) {
            std::snprintf(buf, sizeof(buf), "%s%.0f", (r == 0 && c == 0) ? "" : ",",
                          terrain.HeightAt(-60000.0 + 2000.0 * c, -40000.0 + 2000.0 * r));
            meta += buf;
        }
    meta += "]},\"events\":[";
    bool first_ev = true;
    for (const EmEvent& e : s.events) {
        if (e.kind == EmEventKind::MessageSent) continue;
        std::snprintf(buf, sizeof(buf), "%s{\"t\":%.1f,\"k\":", first_ev ? "" : ",", e.time_s);
        meta += buf;
        AppendJsonString(meta, EmEventKindName(e.kind));
        meta += ",\"x\":";
        AppendJsonString(meta, e.text);
        meta += "}";
        first_ev = false;
    }
    meta += "],\"checks\":[";
    for (int k = 0; k < 11; ++k) {
        meta += k == 0 ? "" : ",";
        meta += "{\"name\":";
        AppendJsonString(meta, kChecks[k]);
        meta += ok[k] ? ",\"ok\":true}" : ",\"ok\":false}";
    }
    meta += "]}";

    std::ofstream jf(json_path, std::ios::binary);
    if (jf) {
        const std::string doc = "{\"meta\":" + meta + ",\n\"frames\":" + frames + "}\n";
        jf.write(doc.data(), static_cast<std::streamsize>(doc.size()));
        std::printf("\nwrote %s (%zu bytes)\n", json_path.c_str(), doc.size());
    } else {
        std::printf("\n(could not write %s)\n", json_path.c_str());
    }
    if (acmi.SaveTo(acmi_path)) std::printf("wrote %s\n", acmi_path.c_str());
    return passed == 11 ? 0 : 1;
}
