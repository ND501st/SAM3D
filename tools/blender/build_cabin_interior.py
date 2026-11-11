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
SHELL = os.path.join(REPO, 'data', 'cabins', 'osa_ak_shell.json')
PARTS = os.path.join(REPO, 'data', 'cabins', 'osa_ak_parts.json')     # parts rebuilt from the photographs (docs/OSA_MODELLING_PROCESS.md)
RIGHT_SHIFT_X = 0.302   # what was measured against the right racks before 11 Nov 2026 stands this much further outboard: the turret's axis is 0.914 m from SP-2M2's face, not Fig. 3's 0.612 (docs/research/OSA_REBUILD_C_2026-11.md)
COLLECTION = 'OsaInterior'

# ---------------------------------------------------------------------------------------------- the plan (body frame)
# mon_2112_80 Fig. 3 at 4.785 mm per pixel unless noted.
FLOOR_Z = 0.62          # floor top (layout file); the plan gives no floor level
ROOF_Z = 2.10           # roof lining; the middle and aft racks reach it (sections A-A, B-B); the front two stand lower (docs/OSA_INTERIOR_HOLISTIC.md section 22)
WALL_X = 1.32           # side walls at rack height: -1.333 left, +1.306 right in the drawing, made symmetric
REAR_Y = -1.035         # rear bulkhead (door to the engine compartment): the depth maps' fused cloud, -1.02 to -1.04 across its whole width (docs/OSA_INTERIOR_HOLISTIC.md section 31)
CAB_Y = 4.06            # the side walls turn in towards the bow from here (front post of SŁ-1M / SP-1M)
BOW_Y = 4.90            # footwell front under the dashboard (plan front line 5.04 at floor level, less the bow lining)
BOW_X = 1.14            # half-width at the bow (plan: 2.27 m across at the front line)
DASH_TOP_Z = 1.52       # dashboard top, under the windscreens: g082-b, camera on the mat's vanishing point (docs/OSA_INTERIOR_HOLISTIC.md section 30)
HEADER_Z = 2.06         # the loom along the windscreen's top: g082-b reads 2.13 +-0.05 (section 30), kept under the 2.10 roof
FRONT_RACK_TOP_Z = 1.96 # the frame's top over SŁ-1M and SP-1M, 1.93 measured (deck + 0.95), and the panels' traced height
SCREEN_BOTTOM_Y = 4.80  # windscreen panel: bottom edge (y, z = DASH_TOP_Z) to top edge (y, z = ROOF_Z); the
SCREEN_TOP_Y = 4.55     # lean (28 degrees from vertical) is an estimate from the exterior photos (opisybroni)
SCREEN_CENTRE_S = 0.2795 # the panes' centre up the windscreen's slant from its foot: glass 1.556 to 1.997 (g082-b, section 30)
SCREEN_H = 0.48         # the glass up the slant
WINDSCREEN = {-1: (-0.581, 0.966), 1: (0.537, 1.078)}   # centre across and width of each pane (g082-b, section 30; the right is the wider)
VISOR_X = {-1: -0.42, 1: 0.345}     # the visors hang inboard of the panes' centres (g082-b, section 30)
VISOR_W, VISOR_HINGE_Z = 0.51, 2.05  # 1.85 to 2.04 in g082-b
SIDE_WINDOW_Z = 1.685   # the corner windows' centre, the same band as the windscreen's (exterior profiles g011-a, g012-a)
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

    def loft_x(self, section0, section1, x0, x1, mat):
        """A solid between a closed section [(y, z)...] at x0 and another with as many points at x1; its sides are triangles, since a twisted side has no one plane."""
        a = [self.bm.verts.new(P(x0, y, z)) for y, z in section0]
        b = [self.bm.verts.new(P(x1, y, z)) for y, z in section1]
        idx = self.mat(mat)
        n = len(section0)
        faces = [list(reversed(a)), b]
        for i in range(n):
            j = (i + 1) % n
            faces += [(a[i], a[j], b[j]), (a[i], b[j], b[i])]
        for v in faces:
            self.bm.faces.new(v).material_index = idx

    def prism_y(self, section, y0, y1, mat):
        """Extrude a closed section [(x, z)...] (body) along y from y0 to y1; going round the section with x to the
        right and z up, the solid is on the left."""
        a = [self.bm.verts.new(P(x, y0, z)) for x, z in section]
        b = [self.bm.verts.new(P(x, y1, z)) for x, z in section]
        idx = self.mat(mat)
        n = len(section)
        f0 = self.bm.faces.new(a)
        f1 = self.bm.faces.new(list(reversed(b)))
        for f in (f0, f1):
            f.material_index = idx
        for i in range(n):
            f = self.bm.faces.new((a[(i + 1) % n], a[i], b[i], b[(i + 1) % n]))
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
    'Paint_LightGrey': (0.6, 0.61, 0.62), 'Lens_Amber': (0.75, 0.38, 0.06), 'Lens_Green': (0.1, 0.45, 0.15),
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

def load_shell():
    """The lofted shell's numbers, each with its source (data/cabins/osa_ak_shell.json, roadmap I1)."""
    with open(SHELL, encoding='utf-8') as f:
        return json.load(f)


def lining_z(half, x):
    """The roof lining's height at x across the compartment, from the half section's points."""
    x = abs(x)
    pts = sorted((p['x_m'], p['z_m']) for p in half)
    if x <= pts[0][0]:
        return pts[0][1]
    for (x0, z0), (x1, z1) in zip(pts, pts[1:]):
        if x <= x1:
            return z0 + (z1 - z0) * (x - x0) / (x1 - x0)
    return pts[-1][1]


def lining_strip(half, x0, x1, top):
    """A closed section of the lining between x0 and x1, up to the roof's top."""
    folds = [s * p['x_m'] for p in half for s in (-1, 1) if x0 < s * p['x_m'] < x1]
    xs = sorted(set([x0, x1] + folds))
    return [(x, lining_z(half, x)) for x in xs] + [(x1, top), (x0, top)]


def build_roof(b, shell, y_hatch0, y_hatch1, hatch_xs):
    """The lining from the rear bulkhead forward over the cab to the cab's front band, lofted from the photos' section:
    closed, so Lumen's distance fields see a solid, and cut round the hatches into closed strips. Then the band, the
    rear cove, the beam across it, the turret's opening on it and the wheel arch below."""
    W, T = WALL_X, THICK
    roof = shell['roof_section']
    half = sorted(roof['half'], key=lambda p: -p['x_m'])
    top = max(p['z_m'] for p in half) + T
    whole = lining_strip(half, -W - T, W + T, top)
    y_to = roof['y_to_m']
    pocket_ = shell['roof_pocket']
    b.prism_y(whole, roof['y_from_m'], pocket_['y_from_m'], 'Paint_Interior')          # cut for the recess forward of the band (built below)
    b.prism_y(whole, pocket_['y_to_m'], y_hatch0, 'Paint_Interior')
    b.prism_y(whole, y_hatch1, y_to, 'Paint_Interior')
    xs = sorted(hatch_xs)
    for a, c in ((-W - T, xs[0]), (xs[1], xs[2]), (xs[3], W + T)):                 # beside and between the hatches
        b.prism_y(lining_strip(half, a, c, top), y_hatch0 - 0.001, y_hatch1 + 0.001, 'Paint_Interior')
    cab = shell['cab_front']                                                       # the band drops to the screens' header
    b.box(-W - T, W + T, y_to, y_to + cab['depth_m'], ROOF_Z, top, 'Paint_Interior')
    lip = cab['lip']
    b.box(-lip['half_x_m'], lip['half_x_m'], lip['y_from_m'], lip['y_to_m'], lip['z_from_m'], ROOF_Z + 0.01, 'Paint_Interior')
    cove = shell['rear_cove']
    b.prism([(REAR_Y, cove['z_at_bulkhead_m']), (cove['y_forward_m'], max(p['z_m'] for p in half)),
             (cove['y_forward_m'], top), (REAR_Y, top)], -W, W, 'Paint_Interior')
    pocket = shell['roof_pocket']                                                  # the recess above the band's forward face: a slab higher than the lining, over it
    y0, y1, x0, x1, pz, slab = pocket['y_from_m'], pocket['y_to_m'], pocket['x_from_m'], pocket['x_to_m'], pocket['z_m'], max(top, pocket['z_m'] + T)
    zo = roof['outboard_z_m']
    b.prism_y([(-W - T, zo), (x0, zo), (x0, pz), (x1, pz), (x1, zo), (W + T, zo), (W + T, slab), (-W - T, slab)], y0, y1, 'Paint_Interior')
    beam = shell['beam']
    bc, bx = beam.get('centre_x_m', 0.0), beam['half_x_m']
    b.box(bc - bx, bc + bx, beam['y_aft_m'], beam['y_forward_m'], beam['z_bottom_m'], slab, 'Paint_Interior')
    o = shell['turret_opening']
    cx, cy = o['centre_xy_m']
    ax, ay = o['half_x_m'], o['half_y_m']

    def on(x, y, drop):
        return (x, y, lining_z(half, x) - drop)

    # The dark opening, strips across it following the lining's folds, and its collar.
    folds = [s * p['x_m'] for p in half for s in (-1, 1) if abs(p['x_m']) < ax]
    xs = sorted(set([cx + ax * math.cos(math.pi * i / 32) for i in range(33)] + folds))
    for x0, x1 in zip(xs, xs[1:]):
        h0 = ay * math.sqrt(max(0.0, 1 - ((x0 - cx) / ax) ** 2))
        h1 = ay * math.sqrt(max(0.0, 1 - ((x1 - cx) / ax) ** 2))
        b.face([on(x0, cy - h0, 0.006), on(x0, cy + h0, 0.006), on(x1, cy + h1, 0.006), on(x1, cy - h1, 0.006)], 'Paint_Equipment')   # olive casting (g100-b)
    n = 48
    ring = [(cx + ax * math.cos(2 * math.pi * i / n), cy + ay * math.sin(2 * math.pi * i / n)) for i in range(n + 1)]
    b.tube([on(x, y, 0.012) for x, y in ring], o['lining_roll_r_m'], 'Paint_Interior')         # the lining's soft roll
    collar = [(cx + (ax - 0.04) * math.cos(2 * math.pi * i / n), cy + (ay - 0.04) * math.sin(2 * math.pi * i / n)) for i in range(n + 1)]
    b.tube([on(x, y, 0.04) for x, y in collar], o['collar_r_m'], 'Paint_LightGrey')                # the painted collar inside it
    base = o['base']
    gx, gy, gw, gd = base['gearbox_xy_m']
    b.box(gx - gw / 2, gx + gw / 2, gy - gd / 2, gy + gd / 2, lining_z(half, gx) - base['drop_m'], lining_z(half, gx) - 0.004, 'Paint_Equipment')
    qx, qy = base['drum_xy_m']
    b.cylinder((qx, qy, lining_z(half, qx) - 0.004), (qx, qy, lining_z(half, qx) - base['drop_m'] * 1.6), base['drum_r_m'], 'Canvas', seg=20, smooth=False)
    bo = o['boot']
    bx, by = bo['centre_xy_m']
    bz0, bz1 = bo['z_bottom_m'], bo['z_top_m']
    b.lathe(Vector((bx, by, bz0)), Vector((0, 0, 1)), [(0.0, 0.0), (bo['r_bottom_m'], 0.0), (bo['r_bottom_m'] + 0.012, (bz1 - bz0) * 0.35),
                                                        (bo['r_top_m'] - 0.01, (bz1 - bz0) * 0.75), (bo['r_top_m'], bz1 - bz0), (0.0, bz1 - bz0)], 'Duct_Grey', seg=32)   # dark grey canvas
    b.tube([(bx + bo['r_bottom_m'] * math.cos(2 * math.pi * i / 32), by + bo['r_bottom_m'] * math.sin(2 * math.pi * i / 32), bz0 + 0.012) for i in range(33)],
           bo['hem_r_m'], 'Paint_Equipment', seg=8)
    col = bo.get('column')
    if col:         # the turret's central column below the boot, down to its base ring on the floor (g097-b)
        cz0 = col['z_bottom_m']
        prof = sorted(col['profile_z_r_m'])
        pts = [(0.0, 0.0), (prof[0][1], 0.0)] + [(r, z - cz0) for z, r in prof] + [(0.0, prof[-1][0] - cz0)]
        b.lathe(Vector((bx, by, cz0)), Vector((0, 0, 1)), pts, 'Paint_LightGrey', seg=40)
        ring = col['base_ring']
        b.cylinder((bx, by, ring['z_m'][0]), (bx, by, ring['z_m'][1]), ring['r_m'], 'Paint_LightGrey', seg=40, smooth=False)
    build_roof_details(b, shell, half)
    arch = shell['wheel_arch_right']
    b.prism(arch['section_yz_m'], arch['x_from_m'], arch['x_to_m'], 'Paint_Equipment')
    return top


