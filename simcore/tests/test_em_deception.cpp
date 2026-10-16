// Deception and EMCON depth (roadmap E7): decoys on the same parameters as a radar, frequency changes against a spot
// jammer, false-target repeaters, and dummy radio traffic against intercept and DF. Generic placeholder numbers only.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/em.hpp"
#include "test_framework.hpp"

#ifndef SIMCORE_DATA_DIR
#define SIMCORE_DATA_DIR "data"
#endif

using namespace simcore;
using namespace simcore::em;

namespace {

EmLibrary Lib() {
    EmLibrary lib;
    const EmLoadResult r = LoadEmLibraryFile(std::string(SIMCORE_DATA_DIR) + "/em/generic.json", lib);
    if (!r.ok) std::printf("    %s\n", r.error.c_str());
    return lib;
}

const GeoPoint kOrigin = GeoPoint::FromDegrees(49.5, 17.0, 0.0);

int Aircraft(EmWorld& w, const char* name, Side side, double e, double n, double alt) {
    Platform p;
    p.name = name;
    p.side = side;
    p.kind = PlatformKind::FixedWing;
    p.rcs = RcsModel{3.0, 6.0, 3.0};
    p.position = {e, n, alt - (e * e + n * n) / (2.0 * wgs84::kMeanRadius)};
    return w.AddPlatform(p);
}

// Two blue ESM aircraft holding station, cabled to one node that fixes what both hear.
int BlueEsmNet(EmWorld& w, const char* receiver, double north) {
    const int a = Aircraft(w, "ESM 1", Side::Blue, -25000.0, north, 8000.0);
    const int b = Aircraft(w, "ESM 2", Side::Blue, 25000.0, north, 8000.0);
    const int node = w.AddNode("Blue ESM", a, Side::Blue, true);
    w.AttachSensor(node, w.AddEsm(a, receiver));
    w.AttachSensor(node, w.AddEsm(b, receiver));
    return node;
}

std::vector<FusedTrack> Fixes(const EmWorld& w, int node) {
    std::vector<FusedTrack> out;
    for (const FusedTrack& f : w.Scene().FindNode(node)->kb.Picture(w.Time()))
        if (f.from_fix) out.push_back(f);
    return out;
}

}  // namespace

SC_TEST("em deception: a decoy on the radar's parameters is identified as the radar, and the fix lands off the radar") {
    // Without the decoy, the blue ESM net fixes the radar; with it, the two signals are one signature to the receivers,
    // and the fix goes elsewhere (between them, or on the decoy).
    double err[2] = {0.0, 0.0};
    std::string id[2];
    for (int with = 0; with < 2; ++with) {
        EmWorld w(kOrigin, nullptr, 5);
        SC_EXPECT(w.SetLibrary(Lib()));
        const int node = BlueEsmNet(w, "generic_esm_pod", 60000.0);
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
        const int radar = w.AddRadar(site, "generic_search_c", true);
        if (with == 1) w.AddDecoy(w.AddGroundPlatform("Decoy", Side::Red, 9000.0, -2000.0, 5.0), radar, DecoyMode::WithRadar);
        for (int i = 0; i < 900; ++i) w.Step(0.1);
        const std::vector<FusedTrack> fixes = Fixes(w, node);
        SC_EXPECT(!fixes.empty());
        if (fixes.empty()) continue;
        double best = 1e18;
        for (const FusedTrack& f : fixes) {
            const double d = GroundRange(f.fused.position, w.Scene().FindPlatform(site)->position);
            if (d < best) {
                best = d;
                id[with] = f.fused.type_guess;
            }
        }
        err[with] = best;
        // what the receivers called each signal
        for (const EsmState& e : w.Scene().esm)
            for (const EsmContact& c : e.contacts) SC_EXPECT(c.sig.library_id == "red_sam_search");
    }
    std::printf("    nearest fix to the radar: %.1f km alone, %.1f km with the decoy ('%s' / '%s')\n", err[0] / 1000.0,
                err[1] / 1000.0, id[0].c_str(), id[1].c_str());
    SC_EXPECT(err[0] < 2500.0);
    SC_EXPECT(err[1] > err[0] + 1500.0);
}

