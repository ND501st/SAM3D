"""The 0096-9M2's trace (tools/blender/cabinet0096_trace.py) holds together: every part on the door, no two parts in one place, each working control on a real part."""
import itertools
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import cabinet0096_trace as T  # noqa: E402

ROUND = {"knob", "knob_green", "push", "captive", "tov", "lamp_ring", "adjust"}


def _round_parts():
    return [p for p in T.parts() if p["kind"] in ROUND]


def _bbox(p):
    if "r" in p:
        return p["r"]
    r = p.get("d", 2 * T.FUSE_DISC_R) / 2
    return (p["c"][0] - r, p["c"][1] - r, p["c"][0] + r, p["c"][1] + r)


def _hit(a, b, slack=0.0):
    return min(a[2], b[2]) - max(a[0], b[0]) > slack and min(a[3], b[3]) - max(a[1], b[1]) > slack


def _fuse_box(p):
    return (p["c"][0] - T.FUSE_BULB_R, p["c"][1] - T.FUSE_DISC_R, p["c"][0] + T.FUSE_BULB_R, p["bulb"][1] + T.FUSE_BULB_R)


def test_the_size_is_what_the_photos_give():
    assert T.W_MM == 660.0
    assert 1.02 <= T.H_MM / T.W_MM <= 1.05
    d = T.DOOR
    assert 0 < d[0] < 30 and 640 < d[2] < T.W_MM
    assert 5 < d[1] < 15 and 670 < d[3] <= T.H_MM


def test_every_part_lies_on_the_door():
    d = T.DOOR
    for p in T.parts():
        b = _bbox(p)
        assert b[0] >= d[0] - 0.5 and b[2] <= d[2] + 0.5 and b[1] >= d[1] - 0.5 and b[3] <= d[3] + 0.5, (p, d)


def test_parallax_moves_a_tall_part_toward_the_camera_foot():
    assert T.fix(600.0, 300.0, 25.0)[0] < 600.0 - 15.0
    a = T.fix(100.0, 300.0, 0.0)
    assert 90.0 < a[0] < 100.0


def test_no_two_round_parts_stand_in_one_place():
    for a, b in itertools.combinations(_round_parts(), 2):
        gap = math.dist(a["c"], b["c"]) - (a["d"] + b["d"]) / 2
        assert gap > 0.0, (a, b, gap)


def test_fuses_do_not_collide():
    fuses = T.parts("fuse_disc")
    for a, b in itertools.combinations(fuses, 2):
        if a["row"] == b["row"]:
            gap = abs(a["c"][0] - b["c"][0]) - 2 * T.FUSE_DISC_R
            assert gap > -0.5 and abs(a["c"][0] - b["c"][0]) > 2 * T.FUSE_BULB_R - 0.5, (a, b)
        else:
            assert not _hit(_fuse_box(a), _fuse_box(b)), (a, b)


def test_capsules_do_not_overlap_each_other_or_the_fuses_or_knobs():
    caps = T.parts("capsule")
    for a, b in itertools.combinations(caps, 2):
        assert not _hit(a["r"], b["r"]), (a["id"], b["id"])
    for c in caps:
        for f in T.parts("fuse_disc"):
            assert not _hit(c["r"], _fuse_box(f)), (c["id"], f["c"])
        for k in T.parts("knob") + T.parts("knob_green") + T.parts("push") + T.parts("tov"):
            assert not _hit(c["r"], _bbox(k)), (c["id"], k["id"])


def test_each_button_and_toggle_sits_on_a_capsule():
    caps = T.parts("capsule")
    for p in T.parts("cap_button") + T.parts("cap_toggle"):
        x, y = p["c"]
        hits = [c for c in caps if c["r"][0] - 8 <= x <= c["r"][2] + 8 and c["r"][1] - 8 <= y <= c["r"][3] + 8]
        assert len(hits) == 1, (p["id"], [c["id"] for c in hits])


def test_the_rails_clear_the_fuse_rows_and_stand_above_them():
    for (a, b) in T.RAILS:
        y = (a[1] + b[1]) / 2
        for f in T.parts("fuse_disc"):
            box = _fuse_box(f)
            if a[0] <= f["c"][0] <= b[0]:
                assert not (box[1] < y < box[3]) or T.Z["rail"] > T.Z["fuse_bulb"], (y, f["c"])
    assert T.Z["rail_rod"] > T.Z["fuse_bulb"] > T.Z["fuse_disc"]
    assert all(0.0 < rod - (a[1] + b[1]) / 2 < 12.0 for rod, (a, b) in zip(T.RAIL_ROD_Y, T.RAILS))
    for rod in T.RAIL_ROD_Y:
        for f in T.parts("fuse_disc"):
            assert not (f["c"][1] - T.FUSE_DISC_R < rod < f["bulb"][1] + T.FUSE_BULB_R), (rod, f["c"])
    assert all(a[0] > T.DOOR[0] and b[0] < T.DOOR[2] for a, b in T.RAILS)


