// simcore/em_link.hpp — the electromagnetic layer wired to the Osa's search and tracking radars.
//
// The gameplay engagement model (engagement.hpp) decides what the crew's scopes show and whether the tracking
// radar locks from simple rules (a range window, a clutter height, an azimuth tolerance). With a scenario's
// "em" block it asks this link instead: the unit and its targets are platforms of an em::EmWorld, the search
// radar (СОЦ) and the tracking radar (ССЦ) are calibrated radars from data/em/<system>.json, and
//   * the search picture is the search radar's confirmed tracks (a blip is there for a revolution when the
//     antenna's beam swept the target and the echo was detected);
//   * the tracking radar is pointed by the console's wheels (a designation point), looks only in its beam and
//     range gate, and locks when an echo in the gate is detected; it holds the lock while echoes keep coming.
// Detection comes from the radar equation (calibrated to the published ranges), terrain diffraction, ground
// reflection, MTI and clutter: not from a table. The outcome of a launch (hit or miss) stays with the gameplay
// model. TV tracking and the IFF replies are not electromagnetic here and stay as they were.
//
// Identity: the crew's picture never reads the debug truth fields of the EM layer. A confirmed track becomes a
// contact whose label is the scenario target it lines up with in angle (a referee-side association, ResolveTarget:
// the game needs the scenario's target id to resolve a launch); a track that lines up with no target (chaff, a
// repeater's false echo, a ghost) is shown as a ghost contact "T<n>", so deception reaches the scope. Positions
// the crew sees are always the radar's measured ones. A radar lock on a ghost is not a capture.
//
// Also in the link's world: the unit is a node whose knowledge base holds its own radar tracks and can take reports
// from elsewhere (E4's PU-12); scripted jammers and chaff clouds (the scenario's em.jammers / em.chaff); the
// antenna stands at the radar definition's height above the ground, the height the radar was calibrated at.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "simcore/c2_sources.hpp"
#include "simcore/em/data.hpp"
#include "simcore/em/world.hpp"
#include "simcore/geo.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// A scripted jammer: on a scenario target (self-screening) or at a fixed point (stand-off), aimed at the unit.
struct EmJammerDef {
    std::string jammer_id;  // id in the library (jammers[])
    std::string target_id;  // scenario target carrying it; empty = stand-off at `position`
    Vec3 position;          // stand-off position (ENU)
    double on_at_s = 0.0;
    double off_at_s = -1.0;  // < 0: stays on
};

// A scripted chaff drop: a cloud at the target's position at a time.
struct EmChaffDef {
    std::string target_id;
    double at_s = 0.0;
    double rcs_m2 = 30.0;
    double lifetime_s = 60.0;
};

// A decoy emitter copying the unit's search radar, at a dummy position (roadmap E7). `mode`: with the radar, while the
// radar is off, or always.
struct EmDecoyDef {
    Vec3 position;
    em::DecoyMode mode = em::DecoyMode::WithRadar;
};

// A dummy position (roadmap 7.6): an echo of its own and, optionally, a decoy emitter copying the search radar.
struct EmDummyDef {
    Vec3 position;
    double rcs_m2 = 20.0;
    bool decoy = false;
    em::DecoyMode mode = em::DecoyMode::WhenRadarOff;
};

// What a scenario asks for: the library file and which of its radars are the unit's search and tracking radars.
struct EmLinkDef {
    std::string library_file;  // relative to the data folder, e.g. "em/osa_ak.json"
    std::string search_radar;  // id in the library, role "search"
    std::string track_radar;   // id in the library, role "track"
    std::vector<std::string> extra_libraries;  // more files merged in (jammers, threat libraries): "em/nato_1985.json"
    std::vector<EmJammerDef> jammers;
    std::vector<EmChaffDef> chaff;
    std::vector<EmDecoyDef> decoys;
    std::vector<EmDummyDef> dummies;  // a prepared position's dummy positions
    double unit_rcs_m2 = 0.0;         // the vehicle's echo for the other side (0: the EM layer's default)
    Vec3 wind_mps;  // chaff drift
    double rain_mmh = 0.0;  // rain rate over the scene (ITU-R P.838)
    em::EmLibrary library;
};

