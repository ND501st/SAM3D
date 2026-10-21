"""Build the Osa-AK fighting compartment in Blender from code and export it for Unreal.

The cabin's consoles (panels, blocks, controls, lamps, screens) are built by the game from data/cabins/osa_ak.json.
This script builds everything around them, as meshes: the hull lining (floor, walls, roof with the crew hatches,
windscreens, rear bulkhead with its door), the racks' carcasses behind every console panel, the operators' bench
and seats, the cab seats, the steering wheel, the dashboard, and the equipment the Polish manual's plan places in
the compartment (two R-123M radios, the TA-57 telephone, spare-parts boxes, the GA-3 gyro half-compass...).

Sources, in the body frame of the layout file (metres; x right, y forward, z up; origin on the ground under the
reference point): mon_2112_80 Fig. 3 (plan and wall views of the 9A33BM2), scaled from rack SŁ-2M = 1.89 m at
4.785 mm per pixel; heights from its sections A-A and B-B (the racks reach the roof); the console and rack faces
from the layout file itself; looks from the cabin photos (cabin_photos, usaf_shaw_2017). Anything the sources don't
give is marked "estimate" next to its number.

Every mesh is exported with its origin at the centre of its bounding box, in the cabin's frame turned so that
Unreal's FBX import lands it in the vehicle's frame (Blender +X = forward, +Y = left, +Z = up): a shell part of kind
"mesh" in the layout file places it with "centre_m" (that centre, body frame) and "size_m" (the box, body axes).
Meshes built in place (the hull, the racks) list their centre in art/cabin_interior/manifest.json; reusable ones
(seats, radios...) are built around their own centre, and the layout file places each copy.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_cabin_interior.py
    ... -- --preview out/interior_preview      # also render a few views (PNG) of the whole compartment

Writes art/cabin_interior/<Mesh>.fbx, cabin_interior.blend and manifest.json (Git LFS for the binaries).
Material slots are named after the cabin's Unreal material instances (MI_Cabin_<name>, tools/ue_make_cabin_materials.py).
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, 'art', 'cabin_interior')
LAYOUT = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
COLLECTION = 'OsaInterior'

# ---------------------------------------------------------------------------------------------- the plan (body frame)
# mon_2112_80 Fig. 3 at 4.785 mm per pixel unless noted.
FLOOR_Z = 0.62          # floor top (layout file); the plan gives no floor level
ROOF_Z = 2.10           # roof lining; the racks reach it (sections A-A, B-B)
WALL_X = 1.32           # side walls at rack height: -1.333 left, +1.306 right in the drawing, made symmetric
REAR_Y = -0.936         # rear bulkhead (door to the engine compartment)
CAB_Y = 4.06            # the side walls turn in towards the bow from here (front post of SŁ-1M / SP-1M)
BOW_Y = 4.90            # footwell front under the dashboard (plan front line 5.04 at floor level, less the bow lining)
BOW_X = 1.14            # half-width at the bow (plan: 2.27 m across at the front line)
DASH_TOP_Z = 1.62       # dashboard top, under the windscreens (estimate, from rwd_mb3 9a33_31/32)
SCREEN_BOTTOM_Y = 4.80  # windscreen panel: bottom edge (y, z = DASH_TOP_Z) to top edge (y, z = ROOF_Z); the
SCREEN_TOP_Y = 4.55     # lean (28 degrees from vertical) is an estimate from the exterior photos (opisybroni)
THICK = 0.12            # hull lining and plate (estimate), outwards from the faces given; Lumen's distance fields
                        # need walls this thick to keep the sky out


def window_x(sign):
    """Centre of a windscreen across the vehicle: two screens either side of a 0.16 m pillar (opisybroni exterior)."""
    return sign * 0.54


# ---------------------------------------------------------------------------------------------- geometry helpers

def P(x, y, z):
    """Body frame (x right, y forward, z up) to Blender (X forward, Y left, Z up): Unreal's FBX import then gives
    X forward, Y right, Z up in centimetres, the cabin component's frame."""
    return Vector((y, -x, z))


def body(v):
    return (-v.y, v.x, v.z)


