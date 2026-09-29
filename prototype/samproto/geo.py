"""WGS-84 geodesy, local ENU frame, and ISA atmosphere — mirrors simcore geo.hpp / atmosphere.hpp."""
from __future__ import annotations

import math
from dataclasses import dataclass

from .vec import G0, Vec3, clamp

A = 6378137.0
F = 1.0 / 298.257223563
B = A * (1.0 - F)
E2 = F * (2.0 - F)
MEAN_RADIUS = 6371008.8


@dataclass(frozen=True)
class GeoPoint:
    lat_rad: float
    lon_rad: float
    alt_m: float

    @staticmethod
    def from_degrees(lat: float, lon: float, alt: float) -> "GeoPoint":
        return GeoPoint(math.radians(lat), math.radians(lon), alt)

    @property
    def lat_deg(self) -> float:
        return math.degrees(self.lat_rad)

    @property
    def lon_deg(self) -> float:
        return math.degrees(self.lon_rad)


def geodetic_to_ecef(g: GeoPoint) -> Vec3:
    sl, cl = math.sin(g.lat_rad), math.cos(g.lat_rad)
    n = A / math.sqrt(1.0 - E2 * sl * sl)
    return Vec3((n + g.alt_m) * cl * math.cos(g.lon_rad),
                (n + g.alt_m) * cl * math.sin(g.lon_rad),
                (n * (1.0 - E2) + g.alt_m) * sl)


def ecef_to_geodetic(e: Vec3) -> GeoPoint:
    p = math.hypot(e.x, e.y)
    lon = math.atan2(e.y, e.x)
    if p < 1e-6:
        return GeoPoint(math.copysign(math.pi / 2, e.z), lon, abs(e.z) - B)
    lat = math.atan2(e.z, p * (1.0 - E2))
    alt = 0.0
    for _ in range(8):
        sl = math.sin(lat)
        n = A / math.sqrt(1.0 - E2 * sl * sl)
        alt = p / math.cos(lat) - n
        lat = math.atan2(e.z, p * (1.0 - E2 * n / (n + alt)))
    return GeoPoint(lat, lon, alt)


class LocalFrame:
    """ENU tangent-plane frame: x = East, y = North, z = Up."""

    def __init__(self, origin: GeoPoint):
        self.origin = origin
        self._o = geodetic_to_ecef(origin)
        sl, cl = math.sin(origin.lat_rad), math.cos(origin.lat_rad)
        so, co = math.sin(origin.lon_rad), math.cos(origin.lon_rad)
        self._e = Vec3(-so, co, 0.0)
        self._n = Vec3(-sl * co, -sl * so, cl)
        self._u = Vec3(cl * co, cl * so, sl)

    def ecef_to_enu(self, ecef: Vec3) -> Vec3:
        d = ecef - self._o
        return Vec3(d.dot(self._e), d.dot(self._n), d.dot(self._u))

    def enu_to_ecef(self, enu: Vec3) -> Vec3:
        return self._o + self._e * enu.x + self._n * enu.y + self._u * enu.z

    def geodetic_to_enu(self, g: GeoPoint) -> Vec3:
        return self.ecef_to_enu(geodetic_to_ecef(g))

    def enu_to_geodetic(self, enu: Vec3) -> GeoPoint:
        return ecef_to_geodetic(self.enu_to_ecef(enu))

    def altitude_of(self, enu: Vec3) -> float:
        return self.enu_to_geodetic(enu).alt_m


# ---------------- ISA atmosphere (0–32 km, geopotential == geometric) ----------------
_T0, _P0, _R, _GAMMA = 288.15, 101325.0, 287.05287, 1.4


@dataclass(frozen=True)
class Atmosphere:
    temperature_k: float
    pressure_pa: float
    density_kg_m3: float
    speed_of_sound_mps: float


def standard_atmosphere(alt_m: float) -> Atmosphere:
    h = clamp(alt_m, -500.0, 32000.0)
    t11 = 216.65
    p11 = _P0 * (t11 / _T0) ** (G0 / (0.0065 * _R))
    if h <= 11000.0:
        t = _T0 - 0.0065 * h
        p = _P0 * (t / _T0) ** (G0 / (0.0065 * _R))
    elif h <= 20000.0:
        t = t11
        p = p11 * math.exp(-G0 * (h - 11000.0) / (_R * t11))
    else:
        p20 = p11 * math.exp(-G0 * 9000.0 / (_R * t11))
        t = t11 + 0.001 * (h - 20000.0)
        p = p20 * (t / t11) ** (-G0 / (0.001 * _R))
    if alt_m > 32000.0:
        p *= math.exp(-G0 * (alt_m - 32000.0) / (_R * t))
    rho = p / (_R * t)
    return Atmosphere(t, p, rho, math.sqrt(_GAMMA * _R * t))
