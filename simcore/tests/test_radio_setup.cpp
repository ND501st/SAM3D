// Tuning the R-123M sets (roadmap 7.8): the fixed frequencies set up under their fixators, smooth tuning, the antenna
// circuit; the crew's set-up drill.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "simcore/radio_setup.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

// A deployed Osa with the PU-12 on preset 1 (no frequency in the scenario: the placeholder plan's 30.000 MHz) and an EWR
// on preset 2 (38.550 MHz); `presets` "set" or "unset".
std::string Text(const std::string& presets, bool npc_commander = false) {
    return std::string(R"({
      "scenario_id": "tune", "name": "Tune", "source": "sam3d", "confidence": "estimate", "seed": 3,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 }, "terrain": { "type": "flat", "height_m": 0 },
      )") + (npc_commander ? R"("crew_commander": "npc", )" : "") + R"(
      "command_post": "command/pu12.json",
      "c2": { "library": "command/c2_sources.json", "primary": "pu12", "fallback_order": ["ewr_north"], "presets": ["pu12", "ewr_north"],
              "sources": [ { "id": "ewr_north", "type": "ewr_voice", "position": { "east_m": 0, "north_m": 8000, "up_m": 0 },
                             "link_range_m": 200000, "frequency_mhz": 38.55 } ] },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "radio_presets": ")" + presets + R"(",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [], "triggers": [],
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

void Steps(ScenarioRunner& r, double seconds, double dt = 0.1) {
    for (double end = r.Time() + seconds; r.Time() < end - 1e-9;) r.Step(dt);
}

bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

SC_TEST("radio setup: the grid, the sub-bands and the antenna circuit follow hand calculations") {
    SC_EXPECT_NEAR(RadioTuning::Grid(31.2627), 31.275, 1e-9);  // 25 kHz steps
    SC_EXPECT(RadioTuning::BandOf(35.725) == 0 && RadioTuning::BandOf(35.75) == 1);
    SC_EXPECT_NEAR(RadioTuning::AntennaOptimum(20.0), 0.0, 1e-12);
    SC_EXPECT_NEAR(RadioTuning::AntennaOptimum(51.5), 100.0, 1e-12);
    SC_EXPECT_NEAR(RadioTuning::AntennaOptimum(35.75), 50.0, 1e-12);
    // The meter: 1 / (1 + (error / 5)^2); 5 divisions off is half the reading, 3 dB.
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error));
    RadioTuning t(c.comms);
    t.SetPresets(0, {31.25, 38.55, 0.0, 0.0});
    t.Settle(c);
    const bool keyed[2] = {false, false};
    t.Step(0.0, 0.1, c, false, keyed);
    SC_EXPECT_NEAR(t.FrequencyMhz(0), 31.25, 1e-9);  // the left set starts on fixed frequency 1
    SC_EXPECT_NEAR(t.Reading(0), 1.0, 1e-12);
    c.panel.Set("radio.left.ant_lock", 1);
    c.panel.SetValue("radio.left.ant_tune", RadioTuning::AntennaOptimum(31.25) + 5.0);
    t.Step(0.1, 0.1, c, false, keyed);
    SC_EXPECT_NEAR(t.Reading(0), 0.5, 1e-9);
    SC_EXPECT_NEAR(t.LossDb(0), 3.0103, 1e-3);
}

