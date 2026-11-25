"""Import the Sketchfab models prepared by tools/blender/sketchfab_prepare.py (art/thirdparty/sketchfab/<id>/SM_SF_*.glb)
into a SAM3D project under /Game/ThirdParty/Sketchfab/<id>/, without opening the editor:

    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject ^
        -ExecutePythonScript="C:/dev/SAM3D/tools/ue_import_sketchfab.py" -unattended -nullrhi -nosplash

(Runs inside Unreal's Python, in the editor started without a window, which it closes when done: the LODs need the
editor's static mesh subsystem, which the pythonscript commandlet does not have; run so, the meshes come with LOD 0
only and a warning.) SAM3D_SKETCHFAB_ART names the folder of the prepared files when they are not this
checkout's art/thirdparty/sketchfab (one copy shared by worktrees); SAM3D_SKETCHFAB_ONLY, a comma-separated list of ids,
imports only those. Everything it writes is under Content/ThirdParty/ (ignored by git); data/scenery/assets.json names
each mesh by its object path, and FSimMapAssets loads it from there (docs/scenery/ASSETS.md).

For each model it logs a line "SKETCHFAB IMPORT: {...}": the mesh's bounds in centimetres (x the length, nose forward),
its triangles and LODs, and where the nose is (the side of the origin the thinner end is on: +x when the import kept
the model's frame).
"""
import json
import os

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.environ.get('SAM3D_SKETCHFAB_ART') or os.path.join(REPO, 'art', 'thirdparty', 'sketchfab')
ONLY = [s for s in os.environ.get('SAM3D_SKETCHFAB_ONLY', '').split(',') if s]
DEST_ROOT = '/Game/ThirdParty/Sketchfab'
# LOD 1-3 by the share of LOD 0's triangles kept, and the screen size each is drawn below.
LODS = [(1.0, 1.0), (0.5, 0.35), (0.2, 0.12), (0.06, 0.04)]

with open(os.path.join(REPO, 'tools', 'sketchfab', 'models.json'), encoding='utf-8') as f:
    specs = json.load(f)['models']

assets = unreal.EditorAssetLibrary
# The editor's subsystem when there is one; a commandlet has none, and the older library does the same.
mesh_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary


def nose_side(mesh):
    """+1 if the end of the mesh at +x is the thinner one (the nose, a pointed body), -1 if the end at -x is: a check
    that the import kept the model's frame. From LOD 0's vertices in the tenth of the length at each end."""
    lo, hi = [], []
    bounds = mesh.get_bounding_box()
    x0, x1 = bounds.min.x, bounds.max.x
    tenth = 0.1 * (x1 - x0)
    for section in range(mesh.get_num_sections(0)):
        verts = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)[0]
        for v in verts:
            if v.x > x1 - tenth:
                hi.append(v)
            elif v.x < x0 + tenth:
                lo.append(v)

    def spread(vs):
        if not vs:
            return 0.0
        return max(max(v.y for v in vs) - min(v.y for v in vs), max(v.z for v in vs) - min(v.z for v in vs))

    return 1 if spread(hi) < spread(lo) else -1


def import_one(spec):
    aid, name = spec['id'], spec['mesh_name']
    src = os.path.join(ART, aid, name + '.glb')
    if not os.path.exists(src):
        unreal.log_warning('SKETCHFAB IMPORT: %s: no %s (tools/blender/sketchfab_prepare.py)' % (aid, src))
        return
    dest = '%s/%s' % (DEST_ROOT, aid)
    if assets.does_directory_exist(dest):
        assets.delete_directory(dest)
    t = unreal.AssetImportTask()
    t.filename = src
    t.destination_path = dest
    t.automated = True
    t.replace_existing = True
    t.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    meshes = [p for p in assets.list_assets(dest, recursive=True)
              if assets.find_asset_data(p).asset_class_path.asset_name == 'StaticMesh']
    if len(meshes) != 1:
        unreal.log_error('SKETCHFAB IMPORT: %s: %d static meshes imported from %s: %s' % (aid, len(meshes), src, meshes))
        return
    path = '%s/%s' % (dest, name)
    if meshes[0].split('.')[0] != path:
        assets.rename_asset(meshes[0].split('.')[0], path)
    mesh = assets.load_asset(path)
    # Interchange builds Nanite by default; these meshes have translucent glass (canopies), which Nanite does not draw,
    # and LODs are what they need.
    nanite = mesh.get_editor_property('nanite_settings')
    was_nanite = nanite.enabled
    nanite.enabled = False
    mesh.set_editor_property('nanite_settings', nanite)
    options = unreal.StaticMeshReductionOptions()
    options.auto_compute_lod_screen_size = False
    options.reduction_settings = [unreal.StaticMeshReductionSettings(p, s) for p, s in LODS]
    made = mesh_sub.set_lods(mesh, options)
    if made != len(LODS):
        unreal.log_warning('SKETCHFAB IMPORT: %s: set_lods made %s LODs, not %d' % (aid, made, len(LODS)))
    body = mesh.get_editor_property('body_setup')
    if body is not None:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    assets.save_directory(dest, only_if_is_dirty=False, recursive=True)
    b = mesh.get_bounding_box()
    out = {'id': aid, 'path': path + '.' + name, 'size_cm': [round(b.max.x - b.min.x, 1), round(b.max.y - b.min.y, 1), round(b.max.z - b.min.z, 1)],
           'min_cm': [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)], 'max_cm': [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)],
           'lods': mesh.get_num_lods(), 'lod_triangles': [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())], 'materials': len(mesh.static_materials), 'nanite_on_import': was_nanite,
           'nose': nose_side(mesh)}
    unreal.log('SKETCHFAB IMPORT: ' + json.dumps(out))


for spec in specs:
    if not ONLY or spec['id'] in ONLY:
        import_one(spec)
if unreal.SystemLibrary.is_unattended() and unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem):
    unreal.SystemLibrary.quit_editor()  # the editor started for this script (-ExecutePythonScript -unattended)
