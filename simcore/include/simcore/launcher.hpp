// simcore/launcher.hpp — the missile load and the two guidance channels (roadmap 6.3).
//
// Driven by the console's "launcher" block (console_data.hpp, LauncherDef):
//   * Six containers, each on a channel (I or II); each has an ammunition lamp, lit while a missile is in it.
//   * At ВНИМАНИЕ (or when preparation is started by hand) the launcher picks one missile for each channel the КАНАЛЫ selector allows, lowest order first
//     (the outer containers), and that channel's command transmitter comes on (ВКЛ ВЫС).
//   * A launch takes the selected missile of the first free channel in the selector's order. Its lamps then follow
//     the missile: СХОД, ЗАХВАТ (captured in the wide beam), СР.Л (medium beam), УЗКИЙ ЛУЧ (narrow beam), К3 (fuse
//     armed near the target, or with the medium beam against a jamming target: method Н, 1991 textbook p23-24).
//   * The channel is busy until the shot is over (the outcome model says so, or the nominal flight time passes).
//   * Launches are at least min_interval_s apart; the hold input is set while the next one must wait.
//   * Reload (roadmap 7.3): with the TZM alongside and the vehicle stopped, one missile goes into an empty container
//     every reload_s_per_missile; nothing is loaded while a channel is guiding or a pair is selected.
//   * PairSpent(): no selected missile is left, so the next ones need preparing again.
//   * Letter change: СМЕНА ЛИТ. changes the command transmitters' frequency letter; ЗАХВ. АПЧ goes out for relock_s on
//     both channels and the next launch waits for it.
//   * Emergency launch: ИЗДЕЛИЯ picks a container and АВАРИЙНЫЙ ПУСК throws that missile off unguided; it climbs for
//     flight_s and self-destructs. It uses no channel and no outcome model, and counts as no guided launch.
// Deterministic; it reads the console and sets the console's inputs, nothing else.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"

namespace simcore {

class Launcher {
public:
    struct Shot {
        int channel = 0;  // 1 = I, 2 = II
        std::string container;
    };

    explicit Launcher(const LauncherDef& def) : def_(def), loaded_(def.containers.size(), true) {}

    // Empty the given containers (a scenario that starts with fewer than six missiles).
    void Unload(const std::string& container_id) {
        for (std::size_t i = 0; i < def_.containers.size(); ++i) {
            if (def_.containers[i].id == container_id) loaded_[i] = false;
        }
    }

    // Once per step: selection at ВНИМАНИЕ, the end of finished flights, and every lamp input.
    void Step(double t, ConsoleData& console) {
        Equipment& eq = console.equipment;
        Reload(t, console);
        // Missiles are picked at ВНИМАНИЕ, or when preparation is started by hand (БОРТ СПК, e.g. with no radar range).
        const bool attention = eq.GetInput(def_.attention_input) || eq.State(def_.pair_preparation) != SubsystemState::Off;
        if (attention) {
            for (int ch : AllowedChannels(console)) {
                Channel& c = channels_[static_cast<std::size_t>(ch - 1)];
                if (c.selected < 0 && !c.flying) c.selected = Pick(ch);
            }
        }
        for (Channel& c : channels_) {
            if (c.flying && t >= c.end_s) c.flying = false;  // the nominal flight is over
        }
        StepLetter(t, console);
        const bool jamming = !def_.jam_control.empty() && console.panel.Get(def_.jam_control) == 1;
        Set(eq, def_.hold_input, NextChannel(t, console) == 0);
        for (std::size_t i = 0; i < def_.containers.size(); ++i) {
            if (!def_.containers[i].loaded_input.empty()) eq.SetInput(def_.containers[i].loaded_input, loaded_[i]);
        }
        for (std::size_t k = 0; k < channels_.size() && k < def_.channels.size(); ++k) {
            const Channel& c = channels_[k];
            const LauncherChannelDef& in = def_.channels[k];
            const double since = t - c.launch_s;
            const bool narrow = c.flying && since >= def_.narrow_s;
            const bool near_target = c.flying && c.speed_mps > 0.0 && (c.end_s - t) * c.speed_mps <= def_.fuse_range_m;
            Set(eq, in.transmitter_input, c.selected >= 0 || c.flying);
            if (def_.has_letter) Set(eq, def_.letter.lock_inputs[k], (c.selected >= 0 || c.flying) && t >= relock_until_s_);
            Set(eq, in.away_input, c.flying);
            Set(eq, in.captured_input, c.flying && since >= def_.capture_s);
            Set(eq, in.medium_input, c.flying && since >= def_.medium_s);
            Set(eq, in.narrow_input, narrow);
            const bool medium = c.flying && since >= def_.medium_s;
            Set(eq, in.fuse_input, c.flying && (near_target || (jamming && medium)));
        }
        if (def_.has_emergency) Set(eq, def_.emergency.ready_input, EmergencyContainer(console) >= 0);
    }

