"""The photo-traced faces and the cabin layout draw each part once (docs/MODELLING_FROM_PHOTOS.md section 7).

A face's mesh (tools/blender/<face>_model.py) models every part of its trace; the cabin (data/cabins/osa_ak.json, drawn by
SimCabinComponent) draws the controls and lamps the game works. Where the layout puts one on a traced part, the trace marks the
part `game` with the layout's id and the mesh leaves it out; a place the trace never modelled is a `CONTROL_MM` entry. So:
every layout control or lamp on the face stands on its own `game` part or `CONTROL_MM` place, or on bare plate; every `game`
part has its item; and no item stands on a part the mesh still draws (a switch drawn over a switch, a lamp over a jack).
"""
import importlib
import json
import math
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))

LAYOUT = json.loads((REPO / "data" / "cabins" / "osa_ak.json").read_text(encoding="utf-8"))

# panel id -> trace module
FACES = {"gyro_panel": "sl1m_trace", "sp2m_panel": "sp2m_trace", "sl3m_panel": "sl3m_trace", "sp1m_panel": "sp1m_trace",
         "sp3m_panel": "sp3m_trace"}
# parts an item may stand on: plates and fields that carry other parts (the OR-11M's field of openings holds its lamps)
CARRIERS = {"black_panel", "sockets", "plate", "nameplate", "latch_plate", "data_plate", "bar", "divider", "block", "frame",
            "dial_block", "label", "text", "rule", "panel", "cover_frame"}
TOL_MM = 0.6


def _items(panel):
    """The layout's controls and lamps on the panel that the cabin draws: (id, (a, b) m)."""
    out = [(c["control"], c["at_m"]) for c in LAYOUT["controls"]
           if c["panel"] == panel and c.get("look", {}).get("kind") != "in_mesh" and "flap" not in c]
    out += [(lamp["id"], lamp["at_m"]) for lamp in LAYOUT["lamps"] if lamp["panel"] == panel]
    return out


def _to_mm(T, a, b):
    """The inverse of the trace's to_m (an affine map)."""
    o = T.to_m(0.0, 0.0)
    ex = T.to_m(1.0, 0.0)
    ey = T.to_m(0.0, 1.0)
    m = ((ex[0] - o[0], ey[0] - o[0]), (ex[1] - o[1], ey[1] - o[1]))
    det = m[0][0] * m[1][1] - m[0][1] * m[1][0]
    da, db = a - o[0], b - o[1]
    return ((m[1][1] * da - m[0][1] * db) / det, (-m[1][0] * da + m[0][0] * db) / det)


def _centre(p):
    if "c" in p:
        return tuple(p["c"])
    r = p.get("r") or p.get("rect")
    return ((r[0] + r[2]) / 2.0, (r[1] + r[3]) / 2.0) if r else None


def _covers(p, x, y):
    """Whether the part's footprint on the face covers the point (mm)."""
    if "c" in p and "d" in p:
        return math.dist(p["c"], (x, y)) < p["d"] / 2.0
    if "c" in p and "size" in p:
        w, h = p["size"]
        return abs(x - p["c"][0]) < w / 2.0 and abs(y - p["c"][1]) < h / 2.0
    r = p.get("r") or p.get("rect")
    if r:
        return min(r[0], r[2]) < x < max(r[0], r[2]) and min(r[1], r[3]) < y < max(r[1], r[3])
    if "c" in p:            # a toggle or a key traced by its centre only
        return math.dist(p["c"], (x, y)) < 6.0
    return False


def _trace(panel):
    T = importlib.import_module(FACES[panel])
    return T, (T.parts() if hasattr(T, "parts") else [])


@pytest.mark.parametrize("panel", sorted(FACES))
def test_every_game_part_has_its_item_at_its_centre(panel):
    T, parts = _trace(panel)
    items = dict(_items(panel))
    for p in parts:
        if not p.get("game"):
            continue
        assert p["game"] in items, (panel, p["game"], "no such control or lamp on the panel")
        x, y = _to_mm(T, *items[p["game"]])
        assert math.dist((x, y), _centre(p)) <= TOL_MM, (panel, p["game"], (round(x, 1), round(y, 1)), _centre(p))


@pytest.mark.parametrize("panel", sorted(FACES))
def test_no_item_stands_on_a_part_the_mesh_draws(panel):
    T, parts = _trace(panel)
    places = getattr(T, "CONTROL_MM", {})
    for item, at in _items(panel):
        if item in places:
            continue                    # the trace's own test keeps its places clear
        x, y = _to_mm(T, *at)
        for p in parts:
            if p.get("game") or p["kind"] in CARRIERS or p["kind"] == "screw":
                continue
            assert not _covers(p, x, y), (panel, item, (round(x, 1), round(y, 1)), p["kind"], _centre(p))


@pytest.mark.parametrize("panel", sorted(FACES))
def test_the_places_are_where_the_layout_puts_them(panel):
    T, _ = _trace(panel)
    items = dict(_items(panel))
    for item, (x, y) in getattr(T, "CONTROL_MM", {}).items():
        if item in items:
            a, b = T.to_m(x, y)
            assert math.dist((a, b), items[item]) <= TOL_MM / 1000.0, (panel, item, (a, b), items[item])
