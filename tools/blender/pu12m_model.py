"""The PU-12M's exterior in Blender, built from its trace (pu12m_trace.py; docs/PU12_DEVELOPMENT.md P5).

    "C:/Program Files/Blender Foundation/Blender 4.2/blender.exe" --background --python tools/blender/pu12m_model.py -- [--preview out/pu12m]

Writes art/pu12m/SM_PU12M.fbx (hull, wheels, fittings, the mast's fixed base), SM_PU12M_Mast1..5.fbx (the moving sections,
each with its origin at its top centre so Unreal sets one height per section from pu12m_trace.mast_section_tops),
SM_PU12M_AB1.fbx (the generator set, shown on the ground when it is set out), SM_PU12M_Guy.fbx (a guy wire 1 m long along
+Z, stretched by Unreal) and art/pu12m/pu12m.blend. The vehicle points along Blender -Y (Unreal +X after export), stands on
Z = 0 and is centred between its axles, as the other support vehicles (build_support_vehicles.py). Trace frame: X
forward, Y left; Blender X = trace Y, Blender Y = -trace X.
"""
import importlib
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import build_support_vehicles as S  # noqa: E402  (Part, materials, export, preview)
import pu12m_trace as T  # noqa: E402

importlib.reload(T)

REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(REPO, 'art', 'pu12m')


def B(x, y, z):
    """Trace (X forward, Y left, Z up) to Blender (X = left, Y = back)."""
    return (y, -x, z)


def side_plate_w(z):
    """The upper side plate's half-width at height z (between the chine and the roof edge, middle stations)."""
    zc, wc = 1.375, T.HALF_W
    zt, wt = T.ROOF_Z, T.ROOF_HALF_W
    return wc + (wt - wc) * (z - zc) / (zt - zc)


def hull(p):
    rings = []
    for x, *pts in T.STATIONS:
        right = [B(x, -w, z) for z, w in pts]
        left = [B(x, w, z) for z, w in reversed(pts)]
        rings.append(right + left)
    p.loft(rings, 'M_Paint')
    sb = T.STERN_BOX
    p.box(B(0.5 * (sb['x0'] + sb['x1']), 0.0, 0.5 * (sb['z0'] + sb['z1'])), (2 * sb['half_w'], sb['x1'] - sb['x0'], sb['z1'] - sb['z0']), 'M_Paint')


def wheels(p):
    for x, y in T.WHEELS:
        bx, by, _ = B(x, y, 0.0)
        p.cylinder((bx, by, T.TYRE_RADIUS), T.TYRE_RADIUS, T.TYRE_WIDTH, 'X', 'M_Tyre', 32)
        out = 1.0 if bx > 0 else -1.0
        # the rim's disc just proud of the tyre's face and the hub's cap beyond it, inside the published width
        p.cylinder((bx + out * (0.5 * T.TYRE_WIDTH - 0.015), by, T.TYRE_RADIUS), T.TYRE_RADIUS * 0.5, 0.05, 'X', 'M_Paint', 24)
        p.cylinder((bx + out * (0.5 * T.TYRE_WIDTH + 0.02), by, T.TYRE_RADIUS), 0.12, 0.04, 'X', 'M_Metal', 16)


def roof(p):
    z = T.ROOF_Z
    for x, y, lx, ly in T.ROOF_HATCHES:
        p.box(B(x, y, z + 0.02), (ly, lx, 0.04), 'M_Paint')
        p.box(B(x, y, z + 0.045), (ly * 0.9, lx * 0.9, 0.012), 'M_Dark')
    for x0, x1, yc, hw in T.ENGINE_GRILLES:
        p.box(B(0.5 * (x0 + x1), yc, z + 0.012), (2 * hw, x1 - x0, 0.024), 'M_Dark')
    b = T.ROOF_BOX
    p.box(B(0.5 * (b['x0'] + b['x1']), 0.5 * (b['y0'] + b['y1']), 0.5 * (b['z0'] + b['z1'])),
          (b['y1'] - b['y0'], b['x1'] - b['x0'], b['z1'] - b['z0']), 'M_Paint')
    for x, y in T.WHIPS:
        bx, by, _ = B(x, y, 0.0)
        p.cylinder((bx, by, z + 0.06), 0.05, 0.12, 'Z', 'M_Metal', 12)
        p.cylinder((bx, by, z + 0.12 + 0.5 * T.WHIP_LENGTH), 0.008, T.WHIP_LENGTH, 'Z', 'M_Dark', 6)
    for x0, x1, y in T.RAILS:
        for xx in (x0, x1):
            bx, by, _ = B(xx, y, 0.0)
            p.cylinder((bx, by, z + 0.5 * T.RAIL_HEIGHT), 0.015, T.RAIL_HEIGHT, 'Z', 'M_Metal', 8)
        bx, by, _ = B(0.5 * (x0 + x1), y, 0.0)
        p.cylinder((bx, by, z + T.RAIL_HEIGHT), 0.015, x1 - x0, 'Y', 'M_Metal', 8)


