// simcore/em/knowledge_system.hpp — a node's own sensors feed its knowledge base.
//
// Each node (a vehicle, a command post, an aircraft) owns the sensors cabled to it. Their outputs
// become knowledge items with an origin (node, sensor, item):
//   * radar tracks (confirmed only): position, velocity, covariance, IFF replies, a kinematic guess;
//   * ESM contacts: bearing-only items carrying their lines of bearing and emitter signature;
//   * jamming strobes: bearing-only items tagged as a noise jammer.
// Own-sensor items are replaced every cycle and removed when the sensor drops them; items heard
// from the network age out on their own (knowledge.hpp).
#pragma once

#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "simcore/em/knowledge.hpp"
#include "simcore/em/model.hpp"

namespace simcore {
namespace em {

constexpr int kStrobeItemBase = 100000;

// A crude classification from speed: what an operator would guess from the track alone.
inline void ClassifyByKinematics(KnowledgeTrack& k) {
    if (!k.velocity_valid) return;
    const double v = k.velocity.Length();
    if (v < 8.0) {
        k.type_guess = "hovering or very slow (helicopter?)";
        k.type_confidence = 0.35;
    } else if (v < 90.0) {
        k.type_guess = "slow (helicopter or light aircraft)";
        k.type_confidence = 0.35;
    } else if (v > 650.0) {
        k.type_guess = "very fast (missile?)";
        k.type_confidence = 0.4;
    } else {
        k.type_guess = "fixed-wing";
        k.type_confidence = 0.45;
    }
}

inline KnowledgeTrack FromSensorTrack(const NodeState& n, const RadarState& r, const SensorTrack& t,
                                      const TrackerParams& prm, double now) {
    KnowledgeTrack k;
    k.origin = {n.id, r.id, t.id};
    k.source = InfoSource::OwnRadar;
    k.category = Category::Air;
    k.position_valid = true;
    k.position = t.Position();
    k.cov = t.PositionCov();
    k.velocity_valid = t.updates >= 2;
    k.velocity = t.Velocity();
    k.velocity_sigma_mps = std::sqrt((t.p(3, 3) + t.p(4, 4) + t.p(5, 5)) / 3.0);
    k.time_s = t.time_s;
    k.received_s = now;
    k.quality = TrackQuality(t, prm);
    if (t.iff_replies > 0 && now - t.last_iff_s < 30.0) {
        k.identity = Identity::Friend;
        k.identity_confidence = t.iff_replies >= 2 ? 0.9 : 0.7;
    }
    ClassifyByKinematics(k);
    k.debug_truth_platform = t.debug_truth_platform;
    return k;
}

// TDOA groups write their fixes straight into their node's knowledge; those age out normally.
inline bool IsGroupOrigin(const EmScene& s, int sensor_id) {
    for (const TdoaGroup& g : s.tdoa_groups)
        if (g.id == sensor_id) return true;
    return false;
}

inline void IngestOwnSensors(EmScene& s, NodeState& n) {
    if (!n.alive) return;
    std::set<long long> live;
    for (int sid : n.sensors) {
        if (RadarState* r = s.FindRadar(sid)) {
            const RadarDef& d = s.lib.radars[static_cast<std::size_t>(r->def_index)];
            r->tracker.PredictAll(s.time_s);
            for (const SensorTrack& t : r->tracker.Tracks()) {
                if (t.status != TrackStatus::Confirmed) continue;
                KnowledgeTrack k = FromSensorTrack(n, *r, t, d.tracker, s.time_s);
                live.insert(k.origin.Key());
                n.kb.Upsert(k);
            }
            for (const StrobeTrack& st : r->strobe_tracks) {
                if (st.lines.empty()) continue;
                KnowledgeTrack k;
                k.origin = {n.id, r->id, kStrobeItemBase + st.id};
                k.source = InfoSource::OwnStrobe;
                k.category = Category::Emitter;
                k.bearings = st.lines;
                k.time_s = st.last_s;
                k.received_s = s.time_s;
                k.has_emitter = true;
                k.emitter.library_id = "noise_jammer";
                k.emitter.library_name = "noise jammer";
                k.emitter.library_confidence = 0.9;
                k.emitter.freq_hz = 0.0;  // a strobe gives no usable carrier measurement
                k.emitter.hostile = true;
                k.identity = Identity::Hostile;
                k.identity_confidence = 0.8;
                live.insert(k.origin.Key());
                n.kb.Upsert(k);
            }
        }
        if (EsmState* e = s.FindEsm(sid)) {
            const Platform* rx = s.FindPlatform(e->platform_id);
            for (const EsmContact& c : e->contacts) {
                if (c.intercepts < 2) continue;
                KnowledgeTrack k;
                k.origin = {n.id, e->id, c.id};
                k.source = InfoSource::OwnEsm;
                k.category = Category::Emitter;
                k.bearings = c.lines;
                if (k.bearings.empty() && rx != nullptr)
                    k.bearings.push_back({rx->position, c.bearing_rad, c.bearing_sigma_rad, c.last_seen_s});
                k.time_s = c.last_seen_s;
                k.received_s = s.time_s;
                k.has_emitter = true;
                k.emitter = c.sig;
                if (!c.sig.library_id.empty()) {
                    k.type_guess = c.sig.library_name;
                    k.type_confidence = c.sig.library_confidence;
                    if (c.sig.hostile) {
                        k.identity = Identity::Hostile;
                        k.identity_confidence = 0.8 * c.sig.library_confidence;
                    } else {
                        k.identity = Identity::AssumedFriend;
                        k.identity_confidence = 0.5 * c.sig.library_confidence;
                    }
                }
                k.debug_truth_platform = 0;
                if (const EmitterState* te = s.FindEmitter(c.debug_truth_emitter)) k.debug_truth_platform = te->platform_id;
                live.insert(k.origin.Key());
                n.kb.Upsert(k);
            }
        }
    }
    // Own-sensor items the sensors no longer hold are gone (the operator saw them dropped).
    const int node_id = n.id;
    n.kb.RemoveIf([&](const KnowledgeTrack& k) {
        return k.origin.node == node_id && k.via_node == 0 && k.source != InfoSource::Network &&
               k.origin.sensor != 0 && live.count(k.origin.Key()) == 0 && !IsGroupOrigin(s, k.origin.sensor);
    });
}

}  // namespace em
}  // namespace simcore
