"""Imports a model normalised for its part's frame into Unreal and registers it (roadmap T5-T8,
docs/scenery/T5_T8_PLAN.md, docs/scenery/ASSETS.md). Two steps, the first outside Unreal:

    python tools/prepare_model.py crate_test                  # Blender: join the parts, fit the frame, bake the pivot
    set SAM3D_MODEL=crate_test
    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript ^
        -script="C:/dev/SAM3D-wt/terrain/tools/ue_import_model.py" -unattended -nullrhi

The model is a pick in data/scenery/model_picks.json (its source folder under art/thirdparty/models/, the part it replaces,
that part's frame: cube or tree, see tools/blender/normalize_model.py). prepare_model.py writes <folder>/normalized/<id>.glb,
one mesh already the size and pivot of the frame (a unit, 1 m, which Unreal makes 100 cm), so here the register's scale is 1
and its z_offset_m 0. This script imports that file (one static mesh under /Game/ThirdParty/Models/<id>/, ignored by git),
checks its bounds against the frame, and adds the asset (id, path, origin, author, licence, the source file's SHA-256) and the
part's entry to data/scenery/assets.json. Nothing is committed.
"""
import hashlib
import json
import os

import unreal

ROOT = 'C:/dev/SAM3D-wt/terrain'
PICKS = ROOT + '/data/scenery/model_picks.json'
REGISTER = ROOT + '/data/scenery/assets.json'
SOURCES = ROOT + '/art/thirdparty/models'
DEST = '/Game/ThirdParty/Models'


def log(msg):
    unreal.log('MODEL IMPORT: ' + msg)


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def dest_of(pick):
    """Our own work (licence Own) is kept in the project (/Game/SAM3D/Map/Cards, committed); a fetched model under
    /Game/ThirdParty/Models (ignored by git)."""
    return ('/Game/SAM3D/Map/Cards/' if pick['licence'] == 'Own' else DEST + '/') + pick['id']


