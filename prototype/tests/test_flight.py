"""Mirror of simcore/tests/test_flight.cpp (roadmap R3): flight with attitude. Generic placeholder numbers."""
import math
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from samproto import AcmiWriter, Aircraft, GeoPoint, LocalFrame, Vec3, World  # noqa: E402
from samproto.engagement import (coordinated_turn_rate, corner_speed_mps, load_factor_of_bank,  # noqa: E402
                                 turn_radius_m)
from samproto.vec import G0, azimuth_of  # noqa: E402


def turner(speed, turn_deg_s, max_g, roll_deg_s):
    return Aircraft("t", Vec3(0, 0, 1000), speed_mps=speed, velocity=Vec3(0, speed, 0),
                    waypoints=[Vec3(3000, -60000, 1000)], max_turn_rate_rad_s=math.radians(turn_deg_s),
                    max_load_g=max_g, roll_rate_rad_s=math.radians(roll_deg_s))


def turn_right(a, steps, dt):
    for _ in range(steps):
        a.waypoints = [a.position + Vec3(math.cos(a.heading_rad), -math.sin(a.heading_rad), 0) * 50000.0]
        a.next_wp = 0
        a.step(dt)


def wrap(x):
    return math.remainder(x, 2 * math.pi)


def test_coordinated_turn_by_hand():
    bank = math.radians(60.0)
    assert math.degrees(coordinated_turn_rate(200.0, bank)) == pytest.approx(4.866, abs=0.001)
    assert load_factor_of_bank(bank) == pytest.approx(2.0, abs=1e-12)
    assert turn_radius_m(200.0, bank) == pytest.approx(2354.9, abs=0.1)
    a = turner(200.0, 30.0, 2.0, 90.0)
    assert a.flies_by_bank()
    a.step(0.05)
    turn_right(a, 100, 0.05)
    assert math.degrees(a.bank_rad) == pytest.approx(60.0, abs=1e-6)
    assert a.load_g == pytest.approx(2.0, abs=1e-6)
    h0 = a.heading_rad
    turn_right(a, 400, 0.05)
    assert math.degrees(wrap(a.heading_rad - h0)) / 20.0 == pytest.approx(4.866, abs=0.001)
    v = a.velocity / a.velocity.length()
    centre = a.position + Vec3(v.y, -v.x, 0) * 2354.9
    worst = 0.0
    for _ in range(600):
        turn_right(a, 1, 0.05)
        worst = max(worst, abs(math.hypot(a.position.x - centre.x, a.position.y - centre.y) - 2354.9))
    assert worst < 6.0


def test_roll_rate_makes_roll_in_take_time():
    a = turner(200.0, 30.0, 2.0, 30.0)
    for _ in range(10):
        a.step(0.1)
    assert math.degrees(a.bank_rad) == pytest.approx(30.0, abs=1e-6)
    for _ in range(10):
        a.step(0.1)
    assert math.degrees(a.bank_rad) == pytest.approx(60.0, abs=1e-6)
    a.waypoints = [a.position + Vec3(math.sin(a.heading_rad), math.cos(a.heading_rad), 0) * 50000.0]
    a.next_wp = 0
    a.step(0.1)
    assert math.radians(50.0) < a.bank_rad < math.radians(60.0)
    for _ in range(100):
        a.step(0.1)
    assert abs(math.degrees(a.bank_rad)) < 1.0


def test_roll_out_ahead_of_the_heading():
    a = Aircraft("a", Vec3(0, 0, 1000), speed_mps=250.0, velocity=Vec3(0, 250, 0), waypoints=[Vec3(60000, 3000, 1000)],
                 max_turn_rate_rad_s=math.radians(12.0), max_load_g=6.0, roll_rate_rad_s=math.radians(90.0))
    most = 0.0
    for _ in range(400):
        a.step(0.05)
        most = max(most, math.degrees(a.heading_rad))
    want = math.degrees(azimuth_of(a.waypoints[0] - a.position))
    assert math.degrees(a.heading_rad) == pytest.approx(want, abs=0.2)
    assert most < want + 1.0
    assert abs(a.bank_rad) < math.radians(1.0)