class Builder:
    """Collects bmesh geometry for one mesh, in body-frame coordinates, with named materials."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.materials = []

    def mat(self, name):
        if name not in self.materials:
            self.materials.append(name)
        return self.materials.index(name)

    def face(self, pts, mat, smooth=False):
        verts = [self.bm.verts.new(P(*p)) for p in pts]
        f = self.bm.faces.new(verts)
        f.material_index = self.mat(mat)
        f.smooth = smooth
        return f

    # -- solids -------------------------------------------------------------------------------------------------
    def box(self, x0, x1, y0, y1, z0, z1, mat):
        if x1 < x0:
            x0, x1 = x1, x0
        if y1 < y0:
            y0, y1 = y1, y0
        if z1 < z0:
            z0, z1 = z1, z0
        c = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
        v = [self.bm.verts.new(P(*p)) for p in c]
        idx = self.mat(mat)
        # bottom, top, front(-y), back(+y), left(-x), right(+x) with outward winding in body frame (right-handed);
        # Blender's frame here is a rotation of the body frame, so the winding stays outward.
        for q in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
            f = self.bm.faces.new([v[i] for i in q])
            f.material_index = idx
        return v

    def obox(self, centre, u, v, n, size, mat):
        """An oriented box: centre (body), unit axes u, v, n (body) and full sizes along them."""
        hu, hv, hn = size[0] / 2, size[1] / 2, size[2] / 2
        c = Vector(centre)
        pts = []
        for k in (-hn, hn):
            for j in (-hv, hv):
                for i in (-hu, hu):
                    pts.append(c + u * i + v * j + n * k)
        verts = [self.bm.verts.new(P(*p)) for p in pts]
        idx = self.mat(mat)
        for q in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
            f = self.bm.faces.new([verts[i] for i in q])
            f.material_index = idx
        return verts

    def slab(self, origin, u, v, outer, holes, t, mat, side_mat=None, back=True):
        """A flat plate in the plane origin + a*u + b*v (body), from outline `outer` [(a, b)...] with `holes`, t thick
        towards -n (n = u x v). Faces wound so the front looks along +n."""
        o, u, v = Vector(origin), Vector(u), Vector(v)
        n = u.cross(v).normalized()
        idx = self.mat(mat)
        sidx = self.mat(side_mat or mat)
        loops = [outer] + list(holes)

        def ring(pts, off):
            return [self.bm.verts.new(P(*(o + u * a + v * b + n * off))) for a, b in pts]

        front = [ring(lp, 0.0) for lp in loops]
        rear = [ring(lp, -t) for lp in loops]
        for layer, flip in ((front, False), (rear, True)):
            if layer is rear and not back:
                continue
            edges = []
            for lp in layer:
                for i in range(len(lp)):
                    edges.append(self.bm.edges.new((lp[i], lp[(i + 1) % len(lp)])))
            res = bmesh.ops.triangle_fill(self.bm, use_beauty=True, use_dissolve=False, edges=edges,
                                          normal=P(*(-n if flip else n)) - P(0, 0, 0))
            for f in res['geom']:
                if isinstance(f, bmesh.types.BMFace):
                    f.material_index = idx
        for li, (fl, rl) in enumerate(zip(front, rear)):
            m = len(fl)
            for i in range(m):
                a, b = fl[i], fl[(i + 1) % m]
                c, d = rl[(i + 1) % m], rl[i]
                # outer loop: side faces look outwards; holes: inwards (the loops all run the same way)
                quad = (a, d, c, b)
                f = self.bm.faces.new(quad)
                f.material_index = sidx

    def prism(self, section, x0, x1, mat):
        """Extrude a closed section [(y, z)...] (body) along x from x0 to x1."""
        a = [self.bm.verts.new(P(x0, y, z)) for y, z in section]
        b = [self.bm.verts.new(P(x1, y, z)) for y, z in section]
        idx = self.mat(mat)
        n = len(section)
        f0 = self.bm.faces.new(list(reversed(a)))
        f1 = self.bm.faces.new(b)
        for f in (f0, f1):
            f.material_index = idx
        for i in range(n):
            f = self.bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
            f.material_index = idx

    def cylinder(self, p0, p1, r, mat, seg=24, caps=True, smooth=True, r1=None):
        """A cylinder (or cone with r1) between body points p0 and p1."""
        p0, p1 = Vector(p0), Vector(p1)
        axis = (p1 - p0)
        length = axis.length
        axis.normalize()
        ref = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
        e1 = axis.cross(ref).normalized()
        e2 = axis.cross(e1).normalized()
        rr = r1 if r1 is not None else r
        ring0, ring1 = [], []
        for k in range(seg):
            ang = 2 * math.pi * k / seg
            d = e1 * math.cos(ang) + e2 * math.sin(ang)
            ring0.append(self.bm.verts.new(P(*(p0 + d * r))))
            ring1.append(self.bm.verts.new(P(*(p1 + d * rr))))
        idx = self.mat(mat)
        for k in range(seg):
            f = self.bm.faces.new((ring0[k], ring0[(k + 1) % seg], ring1[(k + 1) % seg], ring1[k]))
            f.material_index = idx
            f.smooth = smooth
        if caps:
            f = self.bm.faces.new(list(reversed(ring0)))
            f.material_index = idx
            f = self.bm.faces.new(ring1)
            f.material_index = idx
        return length

    def lathe(self, centre, axis, profile, mat, seg=32, smooth=True):
        """Revolve [(r, h)...] around the body axis through centre (h along the axis)."""
        c, axis = Vector(centre), Vector(axis).normalized()
        ref = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
        e1 = axis.cross(ref).normalized()
        e2 = axis.cross(e1).normalized()
        rows = []
        for r, h in profile:
            row = []
            for k in range(seg):
                ang = 2 * math.pi * k / seg
                row.append(self.bm.verts.new(P(*(c + axis * h + (e1 * math.cos(ang) + e2 * math.sin(ang)) * r))))
            rows.append(row)
        idx = self.mat(mat)
        for i in range(len(rows) - 1):
            for k in range(seg):
                f = self.bm.faces.new((rows[i][k], rows[i][(k + 1) % seg], rows[i + 1][(k + 1) % seg], rows[i + 1][k]))
                f.material_index = idx
                f.smooth = smooth
        for row, (r, h), flip in ((rows[0], profile[0], True), (rows[-1], profile[-1], False)):
            if r > 1e-6:
                f = self.bm.faces.new(list(reversed(row)) if flip else row)
                f.material_index = idx

    def torus(self, centre, axis, R, r, mat, seg=48, rseg=12):
        c, axis = Vector(centre), Vector(axis).normalized()
        ref = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
        e1 = axis.cross(ref).normalized()
        e2 = axis.cross(e1).normalized()
        rows = []
        for k in range(seg):
            a = 2 * math.pi * k / seg
            d = e1 * math.cos(a) + e2 * math.sin(a)
            row = []
            for j in range(rseg):
                b = 2 * math.pi * j / rseg
                row.append(self.bm.verts.new(P(*(c + d * (R + r * math.cos(b)) + axis * (r * math.sin(b))))))
            rows.append(row)
        idx = self.mat(mat)
        for k in range(seg):
            for j in range(rseg):
                f = self.bm.faces.new((rows[k][j], rows[(k + 1) % seg][j], rows[(k + 1) % seg][(j + 1) % rseg],
                                       rows[k][(j + 1) % rseg]))
                f.material_index = idx
                f.smooth = True

    def tube(self, points, r, mat, seg=16):
        """A tube along a polyline of body points (bent frames, hoses), with mitred joints and capped ends."""
        pts = [Vector(p) for p in points]
        rings = []
        for i, p in enumerate(pts):
            if i == 0:
                t = (pts[1] - p).normalized()
            elif i == len(pts) - 1:
                t = (p - pts[i - 1]).normalized()
            else:
                t = ((p - pts[i - 1]).normalized() + (pts[i + 1] - p).normalized()).normalized()
            ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            e1 = t.cross(ref).normalized()
            e2 = t.cross(e1).normalized()
            rings.append([self.bm.verts.new(P(*(p + (e1 * math.cos(2 * math.pi * k / seg) +
                                                     e2 * math.sin(2 * math.pi * k / seg)) * r))) for k in range(seg)])
        idx = self.mat(mat)
        for a, b in zip(rings, rings[1:]):
            for k in range(seg):
                f = self.bm.faces.new((a[k], a[(k + 1) % seg], b[(k + 1) % seg], b[k]))
                f.material_index = idx
                f.smooth = True
        for ring, flip in ((rings[0], True), (rings[-1], False)):
            f = self.bm.faces.new(list(reversed(ring)) if flip else ring)
            f.material_index = idx

    def rounded_box(self, centre, size, radius, mat, u=Vector((1, 0, 0)), v=Vector((0, 1, 0)), seg=4):
        """A cushion: a box (size along u, v, vertical) with its corners rounded in the u-v plane."""
        c = Vector(centre)
        w, d, h = size
        outline = rrect(0, 0, w, d, radius, seg)
        n = u.cross(v).normalized()
        self.slab(c + n * (h / 2), u, v, outline, [], h, mat)


def rrect(cx, cy, w, h, r, seg=6):
    """A rounded rectangle's outline, anticlockwise from the bottom-right corner arc."""
    r = min(r, w / 2 - 1e-4, h / 2 - 1e-4)
    pts = []
    corners = ((cx + w / 2 - r, cy - h / 2 + r, -90), (cx + w / 2 - r, cy + h / 2 - r, 0),
               (cx - w / 2 + r, cy + h / 2 - r, 90), (cx - w / 2 + r, cy - h / 2 + r, 180))
    for ox, oy, a0 in corners:
        for k in range(seg + 1):
            a = math.radians(a0 + 90 * k / seg)
            pts.append((ox + r * math.cos(a), oy + r * math.sin(a)))
    return pts


def circle(cx, cy, r, seg=32):
    return [(cx + r * math.cos(2 * math.pi * k / seg), cy + r * math.sin(2 * math.pi * k / seg)) for k in range(seg)]


# ---------------------------------------------------------------------------------------------- materials

# Viewport colours for the preview only; Unreal assigns MI_Cabin_<name> by slot name.
PREVIEW = {
    'Paint_Interior': (0.62, 0.64, 0.6), 'Paint_Rack': (0.45, 0.47, 0.47), 'Paint_Bench': (0.16, 0.19, 0.15),
    'Paint_Equipment': (0.2, 0.24, 0.18), 'Paint_Radio': (0.33, 0.35, 0.36), 'Floor_Steel': (0.25, 0.25, 0.24),
    'Rubber_Mat': (0.06, 0.06, 0.06), 'Rubber_Black': (0.03, 0.03, 0.03), 'Vinyl_Olive': (0.16, 0.18, 0.08),
    'Vinyl_Brown': (0.22, 0.09, 0.05), 'Metal_Steel': (0.55, 0.56, 0.57), 'Metal_Dark': (0.12, 0.12, 0.13),
    'Bakelite_Black': (0.02, 0.02, 0.02), 'Glass': (0.2, 0.25, 0.27), 'Lamp_Glass': (1.0, 0.9, 0.7),
    'Lamp_Blue': (0.1, 0.3, 0.6), 'Duct_Grey': (0.35, 0.36, 0.36), 'Grate_Rubber': (0.18, 0.1, 0.06),
    'Paint_Panel': (0.72, 0.71, 0.64), 'Canvas': (0.35, 0.33, 0.22),
    'Paint_Letters': (0.8, 0.78, 0.7), 'Paint_Red': (0.62, 0.13, 0.05), 'Metal_Brass': (0.62, 0.48, 0.24),
    'Paint_LightGrey': (0.6, 0.61, 0.62), 'Lens_Amber': (0.75, 0.38, 0.06),
}


def material(name):
    """The material slot `name`, called after the Unreal material instance that fills it (MI_Cabin_<name>), so the
    import binds the slot to it (tools/ue_import_cabin_interior.py)."""
    full = 'MI_Cabin_' + name
    m = bpy.data.materials.get(full)
    if m is None:
        m = bpy.data.materials.new(full)
        m.diffuse_color = (*PREVIEW.get(name, (0.5, 0.5, 0.5)), 1.0)
        if bpy.app.version < (5, 0, 0):
            m.use_nodes = True          # always on from Blender 5
        bsdf = m.node_tree.nodes.get('Principled BSDF') if m.node_tree else None
        if bsdf:
            bsdf.inputs['Base Color'].default_value = (*PREVIEW.get(name, (0.5, 0.5, 0.5)), 1.0)
            bsdf.inputs['Roughness'].default_value = 0.6
            if name.startswith('Metal'):
                bsdf.inputs['Metallic'].default_value = 1.0
                bsdf.inputs['Roughness'].default_value = 0.35
            if name.startswith('Lamp'):
                bsdf.inputs['Emission Color'].default_value = (*PREVIEW[name], 1.0)
                bsdf.inputs['Emission Strength'].default_value = 3.0
    return m