// Loads the library and checks the two radars exist, have the right roles and calibrate.
inline bool LoadEmLinkDef(const std::string& data_dir, const std::string& library_file, const std::string& search_id,
                          const std::string& track_id, EmLinkDef& out, std::string* error = nullptr,
                          const std::vector<std::string>& extra_libraries = {}) {
    auto fail = [&](const std::string& what) {
        if (error != nullptr) *error = what;
        return false;
    };
    em::EmLibrary lib;
    const em::EmLoadResult res = em::LoadEmLibraryFile(data_dir + "/" + library_file, lib);
    if (!res.ok) return fail(res.error);
    for (const std::string& extra : extra_libraries) {
        const em::EmLoadResult more = em::LoadEmLibraryFile(data_dir + "/" + extra, lib);
        if (!more.ok) return fail(more.error);
    }
    const em::Environment env;
    for (const auto& want : {std::make_pair(&search_id, em::RadarRole::Search), std::make_pair(&track_id, em::RadarRole::Track)}) {
        const int idx = lib.Radar(*want.first);
        if (idx < 0) return fail("radar '" + *want.first + "' is not in " + library_file);
        em::RadarDef d = lib.radars[static_cast<std::size_t>(idx)];
        if (d.role != want.second) {
            return fail("radar '" + *want.first + "' is not a " + (want.second == em::RadarRole::Search ? "search" : "track") + " radar");
        }
        if (!em::Calibrate(d, env)) return fail("radar '" + *want.first + "' has no usable calibration anchor");
    }
    out.library_file = library_file;
    out.search_radar = search_id;
    out.track_radar = track_id;
    out.extra_libraries = extra_libraries;
    out.library = std::move(lib);
    return true;
}

class EmSensorLink {
public:
    // One EM tick. The search radar sweeps a few degrees per tick at 33 rpm, the tracking radar looks once.
    static constexpr double kStepS = 0.1;

    // The auto-capture circuit takes the lock once the radar has echoed `hits` times in its last `looks` looks (set
    // from the scenario's tuning, data/gameplay/<system>.json).
    void SetCaptureRule(int looks, int hits) {
        capture_looks_ = looks > 1 ? looks : 1;
        capture_hits_ = hits < capture_looks_ ? hits : capture_looks_;
    }

    // What the search radar shows for one target: where it measured it (true azimuth from north) and how far.
    struct Contact {
        std::string target_id;
        Vec3 position;  // the track's estimate, ENU
        bool ghost = false;  // lines up with no target: chaff, a false echo (target_id is "T<track number>")
        bool iff = false;    // a friendly reply came back with this track's echo within the last revolution or so
        double range_m = 0.0;
        double true_az_deg = 0.0;  // -180..180
        double el_deg = 0.0;
        int beam = -1;  // the search radar's elevation beam its last echo came in (-1 unknown)
        bool no_range = false;  // angles only (the tracking radar on a jammer): range_m and position mean nothing
    };

    EmSensorLink(const GeoPoint& origin, const Terrain* terrain, unsigned seed, const EmLinkDef& def, const Vec3& unit_position)
        : world_(origin, terrain, seed), effects_(def) {
        std::string err;
        if (!world_.SetLibrary(def.library, &err)) {
            error_ = err;
            return;
        }
        if (def.wind_mps.LengthSquared() > 0.0 || def.rain_mmh > 0.0) {
            em::Environment env = world_.Scene().env;
            env.wind_mps = def.wind_mps;
            env.rain_rate_mmh = def.rain_mmh;
            world_.SetEnvironment(env);
        }
        const int search_def = world_.Scene().lib.Radar(def.search_radar);
        antenna_height_m_ = search_def >= 0 ? world_.Scene().lib.radars[static_cast<std::size_t>(search_def)].antenna_height_m : 0.0;
        unit_ = last_unit_ = unit_position;
        em::Platform unit;
        unit.name = "Own unit";
        unit.side = em::Side::Red;
        unit.kind = em::PlatformKind::Ground;
        unit.position = Antenna();
        if (def.unit_rcs_m2 > 0.0) unit.rcs = em::RcsModel{def.unit_rcs_m2, def.unit_rcs_m2, def.unit_rcs_m2};
        unit_id_ = world_.AddPlatform(unit);
        search_ = world_.AddRadar(unit_id_, def.search_radar, false);
        track_ = world_.AddRadar(unit_id_, def.track_radar, false);
        if (search_ == 0 || track_ == 0) {
            error_ = "radar not found in the library";
            return;
        }
        // The unit is a node: its radars are cabled to it, so its own tracks become knowledge it can fuse with
        // reports from elsewhere (the battery's PU-12, roadmap phase 7 / E4).
        node_ = world_.AddNode("Own unit", unit_id_, em::Side::Red);
        world_.AttachSensor(node_, search_);
        world_.AttachSensor(node_, track_);
        if (em::RadarState* sr = Radar(search_)) sr->iff_interrogating = false;  // until the console interrogates
        for (const EmDummyDef& dm : def.dummies) {
            const double mast = antenna_height_m_ > 0.0 ? antenna_height_m_ : 5.0;
            const int dp = world_.AddGroundPlatform("Dummy position", em::Side::Red, dm.position.x, dm.position.y, mast,
                                                    em::RcsModel{dm.rcs_m2, dm.rcs_m2, dm.rcs_m2});
            dummy_platforms_.push_back(dp);
            if (dm.decoy) decoy_emitters_.push_back(world_.AddDecoy(dp, search_, dm.mode));
        }
        for (const EmDecoyDef& dd : def.decoys) {
            const double mast = antenna_height_m_ > 0.0 ? antenna_height_m_ : 5.0;
            const int dp = world_.AddGroundPlatform("Decoy", em::Side::Red, dd.position.x, dd.position.y, mast);
            decoy_emitters_.push_back(world_.AddDecoy(dp, search_, dd.mode));
        }
        jammer_ids_.assign(def.jammers.size(), 0);
        chaff_done_.assign(def.chaff.size(), 0);
        for (std::size_t i = 0; i < def.jammers.size(); ++i) {
            if (!def.jammers[i].target_id.empty()) continue;  // on a target: made when the target first appears
            em::Platform sp;
            sp.name = "Stand-off jammer";
            sp.side = em::Side::Blue;
            sp.kind = em::PlatformKind::FixedWing;
            sp.rcs = em::RcsModel{10.0, 10.0, 10.0};
            sp.position = def.jammers[i].position;
            const int pid = world_.AddPlatform(sp);
            AddScriptedJammer(i, pid);
        }
    }

