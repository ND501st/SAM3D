"""The holistic register as a 3-D blockout (docs/OSA_INTERIOR_HOLISTIC.md): every component with a place, a box coloured
by its class group, solid where measured, see-through where only Fig. 3 or the photos' relations place it; the
compartment's shell from docs/INTERIOR_HANDOFF.md section 3. Rendered from inside like the book's wide photos, for
reference only (nothing is exported).

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_holistic_blockout.py
    ... -- --out out/recon/holistic/blockout
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REGISTER = os.path.join(REPO, 'data', 'cabins', 'osa_ak_components.json')
GROUP_RGB = {'structure': (0.54, 0.56, 0.6), 'equipment': (0.23, 0.49, 0.85), 'crew': (0.79, 0.51, 0.17),
             'fittings': (0.48, 0.36, 0.76), 'runs': (0.18, 0.62, 0.42), 'light_controls': (0.82, 0.25, 0.36),
             'loose': (0.61, 0.61, 0.23)}
sys.path.insert(0, os.path.join(REPO, 'tools', 'recon'))
import holistic as H  # noqa: E402

# the compartment (docs/INTERIOR_HANDOFF.md section 3): floor 0.62, roof 2.10, walls +-1.32, rear bulkhead -0.936
SHELL = [('floor', (0.0, 1.8, 0.60), (2.64, 5.5, 0.04)), ('roof', (0.0, 1.8, 2.12), (2.64, 5.5, 0.04)),
         ('wall_l', (-1.34, 1.8, 1.36), (0.04, 5.5, 1.5)), ('wall_r', (1.34, 1.8, 1.36), (0.04, 5.5, 1.5)),
         ('bulkhead', (0.0, -0.956, 1.36), (2.64, 0.04, 1.5))]
VIEWS = {  # name: (camera position, look-at, lens mm) in the body frame
    'aisle_rear_fwd': ((-0.05, -0.75, 1.62), (-0.05, 3.5, 1.2), 14),    # from the rear, over the cabinet, forward
    'aisle_right_wall': ((-0.2, 0.2, 1.5), (1.2, 1.9, 1.35), 15),      # like g092-a
    'aisle_left_wall': ((0.65, 1.5, 1.55), (-1.2, 1.5, 1.25), 15),     # like g094-a
    'cab_back': ((0.0, 4.5, 1.75), (0.0, 0.0, 1.0), 14),
    'rear_bulkhead': ((0.2, 2.2, 1.6), (0.0, -1.0, 1.15), 18),
    'top': ((0.0, 1.0, 9.0), (0.0, 1.0, 0.0), 24),
}


def mat(name, rgb, alpha):
    """Workbench draws a material's viewport colour: what only Fig. 3 or the photos place is drawn paler."""
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*[c + (1.0 - c) * (1.0 - alpha) * 0.8 for c in rgb], 1.0)
    return m


def cube(name, centre, size, m, yaw_deg=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=centre, rotation=(0.0, 0.0, -math.radians(yaw_deg)))
    o = bpy.context.active_object
    o.name = name
    o.scale = [max(0.01, s) for s in size]
    o.data.materials.append(m)
    return o


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    out = os.path.abspath(argv[argv.index('--out') + 1]) if '--out' in argv else os.path.join(REPO, 'out', 'recon', 'holistic', 'blockout')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    reg = json.load(open(REGISTER, encoding='utf-8'))['components']
    mats = {}
    for g, rgb in GROUP_RGB.items():
        for kind, a in (('measured', 1.0), ('plan', 0.55), ('estimate', 0.3)):
            mats[(g, kind)] = mat('%s_%s' % (g, kind), rgb, a)
    shell = mat('shell', (0.78, 0.8, 0.76), 1.0)
    for name, c, s in SHELL:
        if name != 'roof':
            cube('shell_' + name, c, s, shell)
    n = 0
    for e in reg:
        if e['class'] in ('hull_wall', 'floor', 'roof', 'bulkhead') or e['zone'] in ('outside', 'engine'):
            continue
        b = e.get('box_m') or {}
        est = e.get('estimate') or {}
        c = list(b.get('centre') or [None] * 3)
        s = list(b.get('size') or [None] * 3)
        kind = 'measured' if None not in c and None not in s else 'plan' if b else 'estimate'
        if est:
            c = [c[i] if c[i] is not None else est['centre'][i] for i in range(3)]
        if None in c:
            continue
        sig = est.get('sigma_m', 0.2) if kind == 'estimate' else b.get('sigma_m', 0.2)
        default = {'cable': 0.03, 'loom': 0.05, 'lamp': 0.08, 'plate': 0.1, 'switch': 0.06}.get(e['class'], min(0.25, sig))
        s = [s[i] if s[i] is not None else default for i in range(3)]
        if e['class'] not in ('rack', 'bench', 'console', 'duct', 'loom', 'cable', 'pipe', 'hose', 'rail') and max(s) > 1.2:
            continue    # a group mesh of the cabin (the cab's fittings, levers), not one component
        yaw = b.get('yaw_deg', 0.0)
        if yaw:
            s = b['size_local']
        cube(e['id'], c, s, mats[(H.CLASS_GROUP[e['class']], kind)], yaw)
        n += 1
    print('blockout', n, 'components')
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.render.resolution_x, sc.render.resolution_y = 1600, 1000
    sh = sc.display.shading
    sh.light, sh.color_type = 'STUDIO', 'MATERIAL'
    sh.show_object_outline, sh.object_outline_color = True, (0.1, 0.1, 0.1)
    sh.show_cavity, sh.cavity_type = True, 'WORLD'
    sh.background_type, sh.background_color = 'VIEWPORT', (0.95, 0.95, 0.93)
    sc.view_settings.view_transform = 'Standard'
    cam_data = bpy.data.cameras.new('cam')
    cam = bpy.data.objects.new('cam', cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    for name, (p, t, lens) in VIEWS.items():
        cam.location = p
        d = Vector(t) - Vector(p)
        cam.rotation_euler = d.to_track_quat('-Z', 'Y' if name != 'top' else 'Y').to_euler()
        cam_data.lens = lens
        cam_data.clip_start = 0.02
        sc.render.filepath = '%s_%s.png' % (out, name)
        bpy.ops.render.render(write_still=True)
        print('rendered', sc.render.filepath)


main()
