"""Photograph one cabin panel in the open editor, with the camera held in front of it.

Needs the SAM3D editor open with remote execution on (see tools/ue_remote.py). Starts Play, waits a few seconds of sim
time, holds the cabin camera 0.5 m in front of the panel (ASimOsaVehicle::SetCabinViewOverride) and takes one
screenshot per panel. Stops Play at the end.

    python tools/ue_panel_shot.py radio_i_panel radio_ii_panel commander_panel
    python tools/ue_panel_shot.py --out out/panels --distance 0.45 --fov 55 radio_i_panel
    python tools/ue_panel_shot.py --set rad1.preset=2 --set rad.listen=1 --wait 8 radio_i_panel

--set moves a control (as a script would) before the shot; --wait is the sim seconds to let pass after the sets.
"""
import argparse
import json
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_cabin_check import lamp_ids, query, shoot  # noqa: E402
from ue_remote import Editor, EditorNotFound  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def camera_for_panel(panel_id, distance_m):
    """Body-frame camera location (m) and (pitch, yaw) in degrees looking straight at the middle of a panel."""
    with open(os.path.join(REPO, "data", "cabins", "osa_ak.json"), encoding="utf-8") as f:
        cabin = json.load(f)
    panel = next(p for p in cabin["panels"] if p["id"] == panel_id)
    facing, tilt = math.radians(panel["facing_deg"]), math.radians(panel["tilt_deg"])
    front = (math.sin(facing), math.cos(facing))
    normal = (front[0] * math.cos(tilt), front[1] * math.cos(tilt), math.sin(tilt))
    eye = [panel["centre_m"][i] + distance_m * normal[i] for i in range(3)]
    look = [-n for n in normal]
    return eye, math.degrees(math.asin(look[2])), math.degrees(math.atan2(look[0], look[1]))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("panels", nargs="+")
    parser.add_argument("--out", default=os.path.join(REPO, "out", "panel_shots"))
    parser.add_argument("--distance", type=float, default=0.5)
    parser.add_argument("--fov", type=float, default=60.0)
    parser.add_argument("--set", action="append", default=[], metavar="CONTROL=POS")
    parser.add_argument("--wait", type=float, default=3.0, help="sim seconds to pass after the sets")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    lamps = lamp_ids()
    try:
        with Editor() as ue:
            ue.run("les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
                   "if les.is_in_play_in_editor(): les.editor_request_end_play()")
            time.sleep(3)
            ue.run("unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()")
            for _ in range(120):
                time.sleep(1)
                if query(ue, lamps).get("playing"):
                    break
            time.sleep(2)
            world = ("world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\n"
                     "sim = [s for s in unreal.ObjectIterator(unreal.SimWorldSubsystem) if s.get_world() == world][0]\n")
            for item in args.set:
                control, _, pos = item.partition("=")
                ok, out = ue.run(world + f"print(sim.set_control_position({control!r}, {int(pos)}))")
                print(f"set {control} = {pos}: {out.strip()}")
            t0 = query(ue, lamps).get("t", 0)
            while query(ue, lamps).get("t", 0) < t0 + args.wait:
                time.sleep(0.5)
            for panel in args.panels:
                eye, pitch, yaw = camera_for_panel(panel, args.distance)
                ok, out = ue.run(
                    "world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\n"
                    "car = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimOsaVehicle)[0]\n"
                    f"car.set_cabin_view_override(unreal.Vector({eye[0]}, {eye[1]}, {eye[2]}), "
                    f"unreal.Rotator(roll=0, pitch={pitch}, yaw={yaw}), {args.fov})")
                time.sleep(1.0)
                dest = os.path.join(args.out, f"{panel}.png")
                print(("wrote " + dest) if shoot(ue, dest) else f"no screenshot for {panel}")
            ue.run("unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()")
    except EditorNotFound as e:
        print(e, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
