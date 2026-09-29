"""Tiny 3D vector helpers (pure Python, no numpy) — mirrors simcore/vec3.hpp."""
from __future__ import annotations

import math
from dataclasses import dataclass

G0 = 9.80665


@dataclass(frozen=True)
class Vec3:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def __add__(self, o: "Vec3") -> "Vec3":
        return Vec3(self.x + o.x, self.y + o.y, self.z + o.z)

    def __sub__(self, o: "Vec3") -> "Vec3":
        return Vec3(self.x - o.x, self.y - o.y, self.z - o.z)

    def __neg__(self) -> "Vec3":
        return Vec3(-self.x, -self.y, -self.z)

    def __mul__(self, s: float) -> "Vec3":
        return Vec3(self.x * s, self.y * s, self.z * s)

    __rmul__ = __mul__

    def __truediv__(self, s: float) -> "Vec3":
        return Vec3(self.x / s, self.y / s, self.z / s)

    def dot(self, o: "Vec3") -> float:
        return self.x * o.x + self.y * o.y + self.z * o.z

    def cross(self, o: "Vec3") -> "Vec3":
        return Vec3(self.y * o.z - self.z * o.y, self.z * o.x - self.x * o.z, self.x * o.y - self.y * o.x)

    def length(self) -> float:
        return math.sqrt(self.dot(self))

    def length_sq(self) -> float:
        return self.dot(self)

    def horizontal_length(self) -> float:
        return math.hypot(self.x, self.y)

    def normalized(self) -> "Vec3":
        n = self.length()
        return self / n if n > 1e-12 else Vec3()


def clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v


def angle_between(a: Vec3, b: Vec3) -> float:
    la, lb = a.length(), b.length()
    if la < 1e-12 or lb < 1e-12:
        return 0.0
    return math.acos(clamp(a.dot(b) / (la * lb), -1.0, 1.0))


def rotate_about_axis(v: Vec3, k: Vec3, angle: float) -> Vec3:
    c, s = math.cos(angle), math.sin(angle)
    return v * c + k.cross(v) * s + k * (k.dot(v) * (1.0 - c))


def turn_toward(frm: Vec3, to: Vec3, max_angle: float) -> Vec3:
    angle = angle_between(frm, to)
    if angle <= max_angle or angle < 1e-12:
        return to.normalized()
    axis = frm.cross(to)
    if axis.length() < 1e-12:
        axis = frm.cross(Vec3(0, 0, 1))
        if axis.length() < 1e-12:
            axis = frm.cross(Vec3(1, 0, 0))
    return rotate_about_axis(frm, axis.normalized(), max_angle).normalized()


def azimuth_of(v: Vec3) -> float:
    """Radians clockwise from North, [0, 2pi)."""
    az = math.atan2(v.x, v.y)
    return az + 2 * math.pi if az < 0 else az


def elevation_of(v: Vec3) -> float:
    return math.atan2(v.z, v.horizontal_length())


def direction_from_az_el(az: float, el: float) -> Vec3:
    ce = math.cos(el)
    return Vec3(ce * math.sin(az), ce * math.cos(az), math.sin(el))
