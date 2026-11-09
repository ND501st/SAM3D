"""Build the distribution cabinet 0096-9M2 alone: export art/cabin_interior/SM_Cabinet0096.fbx at the game's size and update its entry in manifest.json (a full build_cabin_interior.py run drops that entry: run this after it), or render the checks.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_cabinet0096.py
    ... -- --check                     # front ortho, close-ups and the two photos' cameras to out/cabinet0096/check/, no export

The model is tools/blender/cabinet0096_model.py, from the trace (cabinet0096_trace.py).
"""
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_cabin_interior as bci  # noqa: E402
import cabinet0096_model as model  # noqa: E402
import cabinet0096_trace as T  # noqa: E402
import trace_check as C  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
CHECK = os.path.join(REPO, 'out', 'cabinet0096', 'check')
CFG = {'width_mm': T.W_MM, 'height_mm': T.H_MM, 'px_per_mm': 2.0, 'margin_mm': 40.0}
FRONT_Z = T.LOCAL_TOP_Z - T.H_MM / 2000.0
FRONT = {'x': T.LOCAL_DOOR_Y, 'y': 0.0, 'z': FRONT_Z}               # the ortho render: door's plane, the face's centre (Blender X = local y, Y = -local x)
PLATE = {'x': T.LOCAL_DOOR_Y - T.DOOR_PROUD / 1000.0, 'y': 0.0, 'z': FRONT_Z}   # the cameras' plate: the frame's plane
RIG = 0.62
CAMERAS = {'g096a': 'cabinet0096_g096a', 'g097b': 'cabinet0096_g097b'}
# the check renders in the slots' Unreal colours (tools/ue_import_cabin_interior.py); trace_check's rig is about 1.6 times too bright for them, so these are scaled below it
PREVIEW = {'Paint_Light': (0.62, 0.57, 0.43), 'Paint_Panel': (0.53, 0.52, 0.47), 'Paint_LightGrey': (0.36, 0.37, 0.38), 'Paint_BlueGrey': (0.16, 0.2, 0.24), 'Paint_Letters': (0.5, 0.48, 0.42),
           'Duct_Grey': (0.19, 0.19, 0.18), 'Vinyl_Olive': (0.052, 0.054, 0.03), 'Bakelite_Black': (0.012, 0.012, 0.012), 'Metal_Dark': (0.03, 0.03, 0.03),
           'Paint_Red': (0.3, 0.035, 0.012), 'Lens_Amber': (0.45, 0.17, 0.02), 'Lens_Green': (0.03, 0.16, 0.05), 'Metal_Steel': (0.42, 0.42, 0.43), 'Metal_Brass': (0.42, 0.3, 0.12)}


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.reset()
    bci.PREVIEW.update({k: tuple(v * RIG for v in c) for k, c in PREVIEW.items()})
    os.makedirs(bci.OUT, exist_ok=True)
    obj = model.build(bci.Builder, bci.finish, scale=1.0 if '--check' in argv else T.SCALE)
    print(obj.name, 'size', [round(v, 4) for v in obj['body_size']], 'bbox centre', [round(v, 4) for v in obj['bbox_centre']],
          'origin', [round(v, 4) for v in obj['body_centre']], 'triangles', sum(len(p.vertices) - 2 for p in obj.data.polygons))
    if '--check' not in argv:
        print('exported', bci.export(obj))
        path = os.path.join(bci.OUT, 'manifest.json')
        manifest = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}
        manifest[obj.name] = {'size_m': [round(v, 4) for v in obj['body_size']]}
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(manifest, f, indent=1, sort_keys=True)
        return
    os.makedirs(CHECK, exist_ok=True)
    prefix = os.path.join(CHECK, 'cabinet')
    print('front', C.render_front(FRONT, CFG, prefix + '_front.png'))
    target = (T.LOCAL_DOOR_Y, 0.0, FRONT_Z)
    views = {
        'whole': (1.35, -0.7, 0.45),
        'upper': (0.6, -0.1, 0.35),
        'fuses': (0.5, -0.1, -0.05),
    }
    print('view', C.render_views(target, prefix, views, lens=40))
    print('close', C.render_views((T.LOCAL_DOOR_Y, 0.12, T.LOCAL_TOP_Z - 0.17), prefix + '_top', {'meters': (0.42, -0.08, 0.08), 'lid': (0.42, -0.25, 0.42)}, lens=40))
    print('plugs', C.render_views((0.02, 0.0, -0.33), prefix + '_foot', {'plugs': (0.75, -0.2, 0.15)}, lens=40))
    for name, folder in CAMERAS.items():
        cam = os.path.join(REPO, 'out', 'traces', folder, 'camera.json')
        print(name, C.render_photo_camera(PLATE, CFG, cam, prefix + '_cam_%s.png' % name, width=1500))


main()
