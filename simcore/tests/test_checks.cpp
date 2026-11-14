// The BM's own checks (roadmap C4, simcore/checks.hpp): the function-check set's meters ИП1 and ИП2 (0004-12M1), the
// error-signal meters of 0004-14M with ВЫВОД — НАВЕДЕНИЕ and ЧАСТОТА, the noise check on 0062-6M1 and what a degraded
// receiver costs, and the ЭМУ's thermal relays (РЕЛЕ ТЕПЛ.).
#include <cmath>
#include <string>

#include "simcore/cabin.hpp"
#include "simcore/checks.hpp"
#include "simcore/console_data.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool LoadOsa(ConsoleData& c) {
    std::string error;
    const bool ok = LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error);
    if (!ok) std::printf("    load: %s\n", error.c_str());
    return ok;
}

// Steps the equipment for `s` seconds.
void Run(ConsoleData& c, double s) {
    for (double t = 0.0; t < s; t += 0.5) c.equipment.Update(0.5, c.panel);
}

// The equipment on and warm, the radars' high voltage ready, in ФК with the check set's anode on.
void IntoFk(ConsoleData& c) {
    c.equipment.SetInput("vehicle_stopped", true);
    c.equipment.SetInput("external_power", true);
    c.panel.Set("pwr.equipment", 1);
    Run(c, 200.0);
    c.panel.Set("fo.mode", 3);
    Run(c, 200.0);
    c.panel.Set("fk.anode", 1);
    Run(c, 1.0);
    c.panel.Set("fk.anode", 0);
}

}  // namespace

SC_TEST("checks: the console names the meters' controls and faults, and the cabin reads them") {
    ConsoleData c;
    SC_EXPECT(LoadOsa(c));
    SC_EXPECT(c.has_checks && c.checks.has_commands && c.checks.has_errors && c.checks.has_noise);
    SC_EXPECT(c.checks.commands.systems_control == "fk.systems" && c.checks.commands.fault_input == "command_channel_fault");
    SC_EXPECT(c.checks.errors.output_control == "err.output" && c.checks.errors.freq_control == "err.freq");
    SC_EXPECT(c.checks.noise.fire_control == "ssc.noise_fire" && c.checks.noise.channels.size() == 3);
    CabinLayout cab;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", c, cab, &error));
    int found = 0;
    for (const CabinDisplay& d : cab.displays) {
        if (d.reading == "fk_ip1" || d.reading == "fk_ip2" || d.reading == "noise_control") ++found;
        if (d.id.rfind("err_", 0) == 0) SC_EXPECT(d.reading == "fk_target_error" || d.reading == "fk_missile_error");
    }
    SC_EXPECT(found == 3);
    // A console without checks has none of their meters.
    const ConsoleData bare;
    SC_EXPECT(!HasCheckReading("fk_ip1", bare) && CheckReading("fk_ip1", bare) == 0.0);
    SC_EXPECT(KnownCheckReading("noise_control") && !KnownCheckReading("r407_meter"));
}

SC_TEST("checks: ИП1 and ИП2 read the static task, the commands' levels and sign, and the transients; a fault leaves them dead") {
    ConsoleData c;
    SC_EXPECT(LoadOsa(c));
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.0, 1e-12);  // no anode
    IntoFk(c);
    SC_EXPECT(c.equipment.IsReady("fk_anode"));
    // The static task: ИСХ. ПОЛОЖ. zero, ПУСК the computer's commands.
    c.panel.Set("fk.systems", 0);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.0, 1e-12);
    c.panel.Set("fk.launch", 1);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.6, 1e-12);
    SC_EXPECT_NEAR(CheckReading("fk_ip2", c), -0.4, 1e-12);
    c.panel.Set("fk.launch", 0);
    // The passage of commands: the zero levels, then УРОВНИ КОМАНД's standard levels, СМЕНА ЗНАКА's sign.
    c.panel.Set("fk.systems", 1);
    SC_EXPECT_NEAR(CheckReading("fk_ip2", c), 0.0, 1e-12);
    c.panel.Set("fk.levels", 1);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.8, 1e-12);
    SC_EXPECT_NEAR(CheckReading("fk_ip2", c), 0.8, 1e-12);
    c.panel.Set("fk.sign", 1);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), -0.8, 1e-12);
    c.panel.Set("fk.levels", 0);
    c.panel.Set("fk.letter", 1);  // the letter change passes the levels too
    SC_EXPECT_NEAR(CheckReading("fk_ip2", c), -0.8, 1e-12);
    c.equipment.SetInput("command_channel_fault", true);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.0, 1e-12);
    SC_EXPECT_NEAR(CheckReading("fk_ip2", c), 0.0, 1e-12);
    c.equipment.SetInput("command_channel_fault", false);
    c.panel.Set("fk.letter", 0);
    // The transients: the channel the switch picks settles.
    c.panel.Set("fk.systems", 2);
    c.panel.Set("fk.transients", 0);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.5, 1e-12);
    SC_EXPECT_NEAR(CheckReading("fk_ip2", c), 0.0, 1e-12);
    c.panel.Set("fk.transients", 1);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.0, 1e-12);
    SC_EXPECT_NEAR(CheckReading("fk_ip2", c), 0.5, 1e-12);
    c.panel.Set("fk.transients", 2);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c) + CheckReading("fk_ip2", c), 1.0, 1e-12);
    c.panel.Set("fk.systems", 3);  // a system not modelled
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.0, 1e-12);
    // ОТКАЗ СРП: the computer down, the static task works nothing out.
    c.panel.Set("fk.systems", 0);
    c.panel.Set("fk.launch", 1);
    c.equipment.SetInput("srp_fault", true);
    Run(c, 1.0);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.0, 1e-12);
    c.equipment.SetInput("srp_fault", false);
    // Out of ФК the set has no anode.
    c.panel.Set("fo.mode", 1);
    Run(c, 1.0);
    c.panel.Set("fk.systems", 2);
    SC_EXPECT_NEAR(CheckReading("fk_ip1", c), 0.0, 1e-12);
}

