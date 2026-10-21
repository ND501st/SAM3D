"""Build the crew's body parts in Blender from code and export them for Unreal (roadmap 8.10).

A crewman is drawn as rigid parts that simcore poses (simcore/crew_body.hpp, PartsOf): each part has its origin at its
proximal joint, its bone along +X and its "up" along +Z (Blender +Y is the part's left), so Unreal's FBX import lands
it with the bone along the mesh's +X and up along +Z, the frame USimCrewBodyComponent puts it in. The parts are sized
from data/crew/body.json (the same numbers simcore uses), so a joint of one part meets the next.

    SM_Crew_Pelvis      origin between the hip joints; +X up to the waist, +Z forward
    SM_Crew_Torso       origin between the hip joints; +X up the spine to the shoulder joints, +Z forward (the chest)
    SM_Crew_Neck        origin at the base of the neck; +X up to the head's pivot
    SM_Crew_Head        origin at the head's pivot; +X the face's direction, +Z up: the padded tank helmet
                        (shlemofon) with its ribs and earphone cups, the face
    SM_Crew_UpperArm    shoulder to elbow          SM_Crew_Forearm   elbow to wrist (the sleeve, the wrist)
    SM_Crew_Hand<Shape>_<L|R>   wrist to the fingers, +Z the back of the hand: Rest, Point (index out), Grip (fist)
    SM_Crew_Thigh       hip joint to knee, +Z the top of the thigh
    SM_Crew_Shin        knee to ankle, +Z the front: the trouser, then the tall boot
    SM_Crew_Foot        ankle to the toe, +Z up: the boot

Looks (by eye, sam3d): an olive-khaki field uniform, a black padded tank helmet with an intercom headset, tall black
boots, a brown belt. Material slots M_CrewCloth, M_CrewSkin, M_CrewHelmet, M_CrewBoot, M_CrewBelt.

    "C:/Program Files/Blender Foundation/Blender 4.2/blender.exe" --background --python tools/blender/build_crew_body.py
    ... -- --preview out/crew_body              # also render the parts laid out

Writes art/crew_body/<name>.fbx and art/crew_body/crew_body.blend (Git LFS).
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, 'art', 'crew_body')
BODY = os.path.join(REPO, 'data', 'crew', 'body.json')
SLOTS = ['M_CrewCloth', 'M_CrewSkin', 'M_CrewHelmet', 'M_CrewBoot', 'M_CrewBelt']
COLOURS = {'M_CrewCloth': (0.20, 0.22, 0.13, 1), 'M_CrewSkin': (0.62, 0.45, 0.36, 1), 'M_CrewHelmet': (0.03, 0.03, 0.03, 1),
           'M_CrewBoot': (0.02, 0.02, 0.02, 1), 'M_CrewBelt': (0.25, 0.14, 0.07, 1)}


def load_dims():
    """The lengths simcore/crew_body.hpp derives from data/crew/body.json (BodyDims)."""
    with open(BODY, encoding='utf-8') as f:
        b = json.load(f)
    h = b['body']['stature_m']
    fr = b['fractions']
    d = {
        'upper_arm': fr['upper_arm'] * h, 'forearm': fr['forearm'] * h, 'hand': fr['hand'] * h,
        'thigh': (fr['hip'] - fr['knee']) * h, 'shank': (fr['knee'] - fr['ankle']) * h, 'foot': fr['foot'] * h,
        'torso': (fr['shoulder'] - fr['hip']) * h, 'eye_above_shoulder': (fr['eye'] - fr['shoulder']) * h,
        'shoulder_half': 0.5 * fr['shoulder_breadth'] * h - 0.04, 'hip_half': 0.5 * fr['hip_breadth'] * h - 0.07,
        'ankle': fr['ankle'] * h, 'eye_ahead': b['seat']['eye_ahead_m'], 'hip_above_seat': b['seat']['hip_above_seat_m'],
    }
    d['grip'] = 0.45 * d['hand']
    d['point'] = 0.95 * d['hand']
    return d


class Part:
    """Collects closed shells into one bmesh with per-face material indices (Blender coordinates)."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()

    def add(self, verts, faces, slot):
        vs = [self.bm.verts.new(v) for v in verts]
        idx = SLOTS.index(slot)
        for f in faces:
            face = self.bm.faces.new([vs[i] for i in f])
            face.material_index = idx

    def loft(self, rings, slot, cap=True):
        """Skin a list of closed rings (same point count) into a tube, capped at both ends."""
        n = len(rings[0])
        verts = [p for ring in rings for p in ring]
        faces = []
        for s in range(len(rings) - 1):
            for i in range(n):
                a, b = s * n + i, s * n + (i + 1) % n
                faces.append((a, b, b + n, a + n))
        if cap:
            faces.append(tuple(reversed(range(n))))
            last = (len(rings) - 1) * n
            faces.append(tuple(range(last, last + n)))
        self.add(verts, faces, slot)

    def along_x(self, sections, slot, seg=16):
        """A body of revolution-like shell along +X: sections are (x, centre_z, half_y, half_z) ellipses."""
        rings = []
        for x, cz, ry, rz in sections:
            rings.append([(x, ry * math.cos(2 * math.pi * i / seg), cz + rz * math.sin(2 * math.pi * i / seg))
                          for i in range(seg)])
        self.loft(rings, slot)

    def capsule(self, x0, x1, r0, r1, slot, seg=14, centre=(0.0, 0.0)):
        """A tapered capsule from x0 (radius r0) to x1 (radius r1), rounded ends."""
        cy, cz = centre
        secs = []
        for k in range(4):  # the near cap
            a = math.pi / 2 * (1 - k / 4)
            secs.append((x0 - r0 * math.sin(a) * 0.8, r0 * math.cos(a)))
        steps = 4
        for k in range(steps + 1):
            t = k / steps
            secs.append((x0 + (x1 - x0) * t, r0 + (r1 - r0) * t))
        for k in range(1, 5):  # the far cap
            a = math.pi / 2 * k / 4
            secs.append((x1 + r1 * math.sin(a) * 0.8, max(r1 * math.cos(a), 0.002)))
        rings = []
        for x, r in secs:
            r = max(r, 0.002)
            rings.append([(x, cy + r * math.cos(2 * math.pi * i / seg), cz + r * math.sin(2 * math.pi * i / seg))
                          for i in range(seg)])
        self.loft(rings, slot)

    def tube(self, points, radius, slot, seg=10):
        """A rounded tube through points (a finger, a rib)."""
        pts = [Vector(p) for p in points]
        rings = []
        for k, p in enumerate(pts):
            d = (pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]).normalized()
            a = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
            u = d.cross(a).normalized()
            v = d.cross(u).normalized()
            r = radius * (0.85 if k in (0, len(pts) - 1) else 1.0)
            rings.append([tuple(p + (u * math.cos(2 * math.pi * i / seg) + v * math.sin(2 * math.pi * i / seg)) * r)
                          for i in range(seg)])
        # round the ends a little: one shrunken ring beyond each end
        d0 = (pts[0] - pts[1]).normalized()
        d1 = (pts[-1] - pts[-2]).normalized()
        start = [tuple(Vector(q) + d0 * radius * 0.6 - (Vector(q) - pts[0]) * 0.5) for q in rings[0]]
        end = [tuple(Vector(q) + d1 * radius * 0.6 - (Vector(q) - pts[-1]) * 0.5) for q in rings[-1]]
        self.loft([start] + rings + [end], slot)

    def ellipsoid(self, centre, radii, slot, seg=20, rings_n=12, skip=None):
        """An ellipsoid; `skip(centroid)` leaves out the faces it returns True for (an opening)."""
        cx, cy, cz = centre
        rx, ry, rz = radii
        verts = [(cx, cy, cz - rz)]
        for j in range(1, rings_n):
            phi = math.pi * j / rings_n - math.pi / 2
            for i in range(seg):
                th = 2 * math.pi * i / seg
                verts.append((cx + rx * math.cos(phi) * math.cos(th), cy + ry * math.cos(phi) * math.sin(th),
                              cz + rz * math.sin(phi)))
        verts.append((cx, cy, cz + rz))
        faces = []
        for i in range(seg):
            faces.append((0, 1 + (i + 1) % seg, 1 + i))
        for j in range(rings_n - 2):
            a0 = 1 + j * seg
            a1 = 1 + (j + 1) * seg
            for i in range(seg):
                faces.append((a0 + i, a0 + (i + 1) % seg, a1 + (i + 1) % seg, a1 + i))
        top = len(verts) - 1
        last = 1 + (rings_n - 2) * seg
        for i in range(seg):
            faces.append((last + i, last + (i + 1) % seg, top))
        if skip is not None:
            def centroid(f):
                return tuple(sum(verts[k][a] for k in f) / len(f) for a in range(3))
            faces = [f for f in faces if not skip(centroid(f))]
        self.add(verts, faces, slot)

    def cylinder_y(self, centre, radius, width, slot, seg=18):
        """A short cylinder across Y (an earphone cup)."""
        cx, cy, cz = centre
        rings = []
        for off in (-0.5 * width, 0.5 * width):
            rings.append([(cx + radius * math.cos(2 * math.pi * i / seg), cy + off, cz + radius * math.sin(2 * math.pi * i / seg))
                          for i in range(seg)])
        self.loft(rings, slot)

    def finish(self):
        mesh = bpy.data.meshes.new(self.name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces[:])
        bmesh.ops.triangulate(self.bm, faces=[f for f in self.bm.faces if len(f.verts) > 4])
        self.bm.to_mesh(mesh)
        self.bm.free()
        for s in SLOTS:
            mat = bpy.data.materials.get(s) or bpy.data.materials.new(s)
            mat.diffuse_color = COLOURS[s]
            mesh.materials.append(mat)
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        mesh.shade_smooth()
        if hasattr(mesh, 'set_sharp_from_angle'):
            mesh.set_sharp_from_angle(angle=math.radians(60))
        return obj


