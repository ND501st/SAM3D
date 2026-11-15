// Flight with attitude (roadmap R3, aircraft.hpp): bank-to-turn within the g limit and the roll rate, the energy
// model, a helicopter's lean, and the old way kept for aircraft without their type's limits.
// Generic placeholder numbers, not any real aircraft's.
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>

#include "simcore/acmi.hpp"
#include "simcore/aircraft.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/world.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {
// A bank-to-turn aircraft flying north at `speed` 1000 m up, with a far waypoint behind it and to the right: it turns
// right as hard as it may for a long while.
Aircraft Turner(double speed_mps, double turn_deg_s, double max_g, double roll_deg_s) {
    Aircraft a;
    a.position = {0.0, 0.0, 1000.0};
    a.velocity = {0.0, speed_mps, 0.0};
    a.speed_mps = speed_mps;
    a.max_turn_rate_rad_s = DegToRad(turn_deg_s);
    a.max_load_g = max_g;
    a.roll_rate_rad_s = DegToRad(roll_deg_s);
    a.waypoints = {{3000.0, -60000.0, 1000.0}};
    return a;
}
// Keeps it turning right for ever: a waypoint 50 km square to its right, moved along each step.
void TurnRight(Aircraft& a, int steps, double dt) {
    for (int i = 0; i < steps; ++i) {
        a.waypoints = {a.position + Vec3{std::cos(a.heading_rad), -std::sin(a.heading_rad), 0.0} * 50000.0};
        a.next_waypoint = 0;
        StepAircraft(a, dt);
    }
}
}  // namespace

SC_TEST("flight: a coordinated turn at 60 deg of bank and 200 m/s, by hand") {
    // omega = g tan(phi) / v = 9.80665 * 1.7320508 / 200 = 0.084927 rad/s = 4.866 deg/s; n = 1 / cos(60) = 2.0;
    // r = v^2 / (g tan(phi)) = 40000 / 16.98565 = 2354.9 m.
    const double bank = DegToRad(60.0);
    SC_EXPECT_NEAR(RadToDeg(CoordinatedTurnRate(200.0, bank)), 4.866, 0.001);
    SC_EXPECT_NEAR(LoadFactorOfBank(bank), 2.0, 1e-12);
    SC_EXPECT_NEAR(TurnRadiusM(200.0, bank), 2354.9, 0.1);
    SC_EXPECT_NEAR(RadToDeg(BankForTurnRate(200.0, CoordinatedTurnRate(200.0, bank))), 60.0, 1e-9);

    // Flown: a 2 g limit and a high instantaneous rate leave it g-limited at 60 deg of bank, 4.87 deg/s, 2 g, on a
    // circle of 2355 m.
    Aircraft a = Turner(200.0, 30.0, 2.0, 90.0);
    SC_EXPECT(FliesByBank(a));
    StepAircraft(a, 0.05);
    TurnRight(a, 100, 0.05);  // 5 s: rolled in
    SC_EXPECT_NEAR(RadToDeg(a.bank_rad), 60.0, 1e-6);
    SC_EXPECT_NEAR(a.load_g, 2.0, 1e-6);
    const double h0 = a.heading_rad;
    double min_x = 1e9, max_x = -1e9;
    for (int i = 0; i < 400; ++i) {  // 20 s more: 97 deg of the circle
        TurnRight(a, 1, 0.05);
        min_x = std::fmin(min_x, a.position.x);
        max_x = std::fmax(max_x, a.position.x);
    }
    const double rate_deg_s = RadToDeg(std::remainder(a.heading_rad - h0, kTwoPi)) / 20.0;
    std::printf("    turn rate %.3f deg/s, bank %.2f deg, n %.3f\n", rate_deg_s, RadToDeg(a.bank_rad), a.load_g);
    SC_EXPECT_NEAR(rate_deg_s, 4.866, 0.001);
    // Its centre: the radius to its right, square to its velocity.
    const Vec3 v = a.velocity / a.velocity.Length();
    const Vec3 centre = a.position + Vec3{v.y, -v.x, 0.0} * 2354.9;
    Aircraft b = a;
    double worst = 0.0;
    for (int i = 0; i < 600; ++i) {
        TurnRight(b, 1, 0.05);
        worst = std::fmax(worst, std::fabs(std::hypot(b.position.x - centre.x, b.position.y - centre.y) - 2354.9));
    }
    std::printf("    off the 2354.9 m circle by %.2f m at most\n", worst);
    SC_EXPECT(worst < 6.0);  // the step's own error (0.05 s steps)
}

