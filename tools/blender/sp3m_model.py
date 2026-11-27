"""SP-3M-2's face as a 3D model, from its 3D trace (tools/blender/sp3m_trace.py: the white panel, the drawer, the four instrument
blocks 147.6 mm behind them, their parts in mm from the flattened photos, the cables, the surround).

Run inside Blender through tools/blender/build_sp3m.py. The rack's carcass round it stays build_cabin_interior's SM_RackSP3M.

Frame (as SP-2M2's): the face looks local +y; local x is to the operator's left as he faces it (the face's right is -x), z up; the
panel's centre at the origin, the face plane at y = 0. In the cabin it stands at the sp3m_panel's centre turned by the panel's facing.

Two meshes: SM_SP3MFace, and the front flap SM_SP3MFlap on its own, drawn closed as g098-a and b show it (g098-c shows it folded down).

What is the photo's and what is SAM3D's (see the trace): every position and size on the face is measured; the depth of the lower
row and the handles' standing-off are measured; the other heights, every part's shape in depth and the colours of parts the photos
show only as a shade are SAM3D's. The plates' legends are not drawn (decals, roadmap I10).
"""
import math

from mathutils import Vector

from build_cabin_interior import rrect

import sp3m_trace as T

MM = 0.001
U = Vector((-1.0, 0.0, 0.0))     # the face's right in the model frame
V = Vector((0.0, 0.0, 1.0))      # up
N = Vector((0.0, 1.0, 0.0))      # out of the face
CREAM, STEEL, DARK, BLACK, ORANGE, PLATE = 'Paint_Letters', 'Metal_Steel', 'Metal_Dark', 'Bakelite_Black', 'Lens_Amber', 'Paint_LightGrey'
BROWN = 'Grate_Rubber'


def at(c, h=0.0):
    a, b = T.to_m(c[0], c[1])
    return Vector((-a, h * MM, b))


def ab(c):
    a, b = T.to_m(c[0], c[1])
    return (a, b)


def base_h(p):
    return -T.D_LOWER if p['plane'] == 'L' else 0.0


def box_mm(b, r, h0, h1, mat):
    x0, y0, x1, y1 = r
    p0, p1 = at((min(x0, x1), min(y0, y1)), h0), at((max(x0, x1), max(y0, y1)), h1)
    b.box(p0.x, p1.x, p0.y, p1.y, p0.z, p1.z, mat)


def disc(b, c, r_mm, h0, h1, mat, seg=20, r1_mm=None):
    b.cylinder(at(c, h0), at(c, h1), r_mm * MM, mat, seg=seg, r1=None if r1_mm is None else r1_mm * MM)


def lathe(b, c, h0, profile, mat, seg=20):
    b.lathe(at(c, h0), N, [(r * MM, h * MM) for r, h in profile], mat, seg=seg)


def rr_slab(b, r, h, t, mat, radius=4.0):
    """A rounded plate with its bounds r; its front at h, t mm thick behind."""
    x0, y0, x1, y1 = r
    a, bb = T.to_m((x0 + x1) / 2.0, (y0 + y1) / 2.0)
    outline = rrect(a, bb, (x1 - x0) * MM, (y1 - y0) * MM, radius * MM)
    b.slab(tuple(at((T.W_MM / 2, T.H_MM / 2), h)), U, V, outline, [], t * MM, mat)


def thin_bar(b, c0, c1, width, h0, h1, mat):
    """A bar on the face from c0 to c1 (mm), `width` mm wide, between heights h0 and h1."""
    p0, p1 = at(c0, (h0 + h1) / 2), at(c1, (h0 + h1) / 2)
    u = p1 - p0
    length = u.length
    if length < 1e-6:
        return
    u /= length
    v = N.cross(u).normalized()
    b.obox((p0 + p1) / 2, u, v, N, (length, width * MM, (h1 - h0) * MM), mat)


def smooth(pts, n=4):
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


# ---- the surround and the upper row