def test_the_rail_feet_miss_the_knobs_and_capsules():
    feet = [p for r in T.RAILS for p in r]
    for f in feet:
        b = (f[0] - T.RAIL_FOOT_R, f[1] - T.RAIL_FOOT_R, f[0] + T.RAIL_FOOT_R, f[1] + T.RAIL_FOOT_R)
        for c in T.parts("capsule"):
            assert not _hit(b, c["r"]), (f, c["id"])
        for k in T.parts("knob"):
            assert not _hit(b, _bbox(k)), (f, k["id"])
        for s in T.parts("fuse_disc"):
            assert math.dist(f, s["c"]) > T.RAIL_FOOT_R + T.FUSE_DISC_R, (f, s["c"])
            assert math.dist(f, s["bulb"]) > T.RAIL_FOOT_R + T.FUSE_BULB_R, (f, s["bulb"])


def test_the_hinges_are_on_the_hinge_side_and_miss_the_meter():
    for h in T.HINGES:
        assert h[0] < h[1]
        assert h[2] < h[0] and h[3] > h[1] - 1.0
    assert T.HINGE_BARREL_X < T.DOOR[0]
    assert T.HINGE_LEAF_X < T.HOURS["axis"][0] - T.HOURS["flange_r"] + 40.0


def test_the_meters_clear_their_neighbours():
    a = T.HOURS["axis"]
    r = T.HOURS["flange_r"] + 8.0
    hours = (a[0] - r, a[1] - r, a[0] + r, a[1] + r)
    volt = T.fix_rect(T.VOLT["bezel"], T.Z["meter"])
    assert not _hit(hours, volt)
    for box in (hours, volt):
        for c in T.parts("capsule"):
            assert not _hit(box, c["r"]), c["id"]
        for k in T.parts("knob") + T.parts("push") + T.parts("knob_green"):
            assert not _hit(box, _bbox(k)), k["id"]
    assert T.DOME["base"] <= volt[1] + 2.0


def test_every_control_sits_on_a_part():
    live = T.parts("push") + T.parts("knob") + T.parts("knob_green") + T.parts("cap_button") + T.parts("cap_toggle") + T.parts("tov")
    for name, c in T.CONTROL_MM.items():
        assert any(math.dist(c, p["c"]) < 0.3 for p in live), name


def test_the_controls_the_simulation_names_are_covered():
    have = " ".join(T.CONTROL_MM)
    for name in ("pwr.equipment", "stab.power", "soc.rotation_lock", "tov.supply", "drv.az", "drv.el"):
        assert name in have, name


def test_local_positions_lie_on_the_face():
    for name, (x, z) in T.control_local().items():
        assert -0.33 <= x <= 0.33 and -0.20 <= z <= 0.48, (name, x, z)
    assert T.to_local(0.0, 0.0, 0.0) == (0.33, 0.095, 0.48)


def test_plugs_fit_under_the_body():
    for a, b in itertools.pairwise(T.PLUG_X):
        assert b - a >= 2 * T.PLUG_R
    assert T.PLUG_X[0] - T.PLUG_R > 0 and T.PLUG_X[-1] + T.PLUG_R < T.W_MM
    assert T.PLUG_Z + T.PLUG_R < T.DEPTH_MM


def test_the_game_size_is_the_photos_and_the_items_stay_on_the_door():
    assert 0.59 < T.SCALE * T.W_MM / 1000 < 0.63 and 0.61 < T.SCALE * T.H_MM / 1000 < 0.65    # the door as a control plate solved with certified cameras (docs/research/OSA_REBUILD_C_2026-11.md), against the trace's 660 x 680 mm; 0.57 x 0.59 before came through cameras the audit found out
    _, (w, h) = T.panel_frame()
    for k in T.CONTROL_READ:
        name = k[:-3] if k.endswith('.on') else k[:-4] if k.endswith('.off') else k
        u, v = T.panel_at(name)
        assert abs(u) < w / 2 and abs(v) < h / 2, k

