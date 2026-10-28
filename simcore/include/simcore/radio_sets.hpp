// simcore/radio_sets.hpp — the PU-12M's R-111 and R-407 worked as sets (docs/research/PU12_RADIOS_2026-10.md).
//
// The R-111 (FuG-3) is the post's set on the regiment's command net; the R-407 (FuG-5) brings the regiment's telecode
// to the ASPD-U. Both are modelled from their own documents (r111_to, r407_to, r407_plate): the crew works the controls
// on the front and the set does what the set does.
//
// The R-111:
//   * РЕЖ. РАБОТЫ: ОТКЛ., ДЕЖ. ПР. (receive only), МОЩНОСТЬ 1 %, 20 %, 100 %. The scale holds to ±6 kHz 5 min after
//     switching on (the console's `settled` subsystem); the output valves' cathodes want 1.5-3 min before the
//     transmitter is tuned or used (`cathodes`).
//   * The frequency: the four presets (ЗПЧ) or ПЛАВНО on the scale (the ДИАПАЗ. switch picks the sub-band, or the quartz
//     calibrator). A preset button sets the mechanism turning; at its stop the set is on the preset's frequency and,
//     with a transmitting mode and warm cathodes, tunes its power amplifier and its antenna tuner itself: at most 45 s
//     in all (r111_to p. 6). The scale's knob turns with the mechanism.
//   * It transmits only tuned for the frequency it is on and for the antenna it feeds (НАСТРОЙКА УМ at 20 %, or the
//     tuner alone with САУ), at the power the switch gives; ВИД РАБОТЫ ТЛФ or ДИСТ. УПР. (ПРД in hand relay), ТЛК off.
//   * It hears on ТЛФ, ДИСТ. УПР., ПРМ, ОЖИД. and АВТОМ. РЕТР., on the net's frequency to within half a channel.
//   * ЗАЩИТА ВХОДА: one of the post's own transmitters within 10 % of its frequency, or 8 or 16 MHz from it, trips the
//     receiver's input protection; it stays tripped until the set is switched off (or, on 20 or 100 %, transmits
//     briefly: ВЫЗОВ).
//   * Reach: the ТО's table by which antenna each end has up (whips 35 km, a mast and a whip 45, two masts 60), scaled
//     by the fourth root of the transmitting power's fraction (plane-earth propagation: the received field goes as
//     sqrt(P) h1 h2 / d^2).
//
// The R-407:
//   * ПИТАНИЕ; РОД РАБОТЫ ДУПЛЕКС, ДЕЖ. ПРИЕМ – СИМПЛЕКС or ТА (a telephone set: no radio); the receiver's and the
//     transmitter's drums (52-60 MHz) set by their knobs, fixed by the СТОПОР levers.
//   * It takes the regiment's channel when its receiver's drum is close enough: with АПЧ on, a detuning of up to 15 kHz
//     (r407_to); without, the narrow band holds 7 kHz (its 28 kHz pass band less the 7 kHz deviation, both sides), the
//     wide one twice that at a quarter of the noise margin lost. Before 15 min warm the scale may be off by up to 5 kHz
//     more. In duplex the transmitter must be 4 MHz from the receiver, and neither on its spurious channels (the plate).
//   * Interference (a regiment outage of kind "interference"): АПЧ locks on to the noise; with АПЧ off the link holds.
//   * The channel reaches the ASPD-U on the 4-wire positions of the modes switch (4-ПР. ИВ., 4-ПР. ТВ); on the 2-wire
//     ones it goes to the handset (SAM3D's reading: docs/research/PU12_RADIOS_2026-10.md, decision 3).
//   * Reach: on the mast 15 km (flak11, the PU-12's figure), the mast down 4 km (r407_to: a lowered mast between
//     vehicles on the move); the wide band at 2^(-1/4) of it.
//
// It decides nothing about the battle: the runner gives it the panel, the console's warm-up subsystems, the scenario's
// frequency plan and the distances, and reads back what the sets hear and send.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/pu12.hpp"