SC_TEST("em deception: decoy modes: with the radar, while it is off, always; and it follows a frequency change") {
    EmWorld w(kOrigin, nullptr, 1);
    SC_EXPECT(w.SetLibrary(Lib()));
    const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
    const int radar = w.AddRadar(site, "generic_search_c", true);
    const int with = w.AddDecoy(w.AddGroundPlatform("D1", Side::Red, 3000.0, 0.0, 5.0), radar, DecoyMode::WithRadar);
    const int off = w.AddDecoy(w.AddGroundPlatform("D2", Side::Red, -3000.0, 0.0, 5.0), radar, DecoyMode::WhenRadarOff);
    const int always = w.AddDecoy(w.AddGroundPlatform("D3", Side::Red, 0.0, 3000.0, 5.0), radar, DecoyMode::Always);
    w.Step(0.1);
    EmScene& s = w.Scene();
    SC_EXPECT(s.FindEmitter(with)->radiating && !s.FindEmitter(off)->radiating && s.FindEmitter(always)->radiating);
    s.FindRadar(radar)->on = false;
    w.Step(0.1);
    SC_EXPECT(!s.FindEmitter(with)->radiating && s.FindEmitter(off)->radiating && s.FindEmitter(always)->radiating);
    // the decoys copy the radar's waveform and follow it to another channel
    const EmitterState& re = *s.FindEmitter(s.FindRadar(radar)->emitter_id);
    SC_EXPECT_NEAR(s.FindEmitter(with)->def.waveform.prf_hz, re.def.waveform.prf_hz, 1e-9);
    SC_EXPECT(w.SetEmitterChannel(re.id, 0));
    SC_EXPECT(!w.SetEmitterChannel(re.id, 7));
    w.Step(0.1);
    SC_EXPECT_NEAR(s.FindEmitter(always)->freq_hz, 5.9e9, 1.0);
}

SC_TEST("em deception: a frequency change escapes a spot jammer until it re-tunes") {
    EmWorld w(kOrigin, nullptr, 2);
    SC_EXPECT(w.SetLibrary(Lib()));
    const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
    const int radar = w.AddRadar(site, "generic_search_c", true);
    const int jp = Aircraft(w, "SOJ", Side::Blue, 0.0, 45000.0, 7000.0);
    const int j = w.AddJammer(jp, "generic_soj_spot_c", true);
    JammerState& js = *w.Scene().FindJammer(j);
    js.victim_radar = radar;
    js.aim_at_point = true;
    js.aim_point = w.Scene().FindPlatform(site)->position;
    auto strobe_since = [&](double t0) {
        for (const JamStrobe& st : w.Scene().FindRadar(radar)->strobes)
            if (st.time_s >= t0) return true;
        return false;
    };
    for (int i = 0; i < 60; ++i) w.Step(0.1);  // 6 s, two turns: jammed on channel 2 (6.0 GHz)
    SC_EXPECT(strobe_since(0.0));
    const int emitter = w.Scene().FindRadar(radar)->emitter_id;
    SC_EXPECT(w.SetEmitterChannel(emitter, 0));  // 5.9 GHz: outside the 60 MHz spot
    const double changed = w.Time();
    for (int i = 0; i < 70; ++i) w.Step(0.1);   // 7 s: the jammer has not re-tuned (8 s)
    SC_EXPECT(!strobe_since(changed + 0.05));
    for (int i = 0; i < 80; ++i) w.Step(0.1);   // 15 s: re-tuned at 8 s, a turn later the strobe is back
    SC_EXPECT(strobe_since(changed + 8.0));
    bool logged = false;
    for (const EmEvent& e : w.Scene().events) logged = logged || e.text.find("re-tunes") != std::string::npos;
    SC_EXPECT(logged);
}

SC_TEST("em deception: a false-target repeater leaves a string of ghost tracks behind itself") {
    EmWorld w(kOrigin, nullptr, 4);
    SC_EXPECT(w.SetLibrary(Lib()));
    const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
    const int radar = w.AddRadar(site, "generic_search_c", true);
    const int jp = Aircraft(w, "attacker", Side::Blue, 0.0, 25000.0, 3000.0);
    w.Scene().FindPlatform(jp)->velocity = {0.0, -200.0, 0.0};  // closing (held in place: the test is about the picture)
    const int j = w.AddJammer(jp, "generic_false_targets_c", true);
    w.Scene().FindJammer(j)->aim_at_point = true;
    w.Scene().FindJammer(j)->aim_point = w.Scene().FindPlatform(site)->position;
    for (int i = 0; i < 300; ++i) w.Step(0.1);  // 30 s, ten turns
    int real = 0, ghosts = 0;
    for (const SensorTrack& t : w.Scene().FindRadar(radar)->tracker.Tracks()) {
        if (t.status != TrackStatus::Confirmed) continue;
        const double r = (t.Position() - w.Scene().FindPlatform(site)->position).Length();
        if (t.debug_truth_platform == jp) ++real;
        else if (r > 25500.0) ++ghosts;
    }
    std::printf("    confirmed: %d on the aircraft, %d ghosts behind it\n", real, ghosts);
    SC_EXPECT(real == 1);
    SC_EXPECT(ghosts >= 3);
    // repeaters are not noise: no strobe
    SC_EXPECT(w.Scene().FindRadar(radar)->strobes.empty());
}