SC_TEST("checks: 0004-14M's meters follow РЕЖИМЫ, ВЫВОД — НАВЕДЕНИЕ sets the missile's scale, ЧАСТОТА off tune drops the target's") {
    ConsoleData c;
    SC_EXPECT(LoadOsa(c));
    IntoFk(c);
    c.panel.Set("err.mode", 0);
    SC_EXPECT_NEAR(CheckReading("fk_target_error", c), 0.5, 1e-12);
    c.panel.Set("err.output", 1);  // НАВЕДЕНИЕ
    SC_EXPECT_NEAR(CheckReading("fk_missile_error", c), 0.5, 1e-12);
    c.panel.Set("err.output", 0);  // ВЫВОД: the gathering beam's wider scale
    SC_EXPECT_NEAR(CheckReading("fk_missile_error", c), 0.8, 1e-12);
    c.panel.Set("err.mode", 1);
    SC_EXPECT_NEAR(CheckReading("fk_missile_error", c), -0.8, 1e-12);
    c.panel.Set("err.mode", 3);  // ПП ССЦ: the target pair only
    SC_EXPECT_NEAR(CheckReading("fk_target_error", c), 0.3, 1e-12);
    SC_EXPECT_NEAR(CheckReading("fk_missile_error", c), 0.0, 1e-12);
    c.panel.Set("err.mode", 0);
    c.panel.SetValue("err.freq", 0.6);  // half the band off tune
    SC_EXPECT_NEAR(CheckReading("fk_target_error", c), 0.25, 1e-9);
    c.panel.SetValue("err.freq", 0.9);
    SC_EXPECT_NEAR(CheckReading("fk_target_error", c), 0.0, 1e-12);
    c.panel.SetValue("err.freq", 0.5);
    c.equipment.SetInput("missile_error_fault", true);
    SC_EXPECT_NEAR(CheckReading("fk_missile_error", c), 0.0, 1e-12);
    SC_EXPECT_NEAR(CheckReading("fk_target_error", c), 0.5, 1e-12);
    // The cabin's meter shows it in its units (centre-zero, 10 a side).
    CabinLayout cab;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", c, cab, &error));
    for (const CabinDisplay& d : cab.displays) {
        if (d.id == "err_target_eps") SC_EXPECT_NEAR(MeterValue(d, c), 5.0, 1e-9);
    }
}