def front(p):
    # vision blocks along the top of the bow plate, tilted with the glacis
    for x, y in T.VISION_BLOCKS:
        p.box(B(x, y, 1.90), (0.16, 0.05, 0.09), 'M_Glass', rot_x=-0.45)
    for x, y, z in T.HEADLIGHTS:
        bx, by, bz = B(x, y, z)
        p.cylinder((bx, by, bz), 0.09, 0.12, 'Y', 'M_Metal', 16)
        p.cylinder((bx, by - 0.065, bz), 0.075, 0.01, 'Y', 'M_Glass', 16)
    x, y, z = T.SEARCHLIGHT
    bx, by, bz = B(x, y, z)
    p.cylinder((bx, by, bz), 0.11, 0.16, 'Y', 'M_Metal', 16)
    p.cylinder((bx, by - 0.085, bz), 0.095, 0.01, 'Y', 'M_Glass', 16)
    p.cylinder((bx, by, T.ROOF_Z + 0.5 * (bz - T.ROOF_Z)), 0.02, bz - T.ROOF_Z, 'Z', 'M_Metal', 8)


def sides(p):
    tilt = math.atan((T.HALF_W - T.ROOF_HALF_W) / (T.ROOF_Z - 1.375))  # the upper side plate leans in
    for side in (1.0, -1.0):
        for x, zc, lx, lz in T.SIDE_HATCHES:
            w = side_plate_w(zc) + 0.012
            bx, by, bz = B(x, side * w, zc)
            # a thin plate lying on the leaning side plate (rotate about the vehicle's long axis, Blender Y)
            p.box((bx, by, bz), (0.024, lx, lz), 'M_Paint', rot_z=0.0)
            p.bm.verts.ensure_lookup_table()
            n = len(p.bm.verts)
            for v in p.bm.verts[n - 8:]:
                dx, dz = v.co.x - bx, v.co.z - bz
                a = side * tilt
                v.co.x = bx + dx * math.cos(a) + dz * math.sin(a)
                v.co.z = bz - dx * math.sin(a) + dz * math.cos(a)
    # the stowage box on the right upper side
    sb = T.SIDE_BOX
    zc = 0.5 * (sb['z0'] + sb['z1'])
    w = side_plate_w(zc) + 0.5 * sb['depth']
    bx, by, bz = B(0.5 * (sb['x0'] + sb['x1']), -w, zc)
    p.box((bx, by, bz), (sb['depth'], sb['x1'] - sb['x0'], sb['z1'] - sb['z0']), 'M_Paint')
    x0, x1, z, y = T.EXHAUST
    bx, by, bz = B(0.5 * (x0 + x1), y, z)
    p.cylinder((bx, by, bz), 0.07, x1 - x0, 'Y', 'M_Dark', 14)


def mast_base(p):
    bx, by, _ = B(T.MAST_X, T.MAST_Y, 0.0)
    r0, _ = T.MAST_SECTIONS[0]
    top = T.MAST_BASE_TOP_Z
    # the part that shows above the roof, and a collar where it comes through
    p.cylinder((bx, by, 0.5 * (T.ROOF_Z + top)), r0, top - T.ROOF_Z, 'Z', 'M_Metal', 20)
    p.cylinder((bx, by, T.ROOF_Z + 0.04), r0 + 0.05, 0.08, 'Z', 'M_Paint', 20)
    p.cylinder((bx, by, top - 0.03), r0 + 0.015, 0.06, 'Z', 'M_Metal', 20)


def mast_section(i):
    """Section i (1..5): a tube with its origin at its top centre, hanging down along -Z."""
    r, length = T.MAST_SECTIONS[i]
    p = S.Part()
    p.cylinder((0.0, 0.0, -0.5 * length), r, length, 'Z', 'M_Metal', 18)
    p.cylinder((0.0, 0.0, -0.03), r + 0.012, 0.06, 'Z', 'M_Metal', 18)  # the clamp at its top
    if i == len(T.MAST_SECTIONS) - 1:
        p.cylinder((0.0, 0.0, 0.10), 0.05, 0.20, 'Z', 'M_Dark', 12)  # the antenna head on the top section
    return p.finish('SM_PU12M_Mast%d' % i)


def ab1():
    sx, sy, sz = T.AB1['size']
    p = S.Part()
    p.box((0.0, 0.0, 0.5 * sz + 0.05), (sy, sx, sz), 'M_Paint')
    p.box((0.0, 0.0, 0.025), (sy + 0.05, sx + 0.05, 0.05), 'M_Metal')  # its frame
    p.cylinder((0.0, 0.5 * sx - 0.12, sz + 0.12), 0.04, 0.14, 'Z', 'M_Dark', 10)  # the exhaust
    return p.finish('SM_PU12M_AB1')


