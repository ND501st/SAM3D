#pragma once

// The BM's own checks (roadmap C4, docs/research/OSA_CHECKS_RECEIVERS_DRIVES_2026-11.md, "The checks' model"): what the
// function-check set's meters and the receivers' noise meter read, worked out from the console alone (so a mirrored
// console reads the same), and what the faults they find cost the radars.
//
// - 0004-12M1, the function-check set (voenizdat_osa_1991 p159-162): with its anode on in ФК, КОНТРОЛЬ СИСТЕМ picks the
//   check. On the СРП's static task, ПУСК gives the commands K1, K2 the computer works out (ИП1, ИП2 show "a certain
//   level", p159). On the passage of commands, УРОВНИ КОМАНД (or ПРОВЕРКА СМЕНЫ ЛИТЕРА) sends their standard levels,
//   СМЕНА ЗНАКА their sign; with neither, the zero levels (p159-160). On the transients, the channel the switch picks
//   settles. A fault in the commands' channel leaves both meters dead.
// - 0004-14M (p157-162): the meters КАНАЛ Ц and КАНАЛ Р show the error signals РЕЖИМЫ picks; ВЫВОД — НАВЕДЕНИЕ switches the
//   missile channel's scale, ЧАСТОТА tunes the target channel's quartz oscillator (off tune, the target meters fall).
// - 0062-6M1 (p98-99, p161): ПОДЖИГ ГШ fires the noise generator through the channel РЕЖИМЫ picks; КОНТРОЛЬ reads the
//   receiver's noise, set by the channel's РРУ КШ knob, and its rise with the generator, the less the worse the
//   receiver's noise figure (the Y-factor). A degraded receiver loses that much on every echo (ReceiverLossDb).
//
// The meters' readings are SAM3D's: the sources say what each check shows, not its numbers (confidence low).

#include <algorithm>
#include <cmath>
#include <string>

#include "simcore/console_data.hpp"

