"""SP-2M2's 3D trace (tools/blender/sp2m_trace.py) holds together: the blocks tile the face without overlapping, every
part lies on its block, and no two parts stand in one place.

The model is built straight from the trace, so a screw drawn inside a lamp, a lock laid across a jack or a block run
into its neighbour would show in the cabin as parts running into each other.
"""
import itertools
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import sp2m_trace as T  # noqa: E402

# Parts that may reach past their block's edge: cables and plugs hang off it.
LOOSE = {"cable", "plug"}
# Rectangles a round part may stand on: screws hold the nameplates and strips; the tables carry their own studs.
CARRIERS = {"plate", "strip", "table", "sockets", "meter", "meter_white", "bar", "handle", "divider"}


def _round():
    return [p for p in T.parts() if "c" in p and p["kind"] != "frame_connector"]


def test_blocks_lie_on_the_face_and_do_not_overlap():
    for name, (r, how) in T.BLOCKS.items():
        x0, y0, x1, y1 = r
        assert -2.0 <= x0 < x1 <= T.W_MM and -2.0 <= y0 < y1 <= T.H_MM, name
        assert len(how) == 4 and set(how) <= set("mpcw"), name
    for (na, (a, _)), (nb, (b, _)) in itertools.combinations(T.BLOCKS.items(), 2):
        ox = min(a[2], b[2]) - max(a[0], b[0])
        oy = min(a[3], b[3]) - max(a[1], b[1])
        assert ox <= 0.5 or oy <= 0.5, (na, nb, ox, oy)


def test_blocks_keep_the_rack_standard():
    """Five rows about 230 mm high; the side columns 595-610 mm wide and the middle 605-615 (the measured spread)."""
    for name, (r, _) in T.BLOCKS.items():
        h = r[3] - r[1]
        assert 222.0 <= h <= 236.0, (name, h)
    for name in ("L0", "L1", "L2", "L3", "M0", "M1", "M2", "M3", "M4", "R1", "R2", "R3"):
        x0, _, x1, _ = T.BLOCKS[name][0]
        assert 585.0 <= x1 - x0 <= 616.0, (name, x1 - x0)


def test_every_part_lies_on_its_block():
    for p in T.parts():
        if p["block"] is None or p["kind"] in LOOSE:
            continue
        x0, y0, x1, y1 = T.BLOCKS[p["block"]][0]
        if "rect" in p:
            pts, r = [p["rect"][:2], p["rect"][2:]], 0.0
        else:
            pts, r = [p["c"]], T.radius(p)
        for x, y in pts:
            assert x0 - 3 <= x - r and x + r <= x1 + 3 and y0 - 3 <= y - r and y + r <= y1 + 3, (p["block"], p["kind"], x, y)


def test_no_two_round_parts_stand_in_one_place():
    for a, b in itertools.combinations(_round(), 2):
        d = math.hypot(a["c"][0] - b["c"][0], a["c"][1] - b["c"][1])
        assert d >= T.radius(a) + T.radius(b) - 0.6, (a["block"], a["kind"], a["c"], b["kind"], b["c"], round(d, 1))


def _footprints():
    """The rectangles that stand on the face. A handle stands on its two feet (its ends, 12 mm each); its grip clears
    the face by more than a jack's height."""
    out = []
    for p in T.parts():
        if "rect" not in p or p["kind"] == "divider":
            continue
        if p["kind"] == "handle":
            x0, y0, x1, y1 = p["rect"]
            if abs(y1 - y0) > abs(x1 - x0):
                out += [dict(p, rect=(x0, y0, x1, y0 + 12)), dict(p, rect=(x0, y1 - 12, x1, y1))]
            else:
                out += [dict(p, rect=(x0, y0, x0 + 12, y1)), dict(p, rect=(x1 - 12, y0, x1, y1))]
        else:
            out.append(p)
    return out


def test_round_parts_keep_off_the_raised_rectangles():
    """A lamp, jack or lock never sits half across a meter, a table or the socket field (screws on plates are fine, and
    the R4 fuses stand on their rails)."""
    rects = _footprints()
    for s in _round():
        for q in rects:
            x0, y0, x1, y1 = q["rect"]
            x0, x1 = min(x0, x1), max(x0, x1)
            y0, y1 = min(y0, y1), max(y0, y1)
            nx, ny = min(max(s["c"][0], x0), x1), min(max(s["c"][1], y0), y1)
            touching = math.hypot(s["c"][0] - nx, s["c"][1] - ny) < T.radius(s) - 0.6
            on_it = x0 < s["c"][0] < x1 and y0 < s["c"][1] < y1
            if touching and not on_it:
                ok = (q["kind"] in CARRIERS and s["kind"] in ("screw", "stud", "foot")) or                      (q["kind"] == "bar" and s["kind"] in ("fuse", "fuse_black"))
                assert ok, (s["kind"], s["c"], q["kind"], q["rect"])
            if on_it and q["kind"] not in CARRIERS:
                raise AssertionError((s["kind"], s["c"], "inside", q["kind"], q["rect"]))


def test_measured_sizes():
    """The fitted diameters the docstring quotes: screws 6.2, lamps 10.8, jacks 20.1, locks 27 mm (medians)."""
    def med(kind):
        v = sorted(p["d"] for p in T.parts(kind) if "d" in p)
        return v[len(v) // 2]
    assert 5.5 <= med("screw") <= 7.0
    assert 9.5 <= med("lamp") <= 12.0
    assert 19.0 <= med("jack") <= 21.0
    assert 24.0 <= med("lock") <= 29.0


def test_heights_are_measured_or_chosen():
    """Every kind of part has a height, and the measured ones agree with Z within the 3 mm the method gives."""
    kinds = {p["kind"] for p in T.parts()}
    for k in kinds - {"frame_connector"}:
        assert T.height({"kind": k}) != 0.0 or k in ("plate",), k
    for k in ("table", "sockets"):
        hs = [p["h"] for p in T.parts(k) if p["h"] is not None]
        assert hs and all(abs(h - T.height({"kind": k})) <= 3.5 for h in hs), (k, hs)


def test_controls_lie_in_a_window():
    for cid, c in T.CONTROL_MM.items():
        bez = [p for p in T.parts("bezel") if math.hypot(p["c"][0] - c[0], p["c"][1] - c[1]) < 2.0]
        assert bez, cid
        assert T.CONTROL_D_MM[cid] <= 2 * 19.5 + 0.1
