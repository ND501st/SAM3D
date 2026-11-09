"""SP-1M's trace (tools/blender/sp1m_trace.py) holds together: five blocks in two columns, every part on its block, no two parts in one place, the flaps clear of what stands beside them."""
import itertools
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import sp1m_trace as T  # noqa: E402


def _centre(p):
    if "c" in p:
        return p["c"]
    r = p["r"]
    return ((r[0] + r[2]) / 2, (r[1] + r[3]) / 2)


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
    round_ = [p for p in T.parts() if "c" in p and "d" in p]
    for a, b in itertools.combinations(round_, 2):
        gap = math.dist(a["c"], b["c"]) - (a["d"] + b["d"]) / 2
        assert gap > -2.0, (a["kind"], a["c"], b["kind"], b["c"], gap)


def test_round_parts_clear_the_rectangles_they_do_not_stand_on():
    rects = [p for p in T.parts() if "r" in p and p["kind"] != "latch_plate"]
    for c in T.parts():
        if "c" not in c:
            continue
        for p in rects:
            r = p["r"]
            dx = max(r[0] - c["c"][0], 0.0, c["c"][0] - r[2])
            dy = max(r[1] - c["c"][1], 0.0, c["c"][1] - r[3])
            assert math.hypot(dx, dy) >= c["d"] / 2 - 2.0, (c["kind"], c["c"], p["kind"], r)


def test_trimmers_top_ring_stands_clear_above_its_collar():
    for p in T.parts("pot"):
        assert p["top"][1] < p["c"][1]
        assert math.dist(p["top"], p["c"]) > (p["d"] - p["top_d"]) / 2


def test_the_seven_lenses_each_have_a_lamp_and_a_point():
    assert len(T.parts("dome", "R1")) == 7
    assert len(T.parts("lamp", "R1")) == 7
    assert len(T.parts("dot", "R1")) == 7


def test_the_frame_and_channels_clear_the_blocks():
    assert T.FRAME_LEFT[1] <= min(v[0][0] for v in T.BLOCKS.values())
    assert T.FRAME_RIGHT[0] >= T.W_MM
    assert T.FRAME_TOP[1] <= 0.0 and T.FRAME_BOTTOM[0] >= T.H_MM
    assert max(T.BLOCKS[k][0][2] for k in ("L1", "L2", "L3")) <= T.DIVIDER[0]
    assert T.DIVIDER[1] <= min(T.BLOCKS[k][0][0] for k in ("R1", "R2"))
    for (x0, x1), (y0, y1) in T.RAILS:
        assert x1 > x0 and y1 > y0


def test_rivets_lie_on_the_frame_not_on_a_block():
    for x, y in T.RIVETS:
        for r, _ in T.BLOCKS.values():
            assert not (r[0] + 4.0 < x < r[2] - 4.0 and r[1] + 4.0 < y < r[3] - 4.0), (x, y)


def test_the_rack_is_the_size_of_its_neighbour():
    assert abs(T.W_MM - 1275.0) < 1.0
    assert abs(T.H_MM - 554.0) < 5.0
    assert abs(T.W_MM - 1310.0) < 50.0