def build_floor_parts(b, shell):
    """The floor as the book's photos show it (data/cabins/osa_ak_shell.json 'floor'): the aisle's pressed sheet with its
    ribs and the dark strip at the rack's foot, the bench's chamfered base, the stair at the aft end, the round hatch and
    the housings. Every part a closed solid."""
    f = shell['floor']
    pl = f['plinth_aft_right']
    b.prism_y([tuple(p) for p in pl['section_xz_m']], pl['y_from_m'], pl['y_to_m'], 'Paint_Interior')
    st = f['step_forward_right']
    b.box(st['x_from_m'], st['x_to_m'], st['y_from_m'], st['y_to_m'], FLOOR_Z, st['z_top_m'], 'Metal_Dark')
    no = f['nosing_cab_right']
    b.box(no['x_from_m'], no['x_to_m'], no['y_from_m'], no['y_to_m'], FLOOR_Z, no['z_top_m'], 'Metal_Steel')
    sp = f['spine']
    x0, x1, y0, y1, z = sp['x_from_m'], sp['x_to_m'], sp['y_from_m'], sp['y_to_m'], sp['z_top_m']
    step = sp['forward_step']
    top_at = lambda y: z if y < step['y_m'] else step['z_top_m']        # two levels: the depth maps (section 31)
    ch = sp['channel']
    for a, c, drop in ((x0, ch['x_from_m'], 0.0), (ch['x_from_m'], ch['x_to_m'], ch['drop_m']), (ch['x_to_m'], x1, 0.0)):      # a pressed channel down the walkway
        b.box(a, c, y0, step['y_m'], FLOOR_Z - 0.002, z - drop, 'Floor_Steel')
        b.box(a, c, step['y_m'], y1, FLOOR_Z - 0.002, (step['z_top_m'] if a >= step['x_from_m'] else z) - drop, 'Floor_Steel')      # the step is only right of the channel's left edge
    r0, r1 = sorted(x1 - (x1 - x0) * k for k in sp['ribbed_from_right'])
    for x in (r0, r1):                                                  # the ridges bounding the ribbed band
        b.box(x - 0.012, x + 0.012, y0, step['y_m'], z - 0.001, z + 0.006, 'Floor_Steel')
        b.box(x - 0.012, x + 0.012, step['y_m'], y1, step['z_top_m'] - 0.001, step['z_top_m'] + 0.006, 'Floor_Steel')
    for y in sp['rib_y_m']:
        b.box(r0 + 0.012, r1 - 0.012, y - 0.02, y + 0.02, top_at(y) - 0.001, top_at(y) + 0.005, 'Floor_Steel')
    tr = f['trough_right']
    gap0, gap1 = tr['gap_y_m']
    b.box(tr['x_from_m'], tr['x_to_m'], tr['y_from_m'], gap0, FLOOR_Z - 0.002, tr['z_top_m'], 'Metal_Dark')
    b.box(tr['x_from_m'], tr['x_to_m'], gap1, tr['y_to_m'], FLOOR_Z - 0.002, tr['z_top_m'], 'Metal_Dark')
    b.box(tr['x_from_m'], tr['x_to_m'], gap0, gap1, FLOOR_Z - 0.002, FLOOR_Z + 0.002, 'Rubber_Black')
    bb = f['bench_base_bays']
    pitch = (bb['y_to_m'] - bb['y_from_m']) / bb['count']
    fx, out, top = bb['x_face_m'], bb['protrude_m'], bb['z_top_m']
    for k in range(bb['count']):
        a = bb['y_from_m'] + k * pitch
        b.prism_y([(fx - 0.01, FLOOR_Z), (fx + out, FLOOR_Z), (fx - 0.01, top)], a + 0.01, a + pitch - 0.01, 'Paint_Bench')
    sa = f['stair_plates_aft']
    for s_ in sa['steps']:
        b.box(sa['x_from_m'], sa['x_to_m'], s_['y_from_m'], s_['y_to_m'], FLOOR_Z - 0.002, s_['z_top_m'], 'Floor_Steel')
    h = f['hatch_round_aft_left']
    cx, cy = h['centre_xy_m']
    hz = h['z_top_m']
    b.cylinder((cx, cy, hz - 0.002), (cx, cy, hz + h['rim_m']), h['r_m'], 'Paint_Equipment', seg=32, smooth=False)
    b.cylinder((cx, cy, hz + h['rim_m'] - 0.001), (cx, cy, hz + h['rim_m'] + 0.004), h['r_m'] * 0.55, 'Floor_Steel', seg=24, smooth=False)
    for k in range(h['bolts']):
        a = 2 * math.pi * k / h['bolts']
        q = (cx + 0.87 * h['r_m'] * math.cos(a), cy + 0.87 * h['r_m'] * math.sin(a))
        b.cylinder((q[0], q[1], hz + h['rim_m'] - 0.001), (q[0], q[1], hz + h['rim_m'] + 0.009), 0.012, 'Metal_Steel', seg=6, smooth=False)
    ar = f['arch_forward_right']
    b.prism([tuple(p) for p in ar['section_yz_m']], ar['x_from_m'], ar['x_to_m'], 'Paint_Equipment')
    for r in f['raised']:                                               # what the depth comparison found standing on the floor (docs/OSA_INTERIOR_HOLISTIC.md section 33)
        if superseded('floor.raised.' + r['id']):                       # rebuilt from the photographs: data/cabins/osa_ak_parts.json
            continue
        if 'section_at_x_from_yz_m' in r:
            b.loft_x([tuple(p) for p in r['section_at_x_from_yz_m']], [tuple(p) for p in r['section_at_x_to_yz_m']], r['x_from_m'], r['x_to_m'], r['material'])
        elif 'section_yz_m' in r:
            b.prism([tuple(p) for p in r['section_yz_m']], r['x_from_m'], r['x_to_m'], r['material'])
        else:
            b.prism_y([tuple(p) for p in r['section_xz_m']], r['y_from_m'], r['y_to_m'], r['material'])


def build_aft_fittings(b, shell):
    """The machine hung under the roof, the cover on the blower's opening and the box below it (docs/OSA_INTERIOR_HOLISTIC.md section 36)."""
    f = shell['aft_fittings']
    m = f['hung_motor']
    x, y = m['axis_xy_m']
    for part, r_key, mat in (('drum_z_m', 'drum_r_m', 'Paint_Radio'), ('upper_z_m', 'upper_r_m', 'Paint_Radio'), ('flange_z_m', 'flange_r_m', 'Paint_Radio'), ('mount_z_m', 'mount_r_m', 'Duct_Grey')):
        z0, z1 = m[part]
        b.cylinder((x, y, z0), (x, y, z1), m[r_key], mat, seg=40)
    (x0, y0, z0), (x1, y1, z1) = m['box_xyz_m']
    b.box(x0, x1, y0, y1, z0, z1, 'Paint_Radio')
    q = f['dark_box']
    b.box(q['x_m'][0], q['x_m'][1], REAR_Y - 0.002, q['y_front_m'], q['z_m'][0], q['z_m'][1], 'Metal_Dark')
    c = f['blower_cover']
    b.box(c['x_m'][0], c['x_m'][1], REAR_Y - 0.002, REAR_Y + c['proud_m'], c['z_m'][0], c['z_m'][1], 'Paint_Radio')
    q = f['sloped_box']
    zb, zt = q['z_m']
    b.prism([(REAR_Y - 0.002, zb), (REAR_Y + q['depth_m'], zb), (REAR_Y + q['depth_m'], zb + q['front_height_m']), (REAR_Y - 0.002, zt)], q['x_m'][0], q['x_m'][1], 'Paint_Radio')


def load_parts():
    """The parts rebuilt by docs/OSA_MODELLING_PROCESS.md: {id: entry}. Each names what of the older shell it replaces (`supersedes`)."""
    if not os.path.exists(PARTS):
        return {}
    with open(PARTS, encoding='utf-8') as f:
        return {q['id']: q for q in json.load(f)['parts']}


def superseded(what):
    return any(what in q.get('supersedes', []) for q in load_parts().values())


def build_osa_parts(b):
    """Every rebuilt part as its part object's own mesh (tools/recon/osa/parts.py: no bpy there, so the fit, the tests
    and this builder read one definition). Body-frame vertices; finish() turns the faces outwards."""
    sys.path.insert(0, os.path.join(REPO, 'tools', 'recon', 'osa'))
    import parts as osa_parts
    for q in load_parts().values():
        if 'mesh' in q:                     # a part that keeps a mesh of its own (the first-aid box): its builder reads the part
            continue
        V, F = osa_parts.from_dict(q['shape']).mesh()
        verts = [b.bm.verts.new(P(*v)) for v in V]
        idx = b.mat(q['shape'].get('material', 'Paint_Interior'))
        for tri in F:
            if len({int(i) for i in tri}) < 3:
                continue
            try:
                b.bm.faces.new([verts[int(i)] for i in tri]).material_index = idx
            except ValueError:          # the same face twice
                pass


def build_posts(b, shell):
    """The bent pressed posts between the racks: one channel swept along the outline on the aisle side, upright beside
    the rack and, below the knee, raking inboard and further aft, with no step between (wwp_gecko g098-a); behind the
    leg a thin gusset plate out to the back line (docs/OSA_INTERIOR_HOLISTIC.md section 40)."""
    ps = shell['posts']
    d, gt, gb = ps['channel_depth_m'], ps['gusset_t_m'], ps['gusset_set_back_m']
    for side in ('right', 'left'):
        if superseded('posts.' + side):     # rebuilt from the photographs: data/cabins/osa_ak_parts.json
            continue
        p = ps[side]
        outline = [tuple(q) for q in p['x_z_m']]
        back = p['back_x_m']
        knee = min(range(len(outline)), key=lambda i: abs(outline[i][1] - p['knee_z_m']))
        up, leg = p['y_m'], p['leg_y_m']

        def ys(i):
            if i < knee:
                return leg
            if i > knee:
                return up
            return ((up[0] + leg[0]) / 2, (up[1] + leg[1]) / 2)     # the bend: halfway between leg and upright
        sgn = 1.0 if back > outline[0][0] else -1.0
        rings = []
        for i, (x, z) in enumerate(outline):
            y0, y1 = ys(i)
            xw = x + sgn * d
            xw = min(xw, back) if sgn > 0 else max(xw, back)
            rings.append([b.bm.verts.new(P(x, y0, z)), b.bm.verts.new(P(x, y1, z)),
                          b.bm.verts.new(P(xw, y1, z)), b.bm.verts.new(P(xw, y0, z))])
        idx = b.mat('Paint_Interior')
        faces = [rings[0], list(reversed(rings[-1]))]
        for r0, r1 in zip(rings, rings[1:]):
            for k in range(4):
                m = (k + 1) % 4
                faces += [(r0[k], r0[m], r1[m]), (r0[k], r1[m], r1[k])]
        for f in faces:
            b.bm.faces.new(f).material_index = idx
        if 'shroud' in p:
            build_shroud(b, p['shroud'])
            continue
        # the gusset: from the channel's wall-side edge out to the back line, from the floor to the knee
        sec = [(min(x + sgn * d, back) if sgn > 0 else max(x + sgn * d, back), z) for x, z in outline[:knee + 1]]
        sec += [(back, outline[knee][1]), (back, outline[0][1])]
        if side == 'left':
            sec = list(reversed(sec))
        yg = leg[1] - gb
        b.prism_y(sec, yg - gt / 2, yg + gt / 2, 'Paint_Interior')



def build_shroud(b, sh):
    """The pressed cover behind the right post's leg (docs/OSA_INTERIOR_HOLISTIC.md section 41): its outline at y facing
    forward, carried aft by its depth; the latch's bent plate from the leg's face to the cover's, a bracket on the
    cover's face and the hasp's wire loop."""
    y = sh['y_m']
    ol = [tuple(q) for q in sh['outline_x_z_m']]
    b.prism_y(list(reversed(ol)), y - sh['depth_m'], y, 'Paint_Interior')
    (x0, z0), (x1, z1) = sh['bracket_x_z_m']
    b.box(x0, x1, y, y + 0.01, z0, z1, 'Paint_Interior')
    p0, p1 = Vector(sh['latch_from_xyz_m']), Vector(sh['latch_to_xyz_m'])
    u = (p1 - p0).normalized()
    v = Vector((0, 0, 1))
    b.obox((p0 + p1) * 0.5, u, v, u.cross(v).normalized(), ((p1 - p0).length, sh['latch_h_m'], 0.006), 'Paint_Interior')
    b.tube([tuple(q) for q in sh['hasp_xyz_m']], 0.005, 'Metal_Steel', seg=8)


def build_cab_corner_posts(b, shell):
    """The cab's corner posts, leaning inboard going down (shell 'cab_corner_posts')."""
    c = shell['cab_corner_posts']
    w, t = c['section_m']
    for s_ in (-1, 1):
        a = Vector((s_ * c['top_m'][0], c['top_m'][1], c['top_m'][2]))
        e = Vector((s_ * c['foot_m'][0], c['foot_m'][1], c['foot_m'][2]))
        v = (e - a).normalized()
        u = v.cross(Vector((0, 1, 0))).normalized()
        b.obox((a + e) * 0.5, u, v, u.cross(v).normalized(), (w, (e - a).length, t), 'Paint_Interior')


def build_roof_details(b, shell, half):
    """What hangs on the lining: the white insulators and their cable, the pressed-in seams and the welted edges."""
    d = shell['roof_details']
    ins = d['insulators']
    (x0, y0, z0), (x1, y1, z1) = ins['line_xyz_m']
    n = ins['count']
    cable = []
    for k in range(n):
        t = k / (n - 1)
        x, y, z = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t
        b.cylinder((x, y, z), (x, y, z + ins['stem_m']), 0.004, 'Metal_Steel', seg=8, smooth=False)
        b.cylinder((x, y, z - ins['r_m'] * 1.1), (x, y, z), ins['r_m'], 'Paint_LightGrey', seg=12)
        cable.append((x, y, z - ins['r_m'] * 1.1))
    b.tube(cable, 0.004, 'Rubber_Black')
    sm = d['seams']
    for y in sm['y_m']:
        pts = [(-sm['half_x_m'] + 2 * sm['half_x_m'] * i / 24, y) for i in range(25)]
        b.tube([(x, q, lining_z(half, x) - 0.002) for x, q in pts], sm['r_m'], 'Paint_Interior', seg=8)
    y0, y1 = sm['edge_y_m']
    for s_ in (-1, 1):
        x = s_ * sm['edge_x_m']
        b.tube([(x, y0, lining_z(half, x) - 0.012), (x, y1, lining_z(half, x) - 0.012)], sm['r_m'] * 1.5, 'Paint_Interior', seg=8)


def cab_floor_trace():
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import cab_floor_trace as F
    return F


def ribbed_mat(b, x0, x1, y0, y1, z, pitch):
    """A rubber mat with ribs across it, lying on z (g082-b, g083-a)."""
    b.box(x0, x1, y0, y1, z, z + 0.005, 'Rubber_Mat')
    n = int((y1 - y0) / pitch)
    for i in range(n):
        y = y0 + (i + 0.5) * (y1 - y0) / n
        b.box(x0 + 0.004, x1 - 0.004, y - pitch * 0.28, y + pitch * 0.28, z + 0.005, z + 0.009, 'Rubber_Mat')


