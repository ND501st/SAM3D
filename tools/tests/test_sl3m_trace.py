"""SŁ-3M's trace (tools/blender/sl3m_trace.py) holds together: six blocks tile two columns, every part lies on its block, no two parts
stand in one place, and the drawing's fit to the photos gives what the docs say."""
import itertools
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import sl3m_trace as T  # noqa: E402

LOOSE = {"plug"}          # plugs stand out of the connector plate


def _centre(p):
    if "c" in p:
        return p["c"]
    r = p["r"]
    return ((r[0] + r[2]) / 2, (r[1] + r[3]) / 2)


def test_six_blocks_in_two_columns_without_overlapping():
    assert len(T.BLOCKS) == 6
    rects = [v[0] for v in T.BLOCKS.values()]
    for a, b in itertools.combinations(rects, 2):
        assert min(a[2], b[2]) - max(a[0], b[0]) <= 0.0 or min(a[3], b[3]) - max(a[1], b[1]) <= 0.0, (a, b)
    for r in rects:
        assert r[2] - r[0] == 600.0
        assert 160.0 <= r[3] - r[1] <= 182.0, r
        assert r[2] <= T.W_MM and r[3] <= T.H_MM


def test_every_block_has_its_cabin_id():
    assert set(T.BLOCK_IDS.values()) == set(T.BLOCKS)


def test_every_part_lies_on_its_block():
    for p in T.parts():
        x, y = _centre(p)
        r = T.BLOCKS[p["block"]][0]
        assert r[0] - 2.0 <= x <= r[2] + 2.0 and r[1] - 2.0 <= y <= r[3] + 2.0, (p["kind"], p["block"], x, y, r)


def test_no_two_round_parts_stand_in_one_place():
    skip = {"fuse_cap", "lock"}
    round_ = [p for p in T.parts() if "c" in p and "d" in p and p["kind"] not in skip and not p.get("on_fuse")]
    for a, b in itertools.combinations(round_, 2):
        if a["kind"] == "plug" and b["kind"] == "plug":
            assert math.dist(a["c"], b["c"]) >= (a["d"] + b["d"]) / 2, (a["c"], b["c"])
            continue
        gap = math.dist(a["c"], b["c"]) - (a["d"] + b["d"]) / 2
        assert gap > -2.0, (a["kind"], a["c"], b["kind"], b["c"], gap)


def test_the_fuses_do_not_run_into_the_jacks_below_them():
    for f in T.parts("fuse_base"):
        for j in T.parts("jack", f["block"]):
            dx = abs(f["c"][0] - j["c"][0]) - f["w"] / 2 - j["d"] / 2
            dy = abs(f["c"][1] - j["c"][1]) - f["ht"] / 2 - j["d"] / 2
            assert dx > -1.0 or dy > -1.0, (f["c"], j["c"])


def test_the_connector_plate_lies_inside_its_block_and_clear_of_the_door():
    plate = T.parts("conn_plate")[0]["r"]
    block = T.BLOCKS["A3"][0]
    assert block[0] < plate[0] < plate[2] < block[2] and block[1] < plate[1] < plate[3] < block[3]
    assert plate[2] < T.DOOR_HINGE[0]


def test_the_fit_matches_the_photos_reads():
    # read on g099-b flattened, mm: the seven dark jacks of the top block, x from the drawing's fit
    photo = (232.5, 276.7, 318.3, 361.7, 405.0, 448.3, 491.7)
    drawing = (404, 463, 520, 580, 640, 700, 762)
    for r, px in zip(photo, drawing):
        assert abs(T.fx(px) - (r - 20.0)) < 12.0, (px, r)


def test_the_face_is_smaller_than_figure_three():
    assert 1.15 < T.W_MM / 1000.0 < 1.30 and 0.50 < T.H_MM / 1000.0 < 0.60