SC_TEST("flight: the roll rate makes the roll-in take time") {
    // 30 deg/s to the 60 deg the g limit allows: 2 s, the bank 30 deg after 1 s.
    Aircraft a = Turner(200.0, 30.0, 2.0, 30.0);
    for (int i = 0; i < 10; ++i) StepAircraft(a, 0.1);
    SC_EXPECT_NEAR(RadToDeg(a.bank_rad), 30.0, 1e-6);
    for (int i = 0; i < 10; ++i) StepAircraft(a, 0.1);
    SC_EXPECT_NEAR(RadToDeg(a.bank_rad), 60.0, 1e-6);
    // And the roll-out: past the turn, onto a waypoint straight ahead, the wings come level at the roll rate.
    a.waypoints = {a.position + Vec3{std::sin(a.heading_rad), std::cos(a.heading_rad), 0.0} * 50000.0};
    a.next_waypoint = 0;
    StepAircraft(a, 0.1);
    SC_EXPECT(a.bank_rad < DegToRad(60.0) && a.bank_rad > DegToRad(50.0));
    for (int i = 0; i < 100; ++i) StepAircraft(a, 0.1);  // past the heading by 3.7 deg in the 2 s roll-out, then back
    SC_EXPECT(std::fabs(RadToDeg(a.bank_rad)) < 1.0);
}

SC_TEST("flight: a roll-out ahead of the heading wanted, not an overshoot") {
    // A 90 deg turn to the right at 250 m/s, 6 g, 90 deg/s of roll: it ends on the new heading within a degree.
    Aircraft a;
    a.position = {0.0, 0.0, 1000.0};
    a.velocity = {0.0, 250.0, 0.0};
    a.speed_mps = 250.0;
    a.max_turn_rate_rad_s = DegToRad(12.0);
    a.max_load_g = 6.0;
    a.roll_rate_rad_s = DegToRad(90.0);
    a.waypoints = {{60000.0, 3000.0, 1000.0}};
    double most = 0.0;
    for (int i = 0; i < 400; ++i) {
        StepAircraft(a, 0.05);
        most = std::fmax(most, RadToDeg(a.heading_rad));
    }
    const double want = RadToDeg(AzimuthOf(a.waypoints[0] - a.position));
    std::printf("    heading %.2f deg (to the waypoint %.2f), the most %.2f, bank %.2f\n", RadToDeg(a.heading_rad), want, most,
                RadToDeg(a.bank_rad));
    SC_EXPECT_NEAR(RadToDeg(a.heading_rad), want, 0.2);
    SC_EXPECT(most < want + 1.0);
    SC_EXPECT(std::fabs(a.bank_rad) < DegToRad(1.0));
}