    // A radio source (a PU-12, an EWR site, a neighbour) on the ground at `enu`, on a two-member net with the unit using
    // library radio `radio_id`. Its link to the unit is then decided by propagation, terrain, jamming and the unit's own
    // radars (co-site), as for any radio in the EM layer. Returns false if the radio is not in the library.
    // `freq_hz` > 0 puts the source's net on that frequency (the scenario's frequency plan); 0: the middle of the radio's band.
    bool AddRadioSource(const std::string& id, const Vec3& enu, const std::string& radio_id, double freq_hz = 0.0) {
        em::EmScene& s = world_.Scene();
        const int rd = s.lib.Radio(radio_id);
        if (!Ok() || rd < 0) return false;
        const em::RadioDef& r = s.lib.radios[static_cast<std::size_t>(rd)];
        const int pid = world_.AddGroundPlatform(id, em::Side::Red, enu.x, enu.y, r.antenna_height_m);
        const int node = world_.AddNode(id, pid, em::Side::Red);
        const double f = freq_hz > 0.0 ? freq_hz : 0.5 * (r.freq_min_hz + r.freq_max_hz);
        const int net = world_.AddNet("net " + id, radio_id, f, {node, node_});
        if (net == 0) return false;
        radio_sources_.push_back({id, node, net, {}});
        return true;
    }

    // A voice message from radio source `id` goes on the air from start_s to end_s (times the sender worked out). `tag`
    // names it to VoiceOutcome. It radiates (ESM can hear it) and the link budget decides at its end whether the unit got it.
    void SendVoice(const std::string& id, long long tag, double start_s, double end_s) {
        em::EmScene& s = world_.Scene();
        for (RadioSource& rs : radio_sources_) {
            if (rs.id != id) continue;
            em::NetState* net = s.FindNet(rs.net);
            const em::NodeState* a = s.FindNode(rs.node);
            if (net == nullptr || a == nullptr) return;
            const long long mid = em::SendVoice(s, *net, *a, node_, start_s, end_s);
            rs.voice.emplace_back(tag, mid);
            return;
        }
    }

    bool HasRadioSource(const std::string& id) const {
        for (const RadioSource& rs : radio_sources_) {
            if (rs.id == id) return true;
        }
        return false;
    }

    // 1 delivered, 0 lost, -1 not decided yet (or unknown).
    int VoiceOutcome(const std::string& id, long long tag) {
        em::EmScene& s = world_.Scene();
        for (RadioSource& rs : radio_sources_) {
            if (rs.id != id) continue;
            for (const auto& v : rs.voice) {
                if (v.first != tag) continue;
                for (const em::VoiceResult& r : s.voice_results) {
                    if (r.message_id == v.second && r.node == node_) {
                        const int out = r.delivered ? 1 : 0;
                        s.voice_results.erase(s.voice_results.begin() + (&r - s.voice_results.data()));
                        rs.voice.erase(rs.voice.begin() + (&v - rs.voice.data()));
                        return out;
                    }
                }
            }
        }
        return -1;
    }

    // Loss the unit's own receiving chain adds to every radio source's words (squelch, antenna): see em::NodeState.
    void SetUnitRxLossDb(double db) {
        if (em::NodeState* n = world_.Scene().FindNode(node_)) n->rx_extra_loss_db = db;
    }
    double UnitRxLossDb() const {
        const em::NodeState* n = world_.Scene().FindNode(node_);
        return n != nullptr ? n->rx_extra_loss_db : 0.0;
    }

