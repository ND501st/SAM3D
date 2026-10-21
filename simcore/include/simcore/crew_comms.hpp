// simcore/crew_comms.hpp — the R-123M radio sets and the R-124 intercom as the crew uses them (roadmap 7.8).
//
// The console's controls and subsystems (data/procedures/<system>.json, "comms" block) are the devices; this class reads
// them once per step and tells the rest of the sim what they mean:
//   * R-123M, each set: it hears only when it is switched on, has warmed up (the subsystem: 10 min), and its volume is up.
//     The command network is told per set (C2Network::SetSetReceiving), so a set that is off or still warming up takes
//     the commander off the net (what is said meanwhile is lost) and a silent source on it falls back to the other set.
//   * Squelch and antenna: the published ranges (4 m whip 20 km with the noise suppressor off and 13 km with it on, 1 m
//     whip 8 km, the emergency wire 4 km) become a loss in the receiving chain, 40 log10 of the range ratio (the EM layer's
//     plane-earth law), applied to the set the headset is on. It reaches the EM link budget (em::NodeState::rx_extra_loss_db),
//     so with the EM layer's radio sources a shorter antenna or the suppressor really shortens the net.
//   * R-124: the commander's box A-1 on the radio (Р-123) puts him on the net; on the intercom (ВС) he does not hear it;
//     off (ВЫКЛ) he shouts his orders and the designations he passes on. A crew member whose own box is on the radio
//     (the deputy's A-2 on Р-123, the driver's ПВ on РС) does not hear orders on the intercom either. A shouted order takes
//     shout_quiet_s, or shout_noisy_s while the turbine or the take-off generator runs (SAM3D estimates).
// It only reads the console and sets what the C2 network and the link model expose; it decides nothing about the battle.
#pragma once

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "simcore/c2_sources.hpp"
#include "simcore/console_data.hpp"

namespace simcore {

class CrewComms {
public:
    // Something the debrief should say: who, and what.
    struct Event {
        std::string actor;
        std::string subject;
        std::string text;
    };

    explicit CrewComms(const CommsDef& def) : def_(def) {}

    // Receive loss of a set's chain against the reference (4 m whip, suppressor off): the squelch and the antenna.
    static double RangeLossDb(double reference_km, double range_km) {
        return range_km > 0.0 && reference_km > 0.0 ? 40.0 * std::log10(reference_km / range_km) : 0.0;
    }

    double ReceiveLossDb(int set, const ConsoleData& console) const {
        if (set < 0 || set >= static_cast<int>(def_.radios.size())) return 0.0;
        const CommsRadioDef& r = def_.radios[static_cast<std::size_t>(set)];
        double loss = 0.0;
        switch (console.panel.Get(r.antenna)) {
            case 1: loss += RangeLossDb(def_.range_whip_4m_km, def_.range_whip_1m_km); break;
            case 2: loss += RangeLossDb(def_.range_whip_4m_km, def_.range_wire_km); break;
            default: break;
        }
        if (console.panel.Get(r.squelch) == 1) loss += RangeLossDb(def_.range_whip_4m_km, def_.range_whip_4m_squelch_km);
        return loss + tuning_[static_cast<std::size_t>(set)].loss_db;
    }

    // What the set's tuning means now (radio_setup.hpp; the runner tells it each step): on its net's frequency or not
    // (`why` then says what it is on), and the antenna mismatch's loss.
    void SetTuning(int set, bool on_frequency, const std::string& why, double loss_db) {
        if (set < 0 || set > 1) return;
        tuning_[static_cast<std::size_t>(set)] = {on_frequency, why, loss_db};
    }

    // Is set `set` keyed now: the commander's chest switch at ПРД with his A-1 on Р-123 and his headset on that set
    // (`commander_set`), or for the left set the driver's chest switch with his ПВ on РС.
    bool Keyed(int set, int commander_set, const ConsoleData& console) const {
        if (ChestKeyed(console) && CommanderOnRadio(console) && set == commander_set) return true;
        return set == 0 && !def_.driver_chest.empty() && console.panel.Get(def_.driver_chest) == 1 &&
               console.panel.Get(def_.driver) == def_.driver_radio_position;
    }

