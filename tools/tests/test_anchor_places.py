"""tools/anchor_places.py moves the theatre's and our chain's coordinates onto the country map as places (roadmap F1)."""
import json
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import anchor_places as ap  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BEFORE = "c450fa6"  # tf-f1 before the migration: the theatres and the places file with their own coordinates

MAP = """{
  "kind": "country",
  "settlements": [
    {"id": "cap", "name": "Capital", "kind": "capital", "east_km": 36, "north_km": 58}
  ],
  "landmarks": [
    {"id": "ref", "name": "Refinery", "kind": "refinery", "east_km": 34, "north_km": 40},
    {"id": "ps", "name": "Station", "kind": "pipeline_station", "east_km": 106, "north_km": 114}
  ]
}
"""
DISTRICT = """{
  "theatre_id": "d",
  "notes": "Fictional.",
  "fronts": [
    {"faction": "nato", "name": "West", "line": [{"east_km": -150, "north_km": 140}, {"east_km": -160, "north_km": 40}], "entries": [{"id": "W1", "east_km": -175, "north_km": 110}], "bases": {"east_km": -300, "north_km": 40, "notes": "Far."}, "battery_position": {"east_km": -44, "north_km": 10}}
  ],
  "objects": [
    {"id": "cap", "name": "The capital", "kind": "city", "east_km": 36.0, "north_km": 58.0},
    {"id": "works", "name": "The works", "kind": "factory", "east_km": -32, "north_km": 47, "value": 0.5},
    {"id": "sam", "name": "A SAM", "kind": "sam_site", "east_km": 0.1, "north_km": -0.30000000000000004, "mobile": true,
      "positions": [{"east_km": 24.0, "north_km": 78.0}, {"east_km": 1e-3, "north_km": 7.25}]}
  ]
}
"""
NATION = """{
  "theatre_id": "n",
  "extends": "district.json",
  "notes": "The nation.",
  "fronts": [
    {"faction": "nato", "name": "West", "line": [{"east_km": -150, "north_km": 140}, {"east_km": -160, "north_km": 40}], "entries": [{"id": "W1", "east_km": -175, "north_km": 110}], "bases": {"east_km": -300, "north_km": 40}}
  ],
  "objects": [
    {"id": "works", "name": "The works", "kind": "factory", "east_km": -32, "north_km": 47},
    {"id": "refinery", "name": "The refinery", "kind": "refinery", "east_km": 34, "north_km": 40},
    {"id": "ps", "name": "The station", "kind": "pipeline_station", "east_km": 105.5, "north_km": 113.5}
  ]
}
"""
PLACES = """{
  "places_id": "p",
  "theatre": "air_war/district.json",
  "places": [
    {"id": "cap", "name": "Capital", "kind": "city", "east_km": 36, "north_km": 58},
    {"id": "nv_works", "name": "The works' gate", "kind": "industrial", "east_km": -32, "north_km": 47, "theatre_object": "works"},
    {"id": "hall", "name": "Hall", "kind": "civil", "east_km": -30, "north_km": 40.5}
  ]
}
"""


def run(crossings=None):
    return ap.migrate(MAP, [DISTRICT, NATION], PLACES, crossings)