def import_mesh(pick):
    """Imports the normalised file as one static mesh."""
    base = ROOT + '/' + pick['root'] if pick.get('root') else SOURCES + '/' + pick['folder']
    src = base + '/normalized/' + pick['id'] + '.glb'
    if not os.path.exists(src):
        raise RuntimeError('%s is not there: run tools/prepare_model.py %s first' % (src, pick['id']))
    dest = dest_of(pick)
    if unreal.EditorAssetLibrary.does_directory_exist(dest):
        unreal.EditorAssetLibrary.delete_directory(dest)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', src)
    task.set_editor_property('destination_path', dest)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = [str(p) for p in task.get_editor_property('imported_object_paths')]
    meshes = [p for p in paths if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    if len(meshes) != 1:
        raise RuntimeError('%d static meshes came of %s (the normalised file has one): %s' % (len(meshes), src, paths))
    log('imported %s' % meshes[0])
    mesh = unreal.load_asset(meshes[0])
    log('%d triangles, %d material slots: %s' % (mesh.get_num_triangles(0) if hasattr(mesh, 'get_num_triangles') else -1,
        len(mesh.static_materials), [str(m.material_slot_name) for m in mesh.static_materials][:12]))
    if pick.get('foliage'):
        foliage(mesh, pick)
    if pick.get('lods'):
        make_lods(mesh, pick['lods'])
    if pick.get('nanite'):
        # Nanite draws a million-triangle plant at its distance's cost; the foliage's masked materials are supported.
        nanite = mesh.get_editor_property('nanite_settings')
        nanite.set_editor_property('enabled', True)
        mesh.set_editor_property('nanite_settings', nanite)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        log('Nanite on')
    return meshes[0], src


GRASS_PARTS = ('clutter_grass', 'clutter_tall')  # the near ring's grass, lit as the ground (A.3.40); not the crops' cards


def foliage(mesh, pick):
    """Each material slot gets an instance of M_SimFoliage with the textures the importer found for it (masked, two-sided,
    the instance's colour multiplier): the importer's own glTF materials draw a plant's leaves blended and grey. The near
    ring's grass (a part of GRASS_PARTS) gets M_SimGrass instead, lit as the ground it stands on (A.3.40). Each base
    texture's mips keep its alpha's coverage (keep_coverage)."""
    grass = str(pick.get('part', '')).startswith(GRASS_PARTS)
    name_of = 'M_SimGrass' if grass else 'M_SimFoliage'
    master = unreal.load_asset('/Game/SAM3D/Map/%s.%s' % (name_of, name_of))
    if master is None:
        raise RuntimeError('no %s: run tools/ue_build_tree_material.py first (SAM3D_GRASS_ONLY=1 for the grass alone)' % name_of)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat_dir = dest_of(pick) + '/SimMaterials'
    lib = unreal.MaterialEditingLibrary
    for i, slot in enumerate(mesh.static_materials):
        old = slot.material_interface
        textures = {}
        if old is not None:
            for v in old.get_editor_property('texture_parameter_values'):
                textures[str(v.parameter_info.name)] = v.parameter_value
        name = 'MI_%s_%s' % (pick['id'], str(slot.material_slot_name))
        mi = tools.create_asset(name, mat_dir, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', master)
        for param in ('BaseColorTexture', 'NormalTexture'):
            if textures.get(param) is not None:
                lib.set_material_instance_texture_parameter_value(mi, param, textures[param])
        keep_coverage(textures.get('BaseColorTexture'))
        mesh.set_material(i, mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log('%d slots on %s' % (len(mesh.static_materials), name_of))


def keep_coverage(texture, clip=0.4):
    """The texture's mips keep its alpha's coverage at the foliage materials' clip (0.4), so a clump or a card keeps its
    fill as it shrinks with the distance instead of thinning to stalks and going (A.3.40: Wild Grass covered 14% of its
    texture at 2048 px, 3.4% at 64 and 1.6% at 16 with plain mips)."""
    if texture is None:
        return
    try:
        texture.set_editor_property('do_scale_mips_for_alpha_coverage', True)
        texture.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0.0, 0.0, 0.0, clip))
    except Exception as e:  # an engine without the setting: the plain mips, as before
        log('alpha coverage not kept on %s: %s' % (texture.get_name(), e))
        return
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    log('alpha coverage kept on %s at %.2f' % (texture.get_name(), clip))


def make_lods(mesh, steps):
    """LOD 0 is the model; each further LOD is a share of its triangles from a screen size: [[share, screen_size], ...].
    The editor library's set_lods is tried; where it does not take (a commandlet), the mesh is put in the engine's Foliage
    LOD group, which builds its own chain when the mesh is built."""
    settings = [unreal.EditorScriptingMeshReductionSettings(percent_triangles=1.0, screen_size=1.0)]
    for share, screen in steps:
        settings.append(unreal.EditorScriptingMeshReductionSettings(percent_triangles=share, screen_size=screen))
    opts = unreal.EditorScriptingMeshReductionOptions(auto_compute_lod_screen_size=False, reduction_settings=settings)
    count = unreal.EditorStaticMeshLibrary.set_lods(mesh, opts)
    if count < len(settings):
        log('set_lods gave %s: using the Foliage LOD group' % count)
        mesh.set_editor_property('lod_group', 'Foliage')
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    n = unreal.EditorStaticMeshLibrary.get_lod_count(mesh)
    log('%d LODs: %s' % (n, [unreal.EditorStaticMeshLibrary.get_number_verts(mesh, l) for l in range(n)]))


def check_bounds(mesh_path, frame):
    """The mesh must be the frame's unit (100 cm) with its pivot where the frame wants it; refuses it otherwise."""
    b = unreal.load_asset(mesh_path).get_bounds()
    size = (2 * b.box_extent.x, 2 * b.box_extent.y, 2 * b.box_extent.z)
    zmin = b.origin.z - b.box_extent.z
    want_zmin = -50.0 if frame == 'cube' else 0.0
    log('bounds %.1f x %.1f x %.1f cm, centre (%.1f, %.1f), base at z %.1f (the frame wants 100 x 100 x 100, base %.0f)' % (
        size[0], size[1], size[2], b.origin.x, b.origin.y, zmin, want_zmin))
    if any(abs(x - 100.0) > 1.0 for x in size) or abs(b.origin.x) > 1.0 or abs(b.origin.y) > 1.0 or abs(zmin - want_zmin) > 1.0:
        raise RuntimeError('the imported mesh is not the frame\'s unit: check prepare_model.py and the glTF axes')


def register(pick, mesh_path, src):
    reg = load(REGISTER)
    with open(src, 'rb') as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    entry = {
        'id': pick['id'],
        'unreal_path': mesh_path,
        'origin': pick['origin'],
        'author': pick['author'],
        'licence': pick['licence'],
        'redistributable': pick.get('redistributable', pick['licence'] in ('CC0-1.0', 'CC-BY-4.0')),
        'fetched': pick['fetched'],
        'sha256': sha,
        'scale': 1.0,
        'yaw_deg': pick.get('yaw_deg', 0.0),
        # A pick's z_offset_m (A.3.27) lowers the model in the part's frame, where 1 is the tree's height: an oak's
        # exposed roots go into the ground.
        'z_offset_m': pick.get('z_offset_m', 0.0),
        'notes': pick.get('notes', ''),
    }
    reg['assets'] = [a for a in reg.get('assets', []) if a['id'] != pick['id']] + [entry]
    part = reg.setdefault('parts', {}).setdefault(pick['part'], {'asset': None})
    if 'variant' in pick:
        variants = part.setdefault('variants', [])
        while len(variants) <= pick['variant']:
            variants.append(None)
        variants[pick['variant']] = pick['id']
    else:
        part['asset'] = pick['id']
    with open(REGISTER, 'w', encoding='utf-8', newline='') as f:
        json.dump(reg, f, indent=2)
        f.write('\n')
    log('registered %s as %s' % (pick['id'], pick['part']))


def main():
    wanted = os.environ.get('SAM3D_MODEL', '')
    picks = [p for p in load(PICKS)['picks'] if p['id'] == wanted]
    if not picks:
        raise RuntimeError('SAM3D_MODEL=%r is not a pick in %s' % (wanted, PICKS))
    pick = picks[0]
    mesh_path, src = import_mesh(pick)
    check_bounds(mesh_path, pick['frame'])
    if os.environ.get('SAM3D_NO_REGISTER') != '1':
        register(pick, mesh_path, src)


main()