def build_surround(b):
    # the rail along the top, with its tabs
    box_mm(b, (-86.0, -9.0, 1265.0, -2.0), 0.0, 3.0, DARK)
    for x in T.TAB_X:
        box_mm(b, (x - T.TAB_W / 2, -9.0 - T.TAB_H, x + T.TAB_W / 2, -9.0), 0.0, 6.0, PLATE)
    # the channel upright on the left: two webs and the sloped partitions between them
    x0, x1 = T.CHANNEL_LEFT
    box_mm(b, (x0, -24.0, x1, T.OPENING[3] + 60.0), -2.0, 1.0, PLATE)
    box_mm(b, (x0 - 3.0, -24.0, x0, T.OPENING[3] + 60.0), 0.0, 22.0, PLATE)
    box_mm(b, (x1 - 3.0, -24.0, x1, T.OPENING[3] + 60.0), 0.0, 22.0, PLATE)
    for y in T.GUSSET_Y:
        c = at(((x0 + x1) / 2, y), 11.0)
        v = (V * math.cos(math.radians(50)) + N * math.sin(math.radians(50))).normalized()
        n = U.cross(v).normalized()
        b.obox(c, U, v, n, ((x1 - x0 - 3.0) * MM, 26.0 * MM, 2.5 * MM), PLATE)
    # the flange on the right, three studs on it
    x0, x1 = T.CHANNEL_RIGHT
    box_mm(b, (x0, -24.0, x1, T.OPENING[3] + 60.0), -2.0, 1.0, PLATE)
    box_mm(b, (x1 - 3.0, -24.0, x1, T.OPENING[3] + 60.0), 0.0, 14.0, PLATE)
    for y in (28.0, 180.0, 232.0):
        disc(b, (x1 + 7.0, y), 5.0, 0.0, 6.0, DARK, seg=10)
    # between the plate and the drawer
    box_mm(b, (T.PLATE[2] + 2.0, -2.0, T.COVER[0] - 2.0, 200.0), -2.0, 1.0, 'Paint_Rack')
    # the ribbed shelf under the flap: triangular teeth, the sheet behind them
    y0, y1 = T.RIB_Y
    box_mm(b, (T.RIB_X[0], y0, T.RIB_X[1], y1), -44.0, -40.0, 'Paint_Rack')
    x = T.RIB_X[0]
    while x + 70.0 <= T.RIB_X[1]:
        a0, b0 = ab((x, y0))
        a1, _ = ab((x + 70.0, y0))
        _, b1 = ab((x, y1))
        b.slab(tuple(at((T.W_MM / 2, T.H_MM / 2), 0.0)), U, V, [(a0, b0), (a1, b0), (a0, b1)][::-1], [], 40.0 * MM, PLATE)
        x += T.RIB_PITCH
    # the drawer's carrier under it, and the gusset running back to the white panel's foot
    box_mm(b, T.TRAY, -T.TRAY_DEPTH, 0.0, PLATE)
    a0, b0 = ab((T.TRAY[0] - 170.0, T.TRAY[1]))
    a1, b1 = ab((T.TRAY[0], T.TRAY[3]))
    a2, _ = ab((T.TRAY[0], T.TRAY[1]))
    b.slab(tuple(at((T.W_MM / 2, T.H_MM / 2), -4.0)), U, V, [(a2, b1), (a2, b0), (a0, b0)], [], 90.0 * MM, PLATE)


