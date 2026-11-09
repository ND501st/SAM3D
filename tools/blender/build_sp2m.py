"""Build SP-2M2's face alone: export SM_SP2M2Face.fbx, update its entry in art/cabin_interior/manifest.json, and render
the checks of docs/MODELLING_FROM_PHOTOS.md step 7.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_sp2m.py
    ... -- --check out/sp2m/check [--camera out/sp2m/g092a_cam_tc.json]   # also the front ortho, the photo camera's view
                                                                           # and close-ups (PNG), no export

The model is tools/blender/sp2m_model.py, from the trace (sp2m_trace.py). The front render is framed as the flattened
photo (2 px/mm, 20 mm margin) for tools/photo_trace.py overlay; the camera file is g092-a's solved camera in the face's
frame (metres: R, t, w, h, f_px).
"""
import json
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_cabin_interior as bci  # noqa: E402
import sp2m_model  # noqa: E402
import sp2m_trace as T  # noqa: E402
import trace_check as C  # noqa: E402

CFG = {'width_mm': T.W_MM, 'height_mm': T.H_MM, 'px_per_mm': 2.0, 'margin_mm': 20.0}
FRONT = {'x': 0.0, 'y': 0.0, 'z': 0.0}     # the face's plate plane and centre in Blender (the model frame's origin)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.reset()
    os.makedirs(bci.OUT, exist_ok=True)
    obj = sp2m_model.build(bci.Builder, bci.finish)
    print('size', [round(v, 4) for v in obj['body_size']], 'centre', [round(v, 4) for v in obj['body_centre']],
          'triangles', sum(len(p.vertices) - 2 for p in obj.data.polygons))
    if '--check' not in argv:
        bci.export(obj)
        path = os.path.join(bci.OUT, 'manifest.json')
        manifest = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}
        manifest[obj.name] = {'size_m': [round(v, 4) for v in obj['body_size']]}
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(manifest, f, indent=1, sort_keys=True)
            f.write('\n')
        print('exported', obj.name, manifest[obj.name])
        return
    prefix = os.path.abspath(argv[argv.index('--check') + 1])
    os.makedirs(os.path.dirname(prefix), exist_ok=True)
    print('front', C.render_front(FRONT, CFG, prefix + '_front.png'))
    if '--camera' in argv:
        cam = os.path.abspath(argv[argv.index('--camera') + 1])
        print('photo', C.render_photo_camera(FRONT, CFG, cam, prefix + '_photo.png', width=2000))
    # close-ups, three-quarter from the aisle (Blender: the face looks +X, its right is +Y)
    for name, (x, y) in {'or31': (893, 44), 'or11': (950, 300), 'tables': (300, 600), 'right': (1600, 850),
                         'r0': (1550, 120), 'l4': (300, 1080)}.items():
        a, b = T.to_m(x, y)
        print('view', C.render_views((0.0, a, b), prefix, {name: (0.42, 0.22, 0.16)}))
    print('view', C.render_views((0.0, 0.0, 0.0), prefix, {'whole': (2.2, 0.9, 0.3)}, lens=35))


main()
