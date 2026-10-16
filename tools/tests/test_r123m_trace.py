"""The R-123M's 3D trace (tools/blender/r123m_trace.py) holds together: nothing on the front overlaps what it shouldn't.

The model is built straight from the trace, so a screw drawn inside a well, a plate laid across an opening it isn't cut
round, or two knobs that collide would show in the cabin as parts running into each other.
"""
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import r123m_trace as T  # noqa: E402

SCREW_R = 2.6


def _openings():
    """Round openings in the plate: (name, centre, radius) mm."""
    out = [(w["id"], w["c"], w["r"]) for w in T.WELLS]
    out.append(("drum", T.DRUM["c"], T.DRUM["r_opening"]))
    out += [("hole", (x, y), r) for x, y, r in T.HOLES]
    out += [("brass", (x, y), r) for x, y, r in T.BRASS_IN_HOLE]
    out.append(("lens", T.LENS["c"], T.LENS["r"]))
    out.append(("red_lamp", T.RED_LAMP["c"], T.RED_LAMP["r"]))
    return out


def _rect_circle_overlap(r, c, rad):
    x0, y0, x1, y1 = r
    nx, ny = min(max(c[0], x0), x1), min(max(c[1], y0), y1)
    return math.hypot(c[0] - nx, c[1] - ny) < rad


def _all_screws():
    return (list(T.SCREWS) + list(T.FIXATORS["screws"]) + list(T.LAMP_PLATE["screws"]) + list(T.HOOD["screws"])
            + list(T.DRUM["plate_screws"]) + [(x, y) for x, y, _ in T.BRASS_HEADS])


def test_everything_lies_on_the_front():
    for c in _all_screws() + [w["c"] for w in T.WELLS] + list(T.CONTROL_MM.values()):
        assert -1.0 <= c[0] <= T.W_MM + 1.0 and -1.0 <= c[1] <= T.H_MM + 1.0, c
    for pl in T.PLATES:
        x0, y0, x1, y1 = pl["rect"]
        assert 0.0 <= x0 < x1 <= T.W_MM and 0.0 <= y0 < y1 <= T.H_MM, pl["id"]


def test_no_screw_falls_in_an_opening_or_on_another_screw():
    screws = _all_screws()
    for s in screws:
        for name, c, r in _openings():
            assert math.hypot(s[0] - c[0], s[1] - c[1]) > r + 1.0, (s, name)
    for i, a in enumerate(screws):
        for b in screws[i + 1:]:
            assert math.hypot(a[0] - b[0], a[1] - b[1]) > 2 * SCREW_R, (a, b)


def test_openings_do_not_run_into_each_other():
    ops = _openings()
    for i, (na, ca, ra) in enumerate(ops):
        for nb, cb, rb in ops[i + 1:]:
            assert math.hypot(ca[0] - cb[0], ca[1] - cb[1]) > ra + rb + 1.0, (na, nb)


def test_plates_are_cut_round_the_wells_they_reach():
    """A plate that reaches into a well's opening must say so (`cut`) and is then cut round it; the others stay clear of
    every opening except the holes cut through them (a hole wholly inside the plate, as КАЛИБРОВКА's brass shaft)."""
    for pl in T.PLATES + [{"id": "voltage", "rect": T.VOLT_PLATE, "cut": "voltage"}]:
        for name, c, r in _openings():
            if pl.get("cut") == name:
                continue
            x0, y0, x1, y1 = pl["rect"]
            inside = x0 + r < c[0] < x1 - r and y0 + r < c[1] < y1 - r
            if inside and name in ("hole", "brass"):
                continue
            assert not _rect_circle_overlap(pl["rect"], c, r), (pl["id"], name)


def test_plates_do_not_overlap_each_other():
    rects = [(pl["id"], pl["rect"]) for pl in T.PLATES] + [("voltage", T.VOLT_PLATE), ("pocket", T.POCKET),
                                                           ("lamp_plate", T.LAMP_PLATE["rect"]),
                                                           ("nameplate", T.NAMEPLATE["rect"]),
                                                           ("fixators", T.FIXATORS["recess"])]
    for i, (na, a) in enumerate(rects):
        for nb, b in rects[i + 1:]:
            overlap = min(a[2], b[2]) - max(a[0], b[0]) > 0.5 and min(a[3], b[3]) - max(a[1], b[1]) > 0.5
            assert not overlap, (na, nb)


def test_cabin_controls_sit_in_their_wells_or_clear_of_everything():
    wells = {w["sim"]: w for w in T.WELLS if w["sim"]}
    for name, c in T.CONTROL_MM.items():
        if name in wells:
            w = wells[name]
            assert math.hypot(c[0] - w["c"][0], c[1] - w["c"][1]) < 0.5, name
        elif name.startswith("lamp_"):
            assert any(math.hypot(c[0] - l[0], c[1] - l[1]) < 0.5 for l in T.LAMP_PLATE["c"]), name
        else:
            for oname, oc, r in _openings():
                assert math.hypot(c[0] - oc[0], c[1] - oc[1]) > r + 8.0, (name, oname)
            for s in _all_screws():
                assert math.hypot(c[0] - s[0], c[1] - s[1]) > 10.0, (name, s)


def test_controls_on_the_lower_block_stand_on_it():
    for name in T.ON_LOWER_BLOCK:
        x, y = T.CONTROL_MM[name]
        assert x < T.NOTCH_X - 8.0 and y > T.STEP_Y + 8.0, name


def test_outline_is_a_closed_ring_inside_the_front():
    pts = T.plate_outline_m()
    assert len(pts) > 20
    area = 0.0
    for (a0, b0), (a1, b1) in zip(pts, pts[1:] + pts[:1]):
        area += a0 * b1 - a1 * b0
    area /= 2.0
    full = T.PLATE_W_M * T.PLATE_H_M
    notch = (T.NOTCH_X * (T.H_MM - T.STEP_Y)) * 1e-6
    assert area > 0.0                                    # anticlockwise as seen from the front
    assert abs(area - (full - notch)) < 0.01 * full      # the front less the notch (and the rounded corners)


def test_words_fit_their_plates():
    """Each plate's words fit on it (Arial Narrow, as the model engraves them; skipped where the font isn't installed)."""
    import pytest
    try:
        from PIL import ImageFont
        font = ImageFont.truetype(r"C:\Windows\Fonts\ARIALN.TTF", 100)
    except (ImportError, OSError):
        pytest.skip("Arial Narrow not installed")
    for pl in T.PLATES:
        x0, y0, x1, y1 = pl["rect"]
        along, across = (x1 - x0, y1 - y0) if pl.get("rot", 0) == 0 else (y1 - y0, x1 - x0)
        for line in pl["text"]:
            width = font.getlength(line) / 100 * (pl["h"] / 0.72)     # mm, at the cap height h
            assert width < along - 1.0, (pl["id"], line, round(width, 1), along)
        assert len(pl["text"]) * pl["h"] * 1.5 < across + 0.5 or pl.get("at"), pl["id"]