def build_drawer(b):
    x0, y0, x1, y1 = T.COVER
    wx0, wy0, wx1, wy1 = T.WINDOW
    a0, b0 = ab((x0, y1))
    outer = rrect((ab(((x0 + x1) / 2, 0))[0]), ab((0, (y0 + y1) / 2))[1], (x1 - x0) * MM, (y1 - y0) * MM, 8.0 * MM)
    aw, bw = ab(((wx0 + wx1) / 2, (wy0 + wy1) / 2))
    hole = rrect(aw, bw, (wx1 - wx0) * MM, (wy1 - wy0) * MM, 2.0 * MM)
    b.slab(tuple(at((T.W_MM / 2, T.H_MM / 2), 3.0)), U, V, outer, [hole], 4.0 * MM, 'Paint_Radio')
    # behind the window: the drawer's dark inside, a lit back plate on the right, two pale clips and a yellow lump
    box_mm(b, (wx0, wy0, wx1, wy1), -50.0, -46.0, DARK)
    box_mm(b, (wx0 + 420.0, wy0 + 30.0, wx1, wy1), -46.0, -44.0, 'Paint_Rack')
    for dx, w in ((60.0, 8.0), (100.0, 8.0)):
        box_mm(b, (wx0 + dx, wy0 + 30.0, wx0 + dx + w, wy1 - 8.0), -44.0, -30.0, CREAM)
    box_mm(b, (wx0 + 280.0, wy0 + 38.0, wx0 + 340.0, wy1 - 12.0), -44.0, -22.0, 'Metal_Brass')
    # the window's walls
    for e in ((wx0 - 3, wy0, wx0, wy1), (wx1, wy0, wx1 + 3, wy1), (wx0, wy0 - 3, wx1, wy0), (wx0, wy1, wx1, wy1 + 3)):
        box_mm(b, e, -50.0, 3.0, 'Paint_Radio')


def build_white_plate(b):
    rr_slab(b, T.PLATE, T.Z['plate'], 4.0, CREAM, radius=9.0)        # white enamel, unlike the blocks' grey
    # the dark rim along the plate's foot and left, where the frame is lower
    box_mm(b, (0.0, T.PLATE[3] + 1.0, T.PLATE[2], T.PLATE[3] + 4.0), 0.0, 1.0, DARK)


def build_blocks(b):
    h0 = -T.D_LOWER
    for name, (r, plane, _) in T.BLOCKS.items():
        if plane != 'L':
            continue
        x0, y0, x1, y1 = r
        box_mm(b, (x0, y0, x1, y1), h0 - 20.0, h0 - 2.0, PLATE)
        rr_slab(b, r, h0, 2.0, PLATE, radius=3.0)
    # the plain panel to the right of the last block (lit in g098-c), and the recess's dark back between the blocks
    r = T.BLOCKS['L4'][0]
    box_mm(b, (r[2] + 2.0, T.OPENING[1], T.OPENING[2], T.OPENING[3]), h0 - 6.0, h0 - 3.0, PLATE)
    box_mm(b, (T.OPENING[0], T.OPENING[1], T.OPENING[2], T.OPENING[3]), h0 - 22.0, h0 - 20.0, DARK)
    # the cut-away foot of the right block (g098-d): a dark step
    box_mm(b, (r[0] + 10.0, r[3] - 90.0, r[0] + 120.0, r[3]), h0 - 1.0, h0 + 1.0, DARK)


# ---- the parts

def screw(b, p):
    h = base_h(p)
    r = p['d'] / 2
    mat = 'Metal_Brass' if p.get('colour') == 'brass' else STEEL
    lathe(b, p['c'], h, [(r, 0.0), (r, 1.2), (r * 0.75, 1.9), (0.0, 2.1)], mat, seg=12)
    thin_bar(b, (p['c'][0] - r * 0.8, p['c'][1] - r * 0.3), (p['c'][0] + r * 0.8, p['c'][1] + r * 0.3), 0.7, h + 1.8, h + 2.3, DARK)


def lamp(b, p):
    h = base_h(p)
    r = p['d'] / 2
    # a lamp on a fuse holder stands on the holder's face
    z0 = h + (T.Z['fuse_base'] if p.get('on_fuse') else 0.0)
    lathe(b, p['c'], z0, [(r * 1.15, 0.0), (r * 1.15, 1.5), (r, 2.0), (r, 3.5), (r * 0.7, 6.0), (0.0, 7.0)], CREAM, seg=16)


def jack(b, p):
    h = base_h(p)
    r = p['d'] / 2
    lathe(b, p['c'], h, [(r, 0.0), (r, 3.0), (r * 0.75, 3.8), (r * 0.7, T.Z['jack']), (r * 0.42, T.Z['jack']), (r * 0.42, T.Z['jack'] - 1.5), (0.0, T.Z['jack'] - 1.5)], STEEL)


