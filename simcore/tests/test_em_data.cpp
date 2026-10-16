// Electromagnetic data files: loader rules and the Osa-AK calibration checks against published figures.
#include <cstdio>
#include <string>

#include "simcore/em.hpp"
#include "test_framework.hpp"

#ifndef SIMCORE_DATA_DIR
#define SIMCORE_DATA_DIR "data"
#endif

using namespace simcore;
using namespace simcore::em;

static std::string DataPath(const char* rel) { return std::string(SIMCORE_DATA_DIR) + "/" + rel; }

SC_TEST("em data: the placeholder library loads and every radar calibrates") {
    EmLibrary lib;
    const EmLoadResult r = LoadEmLibraryFile(DataPath("em/generic.json"), lib);
    if (!r.ok) std::printf("    %s\n", r.error.c_str());
    SC_EXPECT(r.ok);
    SC_EXPECT(r.provenance_mode == "placeholder");
    SC_EXPECT(lib.radars.size() == 2 && lib.esm.size() == 5 && lib.jammers.size() == 7 && lib.radios.size() == 2);
    SC_EXPECT(lib.emitters.size() == 2 && lib.threat_libraries.size() == 3);
    EmWorld w(GeoPoint::FromDegrees(49.5, 17.0, 300.0));
    std::string err;
    SC_EXPECT(w.SetLibrary(lib, &err));
    for (const RadarDef& d : w.Scene().lib.radars) SC_EXPECT(d.loop_gain > 0.0);
    // Radio fitted to its range anchor: margin 0 at 20 km between 3 m antennas over flat ground.
    const RadioDef& vhf = w.Scene().lib.radios[0];
    const double f = 0.5 * (vhf.freq_min_hz + vhf.freq_max_hz);
    SC_EXPECT_NEAR(FlatGroundRxDbw(vhf, w.Scene().env, 20000.0, 3.0, f) - vhf.system_loss_db, vhf.sensitivity_dbw, 1e-6);
    SC_EXPECT(vhf.cosite_desense_db > 3.0 && vhf.cosite_desense_db < 5.0);  // ~40 log10(20/16) = 3.9 dB
}

SC_TEST("em data: a real-system file must cite every value") {
    const char* bad = R"({"library_id": "x", "status": "s", "provenance": "per_value",
      "radios": [{"id": "r", "name": "r", "freq_min_hz": 30e6, "freq_max_hz": {"value": 76e6, "source": "s", "confidence": "high"},
                  "power_w": {"value": 20, "source": "s", "confidence": "high"},
                  "sensitivity_dbw": {"value": -130, "source": "s", "confidence": "high"}, "source": "s", "confidence": "high"}]})";
    EmLibrary lib;
    const EmLoadResult r = LoadEmLibraryText(bad, lib);
    SC_EXPECT(!r.ok);
    SC_EXPECT(r.error.find("radios[0].freq_min_hz") != std::string::npos);
    const char* unknown = R"({"library_id": "x", "status": "s", "provenance": "placeholder", "radars": [{"id": "a", "colour": 1}]})";
    SC_EXPECT(!LoadEmLibraryText(unknown, lib).ok);
    SC_EXPECT(lib.radios.empty());  // a failed load leaves the library untouched
}

SC_TEST("em data: Osa-AK file loads with per-value provenance") {
    EmLibrary lib;
    const EmLoadResult r = LoadEmLibraryFile(DataPath("em/osa_ak.json"), lib);
    if (!r.ok) std::printf("    %s\n", r.error.c_str());
    SC_EXPECT(r.ok);
    SC_EXPECT(r.provenance_mode == "per_value");
    SC_EXPECT(r.values.size() > 50);
    int estimates = 0, cited = 0;
    for (const ValueProvenance& v : r.values) {
        SC_EXPECT(!v.source.empty());
        (v.confidence == "estimate" ? estimates : cited) += 1;
    }
    SC_EXPECT(cited > 20 && estimates > 10);
    SC_EXPECT(lib.Radar("osa_ak_soc") >= 0 && lib.Radar("osa_ak_ssc") >= 0 && lib.Radio("r123m") >= 0);
}