namespace simcore {

// ---------------------------------------------------------------------------------------------- the sets' numbers

struct R111Def {
    double freq_min_hz = 20.0e6;
    double freq_max_hz = 52.0e6;
    double subband_split_hz = 36.0e6;    // sub-band I below, II above; the scale reads II, I is 16 MHz lower
    double channel_hz = 25.0e3;
    double power_w = 80.0;
    double fraction_low = 0.01;          // МОЩНОСТЬ 1 %
    double fraction_mid = 0.20;          // МОЩНОСТЬ 20 %
    double preset_drive_s = 15.0;        // the preset mechanism turning
    double tune_s = 30.0;                // the power amplifier and the antenna tuner tuning themselves
    double range_whip_m = 35000.0;       // both ends on whips
    double range_mast_whip_m = 45000.0;  // one on a mast
    double range_mast_mast_m = 60000.0;  // both on masts
    double separation_fraction = 0.10;   // another set's frequency at least this far off
    double bad_spacing_1_hz = 8.0e6;     // and not this far apart
    double bad_spacing_2_hz = 16.0e6;
    double call_s = 5.0;                 // the call lamp's time lit
};

struct R407Def {
    double freq_min_hz = 52.0e6;
    double freq_max_hz = 60.0e6;
    double channel_hz = 50.0e3;
    double duplex_offset_hz = 4.0e6;
    double power_w = 1.0;
    double apch_hold_hz = 15.0e3;        // АПЧ: the detuning it pulls in
    double narrow_hold_hz = 7.0e3;       // without АПЧ, the narrow band
    double wide_hold_hz = 14.0e3;        // without АПЧ, the wide band
    double wide_range_factor = 0.8409;   // the wide band's reach (twice the noise: 2^(-1/4))
    double unsettled_drift_hz = 5.0e3;   // the scale's self-heating drift before it has settled
    double range_mast_m = 15000.0;
    double range_low_m = 4000.0;
    std::vector<double> rx_spurs_hz{53.8e6, 56.85e6, 59.5e6};
    std::vector<double> tx_spurs_hz{52.5e6, 55.55e6, 58.2e6};
    double call_s = 5.0;
};

struct RadioSetsDef {
    std::string id;
    std::string display_name;
    R111Def r111;
    R407Def r407;
};

namespace detail {

class RadioSetsLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, RadioSetsDef& d) {
        if (!Keys(root, "", {"radios_id", "display_name", "status", "notes", "r111", "r407"}) ||
            !RequireString(root, "", "radios_id", d.id) || !OptionalString(root, "", "display_name", d.display_name)) {
            return false;
        }
        const json::Value* a = root.Find("r111");
        if (a == nullptr || !a->Is(json::Type::Object)) return Fail("r111", "required, an object");
        cite_path_ = "r111";
        R111Def& r = d.r111;
        if (!Keys(*a, "r111", {"freq_min_hz", "freq_max_hz", "subband_split_hz", "channel_hz", "power_w", "fraction_low", "fraction_mid",
                               "preset_drive_s", "tune_s", "range_whip_m", "range_mast_whip_m", "range_mast_mast_m",
                               "separation_fraction", "bad_spacing_1_hz", "bad_spacing_2_hz", "call_s", "notes"})) {
            return false;
        }
        for (const auto& f : {std::make_pair("freq_min_hz", &r.freq_min_hz), std::make_pair("freq_max_hz", &r.freq_max_hz),
                              std::make_pair("subband_split_hz", &r.subband_split_hz), std::make_pair("channel_hz", &r.channel_hz),
                              std::make_pair("power_w", &r.power_w), std::make_pair("fraction_low", &r.fraction_low),
                              std::make_pair("fraction_mid", &r.fraction_mid), std::make_pair("preset_drive_s", &r.preset_drive_s),
                              std::make_pair("tune_s", &r.tune_s), std::make_pair("range_whip_m", &r.range_whip_m),
                              std::make_pair("range_mast_whip_m", &r.range_mast_whip_m),
                              std::make_pair("range_mast_mast_m", &r.range_mast_mast_m),
                              std::make_pair("separation_fraction", &r.separation_fraction),
                              std::make_pair("bad_spacing_1_hz", &r.bad_spacing_1_hz), std::make_pair("bad_spacing_2_hz", &r.bad_spacing_2_hz),
                              std::make_pair("call_s", &r.call_s)}) {
            if (!Cited(*a, f.first, *f.second)) return false;
        }
        if (!(r.freq_min_hz < r.subband_split_hz && r.subband_split_hz < r.freq_max_hz)) {
            return Fail("r111.subband_split_hz", "must lie inside the band");
        }
        if (!(r.channel_hz > 0.0) || !(r.fraction_low > 0.0) || !(r.fraction_mid > r.fraction_low) || !(r.fraction_mid <= 1.0)) {
            return Fail("r111", "the channel and the power fractions must be positive and rising");
        }
        const json::Value* b = root.Find("r407");
        if (b == nullptr || !b->Is(json::Type::Object)) return Fail("r407", "required, an object");
        cite_path_ = "r407";
        R407Def& q = d.r407;
        if (!Keys(*b, "r407", {"freq_min_hz", "freq_max_hz", "channel_hz", "duplex_offset_hz", "power_w", "apch_hold_hz", "narrow_hold_hz",
                               "wide_hold_hz", "wide_range_factor", "unsettled_drift_hz", "range_mast_m", "range_low_m", "rx_spurs_hz",
                               "tx_spurs_hz", "call_s", "notes"})) {
            return false;
        }
        for (const auto& f : {std::make_pair("freq_min_hz", &q.freq_min_hz), std::make_pair("freq_max_hz", &q.freq_max_hz),
                              std::make_pair("channel_hz", &q.channel_hz), std::make_pair("duplex_offset_hz", &q.duplex_offset_hz),
                              std::make_pair("power_w", &q.power_w), std::make_pair("apch_hold_hz", &q.apch_hold_hz),
                              std::make_pair("narrow_hold_hz", &q.narrow_hold_hz), std::make_pair("wide_hold_hz", &q.wide_hold_hz),
                              std::make_pair("wide_range_factor", &q.wide_range_factor),
                              std::make_pair("unsettled_drift_hz", &q.unsettled_drift_hz), std::make_pair("range_mast_m", &q.range_mast_m),
                              std::make_pair("range_low_m", &q.range_low_m), std::make_pair("call_s", &q.call_s)}) {
            if (!Cited(*b, f.first, *f.second)) return false;
        }
        if (!(q.freq_min_hz < q.freq_max_hz) || !(q.channel_hz > 0.0)) return Fail("r407", "the band and the channel must be positive");
        for (const auto& f : {std::make_pair("rx_spurs_hz", &q.rx_spurs_hz), std::make_pair("tx_spurs_hz", &q.tx_spurs_hz)}) {
            const json::Value* v = b->Find(f.first);
            const std::string p = Join("r407", f.first);
            if (v == nullptr || !v->Is(json::Type::Object)) return Fail(p, "required, {\"value\": [...], \"source\"}");
            if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            const json::Value* list = v->Find("value");
            if (list == nullptr || !list->Is(json::Type::Array)) return Fail(Join(p, "value"), "expected a list of frequencies");
            f.second->clear();
            for (const json::Value& n : list->items) {
                if (!n.Is(json::Type::Number) || !(n.number > 0.0)) return Fail(Join(p, "value"), "frequencies > 0");
                f.second->push_back(n.number);
            }
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadRadioSetsFile(const std::string& path, RadioSetsDef& out, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text.str(), root, &parse_error)) {
        if (error != nullptr) *error = path + ": " + parse_error;
        return false;
    }
    RadioSetsDef d;
    detail::RadioSetsLoader loader;
    if (!loader.Load(root, d)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(d);
    return true;
}

// ---------------------------------------------------------------------------------------------- the panels' positions

// РЕЖ. РАБОТЫ, ИНДИКАТОР, ВИД РАБОТЫ (clockwise, as fig. 4 draws them), ДИАПАЗ.
enum R111Mode { kR111Off = 0, kR111Duty = 1, kR111Power1 = 2, kR111Power20 = 3, kR111Power100 = 4 };
enum R111Indicator { kR111IndRx = 0, kR111IndTx = 1, kR111IndUm = 2, kR111IndSau = 3, kR111IndSupply = 4 };
enum R111Work { kR111Tone = 0, kR111Phone = 1, kR111Remote = 2, kR111Service = 3, kR111AutoRelay = 4, kR111RelayTx = 5,
                kR111RelayRx = 6, kR111RelayWait = 7 };
enum R111Subband { kR111Calib25 = 0, kR111Calib250 = 1, kR111SubbandI = 2, kR111SubbandII = 3 };
// РОД РАБОТЫ, ПУ, the modes switch, the meter switch of the R-407 (clockwise).
enum R407Mode { kR407Duplex = 0, kR407Duty = 1, kR407Phone = 2 };
enum R407Lines { kR407TwoWireEnd = 0, kR407TwoWireTransit = 1, kR407FourWireIv = 2, kR407FourWireTv = 3 };
enum R407Meter { kR407MeterMeasure = 0, kR407MeterTx = 1, kR407MeterRx = 2, kR407MeterHeater = 3, kR407MeterAntenna = 4 };

// The roles a console's "radios" binding names for each set (console_data.hpp RadioSetBinding).
inline const std::vector<std::string>& R111ControlRoles() {
    static const std::vector<std::string> r{"mode", "indicator", "work", "zpch_pl", "zpch_1", "zpch_2", "zpch_3", "zpch_4",
                                            "subband", "dial", "um", "sau", "call", "squelch", "tlk", "antenna", "rx_level", "tx_level"};
    return r;
}
inline const std::vector<std::string>& R111SubsystemRoles() {
    static const std::vector<std::string> r{"settled", "cathodes", "supply", "mast"};
    return r;
}
inline const std::vector<std::string>& R111InputRoles() {
    static const std::vector<std::string> r{"net", "hear", "tuning_um", "tuning_sau", "tuned", "sau_tuned", "protect", "rx", "tx",
                                            "call_radio", "zpch_1", "zpch_2", "zpch_3", "zpch_4"};
    return r;
}
inline const std::vector<std::string>& R407ControlRoles() {
    static const std::vector<std::string> r{"power", "mode", "apch", "band", "pu", "lines", "meter", "rx_freq", "tx_freq", "rx_lock",
                                            "tx_lock", "calibrator", "call", "measure", "rx_level", "tx_level", "measure_level"};
    return r;
}
inline const std::vector<std::string>& R407SubsystemRoles() {
    static const std::vector<std::string> r{"warm", "settled", "supply", "mast"};
    return r;
}
inline const std::vector<std::string>& R407InputRoles() {
    static const std::vector<std::string> r{"telecode", "link", "call_radio", "call_wire"};
    return r;
}

// Every role named: "" when it is, else the first role missing.
inline std::string MissingRadioRole(const RadioSetBinding& b, bool r111) {
    for (const std::string& role : r111 ? R111ControlRoles() : R407ControlRoles())
        if (b.Control(role).empty()) return "controls." + role;
    for (const std::string& role : r111 ? R111SubsystemRoles() : R407SubsystemRoles())
        if (b.Subsystem(role).empty()) return "subsystems." + role;
    for (const std::string& role : r111 ? R111InputRoles() : R407InputRoles())
        if (b.Input(role).empty()) return "inputs." + role;
    return "";
}

// ---------------------------------------------------------------------------------------------- the frequency plan

// What the scenario gives the post's sets (its "post" block): the regiment's frequencies, whether it works from a mast,
// what the R-111's four presets were prepared on in the park, and the post's own R-123M nets (the 10 % rule).
// A message the regiment's command post sends the post on its command net: said at `at_s`, said again every `repeat_s`
// until the post receipts it, at most `repeats` times more (SAM3D's estimate of a net's discipline).
struct RegimentMessage {
    double at_s = 0.0;
    std::string text;
};

struct RadioPlan {
    bool present = false;             // the scenario gave a plan
    bool prepared = true;             // the R-407's drums and the R-111's scale set in the park to the plan
    std::vector<RegimentMessage> messages;
    double repeat_s = 120.0;
    int repeats = 2;
    double command_net_hz = 0.0;      // the regiment's command net (the R-111); 0: none
    double telecode_hz = 0.0;         // the regiment's telecode transmitter (the post's R-407 receives it); 0: none
    double telecode_return_hz = 0.0;  // the regiment's R-407 receiver (the post transmits on it in duplex)
    bool regiment_mast = true;        // the regiment's antennas on a mast (it stands still)
    std::array<double, 4> presets_hz{{0.0, 0.0, 0.0, 0.0}};
    std::vector<std::pair<std::string, double>> own_nets_hz;  // the post's own sets on other nets: id, frequency
};

// ---------------------------------------------------------------------------------------------- the R-111

struct R111Inputs {
    int mode = kR111Off;
    int work = kR111Phone;
    int subband = kR111SubbandII;
    double dial_mhz = 36.0;          // the scale's reading (sub-band II's figures)
    int zpch_pressed = -1;           // the preset button pressed this step: 0 ПЛ, 1-4; -1 none
    bool um_pressed = false;
    bool sau_pressed = false;
    bool call_pressed = false;
    bool tlk = false;
    bool antenna_mast = false;       // the antenna distributor at ТЕЛЕСКОП
    bool mast_up = false;
    bool settled = false;            // 5 min since switched on (the console's subsystem)
    bool cathodes = false;           // the output valves' cathodes warm
    bool supply = false;             // the vehicle's 27 V network
};

class R111 {
public:
    explicit R111(R111Def def = {}) : def_(def) {}

