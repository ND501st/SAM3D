"""The R-124 intercom's subscriber boxes as 3D models: A-1 (the commander's), A-2 (the second box that can go on a radio)
and A-4 (a crew member's plain box). Built with tools/blender/build_r124.py, placed in the cabin by data/cabins/osa_ak.json.

Sources (docs/SOURCES.md `r124_photos`): photographs of the boxes, a museum A-1 (museum.radioscanner.ru, CC BY-SA 2.5) and
new-from-store A-1, A-2 and A-4 offered for sale (detal-potok.ru, tec-detal96.ru); the A-1's mass, 2.3 kg, its four
positions and the 0.5 A fuse from the same listing. No drawing gives the sizes: they are scaled from the photos with the
switch knob (about 27 mm across its lever) as the yardstick, to about 15 %.

* A-1: a pressed-steel box 150 x 156 mm in front, 85 deep, hammer grey; an oval recess down the middle of its face with
  the mode switch (ВЫКЛ, ВС, Р-123, Р-130 / Р-112 stamped round it) above and the volume knob below; the 0,5А fuse holder on
  the right; А-1 and the nameplate at the bottom; four corner screws; cable glands and a connector underneath.
* A-2: a rounded triangle 120 x 117 mm, 60 deep; a round recess with the mode switch on its steel plate (Р-112, ВС, Р-123
  stamped above it), А-2 at the top, a connector on the left side and a cable gland on the right.
* A-4: a small box 72 x 70 mm, 50 deep, on a base plate with two mounting grommets; its connector socket under a guard on
  its front, 26В and А-4 stamped on its top.

Every model is built facing +y (its face), x to the operator's left as with the R-123M (so the face's right is -x), z up;
FACE_Y[name] is the plane the cabin's bare panel lies in (the recess floor, where the switch's shaft comes out).

Frame for the face layouts below: x mm to the right, y mm down, from the face's centre.
"""
import math

from mathutils import Vector

import r123m_model as R          # text, screws, knobs and slab helpers shared with the R-123M

MM = 0.001
U = Vector((-1.0, 0.0, 0.0))
V = Vector((0.0, 0.0, 1.0))
N = Vector((0.0, 1.0, 0.0))

A1 = {'w': 150.0, 'h': 156.0, 'd': 85.0, 'r': 14.0,
      'recess': (66.0, 100.0, (0.0, -4.0), 12.0),          # width, height, centre, depth: a stadium
      'switch': (0.0, -27.0), 'volume': (0.0, 13.0), 'fuse': (55.0, -3.0),
      'labels': [('ВЫКЛ', (-33.0, -58.0), 4.4, 18), ('ВС', (-12.0, -61.5), 4.4, 6), ('Р-123', (14.0, -61.0), 4.4, -6),
                 ('Р-130', (43.0, -58.5), 4.4, -22), ('Р-112', (40.0, -51.0), 4.4, -22), ('А-1', (-4.0, 57.0), 7.5, 0)],
      'nameplate': (33.0, 31.0, 64.0, 43.0), 'screws': [(-61.0, -63.0), (61.0, -63.0), (-61.0, 60.0), (61.0, 60.0)],
      'rivets': [(-61.0, -34.0), (-61.0, -28.0), (61.0, -34.0), (61.0, -28.0), (-28.0, 58.0), (-22.0, 58.0),
                 (24.0, 58.0), (30.0, 58.0)],
      'glands': [-44.0, -26.0, -8.0], 'connector': 30.0}
