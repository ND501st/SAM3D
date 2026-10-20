// simcore/voice_comms.hpp — the crew's headsets and intercom boxes: who hears what, and on what each seat talks
// (docs/crew/12_COMMUNICATIONS.md).
//
// A seat hears the intercom, a radio set or a telephone line only through its headset (шлемофон), worn and plugged into
// its box, with the box powered and turned to it. Without the headset it hears only the voices in the cabin (the
// PU-12M has no loudspeaker: flak11's signals schematic draws a headset at each station and none).
//
// The boxes (the console's "voice" block):
//   * The Osa's R-124 (r124_refs, r124_bmp2, r123m_training): A-1 the commander's (ВЫКЛ / ВС / Р-123), A-2 (ВС / Р-123),
//     the driver's ПВ (ВС / РС), and boxes with no switch for the others. A-2 or the ПВ on the radio leaves the intercom
//     (A-1 keeps it: its chest switch's ПРМ is receive and the intercom); ВЫЗ on any chest switch is a circular call that
//     reaches every box (SAM3D: an urgent callout is made with it). The intercom is powered while A-1 is not at ВЫКЛ.
//   * The PU-12M's PVS family (flak11, rwd_mb3): the RTK-1 (the commander's: a talk selector ВС / РСТ-1..4, a second set it
//     receives, five line levers, ЦИРКУЛЯР) and PVS-1/2/3 (ВС / РСТ-1..4). A box on a set still hears the intercom (one
//     shared intercom net, the R-145BM analogue, andryushchenko_2004: SAM3D's inference). The plotter has no box.
//
// This class only reads the console: whether a set itself hears its net (on, warm, on frequency, in reach, not jammed)
// is radio_nets.hpp's business; here a set is just a place a box can be turned to.
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"

namespace simcore {

class VoiceComms {
public:
    explicit VoiceComms(const VoiceDef& def) : def_(def), state_(def.boxes.size()) {}

    const VoiceDef& Def() const { return def_; }

    // Read the console: the headsets, the selectors, the line levers, the power. Call once a step before the questions.
    void Update(const ConsoleData& console) {
        powered_ = def_.power.empty() || console.equipment.IsReady(def_.power);
        for (std::size_t i = 0; i < def_.boxes.size(); ++i) {
            const VoiceBoxDef& b = def_.boxes[i];
            BoxState& s = state_[i];
            s.headset = console.panel.Get(b.headset) == 1;
            s.selected = b.selector.empty() ? std::string("intercom") : At(b.positions, console.panel.Get(b.selector));
            if (s.selected == "radio") s.selected = RadioOf(b, console);
            s.monitored = b.monitor.empty() ? std::string() : At(b.monitor_positions, console.panel.Get(b.monitor));
            if (s.monitored == "off") s.monitored.clear();
            s.lines.clear();
            for (std::size_t k = 0; k < b.lines.size(); ++k) {
                if (console.panel.Get(b.lines[k]) == 1) s.lines.push_back(static_cast<int>(k) + 1);
            }
            s.circular = !b.circular.empty() && console.panel.Get(b.circular) == 1;
            s.powered = b.power.empty() || console.equipment.IsReady(b.power);
        }
        phones_.clear();
        for (const VoicePhoneDef& ph : def_.phones) {
            if (console.panel.Get(ph.handset) == 1) phones_.push_back(ph.id);
        }
    }

    bool Powered() const { return powered_; }
    const VoiceBoxDef* BoxOf(const std::string& station) const {
        for (const VoiceBoxDef& b : def_.boxes) {
            if (b.station == station) return &b;
        }
        return nullptr;
    }
    bool Unboxed(const std::string& station) const {
        for (const std::string& s : def_.unboxed) {
            if (s == station) return true;
        }
        return false;
    }
    bool HeadsetOn(const std::string& station) const {
        const BoxState* s = StateOf(station);
        return s != nullptr && s->headset;
    }
    // The box's selector: "off", "intercom" or a set's id ("" : no box).
    std::string Selected(const std::string& station) const {
        const BoxState* s = StateOf(station);
        return s != nullptr ? s->selected : std::string();
    }
    // The second set the box receives ("" : none).
    std::string Monitored(const std::string& station) const {
        const BoxState* s = StateOf(station);
        return s != nullptr ? s->monitored : std::string();
    }
    // Lines raised on the seat's box (1-based).
    std::vector<int> LinesUp(const std::string& station) const {
        const BoxState* s = StateOf(station);
        return s != nullptr ? s->lines : std::vector<int>();
    }
    bool LineUp(const std::string& station, int line) const {
        for (int l : LinesUp(station)) {
            if (l == line) return true;
        }
        return false;
    }
    // Is the circular call held on any box (ЦИРКУЛЯР)?
    bool CircularHeld() const {
        for (const BoxState& s : state_) {
            if (s.circular) return true;
        }
        return false;
    }
    bool PhoneTaken(const std::string& id) const {
        for (const std::string& p : phones_) {
            if (p == id) return true;
        }
        return false;
    }