    const R111Def& Def() const { return def_; }

    // The scale's reading for a frequency (sub-band II's figures), and the frequency a reading gives on a sub-band.
    double DialFor(double hz) const { return (hz < def_.subband_split_hz ? hz + (def_.subband_split_hz - def_.freq_min_hz) : hz) / 1.0e6; }
    double FrequencyOnDial(double dial_mhz, int subband) const {
        const double hz = dial_mhz * 1.0e6;
        return subband == kR111SubbandI ? hz - (def_.subband_split_hz - def_.freq_min_hz) : hz;
    }

    // The set starts the session ready on a preset (a readiness state): on it, tuned for the antenna the inputs give.
    void Prepare(int preset, const RadioPlan& plan, const R111Inputs& in) {
        zpch_ = preset;
        drive_until_ = -1.0;
        tune_until_ = -1.0;
        freq_hz_ = preset >= 1 && preset <= 4 ? plan.presets_hz[static_cast<std::size_t>(preset - 1)] : FrequencyOnDial(in.dial_mhz, in.subband);
        um_tuned_hz_ = freq_hz_;
        sau_tuned_hz_ = freq_hz_;
        sau_mast_ = in.antenna_mast;
        protect_ = false;
        dial_to_set_ = freq_hz_ > 0.0 ? DialFor(freq_hz_) : -1.0;
    }