A2 = {'w': 120.0, 'h': 117.0, 'd': 60.0,
      'lobes': [((-38.0, -36.5), 22.0), ((38.0, -36.5), 22.0), ((0.0, 30.5), 28.0)],    # the rounded triangle: hull of three
      'recess': ((0.0, 6.0), 26.0, 10.0), 'plate_r': 19.6, 'switch': (0.0, 6.0),
      'labels': [('Р-112', (-19.0, -26.0), 4.6, 14), ('ВС', (0.0, -29.5), 4.6, 0), ('Р-123', (19.0, -25.5), 4.6, -14),
                 ('А-2', (0.0, -43.0), 6.5, 0)],
      'screws': [(-44.0, -36.0), (44.0, -35.0), (-22.0, -38.5), (22.5, -38.5), (-2.0, 44.5)],
      'rivets': [(-32.0, -41.0), (-32.0, -34.5), (33.0, -41.0), (33.0, -34.5)]}
A4 = {'w': 72.0, 'h': 70.0, 'd': 50.0, 'r': 9.0, 'base': (100.0, 86.0, 2.0)}

FACE_Y = {}


def outline_stadium(w, h, c, seg=12):
    """A stadium (two half circles joined by straight sides), w wide, h tall (h >= w), centre c (mm, y down) -> plate
    metres (a right, b up), anticlockwise."""
    r = w / 2.0
    cx, cy = c
    pts = []
    for k in range(seg + 1):                                   # top half circle
        t = math.pi * k / seg
        pts.append(((cx + r * math.cos(t)) * MM, -(cy - (h / 2 - r)) * MM + r * math.sin(t) * MM))
    for k in range(seg + 1):                                   # bottom half circle
        t = math.pi + math.pi * k / seg
        pts.append(((cx + r * math.cos(t)) * MM, -(cy + (h / 2 - r)) * MM + r * math.sin(t) * MM))
    return pts


def outline_rounded_rect(w, h, r, seg=6):
    pts = []
    for (cx, cy, t0) in ((w / 2 - r, -(h / 2 - r), -90), (w / 2 - r, h / 2 - r, 0), (-(w / 2 - r), h / 2 - r, 90),
                         (-(w / 2 - r), -(h / 2 - r), 180)):
        for k in range(seg + 1):
            t = math.radians(t0 + 90 * k / seg)
            pts.append(((cx + r * math.cos(t)) * MM, (cy + r * math.sin(t)) * MM))
    return pts


def outline_hull(lobes, seg=48):
    """The convex hull of circles [((x, y mm, y down), r mm)] -> plate metres, anticlockwise."""
    pts = []
    for k in range(seg * 4):
        t = 2 * math.pi * k / (seg * 4)
        d = (math.cos(t), math.sin(t))
        # the support point in direction d (y up) of the union of circles
        best = max(lobes, key=lambda L: L[0][0] * d[0] - L[0][1] * d[1] + L[1])
        (cx, cy), r = best
        pts.append(((cx + r * d[0]) * MM, (-cy + r * d[1]) * MM))
    out = []
    for p in pts:
        if not out or abs(p[0] - out[-1][0]) > 1e-7 or abs(p[1] - out[-1][1]) > 1e-7:
            out.append(p)
    return out


def shrink(pts, f):
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    return [(cx + (p[0] - cx) * f, cy + (p[1] - cy) * f) for p in pts]


class Face:
    """Placing things on a box's face: (x, y) mm from its centre, y down; h metres out of the face (negative: in)."""

    def __init__(self, y_face):
        self.y = y_face

    def pos(self, x, y, h=0.0):
        return Vector((-x * MM, self.y + h, -y * MM))

    def ab(self, x, y):
        return (x * MM, -y * MM)


def slab_ab(b, f, pts, h0, h1, mat, holes=()):
    b.slab(f.pos(0, 0, h1), U, V, pts, list(holes), h1 - h0, mat)


def loft(b, f, ring0, h0, ring1, h1, mat):
    """Side walls between two outlines of equal length (plate metres) at heights h0 and h1."""
    v0 = [b.bm.verts.new(R.P(*f.pos(a / MM, -bb / MM, h0))) for a, bb in ring0]
    v1 = [b.bm.verts.new(R.P(*f.pos(a / MM, -bb / MM, h1))) for a, bb in ring1]
    idx = b.mat(mat)
    n = len(v0)
    for i in range(n):
        q = b.bm.faces.new((v0[i], v0[(i + 1) % n], v1[(i + 1) % n], v1[i]))
        q.material_index = idx