def build_cab_floor(b):
    """The raised area the cab seats stand on and the footwell ahead of it (cab_floor_trace.py, docs section 24)."""
    F = cab_floor_trace()
    deck = FLOOR_Z + F.STEP_M
    gap0, gap1 = F.GAP_X
    for x0, x1 in ((F.DECK_X[0], gap0), (gap1, F.DECK_X[1])):
        b.box(x0, x1, F.DECK_AFT_Y, F.DECK_FORE_Y, FLOOR_Z - 0.002, deck, 'Paint_Interior')
    b.box(gap0, gap1, F.DECK_AFT_Y, F.GAP_AFT_Y, FLOOR_Z - 0.002, deck, 'Paint_Interior')
    b.box(F.DECK_X[0], F.DECK_X[1], F.DECK_AFT_Y - 0.012, F.DECK_AFT_Y + 0.004, deck - 0.03, deck + 0.004, 'Metal_Steel')     # the edge
    # the hull's red lead shows through the opening between the seats (g083-a)
    b.box(gap0, gap1, F.GAP_AFT_Y, F.DECK_FORE_Y, FLOOR_Z, FLOOR_Z + 0.002, 'Paint_Red')
    # the pressed cross-member joining the seats at their front edge, with its rounded opening; u x z looks aft
    fx0, fx1 = F.FRAME_X
    cx, cz, w, h, r = F.FRAME_HOLE
    b.slab((0, F.DECK_FORE_Y + F.FRAME_T_M, FLOOR_Z), Vector((1, 0, 0)), Vector((0, 0, 1)),
           [(fx0, 0.0), (fx0, F.STEP_M), (fx1, F.STEP_M), (fx1, 0.0)], [rrect(cx, cz, w, h, r, 5)], F.FRAME_T_M, 'Paint_Interior')
    for y, dz in F.TUBES:
        b.cylinder((gap0 - 0.02, y, deck + dz), (gap1 + 0.02, y, deck + dz), F.TUBE_R_M, 'Paint_Interior', seg=10)
    x, y, top = F.ROD
    b.cylinder((x, y, FLOOR_Z), (x, y, FLOOR_Z + top), F.ROD_R_M, 'Metal_Steel', seg=10)
    # the bolted ribbed mat behind the seats, and the plate with the red disc at its forward end
    ribbed_mat(b, F.MAT_X[0], F.MAT_X[1], F.MAT_Y[0], F.MAT_Y[1], deck, F.RIB_PITCH_M)
    for bx, by in F.MAT_BOLTS:
        b.cylinder((bx, by, deck + 0.009), (bx, by, deck + 0.013), 0.009, 'Metal_Steel', seg=8, smooth=False)
    px0, px1, py0, py1 = F.DISC_PLATE
    b.box(px0, px1, py0, py1, deck + 0.009, deck + 0.013, 'Paint_Interior')
    dx, dy, dr = F.DISC
    b.cylinder((dx, dy, deck + 0.013), (dx, dy, deck + 0.04), dr, 'Paint_Red', seg=24, smooth=False)
    # the footwell: one wide mat, then the floor rising to the toes
    ribbed_mat(b, F.FOOT_MAT_X[0], F.FOOT_MAT_X[1], F.FOOT_MAT_Y[0], F.FOOT_MAT_Y[1], FLOOR_Z, F.RIB_PITCH_M)
    b.prism([(BOW_Y, FLOOR_Z - 0.002), (F.TOE_Y, FLOOR_Z - 0.002), (BOW_Y, FLOOR_Z + F.TOE_RISE_M)], -BOW_X, BOW_X, 'Floor_Steel')
    # the wheel housings along the turned-in corners; a plate closes the top back to the wall
    y0, y1 = F.ARCH_Y
    sec = [(x, FLOOR_Z + h_) for x, h_ in F.ARCH_SECTION]
    b.prism_y(sec, y0, y1, 'Paint_Interior')
    b.prism_y([(-x, z) for x, z in reversed(sec)], y0, y1, 'Paint_Interior')
    xo, zo = sec[2]
    wall1 = WALL_X - (WALL_X - BOW_X) * (y1 - CAB_Y) / (BOW_Y - CAB_Y)
    wall0 = WALL_X - (WALL_X - BOW_X) * (y0 - CAB_Y) / (BOW_Y - CAB_Y)
    for s in (1, -1):
        cap = [(s * xo, y0), (s * xo, y1), (s * wall1, y1), (s * wall0, y0)]
        b.slab((0, 0, zo), Vector((1, 0, 0)), Vector((0, 1, 0)), cap if s > 0 else list(reversed(cap)), [], 0.006, 'Paint_Interior')


def build_hull():
    b = Builder('SM_OsaHull')
    shell = load_shell()
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
    xs = [-0.36 - hx, -0.36 + hx, 0.33 - hx, 0.33 + hx]
    top = build_roof(b, shell, y0, y1, xs)                                             # lofted, cut round the hatches
    y_band = shell['roof_section']['y_to_m']                                           # ahead of the band, to the screens
    b.prism([(y_band, z0), (SCREEN_TOP_Y + T, z0), (SCREEN_TOP_Y + T, z1), (y_band, z1)],
            -front_x, front_x, 'Paint_Interior')
    b.box(-W - T, -front_x, y_band, SCREEN_TOP_Y + T, z0, z1, 'Paint_Interior')        # over the front corners
    b.box(front_x, W + T, y_band, SCREEN_TOP_Y + T, z0, z1, 'Paint_Interior')
    for x in (-0.36, 0.33):
        build_hatch(b, x, HATCH_Y, top)
    # Every wall below is a slab whose face, the plane given, is the inside of the lining (u x v looks into the
    # cabin) and whose thickness goes outwards; the walls run past each other at the corners so the cabin is closed
    # for Lumen's sky light (a gap would let daylight in).
    up_z = Vector((0, 0, 1))
    tall = ROOF_Z - FLOOR_Z + T
    tall_rear = top - FLOOR_Z                     # the rear bulkhead reaches the lofted lining's top
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
        win = rrect(length * at, SIDE_WINDOW_Z - FLOOR_Z, 0.44, 0.36, 0.06, 5)
        corner = [(-0.06, -T), (length + 0.06, -T), (length + 0.06, ROOF_Z - FLOOR_Z + T), (-0.06, ROOF_Z - FLOOR_Z + T)]
        b.slab(origin, uu, up_z, corner, [win], T, 'Paint_Interior')
        seal(b, origin, uu, up_z, win, 'Rubber_Black')
    # Rear bulkhead with the door to the engine compartment, high on it (data/cabins/osa_ak_shell.json 'bulkhead').
    # u = -x: u x z looks forwards, into the cabin.
    rear = [(-W - T, -T), (-W - T, tall_rear), (W + T, tall_rear), (W + T, -T)]
    build_bulkhead(b, shell, rear, T)
    # Dashboard across the cab: its rear face leans back like the instrument panels on it (20 degrees, layout
    # file), 3.5 cm behind their plane; the footwell runs under it to the bow. It runs into the corner walls.
    dash = [(BOW_Y, 1.17), (4.583, 1.17), (4.583 + (DASH_TOP_Z - 1.17) * 0.363, DASH_TOP_Z), (SCREEN_BOTTOM_Y + 0.02, DASH_TOP_Z),
            (BOW_Y, 1.25)]
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
    screens = [rrect(WINDSCREEN[s][0], SCREEN_CENTRE_S, WINDSCREEN[s][1] - 0.02, SCREEN_H - 0.02, 0.08, 6) for s in (-1, 1)]
    b.slab(o, right, up, [(-WALL_X, -0.05), (-WALL_X, slant + T), (WALL_X, slant + T), (WALL_X, -0.05)],
           screens, T, 'Paint_Interior')
    for sc in screens:
        seal(b, o, right, up, sc, 'Rubber_Black')
    build_floor_parts(b, shell)
    build_posts(b, shell)
    build_osa_parts(b)
    build_aft_fittings(b, shell)
    build_cab_corner_posts(b, shell)
    build_cab_floor(b)
    return finish(b, bevel=0.004)


HATCH_Y = 3.80


def build_hatch(b, x, y, rim):
    """A closed roof hatch seen from below: the lid at the top of its well, its seal, two hinges, the handle and the
    locking bar (shapes: estimate; usaf_shaw_2017 -036 shows the lid and its rounded opening). rim: the roof's top."""
    lid = rrect(x, -y, 0.53, 0.58, 0.06, 5)
    b.slab((0, 0, rim + 0.01), Vector((1, 0, 0)), Vector((0, -1, 0)), list(reversed(lid)), [], 0.03, 'Paint_Interior')
    b.tube([(x - 0.16, y - 0.05, rim + 0.01), (x - 0.16, y - 0.05, rim - 0.04),
            (x + 0.16, y - 0.05, rim - 0.04), (x + 0.16, y - 0.05, rim + 0.01)], 0.011, 'Metal_Steel')
    b.box(x - 0.2, x + 0.2, y + 0.12, y + 0.14, rim - 0.012, rim + 0.01, 'Metal_Dark')
    for hx in (x - 0.18, x + 0.18):
        b.box(hx - 0.04, hx + 0.04, y + 0.27, y + 0.33, rim - 0.09, rim + 0.01, 'Metal_Dark')
    ring = rrect(x, -y, 0.55, 0.60, 0.07, 5)
    seal(b, Vector((0, 0, rim + 0.012)), Vector((1, 0, 0)), Vector((0, -1, 0)), ring, 'Rubber_Black', width=0.018, depth=0.015)


def door_outline(d, grow=0.0):
    """The door's rounded outline on the bulkhead as (x, height above the floor), `grow` outwards from its seal."""
    x = d['x_outboard_m'] - d['width_m'] / 2
    h = d['z_top_m'] - d['height_m'] / 2 - FLOOR_Z
    return rrect(x, h, d['width_m'] + 2 * grow, d['height_m'] + 2 * grow, d['corner_r_m'] + grow, 5)


def build_bulkhead(b, shell, rear, T):
    """The rear bulkhead's face from the photos: the opening, its frame, the closed door with its pressings and fittings,
    the ledge, rib and fairing, and the lower step with its flap. Every piece is a closed solid."""
    bh = shell['bulkhead']
    d = bh['door']
    u, v = Vector((-1, 0, 0)), Vector((0, 0, 1))

    def wall(pts):                                   # (x, height) to the wall plate's (a, b)
        return [(-x, h) for x, h in reversed(pts)]

    b.slab((0, REAR_Y, FLOOR_Z), u, v, [(-x, h) for x, h in rear], [wall(door_outline(d))], T, 'Paint_Interior')
    fr = bh['door_frame']
    b.slab((0, REAR_Y + fr['proud_m'], FLOOR_Z), u, v, wall(door_outline(d, fr['width_m'])), [wall(door_outline(d, 0.004))],
           fr['proud_m'] + 0.005, 'Paint_Interior')
    # The dark seal round the door, closing the gap, then the door's plate just inside it.
    b.slab((0, REAR_Y - 0.006, FLOOR_Z), u, v, wall(door_outline(d, 0.004)), [wall(door_outline(d, -0.012))], 0.014, 'Rubber_Black')
    face = REAR_Y - 0.004
    b.slab((0, face, FLOOR_Z), u, v, wall(door_outline(d, -0.006)), [], 0.03, 'Paint_LightGrey')   # paler than the wall: g100-a d25
    fit = bh['door_fittings']
    edge, door_top = d['x_outboard_m'], d['z_top_m']
    x0, x1 = edge - fit['channel_from_edge_m'][1], edge - fit['channel_from_edge_m'][0]
    hh = fit['channel_height_m']
    for t in fit['channels_from_top_m']:
        z = door_top - t
        ch = rrect((x0 + x1) / 2, z - FLOOR_Z, x1 - x0, hh, hh * 0.48, 5)
        b.slab((0, face + 0.008, FLOOR_Z), u, v, wall(ch), [], 0.012, 'Paint_Interior')
        for k in range(5):
            x = x0 + (x1 - x0) * (0.07 + 0.215 * k)
            b.cylinder((x, face + 0.007, z), (x, face + 0.012, z), 0.0065, 'Metal_Dark', seg=8)
    for a, c, t0, t1 in fit['hinge_pads_from_edge_top_m']:
        b.box(edge - c, edge - a, face - 0.002, face + 0.005, door_top - t1, door_top - t0, 'Paint_Interior')
    a, c, t0, t1 = fit['pocket_from_edge_top_m']
    pk = rrect(edge - (a + c) / 2, door_top - (t0 + t1) / 2 - FLOOR_Z, c - a, t1 - t0, 0.03, 5)
    b.slab((0, face + 0.006, FLOOR_Z), u, v, wall(pk), [], 0.01, 'Paint_Interior')
    a, c = fit['strap_from_edge_m']
    z = door_top - fit['strap_from_top_m']
    b.box(edge - c, edge - a, face + 0.02, face + 0.032, z - 0.017, z + 0.017, 'Metal_Steel')
    for x in (edge - c + 0.012, edge - a - 0.012):
        b.box(x - 0.012, x + 0.012, face - 0.002, face + 0.022, z - 0.017, z + 0.017, 'Metal_Steel')
    for a, t0, c, t1 in fit['dogs_from_edge_top_m']:
        p0 = (edge - a, face + 0.03, door_top - t0)
        b.cylinder((p0[0], face - 0.002, p0[2]), p0, 0.022, 'Paint_Interior', seg=12)
        b.tube([p0, (edge - c, face + 0.03, door_top - t1)], 0.011, 'Paint_Interior')
    le = bh['ledge']
    b.box(le['x_from_m'], le['x_to_m'], REAR_Y - 0.002, REAR_Y + le['proud_m'], le['z_bottom_m'], le['z_top_m'], 'Paint_Interior')
    ri = bh['rib']
    b.box(ri['x_m'] - ri['half_width_m'], ri['x_m'] + ri['half_width_m'], REAR_Y - 0.002, REAR_Y + ri['proud_m'],
          ri['z_from_m'], ri['z_to_m'], 'Paint_Interior')
    fa = bh['fairing']
    xi, xw = fa['x_inboard_m'], fa['x_wall_m']
    r = xw - xi
    arc = [(xi + r * math.cos(math.radians(a)), REAR_Y + r + r * math.sin(math.radians(a))) for a in range(0, -91, -15)]
    b.slab((0, 0, fa['z_to_m']), Vector((1, 0, 0)), Vector((0, 1, 0)), [(xw, REAR_Y - 0.002)] + list(reversed(arc)) + [],
           [], fa['z_to_m'] - FLOOR_Z, 'Paint_Interior')
    st = bh['lower_step']
    front = REAR_Y + st['proud_m']
    b.box(-WALL_X, WALL_X, REAR_Y - 0.002, front, FLOOR_Z, st['z_top_m'], 'Paint_Interior')
    fl = bh['flap']
    b.box(fl['x_from_m'], fl['x_to_m'], front - 0.002, front + fl['proud_m'], fl['z_from_m'], fl['z_to_m'], 'Paint_Equipment')
    y_h = front + fl['proud_m']
    b.cylinder((fl['x_from_m'], y_h, fl['z_to_m']), (fl['x_to_m'], y_h, fl['z_to_m']), 0.004, 'Metal_Brass', seg=8)
    n = fl['hinge_knuckles']
    pitch = (fl['x_to_m'] - fl['x_from_m']) / n
    for k in range(n):
        b.cylinder((fl['x_from_m'] + k * pitch + 0.006, y_h, fl['z_to_m']), (fl['x_from_m'] + (k + 1) * pitch - 0.006, y_h, fl['z_to_m']),
                   0.008, 'Paint_Equipment', seg=10)
    build_bulkhead_details(b, bh, face, front)


