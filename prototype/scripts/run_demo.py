"""Python version of simcore/examples/engagement_demo.cpp.

    python prototype/scripts/run_demo.py [out.acmi]

All parameters are GENERIC PLACEHOLDERS — not real system data.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from samproto import (AcmiWriter, Aircraft, GeoPoint, LocalFrame, MissileParams,  # noqa: E402
                      RadarParams, Vec3, World, evaluate_detection, flat)
from samproto.vec import G0  # noqa: E402


def main(out_path: str = "engagement_demo_py.acmi") -> int:
    frame = LocalFrame(GeoPoint.from_degrees(45.0, 25.0, 300.0))
    world = World(frame)
    ground = flat(300.0)
    radar = RadarParams(100e3, 35.0, 0.03, 1e6, 5.0, 6.0, max_instrumented_range_m=45000.0)
    missile = MissileParams(150.0, 90.0, 30000.0, 2.5, 4000.0, 10.0, 0.025, 0.35, 25 * G0)
    site = Vec3(0.0, 0.0, 5.0)
    tgt_id = world.add_aircraft(Aircraft(
        "Bandit 1", Vec3(-6000.0, 40000.0, 3000.0), speed_mps=250.0,
        max_turn_rate_rad_s=math.radians(4.0),
        waypoints=[Vec3(-1500.0, 15000.0, 2500.0), Vec3(2000.0, -30000.0, 2500.0)]))

    acmi = AcmiWriter(frame)
    acmi.frame_time(0.0)
    acmi.obj(1000, site, Vec3(), "Type=Ground+AntiAircraft,Name=SAM site,Color=Red")

    dt, detected, launched, tail = 0.01, False, False, -1
    for step in range(20000):
        tgt = world.find(tgt_id)
        if tail == 0 or tgt is None:
            break
        if tgt.alive:
            det = evaluate_detection(radar, frame, ground, site, Vec3(), tgt.position, tgt.rcs_m2)
            if det.detected and not detected:
                detected = True
                print(f"t={world.time:6.2f}s  RADAR DETECTION  range {det.range_m / 1000:.1f} km  "
                      f"az {math.degrees(det.azimuth_rad):.1f} deg  SNR {det.snr_db:.1f} dB")
            if detected and not launched and det.detected and det.range_m <= 12000.0:
                launched = True
                world.launch(missile, site, det.azimuth_rad, det.elevation_rad + math.radians(10.0), tgt_id)
        world.step(dt)
        if step % 10 == 0:
            acmi.write_world(world)
        for e in world.drain_events():
            print(f"t={e.time_s:6.2f}s  {e.kind:<22} missile {e.missile_id}  target {e.aircraft_id}  "
                  f"miss {e.miss_m:.1f} m")
        if tail > 0:
            tail -= 1
        elif launched and tail < 0 and not world.missiles[-1].active:
            tail = 300
    acmi.write_world(world)
    acmi.save(out_path)
    print(f"\nWrote {out_path} (open it in Tacview for a 3D replay)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(*sys.argv[1:2]))