def test_g_limit_caps_turn_rate_at_high_speed():
    a = turner(400.0, 20.0, 5.0, 180.0)
    assert math.degrees(a.turn_rate_limit(400.0)) == pytest.approx(6.882, abs=0.001)
    assert math.degrees(a.turn_rate_limit(100.0)) == pytest.approx(20.0, abs=1e-9)
    assert corner_speed_mps(5.0, math.radians(20.0)) == pytest.approx(137.6, abs=0.1)
    for _ in range(40):
        a.step(0.05)
    h0 = a.heading_rad
    for _ in range(100):
        a.step(0.05)
    assert math.degrees(wrap(a.heading_rad - h0)) / 5.0 == pytest.approx(6.882, abs=0.001)
    assert a.load_g == pytest.approx(5.0, abs=1e-6)
    b = turner(100.0, 20.0, 5.0, 180.0)
    for _ in range(40):
        b.step(0.05)
    hb = b.heading_rad
    for _ in range(20):
        b.step(0.05)
    assert math.degrees(wrap(b.heading_rad - hb)) == pytest.approx(20.0, abs=1e-6)


def test_energy_k_by_hand_and_speed_held():
    a = Aircraft("a", Vec3(0, 0, 1000), speed_mps=250.0, velocity=Vec3(0, 250, 0), waypoints=[Vec3(0, 200000, 1000)],
                 max_turn_rate_rad_s=math.radians(15.0), sustained_turn_rate_rad_s=math.radians(10.0), max_load_g=8.0,
                 max_speed_mps=400.0, accel_mps2=8.0)
    assert a.has_energy_model()
    assert corner_speed_mps(8.0, math.radians(15.0)) == pytest.approx(297.32, abs=0.01)
    assert a.turn_drag_k() == pytest.approx(0.12786, abs=0.0001)
    for _ in range(200):
        a.step(0.05)
    assert a.airspeed_mps == pytest.approx(250.0, abs=1e-6)
    a.speed_mps = 300.0
    a.step(0.05)
    assert a.throttle == pytest.approx(1.0, abs=1e-12)
    for _ in range(1200):
        a.step(0.05)
    assert a.airspeed_mps == pytest.approx(300.0, abs=0.5)


def test_climb_costs_speed():
    def make(wp_up):
        return Aircraft("a", Vec3(0, 0, 1000), speed_mps=250.0, velocity=Vec3(0, 250, 0),
                        waypoints=[Vec3(0, 100000, wp_up)], max_turn_rate_rad_s=math.radians(10.0), max_load_g=7.0,
                        roll_rate_rad_s=math.radians(120.0), max_speed_mps=400.0, accel_mps2=6.0, max_climb_mps=150.0)
    a, level = make(400000.0), make(1000.0)
    for _ in range(200):
        a.step(0.05)
        level.step(0.05)
    v0, g0 = a.airspeed_mps, a.path_angle_rad
    a.step(0.05)
    r = v0 / 400.0
    want = 0.05 * (6.0 * (1.0 - r * r) - G0 * math.sin(a.path_angle_rad))
    assert g0 > math.radians(20.0)
    assert a.throttle == pytest.approx(1.0, abs=1e-12)
    assert a.airspeed_mps - v0 == pytest.approx(want, abs=1e-9)
    assert v0 < 240.0
    assert level.airspeed_mps == pytest.approx(250.0, abs=1e-6)


def test_hard_turn_bleeds_speed():
    def make():
        a = turner(297.0, 15.0, 8.0, 180.0)
        a.sustained_turn_rate_rad_s = math.radians(10.0)
        a.max_speed_mps, a.accel_mps2 = 400.0, 8.0
        return a
    a = make()
    a.step(0.05)
    turn_right(a, 400, 0.05)
    assert a.airspeed_mps < 270.0  # 266: easing the turn as he slows
    assert a.throttle == pytest.approx(1.0, abs=1e-12)
    assert a.load_g < 8.0
    # But he won't bleed below 85 % of his told speed: he eases toward the sustained turn there and settles on it.
    turn_right(a, 1200, 0.05)
    assert a.hold_speed_mps() - 0.5 < a.airspeed_mps < a.hold_speed_mps() + 5.0
    assert a.turn_rate_limit(a.airspeed_mps) > math.radians(10.0)
    s = make()
    s.waypoints = [Vec3(0, 100000, 1000)]
    for _ in range(400):
        s.step(0.05)
    assert s.airspeed_mps == pytest.approx(297.0, abs=1e-6)