def stamp(b, f, text, c, h_mm, rot, lift=0.0, mat='Metal_Dark'):
    """Letters stamped in the steel: dark, standing 0.3 mm proud of the face."""
    verts, faces = R.text_mesh(text, h_mm * MM, 0.0003)
    if not faces:
        return
    th = math.radians(rot)
    cs, sn = math.cos(th), math.sin(th)
    ca, cb = f.ab(*c)

    def frame(v):
        a = ca + v.x * cs - v.y * sn
        bb = cb + v.x * sn + v.y * cs
        return f.pos(a / MM, -bb / MM, lift + v.z + 0.00015)

    R._place_glyphs(b, verts, faces, frame, mat)


def screw(b, f, c, h=0.0, r_mm=3.0, mat='Metal_Steel', slot=25):
    x, y = c
    b.lathe(f.pos(x, y, h - 0.0003), N, [(r_mm * MM, 0.0), (r_mm * MM, 0.0012), (r_mm * 0.8 * MM, 0.0019), (0.0, 0.002)],
            mat, seg=12)
    th = math.radians(slot)
    ua, vb = U * math.cos(th) + V * math.sin(th), -U * math.sin(th) + V * math.cos(th)
    b.obox(f.pos(x, y, h + 0.0019), ua, vb, N, (2 * r_mm * 0.9 * MM, 0.0006, 0.0006), 'Metal_Dark')


def rivet(b, f, c, h=0.0):
    b.lathe(f.pos(c[0], c[1], h), N, [(1.3 * MM, 0.0), (1.2 * MM, 0.0007), (0.0, 0.0011)], 'Metal_Steel', seg=10)


def pointer_knob(b, f, c, h, length_mm, angle_deg):
    """The small black mode-switch knob: a round boss and a pointer lever (the cabin draws its own on A-1 and A-2; this
    one is for renders of the box alone)."""
    x, y = c
    b.lathe(f.pos(x, y, h), N, [(6 * MM, 0.0), (6 * MM, 0.008), (5 * MM, 0.010), (0.0, 0.010)], 'Bakelite_Black', seg=20)
    th = math.radians(angle_deg)
    ua, vb = U * math.cos(th) + V * math.sin(th), -U * math.sin(th) + V * math.cos(th)
    mid = f.pos(x + 0.5 * length_mm * math.cos(th), y - 0.5 * length_mm * math.sin(th), h + 0.006)
    b.obox(mid, ua, vb, N, (length_mm * MM, 6.5 * MM, 0.009), 'Bakelite_Black')


# ------------------------------------------------------------------------------------------------ A-1

