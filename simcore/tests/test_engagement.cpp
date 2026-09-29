#include <cmath>
#include <string>

#include "simcore/acmi.hpp"
#include "simcore/missile.hpp"
#include "simcore/world.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {
// Generic placeholder missile — deliberately NOT modelled on any real system.
MissileParams GenericMissile() {
    MissileParams p;
    p.launch_mass_kg = 150.0;
    p.burnout_mass_kg = 90.0;
    p.boost_thrust_n = 30000.0;
    p.boost_time_s = 2.5;
    p.sustain_thrust_n = 4000.0;
    p.sustain_time_s = 10.0;
    p.reference_area_m2 = 0.025;
    p.drag_coefficient = 0.35;
    p.max_lateral_accel_mps2 = 25.0 * kStandardGravity;
    p.navigation_constant = 4.0;
    p.launch_speed_mps = 30.0;
    p.guidance_start_s = 0.5;
    p.fuze_arming_time_s = 1.0;
    p.proximity_fuze_radius_m = 15.0;
    p.lethal_radius_m = 10.0;
    p.max_flight_time_s = 40.0;
    p.min_guided_speed_mps = 250.0;
    return p;
}

Aircraft InboundTarget(double north_m, double alt_m, double speed_mps) {
    Aircraft a;
    a.name = "Target";
    a.position = {2000.0, north_m, alt_m};
    a.speed_mps = speed_mps;
    a.waypoints = {Vec3{2000.0, -50000.0, alt_m}};
    return a;
}
}  // namespace

SC_TEST("missile: mass and thrust schedule") {
    const MissileParams p = GenericMissile();
    SC_EXPECT_NEAR(MissileMass(p, 0.0), 150.0, 1e-12);
    SC_EXPECT_NEAR(MissileMass(p, MissileBurnTime(p) / 2.0), 120.0, 1e-9);
    SC_EXPECT_NEAR(MissileMass(p, 100.0), 90.0, 1e-12);
    SC_EXPECT_NEAR(MissileThrust(p, 1.0), 30000.0, 1e-12);
    SC_EXPECT_NEAR(MissileThrust(p, 5.0), 4000.0, 1e-12);
    SC_EXPECT_NEAR(MissileThrust(p, 20.0), 0.0, 1e-12);
}

SC_TEST("missile: PN command is zero on a collision course and lateral otherwise") {
    // Target dead ahead, closing head-on: LOS rate zero -> no command.
    const Vec3 a0 = ProportionalNavigationAccel({0, 0, 0}, {0, 500, 0}, {0, 10000, 0}, {0, -200, 0}, 4.0);
    SC_EXPECT_NEAR(a0.Length(), 0.0, 1e-9);
    // Target crossing to the east: command should push east (+x).
    const Vec3 a1 = ProportionalNavigationAccel({0, 0, 0}, {0, 500, 0}, {0, 10000, 0}, {200, 0, 0}, 4.0);
    SC_EXPECT(a1.x > 0.0);
}

SC_TEST("missile: min distance over a step catches a fly-through") {
    // Relative position goes from +50 m to -50 m along x, passing 3 m abeam.
    SC_EXPECT_NEAR(MinDistanceOverStep({50.0, 3.0, 0.0}, {-50.0, 3.0, 0.0}), 3.0, 1e-9);
}

SC_TEST("world: guided missile intercepts an inbound non-manoeuvring target") {
    World world(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    const int tgt = world.AddAircraft(InboundTarget(15000.0, 3000.0, 250.0));
    const Vec3 launcher{0.0, 0.0, 5.0};
    const Vec3 to_tgt = world.FindAircraft(tgt)->position - launcher;
    world.LaunchMissile(GenericMissile(), launcher, AzimuthOf(to_tgt), ElevationOf(to_tgt) + DegToRad(10.0), tgt);

    bool destroyed = false;
    double miss = -1.0;
    for (int i = 0; i < 6000 && !destroyed; ++i) {
        world.Step(0.01);
        for (const SimEvent& e : world.DrainEvents()) {
            if (e.type == EventType::AircraftDestroyed) destroyed = true;
            if (e.type == EventType::MissileDetonated || e.type == EventType::MissileMissed) miss = e.miss_distance_m;
        }
    }
    SC_EXPECT(destroyed);
    SC_EXPECT(miss >= 0.0 && miss <= 10.0);
    SC_EXPECT(!world.FindAircraft(tgt)->alive);
}

SC_TEST("world: target out of kinematic reach is not destroyed") {
    World world(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    Aircraft a = InboundTarget(90000.0, 12000.0, 250.0);
    a.waypoints = {Vec3{2000.0, 200000.0, 12000.0}};  // flying away
    const int tgt = world.AddAircraft(a);
    const Vec3 to_tgt = world.FindAircraft(tgt)->position;
    world.LaunchMissile(GenericMissile(), Vec3{}, AzimuthOf(to_tgt), ElevationOf(to_tgt) + DegToRad(10.0), tgt);
    for (int i = 0; i < 6000; ++i) world.Step(0.01);
    SC_EXPECT(world.FindAircraft(tgt)->alive);
    SC_EXPECT(!world.MissileList().front().Active());
}

SC_TEST("world: aircraft follows waypoints with limited turn rate") {
    World world(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    Aircraft a;
    a.position = {0.0, 0.0, 1000.0};
    a.velocity = {0.0, 200.0, 0.0};
    a.speed_mps = 200.0;
    a.max_turn_rate_rad_s = DegToRad(3.0);
    a.waypoints = {Vec3{20000.0, 0.0, 1000.0}};  // 90 degrees to the right
    const int id = world.AddAircraft(a);
    world.Step(1.0);
    const double heading = RadToDeg(AzimuthOf(world.FindAircraft(id)->velocity));
    SC_EXPECT_NEAR(heading, 3.0, 1e-6);  // turned exactly the limit in 1 s
    SC_EXPECT_NEAR(world.FindAircraft(id)->velocity.Length(), 200.0, 1e-9);
}

SC_TEST("acmi: writer produces a valid header and object lines") {
    World world(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    Aircraft a = InboundTarget(10000.0, 3000.0, 250.0);
    a.name = "Bandit";
    world.AddAircraft(a);
    AcmiWriter w(world.Frame());
    w.WriteWorld(world);
    world.Step(0.5);
    w.WriteWorld(world);
    const std::string& t = w.Text();
    SC_EXPECT(t.rfind("FileType=text/acmi/tacview\nFileVersion=2.2\n", 0) == 0);
    SC_EXPECT(t.find("#0.00\n1,T=") != std::string::npos);
    SC_EXPECT(t.find("Name=Bandit") != std::string::npos);
    SC_EXPECT(t.find("#0.50\n1,T=") != std::string::npos);
    // Properties only on first appearance.
    SC_EXPECT(t.find("Name=Bandit") == t.rfind("Name=Bandit"));
}
