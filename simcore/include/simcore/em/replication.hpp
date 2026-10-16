// simcore/em/replication.hpp — multiplayer knowledge (roadmap E10): the server runs the one simulation, and each player
// receives what their own node knows, never the truth.
//
//   * KnowledgeView: everything a player may be shown, built from one node's knowledge: its fused picture, the contacts
//     of the ESM receivers and the tracks of the radars cabled to it, its own position and command status. It is built
//     from knowledge structures only, and it has no field for anything else: no platform list, no emitter list, no
//     origin keys, no debug_truth_* fields, no names. What it cannot hold, it cannot leak.
//   * The wire format: a versioned little-endian binary encoding with fixed quantisation (1 m, 0.01 m/s, 1e-5 rad, 1 ms),
//     items sorted by key, so one view always encodes to the same bytes.
//   * KnowledgeReplicator (server): per client, a numbered stream of views. Each packet is a delta against the newest
//     view the client has acknowledged (items added or changed, keys removed), or a keyframe when there is no usable
//     base. Unreliable transport is fine: a lost packet is simply never acknowledged.
//   * KnowledgeMirror (client): applies keyframes and deltas, keeps the views it received by number (deltas need their
//     base), ignores duplicates and late packets, and acknowledges the newest view it holds.
// Unreal's job is only to carry the bytes (an owner-only replicated byte array or an unreliable RPC per player); the
// simulation stays on the server.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "simcore/em/model.hpp"