def lock(b, p):
    h = base_h(p)
    r = p['d'] / 2
    lathe(b, p['c'], h, [(r, 0.0), (r, 5.0), (r * 0.85, 7.0), (r * 0.8, p['h']), (0.0, p['h'])], STEEL)
    thin_bar(b, (p['c'][0] - r * 0.6, p['c'][1] - r * 0.2), (p['c'][0] + r * 0.6, p['c'][1] + r * 0.2), 5.0, h + p['h'], h + p['h'] + 3.0, DARK)
    if p.get('wires'):
        cx, cy = p['c']
        for dy, dx in ((-4.0, 34.0), (5.0, 26.0)):
            b.tube([tuple(at((cx + 2, cy + dy), h + p['h'] + 1)), tuple(at((cx + dx * 0.5, cy + dy + 6), h + p['h'] + 3)),
                    tuple(at((cx + dx, cy + 18 + dy), h + 6))], 1.1 * MM, 'Paint_Red', seg=6)


def cover(b, p):
    x0, y0, x1, y1 = p['r']
    rr_slab(b, (x0, y0, x1, y1 - 2.0), base_h(p) + T.Z['cover'], 3.0, CREAM, radius=15.0)


def hinge(b, p):
    h = base_h(p)
    cx, cy = p['c']
    w, ht = p['w'], p['ht']
    box_mm(b, (cx - w / 2, cy - ht / 2, cx + w / 2, cy + ht / 2), h + 0.5, h + T.Z['hinge'] - 1.0, CREAM)
    b.cylinder(at((cx - w / 2, cy + ht / 2 - 4), h + 4.0), at((cx + w / 2, cy + ht / 2 - 4), h + 4.0), 2.5 * MM, STEEL, seg=8)


def seal(b, p):
    h = base_h(p)
    r = p['d'] / 2
    lathe(b, p['c'], h, [(r, 0.0), (r, T.Z['seal'] * 0.6), (r * 0.7, T.Z['seal']), (0.0, T.Z['seal'])], DARK if p.get('colour') == 'dark' else STEEL, seg=14)


def fuse_base(b, p):
    h = base_h(p)
    cx, cy = p['c']
    rr_slab(b, (cx - p['w'] / 2, cy - p['ht'] / 2, cx + p['w'] / 2, cy + p['ht'] / 2), h + p['h'], p['h'], ORANGE, radius=12.0)


def fuse_cap(b, p):
    h = base_h(p)
    r = p['d'] / 2
    lathe(b, p['c'], h + 8.0, [(r * 0.9, 0.0), (r * 1.05, (p['h'] - 8.0) * 0.5), (r, (p['h'] - 8.0) * 0.85), (r * 0.55, p['h'] - 8.0), (0.0, p['h'] - 8.0)], CREAM, seg=20)


def fuse_small(b, p):
    h = base_h(p)
    r = p['d'] / 2
    lathe(b, p['c'], h, [(r, 0.0), (r, p['h'] * 0.65), (r * 0.72, p['h']), (0.0, p['h'])], ORANGE, seg=18)


def nameplate(b, p):
    rr_slab(b, p['r'], base_h(p) + T.Z['nameplate'], 1.0, DARK, radius=1.0)


def handle(b, p):
    h = base_h(p)
    a, c = p['a'], p['b']
    H = T.Z['handle']
    b.tube([tuple(at(a, h)), tuple(at(a, h + H)), tuple(at(c, h + H)), tuple(at(c, h))], 4.5 * MM, STEEL, seg=10)
    for q in (a, c):
        disc(b, q, 8.0, h, h + 2.0, STEEL, seg=12)


def stud(b, p):
    h = base_h(p)
    r = p['d'] / 2
    mat = CREAM if p.get('colour') == 'cream' else STEEL
    lathe(b, p['c'], h, [(r, 0.0), (r, 4.0), (r * 0.6, 5.5), (r * 0.5, T.Z['stud']), (0.0, T.Z['stud'])], mat, seg=12)


