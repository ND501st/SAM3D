"""Aircraft, missile (PN), world stepping and ACMI export — mirrors simcore aircraft/missile/world/acmi.

Use this for fast experiments (try a new guidance law, plot miss distance vs. N, etc.).
Once a model is settled, port it to simcore (C++) and keep both test suites agreeing.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from enum import Enum

from .geo import LocalFrame, standard_atmosphere
from .radar import TerrainFn, flat
from .vec import G0, Vec3, azimuth_of, clamp, direction_from_az_el, elevation_of, turn_toward


@dataclass
class Aircraft:
    name: str
    position: Vec3
    speed_mps: float = 200.0
    velocity: Vec3 = Vec3()
    waypoints: list[Vec3] = field(default_factory=list)
    max_turn_rate_rad_s: float = math.radians(6.0)
    rcs_m2: float = 5.0
    capture_radius_m: float = 500.0
    type: str = "Air+FixedWing"
    id: int = 0
    next_wp: int = 0
    alive: bool = True

    def step(self, dt: float) -> None:
        if not self.alive:
            return
        d = self.velocity.normalized()
        if d.length_sq() == 0:
            d = Vec3(0, 1, 0)
        if self.next_wp < len(self.waypoints):
            to_wp = self.waypoints[self.next_wp] - self.position
            if to_wp.length() <= self.capture_radius_m:
                self.next_wp += 1
            else:
                d = turn_toward(d, to_wp.normalized(), self.max_turn_rate_rad_s * dt)
        self.velocity = d * self.speed_mps
        self.position = self.position + self.velocity * dt


@dataclass
class MissileParams:
    launch_mass_kg: float
    burnout_mass_kg: float
    boost_thrust_n: float
    boost_time_s: float
    sustain_thrust_n: float
    sustain_time_s: float
    reference_area_m2: float
    drag_coefficient: float
    max_lateral_accel_mps2: float
    navigation_constant: float = 4.0
    launch_speed_mps: float = 30.0
    guidance_start_s: float = 0.5
    fuze_arming_time_s: float = 1.0
    proximity_fuze_radius_m: float = 15.0
    lethal_radius_m: float = 10.0
    max_flight_time_s: float = 40.0
    min_guided_speed_mps: float = 250.0

    @property
    def burn_time(self) -> float:
        return self.boost_time_s + self.sustain_time_s

    def mass(self, t: float) -> float:
        if self.burn_time <= 0 or t >= self.burn_time:
            return self.burnout_mass_kg
        return self.launch_mass_kg - (self.launch_mass_kg - self.burnout_mass_kg) * t / self.burn_time

    def thrust(self, t: float) -> float:
        if t < self.boost_time_s:
            return self.boost_thrust_n
        if t < self.burn_time:
            return self.sustain_thrust_n
        return 0.0


class Outcome(Enum):
    IN_FLIGHT = "in_flight"
    DETONATED = "detonated"
    MISSED = "missed"
    SELF_DESTRUCTED = "self_destructed"
    HIT_GROUND = "hit_ground"


@dataclass
class Missile:
    id: int
    target_id: int
    params: MissileParams
    position: Vec3
    velocity: Vec3
    t: float = 0.0
    closest_m: float = 1e30
    outcome: Outcome = Outcome.IN_FLIGHT

    @property
    def active(self) -> bool:
        return self.outcome is Outcome.IN_FLIGHT


def pn_accel(mp: Vec3, mv: Vec3, tp: Vec3, tv: Vec3, n: float) -> Vec3:
    r = tp - mp
    vr = tv - mv
    r2 = r.length_sq()
    if r2 < 1e-6:
        return Vec3()
    rng = math.sqrt(r2)
    omega = r.cross(vr) / r2
    vc = -r.dot(vr) / rng
    return omega.cross(r / rng) * (n * vc)


def closest_approach(rel0: Vec3, rel1: Vec3) -> tuple[float, float]:
    d = rel1 - rel0
    dd = d.length_sq()
    s = clamp(-rel0.dot(d) / dd, 0.0, 1.0) if dd > 1e-12 else 0.0
    return (rel0 + d * s).length(), s


def step_missile(m: Missile, tp0: Vec3, tp1: Vec3, tv: Vec3, altitude_m: float, dt: float) -> None:
    if not m.active:
        return
    p = m.params
    speed = m.velocity.length()
    vhat = m.velocity / speed if speed > 1e-6 else Vec3(0, 0, 1)
    rho = standard_atmosphere(altitude_m).density_kg_m3
    drag = 0.5 * rho * speed * speed * p.drag_coefficient * p.reference_area_m2
    acc = vhat * ((p.thrust(m.t) - drag) / p.mass(m.t)) + Vec3(0, 0, -G0)
    if m.t >= p.guidance_start_s:
        cmd = pn_accel(m.position, m.velocity, tp0, tv, p.navigation_constant) + Vec3(0, 0, G0)
        cmd = cmd - vhat * cmd.dot(vhat)
        mag = cmd.length()
        if mag > p.max_lateral_accel_mps2:
            cmd = cmd * (p.max_lateral_accel_mps2 / mag)
        acc = acc + cmd
    pos0 = m.position
    m.velocity = m.velocity + acc * dt
    m.position = m.position + m.velocity * dt
    m.t += dt
    rel0, rel1 = tp0 - pos0, tp1 - m.position
    dist, frac = closest_approach(rel0, rel1)
    m.closest_m = min(m.closest_m, dist)
    armed = m.t >= p.fuze_arming_time_s
    if armed and dist <= p.proximity_fuze_radius_m and frac < 1.0:
        m.outcome = Outcome.DETONATED
    elif armed and rel1.length() > rel0.length() and m.closest_m < 1000.0:
        m.outcome = Outcome.MISSED
    elif m.t >= p.max_flight_time_s:
        m.outcome = Outcome.SELF_DESTRUCTED
    elif m.t > p.burn_time and m.velocity.length() < p.min_guided_speed_mps:
        m.outcome = Outcome.SELF_DESTRUCTED


@dataclass
class Event:
    kind: str
    time_s: float
    missile_id: int
    aircraft_id: int
    miss_m: float = 0.0


class World:
    def __init__(self, frame: LocalFrame, terrain: TerrainFn | None = None):
        self.frame = frame
        self.terrain = terrain or flat(0.0)
        self.time = 0.0
        self._next_id = 1
        self.aircraft: list[Aircraft] = []
        self.missiles: list[Missile] = []
        self.events: list[Event] = []

    def add_aircraft(self, a: Aircraft) -> int:
        a.id = self._next_id
        self._next_id += 1
        if a.velocity.length_sq() == 0 and a.waypoints:
            a.velocity = (a.waypoints[0] - a.position).normalized() * a.speed_mps
        self.aircraft.append(a)
        return a.id

    def find(self, aircraft_id: int) -> Aircraft | None:
        return next((a for a in self.aircraft if a.id == aircraft_id), None)

    def launch(self, params: MissileParams, pos: Vec3, az: float, el: float, target_id: int) -> int:
        m = Missile(self._next_id, target_id, params, pos, direction_from_az_el(az, el) * params.launch_speed_mps)
        self._next_id += 1
        self.missiles.append(m)
        self.events.append(Event("MissileLaunched", self.time, m.id, target_id))
        return m.id

    def step(self, dt: float) -> None:
        starts = {a.id: a.position for a in self.aircraft}
        for a in self.aircraft:
            a.step(dt)
        t1 = self.time + dt
        for m in self.missiles:
            if not m.active:
                continue
            tgt = self.find(m.target_id)
            if tgt is None or not tgt.alive:
                m.outcome = Outcome.SELF_DESTRUCTED
                self.events.append(Event("MissileSelfDestructed", t1, m.id, m.target_id))
                continue
            step_missile(m, starts[tgt.id], tgt.position, tgt.velocity, self.frame.altitude_of(m.position), dt)
            if m.active and self.frame.altitude_of(m.position) < self.terrain(m.position.x, m.position.y):
                m.outcome = Outcome.HIT_GROUND
            if m.outcome is Outcome.DETONATED:
                self.events.append(Event("MissileDetonated", t1, m.id, tgt.id, m.closest_m))
                if m.closest_m <= m.params.lethal_radius_m:
                    tgt.alive = False
                    self.events.append(Event("AircraftDestroyed", t1, m.id, tgt.id, m.closest_m))
            elif m.outcome is Outcome.MISSED:
                self.events.append(Event("MissileMissed", t1, m.id, tgt.id, m.closest_m))
            elif m.outcome is Outcome.SELF_DESTRUCTED:
                self.events.append(Event("MissileSelfDestructed", t1, m.id, tgt.id, m.closest_m))
            elif m.outcome is Outcome.HIT_GROUND:
                self.events.append(Event("MissileHitGround", t1, m.id, tgt.id, m.closest_m))
        self.time = t1

    def drain_events(self) -> list[Event]:
        out, self.events = self.events, []
        return out


class AcmiWriter:
    """Tacview ACMI 2.2 text writer (https://www.tacview.net/documentation/acmi/en/)."""

    def __init__(self, frame: LocalFrame, reference_time: str = "2026-01-01T00:00:00Z",
                 title: str = "SAM3D prototype"):
        self.frame = frame
        self.lines = ["FileType=text/acmi/tacview", "FileVersion=2.2",
                      f"0,ReferenceTime={reference_time}", f"0,Title={title}", "0,DataSource=SAM3D prototype"]
        self._seen: set[int] = set()
        self._removed: set[int] = set()

    def frame_time(self, t: float) -> None:
        self.lines.append(f"#{t:.2f}")

    def obj(self, oid: int, enu: Vec3, vel: Vec3, props: str) -> None:
        g = self.frame.enu_to_geodetic(enu)
        heading = math.degrees(azimuth_of(vel))
        pitch = math.degrees(elevation_of(vel)) if vel.length_sq() > 0 else 0.0
        line = f"{oid:x},T={g.lon_deg:.7f}|{g.lat_deg:.7f}|{g.alt_m:.1f}|0|{pitch:.1f}|{heading:.1f}"
        if oid not in self._seen and props:
            line += "," + props
            self._seen.add(oid)
        self.lines.append(line)

    def remove(self, oid: int) -> None:
        self.lines.append(f"-{oid:x}")
        self._seen.discard(oid)

    def write_world(self, w: World) -> None:
        self.frame_time(w.time)
        for a in w.aircraft:
            if a.alive:
                self.obj(a.id, a.position, a.velocity, f"Type={a.type},Name={a.name},Color=Blue")
            elif a.id not in self._removed:
                self._removed.add(a.id)
                self.remove(a.id)
        for m in w.missiles:
            if m.active:
                self.obj(m.id, m.position, m.velocity, "Type=Weapon+Missile,Name=SAM,Color=Red")
            elif m.id not in self._removed:
                self._removed.add(m.id)
                self.remove(m.id)

    def text(self) -> str:
        return "\n".join(self.lines) + "\n"

    def save(self, path: str) -> None:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(self.text())
