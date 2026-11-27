// simcore/crew_audio.hpp — what the crew's headsets carry now (roadmap 7.7), for the presentation layer to play.
//
// A seat's headset carries a set only while his box is turned to it (VoiceComms::HearsSet: the commander's A-1 on
// Р-123, A-2 or the driver's ПВ on the radio). A box on the intercom, a box with no switch (the Osa's A-3 and A-4), a
// headset off or a man out of the vehicle carries none: the player hears the hiss only at a seat that is on a set.
// The headset on a set carries:
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
    bool on_radio = false;   // the seat's headset is on a set and the set works
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

// What `station`'s headset carries now. Without the console's voice block (its boxes) only the commander's A-1 reaches
// a set, the one the network listens on.
inline HeadsetSound HeadsetSoundNow(const ScenarioRunner& r, const std::string& station) {
    HeadsetSound out;
    const CrewComms* comms = r.Comms();
    const ConsoleData& c = r.Console();
    if (comms == nullptr || !c.has_comms) return out;
    int set = -1;
    if (const VoiceComms* voice = r.Voice()) {
        if (r.Outside(station)) return out;
        for (std::size_t i = 0; i < c.comms.radios.size() && set < 0; ++i) {
            if (voice->HearsSet(station, c.comms.radios[i].name)) set = static_cast<int>(i);
        }
    } else if (station == r.Cmdr() && comms->CommanderOnRadio(c)) {
        set = r.C2() != nullptr ? r.C2()->ListeningSet() : 1;
    }
    if (set < 0 || set >= static_cast<int>(c.comms.radios.size())) return out;
    const CommsRadioDef& radio = c.comms.radios[static_cast<std::size_t>(set)];
    out.on_radio = c.panel.Get(radio.power) == 1 && c.equipment.IsReady(radio.warm) && VolumeUp(c.panel, radio.volume);
    if (!out.on_radio) return out;
    out.volume = VolumeLevel(c.panel, radio.volume);
    out.squelched = c.panel.Get(radio.squelch) == 1;
    const double now = r.Time();
    if (const CalloutBus* bus = r.Callouts()) {
        for (const SpokenCallout& s : bus->SpeakingNow()) out.carrier = out.carrier || (s.channel != "intercom" && s.channel.compare(0, 5, "cabin") != 0);
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
    // and the seat's box's own ГРОМКОСТЬ (the A-1's knob), as for every line in his headset
    out.box_level = HeadsetLevel(c, station);
    out.hiss *= out.box_level;
    return out;
}

// The commander's headset (the seat the network's set is listened on without the console's boxes).
inline HeadsetSound HeadsetSoundNow(const ScenarioRunner& r) { return HeadsetSoundNow(r, r.Cmdr()); }

}  // namespace simcore
