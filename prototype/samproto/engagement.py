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


# Flight with attitude (roadmap R3): mirrors simcore/aircraft.hpp. Game values of the flight model.
HEADING_TAU_S = 1.0
PATH_TAU_S = 1.0
SPEED_TAU_S = 4.0
MIN_THROTTLE = -0.5
AOA_1G_RAD = math.radians(3.0)
MAX_AOA_RAD = math.radians(15.0)
HELI_LEAN_AT_TOP_RAD = math.radians(10.0)
HELI_MAX_LEAN_RAD = math.radians(20.0)
HELI_TOP_MPS = 80.0
HOVER_MPS = 2.0
SHOWN_ROLL_RATE_RAD_S = math.radians(90.0)
MAX_SHOWN_BANK_RAD = math.radians(80.0)
HOLD_SPEED_FRACTION = 0.85  # below this share of the speed told he turns and climbs no harder than sustained


def coordinated_turn_rate(v: float, bank: float) -> float:
    """A coordinated turn: omega = g tan(phi) / v."""
    return G0 * math.tan(bank) / v if v > 1e-6 else 0.0


def load_factor_of_bank(bank: float) -> float:
    return 1.0 / math.cos(bank)


def turn_radius_m(v: float, bank: float) -> float:
    return v * v / (G0 * math.tan(bank))


def g_limited_turn_rate(v: float, max_g: float) -> float:
    if v <= 1e-6 or max_g <= 1.0:
        return 1e9
    return G0 * math.sqrt(max_g * max_g - 1.0) / v


def corner_speed_mps(max_g: float, turn_rate: float) -> float:
    if max_g <= 1.0 or turn_rate <= 0.0:
        return 0.0
    return G0 * math.sqrt(max_g * max_g - 1.0) / turn_rate


def roll_out_bank(left: float, v: float, roll_rate: float) -> float:
    """The most bank he may hold with `left` still to turn: cos(phi) >= exp(-left v p / g)."""
    if roll_rate <= 0.0:
        return math.pi / 2.0
    return math.acos(math.exp(-abs(left) * v * roll_rate / G0))


def _approach(frm: float, to: float, step: float) -> float:
    return min(to, frm + step) if to > frm else max(to, frm - step)


@dataclass
class AircraftDemand:
    """What the command layer tells an aircraft to fly (simcore flights.hpp, roadmap B1): while `active` it steers for
    `point` (or, `by_heading`, holds a heading) instead of its next waypoint, whose place it keeps; a speed, turn rate or
    g limit given (> 0) takes the place of its own for the step. With `capture` the point is passed as a waypoint is: the
    step flies on straight and sets `reached`. Inactive, it follows its waypoints as it always has."""
    active: bool = False
    point: Vec3 = Vec3()
    capture: bool = False
    reached: bool = False
    by_heading: bool = False
    heading_rad: float = 0.0
    path_angle_rad: float = 0.0
    level: bool = True
    speed_mps: float = 0.0
    turn_rate_rad_s: float = 0.0
    max_load_g: float = 0.0