SC_TEST("flight: the g limit caps the turn rate at high speed") {
    // 5 g: omega <= g sqrt(24) / v = 0.12011 rad/s = 6.882 deg/s at 400 m/s, under the 20 deg/s instantaneous rate;
    // at 100 m/s the g limit allows 27.5 deg/s, so the instantaneous rate holds. The corner: g sqrt(24) / 0.34907 =
    // 137.6 m/s.
    Aircraft a = Turner(400.0, 20.0, 5.0, 180.0);
    SC_EXPECT_NEAR(RadToDeg(TurnRateLimit(a, 400.0)), 6.882, 0.001);
    SC_EXPECT_NEAR(RadToDeg(TurnRateLimit(a, 100.0)), 20.0, 1e-9);
    SC_EXPECT_NEAR(CornerSpeedMps(5.0, DegToRad(20.0)), 137.6, 0.1);
    for (int i = 0; i < 40; ++i) StepAircraft(a, 0.05);
    const double h0 = a.heading_rad;
    for (int i = 0; i < 100; ++i) StepAircraft(a, 0.05);
    const double rate = RadToDeg(std::remainder(a.heading_rad - h0, kTwoPi)) / 5.0;
    std::printf("    at 400 m/s: %.3f deg/s, n %.3f\n", rate, a.load_g);
    SC_EXPECT_NEAR(rate, 6.882, 0.001);
    SC_EXPECT_NEAR(a.load_g, 5.0, 1e-6);
    // Slow, the instantaneous rate is the limit.
    Aircraft b = Turner(100.0, 20.0, 5.0, 180.0);
    for (int i = 0; i < 40; ++i) StepAircraft(b, 0.05);
    const double hb = b.heading_rad;
    for (int i = 0; i < 20; ++i) StepAircraft(b, 0.05);
    SC_EXPECT_NEAR(RadToDeg(std::remainder(b.heading_rad - hb, kTwoPi)) / 1.0, 20.0, 1e-6);
}

SC_TEST("flight: the energy model's K, by hand, and speed held level") {
    // A = 8, v_max = 400, n_max = 8, 15 deg/s instantaneous, 10 sustained. Corner: g sqrt(63) / 0.261799 = 297.32 m/s.
    // K = A (1 - (v_c / v_max)^2) / (v_c omega_s / g)^2 = 8 (1 - 0.55250) / (297.32 * 0.174533 / 9.80665)^2
    //   = 3.5800 / 27.999 = 0.12786.
    Aircraft a;
    a.max_turn_rate_rad_s = DegToRad(15.0);
    a.sustained_turn_rate_rad_s = DegToRad(10.0);
    a.max_load_g = 8.0;
    a.max_speed_mps = 400.0;
    a.accel_mps2 = 8.0;
    SC_EXPECT(HasEnergyModel(a));
    SC_EXPECT_NEAR(CornerSpeedMps(8.0, DegToRad(15.0)), 297.32, 0.01);
    SC_EXPECT_NEAR(TurnDragK(a), 0.12786, 0.0001);
    // The sustained turn at the corner holds its speed: A (1 - r^2) = K (n_s^2 - 1).
    const double vc = CornerSpeedMps(8.0, DegToRad(15.0));
    const double ns2m1 = std::pow(vc * DegToRad(10.0) / kStandardGravity, 2.0);
    SC_EXPECT_NEAR(8.0 * (1.0 - std::pow(vc / 400.0, 2.0)) - TurnDragK(a) * ns2m1, 0.0, 1e-9);
    // Level and straight it holds the speed it is told, and speeds up to a new one.
    a.position = {0.0, 0.0, 1000.0};
    a.velocity = {0.0, 250.0, 0.0};
    a.speed_mps = 250.0;
    a.waypoints = {{0.0, 200000.0, 1000.0}};
    for (int i = 0; i < 200; ++i) StepAircraft(a, 0.05);
    SC_EXPECT_NEAR(a.airspeed_mps, 250.0, 1e-6);
    a.speed_mps = 300.0;
    StepAircraft(a, 0.05);
    SC_EXPECT_NEAR(a.throttle, 1.0, 1e-12);  // full throttle: (300 - 250) / 4 s asks for more than it has
    for (int i = 0; i < 1200; ++i) StepAircraft(a, 0.05);
    SC_EXPECT_NEAR(a.airspeed_mps, 300.0, 0.5);
}