def build_a1(b, knobs=False):
    a = A1
    d = a['d'] * MM
    f = Face(d / 2)
    FACE_Y['SM_IntercomA1'] = d / 2 - a['recess'][3] * MM
    body = outline_rounded_rect(a['w'], a['h'], a['r'])
    # the body behind its face, the face's pressed rim (a 4 mm roll round the edge), the face with the recess cut out
    slab_ab(b, f, shrink(body, 0.985), -d, -0.004, 'Paint_Radio')
    loft(b, f, shrink(body, 0.985), -0.004, shrink(body, 1.0), -0.0015, 'Paint_Radio')
    rw, rh, rc, rdepth = a['recess']
    rec_top = outline_stadium(rw, rh, rc, seg=14)
    rec_bot = outline_stadium(rw - 8.0, rh - 8.0, rc, seg=14)
    slab_ab(b, f, body, -0.0015, 0.0, 'Paint_Radio', holes=[rec_top])
    loft(b, f, rec_top, 0.0, rec_bot, -rdepth * MM, 'Paint_Radio')                       # the recess's sloping walls
    slab_ab(b, f, rec_bot, -rdepth * MM - 0.002, -rdepth * MM, 'Paint_Radio')          # its floor
    floor = -rdepth * MM
    # the mode switch's steel plate (three screws) and the volume knob's plate (four screws), the volume knob
    sx, sy = a['switch']
    slab_ab(b, f, [((sx + 12 * math.cos(2 * math.pi * k / 32)) * MM, (-sy + 12 * math.sin(2 * math.pi * k / 32)) * MM)
                   for k in range(32)], floor, floor + 0.0012, 'Metal_Steel')
    for ang in (90, 210, 330):
        t = math.radians(ang)
        screw(b, f, (sx + 9.5 * math.cos(t), sy - 9.5 * math.sin(t)), floor + 0.0012, r_mm=1.7)
    vx, vy = a['volume']
    slab_ab(b, f, [((vx + 13 * math.cos(2 * math.pi * k / 32)) * MM, (-vy + 13 * math.sin(2 * math.pi * k / 32)) * MM)
                   for k in range(32)], floor, floor + 0.0012, 'Metal_Steel')
    for ang in (45, 135, 225, 315):
        t = math.radians(ang)
        screw(b, f, (vx + 10.5 * math.cos(t), vy - 10.5 * math.sin(t)), floor + 0.0012, r_mm=1.6)
    b.lathe(f.pos(vx, vy, floor + 0.0012), N, [(7.5 * MM, 0.0), (7.5 * MM, 0.009), (6.6 * MM, 0.012), (0.0, 0.0125)],
            'Bakelite_Black', seg=24)
    b.obox(f.pos(vx, vy - 4.5, floor + 0.0128), U, V, N, (0.0012, 0.004, 0.0006), 'Paint_Letters')    # its index mark
    if knobs:
        pointer_knob(b, f, a['switch'], floor + 0.0012, 18.0, 60)
    # the fuse holder: a round flange with three screws, the cap with 0,5А
    fx, fy = a['fuse']
    b.lathe(f.pos(fx, fy, 0.0), N, [(12 * MM, 0.0), (12 * MM, 0.002), (8.5 * MM, 0.003), (8.5 * MM, 0.008), (7.5 * MM, 0.009),
                                    (0.0, 0.009)], 'Metal_Steel', seg=28)
    for ang in (90, 210, 330):
        t = math.radians(ang)
        screw(b, f, (fx + 10 * math.cos(t), fy - 10 * math.sin(t)), 0.002, r_mm=1.5)
    stamp(b, f, '0,5А', (fx, fy), 3.0, 0, lift=0.009)
    # stamped words, the nameplate, screws and rivets
    for text, c, h, rot in a['labels']:
        stamp(b, f, text, c, h, rot)
    x0, y0, x1, y1 = a['nameplate']
    slab_ab(b, f, [(x0 * MM, -y1 * MM), (x1 * MM, -y1 * MM), (x1 * MM, -y0 * MM), (x0 * MM, -y0 * MM)], 0.0, 0.0006,
            'Metal_Steel')
    for i, line in enumerate(('Р-124', 'ЖФ2.089.002', '№ 0000376')):
        stamp(b, f, line, ((x0 + x1) / 2, y0 + 2.2 + i * 3.6), 2.2, 0, lift=0.0006)
    for c in a['screws']:
        screw(b, f, c, 0.0, r_mm=3.6)
    for c in a['rivets']:
        rivet(b, f, c)
    # underneath: three brass cable glands with their cables, the radio's connector
    bot = -a['h'] / 2 * MM
    for gx in a['glands']:
        c = Vector((-gx * MM, f.y - 0.03, bot))
        b.cylinder(c, c + Vector((0, 0, -0.008)), 0.0085, 'Metal_Brass', seg=6)
        b.cylinder(c + Vector((0, 0, -0.008)), c + Vector((0, 0, -0.016)), 0.007, 'Metal_Brass', seg=6)
        b.cylinder(c + Vector((0, 0, -0.016)), c + Vector((0, 0, -0.04)), 0.004, 'Rubber_Black', seg=10)
    c = Vector((-a['connector'] * MM, f.y - 0.03, bot))
    b.box(c.x - 0.016, c.x + 0.016, c.y - 0.014, c.y + 0.014, bot - 0.012, bot, 'Metal_Dark')
    b.cylinder(c + Vector((0, 0, -0.012)), c + Vector((0, 0, -0.026)), 0.011, 'Metal_Brass', seg=16)
    # the shock-mount bracket behind
    b.box(-0.06, 0.06, -d / 2 - 0.006, -d / 2, -0.07, 0.07, 'Metal_Dark')