def test_helicopter_leans_nose_down_level_in_hover():
    h = Aircraft("h", Vec3(0, 0, 100), speed_mps=80.0, velocity=Vec3(0, 80, 0), waypoints=[Vec3(0, 50000, 100)],
                 max_turn_rate_rad_s=math.radians(30.0), type="Air+Rotorcraft", max_load_g=2.0,
                 roll_rate_rad_s=math.radians(60.0), max_speed_mps=80.0)
    h.step(0.1)
    assert not h.has_energy_model()
    assert math.degrees(h.pitch_rad) == pytest.approx(-10.0, abs=1e-9)
    h.speed_mps = 40.0
    h.step(0.1)
    assert math.degrees(h.pitch_rad) == pytest.approx(-math.degrees(math.atan(0.25 * math.tan(math.radians(10.0)))), abs=1e-9)
    h.speed_mps = 0.0
    h.step(0.1)
    assert h.pitch_rad == pytest.approx(0.0, abs=1e-12)
    assert h.bank_rad == pytest.approx(0.0, abs=1e-12)
    h.waypoints, h.next_wp = [Vec3(5000, 0, 100)], 0
    before = h.heading_rad
    h.step(0.1)
    assert math.degrees(wrap(h.heading_rad - before)) == pytest.approx(3.0, abs=1e-9)
    h.speed_mps = 60.0
    h.waypoints, h.next_wp = [Vec3(0, -50000, 100)], 0
    most = 0.0
    for _ in range(50):
        h.step(0.1)
        most = max(most, abs(h.bank_rad))
    assert math.degrees(most) == pytest.approx(60.0, abs=1e-6)


def test_crosswind_bank_to_turn_crabs():
    b = Aircraft("b", Vec3(0, 0, 1000), speed_mps=200.0, velocity=Vec3(0, 200, 0), waypoints=[Vec3(0, 20000, 1000)],
                 max_turn_rate_rad_s=math.radians(10.0), max_load_g=6.0, roll_rate_rad_s=math.radians(90.0))
    wind = Vec3(20, 0, 0)
    for _ in range(400):
        b.step(0.05, wind)
    assert b.velocity.x == pytest.approx(0.0, abs=0.05)
    assert b.velocity.y == pytest.approx(199.0, abs=0.05)
    assert math.degrees(wrap(b.heading_rad)) == pytest.approx(-5.74, abs=0.05)
    assert abs(b.position.x) < 60.0


def test_old_way_wind_crab():
    # Mirror of test_world_map.cpp's wind test (the old way, now with the wind in the prototype too).
    b = Aircraft("b", Vec3(0, 0, 1000), speed_mps=200.0, velocity=Vec3(0, 200, 0), waypoints=[Vec3(0, 20000, 1000)],
                 max_turn_rate_rad_s=math.radians(10.0))
    wind = Vec3(20, 0, 0)
    for _ in range(200):
        b.step(0.05, wind)
    assert b.velocity.x == pytest.approx(0.0, abs=0.05)
    assert b.velocity.y == pytest.approx(199.0, abs=0.05)
    air = b.velocity - wind
    assert math.degrees(math.atan2(-air.x, air.y)) == pytest.approx(5.74, abs=0.05)
    assert abs(b.position.x) < 25.0


def test_old_way_shows_a_bank():
    a = Aircraft("a", Vec3(0, 0, 1000), speed_mps=200.0, velocity=Vec3(0, 200, 0), waypoints=[Vec3(3000, -60000, 1000)],
                 max_turn_rate_rad_s=math.radians(6.0))
    assert not a.flies_by_bank()
    a.step(0.1)
    assert math.degrees(azimuth_of(a.velocity)) == pytest.approx(0.6, abs=1e-9)
    for _ in range(20):
        a.step(0.1)
    assert math.degrees(a.bank_rad) == pytest.approx(64.9, abs=0.05)
    assert a.load_g == pytest.approx(2.36, abs=0.01)
    assert math.degrees(a.pitch_rad) == pytest.approx(3.0 * a.load_g, abs=1e-6)
    w = World(LocalFrame(GeoPoint.from_degrees(45, 25, 0)))
    w.add_aircraft(a)
    acmi = AcmiWriter(w.frame)
    acmi.write_world(w)
    assert "|64.9|" in acmi.text()