    // The channel the tracking radar's range indicator follows (1, 2), or 0 when it shows the target (ЦЕЛЬ), or when
    // the console has no such selector.
    int RangeSweepChannel(const ConsoleData& console) const {
        if (def_.range_sweep_control.empty()) return 0;
        const int pos = console.panel.Get(def_.range_sweep_control);
        if (pos < 0 || pos >= static_cast<int>(def_.range_sweep_channels.size())) return 0;
        return def_.range_sweep_channels[static_cast<std::size_t>(pos)];
    }

    // How far from the unit a channel's missile is at time t, along its flight at the shot's average speed; -1 if
    // that channel has no missile in flight or the shot has no timed flight (no outcome model).
    double MissileRangeM(int channel, double t) const {
        if (channel < 1 || channel > 2) return -1.0;
        const Channel& c = channels_[static_cast<std::size_t>(channel - 1)];
        if (!c.flying || c.speed_mps <= 0.0 || t < c.launch_s) return -1.0;
        return c.speed_mps * std::min(t - c.launch_s, c.end_s - c.launch_s);
    }

    // The letter has been changed this many times (the command transmitters' frequency letter).
    int LetterChanges() const { return letter_changes_; }
    bool Relocking(double t) const { return t < relock_until_s_; }

    // The container ИЗДЕЛИЯ is set to, if it holds a missile (-1: ОТКЛ., empty, or no emergency launch). Returns its id too.
    std::string EmergencySelected(const ConsoleData& console) const {
        const int i = EmergencyContainer(console);
        return i >= 0 ? def_.containers[static_cast<std::size_t>(i)].id : std::string();
    }

    // АВАРИЙНЫЙ ПУСК pressed: throws the selected missile off unguided. Returns its container, "" if nothing was thrown.
    std::string EmergencyLaunch(double t, const ConsoleData& console) {
        const int i = EmergencyContainer(console);
        if (i < 0) return std::string();
        loaded_[static_cast<std::size_t>(i)] = false;
        for (Channel& c : channels_) {
            if (c.selected == i) c.selected = -1;  // the channel's missile is gone; it takes another at the next ВНИМАНИЕ
        }
        emergency_end_s_.push_back(t + def_.emergency.flight_s);
        return def_.containers[static_cast<std::size_t>(i)].id;
    }

    // Unguided missiles still climbing at time t.
    int EmergencyInFlight(double t) const {
        int n = 0;
        for (double e : emergency_end_s_) n += t < e ? 1 : 0;
        return n;
    }
    int EmergencyThrown() const { return static_cast<int>(emergency_end_s_.size()); }

    // The container being filled right now (-1 if none): the first empty one, outer ones last.
    int Reloading() const { return reloading_; }
    double ReloadProgress() const { return reload_progress_s_; }

    // The channel the next launch would use (0 = none: no selected missile on a free channel, or too soon).
    int NextChannel(double t, const ConsoleData& console) const {
        if (t < last_launch_s_ + def_.min_interval_s || t < relock_until_s_) return 0;
        for (int ch : AllowedChannels(console)) {
            const Channel& c = channels_[static_cast<std::size_t>(ch - 1)];
            if (c.selected >= 0 && !c.flying) return ch;
        }
        return 0;
    }

    // Why a launch can't happen now ("" if it can).
    std::string WhyNot(double t, const ConsoleData& console) const {
        if (NextChannel(t, console) != 0) return std::string();
        if (MissilesLeft() == 0) return "no missile left";
        if (t < last_launch_s_ + def_.min_interval_s) return "too soon after the last launch";
        if (t < relock_until_s_) return "the command transmitters are relocking after the letter change";
        return "no missile selected on a free channel";
    }

    // Launch the next missile. `flight_s` is how long the channel stays busy (the outcome model's flight time, or 0
    // for the nominal one); `speed_mps` places К3. Returns the channel and container, or a channel of 0 if none.
    Shot Launch(double t, const ConsoleData& console, int shot_number, double flight_s, double speed_mps) {
        Shot out;
        const int ch = NextChannel(t, console);
        if (ch == 0) return out;
        last_launch_s_ = t;
        Channel& c = channels_[static_cast<std::size_t>(ch - 1)];
        const std::size_t i = static_cast<std::size_t>(c.selected);
        loaded_[i] = false;
        c.selected = -1;
        c.flying = true;
        c.shot = shot_number;
        c.launch_s = t;
        c.end_s = t + (flight_s > 0.0 ? flight_s : def_.nominal_flight_s);
        c.speed_mps = flight_s > 0.0 ? speed_mps : 0.0;
        out.channel = ch;
        out.container = def_.containers[i].id;
        return out;
    }

    // The outcome model has resolved a shot (hit, miss or lost): its channel is free.
    void ShotDone(int shot_number) {
        for (Channel& c : channels_) {
            if (c.flying && c.shot == shot_number) c.flying = false;
        }
    }

    // Shots still flying (their numbers), for the runner to ask the outcome model about.
    std::vector<int> ShotsFlying() const {
        std::vector<int> out;
        for (const Channel& c : channels_) {
            if (c.flying) out.push_back(c.shot);
        }
        return out;
    }

