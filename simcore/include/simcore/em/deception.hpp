// simcore/em/deception.hpp — deception and EMCON depth (roadmap E7).
//
// What the defender does to the enemy's picture, and what an enemy repeater does to the defender's:
//   * Decoy emitters on the same parameters as a real radar (DecoyLink): ESM identifies them as the real thing, and
//     bearings to the radar and to its decoys carry one signature, so a direction-finding net can fix the emitter where
//     neither is (a ghost fix), or fix the decoy. Modes: always on, on with the radar, on while the radar is off.
//   * Frequency changes against a spot jammer: a radar moved to another of its channels leaves the jammer's band until
//     the jammer notices and re-tunes (JammerDef::retune_s, 0 = never).
//   * Dummy radio traffic (DummyTraffic): an empty voice message on a schedule. Every transmission radiates, so radio
//     intercept hears and direction-finds it like real traffic; the real battery's own voice designations are what
//     give it away, which is the intercept value of voice.
//   * False-target repeaters on the attacker's side are in radar_system.hpp (AddFalseTargetPlots).
// Velocity-gate pull-off is not modelled: none of the radars here track in Doppler (the Osa's tracking radar is a
// pulse radar with MTI, mon_2112_80 p15-17), so there is no velocity gate to pull.
#pragma once

#include <cmath>
#include <string>

#include "simcore/em/comms_system.hpp"
#include "simcore/em/model.hpp"

namespace simcore {
namespace em {

inline const char* DecoyModeName(DecoyMode m) {
    switch (m) {
        case DecoyMode::Always: return "always";
        case DecoyMode::WithRadar: return "with_radar";
        case DecoyMode::WhenRadarOff: return "when_radar_off";
    }
    return "?";
}

inline bool ParseDecoyMode(const std::string& text, DecoyMode& out) {
    for (DecoyMode m : {DecoyMode::Always, DecoyMode::WithRadar, DecoyMode::WhenRadarOff}) {
        if (text == DecoyModeName(m)) {
            out = m;
            return true;
        }
    }
    return false;
}

// Decoys radiate by their mode and follow their radar's frequency (a channel change moves the decoy with it).
inline void UpdateDecoys(EmScene& s) {
    for (const DecoyLink& d : s.decoys) {
        EmitterState* e = s.FindEmitter(d.emitter_id);
        if (e == nullptr) continue;
        const Platform* dp = s.FindPlatform(e->platform_id);
        const RadarState* r = s.FindRadar(d.radar_id);
        const Platform* rp = r != nullptr ? s.FindPlatform(r->platform_id) : nullptr;
        const bool radar_on = r != nullptr && r->on && rp != nullptr && rp->alive;
        bool on = d.mode == DecoyMode::Always || (d.mode == DecoyMode::WithRadar ? radar_on : !radar_on);
        if (dp == nullptr || !dp->alive) on = false;
        if (on != e->radiating) {
            s.Log(EmEventKind::Info, e->id, d.radar_id, std::string("decoy emitter ") + (on ? "on" : "off"));
            e->radiating = on;
        }
        if (r != nullptr) {
            if (const EmitterState* re = s.FindEmitter(r->emitter_id)) e->freq_hz = re->freq_hz;
        }
    }
}

// A spot jammer set on a radar follows it to a new frequency after its retune time.
inline void UpdateJammerTuning(EmScene& s) {
    for (JammerState& j : s.jammers) {
        const JammerDef& jd = s.lib.jammers[static_cast<std::size_t>(j.def_index)];
        if (jd.technique != JamTechnique::Spot || jd.retune_s <= 0.0 || j.victim_radar == 0 || !j.on) continue;
        EmitterState* je = s.FindEmitter(j.emitter_id);
        const RadarState* r = s.FindRadar(j.victim_radar);
        const EmitterState* re = r != nullptr ? s.FindEmitter(r->emitter_id) : nullptr;
        if (je == nullptr || re == nullptr || !r->on) continue;
        if (std::fabs(je->freq_hz - re->freq_hz) <= 0.25 * jd.emitter.waveform.bandwidth_hz) {
            j.retune_since_s = -1.0;
            continue;
        }
        if (j.retune_since_s < 0.0) {
            j.retune_since_s = s.time_s;
        } else if (s.time_s - j.retune_since_s >= jd.retune_s) {
            je->freq_hz = re->freq_hz;
            j.retune_since_s = -1.0;
            s.Log(EmEventKind::Info, j.id, r->id, "spot jammer re-tunes onto the radar's new frequency");
        }
    }
}

// Empty voice messages on a schedule (the net must be free; otherwise next tick).
inline void RunDummyTraffic(EmScene& s) {
    for (DummyTraffic& d : s.dummy_traffic) {
        if (s.time_s + 1e-9 < d.next_s || s.time_s > d.until_s) continue;
        NodeState* n = s.FindNode(d.node_id);
        NetState* net = s.FindNet(d.net_id);
        if (n == nullptr || net == nullptr || !n->alive) continue;
        if (net->busy_until_s > s.time_s) continue;
        const double start = s.time_s + 0.5;
        SendVoice(s, *net, *n, 0, start, start + d.duration_s);
        d.next_s = s.time_s + d.period_s;
    }
}

}  // namespace em
}  // namespace simcore