# ---------------------------------------------------------------------------------------------- finishing

def finish(b, bevel=0.0, sharp_deg=35.0, reuse=False):
    """Turn a Builder into an object: UVs (box projection, 1 unit = 1 m), smoothing, an optional edge bevel, origin at
    the centre of its bounding box. Returns the object; its body-frame centre and size go in the manifest."""
    bm = b.bm
    # every part is a closed shell (no welding: two solids that touch would share a four-face edge, which upsets the
    # distance-field builder); face normals outwards
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # n-gons (caps, filled plates) to triangles, so the FBX carries tangents and Unreal gets plain polygons
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bm.normal_update()
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for loop in f.loops:
            co = loop.vert.co
            if ax == 0:      # faces looking forward/back: (left, up)
                loop[uv].uv = (co.y if n.x > 0 else -co.y, co.z)
            elif ax == 1:    # faces looking left/right
                loop[uv].uv = (-co.x if n.y > 0 else co.x, co.z)
            else:            # floors and ceilings
                loop[uv].uv = (co.x, co.y)
    mesh = bpy.data.meshes.new(b.name)
    bm.to_mesh(mesh)
    bm.free()
    for name in b.materials:
        mesh.materials.append(material(name))
    obj = bpy.data.objects.new(b.name, mesh)
    bpy.data.collections[COLLECTION].objects.link(obj)
    # origin at the bounding box centre
    xs = [v.co for v in mesh.vertices]
    lo = Vector((min(c.x for c in xs), min(c.y for c in xs), min(c.z for c in xs)))
    hi = Vector((max(c.x for c in xs), max(c.y for c in xs), max(c.z for c in xs)))
    centre = (lo + hi) / 2
    mesh.transform(Matrix.Translation(-centre))
    obj.location = centre
    mesh.shade_smooth()
    mesh.set_sharp_from_angle(angle=math.radians(sharp_deg))
    if bevel > 0:
        mod = obj.modifiers.new('bevel', 'BEVEL')
        mod.width = bevel
        mod.segments = 2
        mod.limit_method = 'ANGLE'
        mod.angle_limit = math.radians(sharp_deg)
        mod.harden_normals = True
        tri = obj.modifiers.new('ngons', 'TRIANGULATE')
        tri.min_vertices = 5
    size = hi - lo
    obj['body_centre'] = body(centre)
    obj['body_size'] = (size.y, size.x, size.z)
    obj['reuse'] = reuse
    return obj


# ---------------------------------------------------------------------------------------------- the layout

def load_layout():
    with open(LAYOUT, encoding='utf-8') as f:
        return json.load(f)


def panel_frame(p):
    """A layout panel's centre and unit axes (normal, right, up) in the body frame (simcore::CabinPanel)."""
    f = math.radians(p['facing_deg'])
    t = math.radians(p.get('tilt_deg', 0.0))
    h = Vector((math.sin(f), math.cos(f), 0.0))
    n = h * math.cos(t) + Vector((0, 0, 1)) * math.sin(t)
    r = Vector((-math.cos(f), math.sin(f), 0.0))
    u = n.cross(r)
    return Vector(p['centre_m']), n, r, u


# ---------------------------------------------------------------------------------------------- the hull

def build_hull():
    b = Builder('SM_OsaHull')
    W, T = WALL_X, THICK
    # Floor: steel plates the full length, under the racks too.
    floor = [(-W, REAR_Y), (W, REAR_Y), (W, CAB_Y), (BOW_X, BOW_Y), (-BOW_X, BOW_Y), (-W, CAB_Y)]
    b.slab((0, 0, FLOOR_Z), Vector((1, 0, 0)), Vector((0, 1, 0)),
           [(x + (T if x > 0 else -T), y + (T if y > 1 else -T)) for x, y in floor], [], T, 'Floor_Steel')
    # Roof lining with the two crew hatches above the cab seats (positions and size: estimate; usaf_shaw_2017 -036
    # shows a rounded rectangular hatch about 0.55 x 0.6 m).
    # The roof is built of closed boxes round the two hatch openings: Unreal's distance-field builder tells inside
    # from outside by counting crossings, and a holed plate with a stray open edge vanished from Lumen's view
    # (daylight then poured in through it).
    front_x = W - (W - BOW_X) * (SCREEN_TOP_Y - CAB_Y) / (BOW_Y - CAB_Y) + T
    hx, hy = 0.55 / 2, 0.60 / 2
    y0, y1 = HATCH_Y - hy, HATCH_Y + hy
    z0, z1 = ROOF_Z, ROOF_Z + T
    b.box(-W - T, W + T, REAR_Y - T, y0, z0, z1, 'Paint_Interior')                    # behind the hatches
    b.prism([(y1, z0), (CAB_Y, z0), (SCREEN_TOP_Y + T, z0), (SCREEN_TOP_Y + T, z1), (y1, z1)],
            -front_x, front_x, 'Paint_Interior')                                       # ahead of them, to the screens
    b.box(-W - T, -front_x, CAB_Y - 0.01, SCREEN_TOP_Y + T, z0, z1, 'Paint_Interior')  # over the front corners
    b.box(front_x, W + T, CAB_Y - 0.01, SCREEN_TOP_Y + T, z0, z1, 'Paint_Interior')
    if y1 < CAB_Y:
        b.box(-W - T, W + T, y1, CAB_Y, z0, z1, 'Paint_Interior')
    xs = sorted([-0.36 - hx, -0.36 + hx, 0.33 - hx, 0.33 + hx])
    for a, c in ((-W - T, xs[0]), (xs[1], xs[2]), (xs[3], W + T)):                 # beside and between them
        b.box(a, c, y0 - 0.001, y1 + 0.001, z0, z1, 'Paint_Interior')
    for x in (-0.36, 0.33):
        build_hatch(b, x, HATCH_Y)
    # Every wall below is a slab whose face, the plane given, is the inside of the lining (u x v looks into the
    # cabin) and whose thickness goes outwards; the walls run past each other at the corners so the cabin is closed
    # for Lumen's sky light (a gap would let daylight in).
    up_z = Vector((0, 0, 1))
    tall = ROOF_Z - FLOOR_Z + T
    # Side walls, rear part (behind the racks for most of their length).
    for s in (-1, 1):
        x = s * W
        length = CAB_Y - REAR_Y + T
        if s > 0:     # u backwards: u x z looks to -x, into the cabin
            b.slab((x, CAB_Y, FLOOR_Z - T), Vector((0, -1, 0)), up_z, [(0, 0), (length, 0), (length, tall), (0, tall)],
                   [], T, 'Paint_Interior')
        else:         # u forwards: u x z looks to +x
            b.slab((x, REAR_Y - T, FLOOR_Z - T), Vector((0, 1, 0)), up_z,
                   [(0, 0), (length, 0), (length, tall), (0, tall)], [], T, 'Paint_Interior')
        # The turned-in front corner, with the side window (exterior photos: a window on each front corner;
        # size and place on the corner: estimate). The window sits 42 % of the way from the back of the corner.
        a0 = Vector((x, CAB_Y, FLOOR_Z))
        a1 = Vector((s * BOW_X, BOW_Y, FLOOR_Z))
        along = (a1 - a0)
        length = along.length
        along.normalize()
        uu, origin, at = (-along, a1, 0.58) if s > 0 else (along, a0, 0.42)
        win = rrect(length * at, 1.86 - FLOOR_Z, 0.44, 0.36, 0.06, 5)
        corner = [(-0.06, -T), (length + 0.06, -T), (length + 0.06, ROOF_Z - FLOOR_Z + T), (-0.06, ROOF_Z - FLOOR_Z + T)]
        b.slab(origin, uu, up_z, corner, [win], T, 'Paint_Interior')
        seal(b, origin, uu, up_z, win, 'Rubber_Black')
    # Rear bulkhead with the door to the engine compartment (plan: door opening at x 0 .. 0.59, hinged on the
    # centreline side; height: estimate). u = -x: u x z looks forwards, into the cabin.
    door = rrect(0.31, 0.56, 0.56, 1.06, 0.08, 5)
    rear = [(-W - T, -T), (-W - T, tall), (W + T, tall), (W + T, -T)]
    b.slab((0, REAR_Y, FLOOR_Z), Vector((-1, 0, 0)), up_z, [(-x, h) for x, h in rear],
           [[(-x, h) for x, h in reversed(door)]], T, 'Paint_Interior')
    build_door(b, door)
    # Dashboard across the cab: its rear face leans back like the instrument panels on it (20 degrees, layout
    # file), 3.5 cm behind their plane; the footwell runs under it to the bow. It runs into the corner walls.
    dash = [(BOW_Y, 1.15), (4.521, 1.15), (4.692, DASH_TOP_Z), (SCREEN_BOTTOM_Y + 0.02, DASH_TOP_Z),
            (BOW_Y, 1.40)]
    b.prism(dash, -WALL_X, WALL_X, 'Paint_Interior')
    # Bow lining below the dashboard (the footwell's front); u = +x: u x z looks backwards, into the cabin.
    b.slab((0, BOW_Y, FLOOR_Z), Vector((1, 0, 0)), up_z,
           [(-WALL_X, -T), (-WALL_X, 0.62), (WALL_X, 0.62), (WALL_X, -T)], [], T, 'Paint_Interior')
    # Windscreen panel with the two screens (opisybroni exterior: two wide screens with rounded corners);
    # u = +x, v up the screen: u x v looks back and down into the cab.
    o = Vector((0, SCREEN_BOTTOM_Y, DASH_TOP_Z))
    up = (Vector((0, SCREEN_TOP_Y, ROOF_Z)) - o)
    slant = up.length
    up.normalize()
    right = Vector((1, 0, 0))
    screens = [rrect(window_x(s), 0.255, 0.90, 0.36, 0.08, 6) for s in (-1, 1)]
    b.slab(o, right, up, [(-WALL_X, -0.05), (-WALL_X, slant + T), (WALL_X, slant + T), (WALL_X, -0.05)],
           screens, T, 'Paint_Interior')
    for sc in screens:
        seal(b, o, right, up, sc, 'Rubber_Black')
    # Floor mats in the cab (opisybroni forward view: ribbed mats under the cab seats' feet).
    for x in (-0.36, 0.33):
        b.box(x - 0.30, x + 0.30, 4.12, BOW_Y - 0.02, FLOOR_Z, FLOOR_Z + 0.008, 'Rubber_Mat')
    return finish(b, bevel=0.004)


