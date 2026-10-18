// The vehicle's heading reference beyond the drill (roadmap 6.5): the gyro half-compass's drift and its latitude
// correction, the crew chief's compass work, the NPC crew orienting at a halt, and the OR-31M Qc scale.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/cabin.hpp"
#include "simcore/em/replication.hpp"
#include "simcore/orientation.hpp"
#include "simcore/readouts.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scope_feed.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

// An Osa at a halt at latitude `lat`, hull heading 40 deg, five crew, the gameplay model and a readiness state.
std::string Osa(const std::string& readiness, double lat, const std::string& triggers, bool npc_commander,
                const std::string& targets = std::string()) {
    char origin[96];
    std::snprintf(origin, sizeof(origin), R"("origin": { "lat_deg": %.3f, "lon_deg": 25, "alt_m": 0 },)", lat);
    return std::string(R"({
      "scenario_id": "o", "name": "O", "source": "sam3d", "confidence": "estimate", "seed": 3, )") + origin + R"(
      "terrain": { "type": "flat", "height_m": 0 }, )" + (npc_commander ? R"("crew_commander": "npc", )" : "") + R"(
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", )" +
           (readiness.empty() ? std::string() : "\"readiness\": \"" + readiness + "\", ") +
           R"("position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "heading_deg": 40, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ )" + targets + R"( ],
      "triggers": [ )" + triggers + R"( ],
      "end": { "time_limit_s": 100000, "when_targets_gone": false } })";
}

std::unique_ptr<ScenarioRunner> Load(const std::string& text) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

void Run(ScenarioRunner& r, double seconds, double dt = 1.0) {
    for (double end = r.Time() + seconds; r.Time() < end - 1e-9;) r.Step(dt);
}

double Off(double a, double b) {
    double d = std::fmod(a - b, 360.0);
    if (d > 180.0) d -= 360.0;
    if (d < -180.0) d += 360.0;
    return d;
}

}  // namespace

SC_TEST("orientation: a gyro half-compass drifts at the Earth's rate times sin(latitude); the correction cancels what is set") {
    // Hand calculations: 15.041 deg/h * sin 45 deg = 10.636 deg/h, heading falling in the northern hemisphere.
    SC_EXPECT_NEAR(GyroDriftDegPerS(15.041, 45.0, 0.0, false, 0.0) * 3600.0, -10.6356, 1e-3);
    SC_EXPECT_NEAR(GyroDriftDegPerS(15.041, 45.0, 45.0, true, 0.0), 0.0, 1e-15);
    // Set to 40 deg at 45 deg: 15.041 * (sin 45 - sin 40) = 15.041 * (0.70711 - 0.64279) = 0.9675 deg/h left.
    SC_EXPECT_NEAR(GyroDriftDegPerS(15.041, 45.0, 40.0, true, 0.0) * 3600.0, -0.9675, 1e-3);
    // At the equator the vertical component is nil; the own drift is added as it is.
    SC_EXPECT_NEAR(GyroDriftDegPerS(15.041, 0.0, 0.0, false, 0.5) * 3600.0, 0.5, 1e-12);
    // Southern hemisphere: the other way.
    SC_EXPECT_NEAR(GyroDriftDegPerS(15.041, -30.0, 0.0, false, 0.0) * 3600.0, 7.5205, 1e-3);
}

SC_TEST("orientation: the Osa's data sets the correction, the compass work and their inputs") {
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error));
    SC_EXPECT(c.has_orientation);
    SC_EXPECT(c.orientation.latitude_control == "orient.latitude");
    SC_EXPECT_NEAR(c.orientation.earth_rate_deg_h, 15.041, 1e-12);
    SC_EXPECT(c.orientation.survey_s > 0.0 && c.orientation.survey_sigma_deg > 0.0);
    SC_EXPECT(c.orientation.qc_known_input == "qc_known" && c.orientation.oriented_input == "vehicle_oriented");
}

SC_TEST("orientation: the compass survey takes its time at a halt, has its error, and moving off spoils it") {
    CompassSurvey s(180.0, 0.2, 7u);
    s.Start(10.0);
    SC_EXPECT(s.Running() && !s.Known());
    SC_EXPECT(!s.Step(189.0, true, 40.0));
    SC_EXPECT_NEAR(s.Progress(100.0), 0.5, 1e-12);
    SC_EXPECT(s.Step(190.0, true, 40.0));
    SC_EXPECT(s.Known() && std::fabs(Off(s.ResultDeg(), 40.0)) < 1.0);
    s.Step(191.0, false, 40.0);  // the vehicle moves off: the result belonged to the halt
    SC_EXPECT(!s.Known() && !s.Running());
    s.Start(200.0);
    s.Step(250.0, false, 40.0);
    SC_EXPECT(!s.Running());
    s.Start(300.0);
    s.Abort();
    SC_EXPECT(!s.Running() && !s.Known());
    // Over many surveys the error is centred and its spread is the data's sigma.
    double sum = 0.0, sq = 0.0;
    const int n = 4000;
    for (int i = 0; i < n; ++i) {
        CompassSurvey k(0.0, 0.2, static_cast<unsigned>(1000 + i));
        k.Start(0.0);
        k.Step(0.0, true, 359.9);  // across north
        const double e = Off(k.ResultDeg(), 359.9);
        sum += e;
        sq += e * e;
    }
    const double mean = sum / n;
    SC_EXPECT(std::fabs(mean) < 0.02);
    SC_EXPECT_NEAR(std::sqrt(sq / n - mean * mean), 0.2, 0.02);
}

