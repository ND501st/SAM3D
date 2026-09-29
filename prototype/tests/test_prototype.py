"""Mirror of the key simcore C++ tests — both implementations must agree on these numbers."""
import math
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from samproto import (AcmiWriter, Aircraft, GeoPoint, LocalFrame, MissileParams, RadarParams, Vec3,  # noqa: E402
                      World, detection_range, evaluate_detection, flat, radar_horizon, snr_db,
                      standard_atmosphere)
from samproto.engagement import closest_approach, pn_accel  # noqa: E402
from samproto.geo import ecef_to_geodetic, geodetic_to_ecef  # noqa: E402
from samproto.radar import beam_shape_loss_db, has_line_of_sight  # noqa: E402
from samproto.vec import G0, azimuth_of, elevation_of  # noqa: E402


def generic_radar():
    return RadarParams(100e3, 35.0, 0.03, 1e6, 5.0, 6.0, beamwidth_3db_rad=math.radians(2.0))


def generic_missile():
    return MissileParams(150.0, 90.0, 30000.0, 2.5, 4000.0, 10.0, 0.025, 0.35, 25 * G0)


@pytest.mark.parametrize("lat,lon,alt", [(-60, -170, -100), (0, 0, 0), (45, 25, 12000), (67.3, 179, 25000)])
def test_geodetic_round_trip(lat, lon, alt):
    g = ecef_to_geodetic(geodetic_to_ecef(GeoPoint.from_degrees(lat, lon, alt)))
    assert g.lat_deg == pytest.approx(lat, abs=1e-9)
    assert g.lon_deg == pytest.approx(lon, abs=1e-9)
    assert g.alt_m == pytest.approx(alt, abs=1e-3)


def test_enu_axes_and_curvature():
    f = LocalFrame(GeoPoint.from_degrees(45.0, 25.0, 200.0))
    n = f.geodetic_to_enu(GeoPoint.from_degrees(45.01, 25.0, 200.0))
    assert 1100 < n.y < 1120 and abs(n.x) < 1e-3 and n.z < 0
    assert 770 < LocalFrame(GeoPoint.from_degrees(45, 25, 0)).altitude_of(Vec3(0, 100000, 0)) < 800


def test_isa_reference_values():
    assert standard_atmosphere(0).density_kg_m3 == pytest.approx(1.2250, abs=5e-4)
    assert standard_atmosphere(11000).pressure_pa == pytest.approx(22632.0, abs=5)
    assert standard_atmosphere(20000).density_kg_m3 == pytest.approx(0.08803, abs=5e-4)
    assert standard_atmosphere(30000).pressure_pa == pytest.approx(1171.87, abs=2)


def test_radar_equation_scaling_and_threshold():
    p = generic_radar()
    assert snr_db(p, 10000, 5) - snr_db(p, 20000, 5) == pytest.approx(40 * math.log10(2))
    r = detection_range(p, 5.0)
    assert snr_db(p, r, 5.0) == pytest.approx(p.detection_threshold_db)
    assert 30000 < r < 50000
    assert beam_shape_loss_db(math.radians(1.0), math.radians(2.0)) == pytest.approx(-6.0)


def test_line_of_sight_matches_horizon():
    horizon = radar_horizon(10.0, 300.0)
    assert has_line_of_sight(flat(), Vec3(0, 0, 10), 10.0, Vec3(0, horizon * 0.97, 300), 300.0, 20.0)
    assert not has_line_of_sight(flat(), Vec3(0, 0, 10), 10.0, Vec3(0, horizon * 1.03, 300), 300.0, 20.0)


def test_detection_masked_below_horizon():
    frame = LocalFrame(GeoPoint.from_degrees(45.0, 25.0, 0.0))
    low_far = frame.geodetic_to_enu(GeoPoint.from_degrees(45.5, 25.0, 20.0))
    det = evaluate_detection(generic_radar(), frame, flat(), Vec3(0, 0, 10), Vec3(), low_far, 5.0)
    assert not det.line_of_sight


def test_pn_zero_on_collision_course_and_lateral_otherwise():
    assert pn_accel(Vec3(), Vec3(0, 500, 0), Vec3(0, 10000, 0), Vec3(0, -200, 0), 4).length() == pytest.approx(0)
    assert pn_accel(Vec3(), Vec3(0, 500, 0), Vec3(0, 10000, 0), Vec3(200, 0, 0), 4).x > 0


def test_closest_approach_fly_through():
    d, s = closest_approach(Vec3(50, 3, 0), Vec3(-50, 3, 0))
    assert d == pytest.approx(3.0) and s == pytest.approx(0.5)


def test_intercept_inbound_target():
    w = World(LocalFrame(GeoPoint.from_degrees(45.0, 25.0, 0.0)))
    tid = w.add_aircraft(Aircraft("T", Vec3(2000, 15000, 3000), speed_mps=250,
                                  waypoints=[Vec3(2000, -50000, 3000)]))
    to = w.find(tid).position - Vec3(0, 0, 5)
    w.launch(generic_missile(), Vec3(0, 0, 5), azimuth_of(to), elevation_of(to) + math.radians(10), tid)
    for _ in range(4000):
        w.step(0.01)
        if not w.find(tid).alive:
            break
    kinds = [e.kind for e in w.drain_events()]
    assert "AircraftDestroyed" in kinds


def test_acmi_header_and_first_appearance_properties():
    w = World(LocalFrame(GeoPoint.from_degrees(45, 25, 0)))
    w.add_aircraft(Aircraft("Bandit", Vec3(2000, 10000, 3000), speed_mps=250, waypoints=[Vec3(2000, -5e4, 3000)]))
    a = AcmiWriter(w.frame)
    a.write_world(w)
    w.step(0.5)
    a.write_world(w)
    t = a.text()
    assert t.startswith("FileType=text/acmi/tacview\nFileVersion=2.2\n")
    assert t.count("Name=Bandit") == 1
    assert "#0.50\n1,T=" in t
