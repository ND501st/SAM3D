"""Draw a generated map from above, from map_dump's JSON (simcore/examples/map_dump.cpp): the ground shaded by height,
the woods, buildings, sites, roads, the railway and the river, with the places' names. A check of the layout without
Unreal (docs/TEST_RANGE.md). Also the whole country, from country_dump's JSON (simcore/examples/country_dump.cpp,
docs/COUNTRY_MAP.md), with the theatre's objects and the enemy's fronts over it.

    ./simcore/build/map_dump data maps/kesmark_range.json out/kesmark_range_map.json
    python tools/map_view.py out/kesmark_range_map.json            # writes out/map_view/kesmark_range.html
    python tools/map_view.py out/kesmark_range_map.json --far      # the ground beyond the map as well
    python tools/map_view.py out/kesmark_range_map.json --png      # a picture instead (needs Pillow)
    ./simcore/build/country_dump data maps/pannonia.json out/pannonia_map.json
    python tools/map_view.py out/pannonia_map.json --png           # the country, with data/air_war/theatre_pannonia.json
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
    "factory": "#806040", "troops": "#406020", "city": "#000000",
}


def country_shapes(dump, theatre=None):
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
        for o in theatre.get("objects", []):
            if o["kind"] == "city":
                continue
            shapes.append(("dot", o["east_km"], o["north_km"], 0.8, OBJECT_COLOURS.get(o["kind"], "#888888")))
        for f in theatre.get("fronts", []):
            colour = NEIGHBOUR_COLOURS.get(f["faction"], "#555555")
            for en in f.get("entries", []):
                shapes.append(("dot", en["east_km"], en["north_km"], 1.6, colour))
                shapes.append(("text", en["east_km"], en["north_km"] + 3, en["id"], 10))
            shapes.append(("dot", f["bases"]["east_km"], f["bases"]["north_km"], 2.4, colour))
            shapes.append(("text", f["bases"]["east_km"], f["bases"]["north_km"] + 4, f["faction"] + " bases", 10))
    return shapes


def country_bounds(dump, theatre=None):
    w, s, e, n = dump["extent_km"]
    if theatre:
        for f in theatre.get("fronts", []):
            for p in [f["bases"]] + f.get("entries", []):
                w, e = min(w, p["east_km"] - 8), max(e, p["east_km"] + 8)
                s, n = min(s, p["north_km"] - 8), max(n, p["north_km"] + 8)
    return w, s, e, n


def country_svg(dump, theatre=None, size_px=1600):
    w, s, e, n = country_bounds(dump, theatre)
    scale = size_px / max(e - w, n - s)
    X = lambda x: (x - w) * scale
    Y = lambda y: (n - y) * scale
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size_px} {size_px}" width="{size_px}" height="{size_px}">',
           f'<rect width="{size_px}" height="{size_px}" fill="#cfcfc8"/>']
    for sh in country_shapes(dump, theatre):
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
        elif sh[0] == "text":
            out.append(f'<text x="{X(sh[1]):.1f}" y="{Y(sh[2]):.1f}" font-family="sans-serif" font-size="{sh[4]}" fill="#111" stroke="#fff" '
                       f'stroke-width="2.5" paint-order="stroke" text-anchor="middle">{html.escape(sh[3])}</text>')
    out.append(f'<rect x="20" y="{size_px - 40}" width="{50 * scale:.1f}" height="6" fill="#111"/>')
    out.append(f'<text x="20" y="{size_px - 48}" font-family="sans-serif" font-size="13">50 km</text>')
    out.append("</svg>")
    return "\n".join(out)


def country_png(dump, path, theatre=None, size_px=2400):
    from PIL import Image, ImageDraw
    w, s, e, n = country_bounds(dump, theatre)
    scale = size_px / max(e - w, n - s)
    X = lambda x: (x - w) * scale
    Y = lambda y: (n - y) * scale
    img = Image.new("RGB", (size_px, size_px), (207, 207, 200))
    d = ImageDraw.Draw(img, "RGBA")
    for sh in country_shapes(dump, theatre):
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
        elif sh[0] == "text":
            d.text((X(sh[1]) - 3 * len(sh[3]), Y(sh[2]) - 6), sh[3], fill=(0, 0, 0), stroke_width=2, stroke_fill=(255, 255, 255))
    d.rectangle([20, size_px - 40, 20 + 50 * scale, size_px - 34], fill=(0, 0, 0))
    d.text((20, size_px - 58), "50 km", fill=(0, 0, 0))
    img.save(path)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dump")
    ap.add_argument("--far", action="store_true", help="draw the ground beyond the map too")
    ap.add_argument("--png", action="store_true", help="write a PNG instead of an HTML page (needs Pillow)")
    ap.add_argument("--theatre", default=str(ROOT / "data" / "air_war" / "theatre_pannonia.json"),
                    help="the country: the theatre whose objects and fronts to draw over it ('' none)")
    ap.add_argument("--out", default=None)
    a = ap.parse_args(argv)
    dump = json.loads(Path(a.dump).read_text(encoding="utf-8"))
    out = Path(a.out) if a.out else ROOT / "out" / "map_view" / (dump["map_id"] + ("_far" if a.far else "") + (".png" if a.png else ".html"))
    out.parent.mkdir(parents=True, exist_ok=True)
    if dump.get("kind") == "country":
        theatre = json.loads(Path(a.theatre).read_text(encoding="utf-8")) if a.theatre and Path(a.theatre).exists() else None
        if a.png:
            country_png(dump, out, theatre)
        else:
            out.write_text(f'<!doctype html><meta charset="utf-8"><title>{html.escape(dump["name"])}</title>'
                           f'<body style="margin:0;background:#ddd">{country_svg(dump, theatre)}</body>', encoding="utf-8")
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