SC_TEST("em deception: radio intercept fixes a dummy station; the silent battery is not found until it talks") {
    EmWorld w(kOrigin, nullptr, 6);
    SC_EXPECT(w.SetLibrary(Lib()));
    const int node = BlueEsmNet(w, "generic_comms_intercept", 50000.0);
    // the battery (silent: radio discipline) and a dummy station 8 km away with a partner node
    const int bat_p = w.AddGroundPlatform("Battery", Side::Red, 0.0, 0.0, 4.0);
    const int bat = w.AddNode("Battery", bat_p, Side::Red, true);
    const int dummy_p = w.AddGroundPlatform("Dummy", Side::Red, 8000.0, 3000.0, 4.0);
    const int dummy = w.AddNode("Dummy", dummy_p, Side::Red);
    const int net = w.AddNet("battery net", "generic_vhf_fm", 40e6, {bat, dummy});
    w.Scene().FindNode(bat)->c2_center = false;  // the battery sends no pictures: it keeps silent
    w.AddDummyTraffic(dummy, net, 20.0, 8.0);
    for (int i = 0; i < 1200; ++i) w.Step(0.1);  // 2 min
    const std::vector<FusedTrack> fixes = Fixes(w, node);
    double to_dummy = 1e18, to_battery = 1e18;
    for (const FusedTrack& f : fixes) {
        to_dummy = std::fmin(to_dummy, GroundRange(f.fused.position, w.Scene().FindPlatform(dummy_p)->position));
        to_battery = std::fmin(to_battery, GroundRange(f.fused.position, w.Scene().FindPlatform(bat_p)->position));
    }
    std::printf("    %zu fixes; nearest to the dummy %.1f km, to the battery %.1f km\n", fixes.size(), to_dummy / 1000.0,
                to_battery / 1000.0);
    SC_EXPECT(!fixes.empty());
    SC_EXPECT(to_dummy < 3000.0);
    SC_EXPECT(to_battery > 5000.0);
    // the battery talks once (a voice designation): now it gives itself away too
    for (int k = 0; k < 4; ++k) {
        SendVoice(w.Scene(), *w.Scene().FindNet(net), *w.Scene().FindNode(bat), dummy, w.Time() + 0.5 + 25.0 * k,
                  w.Time() + 8.5 + 25.0 * k);
    }
    for (int i = 0; i < 1200; ++i) w.Step(0.1);
    double now_battery = 1e18;
    for (const FusedTrack& f : Fixes(w, node))
        now_battery = std::fmin(now_battery, GroundRange(f.fused.position, w.Scene().FindPlatform(bat_p)->position));
    std::printf("    after the battery's own voice traffic: nearest fix %.1f km from it\n", now_battery / 1000.0);
    // its voice and the dummy's carry one signature on one net: the fix moves from the dummy towards the battery,
    // landing between the two, which still costs the enemy a clean fix of either
    SC_EXPECT(now_battery < to_battery - 3000.0);
}

SC_TEST("em deception: the library refuses a false-target repeater without targets") {
    EmLibrary lib;
    const std::string text = R"({ "library_id": "x", "status": "test", "provenance": "placeholder", "jammers": [
        { "id": "j", "name": "J", "technique": "false_targets", "emitter": { "freq_hz": 6e9, "bandwidth_hz": 1e8, "peak_power_w": 1 },
          "source": "PLACEHOLDER", "confidence": "estimate" } ] })";
    const EmLoadResult r = LoadEmLibraryText(text, lib);
    std::printf("    %s\n", r.error.c_str());
    SC_EXPECT(!r.ok && r.error.find("false_targets") != std::string::npos);
}