    // Can this set hear? Switched on, warm, volume up. `why` says what is wrong.
    bool Receiving(int set, const ConsoleData& console, std::string* why = nullptr) const {
        if (set < 0 || set >= static_cast<int>(def_.radios.size())) return false;
        const CommsRadioDef& r = def_.radios[static_cast<std::size_t>(set)];
        const std::string name = "R-123M " + r.name;
        std::string reason;
        if (console.panel.Get(r.power) != 1) {
            reason = name + " is switched off";
        } else if (!console.equipment.IsReady(r.warm)) {
            reason = name + " is still warming up (10 min after switching on)";
        } else if (console.panel.Get(r.volume) != 1) {
            reason = name + " has its volume turned right down";
        } else if (!tuning_[static_cast<std::size_t>(set)].on_frequency) {
            reason = name + " " + tuning_[static_cast<std::size_t>(set)].why;
        }
        if (why != nullptr) *why = reason;
        return reason.empty();
    }

    bool Transmitting(int set, const ConsoleData& console) const {
        if (set < 0 || set >= static_cast<int>(def_.radios.size())) return false;
        const CommsRadioDef& r = def_.radios[static_cast<std::size_t>(set)];
        return console.panel.Get(r.power) == 1 && console.equipment.IsReady(r.transmit);
    }

    // Can the commander go on the air on this set (roadmap 7.8)? The set switched on and its transmitter ready (СИМПЛЕКС,
    // warmed up: 3 min after duty reception), and his A-1 on Р-123 with the intercom working, so his chest switch keys it
    // (r123m_training: the chest switch's tangent to ПРД transmits, released it receives). `why` says what is wrong.
    bool CanTransmit(int set, const ConsoleData& console, std::string* why = nullptr) const {
        std::string reason;
        if (set < 0 || set >= static_cast<int>(def_.radios.size())) {
            reason = "no such set";
        } else {
            const CommsRadioDef& r = def_.radios[static_cast<std::size_t>(set)];
            const std::string name = "R-123M " + r.name;
            if (console.panel.Get(r.power) != 1) {
                reason = name + " is switched off";
            } else if (!console.equipment.IsReady(r.warm)) {
                reason = name + " is still warming up (10 min after switching on)";
            } else if (!console.equipment.IsReady(r.transmit)) {
                reason = name + " is not ready to transmit (СИМПЛЕКС, 3 min after duty reception)";
            } else if (!tuning_[static_cast<std::size_t>(set)].on_frequency) {
                reason = name + " " + tuning_[static_cast<std::size_t>(set)].why;
            } else if (!CommanderOnRadio(console)) {
                reason = "the commander's A-1 is not on Р-123";
            }
        }
        if (why != nullptr) *why = reason;
        return reason.empty();
    }
    // The commander's chest switch is at ПРД (a player holds it there to talk; an NPC keys it as he speaks).
    bool ChestKeyed(const ConsoleData& console) const { return !def_.chest.empty() && console.panel.Get(def_.chest) == 1; }
    bool HasChest() const { return !def_.chest.empty(); }
    // The set whose ТОН. ВЫЗОВ is pressed this step (-1: none).
    int TonePressed(const ConsoleData& console) const {
        for (std::size_t i = 0; i < def_.radios.size(); ++i) {
            if (!def_.radios[i].tone.empty() && console.panel.Get(def_.radios[i].tone) == 1) return static_cast<int>(i);
        }
        return -1;
    }
    const std::string& SetName(int set) const {
        static const std::string none;
        return set >= 0 && set < static_cast<int>(def_.radios.size()) ? def_.radios[static_cast<std::size_t>(set)].name : none;
    }

