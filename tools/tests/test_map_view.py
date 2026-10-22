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


COUNTRY = {
    "kind": "country", "map_id": "c", "name": "Country", "extent_km": [-20, -20, 20, 20],
    "ground": {"west": -20, "south": -20, "step": 20, "rows": [[100, 120, 140], [100, 110, 120], [90, 100, 110]]},
    "outline": [[-20, -20], [20, -20], [20, 20], [-20, 20]],
    "borders": [{"id": "w", "neighbour": "nato", "points": [[-20, -20], [-20, 20]]}],
    "regions": [{"id": "r", "name": "R", "points": [[-10, -10], [10, -10], [10, 10]]}],
    "forests": [{"id": "f", "points": [[0, 0], [5, 0], [5, 5]]}],
    "rivers": [{"id": "rv", "name": "River", "class": "river", "points": [[0, -20], [0, 20]]}],
    "roads": [{"id": "m", "name": "Main", "class": "main", "points": [[-20, 0], [20, 0]]}],
    "railways": [], "pipelines": [{"id": "p", "name": "Pipe", "class": "oil", "points": [[5, -20], [5, 20]]}],
    "settlements": [{"id": "cap", "name": "Capital <C>", "kind": "capital", "at": [1, 1], "radius_km": 3, "population": 1}],
    "landmarks": [{"id": "ref", "name": "Refinery", "kind": "refinery", "at": [5, 2]}],
    "crossings": [{"id": "m@rv", "net": "road", "river": "rv", "at": [0, 0]}],
}
THEATRE = {"objects": [{"id": "s", "kind": "sam_site", "east_km": 3, "north_km": 3}],
           "fronts": [{"faction": "nato", "entries": [{"id": "W1", "east_km": -30, "north_km": 0}], "bases": {"east_km": -50, "north_km": 0}}]}


def test_the_country_draws_its_borders_lines_places_and_the_theatre_over_it():
    svg = map_view.country_svg(COUNTRY, THEATRE)
    assert svg.startswith("<svg") and svg.endswith("</svg>")
    assert map_view.NEIGHBOUR_COLOURS["nato"] in svg and map_view.OBJECT_COLOURS["sam_site"] in svg
    assert "Capital &lt;C&gt;" in svg and "nato bases" in svg
    # The fronts' bases beyond the border widen the picture to hold them.
    assert map_view.country_bounds(COUNTRY, THEATRE)[0] < -50


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
