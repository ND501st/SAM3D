"""Draw a system's cabin panels flat, to scale, as one HTML page: a quick look at the layout without Unreal.

Usage:  python tools/panel_sheet.py [system] [--out FILE] [--refs FILE] [--check]

    system   console and cabin file name, default osa_ak
             (data/procedures/<system>.json and data/cabins/<system>.json)
    --out    output page, default out/panel_sheets/<system>.html (out/ is git-ignored)
    --refs   JSON file {"<panel id>": "<image path>"}: a reference photo to show under that panel. Paths are
             relative to the refs file. Keep photos and the refs file in reference/ (git-ignored).
    --check  exit with status 1 if the layout has problems (overlaps, items over a panel edge, unplaced controls)

Standard library only. The page shows every panel seat by seat with its controls, lamps and displays at their
positions, a table of every control (label, positions, start position, source, confidence), and a list of layout
problems. Sizes match what the Unreal cabin draws (SimCabinComponent.cpp); styles follow simcore/cabin.hpp.
"""
import argparse
import html
import json
import math
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Footprints in metres, as the Unreal cabin draws them (SimCabinComponent.cpp).
FOOTPRINT_M = {
    "toggle": (0.030, 0.030),
    "rotary": (0.045, 0.045),
    "key": (0.032, 0.032),
    "button": (0.035, 0.035),
    "wheel": (0.120, 0.120),
    "cover": (0.050, 0.070),
    "lamp": (0.030, 0.030),
}
ROUND = {"toggle", "rotary", "key", "button", "wheel", "lamp", "ppi", "dial"}
CONFIDENCES = ("high", "medium", "low", "estimate")
PX_PER_M = 900.0
MARGIN_PX = 14.0


# ------------------------------------------------------------------ model

def load_system(system, root=ROOT):
    """The console and cabin files of a system, as parsed JSON."""
    with open(Path(root) / "data" / "procedures" / f"{system}.json", encoding="utf-8") as f:
        console = json.load(f)
    with open(Path(root) / "data" / "cabins" / f"{system}.json", encoding="utf-8") as f:
        cabin = json.load(f)
    return console, cabin


def default_style(control, controls):
    """The look of a control when the cabin file gives none. Mirrors DefaultCabinStyle in simcore/cabin.hpp."""
    kind = control.get("kind", "switch")
    if kind == "button":
        return "button"
    if kind == "wheel":
        return "wheel"
    for other in controls:
        if other.get("guard") == control["id"] and other.get("kind") != "wheel":
            return "cover"
    if kind == "key":
        return "key"
    return "rotary" if len(control.get("position_labels", [])) > 2 else "toggle"


def look_size(entry, default):
    """An item's footprint: the size its "look" gives (a diameter or [width, height], as the cabin draws it), or the
    style's default."""
    size = entry.get("look", {}).get("size_m")
    if size is None:
        return default
    return (size, size) if isinstance(size, (int, float)) else tuple(size)


def build_items(console, cabin):
    """Everything drawn on the panels: a list of dicts with panel, kind, centre and size in metres.

    A cover or lock that the cabin file doesn't place sits over the control it guards, as in cabin.hpp.
    """
    controls = console.get("controls", [])
    by_id = {c["id"]: c for c in controls}
    launch_button = console.get("launch", {}).get("button")
    placed = {}
    items = []
    for entry in cabin.get("controls", []):
        control = by_id.get(entry["control"])
        if control is None:
            continue
        style = entry.get("style") or default_style(control, controls)
        placed[control["id"]] = entry
        items.append({
            "what": "control", "id": control["id"], "label": control.get("label", ""), "style": style,
            "panel": entry["panel"], "at": tuple(entry["at_m"]),
            "size": look_size(entry, FOOTPRINT_M[style]),
            "confidence": control.get("confidence", ""), "launch": control["id"] == launch_button,
            "positions": len(control.get("position_labels", [])), "guards": None,
        })
    for control in controls:
        if control["id"] in placed:
            continue
        guarded = [c for c in controls if c.get("guard") == control["id"] and c["id"] in placed]
        if not guarded:
            continue
        entry = placed[guarded[0]["id"]]
        items.append({
            "what": "control", "id": control["id"], "label": control.get("label", ""), "style": "cover",
            "panel": entry["panel"], "at": tuple(entry["at_m"]), "size": FOOTPRINT_M["cover"],
            "confidence": control.get("confidence", ""), "launch": False, "positions": 2,
            "guards": guarded[0]["id"],
        })
    for lamp in cabin.get("lamps", []):
        items.append({
            "what": "lamp", "id": lamp["id"], "label": lamp.get("label", ""), "style": "lamp",
            "panel": lamp["panel"], "at": tuple(lamp["at_m"]), "size": look_size(lamp, FOOTPRINT_M["lamp"]),
            "confidence": lamp.get("confidence", ""), "color": lamp.get("color", "white"),
        })
    for display in cabin.get("displays", []):
        items.append({
            "what": "display", "id": display["id"], "label": display.get("label", ""), "style": display["kind"],
            "panel": display["panel"], "at": tuple(display["at_m"]), "size": tuple(display["size_m"]),
            "confidence": display.get("confidence", ""),
        })
    return items