def hex_nut(b, p):
    h = base_h(p)
    r = p['d'] / 2
    disc(b, p['c'], r, h, h + 6.0, STEEL, seg=6)
    lathe(b, p['c'], h + 6.0, [(r * 0.55, 0.0), (r * 0.5, T.Z['hex'] - 6.0), (0.0, T.Z['hex'] - 6.0)], STEEL, seg=12)


def knob_black(b, p):
    h = base_h(p)
    r = p['d'] / 2
    mat = BROWN if p.get('colour') == 'brown' else BLACK
    lathe(b, p['c'], h, [(r * 1.08, 0.0), (r * 1.08, 3.0), (r, 4.0), (r * 0.94, p['h']), (0.0, p['h'])], mat, seg=14 if p['kind'] == 'knob_fluted' else 20)


def knob_cream(b, p):
    h = base_h(p)
    r = p['d'] / 2
    if 'skirt' in p:
        lathe(b, p['c'], h, [(p['skirt'] / 2, 0.0), (p['skirt'] / 2, 8.0), (r * 1.1, 12.0), (0.0, 12.0)], BLACK, seg=20)
    lathe(b, p['c'], h + (10.0 if 'skirt' in p else 0.0), [(r, 0.0), (r, (p['h'] - 10.0) * 0.8), (r * 0.8, p['h'] - 10.0), (0.0, p['h'] - 10.0)], CREAM, seg=20)


def knob_ribbed(b, p):
    h = base_h(p)
    r = p['d'] / 2
    mat = BROWN if p.get('colour') == 'brown' else CREAM
    H = p['h']
    b.lathe(at(p['c'], h), N, [(r * 0.5 * MM, 0.0), (r * 0.5 * MM, 8.0 * MM), (r * MM, 12.0 * MM), (r * MM, (H - 8.0) * MM), (r * 0.72 * MM, H * MM), (0.0, H * MM)], mat, seg=18, smooth=False)


def chart(b, p):
    h = base_h(p)
    x0, y0, x1, y1 = p['r']
    box_mm(b, (x0, y0, x1, y1), h, h + 2.0, BLACK)
    m = 4.0
    ix0, iy0, ix1, iy1 = x0 + m, y0 + m, x1 - m, y1 - m
    box_mm(b, (ix0, iy0, ix1, iy1), h + 2.0, h + 2.4, CREAM)
    for k in range(1, 8):
        x = ix0 + (ix1 - ix0) * k / 8
        thin_bar(b, (x, iy0), (x, iy1), 0.5, h + 2.4, h + 2.6, 'Paint_Rack')
    for k in range(1, 5):
        y = iy0 + (iy1 - iy0) * k / 5
        thin_bar(b, (ix0, y), (ix1, y), 0.5, h + 2.4, h + 2.6, 'Paint_Rack')
    thin_bar(b, (ix0 + 16, iy1 - 3), (ix1 - 18, iy0 + 3), 1.4, h + 2.6, h + 2.9, BLACK)
    thin_bar(b, (ix0 + 3, iy1 - 8), (ix1 - 44, iy0 + 3), 0.8, h + 2.5, h + 2.75, 'Paint_Red')
    thin_bar(b, (ix0 + 38, iy1 - 3), (ix1 - 3, iy0 + 14), 0.8, h + 2.5, h + 2.75, 'Paint_Red')


def conn_d(b, p):
    h = base_h(p)
    x0, y0, x1, y1 = p['r']
    rr_slab(b, p['r'], h + T.Z['conn_d'], T.Z['conn_d'], ORANGE, radius=8.0)
    cx = (x0 + x1) / 2
    for k in range(4):
        y = y0 + (y1 - y0) * (k + 0.5) / 4
        for dx in (-4.0, 4.0):
            disc(b, (cx + dx, y), 1.6, h + T.Z['conn_d'], h + T.Z['conn_d'] + 0.4, DARK, seg=8)


def meter(b, p):
    h = base_h(p)
    x0, y0, x1, y1 = p['r']
    rr_slab(b, p['r'], h + 8.0, 8.0, BLACK, radius=6.0)
    box_mm(b, (x0 + 8, y0 + 9, x1 - 8, y0 + (y1 - y0) * 0.68), h + 8.0, h + 8.5, CREAM)
    thin_bar(b, ((x0 + x1) / 2 + 12, y0 + (y1 - y0) * 0.66), (x0 + 18, y0 + 26), 1.0, h + 8.5, h + 9.0, BLACK)


