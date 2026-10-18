"""Tests for tools/air_war_view.py: the campaign journal becomes one page, the enemy's mind apart from our side's knowledge."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import air_war_view  # noqa: E402

DOC = {
    "faction": "NATO",
    "days": 2,
    "journal": [
        {"h": 1.0, "clock": "D1 05:00", "kind": "plan", "who": "Oberst Kessler", "visible": False,
         "text": "dead 'Raven 30' against SA-8 battery near Kesmark <secret>"},
        {"h": 2.0, "clock": "D1 06:00", "kind": "own_intel", "who": "warning net", "visible": True,
         "text": "4 aircraft from W2"},
        {"h": 30.0, "clock": "D2 10:00", "kind": "career", "who": "Lt Col Walsh", "visible": False, "text": "captured"},
    ],
    "enemy_mind": "planner: Oberst Dieter Kessler - inventive",
    "intsum": "INTSUM D2",
}


def test_render_puts_the_enemy_left_and_our_side_right_by_day():
    page = air_war_view.render(DOC)
    assert page.startswith("<!doctype html>")
    assert "<h2>D1</h2>" in page and "<h2>D2</h2>" in page
    # Escaped, in the enemy's column; our side's line in ours.
    assert "&lt;secret&gt;" in page and "<secret>" not in page
    enemy_row = page.split("Raven 30")[0].rsplit("<div class='row", 1)[1]
    assert "<div class='enemy'><div class='who'>Oberst Kessler" in "<div class='row" + enemy_row + "Raven"
    assert "<div class='enemy'></div><div class='ours'><div class='who'>warning net" in page
    assert "Oberst Dieter Kessler - inventive" in page and "INTSUM D2" in page
    # A filter per kind present.
    for k in ("plan", "own_intel", "career"):
        assert f"value='{k}'" in page


def test_main_writes_the_page(tmp_path):
    src = tmp_path / "air_war_nato.json"
    import json
    src.write_text(json.dumps(DOC), encoding="utf-8")
    assert air_war_view.main([str(src)]) == 0
    assert (tmp_path / "air_war_nato.html").read_text(encoding="utf-8").count("<div class='row") == 3
