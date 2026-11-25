"""The plot lab (roadmap L1, docs/scenery/TERRAIN_ROADMAP.md part A): render the test plots of data/scenery/plots.json from
their three cameras and set each beside its reference photographs and its last approved render.

    python tools/plot_lab.py run --label a1                         # every plot, then the page
    python tools/plot_lab.py run --label a2 --plots p01_ploughland   # one plot (or several, comma-separated)
    python tools/plot_lab.py page --label a2                        # only (re)build the page from out/plots/a2/
    python tools/plot_lab.py approve --label a2 --plots p01_ploughland   # the owner approved: these become the ground truth
    python tools/plot_lab.py run --label s1 --season summer          # in early July, to compare like with like with the look target

Each run writes out/plots/<label>/: <plot>_eye.png, <plot>_low.png, <plot>_high.png, their .cost.json, a
<plot>_<camera>.missing.txt for a camera the map cannot give, and index.html. The approved renders live in
out/plots/approved/ and are shown beside every later run, with the time each run took. The photographs are links, as in
tools/terrain_views.py (nothing downloaded or copied). The page opens with the look target (data/scenery/look_target.json,
docs/scenery/LOOK_TARGET.md): its pictures from the git-ignored reference/look/ where this PC has them (linked, never
copied) and its traits K1-K14 as the checklist every pass is judged by.
"""
import argparse
import html
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import terrain_views as tv  # noqa: E402  (the photographs' thumbnails, the costs, the game's launch)

ROOT = tv.ROOT
PLOTS = ROOT / "data" / "scenery" / "plots.json"
OUT = ROOT / "out" / "plots"
APPROVED = OUT / "approved"
LOOK = ROOT / "data" / "scenery" / "look_target.json"
CAMERAS = (("eye", "eye, 1.7 m"), ("low", "low, 25 m"), ("high", "high, 150 m"))
# The plot lab's scenarios (data/scenarios/lab/): the Kesmark range under fair cumulus, the look target's weather, on
# 15 October at 13:00 or on 5 July at 15:30. Plots anchored on an October field state may find none in July.
SEASONS = {"october": "scenarios/lab/plot_lab_october.json", "summer": "scenarios/lab/plot_lab_summer.json"}


def look_strip(look, folder, root=ROOT):
    """The look target's section: its pictures (those on this PC, by a relative link into reference/look/) and its traits."""
    if not look:
        return ""
    esc = html.escape
    pics = []
    base = root / look["folder"]
    for p in look.get("pictures", []):
        f = base / p["file"]
        if not f.exists():
            continue
        rel = os.path.relpath(f, folder).replace(os.sep, "/")
        pics.append(f'<figure><a href="{esc(rel)}"><img src="{esc(rel)}" alt="{esc(p["id"])}"></a>'
                    f'<figcaption>{esc(p["id"])}: {esc(p["shows"])}</figcaption></figure>')
    if not pics:
        pics.append(f'<p class="none">The look target\'s pictures are not on this PC ({esc(look["folder"])}/).</p>')
    traits = "".join(f'<li><b>{esc(t["id"])} {esc(t["name"])}</b>: {esc(t["target"])}</li>' for t in look.get("traits", []))
    return (f'<section class="look"><h2>The look target</h2><p class="must">Judge every render against these too '
            f'(docs/scenery/LOOK_TARGET.md): structure, density, light and value; hue by season.</p>'
            f'<div class="refs">{"".join(pics)}</div><ul class="traits">{traits}</ul></section>')


def plot_ids(arg, plots):
    """The plots a command is for: the comma-separated ids given (each must exist), or all of them."""
    known = [p["id"] for p in plots]
    if not arg:
        return known
    ids = [s.strip() for s in arg.split(",") if s.strip()]
    for i in ids:
        if i not in known:
            raise SystemExit(f"no plot {i!r} in {PLOTS.name}: {', '.join(known)}")
    return ids