def guy():
    p = S.Part()
    p.cylinder((0.0, 0.0, 0.5), 0.006, 1.0, 'Z', 'M_Dark', 6)
    return p.finish('SM_PU12M_Guy')


def build():
    p = S.Part()
    hull(p)
    wheels(p)
    roof(p)
    front(p)
    sides(p)
    mast_base(p)
    objs = [p.finish('SM_PU12M')]
    objs += [mast_section(i) for i in range(1, len(T.MAST_SECTIONS))]
    objs += [ab1(), guy()]
    return objs


def place_for_preview(objs, extension=0.0):
    """Stand the sections and the AB-1 where they go (only for renders: the exported meshes keep their own origins)."""
    tops = T.mast_section_tops(extension)
    bx, by, _ = B(T.MAST_X, T.MAST_Y, 0.0)
    for o in objs:
        if o.name.startswith('SM_PU12M_Mast'):
            i = int(o.name[len('SM_PU12M_Mast'):])
            o.location = (bx, by, tops[i])
        elif o.name == 'SM_PU12M_AB1':
            ax, ay, _ = B(T.AB1['x'], T.AB1['y'], 0.0)
            o.location = (ax, ay, 0.0)
        elif o.name == 'SM_PU12M_Guy':
            o.hide_render = True


def write_rig():
    """art/pu12m/pu12m_rig.json: where Unreal stands the moving parts (USimPu12MastComponent). Unreal's actor frame: X
    forward, Y right, Z up, centimetres (trace Y is left: Unreal Y = -trace Y)."""
    import json
    cm = 100.0
    rig = {
        'note': 'Written by tools/blender/pu12m_model.py from pu12m_trace.py. Unreal frame: X forward, Y right, Z up, cm.',
        'mast': {'x': T.MAST_X * cm, 'y': -T.MAST_Y * cm, 'foot_z': T.MAST_FOOT_Z * cm, 'base_top_z': T.MAST_BASE_TOP_Z * cm,
                 'stowed_top_z': T.MAST_STOWED_TOP_Z * cm, 'raised_top_z': T.MAST_TOP_RAISED * cm,
                 'sections': [{'radius': r * cm, 'length': l * cm, 'mesh': 'SM_PU12M_Mast%d' % i if i else ''}
                              for i, (r, l) in enumerate(T.MAST_SECTIONS)],
                 'nest_step': 0.05 * cm},
        'guys': {'levels': list(T.GUY_LEVELS), 'radius': T.GUY_RADIUS * cm, 'mesh': 'SM_PU12M_Guy'},
        'ab1': {'x': T.AB1['x'] * cm, 'y': -T.AB1['y'] * cm, 'mesh': 'SM_PU12M_AB1'},
    }
    path = os.path.join(OUT, 'pu12m_rig.json')
    json.dump(rig, open(path, 'w'), indent=1)
    print('rig ->', path)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    preview = os.path.abspath(argv[argv.index('--preview') + 1]) if '--preview' in argv else None
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    objs = build()
    for o in objs:
        mod = o.modifiers.new('EdgeSplit', 'EDGE_SPLIT')
        mod.split_angle = math.radians(30)
        path = os.path.join(OUT, o.name + '.fbx')
        bpy.ops.object.select_all(action='DESELECT')
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                                 axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True, mesh_smooth_type='FACE',
                                 add_leaf_bones=False, bake_anim=False)
        print(o.name, 'faces', len(o.data.polygons), 'size', tuple(round(d, 3) for d in o.dimensions), '->', path)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'pu12m.blend'))
    write_rig()
    if preview:
        os.makedirs(preview, exist_ok=True)
        S.setup_render()
        for ext, tag in ((0.0, 'stowed'), (0.35, 'raising')):
            place_for_preview(objs, ext)
            for m in bpy.data.materials:
                m.use_nodes = True
                bsdf = m.node_tree.nodes.get('Principled BSDF')
                key = m.name.split('.')[0]
                if bsdf is not None and key in S.COLOURS:
                    bsdf.inputs['Base Color'].default_value = S.COLOURS[key]
                    bsdf.inputs['Roughness'].default_value = 0.6
            cam = bpy.context.scene.camera
            from mathutils import Vector
            target = Vector((0.0, 0.0, 1.6 if ext == 0.0 else 4.0))
            cam.location = Vector((-10.0, -11.0, 5.5)) if ext == 0.0 else Vector((-15.0, -17.0, 7.0))
            cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
            bpy.context.scene.render.filepath = os.path.join(preview, 'pu12m_%s.png' % tag)
            bpy.ops.render.render(write_still=True)
        print('previews in', preview)


if __name__ == '__main__':
    main()
