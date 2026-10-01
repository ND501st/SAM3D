"""Play the open level with the hands-free cabin tour and record every view: sim state and a screenshot.

Needs the SAM3D editor open with remote execution on (see tools/ue_remote.py) and the tour switched on (the
director's Cabin Tour, or SAM3D_CABIN_TOUR=<seconds> when the editor was started). Restarts Play, follows the tour,
and at each new view (once it has settled) records the sim time, the view, equipment states, lit lamps and the
track, and takes a screenshot. Stops Play at the end and writes report.md and the debrief next to the screenshots.

    python tools/ue_cabin_check.py                       # until t=270 s (past the practice shoot's launch)
    python tools/ue_cabin_check.py --until 120 --out out/cabin_check
    python tools/ue_cabin_check.py --until 95 --at 70:tov.power=1    # switch the TV camera on at t=70 s

`--at T:control=position` moves a console control at sim time T (as a script would, not judged as the player).
"""
import argparse
import glob
import json
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_remote import Editor, EditorNotFound  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCREENSHOTS = os.path.join(REPO, "unreal", "SAM3D", "Saved", "Screenshots", "WindowsEditor")
SUBSYSTEMS = ["power", "apu_free", "search_antenna", "soc_transmitter", "ssc_transmitter", "tov_camera",
              "auto_tracking", "missile_prep", "launch_armed"]

# Runs in the editor: the PIE world's sim and the player's vehicle, as one JSON line.
STATE = """
import json
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = {"playing": world is not None}
if world:
    sims = [s for s in unreal.ObjectIterator(unreal.SimWorldSubsystem) if s.get_world() == world]
    cars = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimOsaVehicle)
    if sims:
        sim = sims[0]
        out["t"] = round(sim.get_sim_time_seconds(), 1)
        out["finished"] = sim.is_scenario_finished()
        out["tracked"] = sim.get_tracked_target()
        out["states"] = {k: str(sim.get_subsystem_state(k)).split(".")[-1].split(":")[0] for k in %(subsystems)r}
        out["lamps"] = [k for k in %(lamps)r if sim.is_lamp_lit(k)]
    if cars:
        out["touring"] = cars[0].is_touring_cabin()
        out["view"] = cars[0].get_cabin_hover_text()
print(json.dumps(out))
"""


def lamp_ids():
    with open(os.path.join(REPO, "data", "cabins", "osa_ak.json"), encoding="utf-8") as f:
        return [lamp["id"] for lamp in json.load(f)["lamps"]]


def query(ue, lamps):
    ok, out = ue.run(STATE % {"subsystems": SUBSYSTEMS, "lamps": lamps})
    if not ok:
        raise RuntimeError(out)
    return json.loads(out.strip().splitlines()[-1])


def shoot(ue, dest):
    """Screenshot the Play viewport and move it to dest. False if it didn't appear."""
    before = set(glob.glob(os.path.join(SCREENSHOTS, "*.png")))
    ue.run("unreal.SystemLibrary.execute_console_command("
           "unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'HighResShot 1')")
    for _ in range(40):  # written a frame or two later
        new = set(glob.glob(os.path.join(SCREENSHOTS, "*.png"))) - before
        if new:
            time.sleep(0.3)
            shutil.move(new.pop(), dest)
            return True
        time.sleep(0.25)
    return False


