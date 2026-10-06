"""The cabin layout's modelled parts and the meshes tools/blender/build_cabin_interior.py built for them agree.

Each shell part of kind "mesh" in data/cabins/osa_ak.json names a mesh of art/cabin_interior/manifest.json, gives its
bounding box as size_m, and, for a mesh built in place (the hull, the racks...), puts it where it was built.
"""
import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def _load():
    layout = json.loads((REPO / "data" / "cabins" / "osa_ak.json").read_text(encoding="utf-8"))
    manifest = json.loads((REPO / "art" / "cabin_interior" / "manifest.json").read_text(encoding="utf-8"))
    return layout, manifest


def test_every_mesh_part_names_a_built_mesh_with_its_size():
    layout, manifest = _load()
    parts = [p for p in layout["shell"] if p["kind"] == "mesh"]
    assert len(parts) >= 20
    for part in parts:
        assert part["mesh"] in manifest, part["id"]
        built = manifest[part["mesh"]]
        for a, b in zip(part["size_m"], built["size_m"]):
            assert abs(a - b) < 0.005, (part["id"], part["size_m"], built["size_m"])
        if "centre_m" in built:   # built in place: it can only go where it was built
            for a, b in zip(part["centre_m"], built["centre_m"]):
                assert abs(a - b) < 0.005, (part["id"], part["centre_m"], built["centre_m"])
            assert part.get("yaw_deg", 0) == 0, part["id"]


def test_every_built_mesh_is_used():
    layout, manifest = _load()
    used = {p["mesh"] for p in layout["shell"] if p["kind"] == "mesh"}
    assert set(manifest) == used


def test_racks_stand_behind_their_panels():
    """Each rack carcass reaches from its side wall to just behind the face of the console panels it carries."""
    layout, manifest = _load()
    panels = {p["id"]: p for p in layout["panels"]}
    racks = {"SM_RackSL1M": ["gyro_panel"], "SM_RackSL2M": ["fo_panel", "so_panel", "op_panel"],
             "SM_RackSL3M": ["sl3m_panel"], "SM_RackSP1M": ["sp1m_panel"], "SM_RackSP2M": ["sp2m_panel"],
             "SM_RackSP3M": ["sp3m_panel"]}
    for mesh, ids in racks.items():
        built = manifest[mesh]
        x0 = built["centre_m"][0] - built["size_m"][0] / 2
        x1 = built["centre_m"][0] + built["size_m"][0] / 2
        for pid in ids:
            face = panels[pid]["centre_m"][0]
            if face < 0:
                assert x0 < -1.3 and face - 0.04 < x0 + (x1 - x0)   # from the left wall to the face (or beyond: shelves)
            else:
                assert x1 > 1.3 and x0 < face
