"""Import the PU-12M's art into the SAM3D project without opening the editor (docs/PU12_DEVELOPMENT.md P5, P6):

* the exterior (art/pu12m/SM_PU12M*.fbx, tools/blender/pu12m_model.py) -> /Game/SAM3D/Vehicles, as
  tools/ue_import_support_vehicles.py imports it (its own FBX materials);
* the fighting compartment (art/pu12m_interior/SM_*.fbx, tools/blender/build_pu12m_interior.py) ->
  /Game/SAM3D/Cabin/Interior, as tools/ue_import_cabin_interior.py imports the Osa's: no materials from the FBX, each
  slot bound to the cabin's MI_Cabin_<slot> (the PU-12M's light lining, MI_Cabin_Paint_Light, is made if it is missing).

    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject ^
        -run=pythonscript -script="C:/dev/SAM3D/tools/ue_import_pu12m.py" -unattended -nullrhi

(Runs inside Unreal's Python, with the editor closed. SAM3D_PU12M_STEPS=interior imports only the compartment.)
"""
import glob
import os

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VEHICLES = '/Game/SAM3D/Vehicles'
INTERIOR = '/Game/SAM3D/Cabin/Interior'
MAT = '/Game/SAM3D/Cabin/Materials'
TEX = '/Game/SAM3D/Cabin/Textures'
# The lining's distance field finer than the default (a 3 cm skin on a 5 m mesh; as the Osa's hull, 8).
DF_SCALE = {'SM_PU12MHull': 8.0, 'SM_PU12MEquipment': 2.0}
# New material instances of the cabin's master material: colour, roughness, grime, detail normal strength, tiling.
NEW_MATERIALS = {'Paint_Light': ((0.5, 0.51, 0.48), 0.6, 0.25, 0.12, 3.0)}
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def exterior():
    files = sorted(glob.glob(os.path.join(REPO, 'art', 'pu12m', 'SM_*.fbx')))
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = VEHICLES
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
    tools.import_asset_tasks(tasks)
    for t in tasks:
        unreal.log('PU12M EXTERIOR: %s -> %s' % (os.path.basename(t.filename), list(t.imported_object_paths)))


def materials():
    parent = unreal.load_asset(MAT + '/M_CabinPBR')
    if parent is None:
        unreal.log_warning('PU12M: no M_CabinPBR (run tools/ue_import_cabin_interior.py materials first)')
        return
    for name, (col, rough, grime, nstr, tiling) in NEW_MATERIALS.items():
        path = MAT + '/MI_Cabin_' + name
        if lib.does_asset_exist(path):
            continue
        mi = tools.create_asset('MI_Cabin_' + name, MAT, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_vector_parameter_value(mi, 'Color', unreal.LinearColor(col[0], col[1], col[2], 1))
        mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
        mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', 0.0)
        mel.set_material_instance_scalar_parameter_value(mi, 'GrimeAmount', grime)
        mel.set_material_instance_scalar_parameter_value(mi, 'NormalStrength', nstr)
        mel.set_material_instance_scalar_parameter_value(mi, 'DetailTiling', tiling)
        mel.set_material_instance_texture_parameter_value(mi, 'DetailNormal', unreal.load_asset(TEX + '/T_Cabin_PaintN'))
        mel.update_material_instance(mi)
        lib.save_asset(mi.get_path_name())
        unreal.log('PU12M: made ' + path)


def interior():
    files = sorted(glob.glob(os.path.join(REPO, 'art', 'pu12m_interior', 'SM_*.fbx')))
    pipe = unreal.InterchangeGenericAssetsPipeline()
    pipe.get_editor_property('material_pipeline').set_editor_property('import_materials', False)
    common = pipe.get_editor_property('common_meshes_properties')
    common.set_editor_property('force_all_mesh_as_type', unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    mesh_pipe = pipe.get_editor_property('mesh_pipeline')
    for prop, value in (('collision', False), ('build_nanite', True)):
        try:
            mesh_pipe.set_editor_property(prop, value)
        except Exception as e:  # noqa: BLE001 (older pipelines)
            unreal.log_warning('skip %s %s' % (prop, e))
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = INTERIOR
        t.automated = True
        t.replace_existing = True
        t.save = False
        t.options = pipe
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        sm = unreal.load_asset(INTERIOR + '/' + name)
        if sm is None:
            unreal.log_warning('PU12M: %s did not import' % name)
            continue
        for i, slot in enumerate(sm.get_editor_property('static_materials')):
            slot_name = str(slot.material_slot_name)
            mi = unreal.load_asset(MAT + '/' + slot_name) if lib.does_asset_exist(MAT + '/' + slot_name) else None
            if mi is not None:
                sm.set_material(i, mi)
            else:
                unreal.log_warning('PU12M: no material for %s slot %s' % (name, slot_name))
        scale = DF_SCALE.get(name, 1.0)
        if scale != 1.0:
            # the editor's subsystem, or (in a commandlet, where there is none) the static mesh library
            sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
            bs = sub.get_lod_build_settings(sm, 0)
            bs.set_editor_property('distance_field_resolution_scale', scale)
            sub.set_lod_build_settings(sm, 0, bs)
        lib.save_asset(sm.get_path_name())
        unreal.log('PU12M INTERIOR: ' + name)


# SAM3D_PU12M_STEPS=interior (comma-separated: exterior, materials, interior) runs only those; default all.
for step in os.environ.get('SAM3D_PU12M_STEPS', 'exterior,materials,interior').split(','):
    globals()[step.strip()]()