    // No selected missile is left: the next ones must be selected and prepared again.
    bool PairSpent() const { return channels_[0].selected < 0 && channels_[1].selected < 0; }

    int MissilesLeft() const {
        int n = 0;
        for (bool l : loaded_) n += l ? 1 : 0;
        return n;
    }
    bool Loaded(const std::string& container_id) const {
        for (std::size_t i = 0; i < def_.containers.size(); ++i) {
            if (def_.containers[i].id == container_id) return loaded_[i];
        }
        return false;
    }
    // The container selected on a channel (1 or 2), "" if none.
    std::string Selected(int channel) const {
        const int s = channels_[static_cast<std::size_t>(channel - 1)].selected;
        return s >= 0 ? def_.containers[static_cast<std::size_t>(s)].id : std::string();
    }

    static const char* ChannelName(int channel) { return channel == 1 ? "I" : channel == 2 ? "II" : "-"; }

private:
    struct Channel {
        int selected = -1;  // container index
        bool flying = false;
        int shot = 0;
        double launch_s = 0.0;
        double end_s = 0.0;
        double speed_mps = 0.0;
    };

    // The letter change is the control entering its change position, once.
    void StepLetter(double t, const ConsoleData& console) {
        if (!def_.has_letter) return;
        const int pos = console.panel.Get(def_.letter.control);
        if (letter_pos_ >= 0 && pos == def_.letter.change_position && letter_pos_ != pos) {
            ++letter_changes_;
            relock_until_s_ = t + def_.letter.relock_s;
        }
        letter_pos_ = pos;
    }

    int EmergencyContainer(const ConsoleData& console) const {
        if (!def_.has_emergency) return -1;
        const int pos = console.panel.Get(def_.emergency.select_control);
        if (pos < 0 || pos >= static_cast<int>(def_.emergency.select_containers.size())) return -1;
        const std::string& id = def_.emergency.select_containers[static_cast<std::size_t>(pos)];
        for (std::size_t i = 0; i < def_.containers.size(); ++i) {
            if (def_.containers[i].id == id && loaded_[i]) return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<int> AllowedChannels(const ConsoleData& console) const {
        const int pos = console.panel.Get(def_.channel_control);
        if (pos < 0 || pos >= static_cast<int>(def_.channel_positions.size())) return {};
        return def_.channel_positions[static_cast<std::size_t>(pos)];
    }

    int Pick(int channel) const {
        int best = -1;
        for (std::size_t i = 0; i < def_.containers.size(); ++i) {
            const LauncherContainerDef& c = def_.containers[i];
            if (c.channel != channel || !loaded_[i]) continue;
            if (best < 0 || c.order < def_.containers[static_cast<std::size_t>(best)].order) best = static_cast<int>(i);
        }
        return best;
    }

    void Reload(double t, const ConsoleData& console) {
        const Equipment& eq = console.equipment;
        const double dt = last_t_ < 0.0 ? 0.0 : t - last_t_;
        last_t_ = t;
        reloading_ = -1;
        if (def_.reload_s_per_missile <= 0.0 || !eq.GetInput(def_.reload_input) || !eq.GetInput(def_.stopped_input)) {
            reload_progress_s_ = 0.0;
            return;
        }
        for (const Channel& c : channels_) {
            if (c.flying || c.selected >= 0) {  // the crane waits until the pair is spent and the missiles are away
                reload_progress_s_ = 0.0;
                return;
            }
        }
        int empty = -1;  // loading order: firing order, so the missiles that go next are the first ones back
        for (std::size_t i = 0; i < def_.containers.size(); ++i) {
            if (loaded_[i]) continue;
            if (empty < 0 || def_.containers[i].order < def_.containers[static_cast<std::size_t>(empty)].order ||
                (def_.containers[i].order == def_.containers[static_cast<std::size_t>(empty)].order &&
                 def_.containers[i].channel < def_.containers[static_cast<std::size_t>(empty)].channel)) {
                empty = static_cast<int>(i);
            }
        }
        if (empty < 0) {
            reload_progress_s_ = 0.0;
            return;
        }
        reloading_ = empty;
        reload_progress_s_ += dt;
        if (reload_progress_s_ >= def_.reload_s_per_missile - 1e-9) {
            loaded_[static_cast<std::size_t>(empty)] = true;
            reload_progress_s_ = 0.0;
            reloading_ = -1;
        }
    }

    static void Set(Equipment& eq, const std::string& input, bool value) {
        if (!input.empty()) eq.SetInput(input, value);
    }

    LauncherDef def_;
    std::vector<bool> loaded_;
    double last_launch_s_ = -1e9;
    int letter_pos_ = -1;
    int letter_changes_ = 0;
    double relock_until_s_ = -1e9;
    std::vector<double> emergency_end_s_;
    double last_t_ = -1.0;
    int reloading_ = -1;
    double reload_progress_s_ = 0.0;
    std::array<Channel, 2> channels_{};
};

}  // namespace simcore
