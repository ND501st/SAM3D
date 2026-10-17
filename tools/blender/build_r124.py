"""Build the R-124 intercom's boxes (A-1, A-2, A-4): export SM_IntercomA1/A2/A4.fbx, update their entries in
art/cabin_interior/manifest.json.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_r124.py
    ... -- --knobs     # draw the switch knobs the cabin supplies (for a render of the boxes alone; nothing is exported)

The models are tools/blender/r124_model.py. The manifest also records each box's face_y: the plane, in the mesh's own
frame from its bounding-box centre, of the recess floor its switch stands on (the cabin's bare panel for the box).
"""
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_cabin_interior as bci  # noqa: E402
import r124_model  # noqa: E402


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.reset()
    os.makedirs(bci.OUT, exist_ok=True)
    path = os.path.join(bci.OUT, 'manifest.json')
    manifest = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}
    for name in r124_model.BOXES:
        obj = r124_model.build(name, bci.Builder, bci.finish, knobs='--knobs' in argv)
        size = [round(v, 4) for v in obj['body_size']]
        centre = [round(v, 4) for v in obj['body_centre']]
        print(name, 'size', size, 'centre', centre, 'face_y', round(obj['face_y'], 4))
        if '--knobs' in argv:
            continue
        bci.export(obj)
        manifest[name] = {'size_m': size}
    if '--knobs' not in argv:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(manifest, f, indent=1, sort_keys=True)
            f.write('\n')
        print('manifest updated')


main()
