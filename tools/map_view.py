"""Draw a generated map from above, from map_dump's JSON (simcore/examples/map_dump.cpp): the ground shaded by height,
the woods, buildings, sites, roads, the railway and the river, with the places' names. A check of the layout without
Unreal (docs/TEST_RANGE.md). Also the whole country, from country_dump's JSON (simcore/examples/country_dump.cpp,
docs/COUNTRY_MAP.md), with the theatre's objects and the enemy's fronts over it.

    ./simcore/build/map_dump data maps/kesmark_range.json out/kesmark_range_map.json
    python tools/map_view.py out/kesmark_range_map.json            # writes out/map_view/kesmark_range.html
    python tools/map_view.py out/kesmark_range_map.json --far      # the ground beyond the map as well
    python tools/map_view.py out/kesmark_range_map.json --png      # a picture instead (needs Pillow)
    ./simcore/build/country_dump data maps/pannonia.json out/pannonia_map.json
    python tools/map_view.py out/pannonia_map.json --png           # the country, with the nation's theatre over it
    ./simcore/build/nav_check data 200 --json out/routes.json
    python tools/map_view.py out/pannonia_map.json --routes out/routes.json   # battery 21's routes over the country
    ./simcore/build/fronts_bench data --map sector_cp_duty,rgt_chief_of_staff --map-json out/fronts_map.json --days 10 nato
    python tools/map_view.py out/pannonia_map.json --front out/fronts_map.json   # the truth beside our side's belief (F5.3)
"""
import argparse
import html
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

PART_COLOURS = {
    "house": "#9e855f", "flats": "#85847f", "industrial": "#6b665e", "farm": "#8c7350", "military": "#46503a",
    "hangar": "#5c605a", "shelter": "#5a5e50", "tower": "#c0b8a3", "station": "#8c5c45", "tank": "#9e9e94",
    "chimney": "#734033", "cooling_tower": "#9a9a92", "bridge": "#6b6b6b", "canopy": "#2c4a20", "marsh": "#4a5a38",
    "revetment": "#6b583c", "pad": "#7a7060", "runway": "#606060", "mast": "#333333", "tent": "#4a5536",
    "bunker": "#4a4e3a", "launcher": "#3e4a30",
}
SURFACE_COLOURS = {"water": "#2a5470", "main_road": "#2a2a2a", "road": "#3a3a38", "track": "#6b583c", "rail": "#3a2f25"}


def ground_colour(h, lo, hi):
    t = 0.0 if hi <= lo else (h - lo) / (hi - lo)
    a, b = (70, 92, 42), (150, 135, 95)
    return "#%02x%02x%02x" % tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def svg(dump, far=False, size_px=1600):
    w, s, e, n = dump["extent_m"]
    if far:
        w = s = -dump["far_m"]
        e = n = dump["far_m"]
    scale = size_px / max(e - w, n - s)
    X = lambda x: (x - w) * scale
    Y = lambda y: (n - y) * scale
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size_px} {size_px}" width="{size_px}" height="{size_px}">']
    grids = [dump["far_ground"]] if far else []
    grids.append(dump["ground"])
    all_h = [h for g in grids for row in g["rows"] for h in row]
    lo, hi = min(all_h), max(all_h)
    for g in grids:
        st = g["step"]
        for r, row in enumerate(g["rows"]):
            for c, h in enumerate(row):
                x0, y0 = g["west"] + c * st - st / 2, g["south"] + r * st + st / 2
                out.append(f'<rect x="{X(x0):.1f}" y="{Y(y0):.1f}" width="{st * scale + 0.6:.1f}" height="{st * scale + 0.6:.1f}" '
                           f'fill="{ground_colour(h, lo, hi)}"/>')
    order = ["marsh", "canopy", "pad", "runway"]
    boxes = sorted(dump["boxes"], key=lambda b: order.index(b[0]) if b[0] in order else len(order))
    for part, be, bn, length, width, height, heading in boxes:
        if not (w <= be <= e and s <= bn <= n):
            continue
        wid = length if part in ("tank", "chimney", "cooling_tower") else width
        lp, wp = max(length * scale, 0.6), max(wid * scale, 0.6)
        out.append(f'<rect x="{X(be) - lp / 2:.1f}" y="{Y(bn) - wp / 2:.1f}" width="{lp:.1f}" height="{wp:.1f}" '
                   f'fill="{PART_COLOURS.get(part, "#888")}" transform="rotate({math.degrees(heading) - 90:.1f} {X(be):.1f} {Y(bn):.1f})"/>')
    for strip in dump["strips"]:
        pts = " ".join(f"{X(p[0]):.1f},{Y(p[1]):.1f}" for p in strip["points"])
        width = max(strip["width_m"] * scale, 1.0 if strip["surface"] != "track" else 0.6)
        dash = ' stroke-dasharray="4 2"' if strip["surface"] == "rail" else ""
        out.append(f'<polyline points="{pts}" fill="none" stroke="{SURFACE_COLOURS.get(strip["surface"], "#000")}" '
                   f'stroke-width="{width:.1f}"{dash}/>')
    for lab in dump["labels"]:
        lx, ly = lab["at"]
        if not (w <= lx <= e and s <= ly <= n):
            continue
        big = lab["kind"] in ("town", "city", "village", "airfield")
        out.append(f'<text x="{X(lx):.1f}" y="{Y(ly) - 6:.1f}" font-family="sans-serif" font-size="{14 if big else 10}" '
                   f'fill="#111" stroke="#fff" stroke-width="2.5" paint-order="stroke" text-anchor="middle">{html.escape(lab["name"])}</text>')
    km = 10 if not far else 50
    out.append(f'<rect x="20" y="{size_px - 40}" width="{km * 1000 * scale:.1f}" height="6" fill="#111"/>')
    out.append(f'<text x="20" y="{size_px - 48}" font-family="sans-serif" font-size="13">{km} km</text>')
    out.append("</svg>")
    return "\n".join(out)