    // Chance a transmission from radio source `id` reaches the unit now (the EM layer's own delivery law); -1: unknown.
    double RadioDeliveryProbability(const std::string& id) {
        em::EmScene& s = world_.Scene();
        for (const RadioSource& rs : radio_sources_) {
            if (rs.id != id) continue;
            const em::NetState* net = s.FindNet(rs.net);
            const em::NodeState* a = s.FindNode(rs.node);
            const em::NodeState* b = s.FindNode(node_);
            if (net == nullptr || a == nullptr || b == nullptr) return -1.0;
            const em::LinkState l = em::EvaluateLink(s, *net, *a, *b);
            return l.up ? 1.0 / (1.0 + std::exp(-l.margin_db / 2.0)) : 0.0;
        }
        return -1.0;
    }

    // The antenna's place: the unit's x/y at the height above the ground the radar was calibrated for.
    Vec3 Antenna() const { return AntennaAt(unit_); }
    Vec3 AntennaAt(const Vec3& hull) const {
        const em::EmScene& s = world_.Scene();
        const double ground = s.GetTerrain().HeightAt(hull.x, hull.y);
        const double d2 = hull.x * hull.x + hull.y * hull.y;
        return {hull.x, hull.y, ground + antenna_height_m_ - s.frame.Origin().alt_m - d2 / (2.0 * wgs84::kMeanRadius)};
    }

    bool Ok() const { return error_.empty(); }
    // The decoy emitters copying the search radar (scenario own_unit.em.decoys), in order.
    const std::vector<int>& DecoyEmitters() const { return decoy_emitters_; }
    // The dummy positions' ground echoes (scenario own_unit.prepared_position), in order.
    const std::vector<int>& DummyPlatforms() const { return dummy_platforms_; }
    int UnitPlatformId() const { return unit_id_; }
    const std::string& Error() const { return error_; }
    em::EmWorld& World() { return world_; }
    const em::EmWorld& World() const { return world_; }
    double Time() const { return world_.Time(); }

    void SetTerrain(const Terrain* terrain) { world_.SetTerrain(terrain); }

    // The unit's antenna position now (the hull moved). Its velocity follows from the movement between steps.
    void SetUnit(const Vec3& position) { unit_ = position; }

    // A target as it is now. Added on first sight; once not alive it stops echoing.
    void SyncTarget(const std::string& id, bool alive, const Vec3& position, const Vec3& velocity, double rcs_m2, bool friendly) {
        int pid = PlatformOf(id);
        if (pid == 0) {
            if (!alive) return;
            em::Platform p;
            p.name = id;
            p.side = friendly ? em::Side::Red : em::Side::Blue;
            p.kind = em::PlatformKind::FixedWing;
            p.rcs = em::RcsModel{rcs_m2, rcs_m2, rcs_m2};  // one number in the scenario file: the same from every aspect
            p.iff_reply = friendly;
            p.position = position;
            p.velocity = velocity;
            pid = world_.AddPlatform(p);
            targets_.emplace_back(id, pid);
        }
        em::Platform* p = world_.Scene().FindPlatform(pid);
        if (p == nullptr || !p->alive) return;
        if (!alive) {
            world_.DestroyPlatform(pid);
            return;
        }
        world_.SetPlatformState(pid, position, velocity);
    }

    // The console's state: is each radar radiating (and, for the search radar, is its MTI selected).
    void SetSearch(bool radiating, bool mti) {
        em::RadarState* r = Radar(search_);
        if (r == nullptr) return;
        r->on = radiating;
        const em::RadarDef& d = DefOf(*r);
        r->mti = d.has_mti && mti ? em::MtiMode::Internal : em::MtiMode::Off;
    }

    // The elevation beams the search antenna steps through, one per revolution (indices into the radar's beams).
    // A new programme takes effect at the next revolution.
    void SetSearchBeamProgram(const std::vector<int>& program) {
        em::RadarState* r = Radar(search_);
        if (r == nullptr) return;
        const int count = static_cast<int>(DefOf(*r).emitter.beams.size());
        std::vector<int> valid;
        for (int b : program)
            if (b >= 0 && b < count) valid.push_back(b);
        if (valid.empty() || valid == r->beam_program) return;
        r->beam_program = valid;
    }

    // The beam the search antenna is radiating this revolution (-1 if it has none).
    int ActiveSearchBeam() const {
        const em::RadarState* r = Radar(search_);
        const em::EmitterState* e = r != nullptr ? world_.Scene().FindEmitter(r->emitter_id) : nullptr;
        return e != nullptr && !DefOf(*r).emitter.beams.empty() ? e->beam_index : -1;
    }

    // The IFF interrogator (built into the search antenna) is transmitting: friendly transponders in the beam reply
    // with the echo, and the reply shows on that track (Contact::iff). Off, nobody replies.
    void SetInterrogating(bool on) {
        if (em::RadarState* r = Radar(search_)) r->iff_interrogating = on;
    }

