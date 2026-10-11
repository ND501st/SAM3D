// simcore/launcher.hpp — the missile load and the two guidance channels (roadmap 6.3).
//
// Driven by the console's "launcher" block (console_data.hpp, LauncherDef):
//   * Six containers, each on a channel (I or II); each has an ammunition lamp, lit while a missile is in it.
//   * At ВНИМАНИЕ (or when preparation is started by hand) the launcher picks one missile for each channel the КАНАЛЫ selector allows, lowest order first
//     (the outer containers), and that channel's command transmitter comes on (ВКЛ ВЫС).
//   * A launch takes the selected missile of the first free channel in the selector's order. Its lamps then follow
//     the missile: СХОД, ЗАХВАТ (captured in the wide beam), СР.Л (medium beam), УЗКИЙ ЛУЧ (narrow beam), К3 (fuse
//     armed near the target, or with the narrow beam against a jamming target).
//   * The channel is busy until the shot is over (the outcome model says so, or the nominal flight time passes).
//   * Launches are at least min_interval_s apart; the hold input is set while the next one must wait.
//   * PairSpent(): no selected missile is left, so the next ones need preparing again.
// Deterministic; it reads the console and sets the console's inputs, nothing else.
#pragma once

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
            Set(eq, in.away_input, c.flying);
            Set(eq, in.captured_input, c.flying && since >= def_.capture_s);
            Set(eq, in.medium_input, c.flying && since >= def_.medium_s);
            Set(eq, in.narrow_input, narrow);
            Set(eq, in.fuse_input, c.flying && (near_target || (jamming && narrow)));
        }
    }

    // The channel the next launch would use (0 = none: no selected missile on a free channel, or too soon).
    int NextChannel(double t, const ConsoleData& console) const {
        if (t < last_launch_s_ + def_.min_interval_s) return 0;
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

    static void Set(Equipment& eq, const std::string& input, bool value) {
        if (!input.empty()) eq.SetInput(input, value);
    }

    LauncherDef def_;
    std::vector<bool> loaded_;
    double last_launch_s_ = -1e9;
    std::array<Channel, 2> channels_{};
};

}  // namespace simcore
