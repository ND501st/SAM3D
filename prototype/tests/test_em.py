"""Mirror of simcore/tests/test_em_models.cpp: the Python prototype must give the same numbers.

Reference values marked "C++" were printed by simcore (same inputs); random streams are bit-identical.
"""
import math
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from samproto import em  # noqa: E402


def test_random_streams_match_cpp_bit_for_bit():
    assert em.uniform01(em.mix64(0)) == pytest.approx(0.8833108082, abs=1e-10)
    assert em.Random(42).normal("detection", 1, 2, 3) == pytest.approx(-0.105945877306, abs=1e-12)  # C++
    r = em.Random(7)
    xs = [r.normal("azimuth_noise", i) for i in range(20000)]
    assert sum(xs) / len(xs) == pytest.approx(0.0, abs=0.03)
    assert sum(x * x for x in xs) / len(xs) == pytest.approx(1.0, abs=0.04)


def test_free_space_and_knife_edge():
    assert em.free_space_loss_db(10000.0, 1e9) == pytest.approx(112.45, abs=0.01)
    assert em.knife_edge_loss_db(0.0) == pytest.approx(6.03, abs=0.01)
    assert em.knife_edge_loss_db(-0.8) == 0.0
    assert em.knife_edge_loss_db(2.4) == pytest.approx(20.54, abs=0.05)


def test_ridge_costs_microwave_far_more_than_vhf():
    ridge = lambda e, n: 300.0 if 9000.0 <= e <= 11000.0 else 0.0  # noqa: E731
    g = em.analyse_path(ridge, 0.0, 0.0, 5.0, 20000.0, 0.0, 5.0, 50.0)
    assert not g.line_of_sight
    assert 12.0 < em.diffraction_loss_db(g, 50e6) < 22.0
    assert em.diffraction_loss_db(g, 6e9) > 38.0


def test_gases_and_two_ray():
    assert em.gaseous_loss_db(10000.0, 0.0, 0.0, 6e9) == pytest.approx(0.09, abs=0.01)
    r = em.compute_reflection(10.0, 100.0, 1000.0)
    assert r.path_difference_m == pytest.approx(2.0, abs=0.01)
    r.path_difference_m = 0.025
    assert em.two_ray_factor(r, 0.05, 1.0) == pytest.approx(2.0, abs=1e-9)
    assert em.plane_earth_factor(3.0, 3.0, 15000.0, 7.5) == pytest.approx(4 * math.pi * 9 / (7.5 * 15000), rel=1e-12)


def test_swerling_and_accuracy():
    assert em.db(em.swerling1_required_snr(0.9, 1e-6)) == pytest.approx(21.14, abs=0.01)
    assert em.db(em.swerling1_required_snr(0.5, 1e-6)) == pytest.approx(12.77, abs=0.01)
    assert em.range_sigma_m(1e-6, 100.0, 0.0) == pytest.approx(10.60, abs=0.01)
    assert em.jammer_band_overlap(6e9, 500e6, 6e9, 1e6) == pytest.approx(1e6 / 500e6)
    r = em.self_screen_burn_through_m(1e20, 1e9, 20.0)
    assert 1e20 / r ** 4 / (1 + 1e9 / r ** 2) == pytest.approx(20.0, rel=1e-9)


def generic_search():
    return em.RadarModel("search", 6e9, 34.0, math.radians(1.5), math.radians(4.0), -25.0,
                         [em.Beam(math.radians(3.0), math.radians(6.0)), em.Beam(math.radians(13.0), math.radians(14.0))],
                         antenna_height_m=5.0, max_range_m=80000.0)


def test_calibration_matches_cpp():
    env = em.Environment()
    m = generic_search()
    em.calibrate(m, env, 40000.0, 2.0, 3000.0)
    assert m.loop_gain == pytest.approx(7.637631681315e19, rel=1e-9)  # C++
    assert em.anchor_pd(m, env, 40000.0, 3000.0, 2.0) == pytest.approx(0.5, abs=1e-9)
    assert em.anchor_pd(m, env, 25000.0, 50.0, 2.0) == pytest.approx(0.579239550749, abs=1e-9)  # C++
    assert em.detection_range_at(m, env, 50.0, 2.0, 0.5) == pytest.approx(25300.0, abs=50.0)  # C++
    assert em.detection_range_at(m, env, 300.0, 2.0, 0.5) == pytest.approx(34900.0, abs=50.0)  # C++
    assert em.free_space_detection_range(m, 32.0, 0.5) / em.free_space_detection_range(m, 2.0, 0.5) == pytest.approx(2.0)


