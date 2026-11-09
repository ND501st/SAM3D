"""SP-3M-2's trace (tools/blender/sp3m_trace.py) holds together: the blocks stay in their rows, every part lies on its block, no two
parts stand in one place, and the flattening's measured facts are what the docs say they are.

The model is built straight from the trace, so a screw drawn inside a lamp or a block run into its neighbour would show in the
cabin as parts running into each other.
"""
import itertools
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import sp3m_trace as T  # noqa: E402

# Parts that may reach past their block's edge: the cover's hinges, and plugs hung out of the plate.
LOOSE = {"coax", "coax_flange", "elbow", "hinge", "handle"}
# A round part may stand on these: the fuse base carries its cap and lamp, the cover its seal, the meter its screw, the cube its plate.
CARRIERS = {"fuse_base", "cover", "meter", "cube", "chart", "data_plate"}


def _round():
    return [p for p in T.parts() if "c" in p and "d" in p]


def _centre(p):
    if "c" in p:
        return p["c"]
    if "r" in p:
        r = p["r"]
        return ((r[0] + r[2]) / 2, (r[1] + r[3]) / 2)
    return ((p["a"][0] + p["b"][0]) / 2, (p["a"][1] + p["b"][1]) / 2)


def test_blocks_sit_in_their_rows_without_overlapping():
    lower = [v[0] for k, v in T.BLOCKS.items() if v[1] == "L"]
    for a, b in itertools.combinations(lower, 2):
        assert min(a[2], b[2]) - max(a[0], b[0]) <= 1.0 or min(a[3], b[3]) - max(a[1], b[1]) <= 1.0, (a, b)
    assert T.COVER[0] > T.PLATE[2], "the drawer stands to the right of the white panel"
    for name, (r, plane, _) in T.BLOCKS.items():
        assert r[0] < r[2] and r[1] < r[3], name
        assert plane in "UL"
    for r in lower:
        assert r[1] > T.PLATE[3], "the instrument blocks are below the white panel"
        assert 250.0 <= r[3] - r[1] <= 345.0, r


def test_the_window_lies_inside_the_drawers_cover():
    w, c = T.WINDOW, T.COVER
    assert c[0] < w[0] < w[2] < c[2] and c[1] < w[1] < w[3] < c[3]


def test_the_lower_row_is_behind_the_face():
    assert 130.0 < T.D_LOWER < 165.0


def test_every_block_has_its_cabin_id():
    assert len(T.BLOCK_IDS) == 7
    a = T.BLOCK_IDS["sp3_12rzm"]
    b = T.BLOCK_IDS["sp3_6rzm_2"]
    assert a[2] < b[0]
    for k, r in T.BLOCK_IDS.items():
        assert -6.0 <= r[0] and r[2] <= T.W_MM + 160.0 and -2.0 <= r[1] and r[3] <= T.H_MM + 20.0, k


def test_the_flap_closes_the_recess_below_the_white_panel():
    assert T.FLAP[1] > T.PLATE[3]
    assert T.OPENING[1] <= T.FLAP[1] and T.FLAP[3] <= T.OPENING[3]
    assert T.OPENING[0] <= T.FLAP[0] + 1.0


def test_every_part_lies_on_its_block():
    for p in T.parts():
        if p["kind"] in LOOSE:
            continue
        x, y = _centre(p)
        r = T.BLOCKS[p["block"]][0]
        assert r[0] - 6.0 <= x <= r[2] + 6.0 and r[1] - 6.0 <= y <= r[3] + 6.0, (p["kind"], p["block"], x, y, r)


def test_no_two_round_parts_stand_in_one_place():
    for a, b in itertools.combinations(_round(), 2):
        if a["plane"] != b["plane"] or a["kind"] == "fuse_cap" or b["kind"] == "fuse_cap":
            continue
        if a.get("on_fuse") or b.get("on_fuse"):
            continue
        gap = math.dist(a["c"], b["c"]) - (a["d"] + b["d"]) / 2
        assert gap > -6.0, (a["kind"], a["c"], b["kind"], b["c"], gap)


def test_round_parts_clear_the_rectangles_they_are_not_mounted_on():
    rects = [p for p in T.parts() if "r" in p and p["kind"] not in CARRIERS and p["kind"] != "cube"]
    for c in _round():
        if c["kind"] in LOOSE:
            continue
        for p in rects:
            if p["plane"] != c["plane"]:
                continue
            r = p["r"]
            dx = max(r[0] - c["c"][0], 0.0, c["c"][0] - r[2])
            dy = max(r[1] - c["c"][1], 0.0, c["c"][1] - r[3])
            assert math.hypot(dx, dy) >= c["d"] / 2 - 8.0, (c["kind"], c["c"], p["kind"], r)


def test_parallax_walks_a_tall_part_back_to_its_foot():
    # a knob 70 mm proud, read at x 1326 on g098-c, has its foot near the drawing's place (F 976 to 985)
    knob = T.parts("knob_ribbed")[0]
    assert 960.0 < knob["c"][0] < 995.0
    # and a flat part is not moved
    a = T.read("c", "L", 500.0, 400.0, 0.0)
    b = T.rolled(500.0 - 2.7, 400.0 + 2.6)
    assert math.dist(a, b) < 1e-6


def test_the_two_photos_agree_on_the_chart():
    # the chart's corner read on each photo (b plus and c minus half the difference)
    a = T.read("b", "L", 442.0, 361.0, 0.0)
    b = T.read("c", "L", 447.3, 356.0, 0.0)
    assert math.dist(a, b) < 3.0