SC_TEST("radio setup: selecting a fixed frequency, the motors turn the knobs to it; an unset one is off the net") {
    auto r = Load(Text("set"));
    SC_EXPECT(r != nullptr && r->RadioPresets() != nullptr);
    if (!r || r->RadioPresets() == nullptr) return;
    Steps(*r, 1.0);
    ConsoleData& c = r->Console();
    SC_EXPECT_NEAR(r->RadioPresets()->FrequencyMhz(0), 30.0, 1e-9);
    SC_EXPECT_NEAR(c.panel.GetValue("radio.left.freq"), 30.0, 1e-9);  // the knob where the fixator holds it
    SC_EXPECT(c.equipment.GetInput("r123m_left_on_frequency") && c.equipment.GetInput("r123m_right_on_frequency"));
    // The right set to fixed frequency 1 (the PU-12's net): the motors run 3 s, the set is off tune meanwhile.
    c.panel.Set("radio.right.preset", 0);
    Steps(*r, 1.0);
    SC_EXPECT(r->RadioPresets()->MotorsRunning(1));
    Steps(*r, 3.0);
    SC_EXPECT_NEAR(r->RadioPresets()->FrequencyMhz(1), 30.0, 1e-9);
    // Unset: the fixators hold nothing, so neither set is on its net and the commander hears nothing.
    auto u = Load(Text("unset"));
    SC_EXPECT(u != nullptr);
    if (!u) return;
    Steps(*u, 5.0);
    SC_EXPECT(!u->Console().equipment.GetInput("r123m_left_on_frequency"));
    std::string why;
    SC_EXPECT(!u->CommanderCanTransmit(&why));
    SC_EXPECT(Has(u->Debrief(), "cannot hear the net"));
}

SC_TEST("radio setup: the crew's set-up drill puts both sets on the plan's frequencies, and the net is heard") {
    auto r = Load(Text("unset"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const ConsoleData& c = r->Console();
    Steps(*r, 1.0);
    SC_EXPECT(r->GiveCrewOrder("order_presets"));
    Steps(*r, 600.0);
    const RadioTuning& t = *r->RadioPresets();
    for (int s = 0; s < 2; ++s) {
        SC_EXPECT_NEAR(t.PresetOf(s, 0).freq_mhz, 30.0, 1e-9);
        SC_EXPECT_NEAR(t.PresetOf(s, 1).freq_mhz, 38.55, 1e-9);
        SC_EXPECT(t.PresetOf(s, 1).subband == 1);
        SC_EXPECT(std::fabs(t.PresetOf(s, 1).antenna - RadioTuning::AntennaOptimum(38.55)) < 2.5);  // the meter's peak
    }
    SC_EXPECT(c.equipment.GetInput("r123m_left_on_frequency") && c.equipment.GetInput("r123m_right_on_frequency"));
    SC_EXPECT(c.panel.Get("radio.left.fix_cover") == 0 && c.panel.Get("radio.right.fix_cover") == 0);
    SC_EXPECT(c.panel.Get("radio.chest") == 0 && c.panel.Get("radio.chest_driver") == 0 && c.panel.Get("intercom.pv") == 0);
    const std::string d = r->Debrief();
    SC_EXPECT(Has(d, "completed  preset_setup_left") && Has(d, "completed  preset_setup_right"));
    SC_EXPECT(Has(d, "fixed frequency 2 set to 38.550 MHz, sub-band II"));
    SC_EXPECT(Has(d, "antenna tuned for fixed frequency 2"));
    SC_EXPECT(r->CommanderCanTransmit());
}

SC_TEST("radio setup: smooth tuning goes by the knobs, and on the move it walks off the net") {
    auto r = Load(Text("set"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    Steps(*r, 1.0);
    // The right set (on the EWR's preset 2) to ПЛАВНЫЙ ПОДДИАПАЗОН II with the knob on the EWR's frequency: still on it.
    c.panel.SetValue("radio.right.freq", 38.55);
    c.panel.Set("radio.right.preset", 5);
    Steps(*r, 1.0);
    SC_EXPECT(r->RadioPresets()->Smooth(1));
    SC_EXPECT(c.equipment.GetInput("r123m_right_on_frequency"));
    // Sub-band I with a 38 MHz knob: wrong sub-band.
    c.panel.Set("radio.right.preset", 4);
    Steps(*r, 0.5);
    SC_EXPECT(!c.equipment.GetInput("r123m_right_on_frequency"));
    c.panel.Set("radio.right.preset", 5);
    Steps(*r, 0.5);
    SC_EXPECT(c.equipment.GetInput("r123m_right_on_frequency"));
    // On the move the vibration walks it off (2 kHz a second: past half a 25 kHz step within about 6 s).
    c.equipment.SetInput("vehicle_stopped", false);
    Steps(*r, 10.0);
    SC_EXPECT(!c.equipment.GetInput("r123m_right_on_frequency"));
}