def scale(b, p):
    h = base_h(p)
    x0, y0, x1, y1 = p['r']
    rr_slab(b, p['r'], h + 2.0, 2.0, STEEL, radius=4.0)
    box_mm(b, (x0 + 4, y0 + 3, x1 - 4, y1 - 3), h + 2.0, h + 2.5, CREAM)
    box_mm(b, (x0 + 4, y0 + 3, x0 + 14, y1 - 3), h + 2.5, h + 2.8, 'Paint_Red')


def coax(b, p):
    h = base_h(p)
    r = p['d'] / 2
    cx, cy = p['c']
    dx, dy = p.get('dir', (0.0, 0.0))
    H = p['h']
    if p.get('black'):
        disc(b, p['c'], r, h, h + H, BLACK, seg=14)
        return
    disc(b, p['c'], r * 1.15, h, h + 6.0, STEEL, seg=16)
    tip = (cx + dx * H * 0.7, cy + dy * H * 0.7)
    b.cylinder(at(p['c'], h + 6.0), at(tip, h + H), r * 0.8 * MM, STEEL, seg=14)
    b.cylinder(at(tip, h + H), at((tip[0] + dx * 14.0, tip[1] + dy * 14.0), h + H + 4.0), r * 0.5 * MM, 'Rubber_Black', seg=10)


def coax_flange(b, p):
    h = base_h(p)
    f = p['flange'] / 2
    cx, cy = p['c']
    box_mm(b, (cx - f, cy - f, cx + f, cy + f), h, h + 3.0, STEEL)
    for dx, dy in ((-f + 5, -f + 5), (f - 5, -f + 5), (-f + 5, f - 5), (f - 5, f - 5)):
        disc(b, (cx + dx, cy + dy), 3.0, h + 3.0, h + 5.0, STEEL, seg=8)
    coax(b, p)


def socket(b, p):
    h = base_h(p)
    r = p['d'] / 2
    lathe(b, p['c'], h, [(r, 0.0), (r, T.Z['socket']), (r * 0.7, T.Z['socket']), (r * 0.7, 2.0), (0.0, 2.0)], BLACK, seg=18)


def elbow(b, p):
    h = base_h(p)
    r = p['d'] / 2
    cx, cy = p['c']
    H = T.Z['elbow']
    disc(b, p['c'], r * 1.05, h, h + H * 0.75, STEEL, seg=14)
    b.cylinder(at(p['c'], h + H * 0.75), at((cx + 42.0, cy + 8.0), h + H * 0.75), r * 0.85 * MM, STEEL, seg=14)
    b.cylinder(at((cx + 42.0, cy + 8.0), h + H * 0.75), at((cx + 62.0, cy + 12.0), h + H * 0.75), r * 0.45 * MM, 'Rubber_Black', seg=10)


def selector(b, p):
    h = base_h(p)
    cx, cy = p['c']
    disc(b, p['c'], p['d'] / 2, h, h + 8.0, BLACK, seg=18)
    thin_bar(b, (cx + 6.0, cy - 22.0), (cx - 4.0, cy + 18.0), 11.0, h + 8.0, h + p['h'], BLACK)


def cube(b, p):
    h = base_h(p)
    x0, y0, x1, y1 = p['r']
    box_mm(b, p['r'], h, h + p['h'], 'Paint_Radio')
    if p.get('red'):
        box_mm(b, (x0 + 18, y0 + 12, x1 - 10, y1 - 12), h + p['h'], h + p['h'] + 0.6, 'Paint_Red')


def latch(b, p):
    box_mm(b, p['r'], base_h(p), base_h(p) + T.Z['latch'], STEEL)