    // One step. `cosite_tx_hz`: the post's own transmitters on the air now.
    void Update(double t, const R111Inputs& in, const RadioPlan& plan, const std::vector<double>& cosite_tx_hz) {
        const bool on = in.mode >= kR111Duty && in.supply;
        dial_to_set_ = -1.0;
        if (!on) {
            // Off: the mechanism and the automatics stop where they are; the protection resets (r111_to p. 65).
            drive_until_ = -1.0;
            tune_until_ = -1.0;
            protect_ = false;
            return;
        }
        // The preset buttons: ПЛ frees the scale; 1-4 set the mechanism turning (its motor runs off the set's supply).
        if (in.zpch_pressed == 0) {
            zpch_ = 0;
            drive_until_ = -1.0;
        } else if (in.zpch_pressed >= 1 && in.zpch_pressed <= 4 && in.zpch_pressed != zpch_) {
            zpch_ = in.zpch_pressed;
            drive_until_ = t + def_.preset_drive_s;
            tune_until_ = -1.0;
            freq_hz_ = 0.0;
        }
        if (drive_until_ >= 0.0 && t + 1e-9 >= drive_until_) {
            drive_until_ = -1.0;
            freq_hz_ = plan.presets_hz[static_cast<std::size_t>(zpch_ - 1)];
            if (freq_hz_ > 0.0) dial_to_set_ = DialFor(freq_hz_);
            // The automatics retune for the new frequency when the transmitter can be tuned.
            if (in.mode >= kR111Power1 && in.cathodes && freq_hz_ > 0.0) StartTuning(t, true);
        }
        if (zpch_ == 0) {
            const double f = in.subband >= kR111SubbandI ? FrequencyOnDial(in.dial_mhz, in.subband) : 0.0;
            freq_hz_ = f;
        }
        // НАСТРОЙКА УМ: at 20 % with warm cathodes, the amplifier then the tuner; САУ: the tuner alone.
        if (Driving() || freq_hz_ <= 0.0) {
            tune_until_ = -1.0;
        } else if (in.um_pressed && in.mode == kR111Power20 && in.cathodes && !Tuning(t)) {
            StartTuning(t, true);
        } else if (in.sau_pressed && in.mode >= kR111Power1 && in.cathodes && !Tuning(t) && UmTuned()) {
            StartTuning(t, false);
        }
        if (tune_until_ >= 0.0 && in.mode < kR111Power1) tune_until_ = -1.0;  // cut short: nothing tuned
        if (tune_until_ >= 0.0 && t + 1e-9 >= tune_until_) {
            tune_until_ = -1.0;
            if (tune_um_) um_tuned_hz_ = freq_hz_;
            sau_tuned_hz_ = freq_hz_;
            sau_mast_ = in.antenna_mast;
        }
        tuning_from_ = Tuning(t) ? tuning_from_ : -1.0;
        // The input protection: a strong set near by on a frequency too close (r111_to pp. 3, 65).
        if (!protect_) {
            for (double f : cosite_tx_hz)
                if (TooClose(f)) protect_ = true;
        } else if (in.call_pressed && in.mode >= kR111Power20 && CanTransmit(in, t)) {
            protect_ = false;  // transmitting briefly resets it
        }
    }

