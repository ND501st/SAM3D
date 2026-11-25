"""Check the register of third-party art (data/scenery/assets.json, docs/scenery/ASSETS.md): no asset that may not be
redistributed is in git, and git ignores where each one lives.

    python tools/check_assets.py          # lists the problems; exit code 1 if there are any

The register's own rules (licence, author, paths) are simcore's (simcore/scenery/asset_register.hpp, tests in
test_scenery.cpp); this checks the repository against them:
- nothing under the third-party folders (unreal/<project>/Content/ThirdParty/, art/thirdparty/) is tracked;
- every asset that may not be redistributed lives under /Game/ThirdParty/, and its file there is ignored by git;
- every asset's licence is one the register knows, and a CC-BY one names its author;
- "models" names basic models (data/scenery/basic_models.json) and "munitions" munitions (data/scenery/munitions.json)
  that exist, each drawn by a registered asset (or null).
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECT_CONTENT = "unreal/SAM3D/Content"
THIRD_PARTY_DIRS = ("unreal/SAM3D/Content/ThirdParty/", "art/thirdparty/")
LICENCES = ("CC0-1.0", "CC-BY-4.0", "Fab-Standard", "Epic-Content-Only", "Own")


def content_file(unreal_path):
    """The .uasset an Unreal object path names, relative to the repository: /Game/A/B.B is unreal/SAM3D/Content/A/B.uasset."""
    if not unreal_path.startswith("/Game/"):
        return None
    package = unreal_path[len("/Game/"):].split(".", 1)[0]
    return f"{PROJECT_CONTENT}/{package}.uasset"


def problems(register, tracked, is_ignored, libraries=None):
    """Every problem, given the register (parsed JSON), the tracked files (repository-relative, forward slashes), a
    function telling whether git ignores a path and, optionally, the libraries' ids ({"models": set, "munitions": set})."""
    out = []
    ids = {a.get("id") for a in register.get("assets", [])}
    for kind, library in (("models", "basic_models.json"), ("munitions", "munitions.json")):
        for key, asset in register.get(kind, {}).items():
            if libraries is not None and key not in libraries[kind]:
                out.append(f"{kind}: {key} is not in data/scenery/{library}")
            if asset is not None and asset not in ids:
                out.append(f"{kind}: {key} names {asset}, which is not registered")
    for f in tracked:
        lower = f.lower()
        if any(lower.startswith(d.lower()) for d in THIRD_PARTY_DIRS):
            out.append(f"{f}: third-party art is tracked by git (it is fetched per machine: docs/scenery/ASSETS.md)")
    for a in register.get("assets", []):
        aid = a.get("id", "?")
        licence = a.get("licence")
        if licence not in LICENCES:
            out.append(f"{aid}: licence {licence!r} is not one of {', '.join(LICENCES)}")
        if licence == "CC-BY-4.0" and not a.get("author"):
            out.append(f"{aid}: CC-BY needs its author, for the credits")
        if a.get("redistributable", False):
            continue
        path = a.get("unreal_path", "")
        if not path.startswith("/Game/ThirdParty/"):
            out.append(f"{aid}: may not be redistributed but lives at {path}, not under /Game/ThirdParty/")
            continue
        f = content_file(path)
        if f in tracked:
            out.append(f"{aid}: {f} is tracked by git")
        if not is_ignored(f):
            out.append(f"{aid}: {f} is not ignored by git (.gitignore: unreal/**/Content/ThirdParty/)")
    return out


def git_tracked():
    r = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True)
    return set(r.stdout.splitlines())


def git_ignored(path):
    r = subprocess.run(["git", "check-ignore", "-q", "--no-index", path], cwd=ROOT, capture_output=True, text=True)
    return r.returncode == 0


def library_ids():
    """The ids of the basic models and the munitions the register may name."""
    scenery = ROOT / "data" / "scenery"
    models = json.loads((scenery / "basic_models.json").read_text(encoding="utf-8"))["models"]
    munitions = json.loads((scenery / "munitions.json").read_text(encoding="utf-8"))["munitions"]
    return {"models": {m["id"] for m in models}, "munitions": {m["id"] for m in munitions}}


def main(argv=None):
    register = json.loads((ROOT / "data" / "scenery" / "assets.json").read_text(encoding="utf-8"))
    found = problems(register, git_tracked(), git_ignored, library_ids())
    # The folders themselves must be ignored even while the register is empty.
    for d in THIRD_PARTY_DIRS:
        if not git_ignored(d + "probe.uasset"):
            found.append(f"{d}: not ignored by git")
    for p in found:
        print(p)
    print(f"{len(register.get('assets', []))} registered asset(s), {len(found)} problem(s)")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