# ------------------------------------------------------------------------------------------------ the parts

def pelvis(d):
    p = Part('SM_Crew_Pelvis')
    # (x up, centre z forward, half width, half depth): the seat of the trousers to the waist
    p.along_x([(-0.11, -0.03, 0.10, 0.07), (-0.08, -0.025, 0.15, 0.10), (0.0, -0.02, 0.175, 0.11),
               (0.10, -0.01, 0.165, 0.10), (0.17, -0.005, 0.15, 0.095)], 'M_CrewCloth')
    return p.finish()


def torso(d):
    t = Part('SM_Crew_Torso')
    s = d['torso']
    t.along_x([(0.10, -0.005, 0.152, 0.096), (0.25, 0.0, 0.156, 0.10), (0.36, 0.01, 0.172, 0.115),
               (s - 0.05, 0.0, 0.19, 0.10), (s + 0.015, -0.005, 0.20, 0.075), (s + 0.055, -0.01, 0.10, 0.055),
               (s + 0.075, -0.01, 0.06, 0.045)], 'M_CrewCloth', seg=20)
    # the belt round the waist
    t.along_x([(0.105, -0.005, 0.158, 0.101), (0.150, -0.004, 0.159, 0.102)], 'M_CrewBelt', seg=20)
    return t.finish()