SC_TEST("orientation: an oriented vehicle keeps north while the latitude is set; with the scale left at 0 it runs off") {
    // Deployed and oriented (the readiness sets the site's latitude): after an hour only the gyro's own drift is left.
    auto good = Load(Osa("deployed", 45.0, "", false));
    SC_EXPECT(good != nullptr);
    if (!good) return;
    Run(*good, 3600.0);
    SC_EXPECT(std::fabs(Off(good->Engagement().GyroHeadingDeg(), 40.0)) <= 0.5 + 1e-6);
    SC_EXPECT(std::fabs(Off(good->Engagement().GyroHeadingDeg(), 40.0)) >= 0.5 - 1e-6);  // 0.5 deg/h, one hour
    // The latitude scale at 0: the full Earth rate, 10.64 deg in the hour, plus or minus the own drift.
    auto bad = Load(Osa("deployed", 45.0, "", false));
    SC_EXPECT(bad != nullptr);
    if (!bad) return;
    bad->Console().panel.SetValue("orient.latitude", 0.0);
    Run(*bad, 3600.0);
    const double off = Off(bad->Engagement().GyroHeadingDeg(), 40.0);
    SC_EXPECT(std::fabs(off - (-10.636)) <= 0.5 + 0.01);
    // The crew's picture turns with it: a true 100 deg shows on the wheels at 100 - off.
    SC_EXPECT_NEAR(bad->Engagement().IndicatedToTrueDeg(100.0 + off), 100.0, 1e-6);
    // РЕЛЕ — Qc left at Qc with АК: no latitude correction at all, so the full rate however the scale is set.
    auto relay = Load(Osa("deployed", 45.0, "", false));
    SC_EXPECT(relay != nullptr);
    if (!relay) return;
    relay->Console().panel.Set("orient.relay", 1);
    Run(*relay, 3600.0);
    SC_EXPECT(std::fabs(Off(relay->Engagement().GyroHeadingDeg(), 40.0) - (-10.636)) <= 0.5 + 0.01);
}

SC_TEST("orientation: at a halt the NPC commander finds Qc with the compass, commands it, and the deputy orients the gyro") {
    // Cold at a halt: the commander's compass work runs while the equipment warms up.
    auto r = Load(Osa("", 50.0, "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const ConsoleData& c = r->Console();
    r->GiveCrewOrder("order_k_boyu");
    r->Step(0.1);
    SC_EXPECT(r->Compass().Running());
    Run(*r, 175.0, 0.1);
    SC_EXPECT(!c.equipment.GetInput("qc_known"));
    Run(*r, 10.0, 0.1);
    SC_EXPECT(c.equipment.GetInput("qc_known"));
    SC_EXPECT(r->Debrief().find("Курс машины") != std::string::npos);  // he says it to the deputy
    SC_EXPECT(r->Debrief().find("Qc by compass") != std::string::npos);
    for (int i = 0; i < 6000 && !r->OrientedAtHalt(); ++i) r->Step(0.1);
    SC_EXPECT(r->OrientedAtHalt() && c.equipment.GetInput("vehicle_oriented"));
    for (int i = 0; i < 600 && c.panel.Get("orient.relay") != 0; ++i) r->Step(0.1);  // the drill ends at АК, РЕЛЕ
    SC_EXPECT(c.panel.Get("orient.ak_zk") == 0 && c.panel.Get("orient.relay") == 0);
    // The deputy set the site's latitude on the correction and the commanded Qc on the setter.
    SC_EXPECT_NEAR(c.panel.GetValue("orient.latitude"), 50.0, 1e-6);
    SC_EXPECT_NEAR(Off(c.panel.GetValue("orient.qc"), r->Engagement().CommandedQcDeg()), 0.0, 1e-6);
    SC_EXPECT(std::fabs(Off(r->Engagement().GyroHeadingDeg(), 40.0)) < 0.8);
    SC_EXPECT(r->Debrief().find("orient_vehicle") != std::string::npos);
    // Moving off: this halt's Qc and orientation are gone; the gyro keeps its heading.
    r->Console().equipment.SetInput("vehicle_stopped", false);
    r->Step(0.1);
    SC_EXPECT(!c.equipment.GetInput("qc_known") && !r->OrientedAtHalt());
    SC_EXPECT(r->Engagement().Oriented() && c.equipment.GetInput("vehicle_oriented"));
}

SC_TEST("orientation: a contact sends the commander back to his post before he has Qc") {
    const std::string target = R"({ "id": "t1", "name": "Target", "type": "Air+FixedWing", "class": "fighter",
        "start": { "east_m": 0, "north_m": 30000, "up_m": 1000 }, "waypoints": [ { "east_m": 0, "north_m": -30000, "up_m": 1000 } ],
        "speed_mps": 200, "rcs_m2": 3, "appear_at_s": 60 })";
    auto r = Load(Osa("deployed", 45.0, "", true, target));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    // Deployed and oriented: no survey at all.
    Run(*r, 2.0, 0.1);
    SC_EXPECT(!r->Compass().Running());
    // A new halt after a move, then a target before he has Qc: he breaks the survey off.
    r->Console().equipment.SetInput("vehicle_stopped", false);
    Run(*r, 1.0, 0.1);
    r->Console().equipment.SetInput("vehicle_stopped", true);
    Run(*r, 1.0, 0.1);
    SC_EXPECT(r->Compass().Running());
    for (int i = 0; i < 3000 && r->Compass().Running(); ++i) r->Step(0.1);
    SC_EXPECT(!r->Compass().Running() && !r->Compass().Known());
    SC_EXPECT(r->Debrief().find("breaks off the compass work") != std::string::npos);
}