// The calibration check the Osa-AK spec records (docs/research/OSA_EM_SPEC_2026-10.md §5): fitted
// to the high-altitude figures, the model's low-altitude ranges are compared with Korovin's, and
// its Pd 0.8 ranges with the Osa-AK manual's 20-35 km "depending on altitude" for a MiG-21.
SC_TEST("em data: Osa-AK radars reproduce the published ranges") {
    EmLibrary lib;
    SC_EXPECT(LoadEmLibraryFile(DataPath("em/osa_ak.json"), lib).ok);
    EmWorld w(GeoPoint::FromDegrees(49.5, 17.0, 300.0));
    SC_EXPECT(w.SetLibrary(lib));
    const Environment& env = w.Scene().env;
    const RadarDef& soc = w.Scene().lib.radars[static_cast<std::size_t>(w.Scene().lib.Radar("osa_ak_soc"))];
    const RadarDef& ssc = w.Scene().lib.radars[static_cast<std::size_t>(w.Scene().lib.Radar("osa_ak_ssc"))];
    const double soc_high = SustainedDetectionRange(soc, env, 5000.0, 3.0, 0.5);
    const double soc_low = SustainedDetectionRange(soc, env, 50.0, 3.0, 0.5);
    const double ssc_high = SustainedDetectionRange(ssc, env, 5000.0, 3.0, 0.5);
    const double ssc_low = SustainedDetectionRange(ssc, env, 50.0, 3.0, 0.5);
    std::printf("    Osa-AK SOC held from %.1f km at 5 km altitude (published 40), %.1f km at 50 m (published 27)\n",
                soc_high / 1000.0, soc_low / 1000.0);
    std::printf("    Osa-AK SSC held from %.1f km at 5 km altitude (published 23), %.1f km at 50 m (published 14)\n",
                ssc_high / 1000.0, ssc_low / 1000.0);
    for (double alt : {50.0, 100.0, 300.0, 1000.0, 3000.0, 5000.0})
        std::printf("      alt %5.0f m: SOC first %5.1f held %5.1f Pd.8 %5.1f km | SSC first %5.1f held %5.1f Pd.8 %5.1f km\n",
                    alt, DetectionRangeAt(soc, env, alt, 3.0, 0.5) / 1000.0,
                    SustainedDetectionRange(soc, env, alt, 3.0, 0.5) / 1000.0,
                    SustainedDetectionRange(soc, env, alt, 3.0, 0.8) / 1000.0, DetectionRangeAt(ssc, env, alt, 3.0, 0.5) / 1000.0,
                    SustainedDetectionRange(ssc, env, alt, 3.0, 0.5) / 1000.0,
                    SustainedDetectionRange(ssc, env, alt, 3.0, 0.8) / 1000.0);
    // Fitted: exact at the anchors.
    SC_EXPECT_NEAR(AnchorPd(soc, env, 40000.0, 5000.0, 3.0), 0.5, 1e-6);
    SC_EXPECT_NEAR(AnchorPd(ssc, env, 23000.0, 5000.0, 3.0), 0.5, 1e-6);
    SC_EXPECT_NEAR(soc_high, 40000.0, 4000.0);  // beam II's shape: held from a little beyond the anchor
    SC_EXPECT_NEAR(ssc_high, 23000.0, 1000.0);
    // Not fitted: the propagation model must land within 20 % of the published low-altitude figures.
    SC_EXPECT_NEAR(soc_low, 27000.0, 5400.0);
    // The tracking radar's published 14 km at 50 m is NOT reproduced: at Pd 0.5 the Ku-band first
    // multipath lobe (below 0.1 deg over smooth ground) lifts the 50 m target to ~25 km. An older
    // "Pd 0.8 gives 14 km" reading came from one deep multipath null in this flat-ground path and
    // is not asserted. Recorded as an open calibration item in the spec; this pins the gap so a
    // model change shows up.
    SC_EXPECT(ssc_low > 20000.0);
    // Osa-AK manual: MiG-21-type target detected at 20-35 km depending on altitude (taken as Pd 0.8).
    for (double alt : {100.0, 300.0, 1000.0, 3000.0}) {
        const double r = SustainedDetectionRange(soc, env, alt, 3.0, 0.8);
        SC_EXPECT(r >= 20000.0 && r <= 36000.0);
    }
    // R-123M fitted to the manual's 15 km / 12 km.
    const RadioDef& r123 = w.Scene().lib.radios[static_cast<std::size_t>(w.Scene().lib.Radio("r123m"))];
    std::printf("    R-123M: fitted system loss %.1f dB, co-site desense %.1f dB\n", r123.system_loss_db, r123.cosite_desense_db);
    SC_EXPECT(r123.system_loss_db > -6.0 && r123.system_loss_db < 10.0);
    SC_EXPECT_NEAR(r123.cosite_desense_db, 3.9, 0.5);
}