def png(dump, path, far=False, size_px=2400):
    """The same drawing as a PNG (Pillow)."""
    from PIL import Image, ImageDraw
    w, s, e, n = dump["extent_m"]
    if far:
        w = s = -dump["far_m"]
        e = n = dump["far_m"]
    scale = size_px / max(e - w, n - s)
    X = lambda x: (x - w) * scale
    Y = lambda y: (n - y) * scale
    img = Image.new("RGB", (size_px, size_px), (200, 200, 200))
    d = ImageDraw.Draw(img)
    grids = ([dump["far_ground"]] if far else []) + [dump["ground"]]
    all_h = [h for g in grids for row in g["rows"] for h in row]
    lo, hi = min(all_h), max(all_h)
    for g in grids:
        st = g["step"]
        for r, row in enumerate(g["rows"]):
            for c, h in enumerate(row):
                x0, y0 = g["west"] + c * st - st / 2, g["south"] + r * st + st / 2
                d.rectangle([X(x0), Y(y0), X(x0 + st) + 1, Y(y0 - st) + 1], fill=ground_colour(h, lo, hi))
    order = ["marsh", "canopy", "pad", "runway"]
    for part, be, bn, length, width, height, heading in sorted(dump["boxes"], key=lambda b: order.index(b[0]) if b[0] in order else len(order)):
        if not (w <= be <= e and s <= bn <= n):
            continue
        wid = length if part in ("tank", "chimney", "cooling_tower") else width
        se, ce = math.sin(heading), math.cos(heading)
        pts = []
        for fl, fr in ((0.5, 0.5), (0.5, -0.5), (-0.5, -0.5), (-0.5, 0.5)):
            pe = be + fl * length * se + fr * wid * ce
            pn = bn + fl * length * ce - fr * wid * se
            pts.append((X(pe), Y(pn)))
        d.polygon(pts, fill=PART_COLOURS.get(part, "#888888"))
    for strip in dump["strips"]:
        pts = [(X(p[0]), Y(p[1])) for p in strip["points"]]
        d.line(pts, fill=SURFACE_COLOURS.get(strip["surface"], "#000000"), width=max(1, int(round(strip["width_m"] * scale * 1.5))))
    for lab in dump["labels"]:
        lx, ly = lab["at"]
        if w <= lx <= e and s <= ly <= n and lab["kind"] in ("town", "city", "village", "airfield", "sam_site", "osa_position", "radar_post"):
            d.text((X(lx) + 4, Y(ly) - 12), lab["name"], fill=(0, 0, 0), stroke_width=2, stroke_fill=(255, 255, 255))
    km = 10 if not far else 50
    d.rectangle([20, size_px - 40, 20 + km * 1000 * scale, size_px - 34], fill=(0, 0, 0))
    d.text((20, size_px - 58), f"{km} km", fill=(0, 0, 0))
    img.save(path)