namespace simcore {
namespace em {

// ------------------------------------------------------------------------------------------------ the view

struct ViewTrack {  // a fused picture item
    int id = 0;
    std::uint8_t identity = 0;  // Identity
    std::uint8_t category = 0;  // Category
    std::uint8_t source = 0;    // InfoSource
    bool position_valid = false;
    bool velocity_valid = false;
    bool from_fix = false;
    bool bearing_only = false;
    bool hostile_emitter = false;
    Vec3 position;
    Vec3 velocity;
    double sigma_h_m = 0.0;
    double time_s = 0.0;
    double bearing_rad = 0.0;   // bearing-only items: the newest line's azimuth ...
    Vec3 bearing_origin;        // ... and where it was taken
    std::string type_guess;
    int Key() const { return id; }
};

struct ViewContact {  // an ESM contact of a receiver cabled to the node
    int key = 0;      // receiver id << 16 | contact id
    double bearing_rad = 0.0;
    double sigma_rad = 0.0;
    double last_seen_s = 0.0;
    bool hostile = false;
    bool steady = false;
    std::string library_name;  // what the receiver's own threat library calls it ("" unknown)
    int Key() const { return key; }
};

struct ViewSensorTrack {  // a track of a radar cabled to the node
    int key = 0;          // radar id << 16 | track id
    bool confirmed = false;
    Vec3 position;
    Vec3 velocity;
    double sigma_m = 0.0;
    int Key() const { return key; }
};

// What the crew's scopes show (the search indicator, the range and elevation indicators, the TV's pointing), carried
// in the knowledge stream so a network client's cabin draws the server's scopes. An echo carries an opaque key, never
// the scenario's target id: a client must not be able to tell a real echo from chaff by its name.
struct ViewEcho {
    int key = 0;
    double range_m = 0.0;
    double azimuth_deg = 0.0;
    double elevation_deg = 0.0;
    int beam = -1;
    bool control_reply = false;
};

struct ViewScope {
    bool valid = false;
    std::vector<ViewEcho> search;         // search radar echoes this step
    std::vector<ViewEcho> iff;            // IFF replies this step
    std::vector<ViewEcho> track_returns;  // the tracking radar's echoes
    int elevation_beam = 0;
    double swept_low_deg = 0.0;
    double swept_high_deg = 0.0;
    double antenna_el_deg = 0.0;
    double indicated_to_true_deg = 0.0;   // the wheels' azimuth + this = true azimuth
    double missile_range_m[2] = {-1.0, -1.0};  // guidance channels I and II (-1: none in flight)
    Vec3 unit_position;
};

struct KnowledgeView {
    std::uint32_t seq = 0;
    double time_s = 0.0;
    int node = 0;
    std::uint8_t c2_status = 0;  // C2Status
    Vec3 own_position;           // the player's own platform: theirs to know
    std::vector<ViewTrack> tracks;
    std::vector<ViewContact> contacts;
    std::vector<ViewSensorTrack> sensor_tracks;
    ViewScope scope;  // filled by the server's caller (the engagement model's presentation), not by BuildView
};

// What node `node_id` knows at the scene's time. Empty (node 0) if there is no such node.
inline KnowledgeView BuildView(const EmScene& s, int node_id) {
    KnowledgeView v;
    const NodeState* n = s.FindNode(node_id);
    if (n == nullptr) return v;
    v.time_s = s.time_s;
    v.node = n->id;
    v.c2_status = static_cast<std::uint8_t>(n->status);
    if (const Platform* p = s.FindPlatform(n->platform_id)) v.own_position = p->position;
    for (const FusedTrack& f : n->kb.Picture(s.time_s)) {
        const KnowledgeTrack& k = f.fused;
        ViewTrack t;
        t.id = f.id;
        t.identity = static_cast<std::uint8_t>(k.identity);
        t.category = static_cast<std::uint8_t>(k.category);
        t.source = static_cast<std::uint8_t>(k.source);
        t.position_valid = k.position_valid && !f.bearing_only;
        t.velocity_valid = k.velocity_valid;
        t.from_fix = f.from_fix;
        t.bearing_only = f.bearing_only;
        t.hostile_emitter = k.has_emitter && k.emitter.hostile;
        if (t.position_valid) t.position = k.position;
        if (t.velocity_valid) t.velocity = k.velocity;
        t.sigma_h_m = t.position_valid ? k.HorizontalSigmaM() : 0.0;
        t.time_s = k.time_s;
        if (f.bearing_only && !k.bearings.empty()) {
            t.bearing_rad = k.bearings.back().azimuth_rad;
            t.bearing_origin = k.bearings.back().origin;
        }
        t.type_guess = k.type_guess;
        v.tracks.push_back(t);
    }
    for (int sensor : n->sensors) {
        if (const EsmState* e = s.FindEsm(sensor)) {
            for (const EsmContact& c : e->contacts) {
                ViewContact vc;
                vc.key = (e->id << 16) | (c.id & 0xffff);
                vc.bearing_rad = c.bearing_rad;
                vc.sigma_rad = c.bearing_sigma_rad;
                vc.last_seen_s = c.last_seen_s;
                vc.hostile = c.sig.hostile;
                vc.steady = c.sig.steady;
                vc.library_name = c.sig.library_name;
                v.contacts.push_back(vc);
            }
        } else if (const RadarState* r = s.FindRadar(sensor)) {
            for (const SensorTrack& st : r->tracker.Tracks()) {
                ViewSensorTrack vt;
                vt.key = (r->id << 16) | (st.id & 0xffff);
                vt.confirmed = st.status == TrackStatus::Confirmed;
                vt.position = st.Position();
                vt.velocity = st.Velocity();
                vt.sigma_m = st.PositionSigmaM();
                v.sensor_tracks.push_back(vt);
            }
        }
    }
    auto by_key = [](const auto& a, const auto& b) { return a.Key() < b.Key(); };
    std::sort(v.tracks.begin(), v.tracks.end(), by_key);
    std::sort(v.contacts.begin(), v.contacts.end(), by_key);
    std::sort(v.sensor_tracks.begin(), v.sensor_tracks.end(), by_key);
    return v;
}

// ------------------------------------------------------------------------------------------------ the wire format

namespace wire {

constexpr std::uint32_t kMagic = 0x31564b53u;  // "SKV1"
constexpr std::uint8_t kKeyframe = 0;
constexpr std::uint8_t kDelta = 1;

class Writer {
public:
    std::vector<std::uint8_t> bytes;
    void U8(std::uint8_t v) { bytes.push_back(v); }
    void U32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xffu));
    }
    void I32(std::int32_t v) { U32(static_cast<std::uint32_t>(v)); }
    void I64(std::int64_t v) {
        const std::uint64_t u = static_cast<std::uint64_t>(v);
        for (int i = 0; i < 8; ++i) bytes.push_back(static_cast<std::uint8_t>((u >> (8 * i)) & 0xffu));
    }
    // Quantised values: round to the step, clamp to the 32-bit range.
    void Q(double v, double step) {
        const double q = std::round(v / step);
        const double lim = 2147483647.0;
        I32(static_cast<std::int32_t>(q > lim ? lim : (q < -lim ? -lim : q)));
    }
    void Time(double t) { I64(static_cast<std::int64_t>(std::llround(t * 1000.0))); }
    void Str(const std::string& s) {
        const std::size_t n = s.size() < 255 ? s.size() : 255;
        U8(static_cast<std::uint8_t>(n));
        bytes.insert(bytes.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(n));
    }
    void V3(const Vec3& v, double step) {
        Q(v.x, step);
        Q(v.y, step);
        Q(v.z, step);
    }
};

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& b) : b_(b) {}
    bool ok = true;
    std::uint8_t U8() {
        if (pos_ + 1 > b_.size()) return Bad();
        return b_[pos_++];
    }
    std::uint32_t U32() {
        if (pos_ + 4 > b_.size()) return Bad();
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(b_[pos_++]) << (8 * i);
        return v;
    }
    std::int32_t I32() { return static_cast<std::int32_t>(U32()); }
    std::int64_t I64() {
        if (pos_ + 8 > b_.size()) return Bad();
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(b_[pos_++]) << (8 * i);
        return static_cast<std::int64_t>(v);
    }
    double Q(double step) { return static_cast<double>(I32()) * step; }
    double Time() { return static_cast<double>(I64()) / 1000.0; }
    std::string Str() {
        const std::size_t n = U8();
        if (pos_ + n > b_.size()) {
            Bad();
            return std::string();
        }
        std::string s(b_.begin() + static_cast<std::ptrdiff_t>(pos_), b_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
        pos_ += n;
        return s;
    }
    Vec3 V3(double step) {
        Vec3 v;
        v.x = Q(step);
        v.y = Q(step);
        v.z = Q(step);
        return v;
    }
    bool AtEnd() const { return pos_ == b_.size(); }

private:
    std::uint8_t Bad() {
        ok = false;
        pos_ = b_.size();
        return 0;
    }
    const std::vector<std::uint8_t>& b_;
    std::size_t pos_ = 0;
};

