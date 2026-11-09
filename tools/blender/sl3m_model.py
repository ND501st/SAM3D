"""SŁ-3M's face as a 3D model, from its trace (tools/blender/sl3m_trace.py: six blocks in two columns, every part in mm).

Run inside Blender through tools/blender/build_sl3m.py. The carcass round it is build_cabin_interior's SM_RackSL3M.

Frame (as SP-3M-2's): the face looks local +y; the face's right is local -x, z up; the panel's centre at the origin, the face plane at y = 0.
In the cabin it stands at the sl3m_panel's centre turned by the panel's facing (+90 degrees: it looks across the aisle at SP-3M-2).

What is the photos' and what is SAM3D's: see the trace. The plates' legends are not drawn (decals, roadmap I10).
"""
import math

from mathutils import Vector

from build_cabin_interior import rrect

import sl3m_trace as T

MM = 0.001
U = Vector((-1.0, 0.0, 0.0))     # the face's right in the model frame
V = Vector((0.0, 0.0, 1.0))      # up
N = Vector((0.0, 1.0, 0.0))      # out of the face
CREAM, STEEL, DARK, BLACK, RED, PLATE, RACK = 'Paint_Letters', 'Metal_Steel', 'Metal_Dark', 'Bakelite_Black', 'Paint_Red', 'Paint_LightGrey', 'Paint_Rack'
OLIVE, BRAID, BROWN = 'Vinyl_Olive', 'Duct_Grey', 'Grate_Rubber'


def at(c, h=0.0):
    a, b = T.to_m(c[0], c[1])
    return Vector((-a, h * MM, b))


def box_mm(b, r, h0, h1, mat):
    x0, y0, x1, y1 = r
    p0, p1 = at((min(x0, x1), min(y0, y1)), h0), at((max(x0, x1), max(y0, y1)), h1)
    b.box(p0.x, p1.x, p0.y, p1.y, p0.z, p1.z, mat)


def disc(b, c, r_mm, h0, h1, mat, seg=20):
    b.cylinder(at(c, h0), at(c, h1), r_mm * MM, mat, seg=seg)


def lathe(b, c, h0, profile, mat, seg=20):
    b.lathe(at(c, h0), N, [(r * MM, h * MM) for r, h in profile], mat, seg=seg)


def rr_slab(b, r, h, t, mat, radius=4.0):
    """A rounded plate with its bounds r; its front at h, t mm thick behind."""
    x0, y0, x1, y1 = r
    a, bb = T.to_m((x0 + x1) / 2.0, (y0 + y1) / 2.0)
    outline = rrect(a, bb, (x1 - x0) * MM, (y1 - y0) * MM, radius * MM)
    b.slab(tuple(at((T.W_MM / 2, T.H_MM / 2), h)), U, V, outline, [], t * MM, mat)


def thin_bar(b, c0, c1, width, h0, h1, mat):
    p0, p1 = at(c0, (h0 + h1) / 2), at(c1, (h0 + h1) / 2)
    u = p1 - p0
    length = u.length
    if length < 1e-6:
        return
    u /= length
    v = N.cross(u).normalized()
    b.obox((p0 + p1) / 2, u, v, N, (length, width * MM, (h1 - h0) * MM), mat)


# ---- the surround and the blocks

def build_surround(b):
    xl0, xl1 = T.FRAME_LEFT
    xr0, xr1 = T.FRAME_RIGHT
    yt0, yt1 = T.FRAME_TOP
    yb0, yb1 = T.FRAME_BOTTOM
    box_mm(b, (xl0, yt0, xr1, yb1), -24.0, -20.0, DARK)                   # the back sheet: what the gaps between the rows show
    box_mm(b, (xl0, yt0, xl1, yb1), -20.0, 3.0, PLATE)
    box_mm(b, (xr0, yt0, xr1, yb1), -20.0, 3.0, PLATE)
    box_mm(b, (xl1, yt0, xr0, yt1), -20.0, 3.0, RACK)
    box_mm(b, (xl1, yb0, xr0, yb1), -20.0, 3.0, RACK)
    box_mm(b, (T.DIVIDER[0], yt1, T.DIVIDER[1], yb0), -20.0, 4.0, PLATE)  # the bright divider between the columns (g099-a)
    for x in T.TAB_X:                                                     # the top strip's tabs
        box_mm(b, (x - 14.0, yt0 + 3.0, x + 14.0, yt1 - 3.0), 3.0, 5.0, PLATE)
        disc(b, (x, (yt0 + yt1) / 2), 3.5, 5.0, 5.6, DARK, seg=10)
    x = -30.0                                                             # the bottom strip's rivets
    while x < T.W_MM + 40.0:
        disc(b, (x, yb0 + 8.0), 3.0, 3.0, 4.2, STEEL, seg=8)
        x += 41.0
    for x0 in T.POCKET_X:                                                 # its five ribbed pockets
        box_mm(b, (x0, yb0 + 18.0, x0 + T.POCKET_W, yb1 - 6.0), 3.0, 3.6, DARK)
        for k in range(5):
            xr = x0 + 6.0 + k * (T.POCKET_W - 12.0) / 4.0
            thin_bar(b, (xr, yb0 + 20.0), (xr, yb1 - 8.0), 3.5, 3.6, 7.0, RACK)