def _overlap(a, b):
    """True if two footprints overlap. Round items are tested as circles, the rest as rectangles."""
    dx = abs(a["at"][0] - b["at"][0])
    dy = abs(a["at"][1] - b["at"][1])
    def round_(item):
        return item["style"] in ROUND and abs(item["size"][0] - item["size"][1]) < 1e-9

    if round_(a) and round_(b):
        return math.hypot(dx, dy) < (a["size"][0] + b["size"][0]) / 2.0 - 1e-9
    return dx < (a["size"][0] + b["size"][0]) / 2.0 - 1e-9 and dy < (a["size"][1] + b["size"][1]) / 2.0 - 1e-9


def find_problems(console, cabin):
    """Layout problems as text lines: unplaced controls, items over a panel edge, overlapping items."""
    problems = []
    items = build_items(console, cabin)
    panels = {p["id"]: p for p in cabin.get("panels", [])}
    drawn = {i["id"] for i in items if i["what"] == "control"}
    for control in console.get("controls", []):
        if control["id"] not in drawn:
            problems.append(f"control '{control['id']}' is not placed on any panel")
    # Covers are flaps hinged 4.5 cm in front of the face (SimCabinComponent), so they may hang past a column's edge
    # or over a neighbour, as the guards in the samsim_panels photos do; only items on the face are checked.
    on_face = [i for i in items if i["style"] != "cover"]
    for item in items:
        panel = panels.get(item["panel"])
        if panel is None:
            problems.append(f"{item['what']} '{item['id']}' is on unknown panel '{item['panel']}'")
            continue
        half_w, half_h = panel["size_m"][0] / 2.0, panel["size_m"][1] / 2.0
        if item["style"] != "cover" and (abs(item["at"][0]) + item["size"][0] / 2.0 > half_w + 1e-9 or
                abs(item["at"][1]) + item["size"][1] / 2.0 > half_h + 1e-9):
            problems.append(f"{item['what']} '{item['id']}' reaches over the edge of panel '{item['panel']}'")
    for n, a in enumerate(on_face):
        for b in on_face[n + 1:]:
            if a["panel"] != b["panel"]:
                continue
            if _overlap(a, b):
                problems.append(f"'{a['id']}' and '{b['id']}' overlap on panel '{a['panel']}'")
    return problems


# ------------------------------------------------------------------ drawing

def _esc(text):
    return html.escape(str(text), quote=True)


def _conf_class(confidence):
    return "conf-" + (confidence if confidence in CONFIDENCES else "none")


