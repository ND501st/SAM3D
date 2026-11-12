"""SŁ-2M's raised housings (tools/blender/sl2m_housings.py) fit the game's layout: each on its display, its hole no smaller
than the screen, clear of every control, lamp and fixture of its panel, within the panel, and no flat fixture left under
a housing that now draws it."""
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import sl2m_housings as H  # noqa: E402


def test_no_problems():
    assert H.problems() == []


def test_every_housing_on_a_display_of_its_panel():
    cabin = H.load_cabin()
    displays = {d["id"]: d for d in cabin["displays"]}
    ids = [h["id"] for h in H.HOUSINGS]
    assert len(ids) == len(set(ids))
    for h in H.HOUSINGS:
        assert h["panel"] in H.PANELS
        if "on" in h:
            assert displays[h["on"]]["panel"] == h["panel"], h["id"]
        assert h["why"] and 0.0 < h["depth"] <= 0.05, h["id"]


def test_the_round_screens_and_the_tv_have_housings():
    on = {h.get("on") for h in H.HOUSINGS}
    assert {"soc_ppi", "ssc_range", "tov_monitor", "elev_search", "beta_c", "epsilon_c", "dts_voltmeter"} <= on


def test_no_flat_fixture_left_under_a_housing():
    """The bezels, ovals and windows a housing replaces are gone from the cabin file (the game would draw both)."""
    cabin = H.load_cabin()
    for h in H.HOUSINGS:
        cx, cy = H.centre(h, cabin)
        for fx in cabin["fixtures"]:
            if fx["panel"] != h["panel"] or fx["kind"] not in ("bezel", "oval", "window"):
                continue
            u, v = fx["at_m"]
            assert abs(u - cx) > 0.01 or abs(v - cy) > 0.02, (h["id"], fx)


def test_a_notch_clears_its_control():
    cabin = H.load_cabin()
    for h in H.HOUSINGS:
        cut = H.notch_circle(h, cabin)
        if cut is None:
            continue
        nu, nv, nr = cut
        for (u, v) in H.outer_outline(h, cabin):
            assert (u - nu) ** 2 + (v - nv) ** 2 >= (nr - 1e-6) ** 2, h["id"]


WHEELS = ("ssc.range_wheel", "ssc.az_wheel", "ssc.el_wheel")
HANDLE_R_M = 0.01          # the crank's grip, 0.22 of its length (build_cabin_parts.wheel_handle) at its longest


def _sweep_m(control):
    """How far from a hand wheel's axis anything of it reaches as it turns: its rim, or its crank (0.4 of the wheel's
    width out, SimCabinComponent::AnimatePart) with the grip's radius."""
    d = control.get("look", {}).get("size_m", 0.12)
    return max(d / 2, 0.4 * d + HANDLE_R_M)


def test_the_hand_wheels_turn_clear():
    """Each of SŁ-2M's hand wheels turns a full circle without its rim or crank touching another item or a housing."""
    import json
    import math
    cabin = H.load_cabin()
    console = json.loads((REPO / "data" / "procedures" / "osa_ak.json").read_text(encoding="utf-8"))
    controls = {c["control"]: c for c in cabin["controls"]}
    for wid in WHEELS:
        w = controls[wid]
        u, v = w["at_m"]
        reach = _sweep_m(w)
        for it in H.footprints(cabin, console, w["panel"]):
            if it[1] == wid:
                continue
            pts = H.item_outline(it[2], it[3], it[4])
            assert not H._inside((u, v), pts), (wid, it[1])
            gap = min(math.hypot(x - u, y - v) for x, y in pts) - reach
            assert gap >= H.CLEAR_M, (wid, it[1], round(gap * 1000, 1))
        for h in H.HOUSINGS:
            if h["panel"] == w["panel"]:
                gap = min(math.hypot(x - u, y - v) for x, y in H.outer_outline(h, cabin)) - reach
                assert gap >= H.CLEAR_M - 1e-6, (wid, h["id"], round(gap * 1000, 1))


def test_the_hand_wheels_are_worked_against_a_display():
    """A hand wheel names the display its crewman watches as he turns it, on his own column, and turns a set amount."""
    cabin = H.load_cabin()
    displays = {d["id"]: d for d in cabin["displays"]}
    for c in cabin["controls"]:
        if c["control"] in WHEELS:
            assert c["watch"] in displays and displays[c["watch"]]["panel"] in H.PANELS, c["control"]
            assert c["units_per_turn"] > 0, c["control"]
    dome = next(c for c in cabin["controls"] if c["control"] == "ssc.range_wheel")
    assert dome["look"]["kind"] == "dome"


def test_the_mounts_hold_their_lamps_and_buttons():
    """Every SŁ-2M mount (a lamp bank's scalloped plate, a button's stadium, a window, a frame) carries every item on it
    and runs over no other item by more than a millimetre."""
    assert H.mount_problems() == []


def test_mesh_drawn_fixtures_are_the_mounts():
    """A fixture the rack's mesh draws ("mesh": SM_RackSL2M) is one of the three columns', has a mount style, and (all but a
    frame) holds items; the game skips drawing it, so none is left flat."""
    cabin = H.load_cabin()
    meshed = [k for k, f in enumerate(cabin["fixtures"]) if f.get("mesh")]
    assert len(meshed) == 13
    by_fixture = {m["fixture"]: m for m in H.mounts(cabin)}
    for k in meshed:
        f = cabin["fixtures"][k]
        assert f["mesh"] == H.MESH and f["panel"] in H.PANELS, k
        m = by_fixture[k]
        assert m["style"] in ("bank", "stadium", "window", "frame"), k
        if m["style"] != "frame":
            assert m["cups"] or m["glass"] is not None, k