# ------------------------------------------------------------------------------------------------ A-2

def build_a2(b, knobs=False):
    a = A2
    d = a['d'] * MM
    f = Face(d / 2)
    (rcx, rcy), rr, rdepth = a['recess']
    FACE_Y['SM_IntercomA2'] = d / 2 - rdepth * MM
    body = outline_hull(a['lobes'])
    slab_ab(b, f, shrink(body, 0.985), -d, -0.004, 'Paint_Radio')
    loft(b, f, shrink(body, 0.985), -0.004, body, -0.0015, 'Paint_Radio')
    top = [((rcx + rr * math.cos(2 * math.pi * k / 48)) * MM, (-rcy + rr * math.sin(2 * math.pi * k / 48)) * MM)
           for k in range(48)]
    bot = [((rcx + (rr - 4) * math.cos(2 * math.pi * k / 48)) * MM, (-rcy + (rr - 4) * math.sin(2 * math.pi * k / 48)) * MM)
           for k in range(48)]
    slab_ab(b, f, body, -0.0015, 0.0, 'Paint_Radio', holes=[top])
    loft(b, f, top, 0.0, bot, -rdepth * MM, 'Paint_Radio')
    slab_ab(b, f, bot, -rdepth * MM - 0.002, -rdepth * MM, 'Paint_Radio')
    floor = -rdepth * MM
    sx, sy = a['switch']
    pr = a['plate_r']
    slab_ab(b, f, [((sx + pr * math.cos(2 * math.pi * k / 40)) * MM, (-sy + pr * math.sin(2 * math.pi * k / 40)) * MM)
                   for k in range(40)], floor, floor + 0.0015, 'Metal_Steel')
    for ang in (75, 195, 315):
        t = math.radians(ang)
        screw(b, f, (sx + 13.5 * math.cos(t), sy - 13.5 * math.sin(t)), floor + 0.0015, r_mm=2.0)
    if knobs:
        pointer_knob(b, f, a['switch'], floor + 0.0015, 26.0, -60)
    for text, c, h, rot in a['labels']:
        stamp(b, f, text, c, h, rot)
    for c in a['screws']:
        screw(b, f, c, 0.0, r_mm=3.2)
    for c in a['rivets']:
        rivet(b, f, c)
    # the connector on the left side, the cable gland on the right (as the photo: the box's own left and right)
    zc = -0.012
    xl, xr = -a['w'] / 2 * MM + 0.012, a['w'] / 2 * MM - 0.012
    b.cylinder(Vector((-xl, f.y - 0.025, zc)), Vector((-xl + 0.016, f.y - 0.025, zc)), 0.011, 'Metal_Brass', seg=16)
    b.cylinder(Vector((-xr, f.y - 0.025, zc)), Vector((-xr - 0.014, f.y - 0.025, zc)), 0.0095, 'Metal_Dark', seg=16)
    b.cylinder(Vector((-xr - 0.014, f.y - 0.025, zc)), Vector((-xr - 0.024, f.y - 0.025, zc)), 0.009, 'Metal_Brass', seg=6)
    b.box(-0.04, 0.04, -d / 2 - 0.005, -d / 2, -0.04, 0.04, 'Metal_Dark')


