// The PU-12M's R-111 and R-407 worked as sets (simcore/radio_sets.hpp, docs/research/PU12_RADIOS_2026-10.md): what each
// set hears and sends from its controls, its warm-up, the R-111's presets and tuning and its input protection, the R-407's
// channel (АПЧ, duplex, spurious channels, interference), the meters, and the post's runner with both. Behaviour tests use
// round placeholder numbers; one test loads the real data file.
#include <cmath>
#include <string>
#include <vector>

#include "simcore/cabin.hpp"
#include "simcore/radio_sets.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

// Placeholder numbers, round for hand calculation.
R111Def PlaceholderR111() {
    R111Def d;
    d.freq_min_hz = 20.0e6;
    d.freq_max_hz = 52.0e6;
    d.subband_split_hz = 36.0e6;
    d.channel_hz = 25.0e3;
    d.power_w = 100.0;
    d.fraction_low = 0.01;
    d.fraction_mid = 0.20;
    d.preset_drive_s = 10.0;
    d.tune_s = 20.0;
    d.range_whip_m = 30000.0;
    d.range_mast_whip_m = 40000.0;
    d.range_mast_mast_m = 50000.0;
    d.separation_fraction = 0.10;
    d.bad_spacing_1_hz = 8.0e6;
    d.bad_spacing_2_hz = 16.0e6;
    return d;
}

R407Def PlaceholderR407() {
    R407Def d;
    d.freq_min_hz = 50.0e6;
    d.freq_max_hz = 60.0e6;
    d.channel_hz = 50.0e3;
    d.duplex_offset_hz = 4.0e6;
    d.apch_hold_hz = 15.0e3;
    d.narrow_hold_hz = 7.0e3;
    d.wide_hold_hz = 14.0e3;
    d.wide_range_factor = 0.5;
    d.unsettled_drift_hz = 5.0e3;
    d.range_mast_m = 10000.0;
    d.range_low_m = 2000.0;
    d.rx_spurs_hz = {53.0e6};
    d.tx_spurs_hz = {51.0e6};
    return d;
}

RadioPlan PlaceholderPlan() {
    RadioPlan p;
    p.present = true;
    p.command_net_hz = 30.0e6;
    p.presets_hz = {{30.0e6, 40.0e6, 25.0e6, 0.0}};
    p.telecode_hz = 56.0e6;
    p.telecode_return_hz = 52.0e6;
    return p;
}

// A warm R-111 on duty reception, telephony, the scale settled.
R111Inputs Warm(int mode = kR111Duty) {
    R111Inputs in;
    in.mode = mode;
    in.work = kR111Phone;
    in.supply = true;
    in.settled = true;
    in.cathodes = mode >= kR111Power1;
    return in;
}

// Steps the set from t0 to t1 with the same panel (the buttons released after the first step).
// The buttons are let go after the first step, as the panel does.
double Run(R111& set, R111Inputs& in, const RadioPlan& plan, double t0, double t1, const std::vector<double>& cosite = {}) {
    for (double t = t0; t <= t1 + 1e-9; t += 0.5) {
        set.Update(t, in, plan, cosite);
        in.zpch_pressed = -1;
        in.um_pressed = in.sau_pressed = in.call_pressed = false;
    }
    return t1;
}

R407Inputs WarmR407() {
    R407Inputs in;
    in.power = true;
    in.supply = true;
    in.warm = true;
    in.settled = true;
    in.mode = kR407Duplex;
    in.apch = true;
    in.lines = kR407FourWireTv;
    in.rx_mhz = 56.0;
    in.tx_mhz = 52.0;
    in.mast_up = true;
    return in;
}

}  // namespace

