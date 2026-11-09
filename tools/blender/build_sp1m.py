"""Build SP-1M's face alone: export SM_SP1MFace.fbx, update its entry in art/cabin_interior/manifest.json, and render a front view.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_sp1m.py
    ... -- --check out/sp1m/check      # front ortho (PNG), no export

The model is tools/blender/sp1m_model.py, from the trace (sp1m_trace.py).
"""
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_cabin_interior as bci  # noqa: E402
import sp1m_model  # noqa: E402
import sp1m_trace as T  # noqa: E402
import trace_check as C  # noqa: E402

CFG = {'width_mm': T.W_MM, 'height_mm': T.H_MM, 'px_per_mm': 2.0, 'margin_mm': 90.0}
FRONT = {'x': 0.0, 'y': 0.0, 'z': 0.0}


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.reset()
    os.makedirs(bci.OUT, exist_ok=True)
    obj = sp1m_model.build(bci.Builder, bci.finish)
    print(obj.name, 'size', [round(v, 4) for v in obj['body_size']], 'centre', [round(v, 4) for v in obj['body_centre']],
          'triangles', sum(len(p.vertices) - 2 for p in obj.data.polygons))
    if '--check' not in argv:
        path = os.path.join(bci.OUT, 'manifest.json')
        manifest = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}
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


main()
