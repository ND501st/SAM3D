"""Loads every texture under /Game/ThirdParty/Models and waits for Unreal to finish building them, so the derived data is
in the cache before a render: a texture still compiling draws white in the first frames of a shot.

    UnrealEditor-Cmd SAM3D.uproject -run=pythonscript -script=tools/ue_warm_textures.py -unattended -nullrhi
"""
import unreal

reg = unreal.AssetRegistryHelpers.get_asset_registry()
n = 0
keep = []
for data in reg.get_assets_by_path('/Game/ThirdParty/Models', recursive=True):
    if str(data.asset_class_path.asset_name) == 'Texture2D':
        keep.append(data.get_asset())
        n += 1
unreal.log('WARM loaded %d textures' % n)
for name in ('AssetCompilingManager',):
    mgr = getattr(unreal, name, None)
    unreal.log('WARM manager %s' % mgr)
try:
    unreal.AssetCompilingManager.get().finish_all_compilation()
    unreal.log('WARM finished via AssetCompilingManager')
except Exception as e:
    unreal.log('WARM compile wait failed: %s' % e)
    try:
        unreal.SystemLibrary.execute_console_command(None, 'Tex.WaitForAll')
    except Exception as e2:
        unreal.log('WARM console wait failed: %s' % e2)