SC_TEST("radio sets: the data file loads, every value cited") {
    RadioSetsDef d;
    std::string error;
    SC_EXPECT(LoadRadioSetsFile(kData + "/systems/pu12m_radios.json", d, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT_NEAR(d.r111.freq_min_hz, 20.0e6, 1.0);        // r111_to p. 6
    SC_EXPECT_NEAR(d.r111.freq_max_hz, 52.0e6, 1.0);
    SC_EXPECT_NEAR(d.r111.channel_hz, 25.0e3, 1e-6);
    SC_EXPECT_NEAR(d.r111.preset_drive_s + d.r111.tune_s, 45.0, 1e-9);  // the whole retune at most 45 s
    SC_EXPECT_NEAR(d.r111.range_mast_mast_m, 60000.0, 1e-9);
    SC_EXPECT_NEAR(d.r407.duplex_offset_hz, 4.0e6, 1.0);    // r407_to, r407_plate
    SC_EXPECT(d.r407.rx_spurs_hz.size() == 3 && d.r407.tx_spurs_hz.size() == 3);
    SC_EXPECT_NEAR(d.r407.range_mast_m, 15000.0, 1e-9);     // flak11
}

SC_TEST("radio sets: the R-111 hears its net on duty reception, settled, on the net's frequency, within its antennas' reach") {
    R111 set(PlaceholderR111());
    const RadioPlan plan = PlaceholderPlan();
    R111Inputs in = Warm();
    in.zpch_pressed = 1;
    Run(set, in, plan, 0.0, 12.0);                     // the mechanism turns 10 s
    in.zpch_pressed = -1;
    SC_EXPECT(set.Preset() == 1 && std::fabs(set.FrequencyHz() - 30.0e6) < 1.0);
    SC_EXPECT(set.Hears(in, plan.command_net_hz, 29000.0, false));      // whips both ends: 30 km
    SC_EXPECT(!set.Hears(in, plan.command_net_hz, 31000.0, false));
    SC_EXPECT(set.Hears(in, plan.command_net_hz, 39000.0, true));       // the regiment on a mast: 40 km
    in.antenna_mast = true;
    SC_EXPECT(set.Hears(in, plan.command_net_hz, 39000.0, true));       // the post's distributor on ТЕЛЕСКОП, the mast down: still 40
    in.mast_up = true;
    SC_EXPECT(set.Hears(in, plan.command_net_hz, 49000.0, true));       // both on masts: 50 km
    // what keeps it off the net
    R111Inputs off = in;
    off.settled = false;
    SC_EXPECT(!set.Hears(off, plan.command_net_hz, 1000.0, true));      // the scale not yet settled
    off = in;
    off.work = kR111Tone;
    SC_EXPECT(!set.Hears(off, plan.command_net_hz, 1000.0, true));      // 800 ГЦ: the level check, not the net
    off = in;
    off.tlk = true;
    SC_EXPECT(!set.Hears(off, plan.command_net_hz, 1000.0, true));      // ТЛК: the channel to the telecode equipment
    SC_EXPECT(!set.Hears(in, 40.0e6, 1000.0, true));                    // another net
    SC_EXPECT(set.Hears(in, 30.0e6 + 12.0e3, 1000.0, true));            // within half a channel
    SC_EXPECT(!set.Hears(in, 30.0e6 + 13.0e3, 1000.0, true));
}

SC_TEST("radio sets: a preset button turns the R-111 and it tunes itself, all within the drive and the tuning; then it transmits") {
    R111 set(PlaceholderR111());
    const RadioPlan plan = PlaceholderPlan();
    R111Inputs in = Warm(kR111Power20);
    in.zpch_pressed = 1;
    set.Update(0.0, in, plan, {});
    in.zpch_pressed = -1;
    SC_EXPECT(set.Driving() && set.FrequencyHz() == 0.0);               // between frequencies while it turns
    Run(set, in, plan, 0.5, 9.5);
    SC_EXPECT(set.Driving());
    Run(set, in, plan, 10.0, 10.0);
    SC_EXPECT(!set.Driving() && set.Tuning(10.0) && set.TuningUm(10.0));  // the amplifier first
    SC_EXPECT(std::fabs(set.DialToSet() - 46.0) < 1e-9);                 // 30 MHz is sub-band I: the scale reads 46
    Run(set, in, plan, 10.5, 20.5);
    SC_EXPECT(set.TuningSau(20.5));                                        // then the antenna tuner
    SC_EXPECT(!set.CanTransmit(in, 20.5));
    Run(set, in, plan, 21.0, 30.0);
    SC_EXPECT(!set.Tuning(30.0) && set.UmTuned() && set.SauTuned(false));
    SC_EXPECT(set.CanTransmit(in, 30.0));
    // its reach goes with the fourth root of the power: 20 % of whips' 30 km
    SC_EXPECT(set.Reaches(in, 30.0, plan.command_net_hz, 30000.0 * std::pow(0.2, 0.25) - 10.0, false));
    SC_EXPECT(!set.Reaches(in, 30.0, plan.command_net_hz, 30000.0 * std::pow(0.2, 0.25) + 10.0, false));
    in.mode = kR111Power1;
    SC_EXPECT(set.Reaches(in, 30.0, plan.command_net_hz, 9000.0, false));   // 1 %: 30 km x 0.316
    SC_EXPECT(!set.Reaches(in, 30.0, plan.command_net_hz, 10000.0, false));
    in.mode = kR111Duty;
    SC_EXPECT(!set.CanTransmit(in, 30.0));                                   // duty reception receives only
    // a preset never prepared lands nowhere
    in.mode = kR111Power20;
    in.zpch_pressed = 4;
    Run(set, in, plan, 31.0, 50.0);
    SC_EXPECT(set.Preset() == 4 && set.FrequencyHz() == 0.0 && !set.Hears(in, plan.command_net_hz, 1.0, false));
}

SC_TEST("radio sets: by hand the R-111 tunes only at 20 % with warm cathodes; a new frequency or antenna wants tuning again") {
    R111 set(PlaceholderR111());
    const RadioPlan plan = PlaceholderPlan();
    R111Inputs in = Warm(kR111Power20);
    in.cathodes = false;
    in.subband = kR111SubbandII;
    in.dial_mhz = 40.0;
    in.um_pressed = true;
    Run(set, in, plan, 0.0, 1.0);
    SC_EXPECT(!set.Tuning(1.0) && !set.UmTuned());                  // cold cathodes: the button does nothing (r111_to p. 4)
    in.cathodes = true;
    in.mode = kR111Power100;
    in.um_pressed = true;
    Run(set, in, plan, 2.0, 3.0);
    SC_EXPECT(!set.Tuning(3.0));                                     // tuned at 20 % only (r111_to p. 66)
    in.mode = kR111Power20;
    in.um_pressed = true;
    Run(set, in, plan, 4.0, 30.0);
    SC_EXPECT(set.UmTuned() && set.SauTuned(false) && std::fabs(set.FrequencyHz() - 40.0e6) < 1.0);
    in.dial_mhz = 40.1;                                             // the scale moved 100 kHz
    Run(set, in, plan, 30.5, 31.0);
    SC_EXPECT(!set.UmTuned() && !set.CanTransmit(in, 31.0));
    in.dial_mhz = 40.0;
    Run(set, in, plan, 31.5, 32.0);
    SC_EXPECT(set.UmTuned());                                        // back where it was tuned
    in.antenna_mast = true;                                          // the distributor to ТЕЛЕСКОП
    SC_EXPECT(!set.SauTuned(true) && !set.CanTransmit(in, 32.0));
    in.sau_pressed = true;
    Run(set, in, plan, 32.5, 50.0);                                  // the tuner alone: half the tuning
    SC_EXPECT(set.SauTuned(true) && set.CanTransmit(in, 50.0));
    in.subband = kR111Calib250;                                      // the calibrator: off any net
    Run(set, in, plan, 50.5, 51.0);
    SC_EXPECT(set.FrequencyHz() == 0.0);
}

SC_TEST("radio sets: a set beside the R-111 within 10 %, or 8 or 16 MHz off, trips its input protection until it is reset") {
    R111 set(PlaceholderR111());
    RadioPlan plan = PlaceholderPlan();
    R111Inputs in = Warm(kR111Power20);
    in.subband = kR111SubbandII;
    in.dial_mhz = 40.0;
    in.um_pressed = true;
    Run(set, in, plan, 0.0, 25.0);                                   // on 40 MHz, tuned
    SC_EXPECT(!set.TooClose(44.5e6) && set.TooClose(43.9e6));        // 10 % of 40 MHz
    SC_EXPECT(set.TooClose(32.0e6) && set.TooClose(24.0e6) && !set.TooClose(33.0e6));
    Run(set, in, plan, 25.5, 26.0, {45.0e6});
    SC_EXPECT(!set.Protected());
    Run(set, in, plan, 26.5, 27.0, {42.0e6});
    SC_EXPECT(set.Protected() && !set.CanReceive(in));               // ЗАЩИТА ВХОДА lit: deaf
    Run(set, in, plan, 27.5, 30.0);
    SC_EXPECT(set.Protected());                                       // stays until reset
    in.call_pressed = true;                                           // on 20 %, a short transmission
    Run(set, in, plan, 30.5, 31.0);
    SC_EXPECT(!set.Protected() && set.CanReceive(in));
    Run(set, in, plan, 31.5, 32.0, {48.0e6});                         // 8 MHz off
    SC_EXPECT(set.Protected());
    R111Inputs off = in;
    off.mode = kR111Off;
    Run(set, off, plan, 32.5, 33.0);
    Run(set, in, plan, 33.5, 34.0);
    SC_EXPECT(!set.Protected());                                      // switched off and on
}

SC_TEST("radio sets: the R-407 takes the regiment's channel within its АПЧ's pull, on its mast's reach, its duplex pair clean") {
    R407 set(PlaceholderR407());
    R407Inputs in = WarmR407();
    const double tx = 56.0e6;
    SC_EXPECT(set.Link(in, tx, 9900.0, false, false) && !set.Link(in, tx, 10100.0, false, false));
    in.mast_up = false;
    SC_EXPECT(set.Link(in, tx, 1900.0, false, false) && !set.Link(in, tx, 2100.0, false, false));
    in.mast_up = true;
    in.rx_mhz = 56.014;                                              // 14 kHz off: the АПЧ pulls it in
    SC_EXPECT(set.Link(in, tx, 1000.0, false, false));
    in.rx_mhz = 56.016;
    SC_EXPECT(!set.Link(in, tx, 1000.0, false, false));
    in.apch = false;                                                  // without АПЧ the narrow band holds 7 kHz
    in.rx_mhz = 56.006;
    SC_EXPECT(set.Link(in, tx, 1000.0, false, false));
    in.settled = false;                                               // and the scale not settled drifts 5 kHz more
    SC_EXPECT(!set.Link(in, tx, 1000.0, false, false));
    in.wide = true;                                                   // the wide band holds 14, at half the reach here
    SC_EXPECT(set.Link(in, tx, 4900.0, false, false) && !set.Link(in, tx, 5100.0, false, false));
    in = WarmR407();
    in.tx_mhz = 52.5;                                                 // duplex not 4 MHz apart: its own transmitter deafens it
    SC_EXPECT(!set.Link(in, tx, 1000.0, false, false));
    in.mode = kR407Duty;                                              // duty reception: the transmitter silent
    SC_EXPECT(set.Link(in, tx, 1000.0, false, false));
    in = WarmR407();
    in.rx_mhz = 53.0;
    in.tx_mhz = 49.0;
    SC_EXPECT(!set.Link(in, 53.0e6, 1000.0, false, false));          // the receiver's spurious channel
    in = WarmR407();
    in.rx_mhz = 55.0;
    in.tx_mhz = 51.0;
    SC_EXPECT(!set.Link(in, 55.0e6, 1000.0, false, false));          // the transmitter's, in duplex
    in.mode = kR407Duty;
    SC_EXPECT(set.Link(in, 55.0e6, 1000.0, false, false));            // only in duplex
    in = WarmR407();
    in.mode = kR407Phone;
    SC_EXPECT(!set.Link(in, tx, 1000.0, false, false));               // ТА: a telephone set
    in = WarmR407();
    in.warm = false;
    SC_EXPECT(!set.Link(in, tx, 1000.0, false, false));
    in = WarmR407();
    SC_EXPECT(!set.Link(in, tx, 1000.0, true, false));                // the regiment off the air
    SC_EXPECT(!set.Link(in, tx, 1000.0, false, true));                // interference: the АПЧ chases the noise
    in.apch = false;
    SC_EXPECT(set.Link(in, tx, 1000.0, false, true));                 // АПЧ off rides it out (r407_plate II.9)
    in = WarmR407();
    SC_EXPECT(R407::ToEquipment(in));
    in.lines = kR407TwoWireEnd;
    SC_EXPECT(!R407::ToEquipment(in));                                // the channel ends in the handset
}

SC_TEST("radio sets: the meters read the set from the console alone") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/pu12m.json", c));
    c.equipment.Update(0.0, c.panel);
    SC_EXPECT(c.equipment.Warm("net_27v", c.panel));  // the 27 V network on (its master switch starts on)
    SC_EXPECT(c.r111.present && c.r407.present);
    SC_EXPECT(MissingRadioRole(c.r111, true).empty() && MissingRadioRole(c.r407, false).empty());
    SC_EXPECT_NEAR(RadioMeterReading("r111_indicator", c), 0.0, 1e-9);    // ПР. УР. (where it was left) with the set off
    c.panel.Set("radio.fug3.indicator", kR111IndSupply);
    SC_EXPECT_NEAR(RadioMeterReading("r111_indicator", c), 0.65, 1e-9);   // in its sector
    c.panel.Set("radio.fug3.indicator", kR111IndRx);
    SC_EXPECT_NEAR(RadioMeterReading("r111_indicator", c), 0.0, 1e-9);    // the set off: nothing received
    c.panel.Set("radio.fug3.mode", kR111Duty);
    SC_EXPECT_NEAR(RadioMeterReading("r111_indicator", c), 0.08, 1e-9);   // on: the noise
    c.equipment.SetInput("r111_hear", true);
    SC_EXPECT(RadioMeterReading("r111_indicator", c) > 0.3);              // the net heard
    SC_EXPECT_NEAR(RadioMeterReading("r407_meter", c), 0.0, 1e-9);        // the R-407 off
    c.panel.Set("radio.fug5.power", 1);
    SC_EXPECT_NEAR(RadioMeterReading("r407_meter", c), 0.65, 1e-9);       // НАКАЛ: the heaters on the mark
    CabinDisplay d;
    d.reading = "r407_meter";
    d.full_scale = 5.0;
    SC_EXPECT_NEAR(MeterValue(d, c), 3.25, 1e-9);
}

SC_TEST("radio sets: in the post, the regiment's orders come over the R-111 once it is tuned on the net; the telecode over the R-407") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_deploy.json", kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(def.has_radio_sets && def.radio_plan.present && def.radio_plan.messages.size() == 2);
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.GetR111() != nullptr && r.GetR407() != nullptr);
    double net_s = -1.0, telecode_s = -1.0;
    while (!r.Finished() && r.Time() < 1000.0) {   // the scenario ends itself once the post is deployed
        r.Step(0.1);
        if (net_s < 0.0 && r.Console().equipment.GetInput("r111_net")) net_s = r.Time();
        if (telecode_s < 0.0 && r.Console().equipment.GetInput("regiment_telecode")) telecode_s = r.Time();
    }
    std::printf("    the regiment's net both ways at %.0f s, the telecode at %.0f s\n", net_s, telecode_s);
    SC_EXPECT(net_s > 0.0 && net_s < 900.0);     // deployed and tuned before the regiment's first message
    SC_EXPECT(telecode_s > 0.0);
    bool heard = false;
    for (const auto& m : r.RadioLog()) heard = heard || (m.net == "regiment net" && m.text.find("readiness one") != std::string::npos);
    SC_EXPECT(heard);
    // the R-407's channel on the handset's terminals: the ASPD-U gets nothing
    r.Console().panel.Set("radio.fug5.lines", kR407TwoWireEnd);
    r.Step(0.1);
    SC_EXPECT(!r.Console().equipment.GetInput("regiment_telecode") && r.Console().equipment.GetInput("r407_link"));
}

