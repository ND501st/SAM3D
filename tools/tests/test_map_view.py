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


def test_a_theatre_that_extends_another_is_drawn_whole(tmp_path):
    base = {"objects": [{"id": "a", "kind": "sam_site", "east_km": 0, "north_km": 0},
                        {"id": "b", "kind": "depot", "east_km": 1, "north_km": 1},
                        {"id": "c", "kind": "bridge", "east_km": 2, "north_km": 2}],
            "fronts": [{"faction": "nato", "entries": [], "bases": {"east_km": -50, "north_km": 0}}]}
    child = {"extends": "base.json", "remove": ["c"],
             "objects": [{"id": "b", "kind": "depot", "east_km": 5, "north_km": 5}, {"id": "d", "kind": "refinery", "east_km": 3, "north_km": 3,
                          "positions": [{"east_km": 4, "north_km": 3}]}]}
    (tmp_path / "base.json").write_text(json.dumps(base), encoding="utf-8")
    (tmp_path / "child.json").write_text(json.dumps(child), encoding="utf-8")
    t = map_view.load_theatre(tmp_path / "child.json")
    assert [o["id"] for o in t["objects"]] == ["a", "b", "d"] and t["objects"][1]["east_km"] == 5
    assert t["fronts"] == base["fronts"]
    svg = map_view.country_svg(COUNTRY, t)
    assert map_view.OBJECT_COLOURS["refinery"] in svg and "prepared position" in svg


def test_a_theatre_on_a_place_map_takes_its_points_from_the_map(tmp_path):
    (tmp_path / "maps").mkdir()
    (tmp_path / "air_war").mkdir()
    country = {"settlements": [{"id": "cap", "east_km": 1, "north_km": 1}], "landmarks": [{"id": "ref", "east_km": 5, "north_km": 2}],
               "sites": [{"id": "s1", "east_km": 3.5, "north_km": -2}, {"id": "s1_pos1", "east_km": 4, "north_km": -2},
                         {"id": "nato_bases", "east_km": -50, "north_km": 0}, {"id": "nato_line1", "east_km": -40, "north_km": 10},
                         {"id": "nato_line2", "east_km": -40, "north_km": -10}, {"id": "nato_entry_w1", "east_km": -30, "north_km": 0}]}
    (tmp_path / "maps" / "c.json").write_text(json.dumps(country), encoding="utf-8")
    base = {"place_map": "maps/c.json",
            "objects": [{"id": "s1", "kind": "sam_site", "at": "s1", "positions": [{"at": "s1_pos1"}]}, {"id": "r", "kind": "refinery", "at": "ref"}],
            "fronts": [{"faction": "nato", "line": [{"at": "nato_line1"}, {"at": "nato_line2"}], "entries": [{"id": "W1", "at": "nato_entry_w1"}],
                        "bases": {"at": "nato_bases"}}]}
    child = {"extends": "base.json", "place_map": "maps/c.json", "objects": [{"id": "c", "kind": "city", "at": "cap"}]}
    (tmp_path / "air_war" / "base.json").write_text(json.dumps(base), encoding="utf-8")
    (tmp_path / "air_war" / "child.json").write_text(json.dumps(child), encoding="utf-8")
    t = map_view.load_theatre(tmp_path / "air_war" / "child.json")
    at = {o["id"]: (o["east_km"], o["north_km"]) for o in t["objects"]}
    assert at == {"s1": (3.5, -2), "r": (5, 2), "c": (1, 1)}
    assert t["objects"][0]["positions"][0]["east_km"] == 4 and t["fronts"][0]["bases"]["east_km"] == -50
    assert t["fronts"][0]["line"][1]["north_km"] == -10 and t["fronts"][0]["entries"][0]["east_km"] == -30
    assert map_view.OBJECT_COLOURS["city"] in map_view.country_svg(COUNTRY, t)
    base["objects"][1]["at"] = "nowhere"
    (tmp_path / "air_war" / "base.json").write_text(json.dumps(base), encoding="utf-8")
    try:
        map_view.load_theatre(tmp_path / "air_war" / "base.json")
        assert False, "a place the map lacks"
    except ValueError as e:
        assert "nowhere" in str(e)


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


def test_routes_are_drawn_over_the_country_each_stretch_in_its_kind():
    routes = {"routes": [{"to": "rgt_cp", "time_s": 1740, "length_m": 21000,
                          "points_km": [[0, 0], [5, 0], [5, 5], [6, 6]], "stretches": ["road", "swim", "off_road"]}]}
    svg = map_view.country_svg(COUNTRY, None, routes=routes)
    for k in ("road", "swim", "off_road"):
        assert map_view.ROUTE_COLOURS[k] in svg
    assert "rgt_cp 29 min" in svg and 'stroke-dasharray' in svg