HATCH_Y = 3.80


def build_hatch(b, x, y):
    """A closed roof hatch seen from below: the lid recessed in its opening, its seal, two hinges, the handle and the
    locking bar (shapes: estimate; usaf_shaw_2017 -036 shows the lid and its rounded opening)."""
    lid = rrect(x, -y, 0.53, 0.58, 0.06, 5)
    b.slab((0, 0, ROOF_Z + THICK + 0.01), Vector((1, 0, 0)), Vector((0, -1, 0)), list(reversed(lid)), [], 0.03,
           'Paint_Interior')
    b.tube([(x - 0.16, y - 0.05, ROOF_Z + THICK + 0.01), (x - 0.16, y - 0.05, ROOF_Z + THICK - 0.04),
            (x + 0.16, y - 0.05, ROOF_Z + THICK - 0.04), (x + 0.16, y - 0.05, ROOF_Z + THICK + 0.01)], 0.011,
           'Metal_Steel')
    b.box(x - 0.2, x + 0.2, y + 0.12, y + 0.14, ROOF_Z + THICK - 0.012, ROOF_Z + THICK + 0.01, 'Metal_Dark')
    for hx in (x - 0.18, x + 0.18):
        b.box(hx - 0.04, hx + 0.04, y + 0.27, y + 0.33, ROOF_Z + 0.03, ROOF_Z + THICK + 0.01, 'Metal_Dark')
    ring = rrect(x, -y, 0.55, 0.60, 0.07, 5)
    seal(b, Vector((0, 0, ROOF_Z + THICK + 0.012)), Vector((1, 0, 0)), Vector((0, -1, 0)), ring, 'Rubber_Black',
         width=0.018, depth=0.015)


def build_door(b, door):
    """The door to the engine compartment, closed in its opening: a plate, two hinges on the centreline side, the
    handle (estimate)."""
    inner = [(-x, h) for x, h in reversed(rrect(0.31, 0.56, 0.545, 1.045, 0.075, 5))]
    b.slab((0, REAR_Y - 0.03, FLOOR_Z), Vector((-1, 0, 0)), Vector((0, 0, 1)), inner, [], 0.03, 'Paint_Interior')
    for z in (FLOOR_Z + 0.25, FLOOR_Z + 0.85):
        b.cylinder((0.045, REAR_Y - 0.018, z - 0.06), (0.045, REAR_Y - 0.018, z + 0.06), 0.012, 'Metal_Dark')
    b.tube([(0.50, REAR_Y - 0.035, FLOOR_Z + 0.62), (0.50, REAR_Y + 0.03, FLOOR_Z + 0.62),
            (0.50, REAR_Y + 0.03, FLOOR_Z + 0.44), (0.50, REAR_Y - 0.035, FLOOR_Z + 0.44)], 0.011, 'Metal_Steel')


def seal(b, origin, u, v, outline, mat, width=0.022, depth=0.012):
    """A rubber seal round an opening: a ring just proud of the face, `width` wide."""
    o, u, v = Vector(origin), Vector(u), Vector(v)
    n = u.cross(v).normalized()
    cx = sum(p[0] for p in outline) / len(outline)
    cy = sum(p[1] for p in outline) / len(outline)
    outer = []
    for a, bb in outline:
        d = Vector((a - cx, bb - cy))
        ln = d.length
        k = (ln + width) / ln if ln > 1e-9 else 1.0
        outer.append((cx + (a - cx) * k, cy + (bb - cy) * k))
    b.slab(o + n * depth, u, v, outer, [list(reversed(outline))], depth + 0.002, mat)


def build_glass():
    b = Builder('SM_OsaGlass')
    o = Vector((0, SCREEN_BOTTOM_Y, DASH_TOP_Z))
    up = (Vector((0, SCREEN_TOP_Y, ROOF_Z)) - o).normalized()
    right = Vector((1, 0, 0))
    n = right.cross(up).normalized()          # into the cab; the panes sit halfway through the lining
    for s in (-1, 1):
        pane = rrect(window_x(s), 0.255, 0.92, 0.38, 0.09, 6)
        b.slab(o - n * (THICK * 0.5), right, up, pane, [], 0.008, 'Glass')
    for s in (-1, 1):
        a0 = Vector((s * WALL_X, CAB_Y, FLOOR_Z))
        a1 = Vector((s * BOW_X, BOW_Y, FLOOR_Z))
        along = (a1 - a0)
        length = along.length
        along.normalize()
        uu, origin, at = (-along, a1, 0.58) if s > 0 else (along, a0, 0.42)
        nn = uu.cross(Vector((0, 0, 1))).normalized()
        pane = rrect(length * at, 1.86 - FLOOR_Z, 0.46, 0.38, 0.07, 5)
        b.slab(origin - nn * (THICK * 0.5), uu, Vector((0, 0, 1)), pane, [], 0.008, 'Glass')
    return finish(b)


# ---------------------------------------------------------------------------------------------- the racks