def build_bulkhead_details(b, bh, face, front):
    """What the square-on photo g100-a shows on and beside the door: the plates, latches and screws, the hood and the blower."""
    dd = bh['door_detail']

    def plate(q, y0, y1, mat):
        b.box(q[0], q[1], y0, y1, q[2], q[3], mat)

    def screw(x, z, y, r=0.0065, mat='Metal_Dark', h=0.005):
        b.cylinder((x, y - 0.001, z), (x, y + h, z), r, mat, seg=8, smooth=False)

    q = dd['red_plate_xz_m']
    plate(q, face - 0.001, face + 0.002, 'Paint_Letters')                                  # its thin white border (the trace's outline)
    plate([q[0] + 0.004, q[1] - 0.004, q[2] + 0.004, q[3] - 0.004], face + 0.001, face + 0.0035, 'Paint_Red')
    for x, z in ((q[0] + 0.008, q[2] + 0.008), (q[1] - 0.008, q[2] + 0.008), (q[0] + 0.008, q[3] - 0.008), (q[1] - 0.008, q[3] - 0.008)):
        screw(x, z, face + 0.003, 0.004, 'Metal_Steel', 0.003)
    q = dd['black_plate_xz_m']
    plate(q, face - 0.001, face + 0.003, 'Bakelite_Black')
    for x, z in ((q[0] + 0.006, q[2] + 0.006), (q[1] - 0.006, q[2] + 0.006), (q[0] + 0.006, q[3] - 0.006), (q[1] - 0.006, q[3] - 0.006)):
        screw(x, z, face + 0.002, 0.0035, 'Metal_Steel', 0.003)
    for q in dd['latch_plates_xz_m']:
        plate(q, face - 0.001, face + 0.012, 'Paint_LightGrey')
    for q in dd['keepers_xz_m']:
        plate(q, REAR_Y - 0.002, REAR_Y + 0.03, 'Paint_Interior')
    for (x0, z0), (x1, z1) in dd['levers_xz_m']:
        b.tube([(x0, face + 0.034, z0), (x1, face + 0.034, z1)], 0.009, 'Paint_LightGrey')
    for x, z in dd['screws_xz_m']:
        screw(x, z, face + 0.004)
    for x, z in dd['studs_xz_m']:
        screw(x, z, face, 0.006, 'Paint_LightGrey', 0.008)
    h = bh['hood']
    y0, y1 = REAR_Y - 0.002, REAR_Y + h['depth_m']
    zb, zt = h['z_bottom_m'], h['z_top_m']
    b.prism([(y0, zb), (y1, zb), (y1, zb + h['front_height_m']), (y0, zt)], h['x_from_m'], h['x_to_m'], 'Duct_Grey')
    q = h['cover_xz_m']
    slope = (zb + h['front_height_m'] - zt) / h['depth_m']
    yc = (zb + h['front_height_m'] - (q[2] + q[3]) / 2) / slope
    yc = max(0.012, min(h['depth_m'], h['depth_m'] - yc))

    def on_hood(z):
        """The sloped face's y at height z, or the front's, whichever stands further out."""
        zf = zb + h['front_height_m']
        if z <= zf:
            return y1
        return REAR_Y + h['depth_m'] * (zt - z) / (zt - zf)

    b.box(q[0], q[1], on_hood(q[2]) - 0.01, on_hood(q[2]) + 0.006, q[2], q[3], 'Duct_Grey')
    for x, z in h['screws_xz_m']:
        screw(x, z, on_hood(z), 0.005, 'Metal_Brass', 0.008)
    b.tube([(x, y1 + h['tube_r_m'], max(z, zb + 0.01)) for x, z in h['tube_xz_m']], h['tube_r_m'], 'Duct_Grey')
    bw = bh['blower']
    x0, x1, z0, z1, fr, pr = bw['x_from_m'], bw['x_to_m'], bw['z_from_m'], bw['z_to_m'], bw['frame_m'], bw['proud_m']
    y_f = front + pr
    b.box(x0 + fr, x1 - fr, front - 0.004, front + 0.002, z0 + fr, z1 - fr, 'Rubber_Black')                  # the dark recess
    for q in ((x0, x1, z1 - fr, z1), (x0, x1, z0, z0 + fr), (x0, x0 + fr, z0, z1), (x1 - fr, x1, z0, z1)):
        b.box(q[0], q[1], REAR_Y - 0.002, y_f, q[2], q[3], 'Paint_LightGrey')
    cx, cz = bw['ring_centre_xz_m']
    b.cylinder((cx, front, cz), (cx, front + 0.012, cz), bw['ring_r_m'], 'Rubber_Black', seg=28, smooth=False)
    b.cylinder((cx, front + 0.011, cz), (cx, front + 0.03, cz), bw['hub_r_m'], 'Paint_Panel', seg=24)
    q = bw['foot_plate_xz_m']
    plate([max(q[0], x0 + fr), min(q[1], x1 - fr), q[2], q[3]], front, front + 0.008, 'Paint_LightGrey')
    for x, z in bw['screws_xz_m']:
        screw(x, z, front, 0.005, 'Metal_Brass', 0.008)


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
        pane = rrect(WINDSCREEN[s][0], SCREEN_CENTRE_S, WINDSCREEN[s][1], SCREEN_H, 0.09, 6)
        b.slab(o - n * (THICK * 0.5), right, up, pane, [], 0.008, 'Glass')
    for s in (-1, 1):
        a0 = Vector((s * WALL_X, CAB_Y, FLOOR_Z))
        a1 = Vector((s * BOW_X, BOW_Y, FLOOR_Z))
        along = (a1 - a0)
        length = along.length
        along.normalize()
        uu, origin, at = (-along, a1, 0.58) if s > 0 else (along, a0, 0.42)
        nn = uu.cross(Vector((0, 0, 1))).normalized()
        pane = rrect(length * at, SIDE_WINDOW_Z - FLOOR_Z, 0.46, 0.38, 0.07, 5)
        b.slab(origin - nn * (THICK * 0.5), uu, Vector((0, 0, 1)), pane, [], 0.008, 'Glass')
    return finish(b)


# ---------------------------------------------------------------------------------------------- the racks

def build_rack(name, layout, panel_ids, side, posts, lower, extra=None, stops_at_frame=False):
    """A rack's carcass behind its console panels: the cabinet back to the side wall, the frame round the panels
    (posts, top and bottom rails, column dividers), and below the blocks either a recessed kick panel with a foot
    grating (lower='kick'), closed doors flush with the frame (lower='doors'), or a bare wall (lower='wall'). A rack that
    stops_at_frame ends at its frame's top rail; what is above it is its `extra`'s."""
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

    top = zt + 0.03 if stops_at_frame else ROOF_Z
    # the cabinet behind the panels (its front 3.5 cm behind the panel plane, where the game draws the panels'
    # dark backing)
    b.box(X(-0.035), wall, ylo, yhi, zb - 0.03, top, 'Paint_Rack')
    # frame: top rail, posts, bottom rail, dividers between columns, 4 mm behind the plates' faces
    fr0, fr1 = X(-0.035), X(-0.004)
    b.box(fr0, fr1, y0, y1, zt, top, 'Paint_Rack')
    for a, c in posts:
        b.box(fr0, fr1, a, c, zb - 0.03, top, 'Paint_Rack')
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
        bench_edge = BENCH[0] if side < 0 else 0.541
        d = -0.12
        while True:
            xx = X(d)
            if (xx - bench_edge) * n_x >= -0.01:
                break
            b.box(xx, X(d + 0.012), y0 + 0.02, y1 - 0.02, FLOOR_Z, FLOOR_Z + 0.022, 'Grate_Rubber')
            d += 0.028
        for yy in (y0 + 0.01, y1 - 0.01, (y0 + y1) / 2):
            b.box(X(-0.12), X(d), yy - 0.008, yy + 0.008, FLOOR_Z, FLOOR_Z + 0.016, 'Metal_Dark')
    elif lower == 'wall':
        # no doors under the rack (g091-d, g090-a): a bare wall in the old doors' plane, the R-124 boxes on it
        # raking in to the seat deck's side, then down to the floor (data/cabins/osa_ak_shell.json lower_side_wall)
        deck_x = side * load_shell()['lower_side_wall']['forward']['deck_x_m']
        sec = [(X(-0.012), zb - 0.03), (wall, zb - 0.03), (wall, FLOOR_Z), (deck_x, FLOOR_Z), (deck_x, FLOOR_Z + 0.36)]
        b.prism_y(sec if side < 0 else list(reversed(sec)), ylo, yhi, 'Paint_Interior')
    else:
        # closed lower cabinet: door panels between the posts, a plinth at the floor
        b.box(X(-0.035), wall, ylo, yhi, FLOOR_Z, zb - 0.03, 'Paint_Rack')
        b.box(X(-0.06), X(-0.035), ylo, yhi, FLOOR_Z, FLOOR_Z + 0.06, 'Paint_Rack')
        doors = lower if isinstance(lower, list) else [(y0, y1)]
        top, bot = zb - 0.045, FLOOR_Z + 0.07
        h1 = top - min(0.075, 0.25 * (top - bot))      # the handles 12 cm long, on the door however short it is
        h0 = max(bot + 0.025, h1 - 0.12)
        for a, c in doors:
            b.box(X(-0.035), X(-0.012), a + 0.012, c - 0.012, bot, top, 'Paint_Rack')
            for yy in (a + 0.06, c - 0.06):
                b.tube([(X(-0.012), yy, h1), (X(0.02), yy, h1), (X(0.02), yy, h0),
                        (X(-0.012), yy, h0)], 0.006, 'Metal_Steel', seg=10)
    if extra:
        extra(b, X)
    return finish(b, bevel=0.003)


def build_rack_sp3m(layout):
    """SP-3M-2's carcass, from its trace (sp3m_trace.py) and the joint solve: the top row flush with the face, the instrument blocks in a
    recess 147.6 mm behind it, no doors below (g098-a, b: bare wall under the ribbed shelf). The post by SP-2M2 keeps its place."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import sp3m_trace as T
    p = next(q for q in layout['panels'] if q['id'] == 'sp3m_panel')
    face, py, pz = p['centre_m']
    b = Builder('SM_RackSP3M')

    def Y(x_mm):               # face mm to the right -> body y (the panel faces -x, its right is -y)
        return py + (T.W_MM / 2.0 - x_mm) * 0.001

    def Z(y_mm):               # face mm down -> body z
        return pz + (T.H_MM / 2.0 - y_mm) * 0.001

    y_channel = Y(T.CHANNEL_LEFT[0])
    y0, y1 = Y(T.OPENING[2]), Y(T.OPENING[0])      # the recess's mouth, aft and forward
    z0, z1 = Z(T.OPENING[3]), Z(T.OPENING[1])
    back = face + (T.D_LOWER + 22.0) * 0.001
    lower_front = face + 0.04                       # behind the ribbed shelf's teeth
    aft = REAR_Y
    b.box(face + 0.004, WALL_X, 0.485, 0.605, FLOOR_Z, ROOF_Z, 'Paint_Rack')          # the post by SP-2M2 (g098-a)
    b.box(face + 0.035, WALL_X, y_channel, 0.485, FLOOR_Z, ROOF_Z, 'Paint_Rack')      # wall between the post and the channel
    z_plate = Z(-2.0)                                                                  # the white plate's top, its rail above
    b.box(face - 0.001, WALL_X, aft, y_channel, z_plate, ROOF_Z, 'Paint_Rack')        # above the plate, the frame 1 mm proud of the panel plane
    b.box(face + 0.004, WALL_X, aft, y_channel, z1, z_plate, 'Paint_Rack')            # behind the plate and the drawer, clear of their backs
    b.box(face, WALL_X, y1, y_channel, z0, z1, 'Paint_Rack')                          # forward of it
    b.box(face, WALL_X, aft, y0, z0, z1, 'Paint_Rack')                                # aft of it
    b.box(back, WALL_X, y0, y1, z0, z1, 'Paint_Rack')                                 # its back
    foot = load_shell()['lower_side_wall']['aft_right']['x_foot_m']                    # the wall below, raking in to the floor
    b.prism_y([(foot, FLOOR_Z), (WALL_X, FLOOR_Z), (WALL_X, z0), (lower_front, z0)], aft, y_channel, 'Paint_Interior')
    return finish(b, bevel=0.002)


def build_rack_sl3m(layout):
    """SŁ-3M's carcass behind its traced face (sl3m_trace.py): the cabinet to the wall, the post by SŁ-2M where it stood, a plain panel between
    the face's frame and the post; below, the ledge g099-a and b show instead of the closed doors the cabin had from Fig. 3."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import sl3m_trace as T
    p = next(q for q in layout['panels'] if q['id'] == 'sl3m_panel')
    face, py, pz = p['centre_m']
    b = Builder('SM_RackSL3M')

    def Y(x_mm):               # face mm to the right -> body y (the panel faces +x, its right is +y)
        return py + (x_mm - T.W_MM / 2.0) * 0.001

    def Z(y_mm):               # face mm down -> body z
        return pz + (T.H_MM / 2.0 - y_mm) * 0.001

    wall = -WALL_X
    aft, post0, post1 = REAR_Y, 0.415, 0.535   # the post against SŁ-2M's aft end (g094-a: the range column's plates run to y 0.539)
    y_fwd = Y(T.FRAME_RIGHT[1])
    z_top, z_bot = Z(T.FRAME_TOP[0]), Z(T.FRAME_BOTTOM[1])
    b.box(wall, face - 0.026, aft, post0, z_bot, ROOF_Z, 'Paint_Rack')                # the cabinet, just behind the face's back sheet
    if z_top < ROOF_Z - 0.001:
        b.box(wall, face - 0.001, aft, y_fwd, z_top, ROOF_Z, 'Paint_Rack')            # above the face's frame
    b.box(wall, face - 0.012, y_fwd, post0, z_bot, ROOF_Z, 'Paint_Rack')              # the plain panel to the post
    b.box(wall, face - 0.004, post0, post1, FLOOR_Z, ROOF_Z, 'Paint_Rack')            # the post by SŁ-2M
    # below it the plain wall, and the ledge the rack stands on (g099-a, b: its top meets the foot of the bottom strip; at least 116 mm deep by g099-a's ray,
    # 160 mm SAM3D's, how far down its front runs hidden behind the seats)
    front = face - 0.035
    ledge = z_bot - 0.21
    foot = front + load_shell()['lower_side_wall']['rake_x_per_z'] * (ledge - FLOOR_Z)   # under the ledge the wall rakes in to the floor
    b.box(wall, front, aft, post0, ledge, z_bot, 'Paint_Interior')
    b.prism_y([(wall, ledge), (wall, FLOOR_Z), (foot, FLOOR_Z), (front, ledge)], aft, post0, 'Paint_Interior')
    b.box(front, face + 0.16, aft, post0, z_bot - 0.21, z_bot, 'Paint_Interior')
    b.box(face + 0.152, face + 0.16, aft, post0, z_bot, z_bot + 0.008, 'Paint_Interior')                # the turned-up lip at its front
    return finish(b, bevel=0.002)


