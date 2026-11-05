"""Exports one Megaplant tree (a skeletal mesh) to FBX and the textures of its folder to PNG, for the model pipeline.

    SAM3D_PLANT=Tree_Black_Poplar SAM3D_MESH=Tree_Black_Poplar_01_A SAM3D_OUTDIR=art/thirdparty/models/black_poplar_mp \
      UnrealEditor-Cmd SAM3D.uproject -run=pythonscript -script=tools/ue_export_megaplant.py -unattended -nullrhi

Writes <outdir>/<mesh>.fbx and <outdir>/tex/<name>.png (what exports) and prints a MPEXPORT line.
"""
import os

import unreal

plant = os.environ["SAM3D_PLANT"]
mesh = os.environ["SAM3D_MESH"]
outdir = os.environ["SAM3D_OUTDIR"]
os.makedirs(os.path.join(outdir, "tex"), exist_ok=True)
reg = unreal.AssetRegistryHelpers.get_asset_registry()
root = os.environ.get("SAM3D_ROOTPATH", "/Game/Megaplant_Library/" + plant)
want_tex = os.environ.get("SAM3D_TEX", "1") == "1"


def export(asset, path):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = path
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    if path.endswith(".fbx"):
        opt = unreal.FbxExportOption()
        opt.vertex_color = True
        opt.level_of_detail = True
        opt.collision = False
        task.options = opt
    return unreal.Exporter.run_asset_export_task(task)


done = {"fbx": False, "tex": [], "failed": []}
for data in reg.get_assets_by_path(root, recursive=True):
    name = str(data.asset_name)
    cls = str(data.asset_class_path.asset_name)
    if name == mesh and cls in ("SkeletalMesh", "StaticMesh"):
        done["fbx"] = bool(export(data.get_asset(), os.path.join(outdir, name + ".fbx")))
    elif want_tex and cls == "Texture2D" and os.environ.get("SAM3D_TEXFILTER", "") in name and "/Textures" in str(data.package_path):
        ok = export(data.get_asset(), os.path.join(outdir, "tex", name + ".png"))
        (done["tex"] if ok else done["failed"]).append(name)
unreal.log("MPEXPORT " + str(done))
