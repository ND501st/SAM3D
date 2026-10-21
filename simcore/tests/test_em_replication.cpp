// Multiplayer knowledge (roadmap E10): each player gets its own node's knowledge, never the truth; the wire format, the
// delta stream, and a soak test over a lossy, reordering, duplicating channel. Generic placeholder numbers only.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "simcore/em.hpp"
#include "test_framework.hpp"

#ifndef SIMCORE_DATA_DIR
#define SIMCORE_DATA_DIR "data"
#endif

using namespace simcore;
using namespace simcore::em;

namespace {

EmLibrary Lib() {
    EmLibrary lib;
    const EmLoadResult r = LoadEmLibraryFile(std::string(SIMCORE_DATA_DIR) + "/em/generic.json", lib);
    if (!r.ok) std::printf("    %s\n", r.error.c_str());
    return lib;
}

const GeoPoint kOrigin = GeoPoint::FromDegrees(49.5, 17.0, 0.0);

int Aircraft(EmWorld& w, const char* name, Side side, double e, double n, double alt, const Vec3& vel) {
    Platform p;
    p.name = name;
    p.side = side;
    p.kind = PlatformKind::FixedWing;
    p.rcs = RcsModel{3.0, 6.0, 3.0};
    p.velocity = vel;
    p.position = {e, n, alt - (e * e + n * n) / (2.0 * wgs84::kMeanRadius)};
    return w.AddPlatform(p);
}

void Fly(EmWorld& w, double dt) {
    for (Platform& p : w.Scene().platforms) {
        if (!p.alive || p.kind == PlatformKind::Ground) continue;
        const double alt = w.Scene().frame.AltitudeOf(p.position);
        p.position += p.velocity * dt;
        p.position.z += alt - w.Scene().frame.AltitudeOf(p.position);
    }
}

// Red: a SAM battery (search radar, node). Blue: two ESM aircraft with a node, a fighter radiating, and a fighter far
// outside red's radar cover that nobody red can know of.
struct Scene {
    EmWorld w{kOrigin, nullptr, 9};
    int red_node = 0, blue_node = 0, red_radar = 0, hidden = 0, fighter = 0;
    Scene() {
        w.SetLibrary(Lib());
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
        red_radar = w.AddRadar(site, "generic_search_c", true);
        red_node = w.AddNode("Battery", site, Side::Red, true);
        w.AttachSensor(red_node, red_radar);
        const int a = Aircraft(w, "ESM 1", Side::Blue, -25000.0, 60000.0, 8000.0, {0.0, 0.0, 0.0});
        const int b = Aircraft(w, "ESM 2", Side::Blue, 25000.0, 60000.0, 8000.0, {0.0, 0.0, 0.0});
        blue_node = w.AddNode("Blue ESM", a, Side::Blue, true);
        w.AttachSensor(blue_node, w.AddEsm(a, "generic_esm_pod"));
        w.AttachSensor(blue_node, w.AddEsm(b, "generic_esm_pod"));
        fighter = Aircraft(w, "TRUTH-CANARY fighter", Side::Blue, 5000.0, 30000.0, 4000.0, {0.0, -150.0, 0.0});
        const EmitterDef fcr = w.Scene().lib.emitters[static_cast<std::size_t>(w.Scene().lib.Emitter("generic_airborne_fcr_x"))];
        w.Scene().FindEmitter(w.AddEmitter(fighter, fcr))->radiating = true;
        hidden = Aircraft(w, "TRUTH-CANARY hidden", Side::Blue, 0.0, -200000.0, 9000.0, {0.0, 0.0, 0.0});
    }
    void Run(int ticks) {
        for (int i = 0; i < ticks; ++i) {
            Fly(w, 0.1);
            w.Step(0.1);
        }
    }
};

bool Contains(const std::vector<std::uint8_t>& bytes, const std::string& needle) {
    return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end()) != bytes.end();
}

}  // namespace

SC_TEST("em replication: each side's view holds its own knowledge and nothing of the truth") {
    Scene sc;
    sc.Run(300);  // 30 s
    const KnowledgeView red = BuildView(sc.w.Scene(), sc.red_node);
    const KnowledgeView blue = BuildView(sc.w.Scene(), sc.blue_node);
    std::printf("    red: %zu picture items, %zu radar tracks; blue: %zu items, %zu ESM contacts\n", red.tracks.size(),
                red.sensor_tracks.size(), blue.tracks.size(), blue.contacts.size());
    // red sees the fighter on radar, nothing near the hidden aircraft 200 km south
    SC_EXPECT(!red.sensor_tracks.empty());
    const Vec3 hidden = sc.w.Scene().FindPlatform(sc.hidden)->position;
    for (const ViewTrack& t : red.tracks) SC_EXPECT(!t.position_valid || (t.position - hidden).Length() > 50000.0);
    for (const ViewSensorTrack& t : red.sensor_tracks) SC_EXPECT((t.position - hidden).Length() > 50000.0);
    // blue hears the red radar; its own aircraft are not in its view as contacts
    SC_EXPECT(!blue.contacts.empty());
    for (const ViewContact& c : blue.contacts) SC_EXPECT(c.library_name == "SAM search radar" || c.library_name.empty());
    SC_EXPECT(blue.sensor_tracks.empty());  // blue has no radar cabled to its node
    // nothing from the truth's names reaches the wire, and each side gets a different view
    const std::vector<std::uint8_t> rb = EncodeView(red), bb = EncodeView(blue);
    SC_EXPECT(!Contains(rb, "TRUTH-CANARY") && !Contains(bb, "TRUTH-CANARY"));
    SC_EXPECT(rb != bb);
    // an unknown node gives an empty view
    SC_EXPECT(BuildView(sc.w.Scene(), 999999).node == 0);
}