constexpr double kPosStep = 1.0;     // m
constexpr double kVelStep = 0.01;    // m/s
constexpr double kAngStep = 1e-5;    // rad
constexpr double kSigmaStep = 1.0;   // m

inline void Put(Writer& w, const ViewTrack& t) {
    w.I32(t.id);
    w.U8(t.identity);
    w.U8(t.category);
    w.U8(t.source);
    w.U8(static_cast<std::uint8_t>((t.position_valid ? 1 : 0) | (t.velocity_valid ? 2 : 0) | (t.from_fix ? 4 : 0) |
                                   (t.bearing_only ? 8 : 0) | (t.hostile_emitter ? 16 : 0)));
    w.V3(t.position, kPosStep);
    w.V3(t.velocity, kVelStep);
    w.Q(t.sigma_h_m, kSigmaStep);
    w.Time(t.time_s);
    w.Q(t.bearing_rad, kAngStep);
    w.V3(t.bearing_origin, kPosStep);
    w.Str(t.type_guess);
}
inline void Get(Reader& r, ViewTrack& t) {
    t.id = r.I32();
    t.identity = r.U8();
    t.category = r.U8();
    t.source = r.U8();
    const std::uint8_t f = r.U8();
    t.position_valid = (f & 1) != 0;
    t.velocity_valid = (f & 2) != 0;
    t.from_fix = (f & 4) != 0;
    t.bearing_only = (f & 8) != 0;
    t.hostile_emitter = (f & 16) != 0;
    t.position = r.V3(kPosStep);
    t.velocity = r.V3(kVelStep);
    t.sigma_h_m = r.Q(kSigmaStep);
    t.time_s = r.Time();
    t.bearing_rad = r.Q(kAngStep);
    t.bearing_origin = r.V3(kPosStep);
    t.type_guess = r.Str();
}
inline void Put(Writer& w, const ViewContact& c) {
    w.I32(c.key);
    w.Q(c.bearing_rad, kAngStep);
    w.Q(c.sigma_rad, kAngStep);
    w.Time(c.last_seen_s);
    w.U8(static_cast<std::uint8_t>((c.hostile ? 1 : 0) | (c.steady ? 2 : 0)));
    w.Str(c.library_name);
}
inline void Get(Reader& r, ViewContact& c) {
    c.key = r.I32();
    c.bearing_rad = r.Q(kAngStep);
    c.sigma_rad = r.Q(kAngStep);
    c.last_seen_s = r.Time();
    const std::uint8_t f = r.U8();
    c.hostile = (f & 1) != 0;
    c.steady = (f & 2) != 0;
    c.library_name = r.Str();
}
inline void Put(Writer& w, const ViewSensorTrack& t) {
    w.I32(t.key);
    w.U8(t.confirmed ? 1 : 0);
    w.V3(t.position, kPosStep);
    w.V3(t.velocity, kVelStep);
    w.Q(t.sigma_m, kSigmaStep);
}
inline void Get(Reader& r, ViewSensorTrack& t) {
    t.key = r.I32();
    t.confirmed = r.U8() != 0;
    t.position = r.V3(kPosStep);
    t.velocity = r.V3(kVelStep);
    t.sigma_m = r.Q(kSigmaStep);
}