def build_chest_5937():
    """The stencilled stowage tin hung on the wall under SP-3M-2 (wwp_gecko g098-a, b, c, session A). Re-measured 11 Nov 2026 on the joint
    solve's points that g092-a, g096-a and g098-a-c see on it, through the certified cameras: its lid's inboard edge at x 0.706-0.718 and
    z 1.47-1.49, its forward end at y 0.09-0.14 (it had been 0.35 m aft, moved only outboard with the right racks); its length and depth are
    the first triangulation's (0.49, 0.24); how deep it runs back to the wall is an estimate."""
    b = Builder('SM_Chest5937')
    x0, x1, y0, y1, z0, z1 = 0.715, 0.956, -0.39, 0.10, 1.32, 1.45
    cx, cy, w, d = (x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0
    plan = rrect(cx, cy, w, d, 0.04)
    def layer(grow, top, t, mat):
        b.slab((0, 0, top), (1, 0, 0), (0, 1, 0), rrect(cx, cy, w + 2 * grow, d + 2 * grow, 0.04 + grow), [], t, mat)
    b.slab((0, 0, z1), (1, 0, 0), (0, 1, 0), plan, [], z1 - z0, 'Paint_Interior')
    for zb in (z0 + 0.04, z0 + 0.085):      # the two pressed beads round its body
        layer(0.004, zb + 0.005, 0.01, 'Paint_Interior')
    layer(0.008, z1 + 0.024, 0.024, 'Paint_Interior')           # the lid and its rim
    for y in (y0 + 0.371, y0 + 0.124):      # the two hasps
        b.box(x0 - 0.016, x0 - 0.008, y - 0.009, y + 0.009, z0 + 0.085, z1 + 0.03, 'Metal_Steel')
    return finish(b, bevel=0.002)


def build_first_aid():
    """The first-aid box under SP-3M-2's aft end, leaning back (wwp_gecko g098-a, c, session A): its top edge triangulated, its foot cast through
    g098-a's solved camera on the plane that makes it a rectangle (20 degrees back). Moved 35 mm toward the wall, clear of the crew's walk.
    The hangar BM3-1 (g092-a, g096-a) carries it elsewhere: it is loose kit in a strap holder."""
    b = Builder('SM_FirstAid')
    tl, tr = Vector((0.504 + RIGHT_SHIFT_X, 0.100, 1.360)), Vector((0.490 + RIGHT_SHIFT_X, -0.159, 1.329))
    bl, br = Vector((0.427 + RIGHT_SHIFT_X, 0.137, 1.145)), Vector((0.420 + RIGHT_SHIFT_X, -0.119, 1.133))
    thick = 0.09
    fitted = load_parts().get('first_aid')          # fitted to its outline in two photographs (tools/recon/osa/recipes/right_kit.py): the box's front face carries what follows
    if fitted:
        import numpy as np
        sys.path.insert(0, os.path.join(REPO, 'tools', 'recon', 'osa'))
        import parts as osa_parts
        box = osa_parts.from_dict(fitted['shape'])
        R, size = box.rotation(), box.size
        k = int(np.argmin(size))
        n_ = R[:, k] * (-1.0 if R[0, k] > 0 else 1.0)                       # its front faces the aisle
        rest = [i for i in range(3) if i != k]
        vi = max(rest, key=lambda i: abs(R[2, i]))
        ui = [i for i in rest if i != vi][0]
        v_ = R[:, vi] * (1.0 if R[2, vi] > 0 else -1.0)
        u_ = np.cross(v_, n_)
        c_ = box.centre + n_ * size[k] / 2
        tl, tr = Vector(c_ - u_ * size[ui] / 2 + v_ * size[vi] / 2), Vector(c_ + u_ * size[ui] / 2 + v_ * size[vi] / 2)
        bl, br = Vector(c_ - u_ * size[ui] / 2 - v_ * size[vi] / 2), Vector(c_ + u_ * size[ui] / 2 - v_ * size[vi] / 2)
        thick = float(size[k])
    # 11 Nov 2026: no one move fits the certified cameras: g098-a's red cross wants it about 0.1 m inboard, which g092-a and g096-a (the
    # hangar session) refuse, and the forward move the edge scores liked takes the cross further from g098-a's. Loose kit, placed
    # differently between the sessions; left as fitted (docs/OSA_MODELLING_HANDOFF.md section 12)
    centre = (tl + tr + bl + br) / 4
    u = (tr - tl).normalized()
    side = (tl + tr - bl - br)
    v = (side - u * side.dot(u)).normalized()
    n = u.cross(v).normalized()
    W_, H_ = (tl - tr).length, (side / 2).length
    b.slab(tuple(centre), u, v, rrect(0, 0, W_, H_, 0.03), [], thick, 'Paint_Letters')
    b.slab(tuple(centre - n * 0.035), u, v, rrect(0, 0, W_ + 0.004, H_ + 0.004, 0.032), [], 0.004, 'Metal_Steel')   # the seam round the lid
    r0, r1 = 0.082, 0.078                   # the red ring and its cross
    b.slab(tuple(centre + n * 0.0008), u, v, rrect(0, 0, 2 * r0, 2 * r0, r0 - 1e-4, seg=16), [rrect(0, 0, 2 * r1, 2 * r1, r1 - 1e-4, seg=16)], 0.0008, 'Paint_Red')
    for size in ((0.116, 0.038), (0.038, 0.116)):
        b.obox(tuple(centre + n * 0.0005), u, v, n, (size[0], size[1], 0.001), 'Paint_Red')
    # the latch on its forward side, and the black strap and steel bracket that hold it to the wall
    b.obox(tuple(centre - u * (W_ / 2 + 0.004) - n * 0.04), v, n, u, (0.03, 0.02, 0.012), 'Metal_Steel')
    top = (tl + tr) / 2
    b.tube([tuple(top - n * thick + v * 0.002 + Vector((0.2, 0, 0))), tuple(top + v * 0.012 - n * thick), tuple(top + v * 0.012 + n * 0.003), tuple(top - v * 0.05 + n * 0.003)],
           0.009, 'Rubber_Black', seg=6)
    back = centre - n * thick
    b.box(back.x - 0.004, back.x + 0.24, back.y - 0.02, back.y + 0.02, back.z - 0.015, back.z + 0.015, 'Metal_Steel')
    return finish(b, bevel=0.002)


RADIO_BOX_TOP = 1.455    # ZCzZ1A nr 2's lid, the right R-123M's frame on it (wwp_gecko g090-a, g090-c; H3, 9 Nov 2026)
RADIO_LEFT_FRAME_TOP = FLOOR_Z + 0.42   # the left R-123M's floor frame in its well under SŁ-1M (wwp_gecko g091-d)
LEFT_TRAY_Z = 1.35      # the steel tray over the left R-123M (g091-d; the book's caption: the radios' steel covers)


def prism(b, X, ya, yb, section, mat, t=0.004):
    """A folded sheet along y: its section a list of (depth from the panels' plane, z)."""
    u = Vector((0, 1 if yb > ya else -1, 0))
    L = abs(yb - ya)
    ym = (ya + yb) / 2
    for (d0, z0), (d1, z1) in zip(section, section[1:]):
        p0, p1 = Vector((X(d0), ym, z0)), Vector((X(d1), ym, z1))
        v = (p1 - p0)
        h = v.length
        v = v / h
        b.obox((p0 + p1) * 0.5, u, v, u.cross(v).normalized(), (L, h, t), mat)


def rack_brow(b, X, side, top, ya, yb):
    """What is above a front rack, once its frame ends (g091-a; g090-a, g090-d on the right): a recess about 75 mm tall, set back 14 cm,
    under a brow of lining that comes out nearly to the panels' plane and leans back up to the roof. By eye, about +-3 cm."""
    wall = side * WALL_X
    f = min(1.0, (ROOF_Z - 0.005 - top) / 0.195)      # the same section, shut up under a roof that is nearer
    b.box(X(-0.14), wall, ya, yb, top, ROOF_Z, 'Paint_Rack')
    prism(b, X, ya, yb, [(0.0, top), (-0.14, top), (-0.14, top + 0.075 * f)], 'Paint_Interior')
    prism(b, X, ya, yb, [(-0.14, top + 0.075 * f), (-0.03, top + 0.085 * f), (-0.022, top + 0.125 * f), (-0.06, top + 0.17 * f), (-0.22, ROOF_Z - 0.012)], 'Paint_Interior')


def sl1m_top(b, X):
    """Over SŁ-1M (g091-a): the brow, the big pipe along its top going back to the roof, the cream cable on saddle clips, the small black
    plate, and the hatch's hanging arm in front of the rack's forward end. Places by eye along the rack's width, about +-5 cm."""
    top = FRONT_RACK_TOP_Z
    rack_brow(b, X, -1, top, 2.505, 4.06)
    pipe = [(X(-0.06), 3.99, 2.04), (X(-0.06), 3.55, 2.04), (X(-0.06), 3.48, 2.035), (X(-0.16), 3.12, 1.99), (X(-0.30), 2.92, 1.94)]
    b.tube(pipe, 0.042, 'Paint_Interior', seg=16)
    b.cylinder((X(-0.06), 3.54, 2.035), (X(-0.06), 3.49, 2.035), 0.05, 'Paint_Interior', seg=20)
    z_cable = top + 0.08
    b.tube([(X(-0.018), 3.66, z_cable + 0.01), (X(-0.02), 3.40, z_cable + 0.008), (X(-0.02), 3.05, z_cable - 0.005), (X(-0.02), 2.62, z_cable - 0.02)],
           0.0035, 'Paint_LightGrey', seg=8)
    for yy, zz in ((3.60, z_cable + 0.01), (3.42, z_cable + 0.008), (3.22, z_cable + 0.002), (3.02, z_cable - 0.006), (2.82, z_cable - 0.012)):
        b.box(X(-0.022), X(-0.012), yy - 0.008, yy + 0.008, zz - 0.009, zz + 0.009, 'Metal_Steel')
    b.obox((X(-0.03), 3.39, top + 0.09), Vector((0, -1, 0)), Vector((0.05, 0, 1)).normalized(), Vector((1, 0, -0.05)).normalized(), (0.08, 0.06, 0.003), 'Bakelite_Black')
    arm0, arm1 = Vector((X(-0.06), 3.74, 2.035)), Vector((X(0.06), 3.58, 1.95))
    b.cylinder((X(-0.09), 3.74, 2.035), (X(-0.04), 3.74, 2.035), 0.05, 'Metal_Steel', seg=20)
    b.cylinder(arm0, arm1, 0.013, 'Paint_Interior', seg=12)
    b.box(X(-0.06), X(-0.03), 3.76, 3.82, 2.0, 2.05, 'Paint_Equipment')


def sp1m_top(b, X):
    """Over SP-1M: the same brow, mirrored; nothing else from the photos yet."""
    rack_brow(b, X, 1, FRONT_RACK_TOP_Z, 2.505, 4.06)


def sl2m_extra(b, X):
    """SŁ-2M's frame from g094-a: a top rail with a chamfer standing proud of the faces, the posts between the columns standing proud
    of them, a black name plate on the rail over the middle column. Rail height and widths by eye, about +-5 mm; the depth SAM3D's."""
    ya, yb = 0.535, 2.505
    b.box(X(-0.004), X(0.012), ya, yb, 2.075, ROOF_Z, 'Paint_Rack')
    prism(b, X, ya, yb, [(0.012, 2.075), (0.018, 2.068), (0.0, 2.062)], 'Paint_Rack', t=0.003)
    for yy in (1.8925, 1.2175):
        b.box(X(-0.004), X(0.012), yy - 0.012, yy + 0.012, 1.108, 2.075, 'Paint_Rack')
    b.box(X(0.012), X(0.0135), 1.50, 1.61, 2.078, 2.096, 'Bakelite_Black')


def sl2m_housings(b, layout):
    """SŁ-2M's raised housings round its displays (tools/blender/sl2m_housings.py: what, where and why): each a ring of
    loops lofted from its foot on the plate, round its front edge and into its hole, or a visor swept over a dial's top,
    or a flat plate. The foot 2 mm into the block plate, so no gap shows."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import sl2m_housings as H
    panels = {p['id']: p for p in layout['panels']}
    foot = -0.002

    for h in H.HOUSINGS:
        c0, n, r, up = panel_frame(panels[h['panel']])

        def at(u, v, d):
            return c0 + r * u + up * v + n * d

        cx, cy = H.centre(h, layout)
        hx, hy = H.hole_centre(h, layout)
        D, mat = h['depth'], h['mat']
        if h['kind'] == 'ring':
            ro, rh = h['outer'] / 2, h['hole'] / 2
            cw, cd = h.get('cone', (0.012, 0.009))
            if h['style'] == 'flange':     # a drum, its outer edge chamfered, its face sloping in to the screen as a cone
                prof = [(ro, foot), (ro, D - 0.004), (ro - 0.004, D), (rh + cw, D), (rh + cw - 0.002, D - 0.002),
                        (rh + 0.001, D - cd), (rh, D - cd), (rh, foot)]
            else:                          # knurl: a collar at the foot, the knurled drum, a rounded lip
                prof = [(ro, foot), (ro, 0.006), (ro - 0.003, 0.008), (ro - 0.003, D - 0.004), (ro - 0.006, D),
                        (rh + 0.004, D), (rh, D - 0.006), (rh, foot)]
            loops = [[at(u, v, d) for u, v in H.circle(cx, cy, rr, 64)] for rr, d in prof]
            if h['style'] == 'flange':     # the drum in its paint, the scale cone and the tube's throat dark
                loft_loops(b, loops[:4], mat)
                loft_loops(b, loops[3:], 'Metal_Dark')
            else:
                loft_loops(b, loops, mat)
            if h['style'] == 'flange':
                rs = (ro + rh + cw) / 2
                for k in range(h.get('screws', 0)):
                    a = 2 * math.pi * (k + 0.5) / h['screws']
                    p = at(cx + rs * math.cos(a), cy + rs * math.sin(a), 0.0)
                    b.cylinder(p + n * (D - 0.0005), p + n * (D + 0.0012), 0.0034, 'Metal_Steel', seg=10)
                for k in range(h.get('knobs', 0)):
                    a = math.pi / 2 + 2 * math.pi * k / h['knobs']
                    p = at(cx + (ro - 0.009) * math.cos(a), cy + (ro - 0.009) * math.sin(a), 0.0)
                    b.cylinder(p + n * (D - 0.001), p + n * (D + 0.009), 0.0055, 'Paint_Red', seg=12)
            else:
                rk = ro - 0.003
                for k in range(40):
                    a = 2 * math.pi * k / 40
                    radial = r * math.cos(a) + up * math.sin(a)
                    tang = -r * math.sin(a) + up * math.cos(a)
                    mid = at(cx, cy, 0.0) + radial * rk + n * ((0.009 + D - 0.005) / 2)
                    b.obox(mid, tang, n, radial, (0.0026, D - 0.014, 0.003), mat)
        elif h['kind'] == 'arc':
            a0, a1 = h['arc_deg']
            ri, ro = h['inner'] / 2, h['outer'] / 2
            visor(b, at, cx, cy, ri, ro, foot, D, a0, a1, mat)
        elif h['style'] == 'dial_plate':
            outline = H.outer_outline(h, layout)
            b.slab(at(0, 0, D), r, up, outline, [], D - foot, mat)
            du, dv = h['dial']['at']
            rd = h['dial']['d'] / 2
            p = at(cx + du, cy + dv, 0.0)
            b.cylinder(p + n * (D - 0.001), p + n * (D + 0.001), rd, 'Metal_Dark', seg=40)
            b.cylinder(p + n * (D + 0.001), p + n * (D + 0.004), 0.006, 'Metal_Steel', seg=16)   # the pointer's boss
            visor(b, at, cx + du, cy + dv, rd + 0.001, rd + 0.005, D - 0.001, D + 0.03, 8, 172, mat)
        else:
            w, ht, rr0 = h['outer'][0], h['outer'][1], h.get('outer_r', 0.0)
            hw, hh, hr = h['hole'][0], h['hole'][1], h.get('hole_r', 0.0)
            edge = {'hood': 0.010, 'cast': 0.006, 'frame': 0.002}[h['style']]
            lip, bevel = {'hood': (0.006, 0.006), 'cast': (0.008, 0.009), 'frame': (0.001, 0.002)}[h['style']]
            prof = [('o', 0.0, foot)]
            for k in range(5):            # the front edge rounded, a quarter circle of radius `edge`
                a = math.radians(90.0 * k / 4)
                prof.append(('o', edge * (1 - math.cos(a)), D - edge + edge * math.sin(a)))
            prof += [('h', lip, D), ('h', 0.0, D - bevel), ('h', 0.0, foot)]
            loops = []
            for side, off, d in prof:
                pts = (H.rrect(cx, cy, w - 2 * off, ht - 2 * off, rr0 - off, 8) if side == 'o'
                       else H.rrect(hx, hy, hw + 2 * off, hh + 2 * off, hr + off, 8))
                loops.append([at(u, v, d) for u, v in pts])
            loft_loops(b, loops, mat)


def loft_loops(b, loops, mat, smooth=False):
    """Quads between successive closed loops of body points, all anticlockwise seen from the front and of one length.
    Run from the outer foot over the front into the hole, the faces look outwards."""
    idx = b.mat(mat)
    rows = [[b.bm.verts.new(P(*p)) for p in lp] for lp in loops]
    for r0, r1 in zip(rows, rows[1:]):
        m = len(r0)
        for k in range(m):
            f = b.bm.faces.new((r0[k], r0[(k + 1) % m], r1[(k + 1) % m], r1[k]))
            f.material_index = idx
            f.smooth = smooth


def visor(b, at, cx, cy, ri, ro, d0, d1, a0, a1, mat, seg=24):
    """A visor over a dial's top: a shell ri..ro swept from a0 to a1 degrees (0 = the panel's right, 90 = up), d0 to
    d1 out from the plate at its top, falling to half that at its ends."""
    rows = []
    for k in range(seg + 1):
        a = math.radians(a0 + (a1 - a0) * k / seg)
        top = d0 + (d1 - d0) * (0.5 + 0.5 * math.sin(a))
        ca, sa = math.cos(a), math.sin(a)
        sec = [(ri, d0), (ro, d0), (ro, top - 0.004), (ro - 0.003, top), (ri, top)]
        rows.append([at(cx + rr * ca, cy + rr * sa, d) for rr, d in sec])
    idx = b.mat(mat)

    def put(pts, outward):
        e1, e2 = pts[1] - pts[0], pts[-1] - pts[0]
        if e1.cross(e2).dot(outward) < 0:
            pts = list(reversed(pts))
        f = b.bm.faces.new([b.bm.verts.new(P(*p)) for p in pts])
        f.material_index = idx

    for k in range(seg):
        r0, r1 = rows[k], rows[k + 1]
        mid = sum(r0 + r1, Vector()) / (2 * len(r0))
        for j in range(len(r0)):
            q = [r0[j], r1[j], r1[(j + 1) % len(r0)], r0[(j + 1) % len(r0)]]
            put(q, sum(q, Vector()) / 4 - mid)
    for row, k in ((rows[0], 0), (rows[-1], seg)):
        nxt = rows[1] if k == 0 else rows[-2]
        put(row, sum(row, Vector()) / len(row) - sum(nxt, Vector()) / len(nxt))


def sp2m_extra(b, X):
    """SP-2M's channel uprights between its three columns (g092-a), standing proud of the blocks: the column gaps from the trace
    (sp2m_trace.py: mm 601-639 and 1253-1295), 30 mm wide; proud by eye, about +-5 mm."""
    for yy in (1.555 + (945 - 620) * 0.001, 1.555 + (945 - 1274) * 0.001):
        b.box(X(-0.004), X(0.024), yy - 0.015, yy + 0.015, 0.85, 2.09, 'Paint_Rack')


def sp1m_extra(b, X):
    sp1m_top(b, X)
    sp1m_box(b, X)


def sp1m_box(b, X):
    """Spare-parts box ZCzZ1A nr 2 under SP-1M, standing on the floor and reaching into the aisle, with the right
    R-123M's frame on its lid. Plan from mon_2112_80 Fig. 3 (y 2.72-3.39, section A-A); height from the photos: g090-a
    shows the box's plate ЗИП №1А ЯЩИК №2 under the set and the box running down to the floor, g090-c its fold-down
    front. Section A-A draws it 0.19 m high just under the blocks; its heights are squashed (5.74 mm a pixel against
    4.86 in plan), so the photo is taken (docs/OSA_INTERIOR_HOLISTIC.md section 10)."""
    b.box(X(-0.03), X(0.245), 2.72, 3.39, FLOOR_Z, RADIO_BOX_TOP, 'Paint_Equipment')
    b.box(X(0.245), X(0.25), 2.80, 2.98, RADIO_BOX_TOP - 0.10, RADIO_BOX_TOP - 0.04, 'Metal_Dark')   # its nameplate
    for yy in (2.98, 3.45):                                                                            # the set's frame
        b.box(X(0.037), X(0.227), yy - 0.01, yy + 0.01, RADIO_BOX_TOP, RADIO_BOX_TOP + 0.03, 'Metal_Steel')


def sl1m_extra(b, X):
    sl1m_top(b, X)
    sl1m_well(b, X)


def sl1m_well(b, X):
    """The left R-123M's well under SŁ-1M: the set's frame on the floor and the steel tray over it on two posts
    (wwp_gecko g091-d: the set low under the rack's bottom blocks, the tray a perforated steel plate). Plan from
    mon_2112_80 Fig. 3 (the set at y 3.03); heights from the photo, about +-0.08 m until the camera solve (I1)."""
    for yy in (2.80, 3.26):
        b.box(X(0.037), X(0.227), yy - 0.01, yy + 0.01, FLOOR_Z, RADIO_LEFT_FRAME_TOP, 'Metal_Steel')
    b.box(X(-0.03), X(0.27), 2.68, 3.40, LEFT_TRAY_Z, LEFT_TRAY_Z + 0.006, 'Paint_Rack')
    for yy in (2.70, 3.38):
        b.box(X(0.24), X(0.26), yy - 0.01, yy + 0.01, FLOOR_Z, LEFT_TRAY_Z, 'Metal_Steel')


# ---------------------------------------------------------------------------------------------- seats and bench

BENCH = (-0.911, -0.514, 0.682, 2.424)   # along y and its width: mon_2112_80 Fig. 3; across, moved out with SŁ-2M (docs/OSA_INTERIOR_HOLISTIC.md section 28)
SEAT_Z = 1.05                            # cushion top (estimate: eye height 1.83 less a seated eye height of 0.78)


def build_bench():
    b = Builder('SM_OperatorBench')
    x0, x1, y0, y1 = BENCH
    base = SEAT_Z - 0.075
    b.box(x0, x1, y0, y1, FLOOR_Z, base, 'Paint_Bench')
    # the top's raised lip (usaf_shaw_2017 -019)
    b.box(x0 - 0.008, x1 + 0.008, y0 - 0.008, y1 + 0.008, base - 0.03, base + 0.006, 'Paint_Bench')
    # a recessed panel in the bench front, under each seat (stowage), with a pull
    for yc in (0.875, 1.555, 2.2):   # the seats under the three columns (data/cabins/osa_ak.json stations)
        b.box(x0 - 0.004, x0 + 0.01, yc - 0.22, yc + 0.22, FLOOR_Z + 0.08, base - 0.06, 'Paint_Bench')
        b.box(x0 - 0.016, x0 - 0.004, yc - 0.05, yc + 0.05, base - 0.13, base - 0.11, 'Metal_Steel')
    return finish(b, bevel=0.004)


SEAT_PAD = (0.50, 0.44, 0.07)       # the seat's padded top: across, deep, thick (the shells' 0.54 x 0.46 of g094-d, less the rim); g095-d's and g093-a's olive vinyl pads
SEAT_UP_CENTRE_XZ = (-0.20, 1.475)  # the pad's centre folded upright (body x, z): fitted on g093-a's outline through its certified camera at 0.93 overlap, the depth map within 0.09 m
SEAT_UP_LEAN_DEG = 13.0             # folded upright, its top leans back towards the aisle (the same fit: 12.7 degrees)
SEAT_PIVOT_XZ = (-0.52, 0.72)       # the frame's pivot brackets at the foot of the bench's aisle face (g094-d)
BENCH_TOP_Z = 0.981                 # the seats' origin height (the bench's top)


def build_operator_seat():
    """One operator's seat, built facing forward (yaw -90 faces the console), folded upright as wwp_gecko g093-a shows two of them: the
    olive vinyl pad (a rolled edge, a row of domed studs round its side) standing over the aisle with its top facing the console, the grey
    pressed shell under it with a riveted rim, a bent tube hoop and the gas-mask bag, carried on a grey tube frame from pivot brackets at the
    foot of the bench's aisle face, and two webbing straps. Folded down (g094-d, g095-c, g096-a, g097-b) the seat lies on the bench, shell
    up with the bag on it: the pans of section 38. g093-a puts the two backs it sees about 0.3 m aft of the deputy's and senior's stations;
    the seats keep their stations' places because the operators sit at their columns (docs/OSA_MODELLING_HANDOFF.md section 12).
    Origin: under the seat centre at the bench top."""
    b = Builder('SM_OperatorSeat')
    # local frame: x across the seat (right), y forward (towards the console: body -x), z up; origin under the seat centre on the bench top
    pw, ph, pt_ = SEAT_PAD
    lean = math.radians(SEAT_UP_LEAN_DEG)
    up = Vector((0, -math.sin(lean), math.cos(lean)))      # up the standing pad
    face = Vector((0, math.cos(lean), math.sin(lean)))     # out of the pad, towards the console
    e = 0.01
    centre = Vector((0, -(SEAT_UP_CENTRE_XZ[0] + 0.71), SEAT_UP_CENTRE_XZ[1] - BENCH_TOP_Z))    # the pad's middle (the stations' x -0.71)
    hinge = centre + face * (pt_ / 2) - up * (ph / 2 + e)  # the pad face's bottom edge
    pivot = Vector((0, -(SEAT_PIVOT_XZ[0] + 0.71), SEAT_PIVOT_XZ[1] - BENCH_TOP_Z))

    def at(x, s_, t):
        """A point on the back: x across, s_ up from the hinge, t behind the pad's face (towards the aisle)."""
        return hinge + Vector((x, 0, 0)) + up * s_ - face * t

    b.rounded_box(at(0, e + ph / 2, 0), (pw, ph, pt_), 0.06, 'Vinyl_Olive', v=up, seg=5)                               # the pad (grows towards the aisle)
    ring = rrect(0, 0, pw - 0.014, ph - 0.014, 0.055, 5)                                                                # its rolled edge on the face
    b.tube([tuple(at(x, e + ph / 2 + y, 0.006)) for x, y in ring + ring[:1]], 0.008, 'Vinyl_Olive', seg=6)
    for k in range(7):                                                                                                  # domed studs on its sides near the shell
        s_ = e + 0.03 + k * (ph - 0.06) / 6
        for sx in (-1, 1):
            b.cylinder(tuple(at(sx * pw / 2 - sx * 0.002, s_, pt_ - 0.012)), tuple(at(sx * (pw / 2 + 0.006), s_, pt_ - 0.012)), 0.0065, 'Metal_Steel', seg=8)
    for k in range(-3, 4):
        b.cylinder(tuple(at(k * 0.065, e + ph - 0.002, pt_ - 0.012)), tuple(at(k * 0.065, e + ph + 0.006, pt_ - 0.012)), 0.0065, 'Metal_Steel', seg=8)
    sh = pt_                                                                                                            # the shell behind the pad
    b.rounded_box(at(0, e + ph / 2, sh), (pw + 0.04, ph + 0.02, 0.035), 0.06, 'Paint_Rack', v=up, seg=4)
    b.rounded_box(at(0, e + ph / 2, sh + 0.035), (pw, ph - 0.02, 0.012), 0.05, 'Paint_Rack', v=up, seg=4)             # its rim's lip
    for k in range(-3, 4):                                                                                              # rivets along the rim
        for s_ in (e + 0.012, e + ph - 0.008):
            b.cylinder(tuple(at(k * 0.075, s_, sh + 0.035)), tuple(at(k * 0.075, s_, sh + 0.05)), 0.006, 'Metal_Steel', seg=8, smooth=False)
    b.rounded_box(at(0, e + ph * 0.45, sh + 0.047), (pw - 0.12, ph - 0.16, 0.06), 0.03, 'Canvas', v=up, seg=3)       # the gas-mask bag on its back
    b.rounded_box(at(0, e + ph * 0.62, sh + 0.105), (pw - 0.22, 0.15, 0.022), 0.012, 'Canvas', v=up, seg=3)           # the bag's flap
    b.tube([tuple(at(x, s_, sh + h)) for x, s_, h in ((-0.2, ph - 0.02, 0.04), (-0.19, ph - 0.04, 0.1), (-0.08, ph - 0.07, 0.13), (0.08, ph - 0.07, 0.13),
                                                      (0.19, ph - 0.04, 0.1), (0.2, ph - 0.02, 0.04))], 0.011, 'Metal_Steel')   # the hoop over the bag (g094-d)
    knee = at(0, 0.0, sh + 0.02)                                                                                        # the shell's foot, where the frame takes it
    b.cylinder(tuple(knee + Vector((-pw / 2 - 0.02, 0, 0))), tuple(knee + Vector((pw / 2 + 0.02, 0, 0))), 0.012, 'Metal_Steel', seg=12)   # the seat's pivot rod
    for sx in (-1, 1):                                                                                                  # the frame each side (g093-a, g095-d): an arm from the bench's foot,
        x = sx * (pw / 2 + 0.01)                                                                                        # a stay from the bench's edge, their brackets
        foot = Vector((x, pivot.y, pivot.z))
        b.tube([tuple(foot), tuple(knee + Vector((x, 0, 0)))], 0.013, 'Paint_Rack', seg=10)
        b.tube([(x, -0.2, -0.02), tuple(foot.lerp(knee + Vector((x, 0, 0)), 0.55))], 0.01, 'Paint_Rack', seg=8)
        b.box(x - 0.02, x + 0.02, pivot.y - 0.01, pivot.y + 0.03, pivot.z - 0.04, pivot.z + 0.04, 'Paint_Rack')
    for x in (-0.17, 0.17):                                                                                             # the webbing from the shell to the bench's foot
        b.tube([tuple(at(x, ph * 0.3, sh + 0.06)), tuple(knee + Vector((x, -0.04, -0.15))), (x * 1.1, pivot.y - 0.03, pivot.z - 0.05)], 0.01, 'Canvas', seg=6)
    return finish(b, reuse=True)


CAB_SEAT_TOP_Z = SEAT_Z            # the cushion's top, deck + 0.07: from the back's top in g082-b and the seated crewman under the windscreen's top (docs/OSA_INTERIOR_HOLISTIC.md section 26)


def build_cab_seat():
    """Driver's and commander's seat, from its trace (cab_seat_trace.py, cab_seat_model.py)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import cab_seat_model
    return cab_seat_model.build(Builder, finish, CAB_SEAT_TOP_Z - FLOOR_Z - cab_floor_trace().STEP_M)


# ---------------------------------------------------------------------------------------------- driving controls

def build_steering():
    """Steering wheel and column: a black three-spoke wheel about 0.47 m across (rwd_mb3 9a33_13, 9a33_31), its
    centre where the plan draws it (x -0.34, y 4.38), tilted 30 degrees back from upright (estimate)."""
    b = Builder('SM_SteeringWheel')
    c = Vector((-0.36, 4.33, 1.47))
    tilt = math.radians(30)
    axis = Vector((0, -math.cos(tilt), math.sin(tilt)))   # towards the driver
    b.torus(c, axis, 0.235, 0.016, 'Rubber_Black', seg=56, rseg=10)
    ref = Vector((1, 0, 0))
    e2 = axis.cross(ref).normalized()                     # ang 0 is 3 o'clock as the driver sees it, 90 is 12
    for ang in (0, 120, 240):                             # thick black spokes at 3, 11 and 7 o'clock (wwp_gecko g084-c, g084-a)
        a = math.radians(ang)
        d = ref * math.cos(a) + e2 * math.sin(a)
        b.tube([c + d * 0.055 + axis * 0.02, c + d * 0.13 + axis * 0.008, c + d * 0.225], 0.017, 'Rubber_Black', seg=12)
    b.lathe(c - axis * 0.02, axis, [(0.0, 0.0), (0.06, 0.0), (0.068, 0.025), (0.06, 0.05), (0.035, 0.062), (0.0, 0.064)],
            'Bakelite_Black', seg=32)                     # the large cup hub (g084-c)
    b.cylinder(c + axis * 0.043, c + axis * 0.046, 0.03, 'Metal_Steel', seg=24)   # its emblem disc
    col_end = c - axis * 0.40
    b.cylinder(c - axis * 0.02, col_end, 0.03, 'Metal_Dark', seg=20)
    b.cylinder(c - axis * 0.10, c - axis * 0.32, 0.045, 'Rubber_Black', seg=20)
    return finish(b)


def build_pedals():
    """The three pedals in the driver's footwell, clutch, brake and accelerator with rubber pads (wwp_gecko g084-c);
    their places and sizes are estimates."""
    b = Builder('SM_Pedals')
    for x, w in ((-0.53, 0.08), (-0.40, 0.08), (-0.20, 0.06)):
        b.obox(Vector((x, 4.72, 0.80)), Vector((1, 0, 0)), Vector((0, 0.5, 0.866)), Vector((0, -0.866, 0.5)),
               (w, 0.13, 0.015), 'Rubber_Black')
        b.cylinder((x, 4.74, 0.80), (x, 4.86, 1.05), 0.01, 'Metal_Dark', seg=10)
    return finish(b, reuse=False)


def build_cab_levers():
    """The gear lever and the handbrake between the cab seats (rwd_mb3 9a33_13: a tall black lever with a ball knob in
    the middle of the cab; 9a33_31: a second black lever low at the right of the driver). Positions, heights and
    the handbrake's throw are estimates: the plan doesn't draw them. Built in place."""
    b = Builder('SM_CabLevers')
    # gear lever on a bracket bolted to the seat frame's forward side (wwp_gecko g083-a): rubber boot, steel shaft, ball knob
    F = cab_floor_trace()
    deck = FLOOR_Z + F.STEP_M
    b.box(-0.06, 0.02, F.GEAR_BRACKET_Y[0], F.GEAR_BRACKET_Y[1], deck - 0.06, deck - 0.045, 'Paint_Interior')
    base = Vector((-0.02, 4.11, deck - 0.045))
    b.lathe(base, Vector((0, 0, 1)), [(0.0, 0.0), (0.06, 0.0), (0.06, 0.02), (0.03, 0.09), (0.0, 0.09)], 'Rubber_Black',
            seg=20)
    top = Vector((-0.02, 4.13, F.GEAR_TOP_Z))
    b.box(-0.075, 0.035, F.GEAR_BRACKET_Y[0] - 0.01, F.GEAR_BRACKET_Y[1] + 0.01, deck - 0.045, deck - 0.035, 'Metal_Dark')   # the gate plate (g084-c)
    b.tube([base + Vector((0, 0, 0.08)), top - Vector((0, 0.0, 0.02))], 0.011, 'Rubber_Black', seg=10)                      # a black lever (g083-a, g084-c)
    b.lathe(top - Vector((0, 0, 0.018)), Vector((0, 0, 1)), [(0.0, 0.0), (0.02, 0.004), (0.029, 0.018), (0.03, 0.03), (0.026, 0.045), (0.015, 0.056), (0.0, 0.06)],
            'Bakelite_Black', seg=20)                     # its large ball knob
    # handbrake: a ratchet plate on the floor, a lever leaning back to the driver, a rubber grip
    foot = Vector((0.045, 3.92, FLOOR_Z))
    b.box(0.035, 0.055, foot.y - 0.05, foot.y + 0.05, FLOOR_Z, FLOOR_Z + 0.07, 'Metal_Dark')
    tip = Vector((0.045, 3.78, 0.98))
    b.tube([foot + Vector((0, 0, 0.05)), tip], 0.011, 'Metal_Dark', seg=10)
    b.tube([tip, tip + Vector((0, -0.05, 0.03))], 0.017, 'Rubber_Black', seg=12)
    return finish(b, reuse=False)


def build_cab_fittings():
    """Fittings along the windscreen header, from opisybroni 54335: a pale sun visor hinged above each screen and
    the cable run along the header with its clips. Sizes, tilt and the cable's route are estimates. Built in place."""
    b = Builder('SM_CabFittings')
    header_y = SCREEN_BOTTOM_Y + (SCREEN_TOP_Y - SCREEN_BOTTOM_Y) * (HEADER_Z - DASH_TOP_Z) / (ROOF_Z - DASH_TOP_Z) - 0.04
    a = math.radians(70)                                  # the visors hang nearly flat against the screens' top (g082-b, g084-a)
    v = Vector((0, -math.cos(a), -math.sin(a)))           # down and back from the hinge
    n = Vector((0, -math.sin(a), math.cos(a)))
    hinge = Vector((0, SCREEN_BOTTOM_Y + (SCREEN_TOP_Y - SCREEN_BOTTOM_Y) * (VISOR_HINGE_Z - DASH_TOP_Z) / (ROOF_Z - DASH_TOP_Z) - 0.03, VISOR_HINGE_Z))
    for s in (-1, 1):
        c = hinge + v * 0.10
        c.x = VISOR_X[s]
        b.obox(c, Vector((1, 0, 0)), v, n, (VISOR_W, 0.20, 0.014), 'Paint_Panel')
        for dx in (-0.22, 0.22):                          # the two hinge lugs
            b.box(VISOR_X[s] + dx - 0.02, VISOR_X[s] + dx + 0.02, hinge.y - 0.01, hinge.y + 0.01, hinge.z - 0.02,
                  hinge.z + 0.005, 'Metal_Dark')
    # the cable run: three black cables sagging between clips
    clips = [x / 10.0 for x in range(-10, 11, 4)]
    for k, dz in enumerate((0.0, 0.016, 0.032)):
        pts = []
        for i, x in enumerate(clips):
            pts.append((x, header_y - 0.035, HEADER_Z + 0.035 - dz + (0.012 if i % 2 else 0.0)))
        b.tube(pts, 0.006, 'Rubber_Black', seg=8)
    for x in clips:
        b.box(x - 0.008, x + 0.008, header_y - 0.045, header_y - 0.025, HEADER_Z, HEADER_Z + 0.05, 'Metal_Steel')
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
    """GA-3 gyro half-compass at the rear (plan: a circle 0.335 m across, mon_2112_80 Fig. 3), standing on a tall holed bracket: wwp_gecko
    g099-d through its depth map puts the flange at z 1.2 and the dome's top near 1.46 (docs/OSA_INTERIOR_HOLISTIC.md section 36; origin at z 0.77).
    Down the bracket: a plate, a perforated square box, a flange with a ring of screws, the dome; a tube handle beside it. Its foot in the photo stands on a floor hump
    the hull lacks, so the bracket here runs to the floor."""
    b = Builder('SM_GA3')
    b.lathe(Vector((0, 0, -0.15)), Vector((0, 0, 1)), [(0.0, 0.0), (0.2, 0.0), (0.2, 0.012), (0.0, 0.012)], 'Paint_Equipment', seg=40)
    b.prism_y([(-0.05, -0.138), (0.05, -0.138), (0.15, 0.28), (-0.15, 0.28)], -0.13, 0.13, 'Paint_LightGrey')
    for sy in (-1, 1):                                                                                          # the bracket's holes
        for hx, hz, hr in ((0, 0.13, 0.055), (-0.085, 0.2, 0.03), (0.085, 0.2, 0.03)):
            b.cylinder((hx, sy * 0.13, hz), (hx, sy * 0.134, hz), hr, 'Rubber_Black', seg=20, smooth=False)
    b.box(-0.2, 0.2, -0.27, 0.17, 0.28, 0.3, 'Paint_LightGrey')
    b.box(-0.16, 0.16, -0.16, 0.16, 0.3, 0.43, 'Paint_LightGrey')
    for i in range(9):                                                                                          # the box's perforation, on its two aisle-side faces
        for k in range(4):
            u, h = -0.12 + 0.03 * i, 0.325 + 0.027 * k
            b.cylinder((0.16, u, h), (0.162, u, h), 0.007, 'Rubber_Black', seg=8, smooth=False)
            b.cylinder((u, 0.16, h), (u, 0.162, h), 0.007, 'Rubber_Black', seg=8, smooth=False)
    b.lathe(Vector((0, 0, 0.43)), Vector((0, 0, 1)), [(0.0, 0.0), (0.178, 0.0), (0.178, 0.03), (0.168, 0.03), (0.165, 0.13), (0.135, 0.22), (0.06, 0.26), (0.0, 0.26)], 'Paint_LightGrey', seg=40)
    for k in range(16):                                                                                          # the ring of screws
        a = 2 * math.pi * k / 16
        b.cylinder((0.17 * math.cos(a), 0.17 * math.sin(a), 0.46), (0.17 * math.cos(a), 0.17 * math.sin(a), 0.464), 0.0055, 'Metal_Steel', seg=8, smooth=False)
    b.tube([(0.1, -0.25, 0.29), (0.1, -0.25, 0.72), (0.05, -0.22, 0.77), (-0.05, -0.16, 0.77), (-0.1, -0.13, 0.72), (-0.1, -0.13, 0.29)], 0.012, 'Metal_Steel')
    b.cylinder((-0.16, 0, 0.37), (-0.19, 0, 0.37), 0.024, 'Metal_Dark', seg=16, smooth=False)                      # the round plug
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
    for u_, v_ in ((1, 0), (0, 1)):                                    # the wire guard over the dome (g098-a, g100-a d34)
        b.tube([(0.046 * u_, -0.031, 0.046 * v_), (0.02 * u_, -0.047, 0.02 * v_), (-0.02 * u_, -0.047, -0.02 * v_), (-0.046 * u_, -0.031, -0.046 * v_)],
               0.0028, 'Paint_LightGrey', seg=8)
    return finish(b, reuse=True)


def _frame(facing_deg):
    """A wall unit's axes from its heading (cabin.hpp): n out of its face, r to the right as one faces it, z up."""
    f = math.radians(facing_deg)
    return Vector((math.sin(f), math.cos(f), 0.0)), Vector((-math.cos(f), math.sin(f), 0.0)), Vector((0.0, 0.0, 1.0))


# The driver's side window's corner wall (build_hull): from the wall at CAB_Y to the bow, the window 42 % along it.
CORNER_A0 = Vector((-WALL_X, CAB_Y, 0.0))
CORNER_ALONG = Vector((WALL_X - BOW_X, BOW_Y - CAB_Y, 0.0)).normalized()
CORNER_FACING = math.degrees(math.atan2(CORNER_ALONG.y, -CORNER_ALONG.x))   # the inboard normal's heading


def corner_point(t, d, zz):
    """A point t m along the left corner wall from its aft end, d m inboard of its lining, at height zz."""
    n, _, _ = _frame(CORNER_FACING)
    p = CORNER_A0 + CORNER_ALONG * t + n * d
    return Vector((p.x, p.y, zz))


def build_fire_system():
    """The fire-suppression system on the left hull wall beside the driver, under his side window (wwp_gecko g086-c,
    looking down into the corner by the driver's left knee; voenizdat_osa_1991 p16 names the system): the red control
    box under a perforated bracket, the bracket's pressure gauge (scale to 250) on top, a pale canister aft of the box;
    three black bottles below, two lying on the wheel housing's top along the wall and one standing on the floor at the
    housing's foot with its red hand wheel; braided leads up to the box. Beside the box, forward, the 0-1 gauge in its
    light-blue square bezel left of PU-303 (g084-b, g086-c). No solved camera sees the cab: places and sizes are
    estimates. The lamps, the dials and the controls are the cabin's (data/cabins/osa_ak.json fire_panel)."""
    b = Builder('SM_FireSystem')
    n, r, z = _frame(CORNER_FACING)
    face = corner_point(0.27, 0.08, 1.40)                               # the box's face (fire_panel at u -0.09)
    b.obox(face - n * 0.04, r, z, n, (0.21, 0.13, 0.08), 'Paint_Red')
    b.obox(face - n * 0.04 + z * 0.069, r, z, n, (0.226, 0.008, 0.08), 'Paint_Red')            # the lid's rim
    for s_ in (-1, 1):                                                                       # the lid's screw posts
        p = face - n * 0.04 + r * (0.12 * s_)
        b.cylinder(tuple(p + z * 0.06), tuple(p + z * 0.08), 0.008, 'Metal_Steel', seg=10)
    # the perforated bracket over it, from the wall's lining to just proud of the face; u x v looks up
    shelf = corner_point(0.245, 0.0, 1.485)
    holes = [rrect(a_, 0.045, 0.03, 0.03, 0.0149, seg=10) for a_ in (-0.10, -0.05, 0.0, 0.05, 0.10)]
    b.slab(tuple(shelf), r, -n, [(-0.155, 0.0), (-0.155, 0.095), (0.155, 0.095), (0.155, 0.0)], holes, 0.004, 'Paint_LightGrey')
    for s_ in (-1, 1):                                                                       # its two wall brackets
        p = shelf + r * (0.145 * s_) + n * 0.002
        b.obox(p - z * 0.04, r, z, n, (0.004, 0.08, 0.004), 'Paint_LightGrey')
    # the pressure gauge to 250 on the bracket's aft end (fire_panel at u -0.245): body and rim, face inboard
    g = corner_point(0.115, 0.06, 1.522)
    b.obox(corner_point(0.115, 0.045, 1.4885), r, z, n, (0.05, 0.007, 0.03), 'Paint_LightGrey')   # its foot
    b.cylinder(tuple(g - n * 0.03), tuple(g + n * 0.012), 0.029, 'Metal_Steel', seg=22)
    # the pale canister aft of the box, standing on the wheel housing
    c = corner_point(0.075, 0.06, 1.18)
    b.lathe(c, z, [(0.0, 0.0), (0.035, 0.0), (0.038, 0.01), (0.038, 0.17), (0.03, 0.185), (0.0, 0.19)], 'Paint_LightGrey', seg=20)
    # the 0-1 gauge's flange and body, forward of the box (fire_panel at u +0.10)
    g = face + r * 0.19 - z * 0.01
    b.obox(g - n * 0.006, r, z, n, (0.10, 0.11, 0.012), 'Paint_Radio')
    b.cylinder(tuple(g - n * 0.012), tuple(g - n * 0.06), 0.045, 'Metal_Steel', seg=24)
    # two bottles lying on the wheel housing's top along the wall (housing top 1.18 at the wall, cab_floor_trace ARCH)
    bottle = [(0.0, 0.0), (0.03, 0.0), (0.05, 0.02), (0.055, 0.05), (0.055, 0.30), (0.05, 0.33), (0.03, 0.35), (0.0, 0.352)]
    feet = []
    for d_, zz in ((0.07, 1.24), (0.195, 1.215)):
        p0 = corner_point(0.11, d_, zz)
        b.lathe(p0, r, bottle, 'Metal_Dark', seg=24)
        b.cylinder(tuple(p0 + r * 0.352), tuple(p0 + r * 0.38), 0.016, 'Metal_Brass', seg=12)       # the valve head
        for t_ in (0.08, 0.27):                                                                   # the cradle's straps
            q = p0 + r * t_
            b.torus(q, r, 0.058, 0.004, 'Metal_Steel', seg=24, rseg=6)
        feet.append(p0 + r * 0.38)
    # the standing bottle at the housing's foot, its red hand wheel the cabin's (fire_valve_panel)
    base = Vector((-0.775, 4.30, FLOOR_Z))
    b.lathe(base, z, bottle, 'Metal_Dark', seg=24)
    b.cylinder(tuple(base + z * 0.352), tuple(base + z * 0.385), 0.016, 'Metal_Brass', seg=12)
    b.cylinder(tuple(base + z * 0.37 + Vector((0.0, 0.0, 0.0))), tuple(base + z * 0.37 + Vector((0.035, 0.0, 0.0))), 0.008, 'Metal_Brass', seg=10)
    feet.append(base + z * 0.37 + Vector((0.035, 0.0, 0.0)))
    # the braided leads from each valve head up to the box's underside
    for i, foot in enumerate(feet):
        top = face - n * 0.04 - z * 0.065 + r * (-0.06 + 0.06 * i)
        if i < 2:                                   # up off the valve head, then aft over the bottles to the box
            pts = [foot, foot + z * 0.07, top - z * 0.03, top]
        else:                                       # up the housing's slope, clear of the inboard bottle
            pts = [foot, foot + z * 0.12, Vector((-0.95, foot.y, 1.315)), top - z * 0.025, top]
        b.tube([tuple(p) for p in pts], 0.006, 'Metal_Steel', seg=8)
    return finish(b, bevel=0.0015)


def build_bp26():
    """BP-26, the R-123M's power unit (its plate reads БП-26 in wwp_gecko g091-b): a case with a finned top over its
    face, four fuse holders with orange caps, the round power connector and the МАССА terminal; on four shock mounts.
    Built with its face looking forward (+y). Sizes scaled from g090-a, g090-d and g091-b at the R-123M's front
    (0.406 m): estimates. It has no switch: the set is switched at the R-123M."""
    b = Builder('SM_PowerBP26')
    w, d, h = 0.26, 0.34, 0.16
    b.box(-w / 2, w / 2, -d / 2, d / 2, -h / 2, h / 2, 'Paint_Radio')
    for i in range(9):                                                              # the fins over the face's end
        x = -0.11 + 0.0275 * i
        b.box(x - 0.004, x + 0.004, d / 2 - 0.14, d / 2 - 0.005, h / 2, h / 2 + 0.018, 'Paint_Radio')
    y = d / 2
    for x, zc in ((-0.085, 0.035), (-0.045, 0.035), (0.04, 0.035), (0.08, 0.035)):  # the fuse holders
        b.cylinder((x, y, zc), (x, y + 0.008, zc), 0.014, 'Metal_Steel', seg=14)
        b.cylinder((x, y + 0.008, zc), (x, y + 0.03, zc), 0.012, 'Lens_Amber', seg=14)
    b.cylinder((0.01, y, -0.015), (0.01, y + 0.03, -0.015), 0.032, 'Bakelite_Black', seg=20)    # the power connector
    b.cylinder((0.01, y + 0.03, -0.015), (0.01, y + 0.045, -0.015), 0.024, 'Metal_Steel', seg=20)
    b.cylinder((-0.09, y, -0.045), (-0.09, y + 0.02, -0.045), 0.008, 'Metal_Steel', seg=10)     # МАССА
    b.box(-0.075, -0.035, y, y + 0.002, -0.012, 0.008, 'Bakelite_Black')                          # the type plate
    for x in (-w / 2 + 0.03, w / 2 - 0.03):                                                   # the shock mounts
        for yy in (-d / 2 + 0.04, d / 2 - 0.04):
            b.cylinder((x, yy, -h / 2), (x, yy, -h / 2 - 0.015), 0.018, 'Rubber_Black', seg=12)
    return finish(b, bevel=0.0015, reuse=True)


def build_cab_units():
    """Small units round the cab with controls the cabin draws (data/cabins/osa_ak.json), each the body behind its
    panel; places are estimates (no solved camera sees the cab):
    the engine pre-heater's control box and its plate at the right corner post (wwp_gecko g087-c, g087-d);
    the DP-3B's measuring box beside the commander's dash (g087-d); the ВЕНТИЛЯЦИЯ — ОТОПЛЕНИЕ valve on the left
    front post by SŁ-1M (g086-a); the switch box with the red lever in the left radio well (g091-d); the two valves on
    the roof header over the windscreens (g089-a, g089-b)."""
    b = Builder('SM_CabUnits')
    n, r, z = _frame(210)                                       # the pre-heater box
    f = Vector((0.88, 4.42, 1.58))
    b.obox(f - n * 0.035, r, z, n, (0.15, 0.115, 0.07), 'Paint_LightGrey')
    b.obox(f + r * 0.115 - n * 0.002, r, z, n, (0.06, 0.09, 0.004), 'Bakelite_Black')
    n, r, z = _frame(195)                                       # the DP-3B
    f = Vector((0.80, 4.50, 1.40))
    b.obox(f - n * 0.05 + z * 0.02, r, z, n, (0.13, 0.10, 0.10), 'Paint_Radio')
    b.obox(f - n * 0.045 - z * 0.055, r, z, n, (0.12, 0.05, 0.09), 'Paint_Radio')
    for s in (-1, 1):
        c = f - z * 0.08 + r * (0.03 * s) - n * 0.03
        b.cylinder(tuple(c), tuple(c - z * 0.03), 0.011, 'Metal_Steel', seg=12)
    n, r, z = _frame(90)                                        # the valve on the left front post
    f = Vector((-1.0, 4.0, 1.78))
    b.obox(f - n * 0.03, r, z, n, (0.06, 0.06, 0.06), 'Paint_LightGrey')
    b.obox(f - n * 0.002 - z * 0.17, r, z, n, (0.09, 0.05, 0.004), 'Bakelite_Black')          # its plate
    f = Vector((-1.04, 3.20, 1.33))                             # the radio well's switch box
    b.obox(f - n * 0.025, r, z, n, (0.09, 0.10, 0.05), 'Paint_LightGrey')
    for x in (-0.45, 0.55):                                     # the header valves, on short pipes
        c = Vector((x, 4.35, 2.0))
        b.cylinder(tuple(c + Vector((-0.04, 0, 0.02))), tuple(c + Vector((0.04, 0, 0.02))), 0.006, 'Metal_Steel', seg=10)
        b.cylinder(tuple(c + Vector((0, 0, 0.035))), tuple(c + Vector((0, 0, 0.005))), 0.012, 'Metal_Brass', seg=12)
    return finish(b, bevel=0.001)


def build_driver_units():
    """The driver's other fittings, built in place (no solved camera sees the cab: places are estimates):
    the stalk on the steering column's right with its red knob drawn by the cabin (wwp_gecko g084-c); the grey
    pillar beside SŁ-1M's front end that carries, top to bottom, the cast junction box with its glands, the
    ВЕНТИЛЯЦИЯ — ОТОПЛЕНИЕ valve (build_cab_units), the plates (drawn by the cabin), the flat ring bracket and the
    clutch instruction plate (g086-a, g088-a); and the web between the hull wall and that pillar, facing forward
    into the corner, with two round lightening holes, the two hinged stowage covers one above the other and a small
    gauge low on the outboard side (g086-b, seen through the left windscreen)."""
    b = Builder('SM_DriverUnits')
    # the column stalk: a boss on the column's shroud, the stalk out to the right (the knob is the cabin's)
    c = Vector((-0.36, 4.33, 1.47))
    axis = Vector((0, -math.cos(math.radians(30)), math.sin(math.radians(30))))
    p = c - axis * 0.14
    b.cylinder(tuple(p + Vector((0.03, 0.0, 0.0))), tuple(p + Vector((0.058, 0.0, 0.0))), 0.018, 'Bakelite_Black', seg=14)
    b.tube([tuple(p + Vector((0.055, 0.0, 0.0))), tuple(p + Vector((0.11, -0.005, -0.008))), (-0.215, 4.444, 1.386)], 0.0055,
           'Metal_Dark', seg=10)
    # the pillar: a grey box-section column from the wheel housing's height to the roof, off SŁ-1M's front post
    n, r, z = _frame(90)
    b.obox(Vector((-1.05, 4.0, 1.64)), r, z, n, (0.06, 0.92, 0.09), 'Paint_LightGrey')
    jb = Vector((-0.985, 4.0, 1.97))                                       # the junction box and its glands
    b.obox(jb - n * 0.02, r, z, n, (0.08, 0.10, 0.04), 'Paint_LightGrey')
    for dy in (-0.022, 0.022):
        b.cylinder(tuple(jb + Vector((0.0, dy, -0.05))), tuple(jb + Vector((0.0, dy, -0.072))), 0.009, 'Metal_Steel', seg=10)
    b.cylinder(tuple(jb), tuple(jb + n * 0.012), 0.018, 'Bakelite_Black', seg=16)
    ring = Vector((-1.0, 4.0, 1.47))                                       # the flat ring bracket, open on its inboard side
    b.torus(ring + n * 0.035, z, 0.045, 0.006, 'Metal_Steel', seg=28, rseg=6)
    b.obox(ring + n * 0.004, r, z, n, (0.03, 0.03, 0.01), 'Metal_Steel')
    # the web between the hull wall and the pillar, its face forward (u x v = +y), the holes and covers in it
    o = Vector((-1.005, 4.065, 1.18))
    u, v = Vector((-1.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0))
    holes = [rrect(0.145, 0.62, 0.13, 0.13, 0.0649, seg=10), rrect(0.145, 0.79, 0.11, 0.11, 0.0549, seg=10)]
    b.slab(tuple(o), u, v, [(0.0, 0.0), (0.315, 0.0), (0.315, 0.90), (0.0, 0.90)], holes, 0.006, 'Paint_Interior')
    for zc, h in ((1.535, 0.15), (1.33, 0.15)):                           # the stowage covers and their hinges
        cv = Vector((-1.15, 4.074, zc))
        b.obox(cv, u, v, Vector((0, 1, 0)), (0.17, h, 0.006), 'Paint_Rack')
        b.cylinder(tuple(cv + Vector((0.085, 0.004, -h / 2 + 0.01))), tuple(cv + Vector((0.085, 0.004, h / 2 - 0.01))), 0.005,
                   'Metal_Steel', seg=8)
    g = Vector((-1.272, 4.07, 1.222))                                     # the small gauge low on the outboard side
    b.cylinder(tuple(g), tuple(g + Vector((0, 0.025, 0))), 0.024, 'Metal_Steel', seg=18)
    return finish(b, bevel=0.0012)


def build_all(layout):
    objs = [build_hull(), build_glass()]
    objs.append(build_rack('SM_RackSL1M', layout, ['gyro_panel'], -1, [(3.96, 4.06), (2.505, 2.65)], 'wall',
                           extra=sl1m_extra, stops_at_frame=True))
    objs.append(build_rack('SM_RackSL2M', layout, ['fo_panel', 'so_panel', 'op_panel'], -1, [], 'kick',
                           extra=lambda b, X: (sl2m_extra(b, X), sl2m_housings(b, layout))))
    objs.append(build_rack_sl3m(layout))
    have = {p['id'] for p in layout['panels']}
    if 'sp1m_panel' in have:
        objs.append(build_rack('SM_RackSP1M', layout, ['sp1m_panel'], 1, [(3.96, 4.06), (2.505, 2.65)], 'wall',
                               extra=sp1m_extra, stops_at_frame=True))
    if 'sp2m_panel' in have:
        objs.append(build_rack('SM_RackSP2M', layout, ['sp2m_panel'], 1, [], 'doors', extra=sp2m_extra))
    if 'sp3m_panel' in have:
        objs.append(build_rack_sp3m(layout))
    objs += [build_bench(), build_operator_seat(), build_cab_seat(), build_steering(), build_pedals(), build_cab_levers(), build_cab_fittings(), build_r123m(),
             build_ta57(), build_spare_box(), build_ga3(), build_capsule(), build_awo(),
             build_dome_lamp(), build_wall_lamp_blue(), build_chest_5937(), build_first_aid(),
             build_fire_system(), build_bp26(), build_cab_units(), build_driver_units()]
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
    'SM_OperatorSeat': [((-0.71, y, SEAT_Z - 0.04), -90) for y in (0.875, 1.555, 2.2)],
    'SM_CabSeat': [((-0.36, 3.866, CAB_SEAT_TOP_Z), 0), ((0.33, 3.866, CAB_SEAT_TOP_Z), 0)],
    'SM_RadioR123M': [((-0.62, 3.28, 1.25), 90), ((0.64, 3.215, 0.80), -90)],
    'SM_PhoneTA57': [((-0.10, 2.37, 1.03), 0)],
    'SM_SpareBox': [((0.51, 2.32, 0.715), 0)],
    'SM_GA3': [((-0.908, -0.345, 0.77), 0)],
    'SM_CGW10P': [((0.0, 0.645, 0.72), 0)],
    'SM_AWO5M1': [((0.0, -0.632, 0.77), 0)],
    'SM_Cabinet0096': [((-0.2816, 0.3804, 1.3049), -15.7)],     # built by build_cabinet0096.py
    'SM_DomeLamp': [((0.12, 2.0, 2.075), 0), ((0.12, 0.6, 2.075), 0), ((0.0, 3.3, 2.075), 0)],
    'SM_WallLampBlue': [((0.605, REAR_Y + 0.025, 1.965), 0)],
    'SM_PowerBP26': [((0.628, 3.215, 1.82), 0), ((-1.08, 2.659, 1.075), 90)],
}

