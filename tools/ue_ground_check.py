"""Roadmap R5's "nothing under the map", in the game itself: play the dynamic campaign testbed for an hour of sim time and
every mission on a map ("terrain": {"type": "map"}) for ten minutes, each in its own game window, with the game's ground
check on (-SimGroundCheck=<s>: USimWorldSubsystem::StartGroundCheck). Once a second it judges every shown entity actor
against the drawn ground (USimWorldSubsystem::GroundAt) and logs "Below ground:" or "Floats:"; at the end a summary, and
the game quits. This script reads each run's log and fails on any such line. simcore's own check is ground_check
(simcore/examples/ground_check.cpp); this one sees what Unreal draws.

    python tools/ue_ground_check.py                        # the testbed for 3600 s, every map mission for 600 s
    python tools/ue_ground_check.py --only dynamic_campaign_test --seconds 300
    python tools/ue_ground_check.py --scale 8              # sim time 8x real time (sam3d.TimeScale)

Needs the game built (tools/play.ps1 -Build) and UE_ROOT (default D:\\UE_5.8). Claude cannot run the editor: a person runs it.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.join(REPO, "unreal", "SAM3D", "SAM3D.uproject")
DEFAULT_LEVEL = "/Game/Maps/KesmarkRange"
TESTBED = "dynamic_campaign_test"


def map_missions():
    """(mission id, level) of every mission whose scenario is on a map, the testbed first."""
    with open(os.path.join(REPO, "data", "scenarios", "missions.json"), encoding="utf-8") as f:
        catalog = json.load(f)
    out = []
    for group in catalog["groups"]:
        for m in group["missions"]:
            if m.get("link"):
                continue
            with open(os.path.join(REPO, "data", m["file"]), encoding="utf-8") as f:
                terrain = json.load(f).get("terrain", {})
            if isinstance(terrain, dict) and terrain.get("type") == "map":
                mission = os.path.splitext(os.path.basename(m["file"]))[0]
                out.append((mission, m.get("level") or DEFAULT_LEVEL))
    out.sort(key=lambda m: m[0] != TESTBED)
    return out


def run(editor, mission, level, seconds, scale, log_dir):
    """One game run; returns (listed lines, summary line or None)."""
    log = os.path.join(log_dir, f"{mission}.log")
    if os.path.exists(log):
        os.remove(log)
    args = [editor, PROJECT, f"{level}?SimMission={mission}", "-game", "-windowed", "-ResX=960", "-ResY=540",
            f"-SimGroundCheck={seconds:.0f}", f'-ExecCmds=sam3d.TimeScale {scale:g}', f"-abslog={log}", "-unattended",
            "-nosplash"]
    print(f"==> {mission}: {seconds:.0f} s of sim time at {scale:g}x on {level}", flush=True)
    limit_s = 600.0 + 3.0 * seconds / scale  # loading, then the run with room for a slow machine
    started = time.time()
    proc = subprocess.Popen(args)
    try:
        proc.wait(timeout=limit_s)
    except subprocess.TimeoutExpired:
        print(f"    no summary after {limit_s:.0f} s: stopping the game it started", flush=True)
        proc.terminate()
        proc.wait(timeout=60)
    listed, summary = [], None
    if os.path.exists(log):
        with open(log, encoding="utf-8", errors="replace") as f:
            for line in f:
                if re.search(r"(Below ground|Floats): ", line):
                    listed.append(line.strip())
                elif "Ground check: " in line and ("PASS" in line or "FAIL" in line):
                    summary = line.strip()
    print(f"    {summary or 'no summary in the log'} ({time.time() - started:.0f} s wall)", flush=True)
    for line in listed[:20]:
        print(f"    {line}")
    return listed, summary


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--seconds", type=float, default=3600.0, help="the testbed's sim seconds (missions: at most 600)")
    parser.add_argument("--scale", type=float, default=8.0, help="sam3d.TimeScale for the runs")
    parser.add_argument("--only", action="append", default=[], help="a mission id to run (repeatable)")
    parser.add_argument("--out", default=os.path.join(REPO, "out", "ue_ground_check"))
    args = parser.parse_args()
    engine = os.environ.get("UE_ROOT", r"D:\UE_5.8")
    editor = os.path.join(engine, "Engine", "Binaries", "Win64", "UnrealEditor.exe")
    if not os.path.exists(editor):
        sys.exit(f"Unreal Editor not found at {editor} (set UE_ROOT to the engine folder)")
    os.makedirs(args.out, exist_ok=True)
    failures = 0
    missions = [m for m in map_missions() if not args.only or m[0] in args.only]
    for mission, level in missions:
        seconds = args.seconds if mission == TESTBED else min(args.seconds, 600.0)
        listed, summary = run(editor, mission, level, seconds, args.scale, args.out)
        if listed or summary is None or "FAIL" in summary:
            failures += 1
    print(f"{'PASS' if failures == 0 else 'FAIL'}: {failures} of {len(missions)} runs with something off the ground "
          f"(or no summary); logs in {args.out}")
    sys.exit(0 if failures == 0 else 1)


if __name__ == "__main__":
    main()
