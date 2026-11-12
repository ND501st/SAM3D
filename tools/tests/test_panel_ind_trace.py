"""The ПАНЕЛЬ ИНДИКАЦИИ's trace (tools/blender/panel_ind_trace.py) holds together: every part on the face, no two parts in
one place, each control on its part."""
import itertools
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import panel_ind_trace as T  # noqa: E402

SCREW_R = 1.8


def _inside(rect, x, y, r=0.0, slack=0.5):
    return rect[0] - slack <= x - r and x + r <= rect[2] + slack and rect[1] - slack <= y - r and y + r <= rect[3] + slack


def _round_parts():
    out = [(c['id'], c['c'], T.CAP_D / 2) for c in T.CAPS]
    out += [('frame_screw', s, SCREW_R) for s in T.CAP_FRAME_SCREWS]
    out += [('sel_knob', T.SEL_KNOB, T.SEL_KNOB_D / 2)]
    out += [('sel_screw', s, SCREW_R) for s in T.SEL_PLATE_SCREWS]
    out += [('otkaz_screw', s, SCREW_R) for s in T.OTKAZ_PLATE_SCREWS]
    out += [('otkaz_lamp', T.OTKAZ_LAMP, T.OTKAZ_BEZEL_R)]
    out += [('sep_screw', s, SCREW_R) for s in T.SEP_PLATE_SCREWS]
    out += [('sep_lamp', T.SEP_LAMP, T.GUARD_RING[1]), ('ukch', T.UKCH_TOGGLE, T.GUARD_RING[1])]
    return out


def test_the_size_is_what_the_photo_gives():
    assert 520 <= T.H_MM <= 570
    assert 50 <= T.W_MM <= 65
    assert 0 < T.DEPTH_MM < 120


def test_every_part_lies_on_the_face():
    face = (0.0, 0.0, T.W_MM, T.H_MM)
    for name, c, r in _round_parts():
        assert _inside(face, c[0], c[1], r), (name, c, r)
    for rect in (T.CAP_FRAME, T.SEL_PLATE, T.OTKAZ_PLATE, T.SEP_PLATE, T.SEP_SUBPLATE):
        assert _inside(face, rect[0], rect[1]) and _inside(face, rect[2], rect[3]), rect


def test_no_two_round_parts_stand_in_one_place():
    for (na, a, ra), (nb, b, rb) in itertools.combinations(_round_parts(), 2):
        assert math.dist(a, b) - ra - rb > 0.0, (na, nb)


def test_the_caps_sit_inside_their_frame():
    x0, y0, x1, y1 = T.CAP_FRAME
    inner = (x0 + T.CAP_FRAME_RIM, y0 + T.CAP_FRAME_RIM, x1 - T.CAP_FRAME_RIM, y1 - T.CAP_FRAME_RIM)
    for c in T.CAPS:
        assert _inside(inner, c['c'][0], c['c'][1], T.CAP_D / 2), c
    assert [c['id'] for c in T.CAPS if c.get('lamp')] == ['cap_r5']


def test_the_plates_do_not_overlap():
    rects = [T.CAP_FRAME, T.SEL_PLATE, T.OTKAZ_PLATE, T.SEP_PLATE]
    for a, b in itertools.combinations(rects, 2):
        assert min(a[3], b[3]) <= max(a[1], b[1]), (a, b)
    y = T.OTKAZ_LAMP[1] - T.OTKAZ_BEZEL_R
    assert y > T.OTKAZ_PLATE[3] and T.OTKAZ_LAMP[1] + T.OTKAZ_BEZEL_R < T.SEP_PLATE[1]


def test_the_selector_knob_turns_in_its_arch():
    assert math.dist(T.SEL_KNOB, T.SEL_ARCH_C) + T.SEL_KNOB_D / 2 < T.SEL_ARCH_R
    for s in T.SEL_PLATE_SCREWS:
        assert _inside(T.SEL_PLATE, s[0], s[1], SCREW_R)
        assert math.dist(s, T.SEL_ARCH_C) > T.SEL_ARCH_R + SCREW_R
    pts = T.sel_plate_outline()
    for x, y in pts:
        assert _inside(T.SEL_PLATE, x, y)
    area = 0.5 * sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(pts, pts[1:] + pts[:1]))
    full = (T.SEL_PLATE[2] - T.SEL_PLATE[0]) * (T.SEL_PLATE[3] - T.SEL_PLATE[1])
    assert 0.5 * full < abs(area) < full


def test_the_button_and_toggle_stand_on_the_sub_plate():
    for c in (T.SEP_LAMP, T.UKCH_TOGGLE):
        assert _inside(T.SEP_SUBPLATE, c[0], c[1], T.GUARD_RING[1])
    assert T.GUARD_RING[0] < T.GUARD_RING[1]


def test_the_plugs_stand_off_the_face():
    assert T.PLUG_SH2_PLATE[1] >= T.H_MM and T.PLUG_SH2_SHELL[1] >= T.PLUG_SH2_PLATE[3] - 0.1
    assert T.PLUG_SH1[0] >= T.W_MM


def test_every_control_stands_on_its_part():
    assert T.CONTROL_MM['pwr.emergency_off'] == T.SEP_LAMP
    assert T.CONTROL_MM['pwr.ukch'] == T.UKCH_TOGGLE
    assert T.CONTROL_MM['ind.select'] == T.SEL_KNOB
    cap = [c['c'] for c in T.CAPS if c.get('lamp') == 'dver_zakryta'][0]
    assert T.CONTROL_MM['dver_zakryta'] == cap
    for k, (x, y) in T.CONTROL_MM.items():
        a, b = T.to_m(x, y)
        assert abs(a) < T.W_MM / 2000 and abs(b) < T.H_MM / 2000, k
        assert k in T.CONTROL_D_MM


def test_to_m_is_centred():
    assert T.to_m(T.W_MM / 2, T.H_MM / 2) == (0.0, 0.0)
    a, b = T.to_m(T.W_MM, 0.0)
    assert a > 0 and b > 0


def test_the_selector_has_seven_positions_marked_on_its_plate():
    assert len(T.SEL_POSITIONS_DEG) == 7 and T.SEL_POSITIONS_DEG[3] == 0.0
    steps = [b - a for a, b in zip(T.SEL_POSITIONS_DEG, T.SEL_POSITIONS_DEG[1:])]
    assert all(abs(s - 30.0) < 1e-9 for s in steps)      # as the cabin turns a seven-position rotary
    cx, cy = T.SEL_KNOB
    for a in T.SEL_POSITIONS_DEG:
        x, y = cx + T.SEL_MARK_R * math.sin(math.radians(a)), cy - T.SEL_MARK_R * math.cos(math.radians(a))
        assert _inside(T.SEL_PLATE, x, y, 1.1)
        assert math.dist((x, y), T.SEL_ARCH_C) > T.SEL_ARCH_R
        for s in T.SEL_PLATE_SCREWS:
            assert math.dist((x, y), s) > SCREW_R + 1.1


def test_the_lamp_and_its_button_share_the_left_ring():
    assert T.CONTROL_MM['sep_avar_otkl'] == T.CONTROL_MM['pwr.emergency_off'] == T.SEP_LAMP