# ---------------------------------------------------------------------------------------------- the country

NEIGHBOUR_COLOURS = {"nato": "#2456b0", "warsaw_pact": "#b02424", "neutral": "#2f8a3a", "rebels": "#a05a10", "none": "#555555"}
OBJECT_COLOURS = {
    "sam_site": "#d02020", "aaa": "#e07030", "ew_radar": "#7030a0", "command_post": "#202020", "headquarters": "#202020",
    "airfield": "#606060", "bridge": "#303030", "rail_junction": "#5a3a20", "depot": "#a08020", "power_plant": "#e0c000",
    "factory": "#806040", "troops": "#406020", "city": "#000000", "refinery": "#ff8c1a", "pipeline_station": "#ffb050",
    "substation": "#c8a800", "dam": "#3a7ec0", "materials_depot": "#c0a070", "port": "#406080", "government": "#a00060",
    "broadcast": "#d050d0", "exchange": "#5050d0",
}


def map_places(map_path):
    """A country map's places by id (roadmap F1): its settlements, landmarks and sites, as (east_km, north_km)."""
    m = json.loads(Path(map_path).read_text(encoding="utf-8"))
    return {p["id"]: (p["east_km"], p["north_km"]) for key in ("settlements", "landmarks", "sites") for p in m.get(key, [])}


def place_points(t, places):
    """Gives every point of a theatre on a place map its east_km and north_km from the place it names ("at"): its
    objects, their prepared positions, its fronts' line and entry points, bases, battery positions and missile launch
    points."""
    def put(d):
        if isinstance(d, dict) and "at" in d and "east_km" not in d:
            if d["at"] not in places:
                raise ValueError(f"no place '{d['at']}' on the theatre's place map")
            d["east_km"], d["north_km"] = places[d["at"]]
    for o in t.get("objects", []):
        put(o)
        for p in o.get("positions", []):
            put(p)
    for f in t.get("fronts", []):
        for p in f.get("line", []) + f.get("entries", []):
            put(p)
        for key in ("bases", "battery_position", "missile_launch"):
            put(f.get(key))
    return t


def load_theatre(path, depth=0):
    """A theatre file, following "extends" as simcore's LoadTheatreFile does: the base's objects in their places, one
    with the same id replaced, "remove" dropped, the new ones after them; a front replaces the base's of its faction.
    A theatre on a place map ("place_map", relative to the data folder above the theatre's) takes its points from it."""
    path = Path(path)
    t = json.loads(path.read_text(encoding="utf-8"))
    if t.get("place_map"):
        place_points(t, map_places(path.parent.parent / t["place_map"]))
    if "extends" not in t:
        return t
    if depth >= 4:
        raise ValueError(f"{path}: extends more than four deep (a loop?)")
    base = load_theatre(path.parent / t["extends"], depth + 1)
    removed = set(t.get("remove", []))
    objects = [o for o in base.get("objects", []) if o["id"] not in removed]
    index = {o["id"]: i for i, o in enumerate(objects)}
    for o in t.get("objects", []):
        if o["id"] in index:
            objects[index[o["id"]]] = o
        else:
            index[o["id"]] = len(objects)
            objects.append(o)
    fronts = list(base.get("fronts", []))
    for f in t.get("fronts", []):
        at = next((i for i, g in enumerate(fronts) if g["faction"] == f["faction"]), None)
        if at is None:
            fronts.append(f)
        else:
            fronts[at] = f
    merged = dict(base)
    merged.update({k: v for k, v in t.items() if k not in ("extends", "remove", "objects", "fronts")})
    merged["objects"], merged["fronts"] = objects, fronts
    return merged


ROUTE_COLOURS = {"road": "#c01818", "off_road": "#c07018", "swim": "#1860c0"}


def route_shapes(routes):
    """nav_check --json's routes (km) as lines: each stretch in its kind's colour, off the road dashed; the far end named."""
    shapes = []
    for r in routes.get("routes", []):
        pts = r["points_km"]
        kinds = r.get("stretches", [])
        for i in range(len(pts) - 1):
            k = kinds[i] if i < len(kinds) else "road"
            shapes.append(("line", [pts[i], pts[i + 1]], ROUTE_COLOURS.get(k, "#c01818"), 2.5, k != "road"))
        end = pts[-1]
        shapes.append(("text", end[0], end[1] + 2, "%s %d min" % (r["to"], round(r["time_s"] / 60)), 9))
    return shapes