constexpr double kDegStep = 1e-4;  // degrees, for the scope

inline void PutEchoes(Writer& w, const std::vector<ViewEcho>& list) {
    w.U32(static_cast<std::uint32_t>(list.size()));
    for (const ViewEcho& e : list) {
        w.I32(e.key);
        w.Q(e.range_m, kPosStep);
        w.Q(e.azimuth_deg, kDegStep);
        w.Q(e.elevation_deg, kDegStep);
        w.I32(e.beam);
        w.U8(e.control_reply ? 1 : 0);
    }
}
inline bool GetEchoes(Reader& r, std::vector<ViewEcho>& list) {
    const std::uint32_t n = r.U32();
    if (!r.ok || n > 100000u) return false;
    list.clear();
    for (std::uint32_t i = 0; i < n && r.ok; ++i) {
        ViewEcho e;
        e.key = r.I32();
        e.range_m = r.Q(kPosStep);
        e.azimuth_deg = r.Q(kDegStep);
        e.elevation_deg = r.Q(kDegStep);
        e.beam = r.I32();
        e.control_reply = r.U8() != 0;
        list.push_back(e);
    }
    return r.ok;
}
inline std::vector<std::uint8_t> ScopeBytes(const ViewScope& sc) {
    Writer w;
    w.U8(sc.valid ? 1 : 0);
    if (!sc.valid) return w.bytes;
    PutEchoes(w, sc.search);
    PutEchoes(w, sc.iff);
    PutEchoes(w, sc.track_returns);
    w.I32(sc.elevation_beam);
    w.Q(sc.swept_low_deg, kDegStep);
    w.Q(sc.swept_high_deg, kDegStep);
    w.Q(sc.antenna_el_deg, kDegStep);
    w.Q(sc.indicated_to_true_deg, kDegStep);
    w.Q(sc.missile_range_m[0], kPosStep);
    w.Q(sc.missile_range_m[1], kPosStep);
    w.V3(sc.unit_position, kPosStep);
    return w.bytes;
}
inline bool GetScope(Reader& r, ViewScope& sc) {
    sc = ViewScope();
    sc.valid = r.U8() != 0;
    if (!sc.valid) return r.ok;
    if (!GetEchoes(r, sc.search) || !GetEchoes(r, sc.iff) || !GetEchoes(r, sc.track_returns)) return false;
    sc.elevation_beam = r.I32();
    sc.swept_low_deg = r.Q(kDegStep);
    sc.swept_high_deg = r.Q(kDegStep);
    sc.antenna_el_deg = r.Q(kDegStep);
    sc.indicated_to_true_deg = r.Q(kDegStep);
    sc.missile_range_m[0] = r.Q(kPosStep);
    sc.missile_range_m[1] = r.Q(kPosStep);
    sc.unit_position = r.V3(kPosStep);
    return r.ok;
}

// One item's bytes (to tell whether it changed since the base).
template <typename T>
std::vector<std::uint8_t> ItemBytes(const T& item) {
    Writer w;
    Put(w, item);
    return w.bytes;
}

