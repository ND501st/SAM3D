"""Build the ПАНЕЛЬ ИНДИКАЦИИ alone: export art/cabin_interior/SM_PanelInd.fbx and update its entry in manifest.json, or render
the checks.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_panel_ind.py
    ... -- --check                     # front ortho, close-ups and g095-b's camera to out/panel_ind/check/, no export

The model is tools/blender/panel_ind_model.py, from the trace (panel_ind_trace.py).
"""
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_cabin_interior as bci  # noqa: E402
import panel_ind_model as model  # noqa: E402
import panel_ind_trace as T  # noqa: E402
import trace_check as C  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
CHECK = os.path.join(REPO, 'out', 'panel_ind', 'check')
CFG = {'width_mm': T.W_MM, 'height_mm': T.H_MM, 'px_per_mm': 5.0, 'margin_mm': 40.0}
FRONT = {'x': 0.0, 'y': 0.0, 'z': 0.0}
PREVIEW = {'Paint_Panel': (0.53, 0.52, 0.47), 'Paint_LightGrey': (0.36, 0.37, 0.38), 'Paint_Letters': (0.5, 0.48, 0.42),
           'Vinyl_Olive': (0.052, 0.054, 0.03), 'Bakelite_Black': (0.012, 0.012, 0.012), 'Metal_Dark': (0.03, 0.03, 0.03),
           'Paint_Red': (0.3, 0.035, 0.012), 'Metal_Steel': (0.42, 0.42, 0.43), 'Metal_Brass': (0.42, 0.3, 0.12)}


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.reset()
    bci.PREVIEW.update({k: tuple(v * 0.62 for v in c) for k, c in PREVIEW.items()})
    os.makedirs(bci.OUT, exist_ok=True)
    obj = model.build(bci.Builder, bci.finish)
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
    os.makedirs(CHECK, exist_ok=True)
    prefix = os.path.join(CHECK, 'panel_ind')
    print('front', C.render_front(FRONT, CFG, prefix + '_front.png'))
    print('view', C.render_views((0.0, 0.0, 0.0), prefix, {'whole': (0.9, -0.35, 0.15)}, lens=35))
    print('close', C.render_views((0.0, 0.0, 0.1), prefix + '_top', {'caps': (0.3, -0.12, 0.05)}, lens=40))
    print('close', C.render_views((0.0, 0.0, -0.22), prefix + '_foot', {'sep': (0.3, -0.12, 0.0), 'plugs': (0.3, 0.2, -0.12)}, lens=40))
    cam = os.path.join(REPO, 'out', 'traces', 'panel_ind_g095b', 'camera.json')
    print('g095b', C.render_photo_camera(FRONT, CFG, cam, prefix + '_cam_g095b.png', width=1524))


main()