SC_TEST("checks: the noise check doubles a sound receiver's noise, barely lifts a degraded one's; that receiver sees less far") {
    ConsoleData c;
    SC_EXPECT(LoadOsa(c));
    SC_EXPECT_NEAR(CheckReading("noise_control", c), 0.0, 1e-12);  // the input stages not powered
    IntoFk(c);
    SC_EXPECT(c.equipment.IsReady("hv_ready"));
    for (int ch = 0; ch < 3; ++ch) {
        c.panel.Set("ssc.modes", ch);
        SC_EXPECT_NEAR(CheckReading("noise_control", c), 0.3, 1e-12);  // the knob at mid-scale: the receiver's own noise
        c.panel.Set("ssc.noise_fire", 1);
        SC_EXPECT_NEAR(CheckReading("noise_control", c), 0.6, 1e-12);
        c.panel.Set("ssc.noise_fire", 0);
    }
    c.panel.Set("ssc.modes", 3);  // a position not modelled
    SC_EXPECT_NEAR(CheckReading("noise_control", c), 0.0, 1e-12);
    // The second tracking channel is set by РРУ КШ ДОП.
    c.panel.Set("ssc.modes", 2);
    c.panel.SetValue("ssc.control_sel", 1.0);
    SC_EXPECT_NEAR(CheckReading("noise_control", c), 0.6, 1e-12);
    c.panel.SetValue("ssc.control_sel", 0.5);
    // A degraded tracking receiver: 1 + 1 / 10^0.6 of a rise.
    c.equipment.SetInput("ssc_receiver_degraded", true);
    c.panel.Set("ssc.noise_fire", 1);
    SC_EXPECT_NEAR(CheckReading("noise_control", c), 0.3 * (1.0 + 1.0 / std::pow(10.0, 0.6)), 1e-9);
    c.panel.Set("ssc.modes", 0);  // the search receiver is sound
    SC_EXPECT_NEAR(CheckReading("noise_control", c), 0.6, 1e-12);
    // What it costs: 6 dB on the tracking radar's echoes, its reach 0.71 of its range.
    SC_EXPECT_NEAR(ReceiverLossDb(c, false), 6.0, 1e-12);
    SC_EXPECT_NEAR(ReceiverLossDb(c, true), 0.0, 1e-12);
    SC_EXPECT_NEAR(ReceiverRangeFactor(6.0), std::pow(10.0, -0.15), 1e-12);
    SC_EXPECT(ReceiverRangeFactor(6.0) > 0.70 && ReceiverRangeFactor(6.0) < 0.71);
}

SC_TEST("checks: РЕЛЕ ТЕПЛ. in, an overloaded ЭМУ's relays stop the turret until the drive is reset; shunted, it turns on until the motor burns") {
    ConsoleData c;
    SC_EXPECT(LoadOsa(c));
    c.equipment.SetInput("vehicle_stopped", true);
    c.equipment.SetInput("external_power", true);
    c.panel.Set("pwr.equipment", 1);
    c.panel.Set("drv.az", 1);
    c.panel.Set("drv.el", 1);
    c.panel.Set("apu.lock", 1);
    Run(c, 120.0);
    const bool turns = c.equipment.IsReady("azimuth_drive");
    SC_EXPECT(turns);
    if (!turns) {
        std::printf("    antenna_drives %d apu_free %d\n", static_cast<int>(c.equipment.State("antenna_drives")),
                    static_cast<int>(c.equipment.State("apu_free")));
        return;
    }
    SC_EXPECT(c.panel.Get("pwr.relay_heat") == 1);  // starts with the relays in
    c.equipment.SetInput("emu_overload", true);
    Run(c, 10.0);
    SC_EXPECT(c.equipment.IsReady("azimuth_drive"));  // within the relay's delay
    Run(c, 15.0);
    SC_EXPECT(c.equipment.IsReady("emu_relay_open") && !c.equipment.IsReady("azimuth_drive"));
    // ПРИВОД βн off and on resets them: the turret turns another delay, then they open again.
    c.panel.Set("drv.az", 0);
    Run(c, 1.0);
    c.panel.Set("drv.az", 1);
    Run(c, 5.0);
    SC_EXPECT(c.equipment.IsReady("azimuth_drive"));
    Run(c, 25.0);
    SC_EXPECT(!c.equipment.IsReady("azimuth_drive"));
    // Shunted: it turns at once and on through the overload, until the motor burns out.
    c.panel.Set("pwr.relay_heat", 0);
    Run(c, 1.0);
    SC_EXPECT(c.equipment.IsReady("azimuth_drive"));
    Run(c, 250.0);
    SC_EXPECT(c.equipment.IsReady("azimuth_drive"));
    Run(c, 60.0);
    SC_EXPECT(c.equipment.IsReady("emu_burnt") && !c.equipment.IsReady("azimuth_drive"));
    c.panel.Set("pwr.relay_heat", 1);  // too late: burnt is burnt
    Run(c, 30.0);
    SC_EXPECT(!c.equipment.IsReady("azimuth_drive"));
    c.equipment.SetInput("emu_overload", false);  // the instructor clears the fault: repaired
    Run(c, 2.0);
    SC_EXPECT(!c.equipment.IsReady("emu_burnt") && c.equipment.IsReady("emu_relay_open"));  // the relays opened meanwhile
    c.panel.Set("drv.az", 0);
    Run(c, 1.0);
    c.panel.Set("drv.az", 1);
    Run(c, 5.0);
    SC_EXPECT(c.equipment.IsReady("azimuth_drive"));
}