def build_page(plots, photos, folder, ids, label, seconds=None, look=None):
    """The page's HTML: per plot, its three cameras beside the approved ones, its costs and its photographs' links."""
    esc = html.escape
    costs = tv.read_costs(folder)
    rows = []
    for p in plots:
        if p["id"] not in ids:
            continue
        cells = []
        for cam, title in CAMERAS:
            name = f"{p['id']}_{cam}"
            now = folder / f"{name}.png"
            ok = APPROVED / f"{name}.png"
            reason = folder / f"{name}.missing.txt"
            pic = f'<img src="{esc(now.name)}" alt="{esc(name)}">' if now.exists() else \
                f'<p class="missing">Not rendered{": " + esc(reason.read_text(encoding="utf-8").strip()) if reason.exists() else ""}.</p>'
            was = f'<img src="{esc(os.path.relpath(ok, folder).replace(os.sep, "/"))}" alt="approved {esc(name)}">' if ok.exists() else \
                '<p class="none">No approved render yet.</p>'
            cost = costs.get(name)
            cost_line = f'<small>GPU {cost["gpu_ms"]:.2f} ms</small>' if cost and "gpu_ms" in cost else ""
            cells.append(f'<div class="cam"><h3>{esc(title)}</h3><div class="pair"><figure>{pic}<figcaption>{esc(label)} {cost_line}</figcaption></figure>'
                         f'<figure>{was}<figcaption>approved</figcaption></figure></div></div>')
        refs = []
        for pid in p.get("photos", []):
            ph = photos.get(pid)
            if ph is None:
                continue
            thumb = tv.thumbnail_url(ph["page_url"])
            img = f'<img src="{esc(thumb)}" alt="{esc(ph["title"])}" loading="lazy">' if thumb else ""
            refs.append(f'<figure><a href="{esc(ph["page_url"])}">{img}<figcaption>{esc(pid)} {esc(ph["title"])}</figcaption></a>'
                        f'<small>{esc(ph["author"])}, {esc(ph["licence"])}</small></figure>')
        rows.append(f'<section><h2>{esc(p["id"])}: {esc(p.get("name", ""))}</h2><p class="must">Must show: {esc(p.get("must_see", ""))}</p>'
                    f'{"".join(cells)}<div class="refs">{"".join(refs)}</div></section>')
    took = f"<p>The run took {seconds:.0f} s.</p>" if seconds else ""
    return ("<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            f"<title>Plot lab {esc(label)}</title><style>"
            "body{font:14px system-ui;margin:16px;background:#fafafa;color:#222}img{max-width:100%}"
            "section{margin:0 0 32px;padding-bottom:16px;border-bottom:1px solid #ccc}.cam h3{margin:12px 0 4px;font-size:14px}"
            ".pair{display:flex;gap:8px}.pair figure{margin:0;flex:1}.missing{color:#a40}.none{color:#888}.must{color:#555}"
            ".refs{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}.refs figure{margin:0;width:200px}"
            ".refs img{width:200px;height:130px;object-fit:cover}"
            ".look .refs figure{width:300px}.look .refs img{width:300px;height:169px}.traits{columns:2;font-size:13px}"
            f"</style><h1>Plot lab: {esc(label)}</h1>{took}" + look_strip(look, folder) + "".join(rows))


FIXED_EXPOSURE_EV = 0.0  # the plot lab's fixed exposure (A.3.20); None: the engine's eye adaptation, as before


def run_game(folder, engine, ids, extra=(), scenario=SEASONS["october"], exposure=FIXED_EXPOSURE_EV):
    """Renders the plots offscreen into `folder`: the editor in game mode on the Kesmark range, only `ids`, at a fixed
    exposure so passes compare (or adapting, with exposure None)."""
    folder.mkdir(parents=True, exist_ok=True)
    exe = Path(engine) / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
    project = ROOT / "unreal" / "SAM3D" / "SAM3D.uproject"
    fixed = [] if exposure is None else [f"-SimFixedExposure={exposure}"]
    cmd = [str(exe), str(project), "/Game/Maps/KesmarkRange", "-game", "-RenderOffscreen",
           f"-SimScenario={scenario}", f"-SimMapShots={folder.as_posix()}", "-SimShotSet=plots",
           f"-SimPlots={','.join(ids)}", "-unattended", *fixed, *extra]
    return subprocess.run(cmd, check=False).returncode


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("run", "page", "approve"))
    ap.add_argument("--label", default="latest")
    ap.add_argument("--plots", default="", help="comma-separated plot ids (default: all)")
    ap.add_argument("--extra", default="", help="more arguments for the game, space-separated (e.g. -SimGroundDebug=2)")
    ap.add_argument("--season", choices=sorted(SEASONS), default="october", help="the plot lab's scenario (default october)")
    ap.add_argument("--engine", default=os.environ.get("UE_ROOT", "D:/UE_5.8"))
    args = ap.parse_args(argv)
    data = json.loads(PLOTS.read_text(encoding="utf-8"))
    photos = json.loads(tv.VIEWS.read_text(encoding="utf-8")).get("photos", {})
    plots = data["plots"]
    ids = plot_ids(args.plots, plots)
    folder = OUT / args.label
    if args.command == "approve":
        APPROVED.mkdir(parents=True, exist_ok=True)
        for pid in ids:
            for cam, _ in CAMERAS:
                src = folder / f"{pid}_{cam}.png"
                if src.exists():
                    shutil.copy2(src, APPROVED / src.name)
                    print(f"approved {src.name}")
        return 0
    seconds = None
    if args.command == "run":
        for pid in ids:
            for f in folder.glob(f"{pid}_*"):
                f.unlink()  # a run replaces these plots' earlier pictures in the label
        start = time.monotonic()
        # One launch per exposure: a plot's own exposure_ev (A.3.20, a plot into the sun or into a wood's shade), else
        # the lab's fixed one. Each plot's pictures are its own files, so the launches share the folder.
        by_ev = {}
        by_id = {p["id"]: p for p in plots}
        for pid in ids:
            by_ev.setdefault(by_id[pid].get("exposure_ev", FIXED_EXPOSURE_EV), []).append(pid)
        code = 0
        for ev, group in by_ev.items():
            code = code or run_game(folder, args.engine, group, args.extra.split(), SEASONS[args.season], exposure=ev)
        seconds = time.monotonic() - start
        print(f"render exit {code} in {seconds:.0f} s ({len(by_ev)} launch(es))")
        if not any(folder.glob("*.png")):
            print("no pictures were written")
            return 1
    look = json.loads(LOOK.read_text(encoding="utf-8")) if LOOK.exists() else None
    page = build_page(plots, photos, folder, ids, args.label, seconds, look)
    (folder / "index.html").write_text(page, encoding="utf-8")
    print(f"wrote {folder / 'index.html'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