def build_blocks(b):
    for name, (r, _, _) in T.BLOCKS.items():
        box_mm(b, r, -20.0, -2.0, PLATE)
        rr_slab(b, r, 0.0, 2.0, PLATE, radius=3.0)
        x0, y0, x1, y1 = r
        thin_bar(b, (x0, y1 - 1.0), (x1, y1 - 1.0), 2.0, 0.0, 0.6, DARK)   # the shadow line at each block's foot


# ---- the parts

def base(p):
    return p.get('base', 0.0)


def screw(b, p):
    h = base(p)
    r = p['d'] / 2
    lathe(b, p['c'], h, [(r, 0.0), (r, 1.2), (r * 0.75, 1.9), (0.0, 2.1)], STEEL, seg=12)
    thin_bar(b, (p['c'][0] - r * 0.8, p['c'][1] - r * 0.3), (p['c'][0] + r * 0.8, p['c'][1] + r * 0.3), 0.7, h + 1.8, h + 2.3, DARK)


def rivet(b, p):
    r = p['d'] / 2
    lathe(b, p['c'], 0.0, [(r, 0.0), (r * 0.85, p['h'] * 0.7), (r * 0.4, p['h']), (0.0, p['h'])], PLATE, seg=14)


def lamp(b, p):
    r = p['d'] / 2
    z0 = T.Z['fuse_base'] if p.get('on_fuse') else 0.0
    lathe(b, p['c'], z0, [(r * 1.15, 0.0), (r * 1.15, 1.5), (r, 2.0), (r, 3.5), (r * 0.7, 6.0), (0.0, 7.0)], CREAM, seg=16)


def cap(b, p):
    r = p['d'] / 2
    H = p['h']
    lathe(b, p['c'], 0.0, [(r, 0.0), (r, H * 0.65), (r * 0.72, H), (0.0, H)], RED, seg=18)


def jack(b, p):
    r = p['d'] / 2
    H = p['h']
    lathe(b, p['c'], 0.0, [(r, 0.0), (r, 3.0), (r * 0.75, 3.8), (r * 0.7, H), (r * 0.42, H), (r * 0.42, H - 1.5), (0.0, H - 1.5)], DARK)


def jack_black(b, p):
    r = p['d'] / 2
    H = p['h']
    lathe(b, p['c'], 0.0, [(r * 1.2, 0.0), (r * 1.2, 2.0)], RED, seg=20)
    lathe(b, p['c'], 2.0, [(r * 1.04, 0.0), (r, 3.0), (r, H - 7.0), (r * 0.72, H - 2.0), (0.0, H - 2.0)], BLACK, seg=20)


def lock_plate(b, p):
    rr_slab(b, p['r'], p['h'], p['h'], PLATE, radius=7.0)


def lock(b, p):
    r = p['d'] / 2
    H = p['h']
    lathe(b, p['c'], 2.0, [(r, 0.0), (r, 5.0), (r * 0.85, 7.0), (r * 0.8, H - 2.0), (0.0, H - 2.0)], STEEL)
    thin_bar(b, (p['c'][0] - r * 0.6, p['c'][1] - r * 0.2), (p['c'][0] + r * 0.6, p['c'][1] + r * 0.2), 5.0, H, H + 3.0, DARK)
    if p.get('wires'):
        cx, cy = p['c']
        for dy, dx in ((-4.0, -34.0), (5.0, -26.0)):
            b.tube([tuple(at((cx + 2, cy + dy), H + 1)), tuple(at((cx + dx * 0.5, cy + dy + 8), H - 6)),
                    tuple(at((cx + dx, cy + 20 + dy), 6.0))], 1.1 * MM, RED, seg=6)


def fuse_base(b, p):
    cx, cy = p['c']
    rr_slab(b, (cx - p['w'] / 2, cy - p['ht'] / 2, cx + p['w'] / 2, cy + p['ht'] / 2), p['h'], p['h'], RED, radius=12.0)


def fuse_cap(b, p):
    r = p['d'] / 2
    H = p['h'] - 8.0
    lathe(b, p['c'], 8.0, [(r * 0.9, 0.0), (r * 1.05, H * 0.5), (r, H * 0.85), (r * 0.55, H), (0.0, H)], CREAM, seg=20)


def nameplate(b, p):
    rr_slab(b, p['r'], p['h'], 1.0, DARK, radius=1.0)


def conn_plate(b, p):
    rr_slab(b, p['r'], p['h'], p['h'], PLATE, radius=22.0)