namespace simcore {

// A meter's reading this header gives (a cabin display's "reading").
inline bool KnownCheckReading(const std::string& name) {
    return name == "fk_ip1" || name == "fk_ip2" || name == "fk_target_error" || name == "fk_missile_error" || name == "noise_control";
}

namespace checks_detail {

inline bool Held(const ControlPanel& p, const std::string& id) { return !id.empty() && p.Get(id) == 1; }
inline bool InputOn(const Equipment& eq, const std::string& id) { return !id.empty() && eq.HasInput(id) && eq.GetInput(id); }
inline bool Ready(const Equipment& eq, const std::string& id) { return id.empty() || eq.IsReady(id); }

// ИП1 (k = 1) or ИП2 (k = 2), of the swing (-1..1).
inline double CommandMeter(const ConsoleData& c, int k) {
    const CommandCheckDef& d = c.checks.commands;
    const Equipment& eq = c.equipment;
    const ControlPanel& p = c.panel;
    if (!eq.IsReady(d.anode_subsystem) || InputOn(eq, d.fault_input)) return 0.0;
    const int system = p.Get(d.systems_control);
    if (system == d.static_position) {
        if (!Ready(eq, d.computer_subsystem)) return 0.0;
        if (d.launch_control.empty() || p.Get(d.launch_control) != d.launch_on_position) return 0.0;
        return k == 1 ? d.static_k1 : d.static_k2;
    }
    if (system == d.commands_position) {
        if (!Held(p, d.levels_control) && !Held(p, d.letter_control)) return 0.0;  // the zero levels
        const bool flipped = !d.sign_control.empty() && p.Get(d.sign_control) == d.sign_on_position;
        return flipped ? -d.level : d.level;
    }
    if (system == d.transients_position) {
        if (!Ready(eq, d.computer_subsystem)) return 0.0;
        const int ch = d.transients_control.empty() ? 2 : p.Get(d.transients_control);
        return (ch == k - 1 || ch >= 2) ? d.transient : 0.0;
    }
    return 0.0;  // the other systems' checks are not modelled
}

// КАНАЛ Ц (missile = false) or КАНАЛ Р, both meters of the pair alike, of the swing.
inline double ErrorMeter(const ConsoleData& c, bool missile) {
    const ErrorCheckDef& d = c.checks.errors;
    const Equipment& eq = c.equipment;
    const ControlPanel& p = c.panel;
    if (!eq.IsReady(d.check_subsystem)) return 0.0;  // in combat the tracker holds them at zero (ТСЦ)
    if (InputOn(eq, missile ? d.missile_fault_input : d.target_fault_input)) return 0.0;
    const std::size_t mode = static_cast<std::size_t>(std::max(0, p.Get(d.mode_control)));
    const std::vector<double>& by_mode = missile ? d.missile_by_mode : d.target_by_mode;
    if (mode >= by_mode.size()) return 0.0;
    double v = by_mode[mode];
    if (missile) {
        if (!d.output_control.empty() && p.Get(d.output_control) == d.gathering_position) v *= d.gathering_factor;
    } else if (!d.freq_control.empty()) {
        v *= std::max(0.0, 1.0 - std::fabs(p.GetValue(d.freq_control) - d.freq_tuned) / d.freq_band);
    }
    return std::clamp(v, -1.0, 1.0);
}

// The receiver a channel of the noise check runs through: its degradation input.
inline const std::string& DegradedInput(const NoiseCheckDef& d, const std::string& radar) {
    return radar == "search" ? d.search_degraded_input : d.track_degraded_input;
}

// КОНТРОЛЬ of 0062-6M1, of the swing (0..1).
inline double NoiseMeter(const ConsoleData& c) {
    const NoiseCheckDef& d = c.checks.noise;
    const Equipment& eq = c.equipment;
    const ControlPanel& p = c.panel;
    if (!eq.IsReady(d.ready_subsystem)) return 0.0;
    const std::size_t pos = static_cast<std::size_t>(std::max(0, p.Get(d.channel_control)));
    if (pos >= d.channels.size() || d.channels[pos].radar.empty()) return 0.0;
    const NoiseCheckChannel& ch = d.channels[pos];
    double v = d.noise_per_gain * p.GetValue(ch.gain_control);
    if (Held(p, d.fire_control)) {
        // The generator's noise over the receiver's own: a noise figure worse by degraded_db leaves less of a rise.
        const double excess = d.generator_y - 1.0;
        const double f = InputOn(eq, DegradedInput(d, ch.radar)) ? std::pow(10.0, d.degraded_db / 10.0) : 1.0;
        v *= 1.0 + excess / f;
    }
    return std::clamp(v, 0.0, 1.0);
}

}  // namespace checks_detail

// What a check's meter reads now, of its swing: -1..1 for the centre-zero meters, 0..1 for КОНТРОЛЬ.
inline double CheckReading(const std::string& name, const ConsoleData& c) {
    if (!c.has_checks) return 0.0;
    if (c.checks.has_commands && name == "fk_ip1") return checks_detail::CommandMeter(c, 1);
    if (c.checks.has_commands && name == "fk_ip2") return checks_detail::CommandMeter(c, 2);
    if (c.checks.has_errors && name == "fk_target_error") return checks_detail::ErrorMeter(c, false);
    if (c.checks.has_errors && name == "fk_missile_error") return checks_detail::ErrorMeter(c, true);
    if (c.checks.has_noise && name == "noise_control") return checks_detail::NoiseMeter(c);
    return 0.0;
}

// Whether the console has the part of the checks a reading needs.
inline bool HasCheckReading(const std::string& name, const ConsoleData& c) {
    if (!c.has_checks) return false;
    if (name == "fk_ip1" || name == "fk_ip2") return c.checks.has_commands;
    if (name == "fk_target_error" || name == "fk_missile_error") return c.checks.has_errors;
    if (name == "noise_control") return c.checks.has_noise;
    return false;
}

// What a degraded receiver costs every echo of its radar (dB): the noise check's degradation, 0 when sound.
inline double ReceiverLossDb(const ConsoleData& c, bool search) {
    if (!c.has_checks || !c.checks.has_noise) return 0.0;
    const NoiseCheckDef& d = c.checks.noise;
    return checks_detail::InputOn(c.equipment, search ? d.search_degraded_input : d.track_degraded_input) ? d.degraded_db : 0.0;
}

// The range a radar keeps with that loss, as a fraction (the radar equation: range goes as the fourth root of the
// signal to noise).
inline double ReceiverRangeFactor(double loss_db) { return std::pow(10.0, -loss_db / 40.0); }

}  // namespace simcore
