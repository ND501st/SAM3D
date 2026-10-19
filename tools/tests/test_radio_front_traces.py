"""The PU-12M's radio fronts' 3D traces (tools/blender/r111_trace.py, r407_trace.py) hold together: nothing on a front
overlaps what it shouldn't, and the cabin's controls stand where the set's are.

The models are built straight from the traces (tools/blender/radio_front_model.py), so a screw drawn on a knob, two knobs
that collide or a label across a connector would show in the cabin as parts running into each other.
"""
import importlib
import math
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))

TRACES = ["r111_trace", "r407_trace"]
SCREW_R = 2.6
# The font's width per character as a fraction of the cap height (Arial Narrow, the model's font): generous.
CHAR_W = 0.62


def _trace(name):
    try:
        return importlib.import_module(name)
    except ImportError:
        pytest.skip(name + " not written yet")


def _inside(pt, rect, grow=0.0):
    x0, y0, x1, y1 = rect
    return x0 - grow <= pt[0] <= x1 + grow and y0 - grow <= pt[1] <= y1 + grow


def _point_in_poly(pt, poly):
    x, y = pt
    inside = False
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            inside = not inside
    return inside


def _circle_hits_poly(c, r, poly):
    """A circle overlaps a polygon: its centre inside, or an edge nearer than r."""
    if _point_in_poly(c, poly):
        return True
    for i in range(len(poly)):
        (x0, y0), (x1, y1) = poly[i], poly[(i + 1) % len(poly)]
        dx, dy = x1 - x0, y1 - y0
        t = max(0.0, min(1.0, ((c[0] - x0) * dx + (c[1] - y0) * dy) / (dx * dx + dy * dy)))
        if math.hypot(c[0] - (x0 + t * dx), c[1] - (y0 + t * dy)) < r:
            return True
    return False


def _text_box(t):
    text, (x, y), h, rot, _, _ = t
    w = CHAR_W * h * len(text)
    if abs(rot) == 90:
        return (x - h / 2, y - w / 2, x + h / 2, y + w / 2)
    return (x - w / 2, y - h / 2, x + w / 2, y + h / 2)


@pytest.mark.parametrize("name", TRACES)
def test_everything_lies_on_the_front(name):
    T = _trace(name)
    for p in T.ROUND:
        assert _inside(p["c"], T.PLATE), p["id"]
    for reg in T.REGIONS:
        for pt in T.region_points(reg):
            assert _inside(pt, T.PLATE, 0.5), (reg["id"], pt)
    for c in list(T.CONTROL_MM.values()) + list(T.LAMP_MM.values()):
        assert _inside(c, T.PLATE), c
    for s, _ in T.SCREWS:
        assert _inside(s, T.PLATE), s
    for t in T.TEXT:
        x0, y0, x1, y1 = _text_box(t)
        assert _inside((x0, y0), T.PLATE, 2.0) and _inside((x1, y1), T.PLATE, 2.0), t[0]


@pytest.mark.parametrize("name", TRACES)
def test_round_parts_do_not_collide(name):
    T = _trace(name)
    parts = T.ROUND
    for i, a in enumerate(parts):
        for b in parts[i + 1:]:
            d = math.hypot(a["c"][0] - b["c"][0], a["c"][1] - b["c"][1])
            assert d > a["r"] + b["r"], (a["id"], b["id"], round(d, 1))
    for x, y, r in T.HOLES:
        for p in parts:
            assert math.hypot(x - p["c"][0], y - p["c"][1]) > r + p["r"], (p["id"], (x, y))


@pytest.mark.parametrize("name", TRACES)
def test_round_parts_stand_on_their_base_and_clear_of_other_raised_parts(name):
    """A round part overlaps only the region it stands on (the one holding its centre) and the ones listed as touching."""
    T = _trace(name)
    allowed = {frozenset(p) for p in getattr(T, "TOUCHING", ())}
    for p in T.ROUND:
        for reg in T.REGIONS:
            poly = T.region_points(reg)
            if _point_in_poly(p["c"], poly) or frozenset((p["id"], reg["id"])) in allowed:
                continue
            assert not _circle_hits_poly(p["c"], p["r"], poly), (p["id"], reg["id"])


@pytest.mark.parametrize("name", TRACES)
def test_screws_are_clear_of_knobs_openings_and_each_other(name):
    T = _trace(name)
    screws = [s for s, _ in T.SCREWS]
    for s in screws:
        for p in T.ROUND:
            assert math.hypot(s[0] - p["c"][0], s[1] - p["c"][1]) > p["r"] + SCREW_R * 0.5, (s, p["id"])
        for x, y, r in T.HOLES:
            assert math.hypot(s[0] - x, s[1] - y) > r + SCREW_R, s
    for i, a in enumerate(screws):
        for b in screws[i + 1:]:
            assert math.hypot(a[0] - b[0], a[1] - b[1]) > 2 * SCREW_R, (a, b)


@pytest.mark.parametrize("name", TRACES)
def test_screws_sit_on_the_surface_they_name(name):
    T = _trace(name)
    for s, key in T.SCREWS:
        if any(r["id"] == key for r in T.REGIONS):
            assert _point_in_poly(s, T.region_points(T.region(key))), (s, key)
        elif key == "frame":
            frames = [r for r in T.REGIONS if r["id"].endswith("frame")]
            assert any(_point_in_poly(s, T.region_points(r)) for r in frames), s


@pytest.mark.parametrize("name", TRACES)
def test_letters_are_clear_of_knobs_and_connectors(name):
    T = _trace(name)
    for t in T.TEXT:
        x0, y0, x1, y1 = _text_box(t)
        box = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
        for p in T.ROUND:
            if p.get("kind") in ("hatch",) or p["id"] in getattr(T, "TEXT_ON", {}).get(t[0], ()):
                continue
            assert not _circle_hits_poly(p["c"], p["r"] * 0.98, box), (t[0], p["id"])


@pytest.mark.parametrize("name", TRACES)
def test_the_cabin_controls_stand_on_their_parts(name):
    T = _trace(name)
    sims = {p["sim"]: p for p in T.ROUND if p.get("sim")}
    for cid, c in T.CONTROL_MM.items():
        if cid in sims:
            assert math.hypot(c[0] - sims[cid]["c"][0], c[1] - sims[cid]["c"][1]) < 0.5, cid
    for cid in sims:
        assert cid in T.CONTROL_MM, cid
    for lid, c in T.LAMP_MM.items():
        holders = [p for p in T.ROUND if p.get("lamp") == lid]
        on_holder = any(math.hypot(c[0] - p["c"][0], c[1] - p["c"][1]) < 0.5 for p in holders)
        assert on_holder or any(_inside(c, l["rect"]) for l in T.LAMPS) or             any(_inside(c, r["rect"]) for r in T.REGIONS if "rect" in r), lid


@pytest.mark.parametrize("name", TRACES)
def test_outline_is_closed_and_its_area_right(name):
    T = _trace(name)
    area = T.polygon_area(T.OUTLINE) + (T.polygon_area(T.STRIP_OUTLINE) if T.STRIP_OUTLINE else 0.0)
    assert abs(area - T.W_MM * T.H_MM) < 0.01 * T.W_MM * T.H_MM, area