SC_TEST("radio sets: drums not set in the park keep the telecode out until someone sets them; interference wants АПЧ off") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_deploy.json", kData, def, console, &error));
    def.radio_plan.prepared = false;
    C2Outage noise;
    noise.from_s = 950.0;
    noise.to_s = 1150.0;
    noise.interference = true;
    noise.reason = "test: noise on the regiment's telecode net";
    def.post_regiment.outages.push_back(noise);
    ScenarioRunner r(std::move(def), std::move(console));
    while (!r.Finished() && r.Time() < 900.0) r.Step(0.1);
    ConsoleData& c = r.Console();
    SC_EXPECT(!c.equipment.GetInput("r407_link") && !c.equipment.GetInput("regiment_telecode"));   // the drum left on 54.4
    SC_EXPECT(!c.panel.SetValue("radio.fug5.rx_freq", 56.0));         // locked
    c.panel.Set("radio.fug5.rx_lock", 1);
    SC_EXPECT(c.panel.SetValue("radio.fug5.rx_freq", 56.0));
    c.panel.Set("radio.fug5.tx_lock", 1);
    SC_EXPECT(c.panel.SetValue("radio.fug5.tx_freq", 52.0));
    c.panel.Set("radio.fug5.rx_lock", 0);
    c.panel.Set("radio.fug5.tx_lock", 0);
    r.Step(0.1);
    SC_EXPECT(c.equipment.GetInput("r407_link") && c.equipment.GetInput("regiment_telecode"));
    while (!r.Finished() && r.Time() < 1000.0) r.Step(0.1);
    SC_EXPECT(!c.equipment.GetInput("r407_link"));                    // the АПЧ chases the noise
    c.panel.Set("radio.fug5.apch", 0);
    r.Step(0.1);
    SC_EXPECT(c.equipment.GetInput("r407_link"));
}