    // The seat can use its box at all: a box, the headset on, the boxes powered, not switched off.
    bool OnBox(const std::string& station) const {
        const BoxState* s = StateOf(station);
        return s != nullptr && s->headset && powered_ && s->powered && s->selected != "off";
    }
    // Hears the intercom: on the box, and the box on ВС (or on a set where boxes on a set still hear the intercom).
    bool HearsIntercom(const std::string& station) const {
        if (!OnBox(station)) return false;
        const std::string sel = Selected(station);
        return sel == "intercom" || def_.intercom_on_radio || BoxOf(station)->keeps_intercom;
    }
    // Hears a circular call (ВЫЗ, ЦИРКУЛЯР): any box with the headset on and the boxes powered.
    bool HearsCall(const std::string& station) const {
        const BoxState* s = StateOf(station);
        return s != nullptr && s->headset && powered_;
    }
    // Hears a set through the box: the selector or the second receiver on it.
    bool HearsSet(const std::string& station, const std::string& set) const {
        if (!OnBox(station) || set.empty()) return false;
        return Selected(station) == set || Monitored(station) == set;
    }
    // Talks on the intercom, or on a set (the selector on it: the RTK-1's talk selector).
    bool TalksOnIntercom(const std::string& station) const {
        return OnBox(station) && (Selected(station) == "intercom" || BoxOf(station)->keeps_intercom);
    }
    bool TalksOnSet(const std::string& station, const std::string& set) const { return OnBox(station) && Selected(station) == set; }
    // Hears a line: the headset on and the line's lever up on its box.
    bool HearsLine(const std::string& station, int line) const { return HeadsetOn(station) && powered_ && LineUp(station, line); }

    // The seats whose boxes put them on a set now.
    std::vector<std::string> SetListeners(const std::string& set) const {
        std::vector<std::string> out;
        for (const VoiceBoxDef& b : def_.boxes) {
            if (HearsSet(b.station, set)) out.push_back(b.station);
        }
        return out;
    }

    // Why a seat cannot talk on `channel` ("intercom" or a set's id): "" if it can.
    std::string WhyNot(const std::string& station, const std::string& channel) const {
        const VoiceBoxDef* b = BoxOf(station);
        if (b == nullptr) return "no intercom box at this seat";
        if (!HeadsetOn(station)) return "the headset is not on";
        if (!powered_) return std::string("the ") + def_.system + " is not powered";
        if (!StateOf(station)->powered) return b->name + " is not switched on";
        const std::string sel = Selected(station);
        if (sel == "off") return b->name + " is switched off";
        if (sel != channel) return b->name + " is on " + Name(sel) + ", not " + Name(channel);
        return std::string();
    }

    // The control and position that turn the seat's box to `channel` (talking). False if the box cannot.
    bool PositionFor(const std::string& station, const std::string& channel, std::string& control, int& position) const {
        const VoiceBoxDef* b = BoxOf(station);
        if (b == nullptr || b->selector.empty()) return false;
        for (std::size_t i = 0; i < b->positions.size(); ++i) {
            const std::string& c = b->positions[i];
            if (c == channel || (c == "radio" && (b->radio == channel || HasSelectable(*b, channel)))) {
                control = b->selector;
                position = static_cast<int>(i);
                return true;
            }
        }
        return false;
    }
    // The second receiver's control and position for a set (the RTK-1).
    bool MonitorFor(const std::string& station, const std::string& set, std::string& control, int& position) const {
        const VoiceBoxDef* b = BoxOf(station);
        if (b == nullptr || b->monitor.empty()) return false;
        for (std::size_t i = 0; i < b->monitor_positions.size(); ++i) {
            if (b->monitor_positions[i] == set) {
                control = b->monitor;
                position = static_cast<int>(i);
                return true;
            }
        }
        return false;
    }

    // A set's name for the log ("intercom" and "off" as words).
    std::string Name(const std::string& channel) const {
        if (channel == "intercom") return "the intercom (ВС)";
        if (channel == "off") return "off";
        for (const VoiceSetDef& s : def_.sets) {
            if (s.id == channel) return s.name;
        }
        return channel;
    }
    const VoiceSetDef* FindSet(const std::string& id) const {
        for (const VoiceSetDef& s : def_.sets) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }

private:
    struct BoxState {
        bool headset = false;
        std::string selected = "intercom";
        std::string monitored;
        std::vector<int> lines;
        bool circular = false;
        bool powered = true;
    };

    static std::string At(const std::vector<std::string>& v, int i) {
        return i >= 0 && static_cast<std::size_t>(i) < v.size() ? v[static_cast<std::size_t>(i)] : std::string("off");
    }
    static std::string RadioOf(const VoiceBoxDef& b, const ConsoleData& console) {
        if (!b.radio_select.empty()) return At(b.radio_select_sets, console.panel.Get(b.radio_select));
        return b.radio;
    }
    static bool HasSelectable(const VoiceBoxDef& b, const std::string& set) {
        for (const std::string& s : b.radio_select_sets) {
            if (s == set) return true;
        }
        return false;
    }
    const BoxState* StateOf(const std::string& station) const {
        for (std::size_t i = 0; i < def_.boxes.size(); ++i) {
            if (def_.boxes[i].station == station) return &state_[i];
        }
        return nullptr;
    }

    VoiceDef def_;
    std::vector<BoxState> state_;
    std::vector<std::string> phones_;
    bool powered_ = false;
};

}  // namespace simcore
