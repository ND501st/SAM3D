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
(as a near model: drawn within near_m of the camera, chosen tree by tree; the part's cards beyond) go into data/scenery/assets.json. Nothing is committed.
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


def flare_top_cm(mesh, height_cm):
    """Where the root flare ends (A.3.27, a pick's "sink_m": "auto"): the lowest height whose slice of the trunk is no
    wider than 1.25 times the trunk's own width, measured over 2-3 m up. Slices 10 cm deep, of the vertices within 2.5 m
    of the pivot's axis; a slice's width is the 90th percentile of their distance from the axis. Capped at 2 m."""
    got = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, True)
    pts = next((g for g in (got if isinstance(got, tuple) else (got,)) if isinstance(g, unreal.GeometryScriptVectorList)))
    arr = unreal.GeometryScript_List.convert_vector_list_to_array(pts)
    slices = {}
    for p in arr:
        if -50.0 <= p.z < 250.0:
            r = (p.x * p.x + p.y * p.y) ** 0.5
            if r < 300.0:
                slices.setdefault(int(p.z // 10.0), []).append(r)

    def width(k):
        rs = sorted(slices.get(k, []))
        return rs[int(0.9 * (len(rs) - 1))] if len(rs) >= 8 else None

    profile = [width(k) for k in range(25)]
    trunk = sorted(w for w in profile[8:20] if w is not None)
    if not trunk:
        log('flare: no trunk found 0.8-2 m up, not sunk')
        return 0.0
    trunk_r = trunk[len(trunk) // 2]
    top = 0.0
    for k in range(15):
        w = profile[k]
        if w is not None and w <= 1.3 * trunk_r:
            break
        top = 10.0 * (k + 1)
    log('flare: widest %s' % ' '.join('%.0f' % max(slices[k]) if k in slices else '-' for k in range(25)))
    log('flare: profile %s; trunk %.0f cm across, flare to %.0f cm of %.0f; sunk by that' % (
        ' '.join('%.0f' % w if w is not None else '-' for w in profile), 2.0 * trunk_r, top, height_cm))
    return top


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
    # A model whose foot is meant to be buried (a hornbeam's root flare) goes down by the pick's sink_m first (A.3.26):
    # what is left above z 0 is the tree the frame fits.
    sink = pick.get('sink_m', 0.0)
    sink_cm = flare_top_cm(mesh, hi.z) if sink == 'auto' else 100.0 * sink
    if sink_cm > 0.0:
        unreal.GeometryScript_MeshTransforms.translate_mesh(mesh, unreal.Vector(0.0, 0.0, -sink_cm))
        height -= sink_cm
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
    lib = unreal.MaterialEditingLibrary
    for i in range(min(slots, len(materials))):
        if materials[i] is not None:
            log('slot %d %s: vectors %s, scalars %s' % (i, materials[i].get_name(),
                [str(n) for n in lib.get_vector_parameter_names(materials[i])],
                [str(n) for n in lib.get_scalar_parameter_names(materials[i])]))
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
    # A Fab model is a near model of the part (A.3.26): drawn within the pick's near_m of the camera, picked tree by tree by
    # place, the part's own (its cards) beyond. Drawn everywhere it cost 150-1340 ms a frame (A.3.25).
    k = pick.get('alternate', 0)
    near = part.setdefault('near', {'alternates': [], 'within_m': pick.get('near_m', 80.0)})
    ids = near['alternates']
    while len(ids) <= k:
        ids.append(None)
    ids[k] = pick['id']
    with open(REGISTER, 'w', encoding='utf-8', newline='') as f:
        json.dump(reg, f, indent=2)
        f.write('\n')
    log('registered %s as %s near model %d (within %.0f m)' % (pick['id'], pick['part'], k, near['within_m']))


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
