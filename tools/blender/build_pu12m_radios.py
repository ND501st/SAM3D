"""Build the PU-12M's R-111 and R-407 from their traced fronts: export SM_RadioR111.fbx and SM_RadioR407.fbx into
art/pu12m_interior/ (with their entries in its manifest: the mesh's size, and where the front plate stands from the mesh's
centre), and render previews.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_pu12m_radios.py
    ... -- --preview out/pu12m_radios/model     # also render front and three-quarter views of each (PNG)
    ... -- --knobs                              # draw every knob (a render of the sets alone; the FBX is for the cabin)

The models: tools/blender/radio_front_model.py, from tools/blender/r111_trace.py and r407_trace.py. The PU-12M's cabin
layout (tools/pu12m_cabin.py) places them where the interior's trace (pu12m_interior_trace.py) has the sets, with bare
panels on their fronts; the interior's own mesh leaves them out.
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
import r111_trace  # noqa: E402
import r407_trace  # noqa: E402
import radio_front_model  # noqa: E402

REPO = bci.REPO
OUT = os.path.join(REPO, 'art', 'pu12m_interior')


def look_at(cam, target):
    d = Vector(target) - cam.location
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


def preview(obj, prefix, scale):
    sc = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items]
    sc.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in engines else 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1500, 1000
    world = bpy.data.worlds.new('w') if sc.world is None else sc.world
    sc.world = world
    world.color = (0.12, 0.125, 0.13)
    centre = obj.location.copy()
    lights = []
    for name, (energy, loc) in {'key': (120, bci.P(-0.35, 0.9, 0.5)), 'fill': (45, bci.P(0.45, 0.8, 0.1))}.items():
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, 'AREA'))
        light.data.energy = energy
        light.data.size = 0.6
        light.location = centre + (loc - bci.P(0, 0, 0))
        bpy.data.collections[bci.COLLECTION].objects.link(light)
        look_at(light, centre)
        lights.append(light)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    bpy.data.collections[bci.COLLECTION].objects.link(cam)
    sc.camera = cam
    cam.data.clip_start = 0.01
    out = []
    views = {'front': ((0.0, 1.0 * scale, 0.0), 50), 'three_quarter': ((0.5 * scale, 0.8 * scale, 0.3 * scale), 45),
             'closeup': ((0.12 * scale, 0.45 * scale, 0.05), 40)}
    for name, (eye, lens) in views.items():
        cam.location = centre + (bci.P(*eye) - bci.P(0, 0, 0))
        cam.data.lens = lens
        look_at(cam, centre)
        sc.render.filepath = '%s_%s.png' % (prefix, name)
        bpy.ops.render.render(write_still=True)
        out.append(sc.render.filepath)
    for o in lights + [cam]:
        bpy.data.objects.remove(o, do_unlink=True)
    return out


def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    loc = obj.location.copy()
    obj.location = (0, 0, 0)
    path = os.path.join(OUT, obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                             axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True, mesh_smooth_type='FACE',
                             add_leaf_bones=False, bake_anim=False, use_tspace=True)
    obj.location = loc
    return path


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    bci.COLLECTION = 'PU12MRadios'
    bci.reset()
    os.makedirs(OUT, exist_ok=True)
    knobs = '--knobs' in argv
    path = os.path.join(OUT, 'manifest.json')
    manifest = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}
    built = []
    for T in (r111_trace, r407_trace):
        obj = radio_front_model.build(bci.Builder, bci.finish, T, knobs=knobs)
        built.append((obj, T))
        # the plate's centre is the model's origin (body frame: the front looks +y): from the mesh's centre it stands
        # `ahead` along the facing, `up` and `right` (the operator's right is body -x)
        c = obj['body_centre']
        entry = {'size_m': [round(v, 4) for v in obj['body_size']], 'centre_m': [round(v, 4) for v in c], 'reuse': True,
                 'plate_m': {'right': round(c[0], 4), 'ahead': round(-c[1], 4), 'up': round(-c[2], 4),
                             'width': round(T.PLATE_W_M, 4), 'height': round(T.PLATE_H_M, 4)}}
        if not knobs:
            export(obj)
            manifest[obj.name] = entry
        print('built', obj.name, entry)
    if not knobs:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(manifest, f, indent=1, sort_keys=True)
            f.write('\n')
    if '--preview' in argv:
        prefix = os.path.abspath(argv[argv.index('--preview') + 1])
        os.makedirs(os.path.dirname(prefix), exist_ok=True)
        for obj, T in built:
            for o, _ in built:
                o.hide_render = o is not obj
            for p in preview(obj, prefix + '_' + T.NAME.lower(), max(T.PLATE_W_M, T.PLATE_H_M) / 0.45):
                print('preview', p)


main()