def build_rack(name, layout, panel_ids, side, posts, lower, extra=None):
    """A rack's carcass behind its console panels: the cabinet back to the side wall, the frame round the panels
    (posts, top and bottom rails, column dividers), and below the blocks either a recessed kick panel with a foot
    grating (lower='kick') or closed doors flush with the frame (lower='doors')."""
    b = Builder(name)
    panels = [p for p in layout['panels'] if p['id'] in panel_ids]
    wall = side * WALL_X
    face = max((panel_frame(p)[0].x for p in panels), key=lambda v: -side * v)  # the face nearest the aisle
    n_x = -side                                    # the face looks across the aisle
    y0 = min(p['centre_m'][1] - p['size_m'][0] / 2 for p in panels)
    y1 = max(p['centre_m'][1] + p['size_m'][0] / 2 for p in panels)
    zb = min(p['centre_m'][2] - p['size_m'][1] / 2 for p in panels)
    zt = max(p['centre_m'][2] + p['size_m'][1] / 2 for p in panels)
    ylo = min([y0] + [a for a, _ in posts])
    yhi = max([y1] + [c for _, c in posts])

    def X(d):          # depth d (0 at the panel plane, negative behind it) to body x
        return face + n_x * d

    # the cabinet behind the panels (its front 3.5 cm behind the panel plane, where the game draws the panels'
    # dark backing)
    b.box(X(-0.035), wall, ylo, yhi, zb - 0.03, ROOF_Z, 'Paint_Rack')
    # frame: top rail, posts, bottom rail, dividers between columns, 4 mm behind the plates' faces
    fr0, fr1 = X(-0.035), X(-0.004)
    b.box(fr0, fr1, y0, y1, zt, ROOF_Z, 'Paint_Rack')
    for a, c in posts:
        b.box(fr0, fr1, a, c, zb - 0.03, ROOF_Z, 'Paint_Rack')
    b.box(fr0, fr1, ylo, yhi, zb - 0.03, zb, 'Paint_Rack')
    ys = sorted(p['centre_m'][1] - p['size_m'][0] / 2 for p in panels)[1:]
    for yy in ys:
        b.box(fr0, X(-0.002), yy - 0.006, yy + 0.006, zb, zt, 'Paint_Rack')
    # hinge plates on the end posts: the racks swing out on hinges (opisybroni 54334 shows three grey plates on the
    # console's side post); two per end here, at a quarter and three quarters of the panel height (positions: estimate)
    for yy in (ylo + 0.02, yhi - 0.02):
        for f in (0.2, 0.8):
            zz = zb + f * (zt - zb)
            b.box(fr0, X(0.006), yy - 0.03, yy + 0.03, zz - 0.04, zz + 0.04, 'Metal_Steel')
            b.cylinder((X(0.006), yy, zz - 0.045), (X(0.006), yy, zz + 0.045), 0.007, 'Metal_Dark', seg=10)
    if lower == 'kick':
        # knee room under the console: the kick panel 13 cm back, a plinth, the foot grating out to the bench
        b.box(X(-0.13), wall, ylo, yhi, FLOOR_Z, zb - 0.03, 'Paint_Rack')
        b.box(X(-0.10), X(-0.13), ylo, yhi, FLOOR_Z, FLOOR_Z + 0.07, 'Paint_Rack')
        bench_edge = -0.541 if side < 0 else 0.541
        d = -0.12
        while True:
            xx = X(d)
            if (xx - bench_edge) * n_x >= -0.01:
                break
            b.box(xx, X(d + 0.012), y0 + 0.02, y1 - 0.02, FLOOR_Z, FLOOR_Z + 0.022, 'Grate_Rubber')
            d += 0.028
        for yy in (y0 + 0.01, y1 - 0.01, (y0 + y1) / 2):
            b.box(X(-0.12), X(d), yy - 0.008, yy + 0.008, FLOOR_Z, FLOOR_Z + 0.016, 'Metal_Dark')
    else:
        # closed lower cabinet: door panels between the posts, a plinth at the floor
        b.box(X(-0.035), wall, ylo, yhi, FLOOR_Z, zb - 0.03, 'Paint_Rack')
        b.box(X(-0.06), X(-0.035), ylo, yhi, FLOOR_Z, FLOOR_Z + 0.06, 'Paint_Rack')
        doors = lower if isinstance(lower, list) else [(y0, y1)]
        for a, c in doors:
            b.box(X(-0.035), X(-0.012), a + 0.012, c - 0.012, FLOOR_Z + 0.07, zb - 0.045, 'Paint_Rack')
            for yy in (a + 0.06, c - 0.06):
                b.tube([(X(-0.012), yy, zb - 0.12), (X(0.02), yy, zb - 0.12), (X(0.02), yy, zb - 0.24),
                        (X(-0.012), yy, zb - 0.24)], 0.006, 'Metal_Steel', seg=10)
    if extra:
        extra(b, X)
    return finish(b, bevel=0.003)


def sp1m_extra(b, X):
    """Spare-parts box ZCzZ1A nr 2 under SP-1M (section A-A: 0.19 m high, about 0.1 m below the blocks), and the base
    frame of the right R-123M in front of the rack (plan position; height: estimate)."""
    b.box(X(-0.03), X(0.005), 2.72, 3.39, 1.18, 1.37, 'Paint_Equipment')
    for yy in (2.85, 3.26):
        b.box(X(0.005), X(0.02), yy - 0.035, yy + 0.035, 1.30, 1.33, 'Metal_Steel')
    b.box(X(0.037), X(0.227), 2.97, 3.46, FLOOR_Z, FLOOR_Z + 0.06, 'Metal_Dark')


def sl1m_extra(b, X):
    """The shelf the left R-123M stands on, on SŁ-1M's lower doors, with two braces (plan position by SŁ-1M;
    height and mounting: estimate)."""
    b.box(X(-0.035), X(0.23), 3.02, 3.54, 1.098, 1.118, 'Paint_Rack')
    up_out = Vector((X(1.0) - X(0.0), 0.0, 1.0)).normalized()   # rising towards the aisle at 45 degrees
    for yy in (3.06, 3.50):
        b.obox(Vector((X(0.10), yy, 1.02)), Vector((0, 1, 0)), up_out, Vector((0, 1, 0)).cross(up_out),
               (0.02, 0.24, 0.006), 'Paint_Rack')


# ---------------------------------------------------------------------------------------------- seats and bench

BENCH = (-0.541, -0.144, 0.682, 2.424)   # plan: x across, y along (mon_2112_80 Fig. 3)
SEAT_Z = 1.05                            # cushion top (estimate: eye height 1.83 less a seated eye height of 0.78)


def build_bench():
    b = Builder('SM_OperatorBench')
    x0, x1, y0, y1 = BENCH
    base = SEAT_Z - 0.075
    b.box(x0, x1, y0, y1, FLOOR_Z, base, 'Paint_Bench')
    # the top's raised lip (usaf_shaw_2017 -019)
    b.box(x0 - 0.008, x1 + 0.008, y0 - 0.008, y1 + 0.008, base - 0.03, base + 0.006, 'Paint_Bench')
    # a recessed panel in the bench front, under each seat (stowage), with a pull
    for yc in (0.91, 1.555, 2.2):
        b.box(x0 - 0.004, x0 + 0.01, yc - 0.22, yc + 0.22, FLOOR_Z + 0.08, base - 0.06, 'Paint_Bench')
        b.box(x0 - 0.016, x0 - 0.004, yc - 0.05, yc + 0.05, base - 0.13, base - 0.11, 'Metal_Steel')
    return finish(b, bevel=0.004)


def build_operator_seat():
    """One operator's place on the bench, built facing forward (yaw -90 faces the console): a cushion with a rounded
    front, and a padded back on a bent tube frame at the aisle edge (usaf_shaw_2017 -019: olive back, worn brown
    cushion, tubular frame; mon_2112_80 Fig. 3: rounded-front seats)."""
    b = Builder('SM_OperatorSeat')
    # local frame: x across the seat (right), y forward (towards the console), z up; origin under the seat centre
    w, d = 0.48, 0.38
    outline = [(-w / 2, -d / 2), (w / 2, -d / 2)]
    for k in range(13):
        a = math.radians(-30 + 240 * k / 12)    # a flattened arc for the front edge
    front = [(w / 2 * math.cos(math.radians(a)), d / 2 - 0.12 + 0.12 * math.sin(math.radians(a)))
             for a in [0 + 15 * k for k in range(13)]]
    outline = [(-w / 2, -d / 2), (w / 2, -d / 2)] + front
    b.slab(Vector((0, 0, 0.075)), Vector((1, 0, 0)), Vector((0, 1, 0)), outline, [], 0.075, 'Vinyl_Brown')
    # back frame and pad, at the rear edge (the aisle)
    for x in (-0.19, 0.19):
        b.tube([(x, -d / 2 + 0.02, 0.0), (x, -d / 2 - 0.02, 0.10), (x, -d / 2 - 0.05, 0.52)], 0.012, 'Metal_Steel')
    b.tube([(-0.19, -d / 2 - 0.05, 0.52), (0.19, -d / 2 - 0.05, 0.52)], 0.012, 'Metal_Steel')
    lean = math.radians(8)
    v = Vector((0, -math.sin(lean), math.cos(lean)))
    b.rounded_box(Vector((0, -d / 2 - 0.02, 0.33)), (0.40, 0.34, 0.055), 0.05, 'Vinyl_Olive',
                  u=Vector((1, 0, 0)), v=v, seg=4)
    return finish(b, reuse=True)


