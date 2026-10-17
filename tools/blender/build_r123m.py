"""Build the R-123M alone: export SM_RadioR123M.fbx, update its entry in art/cabin_interior/manifest.json, and render
previews of the set.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_r123m.py
    ... -- --preview out/r123m/model        # also render front and three-quarter views (PNG)
    ... -- --knobs                           # draw every knob, for a render of the set on its own (the FBX is for the cabin)

The model is tools/blender/r123m_model.py, from the traced panel (r123m_trace.py). The whole compartment's build
(build_cabin_interior.py) builds the same mesh; this runs only the radio, so a change to it needs no rebuild of the hull.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_cabin_interior as bci  # noqa: E402
import r123m_model  # noqa: E402

REPO = bci.REPO


def look_at(cam, target):
    d = Vector(target) - cam.location
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


def preview(obj, prefix):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1500, 900
    world = bpy.data.worlds.new('w') if sc.world is None else sc.world
    sc.world = world
    world.color = (0.12, 0.125, 0.13)
    # the model's bounding box centre sits at obj.location; the front looks +y (Blender here is a turned copy of the body
    # frame: bci.P maps body -> Blender)
    centre = obj.location.copy()
    for name, (energy, loc) in {'key': (90, bci.P(-0.35, 0.55, 0.45)), 'fill': (30, bci.P(0.4, 0.5, 0.1))}.items():
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, 'AREA'))
        light.data.energy = energy
        light.data.size = 0.5
        light.location = loc
        bpy.data.collections[bci.COLLECTION].objects.link(light)
        look_at(light, centre)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    bpy.data.collections[bci.COLLECTION].objects.link(cam)
    sc.camera = cam
    cam.data.clip_start = 0.01
    out = []
    views = {'front': ((0.0, 0.62, 0.0), 50), 'three_quarter': ((0.34, 0.5, 0.18), 45), 'closeup': ((0.1, 0.28, 0.0), 40)}
    for name, (eye, lens) in views.items():
        cam.location = bci.P(*eye) + (centre - bci.P(0, 0, 0))
        cam.data.lens = lens
        look_at(cam, centre + (bci.P(0.0, 0.0, 0.0) - bci.P(0, 0, 0)))
        sc.render.filepath = '%s_%s.png' % (prefix, name)
        bpy.ops.render.render(write_still=True)
        out.append(sc.render.filepath)
    return out


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.reset()
    os.makedirs(bci.OUT, exist_ok=True)
    obj = r123m_model.build(bci.Builder, bci.finish, knobs='--knobs' in argv)
    if '--knobs' not in argv:
        bci.export(obj)
        path = os.path.join(bci.OUT, 'manifest.json')
        manifest = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}
        manifest[obj.name] = {'size_m': [round(v, 4) for v in obj['body_size']]}
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(manifest, f, indent=1, sort_keys=True)
            f.write('\n')
        print('exported', obj.name, manifest[obj.name])
    else:
        bci.export_path = None
    print('size', [round(v, 4) for v in obj['body_size']], 'centre', [round(v, 4) for v in obj['body_centre']])
    if '--preview' in argv:
        prefix = os.path.abspath(argv[argv.index('--preview') + 1])
        os.makedirs(os.path.dirname(prefix), exist_ok=True)
        for p in preview(obj, prefix):
            print('preview', p)


main()