PREVIEW_VIEWS = {  # name: (eye body xyz, yaw deg clockwise from forward, pitch deg, lens mm)
    'bench': ((-0.70, 1.555, 1.83), -90, -8, 18),
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
    only = set(argv[argv.index('--only') + 1].split(',')) if '--only' in argv else None   # the others stay as they are
    manifest_path = os.path.join(OUT, 'manifest.json')
    manifest = {}
    if only:
        with open(manifest_path, encoding='utf-8') as f:
            manifest = json.load(f)
    for obj in objs:
        if only and obj.name not in only:
            continue
        export(obj)
        manifest[obj.name] = {'size_m': [round(v, 4) for v in obj['body_size']]}
        if not obj['reuse']:
            manifest[obj.name]['centre_m'] = [round(v, 4) for v in obj['body_centre']]
    with open(manifest_path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(manifest, f, indent=1, sort_keys=True)
        f.write('\n')
    if bpy.app.background and not only:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'cabin_interior.blend'))
    print('exported', len(only) if only else len(objs), 'meshes to', OUT)
    if '--preview' in argv:
        prefix = os.path.abspath(argv[argv.index('--preview') + 1])
        os.makedirs(os.path.dirname(prefix), exist_ok=True)
        for p in preview(objs, prefix):
            print('preview', p)
    return manifest


if __name__ == '__main__':
    main()
