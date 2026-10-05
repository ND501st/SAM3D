"""Bring the cabin layout's modelled parts in line with the meshes Blender last built.

After tools/blender/build_cabin_interior.py, each shell part of kind "mesh" in data/cabins/<system>.json takes its
mesh's bounding box (size_m) from art/cabin_interior/manifest.json, and a mesh built in place (the hull, the racks...)
also its centre (centre_m). A reusable mesh keeps the centre the layout gives it, except that a part standing on the
floor (its bottom within 2 cm of the floor) stays standing on it when its height changes. Prints every change;
--check only reports (exit 1 if anything is out of step), as tools/tests/test_cabin_interior.py does.

    python tools/cabin_mesh_sync.py
    python tools/cabin_mesh_sync.py --check
"""
import argparse
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLOOR_Z = 0.62


def fmt(v):
    return '[' + ', '.join(repr(round(x, 4) + 0.0) for x in v) + ']'   # + 0.0: no -0.0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('system', nargs='?', default='osa_ak')
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    path = os.path.join(REPO, 'data', 'cabins', args.system + '.json')
    text = open(path, encoding='utf-8').read()
    layout = json.loads(text)
    with open(os.path.join(REPO, 'art', 'cabin_interior', 'manifest.json'), encoding='utf-8') as f:
        manifest = json.load(f)
    changes = []
    for part in layout['shell']:
        if part['kind'] != 'mesh':
            continue
        built = manifest.get(part['mesh'])
        if built is None:
            changes.append('%s: mesh %s was not built' % (part['id'], part['mesh']))
            continue
        size, centre = list(part['size_m']), list(part['centre_m'])
        new_size = [round(v, 4) for v in built['size_m']]
        if 'centre_m' in built:
            new_centre = [round(v, 4) for v in built['centre_m']]
        else:
            new_centre = list(centre)
            if abs(centre[2] - size[2] / 2 - FLOOR_Z) < 0.02:
                new_centre[2] = round(FLOOR_Z + new_size[2] / 2, 4)
        if any(abs(a - b) > 1e-4 for a, b in zip(size + centre, new_size + new_centre)):
            changes.append('%s: size %s -> %s, centre %s -> %s' % (part['id'], size, new_size, centre, new_centre))
            # rewrite this part's numbers in place, keeping the file's layout
            block = re.search(r'\{"id": "%s",.*?\}' % re.escape(part['id']), text, re.S)
            seg = block.group(0)
            seg2 = re.sub(r'"size_m": \[[^\]]*\]', '"size_m": ' + fmt(new_size), seg, count=1)
            seg2 = re.sub(r'"centre_m": \[[^\]]*\]', '"centre_m": ' + fmt(new_centre), seg2, count=1)
            text = text.replace(seg, seg2)
    for line in changes:
        print(line)
    if args.check:
        return 1 if changes else 0
    if changes:
        json.loads(text)
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        print('updated', path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