def country_shapes(dump, theatre=None, routes=None):
    """The country as drawing primitives in km: (kind, ...) tuples that svg and png both render."""
    shapes = []
    g = dump["ground"]
    all_h = [h for row in g["rows"] for h in row]
    lo, hi = min(all_h), max(all_h)
    st = g["step"]
    for r, row in enumerate(g["rows"]):
        for c, h in enumerate(row):
            x0, y0 = g["west"] + c * st - st / 2, g["south"] + r * st - st / 2
            shapes.append(("rect", x0, y0, x0 + st, y0 + st, ground_colour(h, lo, hi)))
    for f in dump["forests"]:
        shapes.append(("poly", f["points"], "#2f4a24", None, 0))
    for m in dump.get("marshes", []):
        shapes.append(("poly", m["points"], "#5b6b45", None, 0))
    for lake in dump.get("lakes", []):
        shapes.append(("poly", lake["points"], "#2a5ea0", None, 0))
        if lake.get("name"):
            shapes.append(("text", sum(p[0] for p in lake["points"]) / len(lake["points"]),
                           sum(p[1] for p in lake["points"]) / len(lake["points"]) - 2.0, lake["name"], 9))
    for r in dump.get("ridges", []):
        shapes.append(("line", r["points"], "#7a5a3a", 1, True))
    for reg in dump["regions"]:
        shapes.append(("line", reg["points"] + reg["points"][:1], "#6a6a5a", 1, True))
    for r in dump["rivers"]:
        shapes.append(("line", r["points"], "#2a5ea0", 3, False))
    for r in dump["roads"]:
        shapes.append(("line", r["points"], "#202020" if r["class"] == "main" else "#4a4a46", 2 if r["class"] == "main" else 1, False))
    for r in dump["railways"]:
        shapes.append(("line", r["points"], "#6a3a1a", 2, True))
    for r in dump["pipelines"]:
        shapes.append(("line", r["points"], "#e07a00", 2, True))
    shapes.append(("line", dump["outline"] + dump["outline"][:1], "#000000", 2, False))
    for b in dump["borders"]:
        shapes.append(("line", b["points"], NEIGHBOUR_COLOURS.get(b["neighbour"], "#555555"), 4, False))
    for x in dump["crossings"]:
        colour = {"road": "#000000", "rail": "#6a3a1a", "oil": "#e07a00"}.get(x["net"], "#000000")
        shapes.append(("dot", x["at"][0], x["at"][1], 0.9, colour))
    for s in dump["settlements"]:
        r = {"capital": 3.0, "city": 2.2, "town": 1.4, "village": 0.7}.get(s["kind"], 1.0)
        shapes.append(("dot", s["at"][0], s["at"][1], r, "#f4f0e0"))
        if s["kind"] != "village":
            shapes.append(("text", s["at"][0], s["at"][1] + r + 0.5, s["name"], 15 if s["kind"] in ("capital", "city") else 12))
    for l in dump["landmarks"]:
        shapes.append(("dot", l["at"][0], l["at"][1], 1.2, "#e07a00" if l["kind"] in ("refinery", "pipeline_station") else "#333333"))
        shapes.append(("text", l["at"][0], l["at"][1] - 2.5, l["name"], 9))
    if theatre:
        kinds = []
        for o in theatre.get("objects", []):
            if o["kind"] == "city":
                continue
            colour = OBJECT_COLOURS.get(o["kind"], "#888888")
            for p in o.get("positions", []):  # its prepared positions
                shapes.append(("line", [[o["east_km"], o["north_km"]], [p["east_km"], p["north_km"]]], colour, 1, True))
                shapes.append(("dot", p["east_km"], p["north_km"], 0.35, colour))
            shapes.append(("dot", o["east_km"], o["north_km"], 0.8, colour))
            if o["kind"] not in kinds:
                kinds.append(o["kind"])
        w, _, _, n = country_bounds(dump, theatre)
        for i, k in enumerate(kinds):
            y = n - 8 - i * 6
            shapes.append(("dot", w + 8, y, 1.2, OBJECT_COLOURS.get(k, "#888888")))
            shapes.append(("ltext", w + 11, y - 1, k.replace("_", " "), 12))
        if kinds:
            y = n - 8 - len(kinds) * 6
            shapes.append(("dot", w + 8, y, 0.35, "#888888"))
            shapes.append(("ltext", w + 11, y - 1, "prepared position", 12))
        for f in theatre.get("fronts", []):
            colour = NEIGHBOUR_COLOURS.get(f["faction"], "#555555")
            for en in f.get("entries", []):
                shapes.append(("dot", en["east_km"], en["north_km"], 1.6, colour))
                shapes.append(("text", en["east_km"], en["north_km"] + 3, en["id"], 10))
            shapes.append(("dot", f["bases"]["east_km"], f["bases"]["north_km"], 2.4, colour))
            shapes.append(("text", f["bases"]["east_km"], f["bases"]["north_km"] + 4, f["faction"] + " bases", 10))
    if routes:
        shapes.extend(route_shapes(routes))
    return shapes


