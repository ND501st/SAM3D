// simcore/em/comms_system.hpp — radio nets, messages, relays and what losing a node does.
//
// Information moves between nodes only as messages over radio nets, so it is late, can be lost,
// and stops when a link or a node goes. The model (docs/EM_ARCHITECTURE.md, "Communications"):
//   * A net is a channel shared by its members (half duplex): one transmission at a time, so a
//     busy voice net queues reports and their latency grows with load.
//   * A link a -> b is up when the received power beats the receiver's sensitivity, raised by
//     noise jamming (J/N) and by co-site interference from the platform's own radars (the Polish
//     Osa-AK manual gives the R-123M at least 12 km with the vehicle's radars on, 15 km off).
//     Propagation is the same as for radar (terrain diffraction is mild at VHF, so radios work over
//     ridges that blind a radar) with the plane-earth law below 1 GHz.
//   * Delivery is decided when the transmission ends: chance = 1 / (1 + exp(-margin_dB / 2)).
//   * Voice transmissions carry an operator delay and take seconds per report; telecode is fast.
//   * Every transmission radiates: ESM and radio-intercept receivers can hear and DF it.
//   * Subordinates report their own tracks up; command posts broadcast their fused picture down;
//     relays pass messages between their nets. A node that hears nothing from its superior goes
//     Degraded, then Autonomous, and its network-sourced knowledge ages out.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "simcore/em/detection.hpp"
#include "simcore/em/knowledge.hpp"
#include "simcore/em/model.hpp"
#include "simcore/em/scene_ops.hpp"

