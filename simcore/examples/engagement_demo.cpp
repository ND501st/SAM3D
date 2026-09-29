// engagement_demo — one radar, one launcher, one inbound target.
//
// Detects the target with a generic radar model (range equation + terrain LOS),
// launches a generic missile when inside the launch range, runs the engagement,
// prints the event log and writes engagement_demo.acmi for Tacview.
//
// All parameters are GENERIC PLACEHOLDERS for testing the pipeline — not real system data.
#include <cstdio>
#include <string>

#include "simcore/simcore.hpp"

using namespace simcore;

static const char* EventName(EventType t) {
    switch (t) {
        case EventType::MissileLaunched: return "MISSILE LAUNCHED";
        case EventType::MissileDetonated: return "MISSILE DETONATED";
        case EventType::MissileMissed: return "MISSILE MISSED";
        case EventType::MissileSelfDestructed: return "MISSILE SELF-DESTRUCT";
        case EventType::MissileHitGround: return "MISSILE HIT GROUND";
        case EventType::AircraftDestroyed: return "TARGET DESTROYED";
    }
    return "?";
}

int main(int argc, char** argv) {
    const std::string out_path = argc > 1 ? argv[1] : "engagement_demo.acmi";

    World world(GeoPoint::FromDegrees(45.0, 25.0, 300.0));

    RadarParams radar;
    radar.peak_power_w = 100e3;
    radar.antenna_gain_db = 35.0;
    radar.wavelength_m = 0.03;
    radar.bandwidth_hz = 1e6;
    radar.noise_figure_db = 5.0;
    radar.system_losses_db = 6.0;
    radar.detection_threshold_db = 13.0;
    radar.max_instrumented_range_m = 45000.0;

    MissileParams missile;
    missile.launch_mass_kg = 150.0;
    missile.burnout_mass_kg = 90.0;
    missile.boost_thrust_n = 30000.0;
    missile.boost_time_s = 2.5;
    missile.sustain_thrust_n = 4000.0;
    missile.sustain_time_s = 10.0;
    missile.reference_area_m2 = 0.025;
    missile.drag_coefficient = 0.35;
    missile.max_lateral_accel_mps2 = 25.0 * kStandardGravity;
    missile.navigation_constant = 4.0;
    missile.proximity_fuze_radius_m = 15.0;
    missile.lethal_radius_m = 10.0;
    missile.max_flight_time_s = 40.0;
    missile.min_guided_speed_mps = 250.0;

    const Vec3 site{0.0, 0.0, 5.0};  // radar/launcher, 5 m above local origin (origin alt 300 m)
    const FlatTerrain ground(300.0);
    const double launch_range_m = 12000.0;

    Aircraft target;
    target.name = "Bandit 1";
    target.position = {-6000.0, 40000.0, 3000.0};
    target.speed_mps = 250.0;
    target.rcs_m2 = 5.0;
    target.max_turn_rate_rad_s = DegToRad(4.0);
    target.waypoints = {Vec3{-1500.0, 15000.0, 2500.0}, Vec3{2000.0, -30000.0, 2500.0}};
    const int tgt_id = world.AddAircraft(target);

    AcmiWriter acmi(world.Frame());
    acmi.BeginFrame(0.0);
    acmi.WriteObject(1000, site, Vec3{}, "Type=Ground+AntiAircraft,Name=SAM site,Color=Red");

    const double dt = 0.01;
    bool detected = false, launched = false;
    for (int step = 0; step < 20000; ++step) {
        const Aircraft* t = world.FindAircraft(tgt_id);
        if (t == nullptr || !t->alive) break;

        const DetectionResult det =
            EvaluateDetection(radar, world.Frame(), ground, site, Vec3{}, t->position, t->rcs_m2);
        if (det.detected && !detected) {
            detected = true;
            std::printf("t=%6.2fs  RADAR DETECTION  range %.1f km  az %.1f deg  el %.1f deg  SNR %.1f dB\n",
                        world.Time(), det.range_m / 1000.0, RadToDeg(det.azimuth_rad), RadToDeg(det.elevation_rad),
                        det.snr_db);
        }
        if (detected && !launched && det.detected && det.range_m <= launch_range_m) {
            launched = true;
            world.LaunchMissile(missile, site, det.azimuth_rad, det.elevation_rad + DegToRad(10.0), tgt_id);
        }

        world.Step(dt);
        if (step % 10 == 0) acmi.WriteWorld(world);
        for (const SimEvent& e : world.DrainEvents()) {
            std::printf("t=%6.2fs  %-22s missile %d  target %d  miss %.1f m\n", e.time_s, EventName(e.type),
                        e.missile_id, e.aircraft_id, e.miss_distance_m);
        }
        if (launched && !world.MissileList().back().Active() && world.Time() > 0.0) {
            // Keep recording a few seconds after the missile's fate is decided.
            for (int k = 0; k < 300; ++k) {
                world.Step(dt);
                if (k % 10 == 0) acmi.WriteWorld(world);
            }
            break;
        }
    }
    acmi.WriteWorld(world);

    if (acmi.SaveTo(out_path)) {
        std::printf("\nWrote %s (open it in Tacview for a 3D replay)\n", out_path.c_str());
    } else {
        std::printf("\nCould not write %s\n", out_path.c_str());
        return 1;
    }
    return 0;
}
