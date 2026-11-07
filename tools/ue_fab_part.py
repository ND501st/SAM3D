"""Makes a Fab asset in the project (a Megaplant, a Megascans plant) into a map part, inside Unreal, and registers it
(docs/scenery/TERRAIN_ROADMAP.md D7 and A.3.25: Unreal assets used in place, never exported through Blender).

    set SAM3D_MODEL=hornbeam_mp_a            (or several, comma-separated; or "fab" for every pick with an unreal_source)
    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript ^
        -script="C:/dev/SAM3D-wt/terrain/tools/ue_fab_part.py" -unattended -nullrhi

A pick in data/scenery/model_picks.json with "unreal_source" names the Fab asset's object path (a skeletal Megaplant or a
static Megascans mesh, as the Fab window or the Procedural Vegetation Editor put it in the project). Geometry Script
copies its highest level of detail into a dynamic mesh, fits it to the part's tree frame (one unit, 100 cm, tall from
its base and across its crown, the pivot left at the trunk's foot) and saves it as a Nanite static mesh under
/Game/ThirdParty/Fab/<id>/ (ignored by git: Fab's Standard licence forbids handing the files on), with the source's own
materials slot by slot. The wind of a Megaplant (its bones) does not come across. Then the asset and the part's entry
(its alternate: another model of the part, chosen tree by tree) go into data/scenery/assets.json. Nothing is committed.
"""
import json
import os

import unreal

ROOT = 'C:/dev/SAM3D-wt/terrain'
PICKS = ROOT + '/data/scenery/model_picks.json'
REGISTER = ROOT + '/data/scenery/assets.json'
DEST = '/Game/ThirdParty/Fab'
GS = unreal


def log(msg):
    unreal.log('FAB PART: ' + msg)


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def copy_source(pick):
    """The source's mesh as a dynamic mesh, and its materials by material id."""
    src = unreal.load_asset(pick['unreal_source'])
    if src is None:
        raise RuntimeError('%s is not in the project (add it from Fab: docs/scenery/ASSETS.md)' % pick['unreal_source'])
    mesh = unreal.DynamicMesh()
    opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    lod = unreal.GeometryScriptMeshReadLOD()
    if isinstance(src, unreal.SkeletalMesh):
        lod.set_editor_property('lod_type', unreal.GeometryScriptLODType.SOURCE_MODEL)
        mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(src, mesh, opts, lod)
        materials = [m.material_interface for m in src.materials]
    elif isinstance(src, unreal.StaticMesh):
        lod.set_editor_property('lod_type', unreal.GeometryScriptLODType.HI_RES_SOURCE_MODEL
                                if src.is_hi_res_mesh_description_valid() else unreal.GeometryScriptLODType.SOURCE_MODEL)
        mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(src, mesh, opts, lod)
        materials = [m.material_interface for m in src.static_materials]
    else:
        raise RuntimeError('%s is a %s, not a mesh' % (pick['unreal_source'], type(src).__name__))
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('Geometry Script could not copy %s' % pick['unreal_source'])
    q = unreal.GeometryScript_MeshQueries
    log('copied: %d triangles, %d vertices, %d materials; ids dense: %s' % (
        q.get_num_triangle_i_ds(mesh), q.get_num_vertex_i_ds(mesh), len(materials), q.get_is_dense_mesh(mesh)))
    # A skeletal mesh's copy has gaps in its ids; saving it as a static mesh then reads past an array (an assertion).
    # Compacted, and its skin weights and bones dropped, it saves.
    unreal.GeometryScript_MeshRepair.compact_mesh(mesh)
    keep = pick.get('material_slots')
    if keep is not None:
        # Drop the slots the part does not want (a Megascans billboard's card): their triangles and the slot itself.
        for i in range(len(materials)):
            if i not in keep:
                gone = unreal.GeometryScript_MeshMaterials.delete_triangles_by_material_id(mesh, i)
                log('slot %d (%s): %s triangles taken out' % (i, materials[i].get_name() if materials[i] else None, gone))
    return mesh, materials


def fit(mesh, pick):
    """The tree frame: its base at z 0 and one unit tall, its crown one unit across (the mean of its two widths), the
    pivot where the source has it (a Megaplant's at the trunk's foot)."""
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    lo, hi = box.min, box.max
    width = 0.5 * ((hi.x - lo.x) + (hi.y - lo.y))
    height = hi.z
    if height <= 0.0 or width <= 0.0:
        raise RuntimeError('the source has no height or width: %s %s' % (lo, hi))
    log('source %.0f x %.0f x %.0f cm (base %.0f), pivot off the box centre (%.0f, %.0f) cm' % (
        hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, lo.z, 0.5 * (hi.x + lo.x), 0.5 * (hi.y + lo.y)))
    s = unreal.Vector(100.0 / width, 100.0 / width, 100.0 / height)
    unreal.GeometryScript_MeshTransforms.scale_mesh(mesh, s, unreal.Vector(0.0, 0.0, 0.0))
    return width / 100.0, height / 100.0


