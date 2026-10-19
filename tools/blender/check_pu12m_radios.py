"""Prove the PU-12M's R-111 and R-407 models on their photographs (docs/MODELLING_FROM_PHOTOS.md step 6): render each square
on (framed as photo_trace's front_mm.png) and from its photo's solved camera (out/traces/<name>/camera.json), and close-ups
from three-quarter angles; then run photo_trace.py overlay on the two.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/check_pu12m_radios.py
    python tools/photo_trace.py overlay tools/traces/r111_front.json out/traces/r111_front/render_front.png
    python tools/photo_trace.py overlay tools/traces/r111_front.json out/traces/r111_front/render_photo.png --photo PHOTO

The models are built with every knob (the cabin's controls drawn too), so the renders show the whole set. The plate's
centre is at the Blender origin, the front facing +X: the trace frame's centre is `front` from it.
"""
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_cabin_interior as bci  # noqa: E402
import r111_trace  # noqa: E402
import r407_trace  # noqa: E402
import radio_front_model  # noqa: E402
import trace_check as C  # noqa: E402

REPO = bci.REPO


def main():
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.COLLECTION = 'PU12MRadiosCheck'
    bci.reset()
    for T, name in ((r111_trace, 'r111_front'), (r407_trace, 'r407_front')):
        for o in list(bpy.data.collections[bci.COLLECTION].objects):
            bpy.data.objects.remove(o, do_unlink=True)
        cfg = json.load(open(os.path.join(REPO, 'tools', 'traces', name + '.json'), encoding='utf-8'))
        out = os.path.join(REPO, 'out', 'traces', name)
        radio_front_model.build(bci.Builder, bci.finish, T, knobs=True)
        # the trace frame's centre from the plate's centre (Blender Y is the trace's right, Z up)
        front = {'x': 0.0, 'y': (cfg['width_mm'] / 2 - T.CX) / 1000.0, 'z': (T.CY - cfg['height_mm'] / 2) / 1000.0}
        C.render_front(front, cfg, os.path.join(out, 'render_front.png'))
        cam = os.path.join(out, 'camera.json')
        if os.path.exists(cam):
            C.render_photo_camera(front, cfg, cam, os.path.join(out, 'render_photo.png'))
        w = T.PLATE_W_M
        C.render_views((0.0, 0.0, 0.0), os.path.join(out, 'close'),
                       {'left': (0.45 * w / 0.4, 0.35 * w / 0.4, 0.15), 'right': (0.45 * w / 0.4, -0.35 * w / 0.4, 0.1),
                        'below': (0.5 * w / 0.4, 0.05, -0.25)})
        print('rendered', name)


main()
