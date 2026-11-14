// simcore/crew_audio.hpp — what the crew's headsets carry now (roadmap 7.7), for the presentation layer to play.
//
// The commander's headset, through his R-124 box A-1 on Р-123, carries the set it is on:
//   * An FM receiver hisses with nothing on its frequency. The noise suppressor (ШУМЫ) shuts the hiss off until a
//     carrier comes (r123m_training: ШУМЫ against the clock to the stop is the suppressor off, set later to the noise),
//     and a carrier quietens it (the capture of an FM receiver). Off, still warming up, or turned right down, the set is
//     silent; on the intercom only (ВС) he hears no set.
//   * A carrier on the net: someone talking on it (a report on the battery net, the vehicle's own transmissions, which
//     the R-124 sidetone puts in his own ears) or a designation coming in.
//   * The call tone while ТОН. ВЫЗОВ is pressed (1 s, the runner's).
// The voices themselves are the callouts (CalloutBus::SpeakingNow) and the designations heard
// (ScenarioRunner::DesignationsHeard). Levels are 0..1, SAM3D's mix: hiss 0.35 open, 0.05 under a carrier, each times
// the set's ГРОМКОСТЬ (`volume`, which the presentation applies to the set's voices too).
// A box with its own ГРОМКОСТЬ (the A-1's knob) sets the level of everything its seat hears in the headset, intercom and
// set alike (HeadsetLevel).
#pragma once

#include "simcore/scenario_runner.hpp"

namespace simcore {

struct HeadsetSound {
    bool on_radio = false;   // the commander's headset is on a set (A-1 on Р-123) and the set works
    double hiss = 0.0;       // the set's noise, 0..1
    bool carrier = false;    // something is on the net now
    bool tone = false;       // the call tone
    bool squelched = false;  // the noise suppressor is on
    double volume = 1.0;     // the set's ГРОМКОСТЬ, 0..1: the hiss and the voices on the set are heard at this level
    double box_level = 1.0;  // the A-1's own ГРОМКОСТЬ (HeadsetLevel), already in `hiss`
};

// The level a seat's box puts its headset at, 0..1: the box's ГРОМКОСТЬ (`volume`) while the headset is on, else 1
// (no box, no knob, or the headset off: what he hears comes through the air).
inline double HeadsetLevel(const ConsoleData& c, const std::string& station) {
    if (!c.has_voice) return 1.0;
    for (const VoiceBoxDef& b : c.voice.boxes) {
        if (b.station != station) continue;
        if (b.volume.empty() || c.panel.Get(b.headset) != 1) return 1.0;
        return std::clamp(VolumeLevel(c.panel, b.volume), 0.0, 1.0);
    }
    return 1.0;
}

inline HeadsetSound HeadsetSoundNow(const ScenarioRunner& r) {
    HeadsetSound out;
    const CrewComms* comms = r.Comms();
    const ConsoleData& c = r.Console();
    if (comms == nullptr || !c.has_comms) return out;
    const int set = r.C2() != nullptr ? r.C2()->ListeningSet() : 1;
    if (set < 0 || set >= static_cast<int>(c.comms.radios.size())) return out;
    const CommsRadioDef& radio = c.comms.radios[static_cast<std::size_t>(set)];
    const bool works = c.panel.Get(radio.power) == 1 && c.equipment.IsReady(radio.warm) && VolumeUp(c.panel, radio.volume);
    out.on_radio = works && comms->CommanderOnRadio(c);
    if (!out.on_radio) return out;
    out.volume = VolumeLevel(c.panel, radio.volume);
    out.squelched = c.panel.Get(radio.squelch) == 1;
    const double now = r.Time();
    if (const CalloutBus* bus = r.Callouts()) {
        for (const SpokenCallout& s : bus->SpeakingNow()) out.carrier = out.carrier || s.channel != "intercom";
    }
    out.carrier = out.carrier || now - r.LastTransmissionS() < 1.0;
    if (!comms->Receiving(set, c)) out.carrier = false;  // off its net's frequency (or tuning) it hears nobody
    out.tone = now - r.LastToneCallS() < 1.0;
    if (out.carrier) {
        out.hiss = 0.05;
    } else {
        out.hiss = out.squelched ? 0.0 : 0.35;
    }
    out.hiss *= out.volume;
    // and the commander's box's own ГРОМКОСТЬ (the A-1's knob), as for every line in his headset
    if (c.has_voice) {
        for (const VoiceBoxDef& b : c.voice.boxes) {
            if (!c.comms.commander.empty() && b.selector == c.comms.commander) out.box_level = HeadsetLevel(c, b.station);
        }
    }
    out.hiss *= out.box_level;
    return out;
}

}  // namespace simcore
