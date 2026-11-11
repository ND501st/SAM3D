"""Every mesh the cabin file places is built: listed in art/cabin_interior/manifest.json with its FBX beside it, so the
interior the game assembles (and tools/blender/assemble_cabin.py's one-model copy) has no part missing."""
import json
import os

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ART = os.path.join(REPO, 'art', 'cabin_interior')


def _placed():
    cab = json.load(open(os.path.join(REPO, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8'))
    return {s['mesh'] for s in cab['shell'] if s.get('kind') == 'mesh'}


def test_every_placed_mesh_is_built():
    manifest = json.load(open(os.path.join(ART, 'manifest.json'), encoding='utf-8'))
    placed = _placed()
    assert placed, 'the cabin file places no meshes'
    assert not sorted(placed - set(manifest)), 'placed but not in the manifest (run python tools/build_osa_interior.py)'
    assert not sorted(m for m in placed if not os.path.exists(os.path.join(ART, m + '.fbx'))), 'placed but no FBX'


def test_every_built_mesh_is_placed():
    manifest = json.load(open(os.path.join(ART, 'manifest.json'), encoding='utf-8'))
    assert not sorted(set(manifest) - _placed()), 'built but never placed by data/cabins/osa_ak.json'