def country_bounds(dump, theatre=None):
    w, s, e, n = dump["extent_km"]
    if theatre:
        for f in theatre.get("fronts", []):
            for p in [f["bases"]] + f.get("entries", []):
                w, e = min(w, p["east_km"] - 8), max(e, p["east_km"] + 8)
                s, n = min(s, p["north_km"] - 8), max(n, p["north_km"] + 8)
    return w, s, e, n


def country_svg(dump, theatre=None, size_px=1600, routes=None, extra=None):
    w, s, e, n = country_bounds(dump, theatre)
    scale = size_px / max(e - w, n - s)
    X = lambda x: (x - w) * scale
    Y = lambda y: (n - y) * scale
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size_px} {size_px}" width="{size_px}" height="{size_px}">',
           f'<rect width="{size_px}" height="{size_px}" fill="#cfcfc8"/>']
    for sh in country_shapes(dump, theatre, routes) + list(extra or []):
        if sh[0] == "rect":
            _, x0, y0, x1, y1, fill = sh
            out.append(f'<rect x="{X(x0):.1f}" y="{Y(y1):.1f}" width="{(x1 - x0) * scale + 0.6:.1f}" height="{(y1 - y0) * scale + 0.6:.1f}" fill="{fill}"/>')
        elif sh[0] == "poly":
            pts = " ".join(f"{X(p[0]):.1f},{Y(p[1]):.1f}" for p in sh[1])
            out.append(f'<polygon points="{pts}" fill="{sh[2]}" fill-opacity="0.55"/>')
        elif sh[0] == "line":
            pts = " ".join(f"{X(p[0]):.1f},{Y(p[1]):.1f}" for p in sh[1])
            dash = ' stroke-dasharray="6 3"' if sh[4] else ""
            out.append(f'<polyline points="{pts}" fill="none" stroke="{sh[2]}" stroke-width="{sh[3]}"{dash}/>')
        elif sh[0] == "dot":
            out.append(f'<circle cx="{X(sh[1]):.1f}" cy="{Y(sh[2]):.1f}" r="{max(sh[3] * scale, 2):.1f}" fill="{sh[4]}" stroke="#111" stroke-width="0.6"/>')
        elif sh[0] in ("text", "ltext"):
            anchor = "middle" if sh[0] == "text" else "start"
            out.append(f'<text x="{X(sh[1]):.1f}" y="{Y(sh[2]):.1f}" font-family="sans-serif" font-size="{sh[4]}" fill="#111" stroke="#fff" '
                       f'stroke-width="2.5" paint-order="stroke" text-anchor="{anchor}">{html.escape(sh[3])}</text>')
    out.append(f'<rect x="20" y="{size_px - 40}" width="{50 * scale:.1f}" height="6" fill="#111"/>')
    out.append(f'<text x="20" y="{size_px - 48}" font-family="sans-serif" font-size="13">50 km</text>')
    out.append("</svg>")
    return "\n".join(out)


