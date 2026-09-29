"""Generic radar physics — mirrors simcore/radar.hpp. Textbook level; no real-system data."""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Callable

from .geo import MEAN_RADIUS, LocalFrame
from .vec import Vec3, angle_between, azimuth_of, elevation_of

BOLTZMANN = 1.380649e-23
T0 = 290.0
K_STD = 4.0 / 3.0

TerrainFn = Callable[[float, float], float]  # (east_m, north_m) -> height above ellipsoid [m]


def flat(height: float = 0.0) -> TerrainFn:
    return lambda _e, _n: height


def db(x: float) -> float:
    return 10.0 * math.log10(x)


def undb(x: float) -> float:
    return 10.0 ** (x / 10.0)


@dataclass
class RadarParams:
    peak_power_w: float
    antenna_gain_db: float
    wavelength_m: float
    bandwidth_hz: float
    noise_figure_db: float
    system_losses_db: float
    beamwidth_3db_rad: float = 0.0
    detection_threshold_db: float = 13.0
    min_range_m: float = 0.0
    max_instrumented_range_m: float = 0.0


def snr_db(p: RadarParams, range_m: float, rcs_m2: float) -> float:
    if range_m <= 0 or rcs_m2 <= 0:
        return -300.0
    g = undb(p.antenna_gain_db)
    num = p.peak_power_w * g * g * p.wavelength_m ** 2 * rcs_m2
    noise = BOLTZMANN * T0 * p.bandwidth_hz * undb(p.noise_figure_db)
    den = (4 * math.pi) ** 3 * range_m ** 4 * noise * undb(p.system_losses_db)
    return db(num / den)


def detection_range(p: RadarParams, rcs_m2: float) -> float:
    return (undb(snr_db(p, 1.0, rcs_m2)) / undb(p.detection_threshold_db)) ** 0.25


def beam_shape_loss_db(off_axis_rad: float, bw3db_rad: float) -> float:
    if bw3db_rad <= 0:
        return 0.0
    r = off_axis_rad / bw3db_rad
    return -24.0 * r * r


def radar_horizon(h_radar: float, h_target: float, k: float = K_STD) -> float:
    re = k * MEAN_RADIUS
    return math.sqrt(2 * re * max(h_radar, 0.0)) + math.sqrt(2 * re * max(h_target, 0.0))


def has_line_of_sight(terrain: TerrainFn, a: Vec3, a_alt: float, b: Vec3, b_alt: float,
                      spacing_m: float = 50.0, k: float = K_STD, clearance_m: float = 0.0) -> bool:
    dx, dy = b.x - a.x, b.y - a.y
    dist = math.hypot(dx, dy)
    if dist < 1.0:
        return True
    re = k * MEAN_RADIUS
    n = max(2, int(dist / spacing_m))
    for i in range(1, n):
        t = i / n
        s = t * dist
        ray = a_alt + (b_alt - a_alt) * t
        ground = terrain(a.x + dx * t, a.y + dy * t) + s * (dist - s) / (2 * re)
        if ground + clearance_m >= ray:
            return False
    return True


@dataclass
class Detection:
    line_of_sight: bool
    in_range: bool
    detected: bool
    range_m: float
    azimuth_rad: float
    elevation_rad: float
    snr_db: float


def evaluate_detection(p: RadarParams, frame: LocalFrame, terrain: TerrainFn, radar: Vec3,
                       boresight: Vec3, target: Vec3, rcs_m2: float) -> Detection:
    los = target - radar
    rng = los.length()
    in_range = rng >= p.min_range_m and (p.max_instrumented_range_m <= 0 or rng <= p.max_instrumented_range_m)
    visible = has_line_of_sight(terrain, radar, frame.altitude_of(radar), target, frame.altitude_of(target))
    s = snr_db(p, rng, rcs_m2)
    if boresight.length_sq() > 0:
        s += beam_shape_loss_db(angle_between(boresight, los), p.beamwidth_3db_rad)
    return Detection(visible, in_range, in_range and visible and s >= p.detection_threshold_db,
                     rng, azimuth_of(los), elevation_of(los), s)