// The items of `now` that are new or changed against `base`, and the keys of `base` that are gone.
template <typename T>
void WriteList(Writer& w, const std::vector<T>& now, const std::vector<T>* base) {
    std::vector<const T*> changed;
    std::vector<int> removed;
    std::size_t i = 0, j = 0;
    const std::size_t nb = base != nullptr ? base->size() : 0;
    while (i < now.size() || j < nb) {
        if (j >= nb || (i < now.size() && now[i].Key() < (*base)[j].Key())) {
            changed.push_back(&now[i++]);
        } else if (i >= now.size() || (*base)[j].Key() < now[i].Key()) {
            removed.push_back((*base)[j++].Key());
        } else {
            if (ItemBytes(now[i]) != ItemBytes((*base)[j])) changed.push_back(&now[i]);
            ++i;
            ++j;
        }
    }
    w.U32(static_cast<std::uint32_t>(changed.size()));
    for (const T* t : changed) Put(w, *t);
    w.U32(static_cast<std::uint32_t>(removed.size()));
    for (int k : removed) w.I32(k);
}

template <typename T>
bool ReadList(Reader& r, std::vector<T>& list) {
    const std::uint32_t n = r.U32();
    if (!r.ok || n > 100000u) return false;
    std::map<int, T> items;
    for (const T& t : list) items[t.Key()] = t;
    for (std::uint32_t k = 0; k < n && r.ok; ++k) {
        T t;
        Get(r, t);
        items[t.Key()] = t;
    }
    const std::uint32_t m = r.U32();
    if (!r.ok || m > 100000u) return false;
    for (std::uint32_t k = 0; k < m && r.ok; ++k) items.erase(r.I32());
    list.clear();
    for (const auto& kv : items) list.push_back(kv.second);
    return r.ok;
}

}  // namespace wire

// A view as a keyframe (base null) or as a delta against `base`.
inline std::vector<std::uint8_t> EncodeView(const KnowledgeView& v, const KnowledgeView* base = nullptr) {
    wire::Writer w;
    w.U32(wire::kMagic);
    w.U8(base != nullptr ? wire::kDelta : wire::kKeyframe);
    w.U32(v.seq);
    w.U32(base != nullptr ? base->seq : 0u);
    w.Time(v.time_s);
    w.I32(v.node);
    w.U8(v.c2_status);
    w.V3(v.own_position, wire::kPosStep);
    wire::WriteList(w, v.tracks, base != nullptr ? &base->tracks : nullptr);
    wire::WriteList(w, v.contacts, base != nullptr ? &base->contacts : nullptr);
    wire::WriteList(w, v.sensor_tracks, base != nullptr ? &base->sensor_tracks : nullptr);
    // The scope block whole, or a single 0 when it is the same as the base's.
    const std::vector<std::uint8_t> scope = wire::ScopeBytes(v.scope);
    if (base != nullptr && wire::ScopeBytes(base->scope) == scope) {
        w.U8(0);
    } else {
        w.U8(1);
        w.bytes.insert(w.bytes.end(), scope.begin(), scope.end());
    }
    return w.bytes;
}

// The header of a packet: is it a delta, its number and its base's number. False if it isn't one of ours.
inline bool PeekPacket(const std::vector<std::uint8_t>& bytes, bool& delta, std::uint32_t& seq, std::uint32_t& base_seq) {
    wire::Reader r(bytes);
    if (r.U32() != wire::kMagic) return false;
    delta = r.U8() == wire::kDelta;
    seq = r.U32();
    base_seq = r.U32();
    return r.ok;
}

// Decode a packet onto its base (a copy of the base view for a delta, an empty view for a keyframe).
inline bool DecodeView(const std::vector<std::uint8_t>& bytes, KnowledgeView& inout) {
    wire::Reader r(bytes);
    if (r.U32() != wire::kMagic) return false;
    r.U8();
    inout.seq = r.U32();
    r.U32();
    inout.time_s = r.Time();
    inout.node = r.I32();
    inout.c2_status = r.U8();
    inout.own_position = r.V3(wire::kPosStep);
    if (!wire::ReadList(r, inout.tracks) || !wire::ReadList(r, inout.contacts) || !wire::ReadList(r, inout.sensor_tracks)) return false;
    if (r.U8() != 0 && !wire::GetScope(r, inout.scope)) return false;
    return r.ok && r.AtEnd();
}

// The view exactly as a client sees it after the wire's quantisation (for comparisons).
inline KnowledgeView Quantised(const KnowledgeView& v) {
    KnowledgeView out;
    DecodeView(EncodeView(v), out);
    return out;
}

// ------------------------------------------------------------------------------------------------ server and client

class KnowledgeReplicator {
public:
    static constexpr std::size_t kHistory = 64;  // views kept per client for delta bases