def neck(d):
    n = Part('SM_Crew_Neck')
    n.capsule(-0.02, 0.09, 0.052, 0.048, 'M_CrewSkin')
    return n.finish()


def head(d):
    """The head about its pivot (the top of the neck): the eyes are eye_ahead forward and (eye_above_shoulder - 0.13) up
    (simcore SolveBody), the face just in front of them."""
    h = Part('SM_Crew_Head')
    eye_up = d['eye_above_shoulder'] - 0.13
    eye_fwd = d['eye_ahead']
    face_c = (0.012, 0.0, eye_up - 0.01)
    h.ellipsoid(face_c, (eye_fwd + 0.016, 0.074, 0.108), 'M_CrewSkin')
    # the tank helmet over the crown, the back of the head down to the nape and the sides, open for the face
    hc = (-0.012, 0.0, eye_up + 0.015)
    hr = (0.108, 0.088, 0.12)
    h.ellipsoid(hc, hr, 'M_CrewHelmet', seg=24, rings_n=14,
                skip=lambda c: c[0] > 0.03 and c[2] < eye_up + 0.03)
    # its padded ribs, front to back over the crown
    for y in (-0.036, 0.0, 0.036):
        pts = []
        for k in range(9):
            a = math.radians(25 + 130 * k / 8)
            pts.append((hc[0] + (hr[0] + 0.004) * math.cos(a), y, hc[2] + (hr[2] + 0.004) * math.sin(a) * math.cos(math.asin(min(abs(y) / hr[1], 0.9)))))
        h.tube(pts, 0.012, 'M_CrewHelmet')
    # the earphone cups and the chin strap
    for side in (-1, 1):
        h.cylinder_y((-0.008, side * (hr[1] + 0.006), eye_up - 0.005), 0.036, 0.024, 'M_CrewHelmet')
    h.tube([(-0.008, -0.088, eye_up - 0.03), (0.035, -0.06, eye_up - 0.095), (0.05, 0.0, eye_up - 0.11),
            (0.035, 0.06, eye_up - 0.095), (-0.008, 0.088, eye_up - 0.03)], 0.006, 'M_CrewHelmet')
    # the nose and the eyes
    h.ellipsoid((eye_fwd + 0.027, 0.0, eye_up - 0.02), (0.02, 0.012, 0.024), 'M_CrewSkin', seg=10, rings_n=6)
    for side in (-1, 1):
        h.ellipsoid((eye_fwd + 0.018, side * 0.032, eye_up), (0.006, 0.009, 0.006), 'M_CrewHelmet', seg=8, rings_n=5)
    return h.finish()


