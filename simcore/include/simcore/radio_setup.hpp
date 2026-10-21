// simcore/radio_setup.hpp — tuning the R-123M sets (roadmap 7.8): the four fixed frequencies set up under their
// fixators, smooth tuning, and the antenna circuit tuned to resonance (r123m_training: the order of work on the set).
//
// Each set keeps, for each fixed frequency, what its fixator holds: the frequency, its sub-band and the antenna setting.
//   * Selecting a fixed frequency, the set's motors drive the frequency and antenna knobs to it (motor_s; until they
//     stop, the set is off tune: "wait until УСТАНОВКА ЧАСТОТЫ and НАСТРОЙКА АНТЕННЫ stop turning").
//   * With that frequency's fixator unlocked, the set tunes off УСТАНОВКА ЧАСТОТЫ (on the 25 kHz grid) and the
//     ПОДДИАПАЗОН switch; locking the fixator keeps what is set. The fixators sit under a cover.
//   * With the antenna knob's lock off, the antenna follows the knob; locking it keeps that setting for this frequency.
//   * In ПЛАВНЫЙ ПОДДИАПАЗОН I or II the set tunes straight off the knobs (no grid); it is meant for a halt: on the move
//     the vibration walks the frequency off (smooth_drift_khz_s, a SAM3D estimate).
// The antenna circuit resonates at a knob setting that rises with the frequency (0 at 20 MHz, 100 at 51.5 MHz: a SAM3D
// model of the variometer). The tuning meter reads 1 / (1 + (error / antenna_width)^2) while the set is keyed; the
// mismatch costs -10 log10 of it on receive and transmit, and the tuning lamp is at its brightest (tuned_input) from
// tuned_reading up. Deterministic.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"

namespace simcore {

class RadioTuning {
public:
    struct Preset {
        double freq_mhz = 0.0;  // 0: never set up
        int subband = 0;        // 0 I, 1 II
        double antenna = 0.0;   // knob divisions
    };
    struct Event {
        int set = 0;
        std::string text;
    };

    static constexpr double kLowMhz = 20.0;
    static constexpr double kHighMhz = 51.5;
    static constexpr double kBandSplitMhz = 35.75;  // sub-band I 20-35.75, II 35.75-51.5 (r123_refs)
    static constexpr double kGridMhz = 0.025;

    static int BandOf(double mhz) { return mhz < kBandSplitMhz ? 0 : 1; }
    static double Grid(double mhz) { return std::round(mhz / kGridMhz) * kGridMhz; }
    static double AntennaOptimum(double mhz) { return std::min(100.0, std::max(0.0, 100.0 * (mhz - kLowMhz) / (kHighMhz - kLowMhz))); }

    explicit RadioTuning(const CommsDef& def) : def_(def) {}

    // What the fixators hold at the start: frequency per set and fixed frequency (0: not set up). The antenna is tuned
    // and the sub-band right for each frequency set.
    void SetPresets(int set, const std::array<double, 4>& mhz) {
        if (set < 0 || set > 1) return;
        for (std::size_t p = 0; p < 4; ++p) {
            Preset& pr = sets_[static_cast<std::size_t>(set)].presets[p];
            pr.freq_mhz = mhz[p] > 0.0 ? Grid(mhz[p]) : 0.0;
            pr.subband = BandOf(pr.freq_mhz);
            pr.antenna = AntennaOptimum(pr.freq_mhz);
        }
    }
    // One fixed frequency set up anew (the crew re-tuned it onto another net), antenna tuned and sub-band right.
    void SetPresetFrequency(int set, int preset, double mhz) {
        if (set < 0 || set > 1 || preset < 0 || preset > 3) return;
        Preset& pr = sets_[static_cast<std::size_t>(set)].presets[static_cast<std::size_t>(preset)];
        pr.freq_mhz = mhz > 0.0 ? Grid(mhz) : 0.0;
        pr.subband = BandOf(pr.freq_mhz);
        pr.antenna = AntennaOptimum(pr.freq_mhz);
    }
    // Put each set's knobs where its selected fixed frequency holds them (the start of a session: the motors have run).
    void Settle(ConsoleData& console) {
        for (std::size_t s = 0; s < def_.radios.size() && s < 2; ++s) {
            const CommsRadioDef& r = def_.radios[s];
            if (!r.HasSetup()) continue;
            State& st = sets_[s];
            st.position = console.panel.Get(r.preset);
            if (st.position >= 0 && st.position < 4) Drive(console, r, st.presets[static_cast<std::size_t>(st.position)]);
            st.motor_until_s = -1.0;
            for (std::size_t p = 0; p < 4; ++p) st.fix_was_open[p] = console.panel.Get(r.fixators[p]) == 1;
            st.ant_was_open = console.panel.Get(r.antenna_lock) == 1;
            Update(s, console, false);
        }
    }

