// simcore/em/world.hpp — EmWorld: builds the scene and runs the electromagnetic systems in order.
//
// Per Step(dt) (dt = 0.1 s is the intended rate; docs/EM_ARCHITECTURE.md, "Update rates"):
//   1. platforms: altitude, height above ground, spatial grid
//   2. doctrine and EMCON schedules (decisions from knowledge)
//   3. emitters: scan rotation, jammer pointing; chaff drift
//   4. radars: looks in this tick's sweep -> plots -> tracks; strobes; tracking-radar lock
//   5. ESM receivers (every 0.2 s): intercepts -> contacts -> identification; TDOA groups -> fixes
//   6. knowledge: own sensors into each node's knowledge base (every 0.5 s)
//   7. comms: links (every 1 s), reports and pictures, deliveries, C2 status
//   8. knowledge ageing (every 1 s)
// Everything is deterministic for a given seed and input sequence.
#pragma once

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "simcore/em/comms_system.hpp"
#include "simcore/em/deception.hpp"
#include "simcore/em/doctrine.hpp"
#include "simcore/em/esm_system.hpp"
#include "simcore/em/knowledge_system.hpp"
#include "simcore/em/model.hpp"
#include "simcore/em/radar_system.hpp"
#include "simcore/em/scene_ops.hpp"
#include "simcore/geo.hpp"

namespace simcore {
namespace em {

struct EmRates {
    double esm_period_s = 0.2;        // passive receivers listen at 5 Hz (beam paints in between still count)
    double knowledge_period_s = 0.5;
    double link_period_s = 1.0;
    double age_period_s = 1.0;
    unsigned threads = 1;             // worker threads for the ESM sites (identical results at any count)
    bool profile = false;             // count wall-clock time per phase in EmStats (em_bench)
};

class EmWorld {
public:
    explicit EmWorld(const GeoPoint& origin, const Terrain* terrain = nullptr, std::uint64_t seed = 1) {
        s_.frame = LocalFrame(origin);
        s_.terrain = terrain;
        s_.rng = Random(seed);
    }

    // The weather is never part of a calibration: ranges are published for a clear day.
    static Environment Dry(Environment env) {
        env.rain_rate_mmh = 0.0;
        return env;
    }

    EmScene& Scene() { return s_; }
    const EmScene& Scene() const { return s_; }
    double Time() const { return s_.time_s; }
    EmRates& Rates() { return rates_; }

    std::vector<SamDoctrine>& Doctrines() { return doctrines_; }
    std::vector<EmconStep>& Emcon() { return emcon_; }

    void SetTerrain(const Terrain* terrain) {
        s_.terrain = terrain;
        s_.ClearPaths();
    }

    // Install the definitions and calibrate every radar to its anchor. Fails if a radar has none.
    bool SetLibrary(EmLibrary lib, std::string* error = nullptr) {
        const Environment dry = Dry(s_.env);
        for (RadarDef& d : lib.radars) {
            if (!Calibrate(d, dry)) {
                if (error != nullptr) *error = "radar '" + d.id + "' has no usable calibration anchor";
                return false;
            }
        }
        for (RadioDef& r : lib.radios) CalibrateRadio(r, dry);
        s_.lib = std::move(lib);
        return true;
    }

    // Changing the environment changes the propagation the calibration was done in: recalibrate.
    void SetEnvironment(const Environment& env) {
        s_.env = env;
        const Environment dry = Dry(env);
        for (RadarDef& d : s_.lib.radars) Calibrate(d, dry);
        for (RadioDef& r : s_.lib.radios) CalibrateRadio(r, dry);
        s_.ClearPaths();
    }

    int AddPlatform(Platform p) {
        p.id = s_.next_id++;
        p.altitude_m = s_.frame.AltitudeOf(p.position);
        p.height_agl_m = p.altitude_m - s_.GetTerrain().HeightAt(p.position.x, p.position.y);
        s_.platforms.push_back(p);
        return p.id;
    }

