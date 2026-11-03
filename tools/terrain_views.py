"""The terrain's check views (roadmap T2, docs/scenery/T2_VIEWS_PLAN.md): render them in the game and set each beside its
reference photographs and its cost.

    python tools/terrain_views.py run --label t1            # render data/scenery/views.json's views, then the page
    python tools/terrain_views.py run --label t3 --against t1   # ... with the costs of an earlier label beside them
    python tools/terrain_views.py page --label t1           # only (re)build the page from what is in out/views/t1/

Each run writes out/views/<label>/: <view>.png (a panorama's <view>_h000.png ...), <view>.cost.json (the mean GPU, game
and render thread milliseconds over the wait before the picture), <view>.missing.txt (a view the map cannot give yet,
with its reason) and index.html. The photographs are links: nothing is downloaded or copied; the page's thumbnails load
from their pages in the viewer's browser, each with its author and licence (CC BY and CC BY-SA ask for them).

Needs the game's editor module built (UnrealBuildTool) and the map's ground textures fetched (tools/fetch_textures.py) for
the real look. Rendered offscreen, as docs/scenery/TERRAIN_ROADMAP.md section 8.2 does.
"""
import argparse
import html
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import quote, unquote

ROOT = Path(__file__).resolve().parent.parent
VIEWS = ROOT / "data" / "scenery" / "views.json"
OUT = ROOT / "out" / "views"
OPEN_LICENCE = re.compile(r"^(CC0( 1\.0)?|CC BY( \d\.\d)?|CC BY-SA( \d\.\d)?|Public domain)$")
COSTS = (("gpu_ms", "GPU"), ("game_ms", "game thread"), ("render_ms", "render thread"))


def licence_ok(licence):
    """Whether a photograph's licence is one of the open ones the project allows: CC0, CC BY, CC BY-SA, public domain."""
    return OPEN_LICENCE.match(licence) is not None


def thumbnail_url(page_url, width=320):
    """The image behind a photograph's page, for a thumbnail the viewer's browser loads: Wikimedia Commons' own redirect
    from a file page, or Fortepan's file from a photo's id. None for any other page."""
    m = re.match(r"https://commons\.wikimedia\.org/wiki/File:(.+)$", page_url)
    if m:
        return "https://commons.wikimedia.org/wiki/Special:FilePath/" + quote(unquote(m.group(1)), safe="()_,.-") + f"?width={width}"
    m = re.match(r"https://fortepan\.hu/hu/photos/\?id=(\d+)$", page_url)
    if m:
        return f"https://fortepan.download/file/fortepan-eu/1600/fortepan_{m.group(1)}.jpg"
    return None


