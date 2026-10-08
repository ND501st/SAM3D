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
// The target identity of a track is the truth platform it came from (the plots carry it for debugging). The
// game needs the scenario's target id to resolve a launch; positions the crew see are the radar's measured ones.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "simcore/em/data.hpp"
#include "simcore/em/world.hpp"
#include "simcore/geo.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// What a scenario asks for: the library file and which of its radars are the unit's search and tracking radars.
struct EmLinkDef {
    std::string library_file;  // relative to the data folder, e.g. "em/osa_ak.json"
    std::string search_radar;  // id in the library, role "search"
    std::string track_radar;   // id in the library, role "track"
    em::EmLibrary library;
};

// Loads the library and checks the two radars exist, have the right roles and calibrate.
inline bool LoadEmLinkDef(const std::string& data_dir, const std::string& library_file, const std::string& search_id,
                          const std::string& track_id, EmLinkDef& out, std::string* error = nullptr) {
    auto fail = [&](const std::string& what) {
        if (error != nullptr) *error = what;
        return false;
    };
    em::EmLibrary lib;
    const em::EmLoadResult res = em::LoadEmLibraryFile(data_dir + "/" + library_file, lib);
    if (!res.ok) return fail(res.error);
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
    out.library = std::move(lib);
    return true;
}

class EmSensorLink {
public:
    // One EM tick. The search radar sweeps a few degrees per tick at 33 rpm, the tracking radar looks once.
    static constexpr double kStepS = 0.1;

    // What the search radar shows for one target: where it measured it (true azimuth from north) and how far.
    struct Contact {
        std::string target_id;
        Vec3 position;  // the track's estimate, ENU
        double range_m = 0.0;
        double true_az_deg = 0.0;  // -180..180
        double el_deg = 0.0;
    };

    EmSensorLink(const GeoPoint& origin, const Terrain* terrain, unsigned seed, const EmLinkDef& def, const Vec3& unit_position)
        : world_(origin, terrain, seed) {
        std::string err;
        if (!world_.SetLibrary(def.library, &err)) {
            error_ = err;
            return;
        }
        em::Platform unit;
        unit.name = "Own unit";
        unit.side = em::Side::Red;
        unit.kind = em::PlatformKind::Ground;
        unit.position = unit_position;
        unit_id_ = world_.AddPlatform(unit);
        unit_ = last_unit_ = unit_position;
        search_ = world_.AddRadar(unit_id_, def.search_radar, false);
        track_ = world_.AddRadar(unit_id_, def.track_radar, false);
        if (search_ == 0 || track_ == 0) error_ = "radar not found in the library";
    }

    bool Ok() const { return error_.empty(); }
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

    void SetTrack(bool radiating) {
        em::RadarState* r = Radar(track_);
        if (r == nullptr) return;
        if (!radiating && r->on) ClearLock();
        r->on = radiating;
    }

    // Point the tracking radar at a place for the next ticks: where the console's wheels say (azimuth from north,
    // elevation, range). It locks onto what its beam and range gate find there.
    void Designate(double true_az_deg, double el_deg, double range_m) {
        em::RadarState* r = Radar(track_);
        if (r == nullptr) return;
        r->has_designation = true;
        r->designation = unit_ + DirectionFromAzEl(DegToRad(true_az_deg), DegToRad(el_deg)) * range_m;
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
    }

    void Advance(double dt) {
        accum_ += dt;
        while (accum_ + 1e-9 >= kStepS) {
            accum_ -= kStepS;
            em::Platform* u = world_.Scene().FindPlatform(unit_id_);
            if (u != nullptr) {
                world_.SetPlatformState(unit_id_, unit_, (unit_ - last_unit_) / kStepS);
                last_unit_ = unit_;
            }
            world_.Step(kStepS);
        }
    }

    // The search radar's picture: confirmed tracks whose last look was a hit, one per target, nearest first.
    std::vector<Contact> SearchPicture() const {
        std::vector<Contact> out;
        const em::RadarState* r = Radar(search_);
        if (r == nullptr || !r->on) return out;
        for (const em::SensorTrack& t : r->tracker.Tracks()) {
            if (t.status != em::TrackStatus::Confirmed || t.consecutive_misses != 0) continue;
            const std::string id = TargetOf(t.debug_truth_platform);
            if (id.empty()) continue;
            bool dup = false;
            for (const Contact& c : out) dup = dup || c.target_id == id;
            if (dup) continue;
            out.push_back(MakeContact(id, t.Position()));
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
        const std::string id = TargetOf(t->debug_truth_platform);
        if (id.empty()) return false;
        if (target_id != nullptr) *target_id = id;
        if (estimate != nullptr) {
            em::SensorTrack now = *t;
            em::PredictTrack(now, world_.Time(), DefOf(*r).tracker.process_noise_accel);
            *estimate = MakeContact(id, now.Position());
        }
        return true;
    }

    // The lock is firm enough for the auto-capture circuit to take it: the radar has had it for kCaptureLooks looks and
    // echoed on at least kCaptureHits of them (a local Pd of about one half). A lock can start on a lucky echo
    // far out, where Swerling I detection is still a few tenths; the circuit waits for the target to come into
    // the range where the published capture figure (Pd 0.5) holds. An existing lock is kept by Locked() alone.
    static constexpr int kCaptureLooks = 20;
    static constexpr int kCaptureHits = 11;
    bool Captured(std::string* target_id, Contact* estimate = nullptr) const {
        if (!Locked(target_id, estimate)) return false;
        const em::RadarState* r = Radar(track_);
        const em::SensorTrack* t = r->tracker.Find(r->locked_track);
        if (t->looks < kCaptureLooks) return false;
        int hits = 0;
        for (int i = 0; i < kCaptureLooks; ++i) hits += static_cast<int>((t->history >> i) & 1u);
        return hits >= kCaptureHits;
    }

    // Whether the tracking radar's beam would show this target's echo on its range indicator now: inside the beam
    // (not only the range gate), strong enough to be detected. The answer holds for half a second at a time, as an
    // indicator integrates.
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

    int PlatformOf(const std::string& id) const {
        for (const auto& t : targets_)
            if (t.first == id) return t.second;
        return 0;
    }
    std::string TargetOf(int platform_id) const {
        for (const auto& t : targets_)
            if (t.second == platform_id) return t.first;
        return std::string();
    }

    Contact MakeContact(const std::string& id, const Vec3& position) const {
        Contact c;
        c.target_id = id;
        c.position = position;
        const Vec3 rel = position - unit_;
        c.range_m = rel.Length();
        double az = RadToDeg(AzimuthOf(rel));
        az = std::fmod(az + 180.0, 360.0);
        if (az < 0.0) az += 360.0;
        c.true_az_deg = az - 180.0;
        c.el_deg = RadToDeg(ElevationOf(rel));
        return c;
    }

    em::EmWorld world_;
    std::string error_;
    int unit_id_ = 0;
    int search_ = 0;
    int track_ = 0;
    Vec3 unit_;
    Vec3 last_unit_;
    double accum_ = 0.0;
    std::vector<std::pair<std::string, int>> targets_;  // scenario target id <-> EM platform id
};

}  // namespace simcore
