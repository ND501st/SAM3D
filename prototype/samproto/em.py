"""Electromagnetic layer maths — mirrors simcore/em/*.hpp (random, propagation, detection,
calibration, geolocation, covariance intersection). Pure Python, no numpy.

Generic and textbook only; system numbers live in data/em/*.json. The C++ and Python tests check the
same values (prototype/tests/test_em.py vs simcore/tests/test_em_models.cpp); the random streams are
bit-identical.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Callable, List, Optional, Sequence, Tuple

from .geo import MEAN_RADIUS
from .radar import K_STD, TerrainFn, db, undb

C = 299792458.0
BOLTZMANN = 1.380649e-23
T0 = 290.0
KT0 = BOLTZMANN * T0
TWO_PI = 2.0 * math.pi
M64 = (1 << 64) - 1

# ----------------------------------------------------------------------------------- random

STREAMS = {"detection": 1, "range_noise": 2, "azimuth_noise": 3, "elevation_noise": 4, "esm_bearing": 5,
           "esm_frequency": 6, "esm_pri": 7, "esm_pulse_width": 8, "comms_delivery": 9, "iff": 10,
           "false_alarm": 11, "tdoa_noise": 12, "doctrine": 13}


def mix64(x: int) -> int:
    x = (x + 0x9E3779B97F4A7C15) & M64
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & M64
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & M64
    return x ^ (x >> 31)


def draw_hash(seed: int, stream: int, a: int, b: int = 0, c: int = 0, d: int = 0) -> int:
    h = mix64((seed ^ ((stream * 0xD1B54A32D192ED03) & M64)) & M64)
    for k in (a, b, c, d):
        h = mix64(h ^ (k & M64))
    return h


def uniform01(h: int) -> float:
    return (h >> 11) * (1.0 / 9007199254740992.0)


def standard_normal(h: int) -> float:
    u1 = uniform01(mix64(h ^ 0x5851F42D4C957F2D))
    u2 = uniform01(mix64(h ^ 0x14057B7EF767814F))
    u1 = max(u1, 1e-300)
    return math.sqrt(-2.0 * math.log(u1)) * math.cos(TWO_PI * u2)


class Random:
    def __init__(self, seed: int = 1):
        self.seed = seed

    def uniform(self, stream: str, a: int, b: int = 0, c: int = 0, d: int = 0) -> float:
        return uniform01(draw_hash(self.seed, STREAMS[stream], a, b, c, d))

    def normal(self, stream: str, a: int, b: int = 0, c: int = 0, d: int = 0) -> float:
        return standard_normal(draw_hash(self.seed, STREAMS[stream], a, b, c, d))


# ----------------------------------------------------------------------------------- propagation

def wavelength(freq_hz: float) -> float:
    return C / freq_hz if freq_hz > 0 else 0.0


def free_space_loss_db(range_m: float, freq_hz: float) -> float:
    if range_m <= 0 or freq_hz <= 0:
        return 0.0
    return 20.0 * math.log10(4.0 * math.pi * range_m / wavelength(freq_hz))


def knife_edge_loss_db(v: float) -> float:
    """ITU-R P.526-15 eq. (31)."""
    if v <= -0.78:
        return 0.0
    t = v - 0.1
    return 6.9 + 20.0 * math.log10(math.sqrt(t * t + 1.0) + t)


_GAS = [(0.1e9, 0.0040), (1.0e9, 0.0055), (3.0e9, 0.0075), (6.0e9, 0.0090), (10.0e9, 0.0130), (15.0e9, 0.0280),
        (20.0e9, 0.0900), (22.2e9, 0.1800), (25.0e9, 0.1400), (30.0e9, 0.1100), (35.0e9, 0.1200), (40.0e9, 0.1500)]


def gaseous_attenuation_db_per_km(freq_hz: float) -> float:
    if freq_hz <= _GAS[0][0]:
        return _GAS[0][1]
    if freq_hz >= _GAS[-1][0]:
        return _GAS[-1][1]
    for (f0, a0), (f1, a1) in zip(_GAS, _GAS[1:]):
        if freq_hz <= f1:
            t = math.log(freq_hz / f0) / math.log(f1 / f0)
            return a0 + t * (a1 - a0)
    return _GAS[-1][1]


def gaseous_loss_db(range_m: float, alt_a: float, alt_b: float, freq_hz: float) -> float:
    if range_m <= 0:
        return 0.0
    hs = 6000.0
    ha, hb = max(alt_a, 0.0), max(alt_b, 0.0)
    mean = math.exp(-ha / hs)
    if abs(hb - ha) >= 1.0:
        mean = hs * (math.exp(-ha / hs) - math.exp(-hb / hs)) / (hb - ha)
    return gaseous_attenuation_db_per_km(freq_hz) * range_m / 1000.0 * mean


@dataclass
class PathGeometry:
    ground_range_m: float = 0.0
    slant_range_m: float = 0.0
    line_of_sight: bool = True
    min_clearance_m: float = 1e9
    worst_v_geometry: float = -1e9


def analyse_path(terrain: TerrainFn, a_e: float, a_n: float, a_alt: float, b_e: float, b_n: float, b_alt: float,
                 spacing_m: float = 100.0, max_samples: int = 600, k: float = K_STD) -> PathGeometry:
    g = PathGeometry()
    dx, dy = b_e - a_e, b_n - a_n
    g.ground_range_m = math.hypot(dx, dy)
    dz = b_alt - a_alt
    g.slant_range_m = math.hypot(g.ground_range_m, dz)
    if g.ground_range_m < 1.0:
        return g
    re = k * MEAN_RADIUS
    n = min(int(g.ground_range_m / max(spacing_m, 1.0)), max_samples)
    n = max(n, 2)
    for i in range(1, n):
        t = i / n
        s = t * g.ground_range_m
        rest = g.ground_range_m - s
        ray = a_alt + dz * t
        ground = terrain(a_e + dx * t, a_n + dy * t) + s * rest / (2.0 * re)
        h = ground - ray
        g.min_clearance_m = min(g.min_clearance_m, -h)
        if h >= 0.0:
            g.line_of_sight = False
        g.worst_v_geometry = max(g.worst_v_geometry, h * math.sqrt(1.0 / s + 1.0 / rest))
    return g


def diffraction_loss_db(g: PathGeometry, freq_hz: float) -> float:
    if g.ground_range_m < 1.0 or freq_hz <= 0:
        return 0.0
    return knife_edge_loss_db(g.worst_v_geometry * math.sqrt(2.0 / wavelength(freq_hz)))


@dataclass
class Reflection:
    valid: bool = False
    near_horizon: bool = False
    d1_m: float = 0.0
    h1_eff_m: float = 0.0
    h2_eff_m: float = 0.0
    grazing_rad: float = 0.0
    path_difference_m: float = 0.0


def compute_reflection(h1: float, h2: float, d: float, k: float = K_STD) -> Reflection:
    r = Reflection()
    if d < 1.0 or h1 <= 0 or h2 <= 0:
        return r
    floor = 0.1
    re = k * MEAN_RADIUS
    d1 = d * h1 / (h1 + h2)
    h1e, h2e = h1, h2
    for _ in range(8):
        d2 = d - d1
        h1e = h1 - d1 * d1 / (2.0 * re)
        h2e = h2 - d2 * d2 / (2.0 * re)
        if h1e < floor:
            h1e, r.near_horizon = floor, True
        if h2e < floor:
            h2e, r.near_horizon = floor, True
        d1 = d * h1e / (h1e + h2e)
    r.valid, r.d1_m, r.h1_eff_m, r.h2_eff_m = True, d1, h1e, h2e
    r.grazing_rad = math.atan((h1e + h2e) / d)
    r.path_difference_m = 2.0 * h1e * h2e / d
    return r


def two_ray_factor(r: Reflection, lam: float, rho: float, gain_ratio: float = 1.0) -> float:
    if not r.valid or lam <= 0:
        return 1.0
    g = rho * gain_ratio
    f2 = 1.0 + g * g - 2.0 * g * math.cos(TWO_PI * r.path_difference_m / lam)
    return math.sqrt(max(f2, 1e-6))


def plane_earth_factor(h1: float, h2: float, d: float, lam: float) -> float:
    if d < 1.0 or lam <= 0:
        return 1.0
    f = 4.0 * math.pi * max(h1, 0.5) * max(h2, 0.5) / (lam * d)
    return min(f, 1.0)


@dataclass
class Environment:
    surface_reflection: float = 0.9
    surface_roughness_m: float = 1.0
    k_factor: float = K_STD
    profile_spacing_m: float = 100.0


def effective_reflection(env: Environment, grazing: float, lam: float) -> float:
    if lam <= 0:
        return 0.0
    g = math.pi * env.surface_roughness_m * math.sin(grazing) / lam
    return env.surface_reflection * math.exp(-8.0 * g * g)


# ----------------------------------------------------------------------------------- detection

def swerling1_pd(snr: float, pfa: float) -> float:
    return pfa if snr <= 0 else pfa ** (1.0 / (1.0 + snr))


def swerling1_required_snr(pd: float, pfa: float) -> float:
    return 0.0 if pd <= pfa else math.log(pfa) / math.log(pd) - 1.0


def range_sigma_m(pw_s: float, snr: float, floor_m: float) -> float:
    s = max(snr, 0.1)
    thermal = C * pw_s / (2.0 * math.sqrt(2.0 * s))
    return math.hypot(thermal, floor_m)


def angle_sigma_rad(bw: float, snr: float, km: float, floor: float) -> float:
    s = max(snr, 0.1)
    thermal = bw / (max(km, 0.1) * math.sqrt(2.0 * s))
    return math.hypot(thermal, floor)


def jammer_band_overlap(jc: float, jbw: float, rc: float, rbw: float) -> float:
    if jbw <= 0 or rbw <= 0:
        return 0.0
    lo = max(jc - 0.5 * jbw, rc - 0.5 * rbw)
    hi = min(jc + 0.5 * jbw, rc + 0.5 * rbw)
    return 0.0 if hi <= lo else (hi - lo) / jbw


def self_screen_burn_through_m(snr_1m: float, jn_1m: float, req: float) -> float:
    if snr_1m <= 0 or req <= 0:
        return 0.0
    a, b, c = req, req * jn_1m, -snr_1m
    x = (-b + math.sqrt(b * b - 4 * a * c)) / (2 * a)
    return math.sqrt(x) if x > 0 else 0.0


def ellipse_scale(p: float) -> float:
    if p <= 0:
        return 0.0
    return math.sqrt(-2.0 * math.log(1.0 - min(p, 0.999999)))


# ----------------------------------------------------------------------------------- calibration

@dataclass
class Beam:
    centre_rad: float
    height_rad: float


@dataclass
class RadarModel:
    """The parts of simcore::em::RadarDef that calibration needs."""
    role: str                      # "search" or "track"
    freq_hz: float
    gain_db: float
    beamwidth_az_rad: float
    beamwidth_el_rad: float
    sidelobe_db: float = -25.0
    beams: List[Beam] = field(default_factory=list)
    pfa: float = 1e-6
    antenna_height_m: float = 4.0
    max_range_m: float = 0.0
    low_target_elevation_rad: float = 0.0
    loop_gain: float = 0.0


def antenna_gain_db(gain_db: float, bw_az: float, bw_el: float, sidelobe_db: float, off_az: float, off_el: float) -> float:
    loss = 0.0
    if bw_az > 0:
        loss -= 12.0 * (off_az / bw_az) ** 2
    if bw_el > 0:
        loss -= 12.0 * (off_el / bw_el) ** 2
    return gain_db + max(loss, sidelobe_db)


def beam_for(m: RadarModel, el: float) -> Beam:
    if not m.beams:
        return Beam(0.0, m.beamwidth_el_rad)
    best, best_d = m.beams[0], 1e9
    for b in m.beams:
        off = abs(el - b.centre_rad)
        if off <= 0.5 * b.height_rad:
            return b
        if off < best_d:
            best, best_d = b, off
    return best


def _beam_gain_db(m: RadarModel, b: Beam) -> float:
    if b.height_rad > 0 and m.beamwidth_el_rad > 0:
        return m.gain_db - 10.0 * math.log10(b.height_rad / m.beamwidth_el_rad)
    return m.gain_db


def anchor_snr(m: RadarModel, env: Environment, range_m: float, target_alt: float, rcs: float) -> float:
    """Mirror of AnchorLook + RadarSnr: flat ground at sea level, antenna at its height."""
    hr = m.antenna_height_m
    dh = target_alt - hr
    ground = math.sqrt(range_m * range_m - dh * dh) if range_m > abs(dh) else 1.0
    el = math.atan(dh / ground - ground / (2.0 * env.k_factor * MEAN_RADIUS))
    if m.role == "search":
        beam = beam_for(m, el)
        g_beam, h_beam = _beam_gain_db(m, beam), (beam.height_rad if beam.height_rad > 0 else m.beamwidth_el_rad)
    else:
        beam = Beam(max(el, m.low_target_elevation_rad), 0.0)
        g_beam, h_beam = m.gain_db, m.beamwidth_el_rad
    g = analyse_path(lambda _e, _n: 0.0, 0.0, 0.0, hr, 0.0, ground, target_alt, env.profile_spacing_m, 600, env.k_factor)
    r = compute_reflection(hr, max(target_alt, 0.5), ground, env.k_factor)
    lam = wavelength(m.freq_hz)
    rho = effective_reflection(env, r.grazing_rad, lam)
    g_dir = antenna_gain_db(g_beam, m.beamwidth_az_rad, h_beam, m.sidelobe_db, 0.0, el - beam.centre_rad)
    g_ref = antenna_gain_db(g_beam, m.beamwidth_az_rad, h_beam, m.sidelobe_db, 0.0, -r.grazing_rad - beam.centre_rad)
    ratio = 10.0 ** ((g_ref - g_dir) / 20.0) if h_beam > 0 else 1.0
    pattern_db = 2.0 * (g_dir - g_beam + (g_beam - m.gain_db))
    loss_db = diffraction_loss_db(g, m.freq_hz) + gaseous_loss_db(range_m, hr, target_alt, m.freq_hz)
    f = two_ray_factor(r, lam, rho, ratio)
    return m.loop_gain * rcs / range_m ** 4 * undb(pattern_db - 2.0 * loss_db) * f ** 4


def calibrate(m: RadarModel, env: Environment, range_m: float, rcs: float, target_alt: float, pd: float = 0.5) -> None:
    m.loop_gain = 1.0
    unit = anchor_snr(m, env, range_m, target_alt, rcs)
    m.loop_gain = swerling1_required_snr(pd, m.pfa) / unit


def anchor_pd(m: RadarModel, env: Environment, range_m: float, target_alt: float, rcs: float) -> float:
    return swerling1_pd(anchor_snr(m, env, range_m, target_alt, rcs), m.pfa)


def detection_range_at(m: RadarModel, env: Environment, target_alt: float, rcs: float, pd: float,
                       step_m: float = 50.0) -> float:
    r = m.max_range_m if m.max_range_m > 0 else 200000.0
    while r > step_m:
        if anchor_pd(m, env, r, target_alt, rcs) >= pd:
            return r
        r -= step_m
    return 0.0


def free_space_detection_range(m: RadarModel, rcs: float, pd: float, jn: float = 0.0) -> float:
    req = swerling1_required_snr(pd, m.pfa) * (1.0 + jn)
    return (m.loop_gain * rcs / req) ** 0.25 if m.loop_gain > 0 and req > 0 else 0.0


# ----------------------------------------------------------------------------------- geolocation

@dataclass
class BearingLine:
    origin: Tuple[float, float]
    azimuth_rad: float
    sigma_rad: float
    time_s: float = 0.0


def wrap_pi(a: float) -> float:
    a = math.fmod(a, TWO_PI)
    if a < 0:
        a += TWO_PI
    return a - TWO_PI if a > math.pi else a


def age_inflated_sigma(sigma: float, age: float, vmax: float, rng: float) -> float:
    if age <= 0 or vmax <= 0 or rng <= 1.0:
        return sigma
    return math.hypot(sigma, vmax * age / rng)


def _inv2(a, b, c, d):
    det = a * d - b * c
    if abs(det) < 1e-300:
        return None
    return (d / det, -b / det, -c / det, a / det)


@dataclass
class Fix:
    valid: bool
    east: float = 0.0
    north: float = 0.0
    cov: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0)  # row-major 2x2
    chi2: float = 0.0


def bearing_fix(lines: Sequence[BearingLine], now: float = 0.0, vmax: float = 0.0, min_crossing: float = 0.05) -> Fix:
    """Mirror of simcore::em::BearingFix: pseudolinear passes, then ML Gauss-Newton on the angles."""
    if len(lines) < 2:
        return Fix(False)
    best = 0.0
    for i in range(len(lines)):
        for j in range(i + 1, len(lines)):
            d = abs(wrap_pi(lines[i].azimuth_rad - lines[j].azimuth_rad))
            best = max(best, math.pi - d if d > 0.5 * math.pi else d)
    if best < min_crossing:
        return Fix(False)
    est = None
    info = None
    for _ in range(4):
        a00 = a01 = a11 = b0 = b1 = 0.0
        for l in lines:
            nx, ny = math.cos(l.azimuth_rad), -math.sin(l.azimuth_rad)
            rng = 10000.0
            if est is not None:
                rng = max(math.hypot(est[0] - l.origin[0], est[1] - l.origin[1]), 100.0)
            sig = max(age_inflated_sigma(l.sigma_rad, now - l.time_s, vmax, rng), 1e-6)
            w = 1.0 / (sig * sig * rng * rng)
            d = nx * l.origin[0] + ny * l.origin[1]
            a00 += w * nx * nx
            a01 += w * nx * ny
            a11 += w * ny * ny
            b0 += w * nx * d
            b1 += w * ny * d
        inv = _inv2(a00, a01, a01, a11)
        if inv is None:
            return Fix(False)
        est = (inv[0] * b0 + inv[1] * b1, inv[2] * b0 + inv[3] * b1)
        info = (a00, a01, a01, a11)
    for _ in range(15):
        j00 = j01 = j11 = r0 = r1 = 0.0
        for l in lines:
            ex, ey = est[0] - l.origin[0], est[1] - l.origin[1]
            d2 = max(ex * ex + ey * ey, 1e4)
            sig = max(age_inflated_sigma(l.sigma_rad, now - l.time_s, vmax, math.sqrt(d2)), 1e-6)
            w = 1.0 / (sig * sig)
            res = wrap_pi(l.azimuth_rad - math.atan2(ex, ey))
            jx, jy = ey / d2, -ex / d2
            j00 += w * jx * jx
            j01 += w * jx * jy
            j11 += w * jy * jy
            r0 += w * jx * res
            r1 += w * jy * res
        inv = _inv2(j00, j01, j01, j11)
        if inv is None:
            break
        sx, sy = inv[0] * r0 + inv[1] * r1, inv[2] * r0 + inv[3] * r1
        ln = math.hypot(sx, sy)
        if ln > 20000.0:
            sx, sy = sx * 20000.0 / ln, sy * 20000.0 / ln
        est = (est[0] + sx, est[1] + sy)
        info = (j00, j01, j01, j11)
        if ln < 1.0:
            break
    behind = sum(1 for l in lines if (est[0] - l.origin[0]) * math.sin(l.azimuth_rad)
                 + (est[1] - l.origin[1]) * math.cos(l.azimuth_rad) < 0)
    if 2 * behind > len(lines):
        return Fix(False)
    cov = _inv2(*info)
    if cov is None:
        return Fix(False)
    chi2 = 0.0
    for l in lines:
        ex, ey = est[0] - l.origin[0], est[1] - l.origin[1]
        miss = wrap_pi(math.atan2(ex, ey) - l.azimuth_rad)
        sig = age_inflated_sigma(l.sigma_rad, now - l.time_s, vmax, max(math.hypot(ex, ey), 1.0))
        chi2 += (miss / sig) ** 2
    c01 = 0.5 * (cov[1] + cov[2])
    return Fix(True, est[0], est[1], (cov[0], c01, c01, cov[3]), chi2)


def ellipse_of(cov: Tuple[float, float, float, float]) -> Tuple[float, float, float]:
    """(semi-major, semi-minor, major-axis azimuth) of a 2x2 covariance, 1 sigma."""
    a, b, c = cov[0], 0.5 * (cov[1] + cov[2]), cov[3]
    mean, diff = 0.5 * (a + c), 0.5 * (a - c)
    r = math.hypot(diff, b)
    from_east = 0.5 * math.atan2(2.0 * b, a - c)
    az = 0.5 * math.pi - from_east
    while az < 0:
        az += math.pi
    while az >= math.pi:
        az -= math.pi
    return math.sqrt(max(mean + r, 0.0)), math.sqrt(max(mean - r, 0.0)), az


def cep_m(cov: Tuple[float, float, float, float]) -> float:
    major, minor, _ = ellipse_of(cov)
    return 0.59 * (major + minor)


def solve_tdoa(sites: Sequence[Tuple[float, float, float]], arrivals: Sequence[float], sigma_t: float,
               initial: Tuple[float, float, float], fix_z: bool = False) -> Optional[Tuple[float, float, float]]:
    """Mirror of simcore::em::SolveTdoa (position only)."""
    n = len(sites)
    if n < (3 if fix_z else 4):
        return None
    m = n - 1
    dmeas = [C * (arrivals[i + 1] - arrivals[0]) for i in range(m)]
    sm = 1.0 / (1.0 + m)
    x = list(initial)
    for _ in range(20):
        u0 = [x[k] - sites[0][k] for k in range(3)]
        r0 = max(math.sqrt(sum(v * v for v in u0)), 1e-6)
        jac, res = [], []
        for i in range(m):
            ui = [x[k] - sites[i + 1][k] for k in range(3)]
            ri = max(math.sqrt(sum(v * v for v in ui)), 1e-6)
            res.append(dmeas[i] - (ri - r0))
            row = [ui[k] / ri - u0[k] / r0 for k in range(3)]
            if fix_z:
                row[2] = 0.0
            jac.append(row)
        sj = [sum(jac[i][k] for i in range(m)) for k in range(3)]
        sr = sum(res)
        a = [[0.0] * 3 for _ in range(3)]
        b = [0.0] * 3
        for i in range(m):
            wj = [jac[i][k] - sj[k] * sm for k in range(3)]
            wr = res[i] - sr * sm
            for p in range(3):
                b[p] += jac[i][p] * wr
                for q in range(3):
                    a[p][q] += jac[i][p] * wj[q]
        if fix_z:
            a[2][2] = 1.0
        step = _solve3(a, b)
        if step is None:
            return None
        x[0] += step[0]
        x[1] += step[1]
        if not fix_z:
            x[2] += step[2]
        if math.sqrt(step[0] ** 2 + step[1] ** 2 + (0 if fix_z else step[2] ** 2)) < 1e-3:
            break
    return tuple(x)


def _solve3(a, b):
    det = (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
           + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))
    if abs(det) < 1e-300:
        return None

    def col(k):
        m = [row[:] for row in a]
        for i in range(3):
            m[i][k] = b[i]
        return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0])) / det
    return [col(0), col(1), col(2)]


# ----------------------------------------------------------------------------------- fusion

def covariance_intersect(xa, pa, xb, pb):
    """Fast covariance intersection of two 3-D estimates (row-major 3x3 lists)."""
    def det3(m):
        return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))

    def inv3(m):
        d = det3(m)
        return [[(m[(j + 1) % 3][(i + 1) % 3] * m[(j + 2) % 3][(i + 2) % 3]
                  - m[(j + 1) % 3][(i + 2) % 3] * m[(j + 2) % 3][(i + 1) % 3]) / d for j in range(3)] for i in range(3)]

    da, dbb = det3(pa), det3(pb)
    w = dbb / (da + dbb) if da + dbb > 0 else 0.5
    ia, ib = inv3(pa), inv3(pb)
    info = [[w * ia[i][j] + (1 - w) * ib[i][j] for j in range(3)] for i in range(3)]
    p = inv3(info)
    v = [sum(w * ia[i][j] * xa[j] + (1 - w) * ib[i][j] * xb[j] for j in range(3)) for i in range(3)]
    x = [sum(p[i][j] * v[j] for j in range(3)) for i in range(3)]
    return x, p


# ----------------------------------------------------------------------------------- optics (TV sight, roadmap E5)

KOSCHMIEDER = 3.912          # -ln(0.02)
REFERENCE_VISIBILITY_M = 40000.0


def apparent_contrast(inherent: float, range_m: float, visibility_m: float) -> float:
    """Koschmieder: a target's apparent contrast against the sky at a range (mirrors em/optical.hpp)."""
    if visibility_m <= 0.0:
        return 0.0
    return inherent * math.exp(-KOSCHMIEDER * range_m / visibility_m)


def contrast_range_m(inherent: float, threshold: float, visibility_m: float) -> float:
    if threshold <= 0.0 or inherent <= threshold:
        return 0.0
    return math.log(inherent / threshold) * visibility_m / KOSCHMIEDER


def lines_across(size_m: float, range_m: float, fov_height_rad: float, tv_lines: float = 575.0) -> float:
    """Johnson: camera lines across a target's critical dimension."""
    if range_m <= 0.0 or fov_height_rad <= 0.0:
        return 1e9
    return (size_m / range_m) / (fov_height_rad / tv_lines)
