// simcore/em/demo_scenario.hpp — the electromagnetic layer's MVP scenario, buildable anywhere.
//
// Used by examples/em_demo.cpp (headless, with checks and JSON/ACMI output) and by Unreal's EM demo
// mode (USimWorldSubsystem::StartEmDemo, drawn with the sam3d.em.Debug overlays). Generic
// placeholder equipment from data/em/generic.json only.
//
//   Red:  SAM-1 (search radar always on, tracking radar), SAM-2 (dark until cued), a battery command
//         post on a VHF voice net with both, three ESM direction-finding sites cabled to the CP.
//   Blue: a striker (fire-control radar on, EMCON at 120 s, RWR), a terrain-following low raider,
//         a stand-off jammer (on at 200 s). At 330 s the CP is destroyed.
// Positions are ENU metres around the scenario origin (SAM-1 at 0, 0).
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "simcore/em/world.hpp"

namespace simcore {
namespace em {

// Rolling hills plus a ridge from (5, 30) km to (50, 24) km, 450 m high, on a 200 m grid; heights are
// metres above the ellipsoid, offset by `base_m` (use the origin's altitude).
inline GridTerrain MakeDemoTerrain(double base_m = 0.0) {
    const double x0 = -60000.0, y0 = -40000.0, step = 200.0;
    const int cols = 801, rows = 701;  // 160 km x 140 km
    std::vector<double> h(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));
    const double ax = 5000.0, ay = 30000.0, bx = 50000.0, by = 24000.0;
    const double lx = bx - ax, ly = by - ay, len2 = lx * lx + ly * ly;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            const double x = x0 + step * c, y = y0 + step * r;
            double z = base_m + 220.0 + 35.0 * std::sin(x / 7000.0) * std::cos(y / 9000.0) + 20.0 * std::sin((x + y) / 4000.0);
            double t = ((x - ax) * lx + (y - ay) * ly) / len2;
            t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
            const double dx = x - (ax + t * lx), dy = y - (ay + t * ly);
            z += 450.0 * std::exp(-(dx * dx + dy * dy) / (2.0 * 1500.0 * 1500.0));
            h[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c)] = z;
        }
    return GridTerrain(x0, y0, step, cols, rows, h);
}

// Waypoint steering for the demo's aircraft (straight legs at constant speed, optional terrain
// following). The game's own aircraft come from simcore::World; this keeps the demo self-contained.
struct DemoMover {
    int id = 0;
    double agl_m = -1.0;  // > 0: terrain following at this height
    std::vector<Vec3> waypoints;
    std::size_t next = 0;
    double speed = 0.0;
};

class EmDemoScenario {
public:
    // Ids of everything the referee (em_demo) or a front end may want to look at.
    int sam1_p = 0, sam2_p = 0, cp_p = 0, sam1_search = 0, sam1_track = 0, sam2_search = 0;
    int cp = 0, sam1 = 0, sam2 = 0, striker = 0, striker_fcr = 0, striker_node = 0, raider = 0, soj = 0, jammer = 0;
    std::vector<int> df_esm;

    EmDemoScenario(const GeoPoint& origin, const Terrain* terrain, std::uint64_t seed = 7)
        : world_(origin, terrain, seed) {}

    EmWorld& Layer() { return world_; }
    const EmWorld& Layer() const { return world_; }

