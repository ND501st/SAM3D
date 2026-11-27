// simcore/em/comms_intercept.hpp — comms intercept receivers hearing a call as it starts (roadmap B9,
// docs/air_battle/09_COMMS_AND_INTERCEPT.md section 4.2 and 4.5).
//
// A receiver of kind CommsIntercept (an EsmDef with its band, sensitivity and bearing error) hears a message when the one-way
// signal at its antenna, through the same terrain, ground and gas losses as every radio link (LinkLossCached), clears its
// sensitivity. It is asked once per message and receiver, at the message's start, not on the ESM tick: a busy raid makes some
// tens of calls a minute and a few posts, so a few hundred cached link budgets a minute. What it hears is the time, the
// frequency, a bearing with the receiver's error and the length; what the call says stays behind `Message::payload`, read only
// through the post's linguist (command/intercept.hpp). The 4/3-earth horizon of an airborne transmitter comes out of the path
// profile, as for the radar and the ESM.
//
// No random stream is new: the bearing error is a keyed draw of Stream::EsmBearing on (receiver, message), a key no ESM sweep
// uses, so no other draw moves.
#pragma once

#include <cmath>
#include <vector>

#include "simcore/em/detection.hpp"
#include "simcore/em/geolocation.hpp"
#include "simcore/em/model.hpp"
#include "simcore/em/scene_ops.hpp"

namespace simcore {
namespace em {

inline constexpr std::uint64_t kInterceptKey = 0xB9;
// HF is carried by the sky wave past any horizon (fm_100_2_1_1984 p. 15-2, the Pact's reach as NATO saw it): below 30 MHz the
// path is free space plus this loss (a game value), whatever the ground between.
inline constexpr double kSkyWaveFreqHz = 30e6;
inline constexpr double kSkyWaveLossDb = 25.0;

// The transmitting radio's effective radiated power towards a receiver: its power times its antenna gain (an omni whip or
// blade, a game value in the library), without the radio's own system loss (a receiver of ours does not share it).
inline double CallEirpW(const RadioDef& rd) { return rd.power_w * DbToLinear(rd.antenna_gain_db); }

// Ask every comms intercept receiver of a message that has just started. Returns how many heard it.
inline int InterceptMessage(EmScene& s, Message& m) {
    m.intercept_done = true;
    const NodeState* from = s.FindNode(m.from_node);
    const NetState* net = s.FindNet(m.net);
    if (from == nullptr || net == nullptr || net->radio_def < 0 || !from->alive) return 0;
    const Platform* tx = s.FindPlatform(from->platform_id);
    if (tx == nullptr || !tx->alive) return 0;
    const RadioDef& rd = s.lib.radios[static_cast<std::size_t>(net->radio_def)];
    const double f = net->freq_hz;
    const double eirp = CallEirpW(rd);
    int heard = 0;
    for (EsmState& esm : s.esm) {
        if (!esm.on || esm.def_index < 0) continue;
        const EsmDef& def = s.lib.esm[static_cast<std::size_t>(esm.def_index)];
        if (def.kind != EsmKind::CommsIntercept || f < def.freq_min_hz || f > def.freq_max_hz) continue;
        const Platform* rx = s.FindPlatform(esm.platform_id);
        if (rx == nullptr || !rx->alive || rx->id == tx->id) continue;
        const double range = (rx->position - tx->position).Length();
        // The cheap bound first: free space alone could not carry it.
        if (OneWayReceivedPowerW(eirp, def.antenna_gain_db, f, range, 0.0) < DbToLinear(def.sensitivity_dbw)) continue;
        const bool sky = f < kSkyWaveFreqHz;
        LinkLossResult loss;
        if (!sky) loss = LinkLossCached(s, esm.paths, *tx, *rx, f);
        // Line of sight dominates in the air bands (09 section 4.2): over the 4/3-earth horizon a call is not read, whatever
        // the diffraction formula leaves of it (a game rule: an AM voice call 25 km past the horizon is lost in the noise).
        if (!sky && !loss.line_of_sight) continue;
        const double pr = OneWayReceivedPowerW(eirp, def.antenna_gain_db, f, range, sky ? kSkyWaveLossDb : loss.TotalOneWayDb());
        const double rx_dbw = pr > 0.0 ? LinearToDb(pr) : -300.0;
        const double margin = rx_dbw - def.sensitivity_dbw;
        if (margin < 0.0) continue;
        const std::uint64_t key = DrawHash(0, Stream::EsmBearing, Key(esm.id), Key(m.id), kInterceptKey);
        CommsHeard h;
        h.message = m.id;
        h.esm = esm.id;
        h.platform = rx->id;
        h.from_node = m.from_node;
        h.net = m.net;
        h.payload = m.payload;
        h.start_s = m.start_s;
        h.end_s = m.end_s;
        h.freq_hz = f;
        h.rx_dbw = rx_dbw;
        h.margin_db = margin;
        h.line_of_sight = sky || loss.line_of_sight;
        const double sigma = def.bearing_sigma_rad * std::sqrt(1.0 + 1.0 / DbToLinear(margin));
        h.bearing_rad = WrapTwoPi(AzimuthFromTo(rx->position, tx->position) + sigma * s.rng.NormalOf(Stream::EsmBearing, key));
        s.heard.push_back(h);
        ++heard;
    }
    return heard;
}

// Every message on the air whose start has come and which no receiver has been asked about yet.
inline void RunCommsIntercept(EmScene& s) {
    if (s.esm.empty()) return;
    for (Message& m : s.messages) {
        if (m.done || m.intercept_done || m.start_s > s.time_s + 1e-9) continue;
        InterceptMessage(s, m);
    }
}

}  // namespace em
}  // namespace simcore