// E1: the NATO-era library. Each side identifies the other's emitters from its own belief, and the
// Osa's real (calibrated) radars are heard by a NATO RWR through the NATO view of the SA-8.
SC_TEST("em data: NATO-era library loads with provenance and each side identifies the other") {
    EmLibrary lib;
    const EmLoadResult nato = LoadEmLibraryFile(DataPath("em/nato_1985.json"), lib);
    if (!nato.ok) std::printf("    %s\n", nato.error.c_str());
    SC_EXPECT(nato.ok);
    SC_EXPECT(nato.values.size() > 40);
    SC_EXPECT(LoadEmLibraryFile(DataPath("em/osa_ak.json"), lib).ok);
    SC_EXPECT(lib.Esm("nato_alr46_class_rwr") >= 0 && lib.Esm("red_ground_esm_class") >= 0 && lib.Jammer("nato_alq99_band9") >= 0);

    EmWorld w(GeoPoint::FromDegrees(49.5, 17.0, 0.0), nullptr, 5);
    SC_EXPECT(w.SetLibrary(lib));
    EmScene& s = w.Scene();
    // Red side: an Osa search radar (search radar on) and a ground ESM station.
    const int sam = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
    w.AddRadar(sam, "osa_ak_soc", true);
    w.AddRadar(sam, "osa_ak_ssc", true);  // the tracking radar radiates too (pointing north, at the strike aircraft)
    const int esm_site = w.AddGroundPlatform("ESM", Side::Red, 3000.0, 0.0, 20.0);
    const int red_esm = w.AddEsm(esm_site, "red_ground_esm_class");
    // Blue side: a fighter 90 km out with its X-band radar, a noise pod, and an RWR.
    Platform p;
    p.name = "strike";
    p.side = Side::Blue;
    p.kind = PlatformKind::FixedWing;
    const double alt = 6000.0, north = 90000.0;
    p.position = {0.0, north, alt - north * north / (2.0 * wgs84::kMeanRadius)};
    const int jet = w.AddPlatform(p);
    const int fcr = w.AddEmitter(jet, lib.emitters[static_cast<std::size_t>(lib.Emitter("nato_fighter_fcr_x"))]);
    s.FindEmitter(fcr)->radiating = true;
    const int pod = w.AddJammer(jet, "nato_alq99_band9", true);
    s.FindJammer(pod)->aim_at_point = true;
    s.FindJammer(pod)->aim_point = s.FindPlatform(sam)->position;
    const int rwr = w.AddEsm(jet, "nato_alr46_class_rwr");
    for (int i = 0; i < 100; ++i) w.Step(0.1);

    bool fcr_id = false, jammer_id = false;
    for (const EsmContact& c : s.FindEsm(red_esm)->contacts) {
        fcr_id = fcr_id || (c.sig.library_id == "nato_x_band_fcr" && c.sig.hostile);
        jammer_id = jammer_id || (c.sig.library_id == "nato_noise_jammer" && c.sig.hostile);
    }
    SC_EXPECT(fcr_id);
    SC_EXPECT(jammer_id);
    // FM 100-2-3 gives letter bands only (H for the search radar, with other Soviet radars in the same band), so the NATO
    // RWR names the search radar with low confidence; the tracking radar's J band is the SA-8's alone.
    bool saw_sa8 = false, saw_track = false;
    double search_conf = 1.0;
    for (const EsmContact& c : s.FindEsm(rwr)->contacts) {
        if (c.sig.library_id == "sa8_land_roll_search" && c.sig.hostile) {
            saw_sa8 = true;
            search_conf = c.sig.library_confidence;
        }
        saw_track = saw_track || (c.sig.library_id == "sa8_land_roll_track" && c.sig.hostile);
    }
    SC_EXPECT(saw_sa8);
    SC_EXPECT(search_conf < 0.5);
    SC_EXPECT(saw_track);
}