    // The tracking radar: radiating; its СДЦ (staggered repetition periods: a repeater that walks the gate in becomes
    // non-synchronous) and its pull-off circuit (УВ. ПОМЕХА: the video after the tracked echo is blanked, so a repeater
    // that walks the gate out can't take it): roadmap 9.4, voenizdat_osa_1991 p80-81.
    void SetTrack(bool radiating, bool mti = false, bool rgpo_protect = false) {
        em::RadarState* r = Radar(track_);
        if (r == nullptr) return;
        if (!radiating && r->on) ClearLock();
        r->on = radiating;
        r->prf_staggered = mti;
        r->rgpo_trailing_blank = rgpo_protect;
    }
    // A repeater holds the tracking radar's range gate (range-gate pull-off): the operator sees the target's mark and
    // the jamming pulse split apart on the range indicator.
    bool RangePulled() const {
        const em::RadarState* r = Radar(track_);
        return r != nullptr && r->rgpo_captured;
    }

    // Point the tracking radar at a place for the next ticks: where the console's wheels say (azimuth from north,
    // elevation, range). It locks onto what its beam and range gate find there.
    void Designate(double true_az_deg, double el_deg, double range_m) {
        em::RadarState* r = Radar(track_);
        if (r == nullptr) return;
        r->has_designation = true;
        r->designation = Antenna() + DirectionFromAzEl(DegToRad(true_az_deg), DegToRad(el_deg)) * range_m;
    }

    void ClearDesignation() {
        if (em::RadarState* r = Radar(track_)) r->has_designation = false;
    }

    // Break the tracking radar's lock (the operator dropped it, or the antenna was released).
    void ClearLock() {
        em::RadarState* r = Radar(track_);
        if (r == nullptr) return;
        r->locked_track = 0;
        r->tracker.Clear();
        r->has_designation = false;
        r->rgpo_captured = false;
        r->rgpo_offset_m = 0.0;
        r->on_jam = false;
    }

    // The tracking radar follows a noise jammer in angle because its echo is drowned in the jamming (the radar's
    // angle_track_on_jam): which target that direction lines up with, and the measured direction. No range: the
    // contact's range_m is 0 and no_range is set. The gate stays where the last designation (the range wheel) put it,
    // so the lock comes back by itself when the echo burns through.
    bool JamTracked(std::string* target_id, Contact* angles = nullptr) const {
        const em::RadarState* r = Radar(track_);
        if (r == nullptr || !r->on || !r->on_jam) return false;
        const Vec3 dir = DirectionFromAzEl(r->jam_az_rad, r->jam_el_rad);
        const std::string id = ResolveTarget(Antenna() + dir * 10000.0, false, std::vector<std::string>());
        if (id.empty()) return false;
        if (target_id != nullptr) *target_id = id;
        if (angles != nullptr) {
            *angles = MakeContact(id, Antenna() + dir * 10000.0);
            angles->range_m = 0.0;
            angles->no_range = true;
        }
        return true;
    }
    // J/N of the jamming the tracking radar follows (dB; -300 when it follows none).
    double JamToNoiseDb() const {
        const em::RadarState* r = Radar(track_);
        return r != nullptr && r->on && r->on_jam ? r->jam_jn_db : -300.0;
    }

    void Advance(double dt) {
        if (!Ok()) return;  // a link that failed to build does nothing (the scenario falls back to the gameplay rules)
        accum_ += dt;
        while (accum_ + 1e-9 >= kStepS) {
            accum_ -= kStepS;
            em::Platform* u = world_.Scene().FindPlatform(unit_id_);
            if (u != nullptr) {
                world_.SetPlatformState(unit_id_, Antenna(), (unit_ - last_unit_) / kStepS);
                last_unit_ = unit_;
            }
            RunEffects();
            world_.Step(kStepS);
            NoteBeams();
            NoteLooks();
        }
    }

    // The search radar's picture: confirmed tracks whose last look was a hit, nearest first. A blip is held for one
    // full beam programme (a target seen only in beam I, in the automatic programme, is refreshed every third turn);
    // a target that leaves the radar's coverage, such as overhead, drops off the scope. A track that lines up with a
    // scenario target carries its id (once: a second track of the same target is a ghost); any other is a ghost.
    std::vector<Contact> SearchPicture() const {
        std::vector<Contact> out;
        const em::RadarState* r = Radar(search_);
        if (r == nullptr || !r->on) return out;
        const em::RadarDef& d = DefOf(*r);
        const double turns = r->beam_program.empty() ? 1.0 : static_cast<double>(r->beam_program.size());
        const double hold_s = turns * d.emitter.scan_period_s + 0.3;
        std::vector<std::string> claimed;
        for (const em::SensorTrack& t : r->tracker.Tracks()) {
            if (t.status != em::TrackStatus::Confirmed || t.consecutive_misses != 0) continue;
            if (world_.Time() - t.last_update_s > hold_s) continue;
            std::string id = ResolveTarget(t.Position(), true, claimed);
            const bool ghost = id.empty();
            if (ghost) {
                id = "T" + std::to_string(t.id);
            } else {
                claimed.push_back(id);
            }
            Contact c = MakeContact(id, t.Position());
            c.ghost = ghost;
            c.iff = !ghost && t.iff_replies > 0 && world_.Time() - t.last_iff_s <= kIffHoldS;
            c.beam = BeamOfTrack(t.id);
            out.push_back(c);
        }
        std::sort(out.begin(), out.end(), [](const Contact& a, const Contact& b) { return a.range_m < b.range_m; });
        return out;
    }