SC_TEST("flight: a climb costs speed") {
    // Full throttle in a climb at gamma: dv/dt = A (1 - (v / v_max)^2) - g sin(gamma), checked step by step.
    Aircraft a;
    a.position = {0.0, 0.0, 1000.0};
    a.velocity = {0.0, 250.0, 0.0};
    a.speed_mps = 250.0;
    a.max_turn_rate_rad_s = DegToRad(10.0);
    a.max_load_g = 7.0;
    a.roll_rate_rad_s = DegToRad(120.0);
    a.max_speed_mps = 400.0;
    a.accel_mps2 = 6.0;
    a.max_climb_mps = 150.0;
    a.waypoints = {{0.0, 100000.0, 400000.0}};  // far up: it climbs at the most its climb rate allows
    Aircraft level = a;
    level.waypoints = {{0.0, 100000.0, 1000.0}};
    for (int i = 0; i < 200; ++i) {
        StepAircraft(a, 0.05);
        StepAircraft(level, 0.05);
    }
    const double v0 = a.airspeed_mps, g0 = a.path_angle_rad;
    StepAircraft(a, 0.05);
    const double r = v0 / 400.0;  // the path angle of the step: the one it climbs at after it
    const double want = 0.05 * (6.0 * (1.0 - r * r) - kStandardGravity * std::sin(a.path_angle_rad));
    std::printf("    after 10 s: %.1f m/s at %.1f deg (level: %.1f m/s); a step's dv %.5f, by hand %.5f\n", v0, RadToDeg(g0),
                level.airspeed_mps, a.airspeed_mps - v0, want);
    SC_EXPECT(g0 > DegToRad(20.0));
    SC_EXPECT_NEAR(a.throttle, 1.0, 1e-12);
    SC_EXPECT_NEAR(a.airspeed_mps - v0, want, 1e-9);
    SC_EXPECT(v0 < 240.0);
    SC_EXPECT_NEAR(level.airspeed_mps, 250.0, 1e-6);
}

SC_TEST("flight: a turn harder than the sustained one bleeds speed") {
    // At the corner speed it turns at 15 deg/s with n^2 - 1 = (v omega / g)^2 = 63: K * 63 = 8.06 m/s2 of drag against
    // the 3.58 its thrust has left, so it slows, easing the turn as it does; the same aircraft straight holds its speed.
    Aircraft a = Turner(297.0, 15.0, 8.0, 180.0);
    a.sustained_turn_rate_rad_s = DegToRad(10.0);
    a.max_speed_mps = 400.0;
    a.accel_mps2 = 8.0;
    StepAircraft(a, 0.05);
    TurnRight(a, 400, 0.05);
    std::printf("    after 20 s at the limit: %.1f m/s, n %.2f\n", a.airspeed_mps, a.load_g);
    SC_EXPECT(a.airspeed_mps < 270.0);  // 266
    SC_EXPECT_NEAR(a.throttle, 1.0, 1e-12);
    SC_EXPECT(a.load_g < 8.0);  // slower, the same 15 deg/s pulls fewer g
    // But he won't bleed below 85 % of his told speed: he eases toward the sustained turn there and settles on it.
    TurnRight(a, 1200, 0.05);
    std::printf("    after 80 s: %.1f m/s (holds %.1f), %.2f deg/s\n", a.airspeed_mps, HoldSpeedMps(a),
                RadToDeg(TurnRateLimit(a, a.airspeed_mps)));
    SC_EXPECT(a.airspeed_mps > HoldSpeedMps(a) - 0.5);
    SC_EXPECT(a.airspeed_mps < HoldSpeedMps(a) + 5.0);
    SC_EXPECT(TurnRateLimit(a, a.airspeed_mps) > DegToRad(10.0));  // sustained, nearer the corner: no worse than 10 deg/s
    Aircraft straight = Turner(297.0, 15.0, 8.0, 180.0);
    straight.sustained_turn_rate_rad_s = DegToRad(10.0);
    straight.max_speed_mps = 400.0;
    straight.accel_mps2 = 8.0;
    straight.waypoints = {{0.0, 100000.0, 1000.0}};
    for (int i = 0; i < 400; ++i) StepAircraft(straight, 0.05);
    SC_EXPECT_NEAR(straight.airspeed_mps, 297.0, 1e-6);
}