def upper_arm(d):
    a = Part('SM_Crew_UpperArm')
    a.capsule(0.0, d['upper_arm'], 0.056, 0.044, 'M_CrewCloth')
    return a.finish()


def forearm(d):
    f = Part('SM_Crew_Forearm')
    L = d['forearm']
    f.capsule(0.0, L - 0.03, 0.045, 0.034, 'M_CrewCloth')
    f.capsule(L - 0.04, L + 0.005, 0.026, 0.025, 'M_CrewSkin', seg=12)
    return f.finish()


def hand(d, shape, side):
    """A hand at the wrist, fingers along +X, back of the hand +Z. side +1 the right hand (its thumb on +Y, the body's
    side of it when the palm is down), -1 the left."""
    name = 'SM_Crew_Hand%s_%s' % (shape, 'R' if side > 0 else 'L')
    p = Part(name)
    p.along_x([(-0.005, -0.001, 0.026, 0.012), (0.03, -0.001, 0.042, 0.016), (0.09, -0.002, 0.044, 0.014),
               (0.105, -0.004, 0.038, 0.011)], 'M_CrewSkin', seg=14)
    s = side
    thumb_base = (0.02, s * 0.038, -0.010)
    fingers_y = [s * 0.030, s * 0.010, -s * 0.010, -s * 0.029]  # index on the thumb's side, the little finger outside
    lengths = [1.0, 1.05, 0.98, 0.82]
    tip = d['point']
    for k, (y, l) in enumerate(zip(fingers_y, lengths)):
        if shape == 'Point' and k == 0:
            p.tube([(0.10, y, 0.0), (0.14, y, -0.002), (tip - 0.008, y, -0.004)], 0.009, 'M_CrewSkin')
        elif shape == 'Rest':
            p.tube([(0.10, y, 0.0), (0.10 + 0.04 * l, y, -0.01), (0.10 + 0.07 * l, y, -0.03)], 0.009, 'M_CrewSkin')
        else:  # curled: a fist round a knob or a rim below the palm
            p.tube([(0.10, y, 0.0), (0.125, y, -0.022), (0.118, y, -0.048), (0.095, y, -0.056)], 0.0095, 'M_CrewSkin')
    if shape == 'Grip':
        p.tube([thumb_base, (0.065, s * 0.05, -0.035), (0.092, s * 0.03, -0.05)], 0.011, 'M_CrewSkin')
    else:
        p.tube([thumb_base, (0.06, s * 0.056, -0.018), (0.09, s * 0.05, -0.026)], 0.011, 'M_CrewSkin')
    return p.finish()