def slug(text):
    keep = "".join(c if c.isalnum() else "_" for c in text)
    return "_".join(part for part in keep.split("_") if part)[:60]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--until", type=float, default=270.0, help="stop at this sim time (s)")
    parser.add_argument("--settle", type=float, default=2.0, help="seconds to wait into each view before sampling")
    parser.add_argument("--at", action="append", default=[], metavar="T:CONTROL=POS",
                        help="move a control at a sim time, e.g. 70:tov.power=1 (repeatable)")
    parser.add_argument("--out", default=os.path.join(REPO, "out", "cabin_check", time.strftime("%Y%m%d-%H%M%S")))
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")  # view names are Cyrillic (βс, ДЦ)
    os.makedirs(args.out, exist_ok=True)
    lamps = lamp_ids()
    rows = []
    actions = []
    for item in args.at:
        when, _, setting = item.partition(":")
        control, _, position = setting.partition("=")
        actions.append([float(when), control, int(position), False])
    try:
        with Editor() as ue:
            # The editor world is None while Play runs; the game world then.
            level = ue.run("ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)\n"
                           "w = ues.get_editor_world() or ues.get_game_world()\n"
                           "print(w.get_path_name() if w else '?')")[1].strip()
            # A fresh run: stop any Play in progress, then start one.
            ue.run("les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
                   "if les.is_in_play_in_editor(): les.editor_request_end_play()")
            time.sleep(3)
            ue.run("unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()")
            state = {}
            for _ in range(60):
                time.sleep(1)
                state = query(ue, lamps)
                if state.get("touring") and state.get("view", "").startswith("Cabin tour"):
                    break
            else:
                print("The tour didn't start. Is Cabin Tour on (director, or SAM3D_CABIN_TOUR)?", file=sys.stderr)
                return 1
            print(f"Playing {level}; recording to {args.out}")
            last_view = None
            while True:
                state = query(ue, lamps)
                if state.get("t", 0) >= args.until or state.get("finished") or not state.get("playing"):
                    break
                for action in actions:
                    if not action[3] and state.get("t", 0) >= action[0]:
                        action[3] = True
                        ok, out = ue.run(
                            "world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\n"
                            "sim = [s for s in unreal.ObjectIterator(unreal.SimWorldSubsystem) "
                            "if s.get_world() == world][0]\n"
                            f"print(sim.set_control_position({action[1]!r}, {action[2]}))")
                        print(f"t={state.get('t'):6.1f}  set {action[1]} = {action[2]}: {out.strip()}")
                view = state.get("view", "")
                if view != last_view:
                    last_view = view
                    time.sleep(args.settle)
                    state = query(ue, lamps)
                    name = f"{len(rows) + 1:02d}_t{int(state.get('t', 0)):03d}_{slug(view.split(':', 1)[-1])}.png"
                    shot = shoot(ue, os.path.join(args.out, name))
                    state["screenshot"] = name if shot else ""
                    rows.append(state)
                    print(f"t={state.get('t'):6.1f}  {view:55s} track={state.get('tracked') or '-':4s} "
                          f"lamps={','.join(state.get('lamps', [])) or '-'}")
                time.sleep(0.5)
            ok, out = ue.run("world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\n"
                             "sims = [s for s in unreal.ObjectIterator(unreal.SimWorldSubsystem) "
                             "if world and s.get_world() == world]\n"
                             "print(sims[0].get_debrief_text() if sims else '')")
            debrief = out if ok else ""
            ue.run("unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()")
    except EditorNotFound as e:
        print(e, file=sys.stderr)
        return 2

    with open(os.path.join(args.out, "debrief.txt"), "w", encoding="utf-8") as f:
        f.write(debrief)
    with open(os.path.join(args.out, "states.json"), "w", encoding="utf-8") as f:
        json.dump(rows, f, ensure_ascii=False, indent=1)
    with open(os.path.join(args.out, "report.md"), "w", encoding="utf-8") as f:
        f.write(f"# Cabin check: {level}\n\n| t (s) | View | Track | Lit lamps | Equipment ready | Screenshot |\n"
                "|---|---|---|---|---|---|\n")
        for r in rows:
            ready = [k for k, v in r.get("states", {}).items() if v == "READY"]
            f.write(f"| {r.get('t')} | {r.get('view', '').split(': ', 1)[-1]} | {r.get('tracked') or '-'} | "
                    f"{', '.join(r.get('lamps', [])) or '-'} | {', '.join(ready) or '-'} | {r.get('screenshot')} |\n")
    print(f"{len(rows)} views recorded; report: {os.path.join(args.out, 'report.md')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