def data_plate(b, p):
    h = base_h(p)
    x0, y0, x1, y1 = p['r']
    rr_slab(b, p['r'], h + T.Z['data_plate'], 1.5, BLACK, radius=1.5)
    for cx in (x0 + 4.0, x1 - 4.0):
        disc(b, (cx, (y0 + y1) / 2), 1.4, h + T.Z['data_plate'], h + T.Z['data_plate'] + 0.6, CREAM, seg=8)


def strip(b, p):
    h = base_h(p)
    x0, y0, x1, y1 = p['r']
    rr_slab(b, p['r'], h + T.Z['strip'], T.Z['strip'], ORANGE, radius=4.0)
    n = max(3, int((x1 - x0) / 7.0))
    for k in range(n):
        disc(b, (x0 + (x1 - x0) * (k + 0.5) / n, (y0 + y1) / 2), 1.8, h + T.Z['strip'], h + T.Z['strip'] + 0.4, DARK, seg=8)


def oval(b, p):
    h = base_h(p)
    x0, y0, x1, y1 = p['r']
    rr_slab(b, p['r'], h + T.Z['oval'], T.Z['oval'], 'Paint_Red', radius=7.0)
    for cx in ((x0 * 2 + x1) / 3, (x0 + x1 * 2) / 3):
        disc(b, (cx, (y0 + y1) / 2), 3.0, h + T.Z['oval'], h + T.Z['oval'] + 0.6, CREAM, seg=10)


DRAW = {'screw': screw, 'lamp': lamp, 'lamp_large': lamp, 'jack': jack, 'lock': lock, 'cover': cover, 'hinge': hinge, 'seal': seal,
        'fuse_base': fuse_base, 'fuse_cap': fuse_cap, 'fuse_small': fuse_small, 'nameplate': nameplate, 'handle': handle, 'stud': stud,
        'hex': hex_nut, 'knob_black': knob_black, 'knob_fluted': knob_black, 'knob_cream': knob_cream, 'knob_ribbed': knob_ribbed,
        'chart': chart, 'conn_d': conn_d, 'meter': meter, 'scale': scale, 'coax': coax, 'coax_flange': coax_flange, 'socket': socket,
        'elbow': elbow, 'selector': selector, 'cube': cube, 'latch': latch, 'data_plate': data_plate, 'strip': strip, 'oval': oval}


def build_parts(b):
    for p in T.parts():
        DRAW[p['kind']](b, p)
    for c in T.CABLES:
        pts = smooth(T.cable_path(c))
        b.tube([tuple(at((u, v), -T.D_LOWER + h)) for u, v, h in pts], c['r'] * MM, c['mat'], seg=8)


def build(B, finish):
    """SM_SP3MFace with the interior script's Builder `B` and `finish`."""
    import bpy
    b = B('SM_SP3MFace')
    build_surround(b)
    build_white_plate(b)
    build_drawer(b)
    build_blocks(b)
    build_parts(b)
    obj = finish(b, reuse=True)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    bpy.context.view_layer.update()
    return obj


def build_flap(B, finish):
    """SM_SP3MFlap: the front flap on its own, closed (g098-a, b); hinged at its foot, pins at both ends (g098-b, c)."""
    b = B('SM_SP3MFlap')
    x0, y0, x1, y1 = T.FLAP
    t = 2.5
    # the sheet, its returns top and bottom, and its ends
    box_mm(b, (x0, y0, x1, y1), -t, 0.0, 'Paint_Rack')
    box_mm(b, (x0, y0, x1, y0 + 3.0), -16.0, 1.5, 'Paint_Rack')
    box_mm(b, (x0, y1 - 3.0, x1, y1), -16.0, 1.5, 'Paint_Rack')
    for e in ((x0, y0, x0 + 3.0, y1), (x1 - 3.0, y0, x1, y1)):
        box_mm(b, e, -16.0, 1.0, 'Paint_Rack')
    # the hinge pins at its foot, out through the ends
    for xa, xb in ((x0 - 30.0, x0), (x1, x1 + 30.0)):
        b.cylinder(at((xa, y1 - 8.0), -10.0), at((xb, y1 - 8.0), -10.0), 5.0 * MM, STEEL, seg=10)
    obj = finish(b, reuse=True)
    return obj