SC_TEST("orientation: the OR-31M Qc scale shows the gyro, turns while it is slaved and stops once they agree") {
    ConsoleData console;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    const CabinDisplay* scale = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.id == "or31_qc") scale = &d;
    }
    SC_EXPECT(scale != nullptr && scale->readout == "gyro_heading" && scale->control.empty());
    if (scale == nullptr) return;
    auto r = Load(Osa("deployed", 45.0, "", false));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    r->Step(0.1);
    // Slave the gyro to a setter 30 deg away and press СОГЛАСОВАНИЕ: the scale turns, then stops.
    c.panel.Set("orient.ak_zk", 1);
    c.panel.Set("orient.relay", 1);
    c.panel.SetValue("orient.qc", 70.0);
    c.panel.Set("orient.sync", 1);
    r->Step(0.1);
    const double a = DialReading(*scale, c, ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time()).gyro_heading_deg);
    r->Step(0.1);
    const double b = DialReading(*scale, c, ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time()).gyro_heading_deg);
    SC_EXPECT(std::fabs(Off(b, a)) > 1.0);  // 15 deg/s: 1.5 deg a step
    for (int i = 0; i < 100 && !c.equipment.GetInput("heading_synced"); ++i) r->Step(0.1);
    SC_EXPECT(c.equipment.GetInput("heading_synced"));
    const double d1 = ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time()).gyro_heading_deg;
    r->Step(0.1);
    const double d2 = ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time()).gyro_heading_deg;
    SC_EXPECT_NEAR(d1, 70.0, 0.06);
    SC_EXPECT_NEAR(Off(d2, d1), 0.0, 1e-9);  // stopped
    // A network client draws the same value from the view.
    EchoKeys keys;
    const em::ViewScope v = ToViewScope(ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time()), keys);
    const std::vector<std::uint8_t> bytes = em::wire::ScopeBytes(v);
    em::wire::Reader rd(bytes);
    em::ViewScope back;
    SC_EXPECT(em::wire::GetScope(rd, back));
    SC_EXPECT_NEAR(back.gyro_heading_deg, d2, 1e-4);
    SC_EXPECT_NEAR(FromViewScope(back).gyro_heading_deg, d2, 1e-4);
}

SC_TEST("orientation: a player commander starts his compass work from the crew menu's order") {
    auto r = Load(Osa("", 45.0, "", false));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("commander", "player"));
    r->Step(0.1);
    SC_EXPECT(!r->Compass().Running());  // nobody does it for him
    SC_EXPECT(r->GiveCrewOrder("order_compass"));
    SC_EXPECT(r->Compass().Running());
    Run(*r, 181.0, 0.1);
    SC_EXPECT(r->Engagement().QcKnown() && r->Console().equipment.GetInput("qc_known"));
    // К бою includes it too (and the deputy's drill then waits for his Qc).
    auto k = Load(Osa("", 45.0, "", false));
    SC_EXPECT(k != nullptr && k->TakeSeat("commander", "player"));
    SC_EXPECT(k->GiveCrewOrder("order_k_boyu") && k->Compass().Running());
}