# ------------------------------------------------------------------------------------------------ A-4

def build_a4(b, knobs=False):
    a = A4
    d = a['d'] * MM
    bw, bh, bt = a['base']
    f = Face(d / 2 + bt * MM)
    FACE_Y['SM_IntercomA4'] = f.y
    # the base plate on the wall, its two grommets; the box on it
    slab_ab(b, f, outline_rounded_rect(bw, bh, 6.0), -d - bt * MM, -d, 'Paint_Radio')
    for gx, gy in ((-bw / 2 + 9, bh / 2 - 9), (bw / 2 - 9, -bh / 2 + 9)):
        b.lathe(f.pos(gx, gy, -d), N, [(5.5 * MM, 0.0), (5.5 * MM, 0.003), (3.0 * MM, 0.004), (3.0 * MM, 0.0),
                                       ], 'Rubber_Black', seg=16)
        b.lathe(f.pos(gx, gy, -d), N, [(3.2 * MM, 0.0), (3.2 * MM, 0.0045), (0.0, 0.0045)], 'Metal_Brass', seg=12)
    body = outline_rounded_rect(a['w'], a['h'], a['r'])
    slab_ab(b, f, shrink(body, 0.97), -d, -0.006, 'Paint_Radio')
    loft(b, f, shrink(body, 0.97), -0.006, shrink(body, 0.9), 0.0, 'Paint_Radio')
    slab_ab(b, f, shrink(body, 0.9), -0.0005, 0.0, 'Paint_Radio')
    # the socket on its face, in a rubber surround, between its guard's lugs
    b.box(-0.016, 0.016, f.y, f.y + 0.004, -0.016, 0.016, 'Rubber_Black')
    b.lathe(f.pos(0, 0, 0.004), N, [(12 * MM, 0.0), (12 * MM, 0.008), (0.0, 0.008)], 'Paint_Letters', seg=24)
    for k in range(6):
        t = 2 * math.pi * k / 6
        b.cylinder(f.pos(7 * math.cos(t), 7 * math.sin(t), 0.0115), f.pos(7 * math.cos(t), 7 * math.sin(t), 0.0125),
                   0.0012, 'Metal_Brass', seg=8)
    for side in (-1, 1):                                   # the guard's two lugs either side of the socket
        b.box(side * 0.018 - 0.002, side * 0.018 + 0.002, f.y, f.y + 0.02, -0.012, 0.012, 'Metal_Steel')
    for c in ((-12.0, 12.0), (12.0, -12.0)):
        screw(b, f, c, 0.004, r_mm=2.4)
    # the cable gland on its side, 26В and А-4 stamped on its top
    zc = 0.0
    b.cylinder(Vector((a['w'] / 2 * MM, f.y - 0.025, zc)), Vector((a['w'] / 2 * MM + 0.014, f.y - 0.025, zc)), 0.01,
               'Paint_Letters', seg=12)
    top = a['h'] / 2 * MM
    yc = f.y - d / 2                                      # the top face's middle; the words read from the front
    for text, back_mm, h in (('26В', 9.0, 5.0), ('А-4', -6.0, 7.5)):
        verts, faces = R.text_mesh(text, h * MM, 0.0003)
        R._place_glyphs(b, verts, faces, lambda v, back_mm=back_mm: Vector((-v.x, yc - back_mm * MM - v.y,
                                                                            top + 0.00015 + v.z)), 'Metal_Dark')


BOXES = {'SM_IntercomA1': build_a1, 'SM_IntercomA2': build_a2, 'SM_IntercomA4': build_a4}


def build(name, B, finish, knobs=False):
    b = B(name)
    BOXES[name](b, knobs=knobs)
    obj = finish(b, reuse=True)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    obj['face_y'] = FACE_Y[name]
    return obj