def test_tracking_radar_low_target_mode_matches_cpp():
    env = em.Environment()
    t = em.RadarModel("track", 15e9, 40.0, math.radians(1.2), math.radians(1.2), -28.0, antenna_height_m=4.5,
                      max_range_m=32000.0, low_target_elevation_rad=math.radians(0.5))
    em.calibrate(t, env, 25000.0, 2.0, 3000.0)
    assert t.loop_gain == pytest.approx(4.764785142391e18, rel=1e-9)  # C++
    assert em.anchor_pd(t, env, 12000.0, 50.0, 2.0) == pytest.approx(0.953514236350, abs=1e-9)  # C++
    assert em.detection_range_at(t, env, 50.0, 2.0, 0.5) == pytest.approx(26100.0, abs=50.0)  # C++


def test_bearing_fix_matches_cpp():
    truth = (15000.0, 40000.0)
    sites = [(0.0, 0.0), (30000.0, 0.0), (-20000.0, 10000.0)]
    noise = [0.01, -0.02, 0.015]
    lines = [em.BearingLine(s, math.atan2(truth[0] - s[0], truth[1] - s[1]) + n, 0.02) for s, n in zip(sites, noise)]
    f = em.bearing_fix(lines)
    assert f.valid
    assert f.east == pytest.approx(14726.713461, abs=1e-3)  # C++
    assert f.north == pytest.approx(38657.189860, abs=1e-3)
    assert f.cov[0] == pytest.approx(369377.821011, rel=1e-6)
    assert f.cov[1] == pytest.approx(197887.475146, rel=1e-6)
    assert f.cov[3] == pytest.approx(1003539.362850, rel=1e-6)
    assert f.chi2 == pytest.approx(0.107064028, rel=1e-6)
    exact = [em.BearingLine(s, math.atan2(truth[0] - s[0], truth[1] - s[1]), 0.02) for s in sites]
    fe = em.bearing_fix(exact)
    assert (fe.east, fe.north) == pytest.approx(truth, abs=1.0)
    parallel = [em.BearingLine((0.0, 0.0), 0.785, 0.02), em.BearingLine((100.0, 0.0), 0.79, 0.02)]
    assert not em.bearing_fix(parallel).valid


def test_tdoa_recovers_the_emitter():
    truth = (40000.0, 120000.0, 8000.0)
    sites = [(0, 0, 30), (-25000, 5000, 30), (25000, 3000, 30), (0, 30000, 30)]
    toa = [math.dist(truth, s) / em.C for s in sites]
    x = em.solve_tdoa(sites, toa, 30e-9, (0.0, 60000.0, 5000.0))
    assert x == pytest.approx(truth, abs=5.0)
    x2 = em.solve_tdoa(sites[:3], toa[:3], 30e-9, (0.0, 60000.0, 8000.0), fix_z=True)
    assert math.hypot(x2[0] - truth[0], x2[1] - truth[1]) < 5.0


def test_ellipses_and_covariance_intersection():
    assert em.ellipse_scale(0.5) == pytest.approx(1.1774, abs=1e-4)
    assert em.ellipse_scale(0.95) == pytest.approx(2.4477, abs=1e-4)
    major, minor, az = em.ellipse_of((100.0 ** 2, 0.0, 0.0, 400.0 ** 2))
    assert (major, minor, az) == pytest.approx((400.0, 100.0, 0.0))
    p = [[200.0 ** 2, 0, 0], [0, 200.0 ** 2, 0], [0, 0, 200.0 ** 2]]
    x, pf = em.covariance_intersect([1000, 2000, 300], p, [1000, 2000, 300], p)
    assert pf[0][0] == pytest.approx(p[0][0])  # the same report twice must not look more certain
    assert x == pytest.approx([1000, 2000, 300])