SC_TEST("em replication: the wire format round-trips, is canonical, and refuses garbage") {
    Scene sc;
    sc.Run(300);
    KnowledgeView v = BuildView(sc.w.Scene(), sc.red_node);
    v.seq = 7;
    const std::vector<std::uint8_t> key = EncodeView(v);
    KnowledgeView back;
    SC_EXPECT(DecodeView(key, back));
    SC_EXPECT(EncodeView(back) == key);  // quantised once, the same bytes forever
    SC_EXPECT(back.seq == 7 && back.node == v.node && back.sensor_tracks.size() == v.sensor_tracks.size());
    if (!v.sensor_tracks.empty()) SC_EXPECT((back.sensor_tracks[0].position - v.sensor_tracks[0].position).Length() < 1.0);
    // a delta against an older view rebuilds the newer one exactly
    sc.Run(50);
    KnowledgeView v2 = BuildView(sc.w.Scene(), sc.red_node);
    v2.seq = 8;
    const std::vector<std::uint8_t> delta = EncodeView(v2, &back);
    KnowledgeView rebuilt = back;
    SC_EXPECT(DecodeView(delta, rebuilt));
    SC_EXPECT(EncodeView(rebuilt) == EncodeView(Quantised(v2)));
    std::printf("    keyframe %zu bytes, delta 5 s later %zu bytes\n", key.size(), delta.size());
    // truncated or foreign bytes are refused
    std::vector<std::uint8_t> cut(key.begin(), key.begin() + static_cast<std::ptrdiff_t>(key.size() / 2));
    KnowledgeView junk;
    SC_EXPECT(!DecodeView(cut, junk));
    std::vector<std::uint8_t> foreign = key;
    foreign[0] ^= 0xff;
    SC_EXPECT(!DecodeView(foreign, junk));
}

SC_TEST("em replication: soak, 10 minutes over a lossy channel: the client always holds exactly what the server sent") {
    Scene sc;
    KnowledgeReplicator server;
    const int red = server.AddClient(sc.red_node);
    const int blue = server.AddClient(sc.blue_node);
    KnowledgeMirror mirror[2];
    // A deterministic bad channel: 25 % lost each way, delay 0-3 sends (so reordered), 5 % duplicated.
    std::uint64_t rng = 0x2545f4914f6cdd1dull;
    auto u = [&rng]() {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return static_cast<double>(rng >> 11) / 9007199254740992.0;
    };
    struct InFlight {
        int at = 0;
        int client = 0;
        std::vector<std::uint8_t> bytes;
        std::uint32_t ack = 0;
        bool is_ack = false;
    };
    std::deque<InFlight> wire_q;
    int applied = 0, mismatches = 0;
    const int sends = 1200;  // every 0.5 s for 10 minutes
    for (int k = 0; k < sends; ++k) {
        sc.Run(5);
        for (int c = 0; c < 2; ++c) {
            const bool clean_end = k >= sends - 6;  // the last few go through, to check convergence
            std::vector<std::uint8_t> p = server.Produce(c == 0 ? red : blue, sc.w.Scene());
            if (!clean_end && u() < 0.25) continue;
            const int delay = clean_end ? 0 : static_cast<int>(u() * 4.0);
            wire_q.push_back({k + delay, c, p, 0, false});
            if (!clean_end && u() < 0.05) wire_q.push_back({k + delay + 1, c, p, 0, false});
        }
        // deliver what is due, in arrival order (late packets overtake nothing: they arrive when they arrive)
        for (std::size_t i = 0; i < wire_q.size();) {
            if (wire_q[i].at > k) {
                ++i;
                continue;
            }
            InFlight f = wire_q[i];
            wire_q.erase(wire_q.begin() + static_cast<std::ptrdiff_t>(i));
            if (f.is_ack) {
                server.Ack(f.client == 0 ? red : blue, f.ack);
                continue;
            }
            std::uint32_t ack = 0;
            if (mirror[f.client].Receive(f.bytes, &ack)) {
                ++applied;
                const KnowledgeView* sent = server.Sent(f.client == 0 ? red : blue, ack);
                if (sent == nullptr || EncodeView(*sent) != EncodeView(mirror[f.client].View())) ++mismatches;
                const bool clean_end = k >= sends - 6;
                if (clean_end || u() >= 0.25) wire_q.push_back({k + (clean_end ? 0 : static_cast<int>(u() * 3.0)), f.client, {}, ack, true});
            }
        }
    }
    const double kb_s = static_cast<double>(server.BytesSent(red) + server.BytesSent(blue)) / 1024.0 / 600.0;
    std::printf("    %d views applied, %d mismatches; red %llu packets (%llu keyframes), blue %llu (%llu); %.2f KiB/s for both; "
                "missing bases %d/%d\n",
                applied, mismatches, static_cast<unsigned long long>(server.Packets(red)),
                static_cast<unsigned long long>(server.Keyframes(red)), static_cast<unsigned long long>(server.Packets(blue)),
                static_cast<unsigned long long>(server.Keyframes(blue)), kb_s, mirror[0].MissingBase(), mirror[1].MissingBase());
    SC_EXPECT(applied > 1200);
    SC_EXPECT(mismatches == 0);
    // converged: both clients hold the server's newest view
    SC_EXPECT(EncodeView(mirror[0].View()) == EncodeView(*server.Sent(red, mirror[0].View().seq)));
    SC_EXPECT(mirror[0].View().seq == server.Packets(red));
    SC_EXPECT(mirror[1].View().seq == server.Packets(blue));
    // deltas did their job: far fewer keyframes than packets
    SC_EXPECT(server.Keyframes(red) < server.Packets(red) / 4);
    SC_EXPECT(kb_s < 8.0);
}

