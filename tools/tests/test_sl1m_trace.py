"""SŁ-1M's trace (tools/blender/sl1m_trace.py) holds together: five blocks in two columns, every part on its block, no two parts in one place, the game's controls on their blocks and clear of the model's parts."""
import itertools
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import sl1m_trace as T  # noqa: E402

CARRIED = {"black_panel"}        # toggles and jacks stand on it


def _centre(p):
    if "c" in p:
        return p["c"]
    r = p["r"]
    return ((r[0] + r[2]) / 2, (r[1] + r[3]) / 2)


def _block_of(x, y):
    for name, (r, _) in T.BLOCKS.items():
        if r[0] <= x <= r[2] and r[1] <= y <= r[3]:
            return name
    return None


def test_five_blocks_do_not_overlap():
    assert len(T.BLOCKS) == 5
    rects = [v[0] for v in T.BLOCKS.values()]
    for a, b in itertools.combinations(rects, 2):
        assert min(a[2], b[2]) - max(a[0], b[0]) <= 0.0 or min(a[3], b[3]) - max(a[1], b[1]) <= 0.0, (a, b)
    assert max(r[2] for r in rects) == T.W_MM and max(r[3] for r in rects) == T.H_MM


def test_every_block_has_its_cabin_id():
    assert set(T.BLOCK_IDS.values()) == set(T.BLOCKS)


def test_every_part_lies_on_its_block():
    for p in T.parts():
        x, y = _centre(p)
        r = T.BLOCKS[p["block"]][0]
        assert r[0] - 2.0 <= x <= r[2] + 2.0 and r[1] - 2.0 <= y <= r[3] + 2.0, (p["kind"], p["block"], x, y, r)


def test_no_two_round_parts_stand_in_one_place():
    skip = {"latch", "button"}
    round_ = [p for p in T.parts() if "c" in p and "d" in p and p["kind"] not in skip]
    for a, b in itertools.combinations(round_, 2):
        gap = math.dist(a["c"], b["c"]) - (a["d"] + b["d"]) / 2
        assert gap > -4.0, (a["kind"], a["c"], b["kind"], b["c"], gap)


def test_round_parts_clear_the_rectangles_they_do_not_stand_on():
    rects = [p for p in T.parts() if "r" in p and p["kind"] not in CARRIED and p["kind"] != "latch_plate"]
    for c in T.parts():
        if "c" not in c or c["kind"] == "clamp":
            continue
        for p in rects:
            r = p["r"]
            dx = max(r[0] - c["c"][0], 0.0, c["c"][0] - r[2])
            dy = max(r[1] - c["c"][1], 0.0, c["c"][1] - r[3])
            assert math.hypot(dx, dy) >= c["d"] / 2 - 4.0, (c["kind"], c["c"], p["kind"], r)


def test_the_game_controls_lie_on_0005_8m():
    for name, (x, y) in T.CONTROL_MM.items():
        assert _block_of(x, y) == "R1", name


def test_no_model_part_stands_on_a_game_control():
    for name, c in T.CONTROL_MM.items():
        for p in T.parts():
            if "c" in p and "d" in p and p["kind"] != "screw":
                assert math.dist(p["c"], c) >= p["d"] / 2 + 5.0, (name, p["kind"], p["c"])
    # nor on the dial's face
    for p in T.parts(block="R1"):
        assert math.dist(_centre(p), T.DIAL_C) >= T.DIAL_R, (p["kind"], _centre(p))


def test_the_rack_fits_figure_three():
    assert abs(T.W_MM - 1310.0) < 50.0
    r = T.BLOCKS["R1"][0]
    assert abs((r[2] - r[0]) - 600.0) < 15.0