    // The frequency the set is on: 0 while the mechanism turns, on the calibrator, or a preset never prepared.
    double FrequencyHz() const { return Driving() ? 0.0 : freq_hz_; }
    int Preset() const { return zpch_; }
    bool Driving() const { return drive_until_ >= 0.0; }
    bool Tuning(double t) const { return tune_until_ >= 0.0 && t < tune_until_; }
    bool TuningUm(double t) const { return Tuning(t) && tune_um_ && t < tuning_from_ + 0.5 * def_.tune_s; }
    bool TuningSau(double t) const { return Tuning(t) && !TuningUm(t); }
    bool Protected() const { return protect_; }
    bool UmTuned() const { return freq_hz_ > 0.0 && std::fabs(um_tuned_hz_ - freq_hz_) <= 0.5 * def_.channel_hz; }
    bool SauTuned(bool antenna_mast) const {
        return freq_hz_ > 0.0 && std::fabs(sau_tuned_hz_ - freq_hz_) <= 0.5 * def_.channel_hz && sau_mast_ == antenna_mast;
    }
    // The scale's reading the mechanism left the knob at this step (-1: it didn't move it).
    double DialToSet() const { return dial_to_set_; }

    // The output in watts (0 when it cannot transmit).
    double PowerW(const R111Inputs& in) const {
        switch (in.mode) {
            case kR111Power1: return def_.power_w * def_.fraction_low;
            case kR111Power20: return def_.power_w * def_.fraction_mid;
            case kR111Power100: return def_.power_w;
            default: return 0.0;
        }
    }
    bool CanTransmit(const R111Inputs& in, double t) const {
        const bool work = in.work == kR111Phone || in.work == kR111Remote || in.work == kR111RelayTx;
        return in.mode >= kR111Power1 && in.supply && in.cathodes && !Driving() && !Tuning(t) && UmTuned() &&
               SauTuned(in.antenna_mast) && work && !in.tlk;
    }
    bool CanReceive(const R111Inputs& in) const {
        const bool work = in.work == kR111Phone || in.work == kR111Remote || in.work == kR111RelayRx || in.work == kR111RelayWait ||
                          in.work == kR111AutoRelay;
        return in.mode >= kR111Duty && in.supply && in.settled && !Driving() && !protect_ && work && !in.tlk && freq_hz_ > 0.0;
    }
    bool OnFrequency(double net_hz) const { return net_hz > 0.0 && std::fabs(FrequencyHz() - net_hz) <= 0.5 * def_.channel_hz; }