SC_TEST("em replication: a player moved to another node gets the new node's view on the same stream") {
    Scene sc;
    sc.Run(300);
    KnowledgeReplicator server;
    const int c = server.AddClient(sc.red_node);
    KnowledgeMirror m;
    std::uint32_t ack = 0;
    SC_EXPECT(m.Receive(server.Produce(c, sc.w.Scene()), &ack));
    server.Ack(c, ack);
    server.SetNode(c, sc.blue_node);
    SC_EXPECT(server.Node(c) == sc.blue_node && server.ClientCount() == 1);
    sc.Run(5);
    SC_EXPECT(m.Receive(server.Produce(c, sc.w.Scene()), &ack));
    SC_EXPECT(m.View().node == sc.blue_node);
    SC_EXPECT(EncodeView(m.View()) == EncodeView(*server.Sent(c, ack)));
    SC_EXPECT(server.Keyframes(c) == 1);  // the switch went as a delta
}

SC_TEST("em replication: the scope block rides the stream, is skipped when unchanged, and carries no target ids") {
    Scene sc;
    sc.Run(100);
    KnowledgeReplicator server;
    const int c = server.AddClient(sc.red_node);
    auto deco = [](KnowledgeView& v) {
        v.scope.valid = true;
        v.scope.search.push_back({7, 25000.0, 12.5, 3.25, 0, false});
        v.scope.track_returns.push_back({7, 25010.0, 12.5, 3.3, -1, false});
        v.scope.antenna_el_deg = 3.0;
        v.scope.indicated_to_true_deg = -1.5;
        v.scope.missile_range_m[0] = 8000.0;
        v.scope.unit_position = {10.0, 20.0, 3.0};
    };
    KnowledgeMirror m;
    std::uint32_t ack = 0;
    const std::vector<std::uint8_t> first = server.Produce(c, &sc.w.Scene(), deco);
    SC_EXPECT(m.Receive(first, &ack));
    server.Ack(c, ack);
    SC_EXPECT(m.View().scope.valid && m.View().scope.search.size() == 1);
    SC_EXPECT(std::fabs(m.View().scope.search[0].azimuth_deg - 12.5) < 1e-3 && m.View().scope.search[0].key == 7);
    SC_EXPECT(std::fabs(m.View().scope.missile_range_m[0] - 8000.0) < 1.0 && m.View().scope.missile_range_m[1] < 0.0);
    // the next one with the same scope: the block is one byte
    const std::vector<std::uint8_t> same = server.Produce(c, &sc.w.Scene(), deco);
    SC_EXPECT(m.Receive(same, &ack));
    SC_EXPECT(m.View().scope.search.size() == 1);  // kept from the base
    // no EM scene at all: the scope alone still goes
    KnowledgeReplicator bare;
    const int b = bare.AddClient(0);
    KnowledgeMirror mb;
    SC_EXPECT(mb.Receive(bare.Produce(b, nullptr, deco), &ack));
    SC_EXPECT(mb.View().scope.valid && mb.View().tracks.empty());
}