    // One step. `moving`: the vehicle is on the move (a smooth-tuned frequency walks off). `keyed[s]`: set s is
    // transmitting (the tuning meter reads, the lamp can light). Returns what the crew set (for the debrief).
    std::vector<Event> Step(double t, double dt, ConsoleData& console, bool moving, const bool keyed[2]) {
        std::vector<Event> out;
        for (std::size_t s = 0; s < def_.radios.size() && s < 2; ++s) {
            const CommsRadioDef& r = def_.radios[s];
            if (!r.HasSetup()) continue;
            State& st = sets_[s];
            const int pos = console.panel.Get(r.preset);
            if (pos != st.position) {  // a new position: the motors drive the knobs there
                st.position = pos;
                st.drift_mhz = 0.0;
                st.motor_until_s = pos < 4 ? t + def_.motor_s : -1.0;
                st.driven = false;
            }
            if (pos < 4 && !st.driven && t + 1e-9 >= st.motor_until_s) {
                Drive(console, r, st.presets[static_cast<std::size_t>(pos)]);
                st.driven = true;
            }
            if (pos < 4) {
                const std::size_t p = static_cast<std::size_t>(pos);
                const bool fix_open = console.panel.Get(r.fixators[p]) == 1;
                if (st.fix_was_open[p] && !fix_open) {  // the fixator locked: it keeps what is set
                    Preset& pr = st.presets[p];
                    pr.freq_mhz = Grid(console.panel.GetValue(r.frequency));
                    pr.subband = console.panel.Get(r.subband);
                    char text[96];
                    std::snprintf(text, sizeof(text), "fixed frequency %d set to %.3f MHz, sub-band %s", pos + 1, pr.freq_mhz,
                                  pr.subband == 0 ? "I" : "II");
                    out.push_back({static_cast<int>(s), text});
                }
                for (std::size_t k = 0; k < 4; ++k) st.fix_was_open[k] = console.panel.Get(r.fixators[k]) == 1;
                const bool ant_open = console.panel.Get(r.antenna_lock) == 1;
                if (st.ant_was_open && !ant_open) {
                    st.presets[p].antenna = console.panel.GetValue(r.antenna_tune);
                    char text[96];
                    std::snprintf(text, sizeof(text), "antenna tuned for fixed frequency %d (meter %.0f %%)", pos + 1,
                                  100.0 * Reading(static_cast<int>(s)));
                    out.push_back({static_cast<int>(s), text});
                }
                st.ant_was_open = ant_open;
            } else if (moving) {
                st.drift_mhz += def_.smooth_drift_khz_s * dt / 1000.0;
            }
            Update(s, console, keyed[s]);
        }
        return out;
    }