SC_TEST("flight: a helicopter leans its nose down to fly forward, level in the hover") {
    Aircraft h;
    h.type = "Air+Rotorcraft";
    h.position = {0.0, 0.0, 100.0};
    h.velocity = {0.0, 80.0, 0.0};
    h.speed_mps = 80.0;
    h.max_turn_rate_rad_s = DegToRad(30.0);
    h.max_load_g = 2.0;  // a 60 deg bank limit
    h.roll_rate_rad_s = DegToRad(60.0);
    h.max_speed_mps = 80.0;
    h.waypoints = {{0.0, 50000.0, 100.0}};
    StepAircraft(h, 0.1);
    SC_EXPECT(!HasEnergyModel(h));  // a helicopter flies its told speed
    SC_EXPECT_NEAR(RadToDeg(h.pitch_rad), -10.0, 1e-9);  // at its top speed: the lean at the top
    h.speed_mps = 40.0;
    StepAircraft(h, 0.1);
    SC_EXPECT_NEAR(RadToDeg(h.pitch_rad), -RadToDeg(std::atan(0.25 * std::tan(DegToRad(10.0)))), 1e-9);  // -2.52 deg
    h.speed_mps = 0.0;
    StepAircraft(h, 0.1);
    SC_EXPECT_NEAR(h.pitch_rad, 0.0, 1e-12);
    SC_EXPECT_NEAR(h.bank_rad, 0.0, 1e-12);
    // In the hover it turns on the spot, level, at its yaw rate.
    h.waypoints = {{5000.0, 0.0, 100.0}};
    h.next_waypoint = 0;
    const double before = h.heading_rad;
    StepAircraft(h, 0.1);
    SC_EXPECT_NEAR(RadToDeg(std::remainder(h.heading_rad - before, kTwoPi)), 3.0, 1e-9);
    SC_EXPECT_NEAR(h.bank_rad, 0.0, 1e-12);
    // Forward again and turning hard: no more than the 60 deg its 2 g allow.
    h.speed_mps = 60.0;
    h.waypoints = {{0.0, -50000.0, 100.0}};
    h.next_waypoint = 0;
    double most = 0.0;
    for (int i = 0; i < 50; ++i) {
        StepAircraft(h, 0.1);
        most = std::fmax(most, std::fabs(h.bank_rad));
    }
    SC_EXPECT_NEAR(RadToDeg(most), 60.0, 1e-6);
}

SC_TEST("flight: in a crosswind a bank-to-turn aircraft crabs and holds its track") {
    // As the old way's test (test_world_map.cpp): a waypoint due north, a 20 m/s wind from the west: the nose 5.74 deg
    // into it, ground speed 199.0 m/s up the north line.
    Aircraft b;
    b.position = {0, 0, 1000};
    b.velocity = {0, 200, 0};
    b.speed_mps = 200.0;
    b.max_turn_rate_rad_s = DegToRad(10.0);
    b.max_load_g = 6.0;
    b.roll_rate_rad_s = DegToRad(90.0);
    b.waypoints = {{0, 20000, 1000}};
    const Vec3 wind{20, 0, 0};
    for (int i = 0; i < 400; ++i) StepAircraft(b, 0.05, wind);
    SC_EXPECT_NEAR(b.velocity.x, 0.0, 0.05);
    SC_EXPECT_NEAR(b.velocity.y, 199.0, 0.05);
    SC_EXPECT_NEAR(RadToDeg(std::remainder(b.heading_rad, kTwoPi)), -5.74, 0.05);  // the nose, not the track
    SC_EXPECT(std::fabs(b.position.x) < 60.0);
}