def test_every_position_names_a_place_and_resolves_to_the_same_double():
    new_map, (d, n), p, plan, moved = run()
    # 3 objects + 2 positions + 2 front points; 3 objects + 1 front point; 3 places.
    assert moved == 14
    m, dt, nt, pl = json.loads(new_map), json.loads(d), json.loads(n), json.loads(p)
    at = {o["id"]: o["at"] for o in dt["objects"] + nt["objects"]}
    assert at == {"cap": "cap", "works": "works", "sam": "sam", "refinery": "ref", "ps": "ps_site"}
    assert [x["at"] for x in dt["objects"][2]["positions"]] == ["sam_pos1", "sam_pos2"]
    assert dt["fronts"][0]["bases"] == {"at": "nato_bases", "notes": "Far."} and nt["fronts"][0]["bases"] == {"at": "nato_bases"}
    assert dt["fronts"][0]["battery_position"] == {"at": "nato_battery_position"}
    # Places at an object's or a settlement's exact point share it; the rest get their own.
    assert [x["at"] for x in pl["places"]] == ["cap", "works", "hall"]
    assert [s["id"] for s in m["sites"]] == ["nato_bases", "nato_battery_position", "works", "sam", "sam_pos1", "sam_pos2", "ps_site", "hall"]
    # A front's line and entries stay its own coordinates (tracks F2, F3).
    assert dt["fronts"][0]["line"][0] == {"east_km": -150, "north_km": 140} and dt["fronts"][0]["entries"][0]["east_km"] == -175
    # Each site keeps the old number's text, so km * 1000.0 is the same double.
    old = {o["id"]: o for o in json.loads(DISTRICT)["objects"]}
    places = ap.Plan(m).at
    for oid in ("works", "sam"):
        e, nn = places[at[oid]]
        assert e * 1000.0 == float(old[oid]["east_km"]) * 1000.0 and nn * 1000.0 == float(old[oid]["north_km"]) * 1000.0
    assert '"east_km": 0.1, "north_km": -0.30000000000000004' in new_map and '"east_km": 1e-3, "north_km": 7.25' in new_map
    assert places["sam_pos2"] == (0.001, 7.25)
    for text in (d, n, p):
        assert '"place_map": "maps/pannonia.json"' in text


def test_the_migration_runs_once():
    new_map, (d, n), p, _, _ = run()
    again = ap.migrate(new_map, [d, n], p)
    assert again[4] == 0 and again[0] == new_map and again[1] == [d, n] and again[2] == p


def test_a_bridge_site_names_its_crossing():
    district = DISTRICT.replace('"kind": "factory"', '"kind": "bridge"', 1)
    crossings = [{"id": "r@river", "net": "road", "river": "river", "at": [-32.3, 47.1]},
                 {"id": "far@river", "net": "road", "river": "river", "at": [-40, 47]}]
    new_map, *_ = ap.migrate(MAP, [district, NATION], PLACES, crossings)
    sites = {s["id"]: s for s in json.loads(new_map)["sites"]}
    assert sites["works"]["crossing"] == "r@river" and "crossing" not in sites["sam"]


def test_odd_input_is_refused():
    with pytest.raises(ValueError):
        ap.migrate(MAP.replace('"id": "ps"', '"id": "cap"'), [DISTRICT, NATION], PLACES)  # one id twice on the map
    with pytest.raises(ValueError):
        ap.migrate(MAP, [DISTRICT.replace('"east_km": -32, "north_km": 47', '"north_km": 47, "east_km": -32', 1), NATION], PLACES)


def git_show(rel):
    try:
        r = subprocess.run(["git", "show", f"{BEFORE}:{rel}"], cwd=ROOT, capture_output=True, timeout=30)
    except (OSError, subprocess.SubprocessError):
        return None
    return r.stdout.decode("utf-8") if r.returncode == 0 else None


def test_the_real_data_stands_on_the_map_where_it_stood_before():
    """Every anchored point of the theatres and the chain's places, read through the map now, is the same double as
    its own km * 1000.0 was before the migration (BEFORE, when git has it)."""
    data = ROOT / "data"
    at = ap.Plan(json.loads((data / ap.MAP).read_text(encoding="utf-8"))).at
    files = [("theatre", t) for t in ap.THEATRES] + [("places", ap.PLACES)]
    for kind, rel in files:
        now = json.loads((data / rel).read_text(encoding="utf-8"))
        assert now.get("place_map") == ap.MAP
        for path, _ in ap.blocks(now):
            assert ap.role(kind, path) is None, f"{rel} {path}: coordinates where a place belongs"
        resolved = ap.resolve(kind, now, at)  # raises KeyError on a place the map lacks
        old_text = git_show(f"data/{rel}")
        if old_text is None:
            pytest.skip(f"git has no {BEFORE}")
        old = ap.resolve(kind, json.loads(old_text), at)
        assert old == resolved, f"{rel}: a point moved"
        assert len(old) > 30
