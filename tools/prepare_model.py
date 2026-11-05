"""Prepares a model pick for Unreal: Blender joins its parts, fits the part's frame and bakes the pivot
(tools/blender/normalize_model.py), writing art/thirdparty/models/<folder>/normalized/<id>.glb. Roadmap T5-T8,
docs/scenery/T5_T8_PLAN.md.

    python tools/prepare_model.py crate_test

Reads the pick from data/scenery/model_picks.json (its `folder`, `file`, `frame`, optional `yaw_deg`, `up`, `thin_tris` (a vegetation model thinned to that many triangles) and `keep_aspect`).
Blender is found at BLENDER (the environment) or in Program Files. Then tools/ue_import_model.py imports the file.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PICKS = ROOT / "data" / "scenery" / "model_picks.json"
MODELS = ROOT / "art" / "thirdparty" / "models"


def blender():
    if os.environ.get("BLENDER"):
        return os.environ["BLENDER"]
    base = Path("C:/Program Files/Blender Foundation")
    found = sorted(base.glob("Blender */blender.exe")) if base.exists() else []
    if not found:
        raise SystemExit("Blender not found: set BLENDER to its blender.exe")
    return str(found[-1])


def main(argv):
    if len(argv) != 2:
        raise SystemExit(__doc__)
    picks = [p for p in json.loads(PICKS.read_text(encoding="utf-8"))["picks"] if p["id"] == argv[1]]
    if not picks:
        raise SystemExit(f"{argv[1]!r} is not a pick in {PICKS}")
    pick = picks[0]
    source = ROOT / pick["root"] if pick.get("root") else MODELS / pick["folder"]  # our own work (art/clutter) or a fetched model
    out = source / "normalized"
    out.mkdir(parents=True, exist_ok=True)
    cmd = [blender(), "--background", "--python", str(ROOT / "tools" / "blender" / "normalize_model.py"), "--",
           str(source / pick["file"]), str(out / (pick["id"] + ".glb")), pick["frame"]]
    if pick.get("yaw_deg"):
        cmd += ["--yaw", str(pick["yaw_deg"])]
    if pick.get("thin_tris"):
        cmd += ["--thin", str(pick["thin_tris"])]
    if pick.get("up"):
        cmd += ["--up", pick["up"]]
    if pick.get("keep_aspect"):
        cmd.append("--keep-aspect")
    done = subprocess.run(cmd, capture_output=True, text=True, check=False)
    line = [l for l in done.stdout.splitlines() if l.startswith("NORMALIZED")]
    if done.returncode != 0 or not line:
        print(done.stdout[-1500:], done.stderr[-1500:])
        raise SystemExit("Blender did not normalise the model")
    for l in done.stdout.splitlines():
        if l.startswith("THINNED"):
            print(l)
    print(line[0])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
