"""Tests for tools/panel_sheet.py: the style rules, the layout checks and the page for the real Osa files."""
import copy
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import panel_sheet  # noqa: E402


def _console():
    return {
        "system_id": "test",
        "launch": {"button": "launch"},
        "controls": [
            {"id": "cover", "label": "guard", "kind": "switch", "position_labels": ["closed", "open"]},
            {"id": "hv", "label": "HV", "kind": "switch", "guard": "cover", "position_labels": ["off", "on"]},
            {"id": "lock", "label": "wheel lock", "kind": "switch", "position_labels": ["locked", "unlocked"]},
            {"id": "wheel", "label": "AZ", "kind": "wheel", "guard": "lock", "min": 0, "max": 10, "unit": "deg"},
            {"id": "scale", "label": "RANGE", "kind": "switch", "position_labels": ["a", "b", "c"], "initial": 1},
            {"id": "key", "label": "KEY", "kind": "key", "position_labels": ["out", "in", "turned"]},
            {"id": "launch", "label": "LAUNCH", "kind": "button"},
        ],
    }


def _cabin():
    return {
        "system_id": "test",
        "stations": [{"id": "seat", "label": "Seat"}],
        "panels": [{"id": "p", "label": "Panel", "station": "seat", "size_m": [1.0, 0.6]}],
        "controls": [
            {"control": "hv", "panel": "p", "at_m": [-0.4, 0.2]},
            {"control": "lock", "panel": "p", "at_m": [-0.2, 0.2]},
            {"control": "wheel", "panel": "p", "at_m": [-0.3, -0.1]},
            {"control": "scale", "panel": "p", "at_m": [0.0, 0.2]},
            {"control": "key", "panel": "p", "at_m": [0.2, 0.2]},
            {"control": "launch", "panel": "p", "at_m": [0.4, 0.2]},
        ],
        "lamps": [{"id": "ready", "label": "READY", "panel": "p", "at_m": [0.4, 0.0], "color": "green"}],
        "displays": [{"id": "scope", "label": "Scope", "kind": "ppi", "panel": "p", "at_m": [0.1, -0.1],
                      "size_m": [0.2, 0.2]}],
    }


def test_default_styles_follow_cabin_hpp():
    controls = _console()["controls"]
    styles = {c["id"]: panel_sheet.default_style(c, controls) for c in controls}
    assert styles == {
        "cover": "cover",    # guards a switch
        "hv": "toggle",
        "lock": "toggle",    # guards a wheel: a lock, not a cover
        "wheel": "wheel",
        "scale": "rotary",   # more than two positions
        "key": "key",
        "launch": "button",
    }


def test_unplaced_cover_sits_over_its_control():
    items = {i["id"]: i for i in panel_sheet.build_items(_console(), _cabin())}
    assert items["cover"]["at"] == items["hv"]["at"]
    assert items["cover"]["guards"] == "hv"
    assert items["launch"]["launch"] is True


def test_clean_layout_has_no_problems():
    assert panel_sheet.find_problems(_console(), _cabin()) == []


def test_overlap_is_reported():
    cabin = _cabin()
    cabin["controls"][4]["at_m"] = [0.02, 0.2]  # the key onto the range selector
    problems = panel_sheet.find_problems(_console(), cabin)
    assert any("'scale' and 'key' overlap" in p for p in problems)


def test_item_over_the_panel_edge_is_reported():
    cabin = _cabin()
    cabin["controls"][2]["at_m"] = [-0.47, -0.1]  # the wheel is 12 cm wide: 6 cm from its centre
    problems = panel_sheet.find_problems(_console(), cabin)
    assert any("'wheel' reaches over the edge" in p for p in problems)


def test_look_size_sets_the_footprint():
    cabin = _cabin()
    cabin["controls"][2]["look"] = {"size_m": 0.08}                  # a smaller wheel
    cabin["lamps"][0]["look"] = {"kind": "legend", "size_m": [0.05, 0.02], "label": "READY"}
    items = {i["id"]: i for i in panel_sheet.build_items(_console(), cabin)}
    assert items["wheel"]["size"] == (0.08, 0.08)
    assert items["ready"]["size"] == (0.05, 0.02)
    cabin["controls"][2]["at_m"] = [-0.455, -0.1]  # inside the edge at 8 cm, over it at the default 12 cm
    assert panel_sheet.find_problems(_console(), cabin) == []


def test_covers_stand_off_the_face():
    # A closed guard hangs in front of the panel, so it may reach past the edge or over a neighbour.
    cabin = _cabin()
    cabin["controls"][0]["at_m"] = [-0.48, 0.2]  # the guarded switch at the edge: its cover reaches past it
    cabin["lamps"][0]["at_m"] = [-0.44, 0.2]     # and over the lamp beside it
    assert panel_sheet.find_problems(_console(), cabin) == []


def test_unplaced_control_is_reported():
    cabin = _cabin()
    del cabin["controls"][5]
    problems = panel_sheet.find_problems(_console(), cabin)
    assert "control 'launch' is not placed on any panel" in problems


def test_page_escapes_text_and_marks_the_start_position():
    console = copy.deepcopy(_console())
    console["controls"][4]["label"] = "<RANGE>"
    page = panel_sheet.render_html(console, _cabin())
    assert "&lt;RANGE&gt;" in page and "<RANGE>" not in page
    assert "a / <b>b</b> / c" in page
    assert "No layout problems" in page


def test_osa_files_draw_every_control():
    console, cabin = panel_sheet.load_system("osa_ak")
    page = panel_sheet.render_html(console, cabin)
    for control in console["controls"]:
        assert f">{control['id']}<" in page, control["id"]
    for station in cabin["stations"]:
        assert f">{station['id']}<" in page
    assert panel_sheet.find_problems(console, cabin) == []