def build_cab_seat():
    """Driver's and commander's seat: cushion and back in olive vinyl on a tube frame, on a pedestal (rwd_mb3
    9a33_13: olive cushion; opisybroni forward view: high backs, tube frames). Built facing forward."""
    b = Builder('SM_CabSeat')
    b.box(-0.17, 0.17, -0.17, 0.15, -SEAT_Z + FLOOR_Z, -0.12, 'Paint_Interior')   # pedestal (estimate)
    b.box(-0.21, 0.21, -0.20, 0.20, -0.12, -0.09, 'Metal_Dark')                  # seat pan
    b.rounded_box(Vector((0, 0.0, -0.09)), (0.46, 0.44, 0.09), 0.06, 'Vinyl_Olive', seg=4)
    lean = math.radians(12)
    v = Vector((0, -math.sin(lean), math.cos(lean)))
    n = Vector((1, 0, 0)).cross(v)
    base = Vector((0, -0.22, 0.02))
    b.rounded_box(base + v * 0.30, (0.44, 0.56, 0.08), 0.07, 'Vinyl_Olive', u=Vector((1, 0, 0)), v=v, seg=4)
    # the frame: side rails under the cushion, rising behind the back to its top
    for x in (-0.18, 0.18):
        top = base + v * 0.52 + n * 0.055        # n looks out of the back of the backrest
        b.tube([(x, 0.18, -0.105), (x, -0.25, -0.105), (x, -0.30, 0.06), (x, top.y, top.z)], 0.011, 'Metal_Steel')
    return finish(b, reuse=True)


# ---------------------------------------------------------------------------------------------- driving controls

def build_steering():
    """Steering wheel and column: a black three-spoke wheel about 0.47 m across (rwd_mb3 9a33_13, 9a33_31), its
    centre where the plan draws it (x -0.34, y 4.38), tilted 30 degrees back from upright (estimate)."""
    b = Builder('SM_SteeringWheel')
    c = Vector((-0.36, 4.33, 1.40))
    tilt = math.radians(30)
    axis = Vector((0, -math.cos(tilt), math.sin(tilt)))   # towards the driver
    b.torus(c, axis, 0.235, 0.016, 'Rubber_Black', seg=56, rseg=10)
    ref = Vector((1, 0, 0))
    e2 = axis.cross(ref).normalized()
    for ang in (90, 210, 330):
        a = math.radians(ang)
        d = ref * math.cos(a) + e2 * math.sin(a)
        b.tube([c + axis * 0.012, c + d * 0.12 + axis * 0.01, c + d * 0.225], 0.009, 'Metal_Dark', seg=10)
    b.lathe(c - axis * 0.02, axis, [(0.0, 0.0), (0.045, 0.0), (0.05, 0.03), (0.035, 0.055), (0.0, 0.06)],
            'Bakelite_Black', seg=24)
    col_end = c - axis * 0.40
    b.cylinder(c - axis * 0.02, col_end, 0.03, 'Metal_Dark', seg=20)
    b.cylinder(c - axis * 0.10, c - axis * 0.32, 0.045, 'Rubber_Black', seg=20)
    return finish(b)


def build_pedals():
    """Brake and accelerator pedals in the driver's footwell (estimate: the sources don't show them)."""
    b = Builder('SM_Pedals')
    for x, w in ((-0.46, 0.09), (-0.20, 0.06)):
        b.obox(Vector((x, 4.72, 0.80)), Vector((1, 0, 0)), Vector((0, 0.5, 0.866)), Vector((0, -0.866, 0.5)),
               (w, 0.13, 0.015), 'Rubber_Black')
        b.cylinder((x, 4.74, 0.80), (x, 4.86, 1.05), 0.01, 'Metal_Dark', seg=10)
    return finish(b, reuse=False)


def build_cab_levers():
    """The gear lever and the handbrake between the cab seats (rwd_mb3 9a33_13: a tall black lever with a ball knob in
    the middle of the cab; 9a33_31: a second black lever low at the right of the driver). Positions, heights and
    the handbrake's throw are estimates: the plan doesn't draw them. Built in place."""
    b = Builder('SM_CabLevers')
    # gear lever: rubber boot, steel shaft leaning slightly forward, ball knob
    base = Vector((-0.02, 4.17, FLOOR_Z))
    b.lathe(base, Vector((0, 0, 1)), [(0.0, 0.0), (0.06, 0.0), (0.06, 0.02), (0.03, 0.09), (0.0, 0.09)], 'Rubber_Black',
            seg=20)
    top = Vector((-0.02, 4.185, 1.0))
    b.tube([base + Vector((0, 0, 0.08)), top], 0.009, 'Metal_Steel', seg=10)
    b.lathe(top, Vector((0, 0.04, 1.0)).normalized(), [(0.0, -0.01), (0.022, 0.0), (0.028, 0.03), (0.018, 0.06), (0.0, 0.065)],
            'Bakelite_Black', seg=20)
    # handbrake: a ratchet plate on the floor, a lever leaning back to the driver, a rubber grip
    foot = Vector((0.06, 4.0, FLOOR_Z))
    b.box(0.05, 0.07, foot.y - 0.05, foot.y + 0.05, FLOOR_Z, FLOOR_Z + 0.07, 'Metal_Dark')
    tip = Vector((0.06, 3.86, 0.98))
    b.tube([foot + Vector((0, 0, 0.05)), tip], 0.011, 'Metal_Dark', seg=10)
    b.tube([tip, tip + Vector((0, -0.05, 0.03))], 0.017, 'Rubber_Black', seg=12)
    return finish(b, reuse=False)


def build_cab_fittings():
    """Fittings along the windscreen header, from opisybroni 54335: a pale sun visor hinged above each screen and
    the cable run along the header with its clips. Sizes, tilt and the cable's route are estimates. Built in place."""
    b = Builder('SM_CabFittings')
    a = math.radians(25)
    v = Vector((0, -math.cos(a), -math.sin(a)))           # down and back from the hinge
    n = Vector((0, -math.sin(a), math.cos(a)))
    hinge = Vector((0, SCREEN_TOP_Y - 0.04, ROOF_Z - 0.03))
    for s in (-1, 1):
        c = hinge + v * 0.10
        c.x = window_x(s)
        b.obox(c, Vector((1, 0, 0)), v, n, (0.56, 0.20, 0.014), 'Paint_Panel')
        for dx in (-0.22, 0.22):                          # the two hinge lugs
            b.box(window_x(s) + dx - 0.02, window_x(s) + dx + 0.02, hinge.y - 0.01, hinge.y + 0.01, hinge.z - 0.02,
                  hinge.z + 0.005, 'Metal_Dark')
    # the cable run: three black cables sagging between clips
    clips = [x / 10.0 for x in range(-10, 11, 4)]
    for k, dz in enumerate((0.0, 0.016, 0.032)):
        pts = []
        for i, x in enumerate(clips):
            pts.append((x, SCREEN_TOP_Y - 0.075, ROOF_Z - 0.035 - dz + (0.012 if i % 2 else 0.0)))
        b.tube(pts, 0.006, 'Rubber_Black', seg=8)
    for x in clips:
        b.box(x - 0.008, x + 0.008, SCREEN_TOP_Y - 0.085, SCREEN_TOP_Y - 0.065, ROOF_Z - 0.07, ROOF_Z - 0.02, 'Metal_Steel')
    # Pillars (opisybroni 54335): a cable bundle clipped down the left pillar beside the left screen with two mounting
    # tabs, and the centre pillar's raised seam, its cable and the screen latch low on it. In the windscreen plane's
    # frame (u across, v up the slant, d out of the panel into the cab). Positions and sizes: estimates.
    o = Vector((0, SCREEN_BOTTOM_Y, DASH_TOP_Z))
    up = (Vector((0, SCREEN_TOP_Y, ROOF_Z)) - o).normalized()
    right = Vector((1, 0, 0))
    nrm = right.cross(up).normalized()

    def at(u, v, d=0.0):
        return o + right * u + up * v + nrm * d

    for du in (-1.025, -1.05):
        b.tube([at(du, 0.45, 0.018), at(du + 0.004, 0.30, 0.02), at(du - 0.003, 0.16, 0.018), at(du, 0.03, 0.018)],
               0.006, 'Rubber_Black', seg=8)
    for v in (0.12, 0.26, 0.40):
        b.obox(at(-1.037, v, 0.026), right, up, nrm, (0.07, 0.03, 0.01), 'Metal_Steel')
        b.cylinder(at(-1.037, v, 0.02), at(-1.037, v, 0.04), 0.006, 'Metal_Dark', seg=8)
    for v in (0.18, 0.33):
        b.obox(at(-0.975, v, 0.012), right, up, nrm, (0.04, 0.07, 0.01), 'Metal_Dark')
        b.cylinder(at(-0.975, v + 0.02, 0.012), at(-0.975, v + 0.02, 0.026), 0.005, 'Metal_Steel', seg=8)
    b.obox(at(0.0, 0.25, 0.006), right, up, nrm, (0.012, 0.40, 0.008), 'Metal_Dark')
    b.tube([at(0.07, 0.45, 0.014), at(0.072, 0.30, 0.016), at(0.066, 0.18, 0.014), at(0.07, 0.12, 0.014)], 0.005,
           'Rubber_Black', seg=8)
    b.obox(at(0.05, 0.09, 0.02), right, up, nrm, (0.06, 0.07, 0.03), 'Metal_Dark')
    b.obox(at(0.05, 0.095, 0.04), right, up, nrm, (0.012, 0.06, 0.012), 'Metal_Steel')
    b.torus(at(0.05, 0.15, 0.03), nrm, 0.014, 0.003, 'Metal_Steel', seg=20, rseg=6)
    # the wiper drive on the dash top (opisybroni 54335: a motor and a bent pipe left of the wheel): a motor can on a
    # bracket, a drive pipe up to the spindle post at the left screen's lower edge, a second spindle post at the
    # right screen (positions: estimate)
    mx, my, mz = -0.80, 4.73, DASH_TOP_Z
    b.box(mx - 0.05, mx + 0.05, my - 0.04, my + 0.04, mz, mz + 0.012, 'Metal_Dark')
    b.cylinder((mx - 0.04, my, mz + 0.045), (mx + 0.04, my, mz + 0.045), 0.032, 'Paint_Equipment', seg=20)
    b.tube([(mx + 0.04, my, mz + 0.045), (mx + 0.10, my, mz + 0.05), (mx + 0.16, my + 0.04, mz + 0.07),
            (mx + 0.16, my + 0.07, mz + 0.12)], 0.007, 'Metal_Steel', seg=10)
    for sx in (-0.70, 0.46):
        b.cylinder((sx, 4.795, mz), (sx, 4.795, mz + 0.07), 0.012, 'Metal_Steel', seg=12)
        b.cylinder((sx, 4.795, mz + 0.07), (sx, 4.795, mz + 0.085), 0.02, 'Metal_Dark', seg=12)
    return finish(b, reuse=False)