    // A ground platform standing on the terrain with its antenna `mast_m` above it.
    int AddGroundPlatform(const std::string& name, Side side, double east_m, double north_m, double mast_m,
                          const RcsModel& rcs = RcsModel{20.0, 30.0, 20.0}) {
        Platform p;
        p.name = name;
        p.side = side;
        p.kind = PlatformKind::Ground;
        p.rcs = rcs;
        const double ground = s_.GetTerrain().HeightAt(east_m, north_m);
        // ENU z for a given altitude near the origin: z = alt - d^2 / (2 R) (curvature drop).
        const double d2 = east_m * east_m + north_m * north_m;
        p.position = {east_m, north_m, ground + mast_m - s_.frame.Origin().alt_m - d2 / (2.0 * wgs84::kMeanRadius)};
        return AddPlatform(p);
    }

    int AddEmitter(int platform_id, const EmitterDef& def) {
        EmitterState e;
        e.id = s_.next_id++;
        e.def = def;
        e.platform_id = platform_id;
        e.freq_hz = def.waveform.freq_hz;
        if (!def.beams.empty()) e.pointing_el_rad = def.beams.front().centre_rad;
        s_.emitters.push_back(e);
        return e.id;
    }

    int AddRadar(int platform_id, const std::string& def_id, bool on = false) {
        const int idx = s_.lib.Radar(def_id);
        if (idx < 0) return 0;
        const RadarDef& d = s_.lib.radars[static_cast<std::size_t>(idx)];
        RadarState r;
        r.id = s_.next_id++;
        r.def_index = idx;
        r.platform_id = platform_id;
        r.emitter_id = AddEmitter(platform_id, d.emitter);
        r.on = on;
        r.mti = d.has_mti ? MtiMode::Internal : MtiMode::Off;
        r.tracker = Tracker(d.tracker);
        for (std::size_t i = 0; i < d.emitter.beams.size(); ++i) r.beam_program.push_back(static_cast<int>(i));
        if (!r.beam_program.empty()) r.program_pos = r.beam_program.size() - 1;
        s_.radars.push_back(r);
        return r.id;
    }

    int AddEsm(int platform_id, const std::string& def_id) {
        const int idx = s_.lib.Esm(def_id);
        if (idx < 0) return 0;
        EsmState e;
        e.id = s_.next_id++;
        e.def_index = idx;
        e.platform_id = platform_id;
        s_.esm.push_back(e);
        return e.id;
    }

    int AddJammer(int platform_id, const std::string& def_id, bool on = false) {
        const int idx = s_.lib.Jammer(def_id);
        if (idx < 0) return 0;
        JammerState j;
        j.id = s_.next_id++;
        j.def_index = idx;
        j.platform_id = platform_id;
        j.emitter_id = AddEmitter(platform_id, s_.lib.jammers[static_cast<std::size_t>(idx)].emitter);
        j.on = on;
        s_.jammers.push_back(j);
        return j.id;
    }

    // A decoy on `platform_id` with the same transmission as radar `radar_id` (frequency, PRF, pulse width, scan).
    int AddDecoy(int platform_id, int radar_id, DecoyMode mode) {
        const RadarState* r = s_.FindRadar(radar_id);
        if (r == nullptr) return 0;
        EmitterDef def = s_.lib.radars[static_cast<std::size_t>(r->def_index)].emitter;
        def.kind = EmitterKind::Decoy;
        def.name = "Decoy: " + def.name;
        const int id = AddEmitter(platform_id, def);
        s_.decoys.push_back({id, radar_id, mode});
        UpdateDecoys(s_);
        return id;
    }

