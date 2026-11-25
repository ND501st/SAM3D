"""Give the map's master materials the usage flags their meshes need, and resave them and their instances in place
(roadmap A.3.11).

The registered plants (the clutter's cards, the trees) are imported as Nanite meshes drawn by instanced static mesh
components, and the map's procedural parts are drawn by instances too; without the flags the game logs "missing usage
flag Nanite (or InstancedStaticMeshes)! Default Material will be used in game" and a packaged build would draw them in
the default material. The flags belong to the master; each instance is updated and resaved so it stops recompiling at
every launch. The masters are tools/ue_build_tree_material.py's; this keeps the existing assets (and their instances'
parents).

Headless, with the editor closed (from PowerShell):
    & 'D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe' C:\\dev\\SAM3D-wt\\terrain\\unreal\\SAM3D\\SAM3D.uproject `
        -run=pythonscript -script=C:\\dev\\SAM3D-wt\\terrain\\tools\\ue_fix_map_material_usage.py -unattended -nosplash -nullrhi
"""
import unreal

MAP = '/Game/SAM3D/Map'
# master: the flags it needs (each is a UMaterial editor property)
MASTERS = {
    'M_SimFoliage': ('used_with_nanite', 'used_with_instanced_static_meshes'),
    'M_SimMapVertexColor': ('used_with_nanite', 'used_with_instanced_static_meshes'),
    'M_SimMapClutter': ('used_with_nanite', 'used_with_instanced_static_meshes'),
    'M_SimMapTree': ('used_with_nanite', 'used_with_instanced_static_meshes'),  # the procedural trees' season tint
}
# Where the masters' instances live: the cards (committed) and the fetched models (ignored by git).
INSTANCE_ROOTS = ('/Game/SAM3D/Map/Cards', '/Game/ThirdParty/Models')
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log('SAM3D-USAGE: ' + msg)


changed = 0
for name, flags in MASTERS.items():
    m = lib.load_asset(MAP + '/' + name)
    if m is None:
        log('no %s' % name)
        continue
    missing = [f for f in flags if not m.get_editor_property(f)]
    if not missing:
        log('%s has its flags' % name)
        continue
    for f in missing:
        m.set_editor_property(f, True)
    mel.recompile_material(m)
    log('%s: set %s, saved %s' % (name, ', '.join(missing), lib.save_loaded_asset(m, False)))
    changed += 1

instances = 0
for root in INSTANCE_ROOTS:
    if not lib.does_directory_exist(root):
        continue
    for path in lib.list_assets(root, recursive=True, include_folder=False):
        data = unreal.EditorAssetLibrary.find_asset_data(path)
        if str(data.asset_class_path.asset_name) != 'MaterialInstanceConstant':
            continue
        asset = lib.load_asset(path)
        mel.update_material_instance(asset)
        lib.save_loaded_asset(asset, False)
        instances += 1
log('%d masters changed, %d instances resaved' % (changed, instances))