def seasoned(material, pick, folder):
    """A foliage material in the pick's season (A.3.25): a child instance of the Megaplant's own, kept with the mesh under
    /Game/ThirdParty/Fab/<id>/, with the master's Seasons switch on and the pick's two colours (MA_Foliage_Trees blends
    the leaves towards them). The Fab files themselves are not changed. A pick without "season" keeps the source's."""
    season = pick.get('season')
    if not season:
        return material
    lib = unreal.MaterialEditingLibrary
    name = 'MI_%s_%s' % (pick['id'], season['name'])
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.MaterialInstanceConstant,
                                                                 unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', material)
    lib.set_material_instance_static_switch_parameter_value(mi, 'Seasons', True)
    for key, param in (('color_1', 'Season Color 1'), ('color_2', 'Season Color 2')):
        r, g, b = season[key]
        lib.set_material_instance_vector_parameter_value(mi, param, unreal.LinearColor(r, g, b, 1.0))
    for param, value in season.get('scalars', {}).items():
        lib.set_material_instance_scalar_parameter_value(mi, param, value)
    lib.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    log('%s: %s, Seasons on, %s / %s' % (material.get_name(), name, season['color_1'], season['color_2']))
    return mi


def save(mesh, materials, pick):
    path = '%s/%s/SM_%s' % (DEST, pick['id'], pick['id'])
    folder = '%s/%s' % (DEST, pick['id'])
    if unreal.EditorAssetLibrary.does_directory_exist(folder):
        unreal.EditorAssetLibrary.delete_directory(folder)
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.set_editor_property('enable_nanite', True)
    opts.set_editor_property('enable_collision', False)
    opts.set_editor_property('enable_recompute_normals', False)
    opts.set_editor_property('enable_recompute_tangents', False)
    sm, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, opts)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS or sm is None:
        raise RuntimeError('could not save %s' % path)
    slots = len(sm.static_materials)
    for i in range(min(slots, len(materials))):
        if materials[i] is not None:
            sm.set_material(i, seasoned(materials[i], pick, folder) if 'Foliage' in materials[i].get_name() else materials[i])
    unreal.EditorAssetLibrary.save_loaded_asset(sm)
    tris = unreal.GeometryScript_MeshQueries.get_num_triangle_i_ds(mesh)
    log('saved %s: %d triangles, %d slots %s, Nanite' % (path, tris, slots,
        [m.material_interface.get_name() if m.material_interface else None for m in sm.static_materials]))
    return '%s.SM_%s' % (path, pick['id']), tris


def register(pick, mesh_path, size_m, tris):
    reg = load(REGISTER)
    entry = {
        'id': pick['id'],
        'unreal_path': mesh_path,
        'origin': pick['origin'],
        'author': pick['author'],
        'licence': pick['licence'],
        'redistributable': False,
        'fetched': pick['fetched'],
        'sha256': '',
        'scale': 1.0,
        'yaw_deg': pick.get('yaw_deg', 0.0),
        'z_offset_m': 0.0,
        'notes': '%s (from %s; as drawn %.1f m across, %.1f m tall; %d triangles)' % (
            pick.get('notes', ''), pick['unreal_source'], size_m[0], size_m[1], tris),
    }
    reg['assets'] = [a for a in reg.get('assets', []) if a['id'] != pick['id']] + [entry]
    part = reg.setdefault('parts', {}).setdefault(pick['part'], {'asset': None})
    # A Fab model draws the whole part: its alternates (A.3.25), chosen tree by tree by place. The part's earlier ladder of
    # thinner variants (a card tree's levels) goes; Nanite is the Fab model's level of detail.
    k = pick.get('alternate', 0)
    alternates = part.get('alternates') or []
    while len(alternates) <= k:
        alternates.append(None)
    alternates[k] = pick['id']
    part['alternates'] = alternates
    part['asset'] = alternates[0]
    part.pop('variants', None)
    with open(REGISTER, 'w', encoding='utf-8', newline='') as f:
        json.dump(reg, f, indent=2)
        f.write('\n')
    log('registered %s as %s alternate %d' % (pick['id'], pick['part'], k))


def main():
    wanted = os.environ.get('SAM3D_MODEL', '')
    picks = [p for p in load(PICKS)['picks'] if p.get('unreal_source')]
    if wanted != 'fab':
        names = wanted.split(',')
        picks = [p for p in picks if p['id'] in names]
    if not picks:
        raise RuntimeError('SAM3D_MODEL=%r names no pick with an unreal_source in %s' % (wanted, PICKS))
    for pick in picks:
        log('--- %s from %s' % (pick['id'], pick['unreal_source']))
        mesh, materials = copy_source(pick)
        size_m = fit(mesh, pick)
        mesh_path, tris = save(mesh, materials, pick)
        if os.environ.get('SAM3D_NO_REGISTER') != '1':
            register(pick, mesh_path, size_m, tris)


main()