def country_png(dump, path, theatre=None, size_px=2400, routes=None, extra=None):
    from PIL import Image, ImageDraw
    w, s, e, n = country_bounds(dump, theatre)
    scale = size_px / max(e - w, n - s)
    X = lambda x: (x - w) * scale
    Y = lambda y: (n - y) * scale
    img = Image.new("RGB", (size_px, size_px), (207, 207, 200))
    d = ImageDraw.Draw(img, "RGBA")
    for sh in country_shapes(dump, theatre, routes) + list(extra or []):
        if sh[0] == "rect":
            _, x0, y0, x1, y1, fill = sh
            d.rectangle([X(x0), Y(y1), X(x1) + 1, Y(y0) + 1], fill=fill)
        elif sh[0] == "poly":
            rgb = tuple(int(sh[2][i:i + 2], 16) for i in (1, 3, 5))
            d.polygon([(X(p[0]), Y(p[1])) for p in sh[1]], fill=rgb + (140,))
        elif sh[0] == "line":
            d.line([(X(p[0]), Y(p[1])) for p in sh[1]], fill=sh[2], width=max(1, int(sh[3] * size_px / 1600)))
        elif sh[0] == "dot":
            r = max(sh[3] * scale, 3)
            d.ellipse([X(sh[1]) - r, Y(sh[2]) - r, X(sh[1]) + r, Y(sh[2]) + r], fill=sh[4], outline=(17, 17, 17))
        elif sh[0] in ("text", "ltext"):
            x = X(sh[1]) - (3 * len(sh[3]) if sh[0] == "text" else 0)
            d.text((x, Y(sh[2]) - 6), sh[3], fill=(0, 0, 0), stroke_width=2, stroke_fill=(255, 255, 255))
    d.rectangle([20, size_px - 40, 20 + 50 * scale, size_px - 34], fill=(0, 0, 0))
    d.text((20, size_px - 58), "50 km", fill=(0, 0, 0))
    img.save(path)


# ------------------------------------------------------------------------------------------------ the fronts (F5.3)
# fronts_bench --map-json's runs: the war's truth by day and, beside it, the situation map a post held at the end (metres in
# the file, km on the page). For the developer and the owner: the player's screen never shows the truth.

TRUTH_LINE = "#b3261e"
BELIEF_LINE = "#1d4ed8"
AIR_FRONT = "#6d28d9"
OURS = "#2e7d32"
ENEMY = "#8b1a1a"


def _km(p):
    return (p[0] / 1000.0, p[1] / 1000.0)


def _runs(front):
    runs = front.get("runs", [])
    if not runs:
        raise ValueError("the fronts file holds no runs (fronts_bench --map ... --map-json)")
    return runs


def _base_shape(run):
    return ("line", [_km(p) for p in run["base_line"]], "#777777", 1, True)


