"""Make each registered tree's thinner variants the LODs of its model (roadmap A.1 item 6, L6).

data/scenery/assets.json names, for a tree part (tree_oak, tree_hornbeam, ...), its variants in order: the model and the
same model thinned to smaller budgets (tools/prepare_model.py). The map draws a kind's model alone (SimMapActor.cpp): an
instanced component's start distance only fades, it does not hide nearer instances, so a ladder of separate meshes drew
every rung from 0 m. Here variant i becomes LOD i of variant 0: copied from the imported variant's mesh where the editor
allows it, else imported from the variant's normalised file (prepare_model.py's <folder>/normalized/<id>.glb, the picks
in data/scenery/model_picks.json). The LODs switch at the engine's own screen sizes. The meshes are fetched art under
/Game/ThirdParty/Models (ignored by git); run again after reimporting a model.

In the full editor, which it quits when done: a commandlet has no static mesh editor subsystem, and the old library's
calls all fail there. With the editor closed beforehand (from PowerShell):
    & 'D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor.exe' C:\\dev\\SAM3D-wt\\terrain\\unreal\\SAM3D\\SAM3D.uproject `
        -ExecutePythonScript=C:\\dev\\SAM3D-wt\\terrain\\tools\\ue_tree_lods.py -unattended -nosplash
"""
import json
import os

import unreal

ROOT = 'C:/dev/SAM3D-wt/terrain'
REGISTER = ROOT + '/data/scenery/assets.json'
PICKS = ROOT + '/data/scenery/model_picks.json'
SOURCES = ROOT + '/art/thirdparty/models'


def log(msg):
    unreal.log('SAM3D-TREE-LODS: ' + msg)


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def normalised_file(pick):
    base = ROOT + '/' + pick['root'] if pick.get('root') else SOURCES + '/' + pick['folder']
    return base + '/normalized/' + pick['id'] + '.glb'


def main():
    reg = load(REGISTER)
    picks = {p['id']: p for p in load(PICKS)['picks']}
    paths = {a['id']: a['unreal_path'] for a in reg.get('assets', [])}
    lib = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)  # not in a commandlet: see the docstring
    done = 0
    for part, entry in sorted(reg.get('parts', {}).items()):
        variants = [v for v in entry.get('variants') or [] if v]
        if not part.startswith('tree_') or len(variants) < 2:
            continue
        base = unreal.load_asset(paths[variants[0]])
        if base is None:
            log('%s: no %s in this project' % (part, variants[0]))
            continue
        for i, vid in enumerate(variants[1:], start=1):
            got = -1
            src = unreal.load_asset(paths[vid]) if vid in paths else None
            if src is not None:
                got = lib.set_lod_from_static_mesh(base, i, src, 0, True)
            how = 'copied'
            if got < 0 and vid in picks and os.path.exists(normalised_file(picks[vid])):
                got = lib.import_lod(base, i, normalised_file(picks[vid]))
                how = 'imported'
            log('%s: LOD %d from %s, %s (%s)' % (part, i, vid, how, got))
            if got < 0:
                break
        unreal.EditorAssetLibrary.save_loaded_asset(base)
        n = lib.get_lod_count(base)
        log('%s: %s has %d LODs, %s vertices' % (part, variants[0], n, [lib.get_number_verts(base, l) for l in range(n)]))
        done += 1 if n == len(variants) else 0
    log('%d tree models given all their LODs' % done)


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
