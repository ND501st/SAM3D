"""Import the cabin parts (art/cabin_parts/*.fbx, built by tools/blender/build_cabin_parts.py) into the open SAM3D editor.

Needs the editor open with remote execution on (tools/ue_remote.py). Imports or re-imports every SM_*.fbx as a static
mesh under /Game/SAM3D/Cabin/Parts and saves them. The cabin (SimCabinComponent) loads them by name and falls back to
engine basic shapes for any that are missing.

    python tools/ue_import_cabin_parts.py
    python tools/ue_import_cabin_parts.py SM_KnobBar      # only the parts named
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_remote import Editor  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = '/Game/SAM3D/Cabin/Parts'

IMPORT = r'''
import unreal
files = %(files)r
tasks = []
for f in files:
    t = unreal.AssetImportTask()
    t.filename = f
    t.destination_path = %(dest)r
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    print(t.filename.split('\\')[-1], '->', list(t.imported_object_paths))
'''


def main():
    files = sorted(glob.glob(os.path.join(REPO, 'art', 'cabin_parts', 'SM_*.fbx')))
    if sys.argv[1:]:
        files = [f for f in files if os.path.splitext(os.path.basename(f))[0] in sys.argv[1:]]
    if not files:
        sys.exit('no parts: run tools/blender/build_cabin_parts.py first')
    with Editor() as ue:
        ok, out = ue.run(IMPORT % {'files': files, 'dest': DEST})
        print(out)
        if not ok:
            sys.exit(1)


if __name__ == '__main__':
    main()