def _draw_item(item, x, y):
    """SVG for one item centred on (x, y), in pixels."""
    w, h = item["size"][0] * PX_PER_M, item["size"][1] * PX_PER_M
    r = w / 2.0
    style = item["style"]
    cls = f"item {style} {_conf_class(item['confidence'])}"
    parts = []
    if style == "toggle":
        parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')
        parts.append(f'<line class="lever" x1="{x:.1f}" y1="{y:.1f}" x2="{x:.1f}" y2="{y - r * 1.5:.1f}"/>')
    elif style == "rotary":
        parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')
        count = max(item.get("positions", 0), 2)
        for k in range(count):
            a = math.radians(-60.0 + 120.0 * k / (count - 1)) if count > 1 else 0.0
            parts.append(f'<line class="tick" x1="{x + math.sin(a) * r * 1.1:.1f}" y1="{y - math.cos(a) * r * 1.1:.1f}" '
                         f'x2="{x + math.sin(a) * r * 1.35:.1f}" y2="{y - math.cos(a) * r * 1.35:.1f}"/>')
        parts.append(f'<line class="lever" x1="{x:.1f}" y1="{y:.1f}" x2="{x:.1f}" y2="{y - r * 0.9:.1f}"/>')
    elif style == "key":
        parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')
        parts.append(f'<rect class="slot" x="{x - r * 0.18:.1f}" y="{y - r * 0.6:.1f}" width="{r * 0.36:.1f}" '
                     f'height="{r * 1.2:.1f}"/>')
    elif style == "button":
        extra = " launch" if item.get("launch") else ""
        parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')
        parts.append(f'<circle class="cap{extra}" cx="{x:.1f}" cy="{y:.1f}" r="{r * 0.68:.1f}"/>')
    elif style == "wheel":
        parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')
        parts.append(f'<circle class="hub" cx="{x:.1f}" cy="{y:.1f}" r="{r * 0.2:.1f}"/>')
        for k in range(3):
            a = math.radians(120.0 * k)
            parts.append(f'<line class="tick" x1="{x:.1f}" y1="{y:.1f}" x2="{x + math.sin(a) * r:.1f}" '
                         f'y2="{y - math.cos(a) * r:.1f}"/>')
    elif style == "cover":
        parts.append(f'<rect class="flap" x="{x - w / 2:.1f}" y="{y - h / 2:.1f}" width="{w:.1f}" height="{h:.1f}" '
                     f'rx="4"/>')
    elif style == "lamp":
        colour = item.get("color", "white")
        parts.append(f'<circle class="glass lamp-{_esc(colour)}" cx="{x:.1f}" cy="{y:.1f}" r="{r * 0.75:.1f}"/>')
    elif style in ("ppi", "dial"):
        parts.append(f'<circle class="screen" cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')
        if style == "ppi":
            for k in (1, 2, 3):
                parts.append(f'<circle class="trace" cx="{x:.1f}" cy="{y:.1f}" r="{r * k / 4:.1f}"/>')
            parts.append(f'<line class="trace" x1="{x:.1f}" y1="{y:.1f}" x2="{x + r * 0.7:.1f}" y2="{y - r * 0.7:.1f}"/>')
        else:
            for k in range(12):
                a = math.radians(30.0 * k)
                parts.append(f'<line class="trace" x1="{x + math.sin(a) * r * 0.8:.1f}" '
                             f'y1="{y - math.cos(a) * r * 0.8:.1f}" x2="{x + math.sin(a) * r * 0.95:.1f}" '
                             f'y2="{y - math.cos(a) * r * 0.95:.1f}"/>')
            parts.append(f'<line class="trace" x1="{x:.1f}" y1="{y:.1f}" x2="{x:.1f}" y2="{y - r * 0.75:.1f}"/>')
    else:  # range_scope, tv, meter and any display kind added later
        parts.append(f'<rect class="screen" x="{x - w / 2:.1f}" y="{y - h / 2:.1f}" width="{w:.1f}" '
                     f'height="{h:.1f}" rx="6"/>')
        if style == "tv":
            parts.append(f'<line class="trace" x1="{x - w * 0.12:.1f}" y1="{y:.1f}" x2="{x + w * 0.12:.1f}" y2="{y:.1f}"/>')
            parts.append(f'<line class="trace" x1="{x:.1f}" y1="{y - h * 0.16:.1f}" x2="{x:.1f}" y2="{y + h * 0.16:.1f}"/>')
        elif style == "range_scope":
            for frac in (-0.2, 0.2):
                parts.append(f'<line class="trace" x1="{x - w * 0.42:.1f}" y1="{y + h * frac:.1f}" '
                             f'x2="{x + w * 0.42:.1f}" y2="{y + h * frac:.1f}"/>')
        elif style == "meter":
            parts.append(f'<path class="trace" d="M {x - w * 0.35:.1f} {y + h * 0.2:.1f} A {w * 0.4:.1f} {w * 0.4:.1f} '
                         f'0 0 1 {x + w * 0.35:.1f} {y + h * 0.2:.1f}"/>')
    if style != "cover":  # a cover shares its control's spot: it is named in the table and the tooltip only
        label_y = y + h / 2.0 + 13.0
        name = item["label"] or item["id"]
        parts.append(f'<text class="label" x="{x:.1f}" y="{label_y:.1f}">{_esc(name)}</text>')
        if item["label"]:
            parts.append(f'<text class="ident" x="{x:.1f}" y="{label_y + 11.0:.1f}">{_esc(item["id"])}</text>')
    title = f"{item['id']} ({style}" + (f", {item['confidence']}" if item["confidence"] else "") + ")"
    return f'<g class="{cls}"><title>{_esc(title)}</title>{"".join(parts)}</g>'