# ---------------------------------------------------------------------------------------------- equipment

def knob(b, centre, normal, r, mat='Bakelite_Black'):
    n = Vector(normal).normalized()
    b.lathe(Vector(centre), n, [(0.0, 0.0), (r, 0.0), (r, r * 0.6), (r * 0.85, r * 1.0), (0.0, r * 1.05)], mat, seg=18)


def build_r123m():
    """R-123M radio set, from the traced front panel (tools/blender/r123m_model.py; mon_2112_80 Fig. 3 draws two,
    0.48 x 0.14 m in plan). Built with its front looking forward (+y). The cabin supplies the knobs of the functions it
    simulates; the model has the casing, the plate with its wells, the preset drum under its cap, the windows, the plates
    and the screws."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import r123m_model
    return r123m_model.build(Builder, finish)


def build_ta57():
    """TA-57 field telephone at the front end of the bench (mon_2112_80 Fig. 3), built looking forward: the case and
    its handset (generic shapes)."""
    b = Builder('SM_PhoneTA57')
    b.box(-0.115, 0.115, -0.085, 0.085, -0.05, 0.03, 'Bakelite_Black')
    b.box(-0.115, 0.115, -0.085, 0.085, 0.03, 0.05, 'Paint_Equipment')
    b.tube([(-0.10, 0.0, 0.07), (-0.06, 0.0, 0.085), (0.06, 0.0, 0.085), (0.10, 0.0, 0.07)], 0.014, 'Bakelite_Black')
    for x in (-0.10, 0.10):
        b.cylinder((x, 0.0, 0.055), (x, 0.0, 0.085), 0.028, 'Bakelite_Black', seg=16)
    b.cylinder((0.06, 0.085, -0.01), (0.06, 0.1, -0.01), 0.012, 'Metal_Steel', seg=12)
    return finish(b, reuse=True)


def build_spare_box():
    """Spare-parts box ZCzZ1A nr 1 (plan: 0.30 x 0.12 m on the floor by SP-2M; 0.19 m high as nr 2 in section A-A):
    a steel box with a lid, two catches and a handle."""
    b = Builder('SM_SpareBox')
    b.box(-0.06, 0.06, -0.15, 0.15, -0.095, 0.075, 'Paint_Equipment')
    b.box(-0.064, 0.064, -0.154, 0.154, 0.075, 0.095, 'Paint_Equipment')
    for y in (-0.09, 0.09):
        b.box(0.06, 0.07, y - 0.015, y + 0.015, 0.05, 0.085, 'Metal_Steel')
    b.tube([(0.0, -0.05, 0.095), (0.0, -0.05, 0.115), (0.0, 0.05, 0.115), (0.0, 0.05, 0.095)], 0.006, 'Metal_Steel')
    return finish(b, reuse=True)


def build_ga3():
    """GA-3 gyro half-compass on the floor at the rear (plan: a circle 0.335 m across); height and housing are not in
    the sources, so it is a plain drum on a base (unpolished)."""
    b = Builder('SM_GA3')
    b.lathe(Vector((0, 0, -0.15)), Vector((0, 0, 1)),
            [(0.0, 0.0), (0.2, 0.0), (0.2, 0.03), (0.168, 0.03), (0.168, 0.27), (0.15, 0.30), (0.0, 0.30)],
            'Paint_Equipment', seg=40)
    return finish(b, reuse=True)


def build_capsule():
    """CGW-10P (plan: a capsule 0.33 x 0.2 m on the centreline behind the bench); not identified, so a plain shape
    (unpolished; height estimate)."""
    b = Builder('SM_CGW10P')
    b.tube([(0, -0.065, 0.0), (0, 0.065, 0.0)], 0.1, 'Paint_Equipment', seg=24)
    for y in (-0.065, 0.065):
        b.lathe(Vector((0, y, 0)), Vector((0, 1 if y > 0 else -1, 0)), [(0.1, 0.0), (0.07, 0.06), (0.0, 0.1)],
                'Paint_Equipment', seg=24)
    return finish(b, reuse=True)


def build_awo():
    """AWO-5M1 (plan: a box 0.24 x 0.25 m at the rear bulkhead); not identified: a plain box (unpolished)."""
    b = Builder('SM_AWO5M1')
    b.box(-0.12, 0.12, -0.125, 0.125, -0.15, 0.15, 'Paint_Equipment')
    return finish(b, reuse=True)


def build_cabinet_0096():
    """The distribution cabinet 0096-9M's body, behind its control panel (pwr_cabinet in the layout file): plan
    0.66 x 0.19 m, standing diagonally in the aisle; its height is an estimate. Built facing forward."""
    b = Builder('SM_Cabinet0096')
    b.box(-0.33, 0.33, -0.095, 0.095, -0.54, 0.54, 'Paint_Rack')
    b.box(-0.34, 0.34, -0.105, 0.105, -0.54, -0.48, 'Paint_Rack')
    for x in (-0.31, 0.31):
        b.cylinder((x, 0.095, 0.2), (x, 0.095, 0.3), 0.008, 'Metal_Steel', seg=10)
    return finish(b, bevel=0.003, reuse=True)


def build_dome_lamp():
    """A ceiling lamp: a pressed steel housing and a frosted glass bowl (shape: estimate)."""
    b = Builder('SM_DomeLamp')
    b.lathe(Vector((0, 0, 0.03)), Vector((0, 0, -1)),
            [(0.0, 0.0), (0.09, 0.0), (0.09, 0.012), (0.075, 0.02), (0.0, 0.02)], 'Paint_Interior', seg=32)
    b.lathe(Vector((0, 0, 0.01)), Vector((0, 0, -1)),
            [(0.072, 0.0), (0.07, 0.015), (0.055, 0.035), (0.0, 0.042)], 'Lamp_Glass', seg=32)
    return finish(b, reuse=True)


def build_wall_lamp_blue():
    """A small wall lamp with a blue lens (usaf_shaw_2017 -019: a round blue-green lens on the right-hand wall,
    probably the blackout light; position: estimate)."""
    b = Builder('SM_WallLampBlue')
    b.lathe(Vector((0, 0, 0)), Vector((0, -1, 0)), [(0.0, 0.0), (0.06, 0.0), (0.06, 0.02), (0.05, 0.03), (0.0, 0.03)],
            'Paint_Interior', seg=28)
    b.lathe(Vector((0, -0.03, 0)), Vector((0, -1, 0)), [(0.045, 0.0), (0.04, 0.012), (0.0, 0.02)], 'Lamp_Blue', seg=28)
    return finish(b, reuse=True)


def build_duct():
    """A flexible ventilation hose along the ceiling over the aisle from the rear bulkhead (usaf_shaw_2017 -019 shows a
    large corrugated hose overhead towards the rear; its route is an estimate)."""
    b = Builder('SM_VentDuct')
    r = 0.075
    path = [Vector((0.42, REAR_Y + 0.02, 1.93)), Vector((0.42, -0.6, 1.98)), Vector((0.40, 0.2, 2.0)),
            Vector((0.30, 0.9, 1.99))]
    pts = []
    for i in range(len(path) - 1):
        a, c = path[i], path[i + 1]
        n = int((c - a).length / 0.03)
        for k in range(n):
            pts.append(a.lerp(c, k / n))
    pts.append(path[-1])
    # corrugation: alternate the radius along the hose
    b2 = b
    rings_r = [r * (1.0 if i % 2 == 0 else 0.93) for i in range(len(pts))]
    rings = []
    seg = 18
    for i, p in enumerate(pts):
        t = ((pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)])).normalized()
        ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
        e1 = t.cross(ref).normalized()
        e2 = t.cross(e1).normalized()
        rings.append([b2.bm.verts.new(P(*(p + (e1 * math.cos(2 * math.pi * k / seg) + e2 * math.sin(2 * math.pi * k / seg))
                                            * rings_r[i]))) for k in range(seg)])
    idx = b2.mat('Duct_Grey')
    for a, c in zip(rings, rings[1:]):
        for k in range(seg):
            f = b2.bm.faces.new((a[k], a[(k + 1) % seg], c[(k + 1) % seg], c[k]))
            f.material_index = idx
            f.smooth = True
    # straps to the roof
    for y in (-0.6, 0.2):
        b.box(0.32, 0.52, y - 0.02, y + 0.02, 1.99 + 0.04, ROOF_Z, 'Metal_Dark')
    return finish(b)


# ---------------------------------------------------------------------------------------------- main

def build_all(layout):
    objs = [build_hull(), build_glass()]
    objs.append(build_rack('SM_RackSL1M', layout, ['gyro_panel'], -1, [(3.96, 4.06), (2.505, 2.65)], 'doors',
                           extra=sl1m_extra))
    objs.append(build_rack('SM_RackSL2M', layout, ['fo_panel', 'so_panel', 'op_panel'], -1, [], 'kick'))
    objs.append(build_rack('SM_RackSL3M', layout, ['sl3m_panel'], -1, [(0.488, 0.605), (REAR_Y, -0.842)], 'doors'))
    have = {p['id'] for p in layout['panels']}
    if 'sp1m_panel' in have:
        objs.append(build_rack('SM_RackSP1M', layout, ['sp1m_panel'], 1, [(3.96, 4.06), (2.505, 2.65)], 'doors',
                               extra=sp1m_extra))
    if 'sp2m_panel' in have:
        objs.append(build_rack('SM_RackSP2M', layout, ['sp2m_panel'], 1, [], 'doors'))
    if 'sp3m_panel' in have:
        objs.append(build_rack('SM_RackSP3M', layout, ['sp3m_panel'], 1, [(0.485, 0.605), (REAR_Y, -0.83)], 'doors'))
    objs += [build_bench(), build_operator_seat(), build_cab_seat(), build_steering(), build_pedals(), build_cab_levers(), build_cab_fittings(), build_r123m(),
             build_ta57(), build_spare_box(), build_ga3(), build_capsule(), build_awo(), build_cabinet_0096(),
             build_dome_lamp(), build_wall_lamp_blue(), build_duct()]
    return objs


def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    loc = obj.location.copy()
    obj.location = (0, 0, 0)       # export around the origin (the bounding box centre)
    path = os.path.join(OUT, obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                             axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True, mesh_smooth_type='FACE',
                             add_leaf_bones=False, bake_anim=False, use_tspace=True)
    obj.location = loc
    return path


# Where the preview puts one copy of each reusable mesh (the layout file places the real ones).
PREVIEW_PLACES = {
    'SM_OperatorSeat': [((-0.34, y, SEAT_Z - 0.04), -90) for y in (0.91, 1.555, 2.2)],
    'SM_CabSeat': [((-0.36, 3.866, SEAT_Z + 0.07), 0), ((0.33, 3.866, SEAT_Z + 0.07), 0)],
    'SM_RadioR123M': [((-0.62, 3.28, 1.25), 90), ((0.64, 3.215, 0.80), -90)],
    'SM_PhoneTA57': [((-0.10, 2.37, 1.03), 0)],
    'SM_SpareBox': [((0.51, 2.32, 0.715), 0)],
    'SM_GA3': [((-0.538, -0.345, 0.77), 0)],
    'SM_CGW10P': [((0.0, 0.645, 0.72), 0)],
    'SM_AWO5M1': [((0.0, -0.632, 0.77), 0)],
    'SM_Cabinet0096': [((0.08, 0.125, 1.16), -38)],
    'SM_DomeLamp': [((0.12, 2.0, 2.075), 0), ((0.12, 0.6, 2.075), 0), ((0.0, 3.3, 2.075), 0)],
    'SM_WallLampBlue': [((0.9, REAR_Y + 0.03, 1.85), 0)],
}

PREVIEW_VIEWS = {  # name: (eye body xyz, yaw deg clockwise from forward, pitch deg, lens mm)
    'bench': ((-0.33, 1.555, 1.83), -90, -8, 18),
    'aisle_fwd': ((0.25, -0.5, 1.70), 2, -10, 14),
    'aisle_aft': ((0.15, 3.3, 1.75), 182, -12, 14),
    'cab': ((0.0, 3.3, 1.80), 0, -12, 16),
    'right_racks': ((-0.30, 1.5, 1.70), 90, -5, 16),
}


def preview(objs, out_prefix):
    """Render a few views of the furnished compartment (Eevee), lit by area lights at the dome lamps."""
    sc = bpy.context.scene
    by_name = {o.name: o for o in objs}
    for name, places in PREVIEW_PLACES.items():
        src = by_name.get(name)
        if src is None:
            continue
        for i, ((x, y, z), yaw) in enumerate(places):
            o = src if i == 0 else src.copy()
            if i:
                bpy.data.collections[COLLECTION].objects.link(o)
            o.location = P(x, y, z)
            o.rotation_euler = (0, 0, math.radians(-yaw))
    for (x, y, z), _ in PREVIEW_PLACES['SM_DomeLamp']:
        light = bpy.data.objects.new('lamp', bpy.data.lights.new('lamp', 'POINT'))
        light.data.energy = 25
        light.data.color = (1.0, 0.85, 0.65)
        light.data.shadow_soft_size = 0.06
        light.location = P(x, y, z - 0.06)
        bpy.data.collections[COLLECTION].objects.link(light)
    world = bpy.data.worlds.new('preview') if sc.world is None else sc.world
    sc.world = world
    world.color = (0.02, 0.022, 0.025)
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1280, 720
    cam = bpy.data.objects.new('preview_cam', bpy.data.cameras.new('preview_cam'))
    bpy.data.collections[COLLECTION].objects.link(cam)
    sc.camera = cam
    cam.data.clip_start = 0.01
    paths = []
    for name, (eye, yaw, pitch, lens) in PREVIEW_VIEWS.items():
        cam.location = P(*eye)
        # a Blender camera looks down its -Z: tilt it up to the horizon, then turn it to the heading
        cam.rotation_euler = (math.radians(90 + pitch), 0, math.radians(-90 - yaw))
        cam.data.lens = lens
        sc.render.filepath = '%s_%s.png' % (out_prefix, name)
        bpy.ops.render.render(write_still=True)
        paths.append(sc.render.filepath)
    return paths


def reset():
    coll = bpy.data.collections.get(COLLECTION)
    if coll is None:
        coll = bpy.data.collections.new(COLLECTION)
        bpy.context.scene.collection.children.link(coll)
    for o in list(coll.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):
        if m.users == 0:
            bpy.data.meshes.remove(m)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    reset()
    os.makedirs(OUT, exist_ok=True)
    layout = load_layout()
    objs = build_all(layout)
    manifest = {}
    for obj in objs:
        export(obj)
        manifest[obj.name] = {'size_m': [round(v, 4) for v in obj['body_size']]}
        if not obj['reuse']:
            manifest[obj.name]['centre_m'] = [round(v, 4) for v in obj['body_centre']]
    with open(os.path.join(OUT, 'manifest.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(manifest, f, indent=1, sort_keys=True)
        f.write('\n')
    if bpy.app.background:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'cabin_interior.blend'))
    print('exported', len(objs), 'meshes to', OUT)
    if '--preview' in argv:
        prefix = os.path.abspath(argv[argv.index('--preview') + 1])
        os.makedirs(os.path.dirname(prefix), exist_ok=True)
        for p in preview(objs, prefix):
            print('preview', p)
    return manifest


if __name__ == '__main__':
    main()