SC_TEST("flight: without its type's limits an aircraft flies the old way and shows a bank for its turn") {
    Aircraft a;
    a.position = {0.0, 0.0, 1000.0};
    a.velocity = {0.0, 200.0, 0.0};
    a.speed_mps = 200.0;
    a.max_turn_rate_rad_s = DegToRad(6.0);
    a.waypoints = {{3000.0, -60000.0, 1000.0}};
    SC_EXPECT(!FliesByBank(a));
    // The old step: the velocity turned 6 deg/s in one go, nothing of a roll-in.
    StepAircraft(a, 0.1);
    SC_EXPECT_NEAR(RadToDeg(AzimuthOf(a.velocity)), 0.6, 1e-9);
    for (int i = 0; i < 20; ++i) StepAircraft(a, 0.1);
    // Shown: atan(v omega / g) = atan(200 * 0.104720 / 9.80665) = 64.9 deg, right wing down; pitch the path and 3 deg
    // of angle of attack at its n (1 / cos 64.9 = 2.36).
    SC_EXPECT_NEAR(RadToDeg(a.bank_rad), 64.9, 0.05);
    SC_EXPECT_NEAR(a.load_g, 2.36, 0.01);
    SC_EXPECT_NEAR(RadToDeg(a.pitch_rad), 3.0 * a.load_g, 1e-6);
    // Tacview gets the roll.
    World w(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    w.AddAircraft(a);
    AcmiWriter acmi(w.Frame());
    acmi.WriteWorld(w);
    SC_EXPECT(acmi.Text().find("|64.9|") != std::string::npos);
}

SC_TEST("flight: the testbed's waves fly by bank-to-turn: they bank, pull and roll; the helicopters lean") {
    // The air war's types give their limits (data/air_war/faction_*.json "flight"), so the dynamic campaign's waves
    // (air_war/test_waves.hpp) fly the new way. Half an hour of them.
    const std::string data = SIMCORE_DATA_DIR;
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(data + "/scenarios/dynamic_campaign_test.json", data, def, console, &e));
    if (def.waves == nullptr) return;
    ScenarioRunner r(std::move(def), std::move(console));
    std::map<int, double> last_bank;
    int by_bank = 0, the_old_way = 0, helicopters_leaning = 0;
    double most_bank = 0.0, most_g = 0.0, most_roll_rate = 0.0, slowest = 1e9, slowest_told = 0.0;
    std::string slowest_name;
    std::set<int> seen;
    while (r.Time() < 1800.0 && !r.Finished()) {
        r.Step(0.1);
        for (const Aircraft& a : r.GetWorld().AircraftList()) {
            if (!a.alive || !a.attitude_set) continue;
            if (seen.insert(a.id).second) (FliesByBank(a) ? by_bank : the_old_way) += 1;
            if (!FliesByBank(a)) continue;
            most_bank = std::fmax(most_bank, std::fabs(a.bank_rad));
            most_g = std::fmax(most_g, a.load_g);
            const auto it = last_bank.find(a.id);
            if (it != last_bank.end()) most_roll_rate = std::fmax(most_roll_rate, std::fabs(a.bank_rad - it->second) / 0.1);
            last_bank[a.id] = a.bank_rad;
            if (IsRotorcraft(a) && a.airspeed_mps > 20.0 && a.pitch_rad < DegToRad(-1.0)) ++helicopters_leaning;
            if (HasEnergyModel(a) && a.airspeed_mps / std::fmax(1.0, a.speed_mps) < slowest) {
                slowest = a.airspeed_mps / std::fmax(1.0, a.speed_mps);
                slowest_name = a.name;
                slowest_told = a.speed_mps;
            }
        }
    }
    std::printf("    %d by bank-to-turn, %d the old way (weapons, ours); the most bank %.0f deg, %.1f g, roll %.0f deg/s; the slowest "
                "against its told speed %.2f (%s, told %.0f m/s)\n", by_bank, the_old_way, RadToDeg(most_bank), most_g,
                RadToDeg(most_roll_rate), slowest, slowest_name.c_str(), slowest_told);
    SC_EXPECT(by_bank > 20);
    SC_EXPECT(most_bank > DegToRad(45.0));       // they bank
    SC_EXPECT(most_g > 2.0);                     // and pull
    SC_EXPECT(most_roll_rate > DegToRad(20.0));  // rolling in and out
    SC_EXPECT(most_roll_rate < DegToRad(241.0)); // within the types' roll rates (the F-16A's 240 deg/s the most)
    SC_EXPECT(helicopters_leaning > 0);
    SC_EXPECT(slowest < 0.99);                   // a turn or a climb has cost one of them speed
}