    // The tracking radar holds a target: who, and where its filter puts it now.
    bool Locked(std::string* target_id, Contact* estimate = nullptr) const {
        const em::RadarState* r = Radar(track_);
        if (r == nullptr || !r->on || r->locked_track == 0) return false;
        const em::SensorTrack* t = r->tracker.Find(r->locked_track);
        if (t == nullptr) return false;
        // By angle only: a repeater walking the range gate off must not make the lock look like a ghost.
        const std::string id = ResolveTarget(t->Position(), false, std::vector<std::string>());
        if (id.empty()) return false;
        if (target_id != nullptr) *target_id = id;
        if (estimate != nullptr) {
            em::SensorTrack now = *t;
            em::PredictTrack(now, world_.Time(), DefOf(*r).tracker.process_noise_accel);
            *estimate = MakeContact(id, now.Position());
        }
        return true;
    }

    // The lock is firm enough for the auto-capture circuit to take it: the radar has echoed on at least capture_hits_ of
    // its last capture_looks_ looks (the scenario's tuning; Osa: 22 of 40, a local Pd of about 0.55, reasoning in
    // data/gameplay/osa_ak.json). An existing lock is kept by Locked() alone.
    bool Captured(std::string* target_id, Contact* estimate = nullptr) const {
        if (!Locked(target_id, estimate)) return false;
        if (static_cast<int>(look_record_.size()) < capture_looks_) return false;
        int hits = 0;
        for (char hit : look_record_) hits += hit;
        return hits >= capture_hits_;
    }

    // The unit as a node of the EM world: its radars' tracks as knowledge, fused with whatever else reaches it.
    // The unit's node in the EM scene: what a multiplayer server replicates to this crew (em/replication.hpp).
    int UnitNodeId() const { return node_; }
    const em::KnowledgeBase& Knowledge() const { return world_.Scene().FindNode(node_)->kb; }
    std::vector<em::FusedTrack> UnitPicture() const { return Knowledge().Picture(world_.Time()); }
    // A report from another node (a PU-12 designation, a neighbour's track) joins the unit's knowledge.
    void ReceiveReport(em::KnowledgeTrack report) {
        report.source = em::InfoSource::Network;
        report.received_s = world_.Time();
        if (em::NodeState* n = world_.Scene().FindNode(node_)) n->kb.Upsert(report);
    }

    // Referee-side view of the range indicator: whether the tracking radar's beam would show this target's echo now: inside the beam
    // (not only the range gate), strong enough to be detected. The answer holds for half a second at a time, as an
    // indicator integrates. Chaff echoes are not drawn here yet.
    bool EchoVisible(const std::string& target_id) {
        em::RadarState* r = Radar(track_);
        const int pid = PlatformOf(target_id);
        if (r == nullptr || !r->on || pid == 0) return false;
        em::EmScene& s = world_.Scene();
        const em::Platform* p = s.FindPlatform(unit_id_);
        const em::Platform* t = s.FindPlatform(pid);
        const em::EmitterState* e = s.FindEmitter(r->emitter_id);
        if (p == nullptr || t == nullptr || e == nullptr || !t->alive) return false;
        const em::RadarDef& d = DefOf(*r);
        const Vec3 tl = t->position - p->position;
        const double range = tl.Length();
        if (range > d.max_range_m || range < d.min_range_m) return false;
        const Vec3 boresight = DirectionFromAzEl(e->pointing_az_rad, e->pointing_el_rad);
        if (AngleBetween(tl, boresight) > 1.5 * d.emitter.antenna.beamwidth_az_rad) return false;
        const em::ElevationBeam beam{e->pointing_el_rad, d.emitter.antenna.beamwidth_el_rad};
        const double rcs = em::RcsToward(t->rcs, t->velocity, (p->position - t->position).Normalized());
        const double vr = em::RadialSpeed(p->position, p->velocity, t->position, t->velocity);
        const long long bucket = 1000000000LL + static_cast<long long>(s.time_s * 2.0);
        return em::EvaluateLook(s, *r, d, *e, *p, *t, rcs, vr, beam, e->pointing_az_rad, e->pointing_el_rad,
                                em::LookKey(r->id, t->id, bucket))
            .detected;
    }