    // The ТО's ranges by the antennas up at each end.
    double RangeM(bool own_mast, bool other_mast) const {
        if (own_mast && other_mast) return def_.range_mast_mast_m;
        if (own_mast || other_mast) return def_.range_mast_whip_m;
        return def_.range_whip_m;
    }
    // The post's antenna up on the mast: the distributor at ТЕЛЕСКОП and the mast raised.
    static bool OwnMast(const R111Inputs& in) { return in.antenna_mast && in.mast_up; }

    // Hears a transmitter on `net_hz` at `distance_m` sending at `fraction` of full power.
    bool Hears(const R111Inputs& in, double net_hz, double distance_m, bool other_mast, double fraction = 1.0) const {
        if (!CanReceive(in) || !OnFrequency(net_hz)) return false;
        return distance_m <= RangeM(OwnMast(in), other_mast) * std::pow(std::max(0.0, fraction), 0.25);
    }
    // Is heard at `distance_m` on `net_hz`.
    bool Reaches(const R111Inputs& in, double t, double net_hz, double distance_m, bool other_mast) const {
        if (!CanTransmit(in, t) || !OnFrequency(net_hz)) return false;
        return distance_m <= RangeM(OwnMast(in), other_mast) * std::pow(PowerW(in) / def_.power_w, 0.25);
    }

    // Another set transmitting on `hz` near by trips the input protection: within the separation, or 8 or 16 MHz off.
    bool TooClose(double hz) const {
        const double f = FrequencyHz();
        if (f <= 0.0 || hz <= 0.0) return false;
        const double d = std::fabs(hz - f);
        return d < def_.separation_fraction * f || std::fabs(d - def_.bad_spacing_1_hz) <= def_.channel_hz ||
               std::fabs(d - def_.bad_spacing_2_hz) <= def_.channel_hz;
    }

private:
    void StartTuning(double t, bool um) {
        tune_um_ = um;
        tuning_from_ = t;
        tune_until_ = t + (um ? def_.tune_s : 0.5 * def_.tune_s);
        if (um) um_tuned_hz_ = 0.0;
        sau_tuned_hz_ = 0.0;
    }

    R111Def def_;
    int zpch_ = 0;
    double freq_hz_ = 0.0;
    double drive_until_ = -1.0;
    double tune_until_ = -1.0;
    double tuning_from_ = -1.0;
    bool tune_um_ = true;
    double um_tuned_hz_ = 0.0;
    double sau_tuned_hz_ = 0.0;
    bool sau_mast_ = false;
    bool protect_ = false;
    double dial_to_set_ = -1.0;
};

// ---------------------------------------------------------------------------------------------- the R-407

struct R407Inputs {
    bool power = false;
    int mode = kR407Duplex;
    bool apch = true;
    bool wide = false;
    int lines = kR407TwoWireEnd;
    double rx_mhz = 52.0;
    double tx_mhz = 56.0;
    bool warm = false;         // the valves warm (the console's subsystem)
    bool settled = false;      // 15 min: the scales to 8 kHz
    bool supply = false;
    bool mast_up = false;
};

class R407 {
public:
    explicit R407(R407Def def = {}) : def_(std::move(def)) {}

    const R407Def& Def() const { return def_; }