    bool CommanderOnRadio(const ConsoleData& console) const {
        return console.panel.Get(def_.commander) == def_.commander_radio_position && console.equipment.IsReady(def_.intercom_subsystem);
    }
    bool CommanderOff(const ConsoleData& console) const { return console.panel.Get(def_.commander) == def_.commander_off_position; }

    // Does the seat have an R-124 subscriber box (is it on the intercom)? Every seat does when the data lists none.
    bool HasBox(const std::string& station) const {
        if (def_.subscribers.empty()) return true;
        for (const CommsDef::Subscriber& s : def_.subscribers) {
            if (s.station == station) return true;
        }
        return false;
    }
    // The box at a seat ("" if none).
    std::string BoxOf(const std::string& station) const {
        for (const CommsDef::Subscriber& s : def_.subscribers) {
            if (s.station == station) return s.box;
        }
        return std::string();
    }

    // The cabin is too loud to talk: the turbine or the take-off generator is running.
    bool Noisy(const ConsoleData& console) const {
        for (const std::string& id : def_.noisy) {
            if (console.equipment.IsReady(id)) return true;
        }
        return false;
    }
    double ShoutS(const ConsoleData& console) const { return Noisy(console) ? def_.shout_noisy_s : def_.shout_quiet_s; }

    // How long an order from the commander takes to reach `role`: 0 on the intercom; shouted when the commander's box is
    // off, or when the crew member's own box is on the radio.
    double OrderDelayS(const std::string& role, const ConsoleData& console) const {
        if (role == "commander") return 0.0;
        if (CommanderOff(console)) return ShoutS(console);
        if (!HasBox(role)) return ShoutS(console);  // no subscriber box at his seat: not on the intercom
        if (role == "deputy_commander" && console.panel.Get(def_.deputy) == def_.deputy_radio_position) return ShoutS(console);
        if (role == "driver" && console.panel.Get(def_.driver) == def_.driver_radio_position) return ShoutS(console);
        return 0.0;
    }

    // Extra time to pass a designation on to the crew: with the commander's box off he shouts it.
    double PassOnExtraS(const ConsoleData& console) const { return CommanderOff(console) ? ShoutS(console) : 0.0; }

    // Once per step: tell the network what each set and the commander's box mean now, and the link what the listening set's
    // antenna and squelch cost. Returns what changed, for the debrief.
    std::vector<Event> Step(const ConsoleData& console, C2Network* c2, C2LinkModel* link) {
        std::vector<Event> out;
        if (c2 != nullptr) {
            for (int i = 0; i < 2; ++i) {
                std::string why;
                const bool on = Receiving(i, console, &why);
                c2->SetSetReceiving(i, on, why);
            }
            const bool on_radio = CommanderOnRadio(console);
            c2->SetCommanderOnRadio(on_radio);
            c2->SetPassOnExtraS(PassOnExtraS(console));
            if (link != nullptr) link->SetReceiveLossDb(ReceiveLossDb(c2->ListeningSet(), console));
        }
        const bool off = CommanderOff(console);
        if (started_ && off != was_off_) {
            out.push_back({"Commander", "R-124", off ? "A-1 is off: orders and designations are shouted over the cabin noise"
                                                      : "A-1 is on again: orders go by the intercom"});
        }
        was_off_ = off;
        const bool noisy = Noisy(console);
        if (started_ && off && noisy != was_noisy_) {
            out.push_back({"Commander", "R-124", noisy ? "the turbine runs: shouting takes longer" : "the cabin is quieter: shouting is quicker"});
        }
        was_noisy_ = noisy;
        started_ = true;
        return out;
    }

private:
    struct Tuning {
        bool on_frequency = true;
        std::string why;
        double loss_db = 0.0;
    };
    CommsDef def_;
    std::array<Tuning, 2> tuning_{};
    bool started_ = false;
    bool was_off_ = false;
    bool was_noisy_ = false;
};

}  // namespace simcore