    // Where the tracking antenna points now (degrees; azimuth from north).
    double AntennaAzimuthDeg() const {
        const em::RadarState* r = Radar(track_);
        const em::EmitterState* e = r != nullptr ? world_.Scene().FindEmitter(r->emitter_id) : nullptr;
        return e != nullptr ? RadToDeg(e->pointing_az_rad) : 0.0;
    }
    double AntennaElevationDeg() const {
        const em::RadarState* r = Radar(track_);
        const em::EmitterState* e = r != nullptr ? world_.Scene().FindEmitter(r->emitter_id) : nullptr;
        return e != nullptr ? RadToDeg(e->pointing_el_rad) : 0.0;
    }

    const em::RadarState* SearchRadar() const { return Radar(search_); }
    const em::RadarState* TrackRadar() const { return Radar(track_); }

private:
    em::RadarState* Radar(int id) {
        for (em::RadarState& r : world_.Scene().radars)
            if (r.id == id) return &r;
        return nullptr;
    }
    const em::RadarState* Radar(int id) const {
        for (const em::RadarState& r : world_.Scene().radars)
            if (r.id == id) return &r;
        return nullptr;
    }
    const em::RadarDef& DefOf(const em::RadarState& r) const {
        return world_.Scene().lib.radars[static_cast<std::size_t>(r.def_index)];
    }

    // Keep the last capture_looks_ looks (hit or miss) of the tracking radar's locked track.
    void NoteLooks() {
        const em::RadarState* r = Radar(track_);
        const em::SensorTrack* t = r != nullptr && r->locked_track != 0 ? r->tracker.Find(r->locked_track) : nullptr;
        if (t == nullptr) {
            look_record_.clear();
            recorded_track_ = 0;
            recorded_looks_ = 0;
            return;
        }
        if (r->locked_track != recorded_track_) {
            look_record_.clear();
            recorded_track_ = r->locked_track;
            recorded_looks_ = 0;
        }
        if (t->looks > recorded_looks_) {
            look_record_.push_back(static_cast<char>(t->history & 1u));
            recorded_looks_ = t->looks;
            if (static_cast<int>(look_record_.size()) > capture_looks_) look_record_.erase(look_record_.begin());
        }
    }

    // Remember which beam each search track was updated in this tick.
    void NoteBeams() {
        const em::RadarState* r = Radar(search_);
        const em::EmitterState* e = r != nullptr ? world_.Scene().FindEmitter(r->emitter_id) : nullptr;
        if (e == nullptr) return;
        for (const em::SensorTrack& t : r->tracker.Tracks()) {
            if (t.last_update_s < world_.Time() - 1e-9) continue;
            bool found = false;
            for (auto& b : beam_of_track_) {
                if (b.first == t.id) {
                    b.second = e->beam_index;
                    found = true;
                }
            }
            if (!found) beam_of_track_.emplace_back(t.id, e->beam_index);
        }
    }
    int BeamOfTrack(int track_id) const {
        for (const auto& b : beam_of_track_)
            if (b.first == track_id) return b.second;
        return -1;
    }

    // Scripted jammers and chaff (the scenario's em.jammers / em.chaff), applied at their times.
    void RunEffects() {
        const double now = world_.Time();
        for (std::size_t i = 0; i < effects_.jammers.size(); ++i) {
            const EmJammerDef& j = effects_.jammers[i];
            if (jammer_ids_[i] == 0 && !j.target_id.empty()) {
                const int pid = PlatformOf(j.target_id);
                const em::Platform* p = pid != 0 ? world_.Scene().FindPlatform(pid) : nullptr;
                if (p != nullptr && p->alive) AddScriptedJammer(i, pid);
            }
            if (jammer_ids_[i] == 0) continue;
            em::JammerState* js = world_.Scene().FindJammer(jammer_ids_[i]);
            if (js == nullptr) continue;
            const bool on = now >= j.on_at_s && (j.off_at_s < 0.0 || now < j.off_at_s);
            if (on != js->on) {
                js->on = on;
                world_.Scene().Log(on ? em::EmEventKind::JammerOn : em::EmEventKind::JammerOff, js->id, 0, j.jammer_id);
            }
            js->aim_at_point = true;
            js->aim_point = Antenna();
        }
        for (std::size_t i = 0; i < effects_.chaff.size(); ++i) {
            const EmChaffDef& c = effects_.chaff[i];
            if (chaff_done_[i] != 0 || now < c.at_s) continue;
            const int pid = PlatformOf(c.target_id);
            const em::Platform* p = pid != 0 ? world_.Scene().FindPlatform(pid) : nullptr;
            if (p == nullptr || !p->alive) continue;  // wait until the dropper exists
            chaff_done_[i] = 1;
            world_.AddChaff(p->position, c.rcs_m2, c.lifetime_s);
        }
    }

    void AddScriptedJammer(std::size_t index, int platform_id) {
        const int id = world_.AddJammer(platform_id, effects_.jammers[index].jammer_id, false);
        jammer_ids_[index] = id;
    }

