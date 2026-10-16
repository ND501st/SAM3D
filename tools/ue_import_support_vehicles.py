"""Import the support vehicles (art/support_vehicles/SM_*.fbx, built by tools/blender/build_support_vehicles.py) into the
SAM3D project under /Game/SAM3D/Vehicles, without opening the editor:

    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject ^
        -run=pythonscript -script="C:/dev/SAM3D/tools/ue_import_support_vehicles.py" -unattended -nullrhi

(Runs inside Unreal's Python.) ASimEntityActor loads them by name for battery entities of kind reload, fuel and
command_post, and falls back to a box when one is missing.
"""
import glob
import os

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = '/Game/SAM3D/Vehicles'

files = sorted(glob.glob(os.path.join(REPO, 'art', 'support_vehicles', 'SM_*.fbx')))
tasks = []
for f in files:
    t = unreal.AssetImportTask()
    t.filename = f
    t.destination_path = DEST
    t.automated = True
    t.replace_existing = True
    t.save = True
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_materials = True
    opts.import_textures = False
    opts.import_as_skeletal = False
    opts.static_mesh_import_data.combine_meshes = True
    opts.static_mesh_import_data.auto_generate_collision = False
    t.options = opts
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    unreal.log('SUPPORT VEHICLE IMPORT: %s -> %s' % (os.path.basename(t.filename), list(t.imported_object_paths)))
