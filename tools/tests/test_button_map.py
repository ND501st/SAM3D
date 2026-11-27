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
    assert "c:fo.launch" in ids and "e:launch" in ids and "p:0062" in ids and "m:launcher" in ids
    assert "p:eccm" not in ids and "p:qcscale" not in ids  # built: 9.4 counter-jamming, 6.5 Qc scale and drift
    assert "p:mc" not in ids and "p:fk_meters" not in ids  # built: МЦ (C9), the function-check meters (C4)
    # The vehicle's systems (data/vehicles/osa_ak_systems.json) and the checks feed their models: none of it inert.
    assert {"m:veh_engine", "m:veh_drive", "m:veh_lights", "m:veh_fire", "m:checks"} <= ids
    status = {n["id"]: n.get("status") for n in data["nodes"]}
    for cid in ("veh.starter", "veh.gear", "veh.light_switch", "fire.button", "fk.systems", "err.output", "ssc.noise_fire", "fo.mc",
                "ind.select", "pwr.relay_heat"):
        assert status["c:" + cid] in ("wired", "model"), (cid, status["c:" + cid])
    assert "c:ssc.rgpo" in ids and "i:range_pulled" in ids and "m:orient" in ids
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


def test_osa_launcher_reads_the_channel_selector_and_lights_the_channel_lamps():
    console = json.loads((ROOT / "data/procedures/osa_ak.json").read_text(encoding="utf-8"))
    cabin = json.loads((ROOT / "data/cabins/osa_ak.json").read_text(encoding="utf-8"))
    b = button_map.build(console, cabin)
    edges = {(e["s"], e["t"]) for e in b.edges}
    assert ("c:msl.channels", "m:launcher") in edges and ("i:target_near_zone", "m:launcher") in edges
    for inp in ("ch1_zakhvat", "ch2_k3", "ammo_3_i", "launch_hold"):
        assert ("m:launcher", "i:" + inp) in edges
    planned = {p[0] for p in button_map.PLANNED["osa_ak"]}
    assert not planned & {"p:containers", "p:select", "p:channels", "p:chanlamps", "p:fuse"}  # built in 6.3


def test_picture_knobs_meter_selectors_and_named_elsewhere():
    con = _console()
    con["controls"] += [
        {"id": "bright", "label": "BRIGHT", "kind": "wheel", "station": "operator"},
        {"id": "meter_sel", "label": "METER", "kind": "rotary", "station": "operator"},
        {"id": "by_code", "label": "CALL", "kind": "button", "station": "operator"},
        {"id": "drilled", "label": "HEAT", "kind": "switch", "station": "operator"},
    ]
    cab = _cabin()
    cab["displays"] = [
        {"id": "scope", "label": "Scope", "kind": "ppi", "panel": "panel", "picture": {"brightness": "bright"}},
        {"id": "meter", "label": "Meter", "kind": "meter", "panel": "panel",
         "levels": [{"when": [{"control": "meter_sel", "is": 1}], "value": 3}]},
    ]
    refs = {"by_code": {"code": ["simcore/include/simcore/x.hpp"]}, "drilled": {"drill": ["data/crew/test_crew.json"]}}
    mb = button_map.build(con, cab, refs)
    nodes = _nodes(mb)
    assert nodes["c:bright"]["status"] == "display"
    assert nodes["c:meter_sel"]["status"] == "display"
    assert nodes["c:by_code"]["status"] == "code"
    assert nodes["c:drilled"]["status"] == "drill"
    assert nodes["c:spare"]["status"] == "inert"
    counts = mb.data()["counts"]
    assert sum(counts[s] for s in button_map.STATES) == counts["controls"]


def test_the_list_names_every_control(tmp_path):
    mb = button_map.build(_console(), _cabin())
    out = tmp_path / "list.md"
    button_map.write_list(mb, out)
    text = out.read_text(encoding="utf-8")
    for cid in ("power", "hv", "lamp_test", "spare", "launch"):
        assert f"`{cid}`" in text
    assert "## inert (1)" in text


def test_find_refs_reads_the_crew_files():
    refs = button_map.find_refs(["gyro.heat", "pvs1.call", "no.such_control"])
    assert any("osa_ak_crew.json" in f for f in refs["gyro.heat"]["drill"])
    assert any("scenario_runner.hpp" in f for f in refs["pvs1.call"]["code"])
    assert "no.such_control" not in refs
