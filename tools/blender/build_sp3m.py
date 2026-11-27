"""Build SP-3M-2's face and flap alone: export SM_SP3MFace.fbx and SM_SP3MFlap.fbx, update their entries in
art/cabin_interior/manifest.json, and render close-ups.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_sp3m.py
    ... -- --check out/sp3m/check      # front ortho and close-ups (PNG), no export

The model is tools/blender/sp3m_model.py, from the trace (sp3m_trace.py).
"""
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_cabin_interior as bci  # noqa: E402
import sp3m_model  # noqa: E402
import sp3m_trace as T  # noqa: E402
import trace_check as C  # noqa: E402

CFG = {'width_mm': T.W_MM, 'height_mm': T.H_MM, 'px_per_mm': 2.0, 'margin_mm': 90.0}
FRONT = {'x': 0.0, 'y': 0.0, 'z': 0.0}


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.reset()
    os.makedirs(bci.OUT, exist_ok=True)
    objs = [sp3m_model.build(bci.Builder, bci.finish), sp3m_model.build_flap(bci.Builder, bci.finish)]
    for obj in objs:
        print(obj.name, 'size', [round(v, 4) for v in obj['body_size']], 'centre', [round(v, 4) for v in obj['body_centre']],
              'triangles', sum(len(p.vertices) - 2 for p in obj.data.polygons))
    if '--check' not in argv:
        path = os.path.join(bci.OUT, 'manifest.json')
        manifest = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}
        for obj in objs:
            bci.export(obj)
            manifest[obj.name] = {'size_m': [round(v, 4) for v in obj['body_size']]}
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(manifest, f, indent=1, sort_keys=True)
            f.write('\n')
        print('exported')
        return
    prefix = os.path.abspath(argv[argv.index('--check') + 1])
    os.makedirs(os.path.dirname(prefix), exist_ok=True)
    print('front', C.render_front(FRONT, CFG, prefix + '_front.png'))
    print('view', C.render_views((0.0, 0.0, 0.0), prefix, {'whole': (1.6, 0.9, 0.2)}, lens=35))


main()