    // The frequency the set is on now (MHz), 0 while its motors run.
    double FrequencyMhz(int set) const { return Valid(set) ? sets_[static_cast<std::size_t>(set)].freq_mhz : 0.0; }
    // On frequency `mhz` (within half a grid step) with the right sub-band switched in.
    bool OnFrequency(int set, double mhz) const {
        if (!Valid(set)) return true;
        const State& st = sets_[static_cast<std::size_t>(set)];
        return st.freq_mhz > 0.0 && st.band_ok && std::fabs(st.freq_mhz - mhz) <= 0.5 * kGridMhz + 1e-9;
    }
    // Why not (for the debrief), or "".
    std::string OffFrequencyWhy(int set, double mhz) const {
        if (!Valid(set) || OnFrequency(set, mhz)) return std::string();
        const State& st = sets_[static_cast<std::size_t>(set)];
        char text[160];
        if (st.freq_mhz <= 0.0) {
            std::snprintf(text, sizeof(text), "is tuning (the motors are turning the knobs)");
        } else if (!st.band_ok) {
            std::snprintf(text, sizeof(text), "is on the wrong sub-band for %.3f MHz", st.freq_mhz);
        } else {
            std::snprintf(text, sizeof(text), "is on %.3f MHz, the net on %.3f MHz", st.freq_mhz, mhz);
        }
        return text;
    }
    // The tuning meter's reading (0..1): how well the antenna circuit is tuned to the frequency.
    double Reading(int set) const { return Valid(set) ? sets_[static_cast<std::size_t>(set)].reading : 1.0; }
    // The mismatch's loss on receive and transmit (dB).
    double LossDb(int set) const {
        const double r = Reading(set);
        return r > 1e-6 ? -10.0 * std::log10(r) : 60.0;
    }
    const Preset& PresetOf(int set, int preset) const { return sets_[static_cast<std::size_t>(set)].presets[static_cast<std::size_t>(preset)]; }
    bool Smooth(int set) const { return Valid(set) && sets_[static_cast<std::size_t>(set)].position >= 4; }
    bool MotorsRunning(int set) const { return Valid(set) && sets_[static_cast<std::size_t>(set)].freq_mhz <= 0.0; }

private:
    struct State {
        std::array<Preset, 4> presets{};
        int position = -1;
        double motor_until_s = -1.0;
        bool driven = true;
        std::array<bool, 4> fix_was_open{};
        bool ant_was_open = false;
        double drift_mhz = 0.0;
        double freq_mhz = 0.0;
        bool band_ok = true;
        double reading = 1.0;
    };

    bool Valid(int set) const {
        return set >= 0 && set < 2 && static_cast<std::size_t>(set) < def_.radios.size() && def_.radios[static_cast<std::size_t>(set)].HasSetup();
    }

    static void Drive(ConsoleData& console, const CommsRadioDef& r, const Preset& pr) {
        if (pr.freq_mhz > 0.0) console.panel.SetValue(r.frequency, pr.freq_mhz);
        console.panel.SetValue(r.antenna_tune, pr.antenna);
    }

    void Update(std::size_t s, ConsoleData& console, bool keyed) {
        const CommsRadioDef& r = def_.radios[s];
        State& st = sets_[s];
        const int pos = st.position;
        double freq = 0.0, antenna = 0.0;
        int band = 0;
        if (pos >= 0 && pos < 4) {
            const Preset& pr = st.presets[static_cast<std::size_t>(pos)];
            const bool fix_open = console.panel.Get(r.fixators[static_cast<std::size_t>(pos)]) == 1;
            freq = fix_open ? Grid(console.panel.GetValue(r.frequency)) : pr.freq_mhz;
            band = fix_open ? console.panel.Get(r.subband) : pr.subband;
            antenna = console.panel.Get(r.antenna_lock) == 1 ? console.panel.GetValue(r.antenna_tune) : pr.antenna;
            if (!st.driven) freq = 0.0;  // the motors are still turning
        } else {
            freq = console.panel.GetValue(r.frequency) + st.drift_mhz;
            band = pos - 4;
            antenna = console.panel.GetValue(r.antenna_tune);
        }
        st.freq_mhz = freq;
        st.band_ok = freq <= 0.0 || BandOf(freq) == band;
        const double err = freq > 0.0 ? (antenna - AntennaOptimum(freq)) / def_.antenna_width : 0.0;
        st.reading = freq > 0.0 ? 1.0 / (1.0 + err * err) : 0.0;
        if (!r.tuned_input.empty()) console.equipment.SetInput(r.tuned_input, keyed && st.reading >= def_.tuned_reading);
    }

    CommsDef def_;
    std::array<State, 2> sets_{};
};

}  // namespace simcore