def thigh(d):
    t = Part('SM_Crew_Thigh')
    L = d['thigh']
    t.along_x([(-0.03, 0.0, 0.06, 0.06), (0.03, 0.005, 0.085, 0.08), (0.20, 0.0, 0.075, 0.068), (L - 0.05, -0.005, 0.06, 0.056),
               (L + 0.02, -0.005, 0.052, 0.05), (L + 0.05, -0.005, 0.03, 0.03)], 'M_CrewCloth')
    return t.finish()


def shin(d):
    s = Part('SM_Crew_Shin')
    L = d['shank']
    s.along_x([(-0.03, 0.0, 0.035, 0.035), (0.0, 0.0, 0.052, 0.052), (0.14, 0.0, 0.05, 0.05), (0.16, 0.0, 0.05, 0.05)],
              'M_CrewCloth')
    # the tall boot: its top just below the knee
    s.along_x([(0.13, 0.0, 0.058, 0.058), (0.16, 0.0, 0.06, 0.06), (L - 0.08, 0.0, 0.05, 0.05), (L + 0.01, 0.0, 0.047, 0.047),
               (L + 0.03, 0.0, 0.03, 0.03)], 'M_CrewBoot')
    return s.finish()


def foot(d):
    f = Part('SM_Crew_Foot')
    a = d['ankle']
    # (x forward, centre z, half width, half height): the boot from the heel to the toe, its sole on the floor
    secs = [(-0.065, -a + 0.025, 0.033, 0.025), (-0.04, -a + 0.045, 0.042, 0.048), (0.04, -a + 0.04, 0.046, 0.04),
            (0.13, -a + 0.03, 0.048, 0.03), (0.185, -a + 0.022, 0.04, 0.022), (0.205, -a + 0.016, 0.022, 0.016)]
    f.along_x(secs, 'M_CrewBoot')
    return f.finish()


def build(d):
    objs = [pelvis(d), torso(d), neck(d), head(d), upper_arm(d), forearm(d), thigh(d), shin(d), foot(d)]
    for shape in ('Rest', 'Point', 'Grip'):
        for side in (-1, 1):
            objs.append(hand(d, shape, side))
    return objs


def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                             axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True, mesh_smooth_type='FACE',
                             add_leaf_bones=False, bake_anim=False)
    return path


def preview(objs, folder):
    """The parts laid out in a row, and lit."""
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items.keys() else 'BLENDER_EEVEE'
    scene.render.resolution_x, scene.render.resolution_y = 1400, 700
    for i, o in enumerate(objs):
        o.location = (0.0, -0.32 * i + 2.0, 0.0)
        for m in o.data.materials:
            m.use_nodes = True
            bsdf = m.node_tree.nodes.get('Principled BSDF')
            if bsdf is not None:
                bsdf.inputs['Base Color'].default_value = COLOURS[m.name.split('.')[0]]
                bsdf.inputs['Roughness'].default_value = 0.7
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    scene.collection.objects.link(cam)
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 5.0
    cam.location = (0.25, 0.0, 2.5)
    cam.rotation_euler = (0.0, 0.0, math.radians(90))
    scene.camera = cam
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(30), 0.0, math.radians(20))
    scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('w')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.7, 0.72, 0.75, 1)
    scene.world = world
    scene.render.filepath = os.path.join(folder, 'crew_parts_top.png')
    bpy.ops.render.render(write_still=True)
    cam.location = (0.25, -6.0, 0.0)
    cam.rotation_euler = (math.radians(90), 0.0, 0.0)
    scene.render.filepath = os.path.join(folder, 'crew_parts_side.png')
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    folder = os.path.abspath(argv[argv.index('--preview') + 1]) if '--preview' in argv else None
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    d = load_dims()
    objs = build(d)
    for o in objs:
        print(o.name, 'faces', len(o.data.polygons), 'size', tuple(round(v, 3) for v in o.dimensions))
        export(o)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'crew_body.blend'))
    if folder:
        os.makedirs(folder, exist_ok=True)
        preview(objs, folder)
        print('preview in', folder)


if __name__ == '__main__':
    main()