def _draw_panel(panel, items):
    """One panel as an SVG, seen by the operator: u to the right, v up."""
    w, h = panel["size_m"][0] * PX_PER_M, panel["size_m"][1] * PX_PER_M
    width, height = w + 2 * MARGIN_PX, h + 2 * MARGIN_PX + 16.0
    cx, cy = MARGIN_PX + w / 2.0, MARGIN_PX + h / 2.0
    # Displays first, then covers, then the rest, so a control and its label stay readable through its cover.
    ordered = sorted(items, key=lambda i: (i["what"] != "display", i["style"] != "cover"))
    drawn = "".join(_draw_item(i, cx + i["at"][0] * PX_PER_M, cy - i["at"][1] * PX_PER_M) for i in ordered)
    scale = (f'<line class="rule" x1="{MARGIN_PX:.1f}" y1="{height - 8:.1f}" x2="{MARGIN_PX + 0.1 * PX_PER_M:.1f}" '
             f'y2="{height - 8:.1f}"/><text class="ident start" x="{MARGIN_PX + 0.1 * PX_PER_M + 6:.1f}" '
             f'y="{height - 4:.1f}">10 cm</text>')
    return (f'<svg viewBox="0 0 {width:.0f} {height:.0f}" style="max-width:{width:.0f}px" role="img" '
            f'aria-label="{_esc(panel.get("label", panel["id"]))}">'
            f'<rect class="face" x="{MARGIN_PX}" y="{MARGIN_PX}" width="{w:.1f}" height="{h:.1f}" rx="8"/>'
            f'{drawn}{scale}</svg>')


def _positions_cell(control):
    labels = control.get("position_labels", [])
    kind = control.get("kind", "switch")
    if kind == "wheel":
        return _esc(f"{control.get('min', '')} to {control.get('max', '')} {control.get('unit', '')}".strip())
    if kind == "button":
        return "push"
    start = control.get("initial", 0)
    return " / ".join(f"<b>{_esc(l)}</b>" if n == start else _esc(l) for n, l in enumerate(labels))