def _air_band(run, offsets_m, depths_by_sector, label_prefix=""):
    """The air front by sector: the line of contact (base line moved by offsets) moved on by that sector's depth."""
    shapes = []
    stations = run["stations"]
    pts_by_sector = {}
    for i, st in enumerate(stations):
        depth = depths_by_sector.get(st["sector"])
        if depth is None or depth <= 0:
            continue
        off = offsets_m[i] + depth
        pts_by_sector.setdefault(st["sector"], []).append(((st["x"] + st["nx"] * off) / 1000.0, (st["y"] + st["ny"] * off) / 1000.0))
    names = {i: sd["name"] for i, sd in enumerate(run.get("sectors", []))}
    for sector, pts in sorted(pts_by_sector.items()):
        shapes.append(("line", pts, AIR_FRONT, 2, True))
        mid = pts[len(pts) // 2]
        shapes.append(("ltext", mid[0] + 1, mid[1] + 1, f"{label_prefix}air front {names.get(sector, sector)} {depths_by_sector[sector] / 1000:.0f} km", 11))
    return shapes


def truth_shapes(run, day=None):
    """The truth: the line of contact on every day (older days paler), the air front by sector and the formations on `day`."""
    days = run["truth"]
    if not days:
        raise ValueError("the run holds no truth days")
    last = days[-1] if day is None else next((d for d in days if d["day"] == day), None)
    if last is None:
        raise ValueError(f"no day {day} in the run (0..{days[-1]['day']})")
    shapes = [_base_shape(run)]
    for d in days:
        if d["day"] > last["day"]:
            break
        fresh = d is last
        shade = int(230 - 140 * (d["day"] / max(1, last["day"])))
        colour = TRUTH_LINE if fresh else "#%02x%02x%02x" % (230, shade, shade)
        shapes.append(("line", [_km(p) for p in d["line"]], colour, 3 if fresh else 1, False))
    depths = {i: v for i, v in enumerate(last.get("air_front_m", []))}
    shapes.extend(_air_band(run, last["offset_m"], depths))
    for f in last.get("formations", []):
        if f.get("spent"):
            continue
        colour = OURS if f["ours"] else ENEMY
        x, y = _km((f["x"], f["y"]))
        shapes.append(("dot", x, y, 0.7, colour))
        shapes.append(("ltext", x + 1, y - 1, f["name"], 9))
    shapes.append(("ltext", run["stations"][0]["x"] / 1000.0, _legend_y(run), f"truth, day {last['day']} (hour {last['hour']:.0f}): the line of contact by day, the air front, the formations", 13))
    return shapes


def _legend_y(run):
    ys = [p[1] for p in run["base_line"]]
    return min(ys) / 1000.0 - 10


def _believed_line(m):
    """The believed line split where the sector changes staleness: [(points, stale)]."""
    stale_by_sector = {}
    for a in m.get("axes", []):
        sec = a["sector"]
        stale_by_sector[sec] = stale_by_sector.get(sec, True) and bool(a.get("stale"))
    out = []
    for p in m.get("line", []):
        stale = stale_by_sector.get(p["sector"], False)
        if out and out[-1][1] == stale:
            out[-1][0].append((p["x"] / 1000.0, p["y"] / 1000.0))
        else:
            if out:  # join to the previous piece so the line has no gaps
                prev = out[-1][0][-1]
                out.append(([prev, (p["x"] / 1000.0, p["y"] / 1000.0)], stale))
            else:
                out.append(([(p["x"] / 1000.0, p["y"] / 1000.0)], stale))
    return [(pts, st) for pts, st in out if len(pts) >= 2]


def line_error_km(run, m, day=None):
    """Mean and worst |believed depth - true depth| over the stations, km (the true depth never below the base line)."""
    days = run["truth"]
    last = days[-1] if day is None else next(d for d in days if d["day"] == day)
    pts = m.get("line", [])
    if not pts or len(pts) != len(last["offset_m"]):
        return None
    diffs = [abs(p["depth_m"] - max(0.0, t)) for p, t in zip(pts, last["offset_m"])]
    return sum(diffs) / len(diffs) / 1000.0, max(diffs) / 1000.0


def belief_shapes(run, post, day=None):
    """What `post` held at the end of the run: the line as reported (dashed where stale), our SAM units as it has them, the air
    defence staff's conclusion on the enemy's air front, and its units and enemy as a list (the map has no place for them)."""
    m = run["posts"].get(post)
    if m is None:
        raise ValueError(f"the run has no post {post!r}; it holds {', '.join(sorted(run['posts']))}")
    shapes = [_base_shape(run)]
    for pts, stale in _believed_line(m):
        shapes.append(("line", pts, "#7f9ae0" if stale else BELIEF_LINE, 2 if stale else 3, stale))
    for a in m.get("axes", []):
        if a["depth_m"] <= 0:
            continue
        cands = [p for p in m["line"] if p["sector"] == a["sector"]]
        if cands:
            deepest = max(cands, key=lambda p: p["depth_m"])
            tag = ", stale" if a.get("stale") else ""
            shapes.append(("ltext", deepest["x"] / 1000.0 + 1, deepest["y"] / 1000.0 + 1,
                           f"{a['name']} {a['depth_m'] / 1000:.0f} km in (h {a['h']:.0f}, {a['age_h']:.0f} h old{tag})", 10))
    depths = {af["sector"]: af["depth_m"] for af in m.get("air_front", [])}
    offsets = [p["depth_m"] for p in m.get("line", [])]
    if depths and len(offsets) == len(run["stations"]):
        shapes.extend(_air_band(run, offsets, depths, "staff: "))
    for sm in m.get("sams", []):
        if sm["basis"] != "plan":
            shapes.append(("dot", sm["x"] / 1000.0, sm["y"] / 1000.0, 0.7, OURS))
            shapes.append(("ltext", sm["x"] / 1000.0 + 1, sm["y"] / 1000.0 - 1, f"{sm['name']} ({sm['basis']} h {sm['h']:.0f})", 9))
    x0 = run["stations"][0]["x"] / 1000.0
    y0 = _legend_y(run)
    err = line_error_km(run, m, day)
    head = f"belief of {post} at hour {m['now_h']:.0f}: the line as reported, never the truth"
    if err:
        head += f" (against the truth: mean {err[0]:.1f} km, worst {err[1]:.0f} km)"
    shapes.append(("ltext", x0, y0, head, 13))
    lines = [f"{u['name']} {u['fraction'] * 100:.0f}% (h {u['h']:.0f}, {u['age_h']:.0f} h old{', stale' if u.get('stale') else ''})" for u in m.get("ours", [])]
    lines += [f"enemy {e['name']} {e['fraction'] * 100:.0f}% (h {e['h']:.0f}, {e['age_h']:.0f} h old{', stale' if e.get('stale') else ''})" for e in m.get("enemy", [])]
    if not lines:
        lines = ["no unit has reported to this post"]
    for i, text in enumerate(lines[:14]):
        shapes.append(("ltext", x0, y0 - 7 - i * 5, text, 10))
    return shapes


def front_page(dump, front, theatre=None, run=0, post="sector_cp_duty", day=None, size_px=1100):
    """The truth beside the belief as one page: two country drawings."""
    r = _runs(front)[run]
    truth = country_svg(dump, theatre, size_px=size_px, extra=truth_shapes(r, day))
    belief = country_svg(dump, theatre, size_px=size_px, extra=belief_shapes(r, post, day))
    title = f"{r['faction']}, seed {r['seed']}, {r['days']} days"
    return (f'<!doctype html><meta charset="utf-8"><title>{html.escape(title)}: the truth beside the belief</title>'
            f'<body style="margin:0;background:#ddd;font-family:sans-serif"><h3 style="margin:6px 10px">{html.escape(title)}: '
            f'the truth (left) and what {html.escape(post)} held (right)</h3>'
            f'<div style="display:flex;flex-wrap:wrap;gap:8px">{truth}{belief}</div></body>')


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dump")
    ap.add_argument("--far", action="store_true", help="draw the ground beyond the map too")
    ap.add_argument("--png", action="store_true", help="write a PNG instead of an HTML page (needs Pillow)")
    ap.add_argument("--theatre", default=str(ROOT / "data" / "air_war" / "theatre_pannonia_nation.json"),
                    help="the country: the theatre whose objects and fronts to draw over it ('' none)")
    ap.add_argument("--routes", default=None, help="the country: nav_check --json's routes to draw over it (roadmap G1.6)")
    ap.add_argument("--front", default=None, help="the country: fronts_bench --map-json's file, to draw the truth's line, air front and formations "
                    "beside our side's belief as a post held it (F5.3)")
    ap.add_argument("--run", type=int, default=0, help="--front: which run of the file")
    ap.add_argument("--post", default="sector_cp_duty", help="--front: the post whose situation map is the belief")
    ap.add_argument("--day", type=int, default=None, help="--front: the truth's day (default the last)")
    ap.add_argument("--out", default=None)
    a = ap.parse_args(argv)
    dump = json.loads(Path(a.dump).read_text(encoding="utf-8"))
    out = Path(a.out) if a.out else ROOT / "out" / "map_view" / (dump["map_id"] + ("_far" if a.far else "") + (".png" if a.png else ".html"))
    out.parent.mkdir(parents=True, exist_ok=True)
    if dump.get("kind") == "country":
        theatre = load_theatre(a.theatre) if a.theatre and Path(a.theatre).exists() else None
        routes = json.loads(Path(a.routes).read_text(encoding="utf-8")) if a.routes else None
        if a.front:
            front = json.loads(Path(a.front).read_text(encoding="utf-8"))
            if not a.png:
                fout = Path(a.out) if a.out else ROOT / "out" / "map_view" / (dump["map_id"] + "_front.html")
                fout.parent.mkdir(parents=True, exist_ok=True)
                fout.write_text(front_page(dump, front, theatre, a.run, a.post, a.day), encoding="utf-8")
                print(f"wrote {fout}")
                return 0
            r = _runs(front)[a.run]
            for tag, shapes in (("truth", truth_shapes(r, a.day)), ("belief", belief_shapes(r, a.post, a.day))):
                fout = out.with_name(f"{out.stem}_front_{tag}.png")
                country_png(dump, fout, theatre, extra=shapes)
                print(f"wrote {fout}")
            return 0
        if a.png:
            country_png(dump, out, theatre, routes=routes)
        else:
            out.write_text(f'<!doctype html><meta charset="utf-8"><title>{html.escape(dump["name"])}</title>'
                           f'<body style="margin:0;background:#ddd">{country_svg(dump, theatre, routes=routes)}</body>', encoding="utf-8")
        print(f"wrote {out}")
        return 0
    if a.png:
        png(dump, out, a.far)
        print(f"wrote {out}")
        return 0
    out.write_text(f'<!doctype html><meta charset="utf-8"><title>{html.escape(dump["name"])}</title>'
                   f'<body style="margin:0;background:#ddd">{svg(dump, a.far)}</body>', encoding="utf-8")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