def test_the_cabin_data_on_the_door_sits_on_traced_parts():
    """Every control, lamp, lamp fixture and the voltmeter that data/cabins/osa_ak.json puts on pwr_cabinet stands on a
    part of the trace (a pair of buttons worked as one switch: the point between them), within 3 mm in the game's size."""
    import json
    (cx, cz), _ = T.panel_frame()

    def uv(x, y, h):
        lx, _, lz = T.to_local(x, y, h)
        return (-T.SCALE * (lx - cx), T.SCALE * (lz - cz))
    spots = [uv(p["c"][0], p["c"][1], p["h"]) for p in T.PARTS if "c" in p]
    buttons = T.parts("push") + T.parts("cap_button")
    for a, b in itertools.combinations(buttons, 2):
        if math.dist(a["c"], b["c"]) < 50.0:
            spots.append(uv((a["c"][0] + b["c"][0]) / 2, (a["c"][1] + b["c"][1]) / 2, a["h"]))
    ax, ay = T.VOLT["axis"]
    dial = uv(ax, ay + (T.VOLT["dial_top_dy"] + T.VOLT["dial_bottom_dy"]) / 2, T.Z["dial"])
    cabin = json.loads((REPO / "data" / "cabins" / "osa_ak.json").read_text(encoding="utf-8"))
    items = [(c["control"], c["at_m"]) for c in cabin["controls"] if c["panel"] == "pwr_cabinet"]
    items += [(l["id"], l["at_m"]) for l in cabin["lamps"] if l["panel"] == "pwr_cabinet"]
    items += [(f["label"], f["at_m"]) for f in cabin["fixtures"] if f["panel"] == "pwr_cabinet" and f["kind"] == "lamp"]
    assert len(items) >= 25
    for name, at in items:
        assert min(math.dist(at, s) for s in spots) < 0.003, name
    meter = next(d for d in cabin["displays"] if d["id"] == "pwr_voltmeter")
    assert math.dist(meter["at_m"], dial) < 0.004
    assert meter["size_m"][0] < T.SCALE * 2 * T.VOLT["dial_half_w"] / 1000 + 0.001


def _inside(p, poly):
    """Point (y, z) inside a polygon [(y, z)...] (ray casting)."""
    y, z = p
    hit = False
    for (y0, z0), (y1, z1) in zip(poly, poly[1:] + poly[:1]):
        if (z0 > z) != (z1 > z) and y < y0 + (z - z0) * (y1 - y0) / (z1 - z0):
            hit = not hit
    return hit


def test_the_repair_toggles_stand_on_their_plate_in_two_rows_under_the_lid():
    x0, x1, s0, s1 = T.REPAIR_PLATE
    assert T.LID_X[0] < x0 < x1 < T.LID_X[1]                     # the cover is wider than the plate
    slope = 2 ** 0.5 * T.BODY_SECTION[1][1]                       # the slope's length (mm)
    assert 0 < s0 < s1 <= slope
    assert len(T.REPAIR_TOP_X) == 13 and len(T.REPAIR_BOTTOM_X) == 10   # g097-a: 23 toggles
    d, h = T.REPAIR_TOGGLE
    cavity = T.lid_inner()
    for s, row in zip(T.REPAIR_ROWS, (T.REPAIR_TOP_X, T.REPAIR_BOTTOM_X)):
        assert s0 + d / 2 < s < s1 - d / 2
        assert all(x0 + d / 2 < x < x1 - d / 2 for x in row)
        assert all(b - a > d for a, b in zip(row, row[1:]))      # washers clear of each other
        _, y, z = T.slope_point(0.0, s, h)
        assert _inside((y, z), cavity), s                         # the lever's tip clears the closed cover
    assert T.REPAIR_ROWS[0] - T.REPAIR_ROWS[1] > d + 2 * h / 3


def test_the_cover_is_a_hollow_sheet_hinged_at_its_rear_top_edge():
    inner, outer = T.lid_inner(), T.LID_SECTION
    assert all(_inside(p, outer + []) or p in outer for p in inner[1:3])
    hy, hz = T.LID_HINGE
    ry, rz = outer[2]
    assert abs(hy - ry) < 8 and abs(hz - rz) < 4                  # the axis at the rear top corner
    assert all(T.LID_X[0] < x < T.LID_X[1] for x in T.LID_HINGE_X)
    assert 60 < T.LID_OPEN_DEG < 100                              # an awning over the plate, not thrown back
    # the lamp channel and the inside plate lie on the cover's top inside, clear of the toggles below them when it is shut
    lx0, lx1, ls, lw = T.LID_LAMP
    ax0, ax1, as0, as1 = T.LID_INNER_PLATE
    assert T.LID_X[0] < lx0 < lx1 < T.LID_X[1] and T.LID_X[0] < ax0 < ax1 < T.LID_X[1]
    assert as1 < ls                                               # the plate at the free edge, the lamp further in
    for s in (ls, ls + lw):
        y, z = T.lid_top_point(s, 10.0)
        along = (-y + z) / 2 ** 0.5                               # where that is up the slope
        assert all(abs(along - r) > T.REPAIR_TOGGLE[0] for r in T.REPAIR_ROWS), along
    sx, ss = T.LID_STAY_FOOT
    assert T.REPAIR_PLATE[1] < sx < T.LID_X[1] and T.REPAIR_PLATE[2] < ss < T.REPAIR_PLATE[3]