@dataclass
class Aircraft:
    name: str
    position: Vec3
    speed_mps: float = 200.0  # the airspeed it is told to fly
    velocity: Vec3 = Vec3()   # over the ground (the wind included)
    waypoints: list[Vec3] = field(default_factory=list)
    max_turn_rate_rad_s: float = math.radians(6.0)
    rcs_m2: float = 5.0
    capture_radius_m: float = 500.0
    type: str = "Air+FixedWing"
    id: int = 0
    next_wp: int = 0
    alive: bool = True
    # The type's limits (0: not given; with none it flies the old way).
    max_load_g: float = 0.0
    sustained_turn_rate_rad_s: float = 0.0
    roll_rate_rad_s: float = 0.0
    max_speed_mps: float = 0.0
    accel_mps2: float = 0.0
    max_climb_mps: float = 0.0
    # The attitude.
    heading_rad: float = 0.0
    pitch_rad: float = 0.0
    bank_rad: float = 0.0
    path_angle_rad: float = 0.0
    load_g: float = 1.0
    airspeed_mps: float = -1.0
    throttle: float = 0.0
    attitude_set: bool = False
    height_held: bool = False
    demand: AircraftDemand = field(default_factory=AircraftDemand)

    @property
    def rotorcraft(self) -> bool:
        return "Rotorcraft" in self.type

    def has_energy_model(self) -> bool:
        return self.accel_mps2 > 0.0 and self.max_speed_mps > 0.0 and not self.rotorcraft

    def flies_by_bank(self) -> bool:
        return self.roll_rate_rad_s > 0.0 or self.max_load_g > 1.0 or self.has_energy_model()

    def airspeed(self) -> float:
        return self.airspeed_mps if self.airspeed_mps >= 0.0 else self.speed_mps

    def keep_speed_mps(self) -> float:
        """The speed he means to keep in a turn or a climb."""
        if not self.has_energy_model():
            return 0.0
        return min(self.speed_mps, 0.9 * self.max_speed_mps)

    def hold_speed_mps(self) -> float:
        """The speed he won't bleed below."""
        return HOLD_SPEED_FRACTION * self.keep_speed_mps()

    def excess_thrust_mps2(self, v: float) -> float:
        if not self.has_energy_model():
            return 0.0
        r = v / self.max_speed_mps
        return self.accel_mps2 * (1.0 - r * r)

    def turn_rate_limit(self, v: float) -> float:
        w = self.max_turn_rate_rad_s
        if self.max_load_g > 1.0:
            w = min(w, g_limited_turn_rate(v, self.max_load_g))
        keep, hold = self.keep_speed_mps(), self.hold_speed_mps()
        if v < keep and keep > hold:  # eased from the full rate at `keep` to the sustained turn at `hold`
            k = self.turn_drag_k()
            if k > 0.0:
                n2m1 = max(0.0, self.excess_thrust_mps2(v) / k)
                w_sus = G0 * math.sqrt(n2m1) / max(1.0, v)
                if w_sus < w:
                    w = w_sus + (w - w_sus) * max(0.0, (v - hold) / (keep - hold))
        return w

    def turn_drag_k(self) -> float:
        if not self.has_energy_model() or self.sustained_turn_rate_rad_s <= 0.0:
            return 0.0
        v_ref = 0.9 * self.max_speed_mps
        corner = corner_speed_mps(self.max_load_g, self.max_turn_rate_rad_s)
        if corner > 0.0:
            v_ref = min(v_ref, corner)
        x = v_ref * self.sustained_turn_rate_rad_s / G0
        if x * x < 1e-9:
            return 0.0
        r = v_ref / self.max_speed_mps
        return self.accel_mps2 * (1.0 - r * r) / (x * x)

    def waypoint_inside_turn(self, d: Vec3, wp: Vec3) -> bool:
        """A waypoint inside one of the turn circles can't be flown over: the pilot flies on to the next."""
        by_bank = self.flies_by_bank()
        v = self.airspeed() if by_bank else self.speed_mps
        w = self.turn_rate_limit(v) if by_bank else self.max_turn_rate_rad_s
        if w <= 1e-6 or v <= 0:
            return False
        h = math.hypot(d.x, d.y)
        if h <= 1e-9:
            return False
        radius = v / w
        lx, ly = -d.y / h, d.x / h
        for side in (1.0, -1.0):
            cx, cy = self.position.x + side * radius * lx, self.position.y + side * radius * ly
            if math.hypot(wp.x - cx, wp.y - cy) < 0.98 * radius:
                return True
        return False

    def _set_pitch(self, v: float, dvdt: float) -> None:
        if self.rotorcraft:
            top = self.max_speed_mps if self.max_speed_mps > 0.0 else HELI_TOP_MPS
            r = v / top
            lean = math.atan(r * r * math.tan(HELI_LEAN_AT_TOP_RAD) + dvdt / G0)
            self.pitch_rad = -clamp(lean, -HELI_MAX_LEAN_RAD, HELI_MAX_LEAN_RAD)
            return
        ref = max(self.speed_mps, v)
        aoa = AOA_1G_RAD * self.load_g * (ref / v) * (ref / v) if v > 1.0 else MAX_AOA_RAD
        self.pitch_rad = self.path_angle_rad + clamp(aoa, 0.0, MAX_AOA_RAD)

    def _show_attitude(self, d: Vec3, dt: float) -> None:
        """The old way's attitude, read off the step it flew (for the drawing only)."""
        v = self.speed_mps
        omega = 0.0
        if d.horizontal_length() > 1e-6 and v > 0.01:
            heading = azimuth_of(d)
            if self.attitude_set and dt > 0.0:
                omega = math.remainder(heading - self.heading_rad, 2 * math.pi) / dt
            self.heading_rad = heading
            self.path_angle_rad = elevation_of(d)
        lateral_g = v * math.cos(self.path_angle_rad) * omega / G0
        want = clamp(math.atan(lateral_g), -MAX_SHOWN_BANK_RAD, MAX_SHOWN_BANK_RAD)
        self.bank_rad = _approach(self.bank_rad, want, SHOWN_ROLL_RATE_RAD_S * dt) if self.attitude_set else want
        self.load_g = math.sqrt(1.0 + lateral_g * lateral_g)
        self.airspeed_mps = v
        self.throttle = 0.0
        self._set_pitch(v, 0.0)
        self.attitude_set = True

    def _step_by_bank(self, dt: float, wind: Vec3) -> None:
        windy = wind.length_sq() > 0.0
        if self.airspeed_mps < 0.0:
            self.airspeed_mps = self.speed_mps
        if not self.attitude_set:
            air = self.velocity - wind if windy else self.velocity
            if air.horizontal_length() < 1e-6 and self.next_wp < len(self.waypoints):
                air = self.waypoints[self.next_wp] - self.position
            self.heading_rad = azimuth_of(air) if air.horizontal_length() > 1e-9 else 0.0
            self.path_angle_rad = 0.0 if self.height_held or air.length_sq() <= 0.0 else elevation_of(air)
            self.bank_rad = 0.0
            self.attitude_set = True
        v = self.airspeed_mps
        want_heading, want_gamma = self.heading_rad, self.path_angle_rad
        steer = None
        nose = Vec3(math.sin(self.heading_rad), math.cos(self.heading_rad), 0.0)
        dm = self.demand
        if dm.active:
            if dm.capture and ((dm.point - self.position).length() <= self.capture_radius_m or self.waypoint_inside_turn(nose, dm.point)):
                dm.reached = True
            else:
                steer = dm.point
        else:
            if self.next_wp < len(self.waypoints):
                to_wp = self.waypoints[self.next_wp] - self.position
                if to_wp.length() <= self.capture_radius_m or self.waypoint_inside_turn(nose, self.waypoints[self.next_wp]):
                    self.next_wp += 1
            if self.next_wp < len(self.waypoints):
                steer = self.waypoints[self.next_wp]
        if steer is not None:
            to_wp = steer - self.position
            h = to_wp.horizontal_length()
            if h > 1e-6:
                want = Vec3(to_wp.x / h, to_wp.y / h, 0.0)
                vh = v * math.cos(self.path_angle_rad)
                wind_h = Vec3(wind.x, wind.y, 0.0)
                if windy and vh > 0.0:
                    gw = want.dot(wind_h)
                    disc = gw * gw - wind_h.length_sq() + vh * vh
                    if disc > 0.0:
                        want = (want * (gw + math.sqrt(disc)) - wind_h) / vh
                if want.horizontal_length() > 1e-9:
                    want_heading = azimuth_of(want)
            if not self.height_held:
                want_gamma = math.atan2(to_wp.z, h)
        if not self.height_held and self.max_climb_mps > 0.0 and v > self.max_climb_mps:
            want_gamma = min(want_gamma, math.asin(self.max_climb_mps / v))
        if not self.height_held and v < self.hold_speed_mps():  # slow: he climbs only on the thrust the turn leaves
            spare = self.excess_thrust_mps2(v) - self.turn_drag_k() * max(0.0, self.load_g * self.load_g - 1.0)
            want_gamma = min(want_gamma, math.asin(clamp(spare / G0, 0.0, 1.0)))

        w_lim = self.turn_rate_limit(v)
        err = math.remainder(want_heading - self.heading_rad, 2 * math.pi)
        p = self.roll_rate_rad_s
        roll_step = p * dt if p > 0.0 else 1e9
        if v < HOVER_MPS:
            self.bank_rad = _approach(self.bank_rad, 0.0, roll_step)
            omega = (-1.0 if err < 0.0 else 1.0) * min(w_lim, abs(err) / HEADING_TAU_S)
        else:
            bank_lim = math.atan(v * w_lim / G0)
            tau = max(HEADING_TAU_S, bank_lim / p) if p > 0.0 else HEADING_TAU_S
            bank_want = min(bank_lim, math.atan(v * abs(err) / (G0 * tau)), roll_out_bank(err, v, p))
            if err < 0.0:
                bank_want = -bank_want
            # A limit that falls is rolled down to at the roll rate; the g limit's bank never passed.
            g_bank = math.acos(1.0 / self.max_load_g) if self.max_load_g > 1.0 else math.pi / 2.0 - 1e-3
            self.bank_rad = clamp(_approach(self.bank_rad, bank_want, roll_step), -g_bank, g_bank)
            omega = coordinated_turn_rate(v, self.bank_rad)
        if omega * err > 0.0 and abs(omega * dt) > abs(err):
            omega = err / dt
        self.heading_rad = math.fmod(self.heading_rad + omega * dt + 2 * math.pi, 2 * math.pi)

        gamma_rate = 0.0
        if not self.height_held:
            gerr = want_gamma - self.path_angle_rad
            gamma_rate = clamp(gerr / PATH_TAU_S, -w_lim, w_lim)
            if abs(gamma_rate * dt) > abs(gerr):
                gamma_rate = gerr / dt if dt > 0.0 else 0.0
            self.path_angle_rad += gamma_rate * dt
        cg, sg = math.cos(self.path_angle_rad), math.sin(self.path_angle_rad)
        n_lat, n_up = v * cg * omega / G0, cg + v * gamma_rate / G0
        self.load_g = math.sqrt(n_lat * n_lat + n_up * n_up)

        dvdt = 0.0
        if self.has_energy_model():
            accel, r = self.accel_mps2, v / self.max_speed_mps
            turn = self.turn_drag_k() * max(0.0, self.load_g * self.load_g - 1.0)
            climb = G0 * sg
            th = r * r + (turn + climb) / accel + (self.speed_mps - v) / (SPEED_TAU_S * accel)
            self.throttle = clamp(th, MIN_THROTTLE, 1.0)
            dvdt = accel * (self.throttle - r * r) - turn - climb
            v = max(0.0, v + dvdt * dt)
        else:
            v = self.speed_mps
            self.throttle = 0.0
        self.airspeed_mps = v
        air = Vec3(math.sin(self.heading_rad) * cg, math.cos(self.heading_rad) * cg, 0.0 if self.height_held else sg) * v
        self.velocity = air + wind if windy else air
        self.position = self.position + self.velocity * dt
        self._set_pitch(v, dvdt)

    def step(self, dt: float, wind: Vec3 = Vec3()) -> None:
        """One step along the waypoints, or by the command layer's demand (its point or heading, with its speed, turn
        rate or g limit in place of the aircraft's own for the step); `wind` (ENU, the way the air moves) carries it,
        the pilot crabs into it."""
        if not self.alive:
            return
        dm = self.demand
        if not dm.active:
            self._step_along(dt, wind)
            return
        if dm.by_heading:
            far_m = 50000.0
            dm.point = self.position + Vec3(math.sin(dm.heading_rad) * far_m, math.cos(dm.heading_rad) * far_m,
                                            0.0 if dm.level else math.tan(dm.path_angle_rad) * far_m)
        speed, turn, g = self.speed_mps, self.max_turn_rate_rad_s, self.max_load_g
        if dm.speed_mps > 0.0:
            self.speed_mps = dm.speed_mps
        if dm.turn_rate_rad_s > 0.0:
            self.max_turn_rate_rad_s = dm.turn_rate_rad_s
        if dm.max_load_g > 1.0 and self.max_load_g > 1.0:
            self.max_load_g = min(self.max_load_g, dm.max_load_g)
        self._step_along(dt, wind)
        self.speed_mps, self.max_turn_rate_rad_s, self.max_load_g = speed, turn, g

    def _step_along(self, dt: float, wind: Vec3) -> None:
        """One step of either way of flying, to the next waypoint or the demand's point."""
        if self.flies_by_bank():
            self._step_by_bank(dt, wind)
            return
        windy = wind.length_sq() > 0.0
        d = (self.velocity - wind if windy else self.velocity).normalized()
        if d.length_sq() == 0:
            d = Vec3(0, 1, 0)
        dm = self.demand
        if dm.active or self.next_wp < len(self.waypoints):
            p = dm.point if dm.active else self.waypoints[self.next_wp]
            to_wp = p - self.position
            passed = (not dm.active or dm.capture) and (to_wp.length() <= self.capture_radius_m or self.waypoint_inside_turn(d, p))
            if passed:
                if dm.active:
                    dm.reached = True
                else:
                    self.next_wp += 1
            elif to_wp.length_sq() > 0.0:
                want = to_wp.normalized()
                if windy:
                    gw = want.dot(wind)
                    disc = gw * gw - wind.length_sq() + self.speed_mps * self.speed_mps
                    if disc > 0.0 and self.speed_mps > 0.0:
                        want = (want * (gw + math.sqrt(disc)) - wind) / self.speed_mps
                d = turn_toward(d, want.normalized(), self.max_turn_rate_rad_s * dt)
        self.velocity = d * self.speed_mps + (wind if windy else Vec3())
        self.position = self.position + self.velocity * dt
        self._show_attitude(d, dt)


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

    def obj_attitude(self, oid: int, enu: Vec3, roll: float, pitch: float, heading: float, props: str) -> None:
        """With its attitude (an aircraft's, roadmap R3): roll (right wing down > 0), pitch, heading; radians."""
        g = self.frame.enu_to_geodetic(enu)
        line = (f"{oid:x},T={g.lon_deg:.7f}|{g.lat_deg:.7f}|{g.alt_m:.1f}|{math.degrees(roll):.1f}|"
                f"{math.degrees(pitch):.1f}|{math.degrees(heading):.1f}")
        if oid not in self._seen and props:
            line += "," + props
            self._seen.add(oid)
        self.lines.append(line)

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
                props = f"Type={a.type},Name={a.name},Color=Blue"
                if a.attitude_set:
                    self.obj_attitude(a.id, a.position, a.bank_rad, a.pitch_rad, a.heading_rad, props)
                else:
                    self.obj(a.id, a.position, a.velocity, props)
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