    bool On(const R407Inputs& in) const { return in.power && in.supply && in.warm; }
    bool RadioMode(const R407Inputs& in) const { return in.mode == kR407Duplex || in.mode == kR407Duty; }
    // How far off the regiment's channel the receiver is (Hz), the scale's drift before it settles included.
    double DetuneHz(const R407Inputs& in, double regiment_tx_hz) const {
        return std::fabs(in.rx_mhz * 1.0e6 - regiment_tx_hz) + (in.settled ? 0.0 : def_.unsettled_drift_hz);
    }
    double HoldHz(const R407Inputs& in) const { return in.apch ? def_.apch_hold_hz : (in.wide ? def_.wide_hold_hz : def_.narrow_hold_hz); }
    bool OnSpur(double hz, const std::vector<double>& spurs) const {
        for (double s : spurs)
            if (std::fabs(hz - s) < 0.5 * def_.channel_hz) return true;
        return false;
    }
    // In duplex the set's own transmitter is on the air: the receiver is deaf unless 4 MHz off, and off the spurs.
    bool DuplexClean(const R407Inputs& in) const {
        if (in.mode != kR407Duplex) return true;
        const double gap = std::fabs(in.tx_mhz - in.rx_mhz) * 1.0e6;
        return std::fabs(gap - def_.duplex_offset_hz) < 0.5 * def_.channel_hz && !OnSpur(in.tx_mhz * 1.0e6, def_.tx_spurs_hz);
    }
    double RangeM(const R407Inputs& in) const { return (in.mast_up ? def_.range_mast_m : def_.range_low_m) * (in.wide ? def_.wide_range_factor : 1.0); }