def read_costs(folder):
    """{view: {"gpu_ms", "game_ms", "render_ms", "samples"}} from a run's <view>.cost.json files; {} for no folder."""
    out = {}
    folder = Path(folder)
    if not folder.is_dir():
        return out
    for f in sorted(folder.glob("*.cost.json")):
        try:
            data = json.loads(f.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        out[f.name[: -len(".cost.json")]] = data
    return out


def shots_of(folder, view_id):
    """The picture files of a view in a run's folder: <id>.png, or a panorama's <id>_hNNN.png, in order."""
    folder = Path(folder)
    names = [f.name for f in folder.glob(view_id + "*.png") if re.fullmatch(re.escape(view_id) + r"(_h\d{3})?\.png", f.name)]
    return sorted(names)


def missing_of(folder, view_id):
    """The reason a view is missing in a run, or None: <id>.missing.txt, or a panorama's <id>_hNNN.missing.txt."""
    folder = Path(folder)
    for f in sorted(folder.glob(view_id + "*.missing.txt")):
        if re.fullmatch(re.escape(view_id) + r"(_h\d{3})?\.missing\.txt", f.name):
            return f.read_text(encoding="utf-8").strip()
    return None


def cost_cells(costs, previous):
    """The cost line of a view: each time with the earlier run's beside it ('3.2 ms (was 2.9)')."""
    cells = []
    for key, label in COSTS:
        if costs is None or key not in costs:
            continue
        text = f"{label} {costs[key]:.2f} ms"
        if previous and key in previous:
            text += f" (was {previous[key]:.2f})"
        cells.append(text)
    return cells


def build_page(views_file, folder, previous_folder=None, label=""):
    """The page's HTML for a run's folder: per view, its pictures, its cost and its photographs' links."""
    data = json.loads(Path(views_file).read_text(encoding="utf-8"))
    photos = data.get("photos", {})
    costs, before = read_costs(folder), read_costs(previous_folder) if previous_folder else {}
    esc = html.escape
    rows = []
    for v in data["views"]:
        vid = v["id"]
        shots = shots_of(folder, vid)
        reason = missing_of(folder, vid)
        pictures = "".join(f'<img src="{esc(s)}" alt="{esc(s)}">' for s in shots)
        if not pictures:
            pictures = f'<p class="missing">Not rendered{": " + esc(reason) if reason else ""}.</p>'
        lines = []
        for name in sorted(k for k in costs if k == vid or k.startswith(vid + "_h")):
            cells = cost_cells(costs[name], before.get(name))
            if cells:
                lines.append(f"<li>{esc(name)}: {esc(', '.join(cells))}</li>")
        cost = f'<ul class="cost">{"".join(lines)}</ul>' if lines else ""
        refs = []
        for pid in v.get("photos", []):
            p = photos.get(pid)
            if p is None:
                continue
            thumb = thumbnail_url(p["page_url"])
            img = f'<img src="{esc(thumb)}" alt="{esc(p["title"])}" loading="lazy">' if thumb else ""
            refs.append(f'<figure><a href="{esc(p["page_url"])}">{img}<figcaption>{esc(pid)} {esc(p["title"])}</figcaption></a>'
                        f'<small>{esc(p["author"])}, {esc(p["licence"])}</small></figure>')
        rows.append(f'<section><h2>{esc(vid)}</h2><p class="must">Must show: {esc(v.get("must_see", ""))}</p>'
                    f'<div class="shots">{pictures}</div>{cost}<div class="refs">{"".join(refs)}</div></section>')
    return ("<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            f"<title>Terrain views {esc(label)}</title><style>"
            "body{font:14px system-ui;margin:16px;background:#fafafa;color:#222}img{max-width:100%}"
            "section{margin:0 0 32px;padding-bottom:16px;border-bottom:1px solid #ccc}"
            ".shots img{width:48%;margin:0 1% 8px 0}.refs{display:flex;flex-wrap:wrap;gap:8px}"
            ".refs figure{margin:0;width:200px}.refs img{width:200px;height:130px;object-fit:cover}"
            ".missing{color:#a40}.cost{margin:4px 0;padding-left:18px;font-variant-numeric:tabular-nums}.must{color:#555}"
            f"</style><h1>Terrain views: {esc(label)}</h1>" + "".join(rows))


def run_game(folder, engine, extra=()):
    """Renders the views offscreen into `folder`: the editor in game mode on the Kesmark range."""
    folder.mkdir(parents=True, exist_ok=True)
    exe = Path(engine) / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
    project = ROOT / "unreal" / "SAM3D" / "SAM3D.uproject"
    cmd = [str(exe), str(project), "/Game/Maps/KesmarkRange", "-game", "-RenderOffscreen",
           "-SimScenario=scenarios/kesmark_range_test.json", f"-SimMapShots={folder.as_posix()}", "-SimShotSet=views",
           "-unattended", *extra]
    return subprocess.run(cmd, check=False).returncode


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("run", "page"))
    ap.add_argument("--label", default="latest")
    ap.add_argument("--against", default="", help="an earlier label whose costs go beside these")
    ap.add_argument("--engine", default=os.environ.get("UE_ROOT", "D:/UE_5.8"))
    args = ap.parse_args(argv)
    folder = OUT / args.label
    if args.command == "run":
        for f in list(folder.glob("*.png")) + list(folder.glob("*.cost.json")) + list(folder.glob("*.missing.txt")):
            f.unlink()  # a run replaces the label's earlier one
        code = run_game(folder, args.engine)
        print(f"render exit {code}")
        if not list(folder.glob("*.png")):
            print("no pictures were written")
            return 1
    page = build_page(VIEWS, folder, OUT / args.against if args.against else None, args.label)
    (folder / "index.html").write_text(page, encoding="utf-8")
    print(f"wrote {folder / 'index.html'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