CSS = """
:root{--bg:#f4f5f2;--ink:#1d2326;--muted:#5d686e;--line:#c9cfcb;--card:#ffffff;--face:#e7e3d6;--edge:#8a8f86;
--metal:#b9bdb8;--dark:#2a2f31;--screen:#16221c;--trace:#7fd49a;--accent:#2f6f8f;--warn:#a8480f;
--high:#2f7d4f;--medium:#2f6f8f;--low:#b07a10;--estimate:#a23b2b}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#15191b;--ink:#e6e9e6;--muted:#9aa5a8;
--line:#333b3e;--card:#1c2224;--accent:#7fb7d3;--warn:#f0a36b}}
:root[data-theme="dark"]{--bg:#15191b;--ink:#e6e9e6;--muted:#9aa5a8;--line:#333b3e;--card:#1c2224;--accent:#7fb7d3;
--warn:#f0a36b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}
main{max-width:1180px;margin:0 auto;padding:24px 16px 64px}
h1{font-size:26px;line-height:1.2;margin:0 0 4px}
h2{font-size:19px;margin:36px 0 4px;padding-top:16px;border-top:1px solid var(--line)}
h3{font-size:15px;margin:18px 0 6px;color:var(--muted);font-weight:600}
p{margin:6px 0;max-width:75ch}
.meta,.note{color:var(--muted);font-size:13.5px}
.problems{border:1px solid var(--warn);border-radius:8px;padding:10px 14px;margin:16px 0;background:var(--card)}
.problems b{color:var(--warn)}
.ok{border-color:var(--line)}
ul{margin:6px 0;padding-left:20px}
.legend{display:flex;flex-wrap:wrap;gap:6px 16px;font-size:13px;color:var(--muted);margin:10px 0}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:5px;vertical-align:-1px}
.panel{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px;margin:8px 0 14px}
svg{display:block;width:100%;height:auto}
.wrap{overflow-x:auto}
table{border-collapse:collapse;width:100%;font-size:13px;background:var(--card)}
th,td{text-align:left;vertical-align:top;padding:5px 8px;border-bottom:1px solid var(--line)}
th{color:var(--muted);font-weight:600;white-space:nowrap}
code,.mono{font:12.5px ui-monospace,Consolas,monospace}
img.ref{display:block;max-width:100%;border-radius:6px;margin-top:8px}
.face{fill:var(--face);stroke:var(--edge);stroke-width:1.5}
.item circle,.item rect{fill:var(--metal);stroke:var(--dark);stroke-width:1.5}
.item .lever{stroke:var(--dark);stroke-width:5;stroke-linecap:round}
.item .tick{stroke:var(--dark);stroke-width:1.5}
.item .slot,.item .hub{fill:var(--dark)}
.item .cap{fill:#d9dcd8}
.item .cap.launch{fill:#b3261e}
.item .flap{fill:#b3261e22;stroke:#b3261e;stroke-dasharray:5 4}
.item .screen{fill:var(--screen);stroke:var(--dark);stroke-width:2}
.item .trace{fill:none;stroke:var(--trace);stroke-width:1.2}
.item .glass{stroke:var(--dark);stroke-width:1.5}
.item .lamp-green{fill:#3fae5f}.item .lamp-red{fill:#cf3a2c}.item .lamp-amber{fill:#e0a020}
.item .lamp-white{fill:#f4f4ee}
.label{font:600 11px system-ui,sans-serif;fill:#1d2326;text-anchor:middle}
.ident{font:9px ui-monospace,Consolas,monospace;fill:#4d575b;text-anchor:middle}
.ident.start{text-anchor:start}
.rule{stroke:#4d575b;stroke-width:2}
.conf-high .ident{fill:var(--high)}.conf-medium .ident{fill:var(--medium)}
.conf-low .ident{fill:var(--low)}.conf-estimate .ident{fill:var(--estimate)}
"""