    int PlatformOf(const std::string& id) const {
        for (const auto& t : targets_)
            if (t.first == id) return t.second;
        return 0;
    }
    // Referee-side association of a radar track with a scenario target by angle from the antenna (and, for the search
    // radar, by range): the nearest target inside the gates that no better track has claimed. It reads the targets'
    // true positions, because the game needs to know which target a lock or a launch is about; the crew's picture
    // only gets the label, never the truth, and a track with no target near it stays a ghost.
    std::string ResolveTarget(const Vec3& track_position, bool use_range, const std::vector<std::string>& claimed) const {
        const Vec3 a = Antenna();
        const Vec3 tr = track_position - a;
        const double tr_az = RadToDeg(AzimuthOf(tr)), tr_el = RadToDeg(ElevationOf(tr));
        std::string best;
        double best_score = 1e18;
        for (const auto& t : targets_) {
            if (std::find(claimed.begin(), claimed.end(), t.first) != claimed.end()) continue;
            const em::Platform* p = world_.Scene().FindPlatform(t.second);
            if (p == nullptr || !p->alive) continue;
            const Vec3 rel = p->position - a;
            double daz = std::fabs(RadToDeg(AzimuthOf(rel)) - tr_az);
            if (daz > 180.0) daz = 360.0 - daz;
            const double del = std::fabs(RadToDeg(ElevationOf(rel)) - tr_el);
            if (daz > kResolveAzDeg || del > kResolveElDeg) continue;
            if (use_range && std::fabs(rel.Length() - tr.Length()) > kResolveRangeM) continue;
            const double score = daz * daz + (del / 5.0) * (del / 5.0);
            if (score < best_score) {
                best_score = score;
                best = t.first;
            }
        }
        return best;
    }
    static constexpr double kIffHoldS = 2.5;  // a reply is shown for a bit more than one revolution
    static constexpr double kResolveAzDeg = 2.5;
    static constexpr double kResolveElDeg = 10.0;  // a search track's height is only known to its beam
    static constexpr double kResolveRangeM = 1500.0;

    Contact MakeContact(const std::string& id, const Vec3& position) const {
        Contact c;
        c.target_id = id;
        c.position = position;
        const Vec3 rel = position - Antenna();
        c.range_m = rel.Length();
        double az = RadToDeg(AzimuthOf(rel));
        az = std::fmod(az + 180.0, 360.0);
        if (az < 0.0) az += 360.0;
        c.true_az_deg = az - 180.0;
        c.el_deg = RadToDeg(ElevationOf(rel));
        return c;
    }

    em::EmWorld world_;
    EmLinkDef effects_;  // scripted jammers and chaff (the loaded library inside is not used again)
    std::string error_;
    int unit_id_ = 0;
    struct RadioSource {
        std::string id;
        int node;
        int net;
        std::vector<std::pair<long long, long long>> voice;  // (sender's tag, EM message id)
    };
    std::vector<RadioSource> radio_sources_;
    std::vector<int> decoy_emitters_;
    std::vector<int> dummy_platforms_;
    int node_ = 0;
    double antenna_height_m_ = 0.0;
    int capture_looks_ = 40;
    int capture_hits_ = 22;
    std::vector<int> jammer_ids_;
    std::vector<char> chaff_done_;
    int search_ = 0;
    int track_ = 0;
    Vec3 unit_;
    Vec3 last_unit_;
    double accum_ = 0.0;
    std::vector<std::pair<std::string, int>> targets_;  // scenario target id <-> EM platform id
    std::vector<char> look_record_;  // 1 = the locked track's look was a hit, newest last
    int recorded_track_ = 0;
    int recorded_looks_ = 0;
    std::vector<std::pair<int, int>> beam_of_track_;  // search track id -> the search beam of its latest update
};

// The EM layer as the C2 network's link model: sources with a radio link are carried or lost by the link budget.
class EmC2LinkModel : public C2LinkModel {
public:
    explicit EmC2LinkModel(EmSensorLink* link) : link_(link) {}
    double DeliveryProbability(const C2SourceDef& source, const Vec3&, double) override {
        const double p = link_->RadioDeliveryProbability(source.id);
        return p < 0.0 ? 1.0 : p;  // a source without an EM radio is not affected
    }
    bool HasRadio(const C2SourceDef& source) override { return link_->HasRadioSource(source.id); }
    void SendVoice(const C2SourceDef& source, long long tag, double start_s, double end_s) override {
        link_->SendVoice(source.id, tag, start_s, end_s);
    }
    int VoiceOutcome(const C2SourceDef& source, long long tag) override { return link_->VoiceOutcome(source.id, tag); }
    void SetReceiveLossDb(double db) override { link_->SetUnitRxLossDb(db); }

private:
    EmSensorLink* link_;
};

}  // namespace simcore
