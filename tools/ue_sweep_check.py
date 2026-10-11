"""Check the range indicator's missile sweeps (Р I / Р II) in the open editor, with the camera held on the indicator.

Needs the SAM3D editor open with remote execution on (see tools/ue_remote.py). Starts Play, runs the sim fast up to
just before the practice scenario's launch, holds the cabin camera on the range indicator
(ASimOsaVehicle::SetCabinViewOverride), and takes screenshots: at ЦЕЛЬ before the launch, then, once a missile is
away, at ЦЕЛЬ, Р I (and Р II, which has no missile with a single shot) a second apart. Stops Play at the end.

    python tools/ue_sweep_check.py                      # writes out/sweep_check/*.png
    python tools/ue_sweep_check.py --out out/sweep --prelaunch 440

The camera's place comes from the cabin file: the display's position on its panel, looked at from 0.35 m in front.
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
DISTANCE_M = 0.35
FOV_DEG = 40.0


def camera_for(display_id):
    """Body-frame camera location (m) and (pitch, yaw) in degrees that look straight at a display."""
    with open(os.path.join(REPO, "data", "cabins", "osa_ak.json"), encoding="utf-8") as f:
        cabin = json.load(f)
    display = next(d for d in cabin["displays"] if d["id"] == display_id)
    panel = next(p for p in cabin["panels"] if p["id"] == display["panel"])
    facing, tilt = math.radians(panel["facing_deg"]), math.radians(panel["tilt_deg"])
    front = (math.sin(facing), math.cos(facing), 0.0)       # the way the panel's front looks
    right = (-math.cos(facing), math.sin(facing), 0.0)      # the operator's right, facing the panel
    normal = (front[0] * math.cos(tilt), front[1] * math.cos(tilt), math.sin(tilt))
    up = (-front[0] * math.sin(tilt), -front[1] * math.sin(tilt), math.cos(tilt))
    u, v = display["at_m"]
    at = [panel["centre_m"][i] + u * right[i] + v * up[i] for i in range(3)]
    eye = [at[i] + DISTANCE_M * normal[i] for i in range(3)]
    look = [-n for n in normal]
    yaw = math.degrees(math.atan2(look[0], look[1]))
    pitch = math.degrees(math.asin(look[2]))
    return eye, pitch, yaw


WORLD = ("world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\n"
         "sim = [s for s in unreal.ObjectIterator(unreal.SimWorldSubsystem) if s.get_world() == world][0]\n")


def console(ue, command):
    ue.run(f"unreal.SystemLibrary.execute_console_command("
           f"unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), {command!r})")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default=os.path.join(REPO, "out", "sweep_check"))
    parser.add_argument("--prelaunch", type=float, default=440.0, help="run fast up to this sim time (s)")
    parser.add_argument("--speed", type=float, default=16.0, help="sam3d.TimeScale for the fast part")
    parser.add_argument("--timeout", type=float, default=900.0, help="give up after this many real seconds")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    lamps = lamp_ids()
    eye, pitch, yaw = camera_for("ssc_range")
    try:
        with Editor() as ue:
            ue.run("les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
                   "if les.is_in_play_in_editor(): les.editor_request_end_play()")
            time.sleep(3)
            ue.run("unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()")
            for _ in range(90):
                time.sleep(1)
                if query(ue, lamps).get("playing"):
                    break
            start = time.time()
            console(ue, f"sam3d.TimeScale {args.speed}")
            while query(ue, lamps).get("t", 0) < args.prelaunch:
                if time.time() - start > args.timeout:
                    print("Timed out before the pre-launch time.", file=sys.stderr)
                    return 1
                time.sleep(1)
            console(ue, "sam3d.TimeScale 1")
            ok, out = ue.run(
                "world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\n"
                "car = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimOsaVehicle)[0]\n"
                f"car.set_cabin_view_override(unreal.Vector({eye[0]}, {eye[1]}, {eye[2]}), "
                f"unreal.Rotator(roll=0, pitch={pitch}, yaw={yaw}), {FOV_DEG})\nprint('camera held')")
            print(out.strip())
            time.sleep(1)
            shots = []

            def snap(label):
                state = query(ue, lamps)
                name = f"{len(shots) + 1:02d}_t{int(state.get('t', 0)):03d}_{label}.png"
                if shoot(ue, os.path.join(args.out, name)):
                    shots.append(name)
                    print(f"t={state.get('t'):6.1f}  {label:12s} lamps={','.join(l for l in state.get('lamps', []) if l.startswith('ch')) or '-'}  {name}")
                return state

            snap("before_target")
            launched = False
            while not launched:
                if time.time() - start > args.timeout:
                    print("No launch seen.", file=sys.stderr)
                    return 1
                state = query(ue, lamps)
                launched = "ch1_skhod" in state.get("lamps", [])
                time.sleep(0.2)
            snap("flight_target")
            ue.run(WORLD + "sim.set_control_position('ssc.target_select', 0)")
            time.sleep(1.5)
            snap("flight_R1")
            time.sleep(2.0)
            snap("flight_R1_later")
            ue.run(WORLD + "sim.set_control_position('ssc.target_select', 2)")
            time.sleep(1.0)
            snap("flight_R2")
            ue.run("unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()")
    except EditorNotFound as e:
        print(e, file=sys.stderr)
        return 2
    print(f"{len(shots)} screenshots in {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