namespace simcore {
namespace em {

inline bool PlatformRadarsRadiating(const EmScene& s, int platform_id) {
    for (const RadarState& r : s.radars)
        if (r.platform_id == platform_id && r.on) return true;
    return false;
}

// Received power (dBW) between two of these radios over flat open ground, without system loss.
inline double FlatGroundRxDbw(const RadioDef& rd, const Environment& env, double range_m, double height_m,
                              double freq_hz) {
    const FlatTerrain flat(0.0);
    const PathGeometry g = AnalysePath(flat, Vec3{0.0, 0.0, height_m}, height_m, Vec3{0.0, range_m, height_m}, height_m,
                                       env.profile_spacing_m, 600, env.k_factor);
    const LinkLossResult loss = LinkLossCore(env, g, height_m, height_m, height_m, height_m, range_m, freq_hz);
    const double eirp = rd.power_w * DbToLinear(rd.antenna_gain_db);
    return LinearToDb(OneWayReceivedPowerW(eirp, rd.antenna_gain_db, freq_hz, range_m, loss.TotalOneWayDb()));
}

// Fit the system loss (and co-site desense) to the radio's published range, as radars are fitted
// to their detection ranges. Without an anchor the file's values stand.
inline void CalibrateRadio(RadioDef& rd, const Environment& env) {
    if (rd.anchor_range_m <= 0.0 || rd.anchor_height_m <= 0.0) return;
    const double f = rd.anchor_freq_hz > 0.0 ? rd.anchor_freq_hz : 0.5 * (rd.freq_min_hz + rd.freq_max_hz);
    rd.system_loss_db = FlatGroundRxDbw(rd, env, rd.anchor_range_m, rd.anchor_height_m, f) - rd.sensitivity_dbw;
    if (rd.cosite_range_m > 0.0 && rd.cosite_range_m < rd.anchor_range_m)
        rd.cosite_desense_db = FlatGroundRxDbw(rd, env, rd.cosite_range_m, rd.anchor_height_m, f) -
                               FlatGroundRxDbw(rd, env, rd.anchor_range_m, rd.anchor_height_m, f);
}

inline LinkState EvaluateLink(EmScene& s, const NetState& net, const NodeState& a, const NodeState& b) {
    LinkState l;
    l.net = net.id;
    l.a = a.id;
    l.b = b.id;
    const Platform* pa = s.FindPlatform(a.platform_id);
    const Platform* pb = s.FindPlatform(b.platform_id);
    if (pa == nullptr || pb == nullptr || !a.alive || !b.alive || !pa->alive || !pb->alive || net.radio_def < 0) return l;
    const RadioDef& rd = s.lib.radios[static_cast<std::size_t>(net.radio_def)];
    const double f = net.freq_hz;
    const LinkLossResult loss = LinkLoss(s, *pa, *pb, f);
    ++s.stats.link_evaluations;
    l.line_of_sight = loss.line_of_sight;
    l.diffraction_db = loss.diffraction_db;
    const double eirp = rd.power_w * DbToLinear(rd.antenna_gain_db);
    const double pr = OneWayReceivedPowerW(eirp, rd.antenna_gain_db, f, loss.range_m, loss.TotalOneWayDb() + rd.system_loss_db);
    l.rx_dbw = pr > 0.0 ? LinearToDb(pr) : -300.0;
    const double cosite = PlatformRadarsRadiating(s, pb->id) ? rd.cosite_desense_db : 0.0;
    // Noise figure implied by the sensitivity and required SNR (for comparing jammer noise).
    const double noise_dbw = rd.sensitivity_dbw - rd.required_snr_db;
    const double nf_db = noise_dbw - LinearToDb(kNoiseDensityW_Hz * rd.bandwidth_hz);
    AntennaDef omni;
    omni.gain_db = rd.antenna_gain_db;
    const double jn = NoiseJammingToNoise(s, *pb, omni, 0.0, 0.0, f, rd.bandwidth_hz, nf_db, cosite);
    l.jn_db = jn > 0.0 ? LinearToDb(jn) : -300.0;
    const double eff_sens = rd.sensitivity_dbw + cosite + LinearToDb(1.0 + jn);
    l.margin_db = l.rx_dbw - eff_sens;
    l.up = l.margin_db >= -3.0;  // below -3 dB nothing gets through (chance < 18 %)
    return l;
}

inline const LinkState* FindLink(const EmScene& s, int net, int a, int b) {
    for (const LinkState& l : s.links)
        if (l.net == net && l.a == a && l.b == b) return &l;
    return nullptr;
}

inline void EvaluateLinks(EmScene& s) {
    s.links.clear();
    for (const NetState& net : s.nets)
        for (int ia : net.members)
            for (int ib : net.members) {
                if (ia == ib) continue;
                const NodeState* a = s.FindNode(ia);
                const NodeState* b = s.FindNode(ib);
                if (a == nullptr || b == nullptr) continue;
                s.links.push_back(EvaluateLink(s, net, *a, *b));
            }
}

inline bool OnNet(const NetState& net, int node_id) {
    for (int m : net.members)
        if (m == node_id) return true;
    return false;
}

// Queue a message on a net; it goes on the air when the channel is free.
inline void SendOnNet(EmScene& s, NetState& net, const NodeState& from, MessageKind kind, int to, int origin_node,
                      const std::vector<KnowledgeTrack>& tracks, int hops, long long reuse_id = 0) {
    if (net.radio_def < 0) return;
    const RadioDef& rd = s.lib.radios[static_cast<std::size_t>(net.radio_def)];
    Message m;
    m.id = reuse_id != 0 ? reuse_id : s.next_message_id++;
    m.kind = kind;
    m.net = net.id;
    m.from_node = from.id;
    m.origin_node = origin_node;
    m.to_node = to;
    m.queued_s = s.time_s;
    m.hops = hops;
    m.tracks = tracks;
    const int per = rd.tracks_per_message > 0 ? rd.tracks_per_message : 1;
    const int n = static_cast<int>(tracks.size());
    // "Nothing to report" is a short call; each report takes message_s per chunk of tracks.
    const double duration = n > 0 ? rd.message_s * static_cast<double>((n + per - 1) / per) : 0.3 * rd.message_s;
    m.start_s = MaxOf(s.time_s, net.busy_until_s) + (rd.voice ? rd.operator_delay_s : 0.5);
    m.end_s = m.start_s + duration;
    net.busy_until_s = m.end_s;
    s.messages.push_back(m);
    s.Log(EmEventKind::MessageSent, from.id, static_cast<int>(m.id),
          from.name + " -> " + (to == 0 ? std::string("all") : std::to_string(to)) + " on " + net.name + ": " +
              (kind == MessageKind::Picture ? "picture" : "report") + ", " + std::to_string(n) + " items");
}

// A newer report or picture replaces one from the same node still waiting for the channel (an
// operator reads out the latest plot, not a backlog). Returns true if it found one to refresh.
inline bool RefreshQueued(EmScene& s, int from, int net, MessageKind kind, const std::vector<KnowledgeTrack>& tracks) {
    for (Message& m : s.messages) {
        if (m.from_node != from || m.net != net || m.kind != kind || m.done || m.start_s <= s.time_s) continue;
        m.tracks = tracks;
        return true;
    }
    return false;
}

// Is this node already on the air (or queued) with this kind of message on this net?
inline bool OnAir(const EmScene& s, int from, int net, MessageKind kind) {
    for (const Message& m : s.messages)
        if (m.from_node == from && m.net == net && m.kind == kind && !m.done && m.start_s <= s.time_s && s.time_s < m.end_s)
            return true;
    return false;
}

// The tracks a node reports upward: what its own sensors hold (not what it heard).
inline std::vector<KnowledgeTrack> OwnReportItems(const NodeState& n) {
    std::vector<KnowledgeTrack> out;
    for (const KnowledgeTrack& k : n.kb.Entries())
        if (k.source != InfoSource::Network) out.push_back(k);
    return out;
}

// The picture a command post broadcasts: its fused view, each item re-originated at the post.
inline std::vector<KnowledgeTrack> PictureItems(const NodeState& n, double now) {
    std::vector<KnowledgeTrack> out;
    for (const FusedTrack& f : n.kb.Picture(now)) {
        KnowledgeTrack k = f.fused;
        k.origin = {n.id, 0, f.id};
        if (f.bearing_only) {
            k.position_valid = false;
        } else if (f.from_fix) {
            k.bearings.clear();
        }
        if (k.time_s < now && k.position_valid) k = PredictKnowledge(k, now, n.kb.Params());
        out.push_back(k);
    }
    return out;
}

inline void Deliver(EmScene& s, NodeState& b, const Message& m) {
    if (m.to_node != 0 && m.to_node != b.id) {
        // Not for us: a relay passes it on into its other nets.
        if (!b.relay) return;
    }
    if (std::find(b.forwarded.begin(), b.forwarded.end(), m.id) != b.forwarded.end()) return;
    const bool for_us = m.to_node == 0 || m.to_node == b.id;
    if (for_us) {
        for (const KnowledgeTrack& t : m.tracks) {
            if (t.origin.node == b.id) continue;  // our own item coming back
            KnowledgeTrack k = t;
            k.source = InfoSource::Network;
            k.via_node = m.from_node;
            k.hops = m.hops + 1;
            k.received_s = s.time_s;
            b.kb.Upsert(k);
        }
        if (m.kind == MessageKind::Picture && m.origin_node == b.parent) b.last_parent_contact_s = s.time_s;
    }
    if (b.relay) {
        b.forwarded.push_back(m.id);
        if (b.forwarded.size() > 200) b.forwarded.erase(b.forwarded.begin());
        for (int net_id : b.nets) {
            if (net_id == m.net) continue;
            NetState* net = s.FindNet(net_id);
            if (net == nullptr) continue;
            SendOnNet(s, *net, b, m.kind, m.to_node, m.origin_node, m.tracks, m.hops + 1, m.id);
        }
    }
}

inline void RunComms(EmScene& s, bool evaluate_links) {
    if (evaluate_links) EvaluateLinks(s);
    // Nodes produce reports and pictures on their schedules.
    for (NodeState& n : s.nodes) {
        if (!n.alive || n.nets.empty()) continue;
        if (n.parent != 0 && s.time_s >= n.next_report_s) {
            n.next_report_s = s.time_s + n.report_period_s;
            const std::vector<KnowledgeTrack> items = OwnReportItems(n);
            if (!items.empty()) {
                NetState* net = nullptr;
                const NodeState* parent = s.FindNode(n.parent);
                for (int id : n.nets) {
                    NetState* cand = s.FindNet(id);
                    if (cand != nullptr && parent != nullptr && OnNet(*cand, parent->id)) net = cand;
                }
                if (net == nullptr) net = s.FindNet(n.nets.front());
                if (net != nullptr && !RefreshQueued(s, n.id, net->id, MessageKind::TrackReport, items) &&
                    !OnAir(s, n.id, net->id, MessageKind::TrackReport))
                    SendOnNet(s, *net, n, MessageKind::TrackReport, n.parent, n.id, items, 0);
            }
        }
        if (n.c2_center && s.time_s >= n.next_picture_s) {
            n.next_picture_s = s.time_s + n.picture_period_s;
            const std::vector<KnowledgeTrack> items = PictureItems(n, s.time_s);
            for (int id : n.nets) {
                NetState* net = s.FindNet(id);
                if (net == nullptr) continue;
                if (RefreshQueued(s, n.id, net->id, MessageKind::Picture, items) || OnAir(s, n.id, net->id, MessageKind::Picture))
                    continue;
                SendOnNet(s, *net, n, MessageKind::Picture, 0, n.id, items, 0);
            }
        }
    }
    // Radios on the air radiate (ESM-visible).
    for (NodeState& n : s.nodes) {
        EmitterState* e = s.FindEmitter(n.radio_emitter_id);
        if (e == nullptr) continue;
        e->radiating = false;
        if (!n.alive) continue;
        for (const Message& m : s.messages) {
            if (m.from_node != n.id || m.done || s.time_s < m.start_s || s.time_s >= m.end_s) continue;
            e->radiating = true;
            if (const NetState* net = s.FindNet(m.net)) e->freq_hz = net->freq_hz;
        }
    }
    // Transmissions that ended this tick are delivered (or not).
    std::vector<Message> due;
    for (Message& m : s.messages) {
        if (!m.done && m.end_s <= s.time_s) {
            m.done = true;
            due.push_back(m);
        }
    }
    s.messages.erase(std::remove_if(s.messages.begin(), s.messages.end(), [](const Message& m) { return m.done; }),
                     s.messages.end());
    for (const Message& m : due) {
        const NodeState* from = s.FindNode(m.from_node);
        const NetState* net = s.FindNet(m.net);
        if (from == nullptr || net == nullptr || !from->alive) continue;
        for (int member : net->members) {
            if (member == m.from_node) continue;
            NodeState* b = s.FindNode(member);
            if (b == nullptr || !b->alive) continue;
            if (m.to_node != 0 && m.to_node != member && !b->relay) continue;
            const LinkState l = EvaluateLink(s, *net, *from, *b);
            const double p = l.up ? 1.0 / (1.0 + std::exp(-l.margin_db / 2.0)) : 0.0;
            const bool ok = s.rng.Uniform(Stream::CommsDelivery, Key(static_cast<long long>(m.id)), Key(member)) < p;
            if (ok) {
                s.Log(EmEventKind::MessageDelivered, member, static_cast<int>(m.id),
                      from->name + " -> " + b->name + " (" + std::to_string(static_cast<int>(l.margin_db)) + " dB margin, " +
                          std::to_string(static_cast<int>(s.time_s - m.queued_s + 0.5)) + " s after queuing)");
                Deliver(s, *b, m);
            } else if (m.to_node == member || m.to_node == 0) {
                s.Log(EmEventKind::MessageLost, member, static_cast<int>(m.id),
                      from->name + " -> " + b->name + " lost (" +
                          (l.up ? std::to_string(static_cast<int>(l.margin_db)) + " dB margin" : std::string("no link")) + ")");
            }
        }
    }
    // Command status from how recently the superior's picture arrived.
    for (NodeState& n : s.nodes) {
        if (!n.alive || n.parent == 0) continue;
        const NodeState* parent = s.FindNode(n.parent);
        const double period = parent != nullptr ? parent->picture_period_s : 30.0;
        const double since = s.time_s - n.last_parent_contact_s;
        const C2Status st = since <= 2.5 * period ? C2Status::Connected
                            : since <= 5.0 * period ? C2Status::Degraded
                                                    : C2Status::Autonomous;
        if (st != n.status) {
            s.Log(EmEventKind::C2StatusChange, n.id, n.parent,
                  n.name + ": " + C2StatusName(n.status) + " -> " + C2StatusName(st));
            n.status = st;
        }
    }
}

}  // namespace em
}  // namespace simcore