SC_TEST("radio sets: a scenario's plan is checked: presets in the R-111's band, an outage's kind") {
    const std::string base = R"JSON({"scenario_id": "t", "name": "t", "source": "sam3d (a test)", "confidence": "estimate", "seed": 1, "origin": {"lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110},
      "terrain": {"type": "flat", "height_m": 110},
      "post": {"board": "command/pu12.json", "aspd": "command/aspd_u.json",
               "regiment": {"position": {"east_m": -6000, "north_m": -5000, "up_m": 2}, "command_net_mhz": 31.5,
                            "outages": [{"from_s": 10, "to_s": 20, "kind": "KIND"}]},
               "radios": {"r111_presets_mhz": [PRESET]}},
      "own_unit": {"console": "procedures/pu12m.json", "position": {"east_m": 0, "north_m": 0, "up_m": 2}},
      "crew": [], "targets": [], "triggers": [], "end": {"time_limit_s": 60}})JSON";
    auto load = [&](const std::string& kind, const std::string& preset, std::string& error) {
        std::string text = base;
        text.replace(text.find("KIND"), 4, kind);
        text.replace(text.find("PRESET"), 6, preset);
        ScenarioDef def;
        ConsoleData console;
        return LoadScenario(text, kData, def, console, &error);
    };
    std::string error;
    SC_EXPECT(load("interference", "31.5", error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(!load("noise", "31.5", error) && error.find("kind") != std::string::npos);
    SC_EXPECT(!load("off_air", "55.0", error) && error.find("band") != std::string::npos);
}