def smooth(pts, n=5):
    """A Catmull-Rom through the points, n pieces a span."""
    out = []
    P = [pts[0]] + list(pts) + [pts[-1]]
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(n):
            t = k / n
            out.append(tuple(0.5 * (2 * p1[j] + (p2[j] - p0[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t * t +
                                    (3 * p1[j] - p0[j] - 3 * p2[j] + p3[j]) * t * t * t) for j in range(3)))
    out.append(tuple(pts[-1]))
    return out


def path(b, pts, r_mm, mat, seg=12):
    b.tube([tuple(at((u, v), h)) for u, v, h in smooth(pts)], r_mm * MM, mat, seg=seg)


def plug(b, p):
    """One of the three olive elbow connectors (g099-a, b): the ribbed coupling on the plate, the elbow turning down, a taped collar, the silver braid going down."""
    h = p['base']
    R = p['d'] / 2
    H = p['h'] - h
    profile = [(R, 0.0), (R, 6.0), (R * 0.92, 8.0)]
    z = 10.0
    while z + 8.0 < H:
        profile += [(R * 0.94, z), (R * 1.22, z + 1.5), (R * 1.22, z + 5.5), (R * 0.94, z + 7.0)]
        z += 9.0
    profile += [(R * 0.9, H), (0.0, H)]
    lathe(b, p['c'], h, profile, OLIVE, seg=24)
    cx, cy = p['c']
    tip = h + H
    # the elbow
    path(b, [(cx, cy, tip - 2.0), (cx, cy, tip + 28.0), (cx, cy + 26.0, tip + 56.0), (cx, cy + 72.0, tip + 66.0)], R * 0.9, OLIVE, seg=16)
    # the taped collar and the braid, down in front of the ledge
    b.cylinder(at((cx, cy + 66.0), tip + 65.0), at((cx, cy + 92.0), tip + 66.0), R * 0.62 * MM, CREAM, seg=14)
    lean = (cx - 300.0) * 0.12              # they fan out a little toward the middle one's side (g099-a)
    path(b, [(cx, cy + 90.0, tip + 66.0), (cx + lean * 0.3, cy + 190.0, tip + 60.0), (cx + lean, cy + 340.0, tip + 80.0), (cx + lean * 1.4, cy + 560.0, tip + 110.0)],
         R * 0.56, BRAID, seg=10)


def build_door(b):
    """The small door of 0096-10M, standing open: its free edge straight out of the face; the fuses ПР1...ПР11 on the face turned to the cameras."""
    hx, y0, y1 = T.DOOR_HINGE
    w = T.Z['door']
    box_mm(b, (hx - w, y0, hx, y1), 0.0, 0.4, DARK)                        # where it closes
    ymid = (y0 + y1) / 2
    c = at((hx - 2.0, ymid), w / 2 + 2.0)
    b.obox(c, N, V, -U, (w * MM, (y1 - y0) * MM, 4.0 * MM), CREAM)
    c = at((hx + 0.3, ymid), w / 2 + 2.0)
    b.obox(c, N, V, -U, ((w - 12.0) * MM, (y1 - y0 - 12.0) * MM, 0.6 * MM), DARK)
    b.cylinder(at((hx, y0), 2.0), at((hx, y1), 2.0), 3.0 * MM, STEEL, seg=10)
    for col in (0, 1):
        h = 22.0 + col * 38.0
        for k in range(T.FUSE_ROWS):
            y = y0 + 32.0 + k * (y1 - y0 - 64.0) / (T.FUSE_ROWS - 1)
            b.cylinder(at((hx + 0.6, y), h), at((hx + 8.0, y), h), 4.5 * MM, BROWN, seg=12)
        for y in (y0 + 13.0, y1 - 13.0):
            b.cylinder(at((hx + 0.6, y), h), at((hx + 3.5, y), h), 6.0 * MM, CREAM, seg=12)
    # the lock's bale, tied across to the door's rim (g099-a)
    lk = next(p for p in T.parts('lock', 'A3') if p['c'][0] > hx)
    b.tube([tuple(at(lk['c'], T.Z['lock'] - 2.0)), tuple(at(((lk['c'][0] + hx) / 2, lk['c'][1] + 10.0), T.Z['lock'])),
            tuple(at((hx + 1.0, lk['c'][1] + 4.0), 30.0))], 1.6 * MM, STEEL, seg=8)


DRAW = {'screw': screw, 'rivet': rivet, 'lamp': lamp, 'cap': cap, 'jack': jack, 'jack_black': jack_black, 'lock_plate': lock_plate, 'lock': lock,
        'fuse_base': fuse_base, 'fuse_cap': fuse_cap, 'nameplate': nameplate, 'conn_plate': conn_plate, 'plug': plug}


def build(B, finish):
    """SM_SL3MFace with the interior script's Builder `B` and `finish`."""
    import bpy
    b = B('SM_SL3MFace')
    build_surround(b)
    build_blocks(b)
    for p in T.parts():
        DRAW[p['kind']](b, p)
    build_door(b)
    obj = finish(b, reuse=True)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    bpy.context.view_layer.update()
    return obj
