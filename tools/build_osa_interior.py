"""Rebuild every mesh of the Osa-AK's fighting compartment in the order they depend on, check that each mesh the cabin
file places was exported, then assemble the whole interior as one model (tools/blender/assemble_cabin.py).

    python tools/build_osa_interior.py [--assemble-only] [--no-render] [OUT_DIR]

A full build_cabin_interior.py run rewrites art/cabin_interior/manifest.json with only its own meshes, so the
builders of the photographed parts (SŁ-1M, SŁ-3M, SP-1M, SP-2M2, SP-3M-2 faces, cabinet 0096, R-123M, R-124 boxes)
run after it and put theirs back. Unreal's import is still the owner's step: tools/ue_import_cabin_interior.py with
the editor open.
"""
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLENDER = os.environ.get('BLENDER', r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe')
BUILDERS = ['build_cabin_interior', 'build_cabinet0096', 'build_r123m', 'build_r124', 'build_sl1m', 'build_sl3m',
            'build_sp1m', 'build_sp2m', 'build_sp3m']


def blender(script, *args):
    cmd = [BLENDER, '--background', '--factory-startup', '--python-exit-code', '3', '--python',os.path.join(REPO, 'tools', 'blender', script + '.py')]
    if args:
        cmd += ['--'] + list(args)
    r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, encoding='utf-8', errors='replace')
    tail = [ln for ln in r.stdout.splitlines() if ln.startswith(('exported', 'assemble_cabin', 'Error', 'Traceback'))]
    print('%-22s exit %d  %s' % (script, r.returncode, '; '.join(tail[-3:])))
    if r.returncode != 0:
        print(r.stderr[-1500:])
    return r.returncode


def unplaced():
    """Meshes the cabin file places that the manifest does not list (not exported)."""
    cab = json.load(open(os.path.join(REPO, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8'))
    man = json.load(open(os.path.join(REPO, 'art', 'cabin_interior', 'manifest.json'), encoding='utf-8'))
    return sorted({s['mesh'] for s in cab['shell'] if s.get('kind') == 'mesh'} - set(man))


def main(argv):
    flags = [a for a in argv if a.startswith('--')]
    rest = [a for a in argv if not a.startswith('--')]
    bad = 0
    if '--assemble-only' not in flags:
        for b in BUILDERS:
            bad |= blender(b) != 0
    missing = unplaced()
    print('meshes placed but not built:', missing or 'none')
    args = rest + (['--no-render'] if '--no-render' in flags else [])
    bad |= blender('assemble_cabin', *args) != 0
    return 1 if bad or missing else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