    // Move an emitter to one of its preset channels (a radar's frequency change). False if it has no such channel.
    bool SetEmitterChannel(int emitter_id, int channel) {
        EmitterState* e = s_.FindEmitter(emitter_id);
        if (e == nullptr || channel < 0 || static_cast<std::size_t>(channel) >= e->def.waveform.channels_hz.size()) return false;
        e->freq_hz = e->def.waveform.channels_hz[static_cast<std::size_t>(channel)];
        s_.Log(EmEventKind::Info, emitter_id, channel, "frequency change to channel " + std::to_string(channel + 1));
        return true;
    }

    // Empty voice messages from `node_id` on `net_id` every `period_s`, from `from_s` until `until_s`.
    void AddDummyTraffic(int node_id, int net_id, double period_s, double duration_s, double from_s = 0.0, double until_s = 1e18) {
        DummyTraffic d;
        d.node_id = node_id;
        d.net_id = net_id;
        d.period_s = period_s > 1.0 ? period_s : 1.0;
        d.duration_s = duration_s > 0.5 ? duration_s : 0.5;
        d.next_s = from_s;
        d.until_s = until_s;
        s_.dummy_traffic.push_back(d);
    }

    int AddNode(const std::string& name, int platform_id, Side side, bool c2_center = false, int parent = 0) {
        NodeState n;
        n.id = s_.next_id++;
        n.name = name;
        n.platform_id = platform_id;
        n.side = side;
        n.c2_center = c2_center;
        n.parent = parent;
        n.status = C2Status::Connected;  // scenarios start with command intact
        n.last_parent_contact_s = s_.time_s;
        s_.nodes.push_back(n);
        return n.id;
    }

    void AttachSensor(int node_id, int sensor_id) {
        if (NodeState* n = s_.FindNode(node_id)) n->sensors.push_back(sensor_id);
    }

    int AddNet(const std::string& name, const std::string& radio_id, double freq_hz, const std::vector<int>& members) {
        NetState net;
        net.id = s_.next_id++;
        net.name = name;
        net.radio_def = s_.lib.Radio(radio_id);
        net.freq_hz = freq_hz;
        net.members = members;
        if (net.radio_def < 0) return 0;
        const RadioDef& rd = s_.lib.radios[static_cast<std::size_t>(net.radio_def)];
        for (int m : members) {
            NodeState* n = s_.FindNode(m);
            if (n == nullptr) continue;
            n->nets.push_back(net.id);
            if (n->radio_emitter_id == 0) {
                EmitterDef ed;
                ed.id = rd.id;
                ed.name = rd.name;
                ed.kind = EmitterKind::Comms;
                ed.peak_power_w = rd.power_w;
                ed.antenna.gain_db = rd.antenna_gain_db;
                ed.waveform.freq_hz = freq_hz;
                ed.waveform.bandwidth_hz = rd.bandwidth_hz;
                ed.scan = ScanKind::Omni;
                ed.source = rd.source;
                ed.confidence = rd.confidence;
                n->radio_emitter_id = AddEmitter(n->platform_id, ed);
            }
        }
        s_.nets.push_back(net);
        return net.id;
    }

    int AddTdoaGroup(const std::string& name, const std::vector<int>& esm_ids, int node_id, bool solve_3d = true,
                     double assumed_altitude_m = 0.0) {
        TdoaGroup g;
        g.id = s_.next_id++;
        g.name = name;
        g.esm_ids = esm_ids;
        g.node_id = node_id;
        g.solve_3d = solve_3d;
        g.assumed_altitude_m = assumed_altitude_m;
        s_.tdoa_groups.push_back(g);
        return g.id;
    }

    int AddChaff(const Vec3& position, double rcs_m2, double lifetime_s = 60.0) {
        ChaffCloud c;
        c.id = s_.next_id++;
        c.position = position;
        c.velocity = s_.env.wind_mps;
        c.rcs_m2 = rcs_m2;
        c.created_s = s_.time_s;
        c.lifetime_s = lifetime_s;
        s_.chaff.push_back(c);
        return c.id;
    }