    // A player who sees what `node_id` knows. Returns the client handle.
    int AddClient(int node_id) {
        Client c;
        c.node = node_id;
        clients_.push_back(c);
        return static_cast<int>(clients_.size()) - 1;
    }

    // The next packet for a client: a delta against its newest acknowledged view if the server still has it, else a
    // keyframe.
    std::vector<std::uint8_t> Produce(int client, const EmScene& s) {
        return Produce(client, &s, [](KnowledgeView&) {});
    }
    // The same, with no EM scene (an empty picture) and/or a caller that adds what the scopes show (`decorate`).
    template <typename F>
    std::vector<std::uint8_t> Produce(int client, const EmScene* s, F&& decorate) {
        Client& c = clients_[static_cast<std::size_t>(client)];
        KnowledgeView v = s != nullptr ? BuildView(*s, c.node) : KnowledgeView();
        decorate(v);
        v.seq = ++c.seq;
        const KnowledgeView* base = nullptr;
        for (const KnowledgeView& h : c.history)
            if (h.seq == c.acked && c.acked != 0) base = &h;
        std::vector<std::uint8_t> bytes = EncodeView(v, base);
        if (base == nullptr) ++c.keyframes;
        c.bytes_sent += bytes.size();
        ++c.packets;
        c.history.push_back(Quantised(v));
        if (c.history.size() > kHistory) c.history.erase(c.history.begin());
        return bytes;
    }

    // The player now sees another node (a different vehicle or post). The stream goes on: the next delta simply carries
    // the difference, as the bases are byte-level views.
    void SetNode(int client, int node_id) { clients_[static_cast<std::size_t>(client)].node = node_id; }
    int Node(int client) const { return clients_[static_cast<std::size_t>(client)].node; }
    int ClientCount() const { return static_cast<int>(clients_.size()); }

    // The client says it holds view `seq` (acknowledgements may arrive late or out of order: the newest wins).
    void Ack(int client, std::uint32_t seq) {
        Client& c = clients_[static_cast<std::size_t>(client)];
        if (seq > c.acked && seq <= c.seq) c.acked = seq;
    }

    std::uint64_t BytesSent(int client) const { return clients_[static_cast<std::size_t>(client)].bytes_sent; }
    std::uint64_t Packets(int client) const { return clients_[static_cast<std::size_t>(client)].packets; }
    std::uint64_t Keyframes(int client) const { return clients_[static_cast<std::size_t>(client)].keyframes; }
    // The view the server sent as `seq` (quantised as the client sees it), if still held.
    const KnowledgeView* Sent(int client, std::uint32_t seq) const {
        for (const KnowledgeView& h : clients_[static_cast<std::size_t>(client)].history)
            if (h.seq == seq) return &h;
        return nullptr;
    }

private:
    struct Client {
        int node = 0;
        std::uint32_t seq = 0;
        std::uint32_t acked = 0;
        std::vector<KnowledgeView> history;
        std::uint64_t bytes_sent = 0, packets = 0, keyframes = 0;
    };
    std::vector<Client> clients_;
};

class KnowledgeMirror {
public:
    static constexpr std::size_t kHistory = 64;

    // Apply a packet. Returns true if it gave a new newest view (then `ack` is what to acknowledge). Duplicates, late
    // packets, garbage and deltas whose base is gone are ignored.
    bool Receive(const std::vector<std::uint8_t>& bytes, std::uint32_t* ack = nullptr) {
        bool delta = false;
        std::uint32_t seq = 0, base_seq = 0;
        if (!PeekPacket(bytes, delta, seq, base_seq)) return false;
        if (seq <= current_.seq && current_.seq != 0) return false;  // old or duplicate
        KnowledgeView v;
        if (delta) {
            const KnowledgeView* base = nullptr;
            for (const KnowledgeView& h : history_)
                if (h.seq == base_seq) base = &h;
            if (base == nullptr) {
                ++missing_base_;
                return false;
            }
            v = *base;
        }
        if (!DecodeView(bytes, v)) return false;
        current_ = v;
        history_.push_back(v);
        if (history_.size() > kHistory) history_.erase(history_.begin());
        if (ack != nullptr) *ack = v.seq;
        return true;
    }

    const KnowledgeView& View() const { return current_; }
    int MissingBase() const { return missing_base_; }

private:
    KnowledgeView current_;
    std::vector<KnowledgeView> history_;
    int missing_base_ = 0;
};

}  // namespace em
}  // namespace simcore