    bool Build(const EmLibrary& lib, std::string* error = nullptr) {
        if (!world_.SetLibrary(lib, error)) return false;
        Environment env;
        env.surface_roughness_m = 3.0;  // rolling farmland
        world_.SetEnvironment(env);
        EmScene& s = world_.Scene();
        if (lib.Emitter("generic_airborne_fcr_x") < 0 || lib.Radar("generic_search_c") < 0) {
            if (error != nullptr) *error = "the EM demo needs data/em/generic.json";
            return false;
        }

        sam1_p = world_.AddGroundPlatform("SAM-1", Side::Red, 0.0, 0.0, 5.0);
        sam2_p = world_.AddGroundPlatform("SAM-2", Side::Red, 18000.0, -4000.0, 5.0);
        cp_p = world_.AddGroundPlatform("Battery CP", Side::Red, 5000.0, -12000.0, 10.0);
        sam1_search = world_.AddRadar(sam1_p, "generic_search_c", true);
        sam1_track = world_.AddRadar(sam1_p, "generic_track_ku", false);
        sam2_search = world_.AddRadar(sam2_p, "generic_search_c", false);
        cp = world_.AddNode("Battery CP", cp_p, Side::Red, true);
        sam1 = world_.AddNode("SAM-1", sam1_p, Side::Red, false, cp);
        sam2 = world_.AddNode("SAM-2", sam2_p, Side::Red, false, cp);
        s.FindNode(cp)->picture_period_s = 15.0;
        s.FindNode(sam1)->report_period_s = 8.0;
        world_.AttachSensor(sam1, sam1_search);
        world_.AttachSensor(sam1, sam1_track);
        world_.AttachSensor(sam2, sam2_search);
        const double df_pos[3][2] = {{-30000.0, -10000.0}, {0.0, -25000.0}, {30000.0, -12000.0}};
        for (int i = 0; i < 3; ++i) {
            const int p = world_.AddGroundPlatform("DF-" + std::to_string(i + 1), Side::Red, df_pos[i][0], df_pos[i][1], 20.0);
            df_esm.push_back(world_.AddEsm(p, "generic_esm_site"));
            world_.AttachSensor(cp, df_esm.back());
        }
        world_.AddNet("battery net", "generic_vhf_fm", 42.0e6, {cp, sam1, sam2});
        SamDoctrine d1;
        d1.node_id = sam1;
        d1.search_radar_id = sam1_search;
        d1.track_radar_id = sam1_track;
        d1.keep_search_on = true;  // SAM-1 is the battery's eyes
        d1.engage_range_m = 22000.0;
        world_.Doctrines().push_back(d1);
        SamDoctrine d2;
        d2.node_id = sam2;
        d2.search_radar_id = sam2_search;
        d2.cue_range_m = 22000.0;
        world_.Doctrines().push_back(d2);

        striker = AddAircraft("Striker", -10000.0, 80000.0, 4000.0, {3.0, 6.0, 3.0});
        movers_.push_back({striker, -1.0, {{-2000.0, 16000.0, 0.0}, {-60000.0, 30000.0, 0.0}}, 0, 250.0});
        striker_fcr = world_.AddEmitter(striker, lib.emitters[static_cast<std::size_t>(lib.Emitter("generic_airborne_fcr_x"))]);
        s.FindEmitter(striker_fcr)->radiating = true;
        striker_node = world_.AddNode("Striker", striker, Side::Blue);
        world_.AttachSensor(striker_node, world_.AddEsm(striker, "generic_rwr"));
        raider = AddAircraft("Low raider", 42000.0, 48000.0, 400.0, {2.0, 4.0, 2.0});
        movers_.push_back({raider, 150.0, {{30000.0, 27000.0, 0.0}, {6000.0, 6000.0, 0.0}, {-20000.0, -10000.0, 0.0}}, 0, 220.0});
        soj = AddAircraft("SOJ", 90000.0, 30000.0, 8000.0, {20.0, 30.0, 20.0});
        DemoMover orbit{soj, -1.0, {}, 0, 180.0};
        for (int k = 0; k < 6; ++k) orbit.waypoints.push_back({90000.0, k % 2 == 0 ? 40000.0 : 20000.0, 0.0});
        movers_.push_back(orbit);
        jammer = world_.AddJammer(soj, "generic_soj_barrage_c", false);
        s.FindJammer(jammer)->aim_at_point = true;
        s.FindJammer(jammer)->aim_point = {0.0, 0.0, 0.0};  // briefed SAM area, not a sensed position
        world_.Emcon().push_back({200.0, EmconTarget::Jammer, jammer, true, "SOJ jammer on (aimed at the briefed SAM area)", false});
        return true;
    }

    // Move the aircraft, run the scripted events, step the layer.
    void Step(double dt) {
        EmScene& s = world_.Scene();
        const double t = world_.Time() + dt;
        if (!fcr_off_ && t >= 120.0) {
            fcr_off_ = true;
            s.FindEmitter(striker_fcr)->radiating = false;
            s.Log(EmEventKind::Info, striker_node, 0, "Striker: fire-control radar OFF (EMCON)");
        }
        if (!cp_destroyed_ && t >= 330.0) {
            cp_destroyed_ = true;
            world_.DestroyPlatform(cp_p);
        }
        Move(dt);
        world_.Step(dt);
    }

private:
    int AddAircraft(const char* name, double e, double n, double alt, const RcsModel& rcs) {
        Platform p;
        p.name = name;
        p.side = Side::Blue;
        p.kind = PlatformKind::FixedWing;
        p.rcs = rcs;
        p.position = {e, n, alt - world_.Scene().frame.Origin().alt_m - (e * e + n * n) / (2.0 * wgs84::kMeanRadius)};
        return world_.AddPlatform(p);
    }

    void Move(double dt) {
        EmScene& s = world_.Scene();
        for (DemoMover& m : movers_) {
            Platform* p = s.FindPlatform(m.id);
            if (p == nullptr || !p->alive) continue;
            if (m.next < m.waypoints.size()) {
                Vec3 to = m.waypoints[m.next] - p->position;
                to.z = 0.0;
                if (to.Length() < 600.0) {
                    ++m.next;
                } else {
                    const Vec3 want = to.Normalized() * m.speed;
                    p->velocity = {want.x, want.y, 0.0};
                }
            }
            const double alt = s.frame.AltitudeOf(p->position);
            p->position += p->velocity * dt;
            double target_alt = alt;
            if (m.agl_m > 0.0) target_alt = s.GetTerrain().HeightAt(p->position.x, p->position.y) + m.agl_m;
            p->position.z += target_alt - s.frame.AltitudeOf(p->position);
        }
    }

    EmWorld world_;
    std::vector<DemoMover> movers_;
    bool fcr_off_ = false;
    bool cp_destroyed_ = false;
};

}  // namespace em
}  // namespace simcore
