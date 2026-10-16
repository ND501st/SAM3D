"""The crew action map (tools/crew_map.py) covers every action in the data and the checked-in copy is up to date."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import crew_map  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent


def test_checked_in_map_is_up_to_date():
    assert crew_map.main(["osa_ak", "--check"]) == 0, "run: python tools/crew_map.py osa_ak"


def test_every_control_procedure_subsystem_and_input_is_in_the_map():
    text = crew_map.build("osa_ak")
    con = json.loads((ROOT / "data/procedures/osa_ak.json").read_text(encoding="utf-8"))
    for c in con["controls"]:
        assert f"| `{c['id']}` |" in text, c["id"]
    for p in con["procedures"]:
        assert f"### `{p['id']}`" in text, p["id"]
    for s in con["subsystems"]:
        assert f"| `{s['id']}` |" in text, s["id"]
    for i in con["inputs"]:
        assert f"| `{i['id']}` |" in text, i["id"]


def test_the_wait_matrix_shows_the_interdependence():
    text = crew_map.build("osa_ak")
    row = next(line for line in text.splitlines() if line.startswith("| **operator** |"))
    assert "`equipment_on`" in row  # the operator's switch-on waits for the driver's power
    row = next(line for line in text.splitlines() if line.startswith("| **deputy_commander** |"))
    assert "`launch`" in row  # the launch waits for the operator's equipment and the commander's hatches
