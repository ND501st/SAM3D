"""Reports what the Megaplant tree packs in /Game/Megaplant_Library are: each mesh's class, triangles per LOD, whether it is
Nanite, its size and its materials. Writes JSON to the path in SAM3D_OUT.

    SAM3D_OUT=out/megaplants.json UnrealEditor-Cmd SAM3D.uproject -run=pythonscript -script=tools/ue_inspect_megaplants.py -unattended -nullrhi
"""
import json
import os

import unreal

ROOT = os.environ.get("SAM3D_ROOT", "/Game/Megaplant_Library")
out = {}
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for data in reg.get_assets_by_path(ROOT, recursive=True):
    name = str(data.asset_name)
    cls = str(data.asset_class_path.asset_name)
    if cls not in ("StaticMesh", "SkeletalMesh") or name.endswith("_Skeleton"):
        continue
    if cls not in ("StaticMesh", "SkeletalMesh", "NaniteAssemblyMesh", "GeometryCollection"):
        out[name] = {"class": cls}
        continue
    asset = data.get_asset()
    info = {"class": cls}
    try:
        if cls == "StaticMesh":
            info["lods"] = asset.get_num_lods()
            info["verts"] = [unreal.EditorStaticMeshLibrary.get_number_verts(asset, i) for i in range(asset.get_num_lods())]
            info["nanite"] = bool(asset.get_editor_property("nanite_settings").enabled)
            b = asset.get_bounds()
            info["size_cm"] = [round(b.box_extent.x * 2), round(b.box_extent.y * 2), round(b.box_extent.z * 2)]
        else:
            b = asset.get_bounds()
            info["size_cm"] = [round(b.box_extent.x * 2), round(b.box_extent.y * 2), round(b.box_extent.z * 2)]
        info["materials"] = [str(m.material_slot_name) for m in (asset.static_materials if cls == "StaticMesh" else [])]
    except Exception as e:  # reported, not fatal
        info["error"] = str(e)
    out[name] = info
path = os.environ.get("SAM3D_OUT", "megaplants.json")
with open(path, "w") as f:
    json.dump(out, f, indent=1)
unreal.log("MEGAPLANTS " + path + " " + str(len(out)))