    // The regiment's channel comes in. `interference`: the regiment's net has noise on it (АПЧ off rides it out);
    // `off_air`: it isn't sending at all.
    bool Link(const R407Inputs& in, double regiment_tx_hz, double distance_m, bool off_air, bool interference) const {
        if (!On(in) || !RadioMode(in) || regiment_tx_hz <= 0.0 || off_air) return false;
        if (interference && in.apch) return false;
        if (DetuneHz(in, regiment_tx_hz) > HoldHz(in)) return false;
        if (OnSpur(in.rx_mhz * 1.0e6, def_.rx_spurs_hz) || !DuplexClean(in)) return false;
        return distance_m <= RangeM(in);
    }
    // The post's own telecode reaches the regiment's receiver (a transmitting post: flak11 PU12-0's figure): on, in
    // duplex, the transmitter on the regiment's receive channel and off the spurs, in reach, the regiment on the air.
    bool SendLink(const R407Inputs& in, double regiment_rx_hz, double distance_m, bool off_air) const {
        if (!On(in) || in.mode != kR407Duplex || regiment_rx_hz <= 0.0 || off_air) return false;
        if (std::fabs(in.tx_mhz * 1.0e6 - regiment_rx_hz) > HoldHz(in) || OnSpur(in.tx_mhz * 1.0e6, def_.tx_spurs_hz)) return false;
        return distance_m <= RangeM(in);
    }
    // The channel goes on to the ASPD-U on the 4-wire positions.
    static bool ToEquipment(const R407Inputs& in) { return in.lines == kR407FourWireIv || in.lines == kR407FourWireTv; }

private:
    R407Def def_;
};

// ---------------------------------------------------------------------------------------------- reading the console

// The panel's R-111 as its inputs (the preset buttons pressed this step included).
inline R111Inputs ReadR111(const ConsoleData& c) {
    const RadioSetBinding& b = c.r111;
    R111Inputs in;
    const ControlPanel& p = c.panel;
    in.mode = p.Get(b.Control("mode"));
    in.work = p.Get(b.Control("work"));
    in.subband = p.Get(b.Control("subband"));
    in.dial_mhz = p.GetValue(b.Control("dial"));
    const char* buttons[] = {"zpch_pl", "zpch_1", "zpch_2", "zpch_3", "zpch_4"};
    for (int i = 0; i < 5; ++i)
        if (p.Get(b.Control(buttons[i])) == 1) in.zpch_pressed = i;
    in.um_pressed = p.Get(b.Control("um")) == 1;
    in.sau_pressed = p.Get(b.Control("sau")) == 1;
    in.call_pressed = p.Get(b.Control("call")) == 1;
    in.tlk = p.Get(b.Control("tlk")) == 1;
    in.antenna_mast = p.Get(b.Control("antenna")) == 1;
    in.mast_up = c.equipment.State(b.Subsystem("mast")) == SubsystemState::Ready;
    in.settled = c.equipment.State(b.Subsystem("settled")) == SubsystemState::Ready;
    in.cathodes = c.equipment.State(b.Subsystem("cathodes")) == SubsystemState::Ready;
    in.supply = c.equipment.State(b.Subsystem("supply")) == SubsystemState::Ready;
    return in;
}

inline R407Inputs ReadR407(const ConsoleData& c) {
    const RadioSetBinding& b = c.r407;
    R407Inputs in;
    const ControlPanel& p = c.panel;
    in.power = p.Get(b.Control("power")) == 1;
    in.mode = p.Get(b.Control("mode"));
    in.apch = p.Get(b.Control("apch")) == 1;
    in.wide = p.Get(b.Control("band")) == 1;
    in.lines = p.Get(b.Control("lines"));
    in.rx_mhz = p.GetValue(b.Control("rx_freq"));
    in.tx_mhz = p.GetValue(b.Control("tx_freq"));
    in.warm = c.equipment.State(b.Subsystem("warm")) == SubsystemState::Ready;
    in.settled = c.equipment.State(b.Subsystem("settled")) == SubsystemState::Ready;
    in.supply = c.equipment.State(b.Subsystem("supply")) == SubsystemState::Ready;
    in.mast_up = c.equipment.State(b.Subsystem("mast")) == SubsystemState::Ready;
    return in;
}

// The needle of a set's meter (0..1 of its swing), worked out from the console alone so a mirrored console reads the
// same: "r111_indicator" (the ИНДИКАТОР switch: ПР. УР., ПРД. УР., НАСТР. БУМ, НАСТР. САУ, БОРТ. СЕТЬ) or "r407_meter"
// (ИЗМЕРИТ., ПЕРЕДАЧА, ПРИЕМ, НАКАЛ, ТОК АНТ.). The marks are SAM3D's: in the sector (about 0.65) when all is well.
inline double RadioMeterReading(const std::string& reading, const ConsoleData& c) {
    const Equipment& eq = c.equipment;
    const ControlPanel& p = c.panel;
    auto in = [&](const RadioSetBinding& b, const char* role) { return eq.GetInput(b.Input(role)); };
    auto ready = [&](const RadioSetBinding& b, const char* role) { return eq.State(b.Subsystem(role)) == SubsystemState::Ready; };
    if (reading == "r111_indicator" && c.r111.present) {
        const RadioSetBinding& b = c.r111;
        if (!ready(b, "supply")) return 0.0;
        const int mode = p.Get(b.Control("mode"));
        switch (p.Get(b.Control("indicator"))) {
            case kR111IndSupply: return 0.65;  // 26 V in the sector (r111_to p. 64: the needle in its sector)
            case kR111IndRx:
                if (mode < kR111Duty) return 0.0;
                return in(b, "hear") ? 0.25 + 0.6 * std::clamp(p.GetValue(b.Control("rx_level")), 0.0, 1.0) : 0.08;
            case kR111IndTx:
                return in(b, "tx") ? 0.25 + 0.6 * std::clamp(p.GetValue(b.Control("tx_level")), 0.0, 1.0) : 0.0;
            case kR111IndUm:
                if (mode < kR111Power1) return 0.0;
                return in(b, "tuning_um") ? 0.35 : (in(b, "tuned") ? 0.75 : 0.1);
            case kR111IndSau:
                if (mode < kR111Power1) return 0.0;
                return in(b, "tuning_sau") ? 0.35 : (in(b, "sau_tuned") ? 0.75 : 0.1);
            default: return 0.0;
        }
    }
    if (reading == "r407_meter" && c.r407.present) {
        const RadioSetBinding& b = c.r407;
        if (!ready(b, "supply") || p.Get(b.Control("power")) != 1) return 0.0;
        const bool warm = ready(b, "warm");
        const int mode = p.Get(b.Control("mode"));
        switch (p.Get(b.Control("meter"))) {
            case kR407MeterHeater: return 0.65;  // the heaters at the red mark
            case kR407MeterMeasure:
                return p.Get(b.Control("measure")) == 1 ? 0.2 + 0.6 * std::clamp(p.GetValue(b.Control("measure_level")), 0.0, 1.0) : 0.0;
            case kR407MeterTx:
                return warm && mode == kR407Duplex ? 0.2 + 0.6 * std::clamp(p.GetValue(b.Control("tx_level")), 0.0, 1.0) : 0.0;
            case kR407MeterRx:
                if (!warm || mode == kR407Phone) return 0.0;
                return in(b, "link") ? 0.2 + 0.6 * std::clamp(p.GetValue(b.Control("rx_level")), 0.0, 1.0) : 0.05;
            case kR407MeterAntenna: return warm && mode == kR407Duplex ? 0.6 : 0.0;
            default: return 0.0;
        }
    }
    return 0.0;
}

inline bool KnownRadioReading(const std::string& name) { return name == "r111_indicator" || name == "r407_meter"; }

}  // namespace simcore
