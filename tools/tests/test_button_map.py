"""Tests for tools/button_map.py: control states, layout order and the page for the real Osa files."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import button_map  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def _console():
    return {
        "system_id": "test",
        "stations": [{"id": "operator", "label": "Operator"}],
        "launch": {"button": "launch", "armed": "armed"},
        "inputs": [{"id": "in_zone", "description": "target in zone"}],
        "controls": [
            {"id": "power", "label": "POWER", "kind": "switch", "station": "operator", "position_labels": ["off", "on"]},
            {"id": "cover", "label": "guard", "kind": "switch", "station": "operator", "position_labels": ["closed", "open"]},
            {"id": "hv", "label": "HV", "kind": "switch", "station": "operator", "guard": "cover", "position_labels": ["off", "on"]},
            {"id": "lamp_test", "label": "LAMP TEST", "kind": "button", "station": "operator"},
            {"id": "spare", "label": "SPARE", "kind": "switch", "station": "operator"},
            {"id": "launch", "label": "LAUNCH", "kind": "button", "station": "operator"},
        ],
        "subsystems": [
            {"id": "pwr", "name": "Power", "enable": [{"control": "power", "is": 1}]},
            {"id": "tx", "name": "Transmitter", "enable": [{"control": "hv", "is": 1}, {"subsystem": "pwr", "is": "ready"}]},
            {"id": "armed", "name": "Armed", "enable": [{"subsystem": "tx", "is": "ready"}, {"input": "in_zone", "is": True}]},
        ],
        "procedures": [{"id": "p", "name": "Go", "steps": [{"control": "power", "position": 1}, {"control": "hv", "position": 1}]}],
    }


def _cabin():
    return {
        "system_id": "test",
        "panels": [{"id": "panel", "label": "Panel", "station": "operator"}],
        "controls": [{"control": c, "panel": "panel"} for c in ("power", "hv", "lamp_test", "spare", "launch")],
        "lamps": [{"id": "ready", "label": "READY", "panel": "panel", "color": "green",
                   "lit_when": [{"subsystem": "tx", "is": "ready"}], "lit_when_any": [{"control": "lamp_test", "is": 1}]}],
        "displays": [],
    }


def _nodes(mb):
    return {n["id"]: n for n in mb.data()["nodes"]}


def test_control_states():
    nodes = _nodes(button_map.build(_console(), _cabin()))
    assert nodes["c:power"]["status"] == "wired"
    assert nodes["c:hv"]["status"] == "wired"
    assert nodes["c:cover"]["status"] == "wired"          # guards a wired control
    assert nodes["c:lamp_test"]["status"] == "indicator"  # only lights a lamp
    assert nodes["c:spare"]["status"] == "inert"
    assert nodes["c:launch"]["status"] == "wired"


def test_guard_without_cabin_entry_sits_on_the_guarded_panel():
    nodes = _nodes(button_map.build(_console(), _cabin()))
    assert nodes["c:cover"]["panel"] == "panel" and nodes["c:cover"]["cover_auto"]


def test_columns_follow_dependencies():
    nodes = _nodes(button_map.build(_console(), _cabin()))
    assert nodes["c:power"]["col"] == 0 and nodes["i:in_zone"]["col"] == 0
    assert nodes["s:pwr"]["col"] < nodes["s:tx"]["col"] < nodes["s:armed"]["col"] < nodes["e:launch"]["col"]
    assert nodes["l:ready"]["col"] > nodes["s:tx"]["col"]


def test_conditions_carry_position_labels_and_procedures():
    mb = button_map.build(_console(), _cabin())
    e = next(e for e in mb.edges if e["s"] == "c:hv" and e["t"] == "s:tx")
    assert e["k"] == "enable" and e["v"] == "on"
    assert _nodes(mb)["c:power"]["procs"] == ["p"]


def test_unknown_reference_is_a_problem():
    console = _console()
    console["subsystems"][0]["enable"].append({"control": "nope", "is": 1})
    mb = button_map.build(console, _cabin())
    assert any("nope" in p for p in mb.problems)


def test_osa_map_is_clean(tmp_path):
    out = tmp_path / "osa.html"
    assert button_map.main(["osa_ak", "--out", str(out), "--check"]) == 0
    page = out.read_text(encoding="utf-8")
    assert "__DATA__" not in page
    data = json.loads(page.split("const DATA = ", 1)[1].split(";\nconst N =", 1)[0])
    ids = {n["id"] for n in data["nodes"]}
    assert "c:fo.launch" in ids and "e:launch" in ids and "p:channels" in ids
    assert data["counts"]["controls"] == len(json.loads((ROOT / "data/procedures/osa_ak.json").read_text(encoding="utf-8"))["controls"])


def test_osa_beam_selector_feeds_the_search_picture_and_lights_the_beam_lamps():
    console = json.loads((ROOT / "data/procedures/osa_ak.json").read_text(encoding="utf-8"))
    cabin = json.loads((ROOT / "data/cabins/osa_ak.json").read_text(encoding="utf-8"))
    b = button_map.build(console, cabin)
    edges = {(e["s"], e["t"]) for e in b.edges}
    assert ("c:soc.beam", "m:search") in edges
    for n in (1, 2, 3):
        assert ("m:search", f"i:search_beam_{n}") in edges
    planned = {p[0] for p in button_map.PLANNED["osa_ak"]}
    assert "p:beams" not in planned and "p:manualtv" not in planned  # built (roadmap 6.4, 6.6)
