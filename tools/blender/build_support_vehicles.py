"""Build the battery's support vehicles in Blender from code and export them for Unreal (exteriors only, no interiors).

    SM_TZM_9T217    transport-loader for the Osa: BAZ-5937 6x6 amphibious hull, twelve missile containers on the rear
                    deck, the loading crane stowed over them (roadmap 7.3)
    SM_ATZ_38_131   fuel truck: ZIL-131 6x6 with the ATZ-3.8 tank and the rear pump cabinet (roadmap 7.4)
(The PU-12M battery command post has its own traced model: tools/blender/pu12m_model.py, art/pu12m/.)

Proportions from published dimensions (each function says which); shapes are simplified. The vehicle points along
Blender -Y (Unreal +X after export), stands on Z = 0, and is centred between its axles. Material slots: M_Paint, M_Tyre,
M_Glass, M_Dark, M_Metal, M_Container, so Unreal can assign materials per slot.

    "C:/Program Files/Blender Foundation/Blender 4.2/blender.exe" --background --python tools/blender/build_support_vehicles.py
    ... -- --preview out/support_vehicles            # also render a picture of each into that folder

Writes art/support_vehicles/<name>.fbx and art/support_vehicles/support_vehicles.blend (Git LFS).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, 'art', 'support_vehicles')
SLOTS = ['M_Paint', 'M_Tyre', 'M_Glass', 'M_Dark', 'M_Metal', 'M_Container']
COLOURS = {'M_Paint': (0.23, 0.27, 0.16, 1), 'M_Tyre': (0.03, 0.03, 0.03, 1), 'M_Glass': (0.12, 0.16, 0.18, 1),
           'M_Dark': (0.06, 0.07, 0.05, 1), 'M_Metal': (0.35, 0.35, 0.33, 1), 'M_Container': (0.30, 0.33, 0.22, 1)}


# ------------------------------------------------------------------------------------------------ building blocks

class Part:
    """Collects geometry into one bmesh with per-face material indices."""

    def __init__(self):
        self.bm = bmesh.new()

    def add(self, verts, faces, slot):
        vs = [self.bm.verts.new(v) for v in verts]
        idx = SLOTS.index(slot)
        for f in faces:
            face = self.bm.faces.new([vs[i] for i in f])
            face.material_index = idx

    def box(self, centre, size, slot='M_Paint', rot_z=0.0, rot_x=0.0):
        cx, cy, cz = centre
        sx, sy, sz = (s * 0.5 for s in size)
        m = Matrix.Rotation(rot_z, 3, 'Z') @ Matrix.Rotation(rot_x, 3, 'X')
        corners = [Vector((x, y, z)) for z in (-sz, sz) for y in (-sy, sy) for x in (-sx, sx)]
        verts = [tuple(m @ c + Vector(centre)) for c in corners]
        faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        self.add(verts, faces, slot)

    def loft(self, sections, slot='M_Paint', cap=True):
        """Join a list of closed rings (each a list of (x, y, z), same count) into a skin, capped at both ends."""
        n = len(sections[0])
        verts = [p for ring in sections for p in ring]
        faces = []
        for s in range(len(sections) - 1):
            for i in range(n):
                a, b = s * n + i, s * n + (i + 1) % n
                faces.append((a, b, b + n, a + n))
        if cap:
            faces.append(tuple(reversed(range(n))))
            last = (len(sections) - 1) * n
            faces.append(tuple(range(last, last + n)))
        self.add(verts, faces, slot)

    def cylinder(self, centre, radius, length, axis='X', slot='M_Tyre', seg=24, radius2=None):
        r2 = radius if radius2 is None else radius2
        rings = []
        for k, (off, r) in enumerate(((-0.5 * length, radius), (0.5 * length, r2))):
            ring = []
            for i in range(seg):
                a = 2.0 * math.pi * i / seg
                u, v = r * math.cos(a), r * math.sin(a)
                if axis == 'X':
                    p = (off, u, v)
                elif axis == 'Y':
                    p = (u, off, v)
                else:
                    p = (u, v, off)
                ring.append(tuple(Vector(p) + Vector(centre)))
            rings.append(ring)
        self.loft(rings, slot)

    def wheel(self, x, y, radius, width):
        self.cylinder((x, y, radius), radius, width, 'X', 'M_Tyre', 28)
        side = 1.0 if x > 0 else -1.0
        self.cylinder((x + side * 0.5 * width, y, radius), radius * 0.55, 0.04, 'X', 'M_Metal', 20)

    def finish(self, name):
        mesh = bpy.data.meshes.new(name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces[:])
        self.bm.to_mesh(mesh)
        self.bm.free()
        for s in SLOTS:
            mat = bpy.data.materials.get(s) or bpy.data.materials.new(s)
            mat.diffuse_color = COLOURS[s]
            mesh.materials.append(mat)
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def hull_ring(y, half_w_bottom, half_w_top, z_bottom, z_top, z_chine=None, half_w_chine=None):
    """A cross-section of a boat-shaped hull at station y: bottom, chine (widest), top deck edge."""
    zc = z_chine if z_chine is not None else 0.5 * (z_bottom + z_top)
    wc = half_w_chine if half_w_chine is not None else max(half_w_bottom, half_w_top)
    return [(-half_w_bottom, y, z_bottom), (half_w_bottom, y, z_bottom), (wc, y, zc), (half_w_top, y, z_top),
            (-half_w_top, y, z_top), (-wc, y, zc)]


# ------------------------------------------------------------------------------------------------ TZM 9T217

def tzm():
    """9T217 on the BAZ-5937 chassis: about 9.1 m long and 2.75 m wide like the 9A33 on the same chassis (data/systems/
    osa.json; ru_wiki), three axles, a boat hull for swimming. Twelve missile containers (two loads of six) lie along the
    rear deck, and the crane (flak11: it loads one missile a minute) is stowed above them."""
    p = Part()
    L, W = 9.1, 2.75
    # the hull: a raked bow at -Y, a flat stern at +Y
    sections = [
        hull_ring(-4.55, 0.55, 1.05, 0.95, 1.55, 1.3, 1.1),
        hull_ring(-3.9, 1.05, 1.35, 0.55, 1.75, 1.2, 1.375),
        hull_ring(-2.5, 1.15, 1.375, 0.5, 1.8, 1.15, 1.375),
        hull_ring(3.9, 1.15, 1.375, 0.5, 1.8, 1.15, 1.375),
        hull_ring(4.55, 1.05, 1.3, 0.62, 1.75, 1.15, 1.3),
    ]
    p.loft(sections, 'M_Paint')
    # the cab: front left/right on top of the bow, sloped windscreen
    p.loft([[(-1.25, -3.6, 1.75), (1.25, -3.6, 1.75), (1.2, -3.15, 2.55), (-1.2, -3.15, 2.55)],
            [(-1.25, -1.9, 1.8), (1.25, -1.9, 1.8), (1.25, -1.9, 2.6), (-1.25, -1.9, 2.6)]], 'M_Paint')
    for x in (-0.62, 0.62):  # windscreen panes on the slope
        a = math.atan2(0.45, 0.8)
        p.box((x, -3.38, 2.17), (1.0, 0.03, 0.62), 'M_Glass', rot_x=-a)
    for x in (-1.26, 1.26):
        p.box((x, -2.55, 2.2), (0.03, 0.8, 0.45), 'M_Glass')
    # the deck behind the cab, with the containers: six across, two high, along the vehicle (missile container ~3.3 m)
    p.box((0.0, 1.15, 1.85), (2.6, 5.2, 0.1), 'M_Dark')
    cw = 0.40
    for row in range(2):
        for i in range(6):
            x = (i - 2.5) * (cw + 0.03)
            p.box((x, 0.6, 2.12 + row * (cw + 0.03)), (cw, 3.4, cw), 'M_Container')
    # cradle frames at the container ends
    for y in (-1.1, 2.3):
        p.box((0.0, y, 2.3), (2.75, 0.12, 0.95), 'M_Metal')
    # the crane: a post at the rear right, the boom stowed along the top of the load, the hook block at its tip
    p.cylinder((1.15, 3.75, 2.5), 0.17, 1.3, 'Z', 'M_Metal', 16)
    p.box((1.15, 3.75, 3.2), (0.5, 0.5, 0.25), 'M_Metal')
    p.box((0.45, 1.5, 3.25), (0.22, 4.6, 0.22), 'M_Metal', rot_z=math.atan2(0.7, 4.5))
    p.box((-0.25, -0.8, 3.05), (0.16, 0.16, 0.3), 'M_Dark')
    # rear deck items and the water-jet housing
    p.box((0.0, 4.5, 1.0), (1.2, 0.2, 0.5), 'M_Dark')
    p.box((-0.9, 4.0, 1.95), (0.6, 0.6, 0.25), 'M_Dark')
    # wheels: three axles, 1.3 m tyres
    for y in (-2.6, 0.55, 3.35):
        for x in (-1.2, 1.2):
            p.wheel(x, y, 0.66, 0.5)
    return p.finish('SM_TZM_9T217')


# ------------------------------------------------------------------------------------------------ ATZ-3.8-131

def atz():
    """ATZ-3.8-131: the 3.8 m3 tanker on the ZIL-131 6x6 (about 7.2 m long, 2.5 m wide, 2.9 m over the tank): the long
    bonnet and cab in front, an oval tank on the frame, the pump and hose cabinet at the rear, one front and two rear
    axles."""
    p = Part()
    # frame
    p.box((0.0, 0.3, 0.95), (0.9, 6.6, 0.25), 'M_Dark')
    # bonnet and wings
    p.loft([[(-0.55, -3.6, 0.95), (0.55, -3.6, 0.95), (0.6, -3.55, 1.6), (-0.6, -3.55, 1.6)],
            [(-0.65, -2.2, 0.95), (0.65, -2.2, 0.95), (0.7, -2.2, 1.7), (-0.7, -2.2, 1.7)]], 'M_Paint')
    p.box((0.0, -3.62, 1.25), (1.0, 0.08, 0.55), 'M_Dark')  # grille
    for x in (-1.0, 1.0):
        p.loft([[(x - 0.28, -3.5, 1.05), (x + 0.28, -3.5, 1.05), (x + 0.28, -3.3, 1.35), (x - 0.28, -3.3, 1.35)],
                [(x - 0.28, -2.2, 1.1), (x + 0.28, -2.2, 1.1), (x + 0.28, -2.2, 1.45), (x - 0.28, -2.2, 1.45)]], 'M_Paint')
    # cab
    p.box((0.0, -1.55, 1.85), (2.3, 1.3, 1.5), 'M_Paint')
    p.box((0.0, -2.21, 2.25), (2.0, 0.03, 0.6), 'M_Glass')
    for x in (-1.16, 1.16):
        p.box((x, -1.7, 2.25), (0.03, 0.7, 0.5), 'M_Glass')
    # the tank: an oval cylinder along the vehicle
    rings = []
    for y in (-0.6, -0.5, 3.4, 3.5):
        ring = []
        shrink = 0.92 if y in (-0.6, 3.5) else 1.0
        for i in range(28):
            a = 2.0 * math.pi * i / 28
            ring.append((1.15 * shrink * math.cos(a), y, 1.95 + 0.78 * shrink * math.sin(a)))
        rings.append(ring)
    p.loft(rings, 'M_Paint')
    p.cylinder((0.0, 1.4, 2.8), 0.28, 0.12, 'Z', 'M_Metal', 18)  # filler dome
    p.box((0.0, 1.4, 2.78), (0.3, 3.4, 0.06), 'M_Dark')  # walkway
    # rear pump and hose cabinet
    p.box((0.0, 3.85, 1.65), (2.3, 0.6, 1.2), 'M_Paint')
    p.box((0.0, 4.16, 1.65), (2.0, 0.03, 0.9), 'M_Dark')
    # wheels: front axle, tandem at the rear (1.12 m tyres)
    for y in (-2.85, 1.65, 3.0):
        for x in (-1.0, 1.0):
            p.wheel(x, y, 0.56, 0.38)
    return p.finish('SM_ATZ_38_131')


# ------------------------------------------------------------------------------------------------ PU-12M placeholder

# ------------------------------------------------------------------------------------------------ export and preview

def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                             axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True, mesh_smooth_type='FACE',
                             add_leaf_bones=False, bake_anim=False)
    return path


def render_preview(obj, folder):
    scene = bpy.context.scene
    for other in scene.objects:
        if other.type == 'MESH':
            other.hide_render = other is not obj and other.name != 'ground'
    for m in obj.data.materials:
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get('Principled BSDF')
        if bsdf is not None:
            bsdf.inputs['Base Color'].default_value = COLOURS[m.name.split('.')[0]]
            bsdf.inputs['Roughness'].default_value = 0.6
    cam = scene.camera
    target = Vector((0.0, 0.0, 1.2))
    cam.location = Vector((-9.5, -11.0, 6.0))
    cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = os.path.join(folder, obj.name + '.png')
    bpy.ops.render.render(write_still=True)


def setup_render():
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items.keys() else 'BLENDER_EEVEE'
    scene.render.resolution_x, scene.render.resolution_y = 960, 600
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    scene.collection.objects.link(cam)
    cam.data.lens = 40
    scene.camera = cam
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0.0, math.radians(-35))
    scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('w')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.6, 0.65, 1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.8
    scene.world = world
    ground = Part()
    ground.box((0.0, 0.0, -0.01), (30.0, 30.0, 0.02), 'M_Metal')
    g = ground.finish('ground')
    g.hide_render = False
    return g


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    preview = os.path.abspath(argv[argv.index('--preview') + 1]) if '--preview' in argv else None
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    objs = [tzm(), atz()]
    x = 0.0
    for o in objs:
        print(o.name, 'faces', len(o.data.polygons), 'size', tuple(round(d, 2) for d in o.dimensions))
        print('  ->', export(o))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'support_vehicles.blend'))
    if preview:
        os.makedirs(preview, exist_ok=True)
        ground = setup_render()
        for o in objs:
            render_preview(o, preview)
            ground.hide_render = False
        print('previews in', preview)


if __name__ == '__main__':
    main()
