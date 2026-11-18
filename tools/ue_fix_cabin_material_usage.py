"""Give the cabin's opaque master materials the Nanite usage flag and resave them and their instances, in place.

The cabin's meshes are Nanite; without the flag the game draws Unreal's default material on them ("missing usage flag
Nanite! Default Material will be used in game") and every instance recompiles at each launch until resaved. Unlike
tools/ue_import_cabin_interior.py and ue_make_cabin_materials.py (which now set the flag when they build the materials)
this keeps the existing assets, so the MI_Cabin_* instances stay parented to them. The translucent glass is left alone.

Headless, with the editor closed (from PowerShell):
    & 'D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe' C:\\dev\\SAM3D\\unreal\\SAM3D\\SAM3D.uproject `
        -run=pythonscript -script=C:\\dev\\SAM3D\\tools\\ue_fix_cabin_material_usage.py -unattended -nosplash -nullrhi
"""
import unreal

FOLDER = '/Game/SAM3D/Cabin/Materials'
MASTERS = ('M_CabinPBR', 'M_CabinSurface')
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

for name in MASTERS:
    m = lib.load_asset(FOLDER + '/' + name)
    if m is None:
        unreal.log_warning('SAM3D-FIX: no ' + name)
        continue
    m.set_editor_property('used_with_nanite', True)
    mel.recompile_material(m)
    unreal.log('SAM3D-FIX saved %s: %s' % (name, lib.save_loaded_asset(m, False)))

for path in lib.list_assets(FOLDER, recursive=False, include_folder=False):
    asset = lib.load_asset(path)
    if isinstance(asset, unreal.MaterialInstanceConstant):
        mel.update_material_instance(asset)
        unreal.log('SAM3D-FIX saved %s: %s' % (path, lib.save_loaded_asset(asset, False)))
