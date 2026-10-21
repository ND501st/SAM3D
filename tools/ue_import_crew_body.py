"""Import the crew's body parts (art/crew_body/SM_Crew_*.fbx, built by tools/blender/build_crew_body.py) into the SAM3D
project under /Game/SAM3D/Crew, without opening the editor (roadmap 8.10):

    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject ^
        -run=pythonscript -script="C:/dev/SAM3D/tools/ue_import_crew_body.py" -unattended -nullrhi

(Runs inside Unreal's Python.) USimCrewBodiesComponent loads the parts by name and colours their material slots
(M_CrewCloth, M_CrewSkin, M_CrewHelmet, M_CrewBoot, M_CrewBelt); engine basic shapes stand in for any not imported.
"""
import glob
import os

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = '/Game/SAM3D/Crew'

files = sorted(glob.glob(os.path.join(REPO, 'art', 'crew_body', 'SM_Crew_*.fbx')))
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
    # Keep each part's origin at its joint (the FBX's own origin): simcore poses the part about it.
    opts.static_mesh_import_data.import_translation = unreal.Vector(0.0, 0.0, 0.0)
    t.options = opts
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    unreal.log('CREW BODY IMPORT: %s -> %s' % (os.path.basename(t.filename), list(t.imported_object_paths)))