def render_html(console, cabin, refs=None):
    """The whole page as a string. `refs` maps panel ids to image URIs shown under the panel."""
    refs = refs or {}
    items = build_items(console, cabin)
    problems = find_problems(console, cabin)
    by_id = {c["id"]: c for c in console.get("controls", [])}
    out = []
    name = console.get("display_name", console.get("system_id", ""))
    out.append(f"<h1>{_esc(name)}: panel sheet</h1>")
    counts = (sum(1 for i in items if i["what"] == "control"), len(cabin.get("lamps", [])),
              len(cabin.get("displays", [])), len(cabin.get("panels", [])), len(cabin.get("stations", [])))
    out.append(f'<p class="meta">{counts[0]} controls, {counts[1]} lamps and {counts[2]} displays on {counts[3]} '
               f'panels at {counts[4]} seats. Panels are drawn flat, to scale, as the operator sees them.</p>')
    if cabin.get("status"):
        out.append(f'<p class="note">Layout status: {_esc(cabin["status"])}</p>')
    if problems:
        rows = "".join(f"<li>{_esc(p)}</li>" for p in problems)
        out.append(f'<div class="problems"><b>{len(problems)} layout problem(s)</b><ul>{rows}</ul></div>')
    else:
        out.append('<div class="problems ok">No layout problems: every control is placed, nothing overlaps and '
                   'nothing reaches over a panel edge.</div>')
    dots = "".join(f'<span><span class="dot" style="background:var(--{c})"></span>{c}</span>' for c in CONFIDENCES)
    out.append(f'<div class="legend"><span>Id colour = confidence of the source:</span>{dots}'
               '<span>Dashed red = cover or lock over a control. Bold position = start position.</span></div>')
    for station in cabin.get("stations", []):
        out.append(f'<h2>{_esc(station.get("label", station["id"]))} <span class="mono meta">{_esc(station["id"])}'
                   '</span></h2>')
        panels = [p for p in cabin.get("panels", []) if p.get("station") == station["id"]]
        if not panels:
            out.append('<p class="note">No panels at this seat.</p>')
        for panel in panels:
            on_panel = [i for i in items if i["panel"] == panel["id"]]
            size = f'{panel["size_m"][0] * 100:.0f} x {panel["size_m"][1] * 100:.0f} cm'
            out.append(f'<h3>{_esc(panel.get("label", panel["id"]))} <span class="mono">{_esc(panel["id"])}</span>, '
                       f'{size}</h3>')
            out.append(f'<div class="panel">{_draw_panel(panel, on_panel)}')
            if panel["id"] in refs:
                out.append(f'<img class="ref" src="{_esc(refs[panel["id"]])}" alt="Reference photo for '
                           f'{_esc(panel["id"])}">')
            out.append("</div>")
            rows = []
            for item in on_panel:
                if item["what"] == "control":
                    control = by_id[item["id"]]
                    rows.append((item["id"], control.get("label", ""), item["style"], _positions_cell(control),
                                 control.get("source", ""), control.get("confidence", ""), control.get("notes", "")))
            for lamp in cabin.get("lamps", []):
                if lamp["panel"] == panel["id"]:
                    rows.append((lamp["id"], lamp.get("label", ""), f'lamp, {lamp.get("color", "")}', "",
                                 lamp.get("source", ""), lamp.get("confidence", ""), lamp.get("notes", "")))
            for display in cabin.get("displays", []):
                if display["panel"] == panel["id"]:
                    rows.append((display["id"], display.get("label", ""), f'display, {display["kind"]}', "",
                                 display.get("source", ""), display.get("confidence", ""), display.get("notes", "")))
            body = "".join(
                f'<tr><td class="mono">{_esc(r[0])}</td><td>{_esc(r[1])}</td><td>{_esc(r[2])}</td><td>{r[3]}</td>'
                f'<td>{_esc(r[4])}</td><td>{_esc(r[5])}</td><td>{_esc(r[6])}</td></tr>' for r in rows)
            out.append('<div class="wrap"><table><thead><tr><th>Id</th><th>Label</th><th>Kind</th><th>Positions</th>'
                       f'<th>Source</th><th>Confidence</th><th>Notes</th></tr></thead><tbody>{body}</tbody></table></div>')
    title = _esc(f"{console.get('system_id', 'system')} panel sheet")
    return ('<!doctype html><html lang="en"><head><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width, initial-scale=1">'
            f'<title>{title}</title><style>{CSS}</style></head><body><main>{"".join(out)}</main></body></html>')


def load_refs(path):
    """Panel id -> image URI, from a refs file. Missing images are skipped."""
    path = Path(path)
    with open(path, encoding="utf-8") as f:
        raw = json.load(f)
    refs = {}
    for panel_id, image in raw.items():
        target = (path.parent / image).resolve()
        if target.is_file():
            refs[panel_id] = target.as_uri()
    return refs


def main(argv=None):
    parser = argparse.ArgumentParser(description="Draw a system's cabin panels as one HTML page.")
    parser.add_argument("system", nargs="?", default="osa_ak")
    parser.add_argument("--out")
    parser.add_argument("--refs")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    console, cabin = load_system(args.system)
    refs = load_refs(args.refs) if args.refs else {}
    out = Path(args.out) if args.out else ROOT / "out" / "panel_sheets" / f"{args.system}.html"
    os.makedirs(out.parent, exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        f.write(render_html(console, cabin, refs))
    problems = find_problems(console, cabin)
    print(f"Wrote {out}")
    for problem in problems:
        print(f"  problem: {problem}")
    return 1 if (args.check and problems) else 0


if __name__ == "__main__":
    sys.exit(main())
