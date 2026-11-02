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


# Woods and buildings stand on the profile beyond this from either end (simcore terrain.hpp kObstacleSkipM, a game value).
OBSTACLE_SKIP_M = 60.0


class Footprints:
    """Woods and buildings as footprints of their own (simcore Terrain::ObstacleSpacingM, ObstacleTopAt,
    ObstacleTopBoundM; roadmap R6). Subclass: `spacing_m` the narrowest footprint, `top_at(e, n)` the top above sea level
    of what stands there (None: nothing), `top_bound(e0, n0, e1, n1, below)` -> (a bound on the tops over that ground
    segment or None, the spacing to look at it at)."""
    spacing_m: float = 0.0

    def top_at(self, e: float, n: float) -> float | None:
        return None

    def top_bound(self, e0: float, n0: float, e1: float, n1: float, below: float) -> tuple[float | None, float]:
        return (1e300, self.spacing_m) if self.spacing_m > 0.0 else (None, 0.0)


def obstacles_along(fp: Footprints | None, ax: float, ay: float, ux: float, uy: float, total: float, s0: float, s1: float,
                    below: float, skip: float):
    """Mirror of simcore terrain.hpp ObstaclesAlong: yields (s, top) for the footprints over [s0, s1] of the path (less
    `skip` at either end) at their own spacing, where something there may stand higher than `below`."""
    if fp is None or fp.spacing_m <= 0.0:
        return
    lo, hi = max(s0, skip), min(s1, total - skip)
    if hi < lo:
        return
    bound, spacing = fp.top_bound(ax + ux * lo, ay + uy * lo, ax + ux * hi, ay + uy * hi, below)
    if bound is None or bound < below or spacing <= 0.0:
        return
    steps = int(math.ceil((hi - lo) / spacing))
    for k in range(steps + 1):
        s = lo + (hi - lo) * k / steps if steps > 0 else lo
        top = fp.top_at(ax + ux * s, ay + uy * s)
        if top is not None:
            yield s, top


def has_line_of_sight(terrain: TerrainFn, a: Vec3, a_alt: float, b: Vec3, b_alt: float,
                      spacing_m: float = 50.0, k: float = K_STD, clearance_m: float = 0.0,
                      obstacle: TerrainFn | None = None, obstacle_skip_m: float = OBSTACLE_SKIP_M,
                      footprints: Footprints | None = None) -> bool:
    """`obstacle(e, n)`: how high woods and buildings stand there (simcore Terrain::ObstacleHeightAt; None: none).
    `footprints`: the same as footprints, looked at between the samples at their own spacing (roadmap R6)."""
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
        x, y = a.x + dx * t, a.y + dy * t
        ground = terrain(x, y) + s * (dist - s) / (2 * re)
        if obstacle is not None and s > obstacle_skip_m and dist - s > obstacle_skip_m:
            ground += obstacle(x, y)
        if ground + clearance_m >= ray:
            return False
    if footprints is None or footprints.spacing_m <= 0.0:
        return True
    ux, uy = dx / dist, dy / dist

    def ray_at(s: float) -> float:
        return a_alt + (b_alt - a_alt) * s / dist

    def bulge_at(s: float) -> float:
        return s * (dist - s) / (2 * re)

    for i in range(1, n + 1):
        s0, s1 = dist * (i - 1) / n, dist * i / n
        most = bulge_at(dist / 2) if s0 <= dist / 2 <= s1 else max(bulge_at(s0), bulge_at(s1))
        below = min(ray_at(s0), ray_at(s1)) - most - clearance_m
        for s, top in obstacles_along(footprints, a.x, a.y, ux, uy, dist, s0, s1, below, obstacle_skip_m):
            if top + bulge_at(s) + clearance_m >= ray_at(s):
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
