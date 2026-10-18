"""tools/map_view.py draws a map_dump JSON from above (docs/TEST_RANGE.md)."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import map_view  # noqa: E402

DUMP = {
    "map_id": "t", "name": "Test", "extent_m": [-1000, -1000, 1000, 1000], "far_m": 3000,
    "ground": {"west": -1000, "south": -1000, "step": 1000, "rows": [[100, 101, 102], [100, 105, 102], [99, 100, 101]]},
    "far_ground": {"west": -3000, "south": -3000, "step": 3000, "rows": [[100, 100, 100], [100, 100, 100], [100, 100, 100]]},
    "boxes": [["house", 0, 0, 12, 8, 6, 0.3], ["canopy", 500, 500, 100, 100, 20, 0], ["tank", -500, 0, 15, 0, 10, 0]],
    "strips": [{"id": "r", "surface": "road", "width_m": 6, "points": [[-1000, 0], [1000, 0]]}],
    "labels": [{"id": "v", "name": "Village <A>", "kind": "village", "at": [0, 0]}],
}


def test_svg_has_every_layer_and_escapes_names():
    svg = map_view.svg(DUMP)
    assert svg.startswith("<svg") and svg.endswith("</svg>")
    assert map_view.PART_COLOURS["house"] in svg and map_view.PART_COLOURS["canopy"] in svg
    assert "<polyline" in svg and "Village &lt;A&gt;" in svg


def test_main_writes_a_page_and_a_picture(tmp_path):
    dump = tmp_path / "d.json"
    dump.write_text(json.dumps(DUMP), encoding="utf-8")
    page, pic = tmp_path / "m.html", tmp_path / "m.png"
    assert map_view.main([str(dump), "--out", str(page)]) == 0
    assert "<svg" in page.read_text(encoding="utf-8")
    try:
        import PIL  # noqa: F401
    except ImportError:
        return
    assert map_view.main([str(dump), "--png", "--far", "--out", str(pic)]) == 0
    assert pic.stat().st_size > 1000