    void SetPlatformState(int id, const Vec3& position, const Vec3& velocity) {
        if (Platform* p = s_.FindPlatform(id)) {
            p->position = position;
            p->velocity = velocity;
        }
    }

    // Destroying a platform silences everything on it and kills its nodes.
    void DestroyPlatform(int id) {
        Platform* p = s_.FindPlatform(id);
        if (p == nullptr || !p->alive) return;
        p->alive = false;
        for (RadarState& r : s_.radars)
            if (r.platform_id == id) r.on = false;
        for (JammerState& j : s_.jammers)
            if (j.platform_id == id) j.on = false;
        for (NodeState& n : s_.nodes) {
            if (n.platform_id != id || !n.alive) continue;
            n.alive = false;
            s_.Log(EmEventKind::NodeDestroyed, n.id, id, n.name + " destroyed");
        }
    }

    void Step(double dt) {
        using Clock = std::chrono::steady_clock;
        Clock::time_point t0;
        auto lap = [&](std::uint64_t& bucket) {
            if (!rates_.profile) return;
            const Clock::time_point t1 = Clock::now();
            bucket += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            t0 = t1;
        };
        if (rates_.profile) t0 = Clock::now();
        s_.time_s += dt;
        ++s_.tick;
        UpdatePlatforms(s_);
        lap(s_.stats.ns_platforms);
        ApplyEmconSteps(s_, emcon_);
        for (SamDoctrine& d : doctrines_) RunSamDoctrine(s_, d);
        UpdateDecoys(s_);
        UpdateJammerTuning(s_);
        UpdateEmitters(s_, dt);
        for (ChaffCloud& c : s_.chaff) {
            c.position += c.velocity * dt;
            c.position.z -= 1.5 * dt;  // chaff falls slowly
        }
        std::vector<ChaffCloud> live;
        for (const ChaffCloud& c : s_.chaff)
            if (s_.time_s - c.created_s <= c.lifetime_s) live.push_back(c);
        s_.chaff.swap(live);
        lap(s_.stats.ns_emitters);
        s_.threads = rates_.threads < 1 ? 1 : rates_.threads;
        RunRadars(s_, dt);
        lap(s_.stats.ns_radars);
        if (s_.time_s + 1e-9 >= next_esm_s_) {
            const double window = rates_.esm_period_s > dt ? rates_.esm_period_s : dt;
            next_esm_s_ = s_.time_s + window;
            s_.threads = rates_.threads < 1 ? 1 : rates_.threads;
            RunEsm(s_, window);
            lap(s_.stats.ns_esm);
            RunTdoa(s_);
            lap(s_.stats.ns_tdoa);
        }
        lap(s_.stats.ns_esm);
        if (s_.time_s + 1e-9 >= next_knowledge_s_) {
            next_knowledge_s_ = s_.time_s + rates_.knowledge_period_s;
            for (NodeState& n : s_.nodes) IngestOwnSensors(s_, n);
        }
        lap(s_.stats.ns_knowledge);
        const bool links = s_.time_s + 1e-9 >= next_links_s_;
        if (links) next_links_s_ = s_.time_s + rates_.link_period_s;
        RunDummyTraffic(s_);
        RunComms(s_, links);
        if (s_.time_s + 1e-9 >= next_age_s_) {
            next_age_s_ = s_.time_s + rates_.age_period_s;
            for (NodeState& n : s_.nodes)
                if (n.alive) n.kb.Age(s_.time_s);
        }
        lap(s_.stats.ns_comms);
    }

private:
    EmScene s_;
    EmRates rates_;
    std::vector<SamDoctrine> doctrines_;
    std::vector<EmconStep> emcon_;
    double next_esm_s_ = 0.0;
    double next_knowledge_s_ = 0.0;
    double next_links_s_ = 0.0;
    double next_age_s_ = 0.0;
};

}  // namespace em
}  // namespace simcore
