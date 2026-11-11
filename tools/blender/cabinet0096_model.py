"""The 0096-9M2 distribution cabinet as a 3D model, from its trace (tools/blender/cabinet0096_trace.py). Run inside Blender through
tools/blender/build_cabinet0096.py.

Frame: built facing forward (+y), local x from the hinge side (+) to the free side (-),
the door's plane at y = 0.095, z up with the face's top at 0.48; the mesh's origin is the old mesh's bounding box centre (0, 0.01625, 0).

What is the photos' and what is SAM3D's: see the trace. Legends are not drawn (decals, roadmap I10); no photograph is baked in.
Under the plugs the cabinet stands on the hull's raised floor (`plinth_under_cabinet` in data/cabins/osa_ak_shell.json, read by the depth comparison), which is not built here. The cables' run is SAM3D's.
"""
import math

from mathutils import Matrix, Vector

from build_cabin_interior import P, rrect

import cabinet0096_trace as T

MM = 0.001
U = Vector((-1.0, 0.0, 0.0))
V = Vector((0.0, 0.0, 1.0))
N = Vector((0.0, 1.0, 0.0))

DOOR, FRAME, CREAM, BLACK, STEEL, BRASS, DARK, RED, AMBER, GREEN = 'Paint_Panel', 'Paint_LightGrey', 'Paint_Light', 'Bakelite_Black', 'Metal_Steel', 'Metal_Brass', 'Metal_Dark', 'Paint_Red', 'Lens_Amber', 'Lens_Green'
OLIVE, BRAID, BLUE, WHITE = 'Vinyl_Olive', 'Paint_LightGrey', 'Paint_BlueGrey', 'Paint_Letters'   # the braid is bright silver in g096-a and g097-b

PLINTH_TOP = -0.387  # (m, built frame) the hull's plinth top, z 0.95, under the placed cabinet: (0.95 - 1.3059) / SCALE (0.9199 since 11 Nov 2026)
BODY_BOTTOM = T.to_local(0, T.H_MM)[2]


def at(c, h=0.0):
    return Vector(T.to_local(c[0], c[1], h))


def at3(x, y, z):
    return at((x, y), T.frame_h(z))


def section(points):
    """The trace's (y, z) section to the builder's (local y, local z)."""
    return [tuple(at3(0.0, y, z))[1:] for y, z in points]


def lx(x):
    return T.LOCAL_HINGE_X - x / 1000.0


def disc(b, c, r_mm, h0, h1, mat, seg=20):
    b.cylinder(at(c, h0), at(c, h1), r_mm * MM, mat, seg=seg)


def lathe(b, c, h0, profile, mat, seg=20):
    b.lathe(at(c, h0), N, [(r * MM, h * MM) for r, h in profile], mat, seg=seg)


def rr_slab(b, r, h, t, mat, radius=4.0):
    """A rounded plate over the door with bounds r (mm); its front at h, t mm thick behind."""
    x0, y0, x1, y1 = r
    outline = rrect((x0 + x1) / 2.0 * MM, -(y0 + y1) / 2.0 * MM, (x1 - x0) * MM, (y1 - y0) * MM, radius * MM)
    b.slab(tuple(at((0.0, 0.0), h)), U, V, outline, [], t * MM, mat)


def poly_slab(b, pts, h, t, mat):
    """A plate from the (x, y) mm outline."""
    b.slab(tuple(at((0.0, 0.0), h)), U, V, [(x * MM, -y * MM) for x, y in pts], [], t * MM, mat)


def box_mm(b, r, h0, h1, mat):
    x0, y0, x1, y1 = r
    p0, p1 = at((max(x0, x1), max(y0, y1)), h0), at((min(x0, x1), min(y0, y1)), h1)
    b.box(p0.x, p1.x, p0.y, p1.y, p0.z, p1.z, mat)


def thin_bar(b, c0, c1, width, h0, h1, mat):
    p0, p1 = at(c0, (h0 + h1) / 2), at(c1, (h0 + h1) / 2)
    u = p1 - p0
    length = u.length
    if length < 1e-6:
        return
    u /= length
    v = N.cross(u).normalized()
    b.obox((p0 + p1) / 2, u, v, N, (length, width * MM, (h1 - h0) * MM), mat)


def screw_head(b, c, h, r=4.0, mat=STEEL):
    lathe(b, c, h, [(r, 0.0), (r, 1.0), (r * 0.7, 1.8), (0.0, 2.0)], mat, seg=10)


def stud_on(b, tip, height, n, r=3.5, head=6.0, mat=STEEL):
    """A post from a surface up its normal n (local) to its tip (local), a head on it."""
    base = tip - n * height * MM
    b.cylinder(base, tip - n * 4.0 * MM, r * MM, mat, seg=10)
    b.lathe(tip - n * 5.0 * MM, n, [(head * MM, 0.0), (head * MM, 3.0 * MM), (head * 0.7 * MM, 5.0 * MM), (0.0, 5.0 * MM)], mat, seg=12)


# ---- the body, door, lid, side

def build_body(b):
    b.prism(section(T.BODY_SECTION), -T.LOCAL_HINGE_X, T.LOCAL_HINGE_X, FRAME)
    x0, y0, x1, y1 = T.DOOR
    rr_slab(b, (x0 - 1.2, y0 - 1.2, x1 + 1.2, y1 + 1.2), -13.0, 2.0, DARK, radius=T.DOOR_RADIUS + 1.0)      # the seam round the door
    rr_slab(b, T.DOOR, 0.0, T.DOOR_PROUD, DOOR, radius=T.DOOR_RADIUS)


def build_lid(b):
    b.prism(section(T.LID_SECTION), lx(T.LID_X[1]), lx(T.LID_X[0]), FRAME)
    foot, top, rear = T.LID_SECTION[0], T.LID_SECTION[1], T.LID_SECTION[2]
    lip_up = (at3(0.0, top[0], top[1]) - at3(0.0, foot[0], foot[1]))
    lip_len = lip_up.length
    lip_up /= lip_len
    lip_n = U.cross(lip_up).normalized()                       # out of the lip: forward and down
    mid = (at3(0.0, top[0], top[1]) + at3(0.0, foot[0], foot[1])) / 2
    a, c = T.LID_LIP_PLATE
    centre = Vector((lx((a + c) / 2), mid.y, mid.z)) + lip_n * 0.0006
    b.obox(centre, U, lip_up, lip_n, ((c - a) * MM, lip_len - 0.002, 0.001), BLACK)   # its legend is not drawn
    for x in (a + 9.0, c - 9.0):
        p = Vector((lx(x), mid.y, mid.z))
        b.cylinder(p + lip_n * 0.001, p + lip_n * 0.0028, 0.0025, BRASS, seg=8)
    d = at3(0.0, rear[0], rear[1]) - at3(0.0, top[0], top[1])
    d.normalize()
    n = U.cross(d).normalized()                                 # the lid's top, outwards
    if n.z < 0.0:
        n = -n
    for tip, height in T.LID_SCREWS:
        stud_on(b, at3(*tip), height, n, r=4.0, head=7.0)

    # the card holder lying on the lid
    b.prism(section(T.HOLDER_SECTION), lx(T.HOLDER_X[1]), lx(T.HOLDER_X[0]), FRAME)
    foot_h, bar, rear_h = T.HOLDER_SECTION[1], T.HOLDER_SECTION[2], T.HOLDER_SECTION[3]
    x0, x1 = T.HOLDER_X
    b.cylinder(at3(x0 + 4.0, bar[0], bar[1]), at3(x1 - 4.0, bar[0], bar[1]), 0.003, STEEL, seg=8)        # the bar along its front
    p0, p1 = at3(x0, bar[0], bar[1]), at3(x0, rear_h[0], rear_h[1])
    e = (p1 - p0).normalized()
    for t in T.HOLDER_HINGE_T:                                                                            # the hinge on its left edge
        c = p0 + (p1 - p0) * t + Vector((0.004, 0.0, 0.0))
        b.cylinder(c - e * 0.010, c + e * 0.010, 0.006, BRASS, seg=10)
    a0, a1 = T.HOLDER_CATCH_X
    fup = (at3(0.0, bar[0], bar[1]) - at3(0.0, foot_h[0], foot_h[1]))
    fmid = (at3(0.0, bar[0], bar[1]) + at3(0.0, foot_h[0], foot_h[1])) / 2
    flen = fup.length
    fup /= flen
    fn = U.cross(fup).normalized()
    centre = Vector((lx((a0 + a1) / 2), fmid.y, fmid.z)) + fn * 0.005
    b.obox(centre, U, fup, fn, ((a1 - a0) * MM, flen * 0.6, 0.010), DARK)                                  # the catch
    for (tip, what) in T.LID_STUDS:
        stud_on(b, at3(*tip), 40.0, n, r=3.0, head=6.0 if what == 'stud' else 5.0, mat=STEEL if what == 'stud' else BRASS)


def build_side(b):
    z0, z1 = T.LOUVRE_Z
    y0, y1 = tuple(at3(0.0, 0.0, z1))[1], tuple(at3(0.0, 0.0, z0))[1]
    X = T.LOCAL_HINGE_X
    for slit in T.LOUVRES_UPPER + T.LOUVRES_LOWER:
        zt = T.LOCAL_TOP_Z - (slit - 21.0) / 1000.0
        zb = T.LOCAL_TOP_Z - slit / 1000.0
        b.prism_y([(X - 0.001, zt), (X - 0.001, zb), (X + 0.006, zb)], y0, y1, FRAME)                      # the hood, open downward
        b.box(X - 0.001, X + 0.0045, y0 + 0.002, y1 - 0.002, zb - 0.0035, zb - 0.0005, DARK)               # the slit under it
    a0, a1, c0, c1 = T.RED_PLATE
    ya, yb = tuple(at3(0.0, 0.0, c1))[1], tuple(at3(0.0, 0.0, c0))[1]
    b.box(X - 0.0005, X + 0.0012, ya, yb, T.LOCAL_TOP_Z - a1 / 1000.0, T.LOCAL_TOP_Z - a0 / 1000.0, RED)


# ---- what is on the door

def hinge(b, h):
    y0, y1, e0, e1, ex, ly0, ly1, screws = h
    x = T.HINGE_BARREL_X
    plate_x = T.fix(ex, 0.0, T.HINGE_EAR_HEIGHT)[0]
    ey0 = T.fix(0.0, e0, T.HINGE_EAR_HEIGHT)[1]
    ey1 = T.fix(0.0, e1, T.HINGE_EAR_HEIGHT)[1]
    rr_slab(b, (plate_x - 6.0, ey0 - 6.0, plate_x + 6.0, ey1 + 6.0), -13.5, 1.5, BRASS, radius=5.5)
    for ey in (ey0, ey1):
        screw_head(b, (plate_x, ey), -13.5, r=4.0, mat=BRASS)
    fy0, fy1 = T.fix(0.0, y0, 0.0)[1], T.fix(0.0, y1, 0.0)[1]
    b.cylinder(at((x, fy0), -2.0), at((x, fy1), -2.0), 0.0055, STEEL, seg=12)
    b.cylinder(at((x, fy0 - 3.0), -2.0), at((x, fy0), -2.0), 0.0042, BRASS, seg=10)
    b.cylinder(at((x, fy1), -2.0), at((x, fy1 + 3.0), -2.0), 0.0042, BRASS, seg=10)
    lfy0, lfy1 = T.fix(0.0, ly0, 1.0)[1], T.fix(0.0, ly1, 1.0)[1]
    rr_slab(b, (x, lfy0, T.HINGE_LEAF_X, lfy1), 1.2, 1.2, BRASS, radius=3.0)
    for s in screws:
        screw_head(b, T.fix(s[0], s[1], 1.0), 1.2, r=4.0, mat=BRASS)


def rivet(b, p):
    r = p['d'] / 2
    lathe(b, p['c'], 0.0, [(r, 0.0), (r * 0.85, 0.9), (r * 0.4, 1.5), (0.0, 1.5)], WHITE, seg=10)


def captive(b, p):
    r = p['d'] / 2
    lathe(b, p['c'], 0.0, [(r, 0.0), (r, 1.8), (r * 0.8, 3.0), (0.0, 3.0)], STEEL, seg=14)
    c = p['c']
    thin_bar(b, (c[0] - r * 0.75, c[1] - r * 0.25), (c[0] + r * 0.75, c[1] + r * 0.25), 1.4, 2.6, 3.2, DARK)


def knob(b, p):
    H = p['h']
    lathe(b, p['c'], 0.0, [(12.2, 0.0), (12.2, 2.0), (10.5, 3.0), (10.5, H - 4.0), (9.5, H - 1.5), (8.2, H), (0.0, H)], BLACK, seg=14)
    c = p['c']
    for k in range(12):                                                                                 # the knurl
        a = k * math.pi / 6.0
        px, py = c[0] + 10.6 * math.cos(a), c[1] + 10.6 * math.sin(a)
        p0, p1 = at((px, py), 4.0), at((px, py), H - 5.0)
        ru = Vector((-math.cos(a), 0.0, -math.sin(a)))                                                  # the rib's radial axis: local x runs against the trace's x, z against y
        tu = Vector((math.sin(a), 0.0, -math.cos(a)))
        b.obox((p0 + p1) / 2, ru, tu, N, (0.0016, 0.0024, (H - 9.0) * MM), BLACK)


def knob_green(b, p):
    H = p['h']
    lathe(b, p['c'], 0.0, [(12.4, 0.0), (12.4, H - 4.0), (10.5, H - 3.0), (9.0, H - 3.0)], DARK, seg=18)
    lathe(b, p['c'], H - 3.0, [(8.8, 0.0), (8.0, 1.8), (5.0, 3.0), (0.0, 3.4)], GREEN, seg=16)


def lamp_ring(b, p):
    lathe(b, p['c'], 0.0, [(8.0, 0.0), (8.0, 2.4), (6.5, 3.0), (4.5, 3.0)], BRASS, seg=14)
    lathe(b, p['c'], 1.5, [(4.5, 0.0), (4.0, 1.5), (0.0, 2.2)], AMBER, seg=10)
    disc(b, p['nub'], 2.5, 0.0, 3.0, DARK, seg=8)


def adjust(b, p):
    r = p['d'] / 2
    disc(b, p['c'], r, 0.0, 0.5, BLACK, seg=14)


def push(b, p):
    lathe(b, p['c'], 0.0, [(15.4, 0.0), (15.4, 3.0), (13.0, 4.0), (11.0, 4.0), (11.0, 1.0)], STEEL, seg=22)
    lathe(b, p['c'], 0.5, [(10.8, 0.0), (10.8, 5.0), (8.5, 6.8), (0.0, 7.3)], BLACK if p['colour'] == 'black' else RED, seg=20)


def capsule(b, p):
    x0, y0, x1, y1 = p['r']
    h = p['h']
    rad = min(x1 - x0, y1 - y0) / 2.0
    rr_slab(b, p['r'], h - 0.7, h - 0.7, CREAM, radius=rad * 0.96)
    rr_slab(b, (x0 + 2.5, y0 + 2.5, x1 - 2.5, y1 - 2.5), h + 0.7, 1.5, CREAM, radius=rad * 0.82)
    if x1 - x0 > 90.0:
        mid = (x0 + x1) / 2.0
        thin_bar(b, (mid, y0 + 2.0), (mid, y1 - 2.0), 1.3, h - 0.2, h + 0.9, DARK)                      # where the moulding's two halves meet
    w = x1 - x0
    for f in (0.13, 0.87):                                                                              # the grooves toward its ends
        x = x0 + f * w
        thin_bar(b, (x, y0 + 3.0), (x, y1 - 3.0), 1.2, h + 0.5, h + 0.9, WHITE)


def cap_button(b, p):
    h0 = T.Z['capsule'] + 0.7
    lathe(b, p['c'], h0, [(9.0, 0.0), (9.0, 0.3), (7.6, 0.3)], WHITE, seg=16)
    lathe(b, p['c'], h0, [(6.8, 0.0), (6.8, p['h'] - h0 - 3.0), (5.0, p['h'] - h0 - 0.8), (0.0, p['h'] - h0)], RED, seg=14)


def lever(b, c0, c1, h0, h1):
    b.cylinder(at(c0, h0), at(c1, h1 - 2.0), 0.0026, STEEL, seg=8, r1=0.0015)
    b.lathe(at(c1, h1 - 3.5), (at(c1, h1) - at(c0, h0)).normalized(), [(0.002, 0.0), (0.0034, 0.002), (0.0026, 0.0045), (0.0, 0.006)], STEEL, seg=10)


def cap_toggle(b, p):
    c = p['c']
    h0 = T.Z['capsule'] + 0.7
    lathe(b, c, h0, [(8.0, 0.0), (8.0, 0.3), (6.8, 0.3)], WHITE, seg=14)
    lathe(b, c, h0, [(6.0, 0.0), (6.0, 2.0), (4.5, 3.0), (0.0, 3.0)], STEEL, seg=12)
    lever(b, c, (c[0], c[1] + 6.5), h0 + 2.5, p['h'])


def tov(b, p):
    c = p['c']
    lathe(b, c, 0.0, [(11.0, 0.0), (11.0, 2.5), (8.5, 3.2), (6.5, 3.2), (6.5, 5.0), (4.5, 5.5), (0.0, 5.5)], STEEL, seg=18)
    lever(b, c, p['tip'], 5.0, 15.0)


def hull(pts):
    """The convex hull of (x, y) points, as a closed outline (Andrew's monotone chain)."""
    pts = sorted(set(pts))

    def cross(o, a, c):
        return (a[0] - o[0]) * (c[1] - o[1]) - (a[1] - o[1]) * (c[0] - o[0])

    lo, up = [], []
    for q in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], q) <= 0:
            lo.pop()
        lo.append(q)
    for q in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], q) <= 0:
            up.pop()
        up.append(q)
    return lo[:-1] + up[:-1]


def pear(c0, r0, c1, r1, n=18):
    pts = []
    for k in range(n):
        a = 2.0 * math.pi * k / n
        pts.append((round(c0[0] + r0 * math.cos(a), 3), round(c0[1] + r0 * math.sin(a), 3)))
        pts.append((round(c1[0] + r1 * math.cos(a), 3), round(c1[1] + r1 * math.sin(a), 3)))
    return hull(pts)


def fuse_disc(b, p):
    """The holder: an amber pear, its round top with the steel-ringed cap, its lower part holding the cream bulb, two cream tips on its shoulders."""
    c, bulb = p['c'], p['bulb']
    Hd, Hb = T.Z['fuse_disc'], T.Z['fuse_bulb']
    Rd, Rb = T.FUSE_DISC_R, T.FUSE_BULB_R
    poly_slab(b, pear(c, Rd, bulb, Rb + 1.0), 10.0, 10.0, AMBER)
    lathe(b, c, 10.0, [(Rd, 0.0), (Rd, Hd - 12.0), (Rd * 0.88, Hd - 10.0), (6.0, Hd - 10.0)], AMBER, seg=14)
    lathe(b, c, Hd - 1.5, [(5.6, 0.0), (5.6, 2.0), (4.5, 2.6), (0.0, 2.6)], STEEL, seg=10)
    thin_bar(b, (c[0] - 3.6, c[1]), (c[0] + 3.6, c[1]), 1.2, Hd + 0.9, Hd + 1.4, DARK)
    lathe(b, bulb, 10.0, [(Rb - 2.0, 0.0), (Rb - 2.0, 1.5), (Rb * 0.78, 5.0), (Rb * 0.52, 7.5), (0.0, Hb - 10.0)], CREAM, seg=12)
    for dx in (-Rd + 0.5, Rd - 0.5):
        lathe(b, (c[0] + dx, c[1] + 7.7), 8.0, [(2.8, 0.0), (2.8, 2.5), (1.4, 4.0), (0.0, 4.0)], CREAM, seg=8)


def strip(b, p):
    x0, y0, x1, y1 = p['r']
    rr_slab(b, p['r'], p['h'], p['h'], BLACK, radius=3.0)
    cx = (x0 + x1) / 2.0
    for k in range(5):
        y = y0 + (k + 0.5) * (y1 - y0) / 5.0
        disc(b, (cx, y), 4.2, p['h'], p['h'] + 0.8, BRASS, seg=10)
        disc(b, (cx, y), 2.0, p['h'] + 0.8, p['h'] + 1.0, DARK, seg=8)
    for s in p['screws']:
        screw_head(b, s, 0.0, r=4.0)


def typebox(b, p):
    x0, y0, x1, y1 = p['r']
    rr_slab(b, p['r'], 0.5, 0.5, DARK, radius=1.0)
    rr_slab(b, (x0 + 1.0, y0 + 1.0, x1 - 1.0, y1 - 1.0), 0.7, 0.2, WHITE, radius=0.8)


def tag(b, p):
    rr_slab(b, p['r'], 0.5, 0.5, RED if p['colour'] == 'red' else BLUE, radius=0.8)


DRAW = {'rivet': rivet, 'captive': captive, 'knob': knob, 'knob_green': knob_green, 'lamp_ring': lamp_ring, 'adjust': adjust, 'push': push, 'capsule': capsule, 'cap_button': cap_button,
        'cap_toggle': cap_toggle, 'tov': tov, 'fuse_disc': fuse_disc, 'strip': strip, 'typebox': typebox, 'tag': tag}


# ---- the rails, meters, plugs

def smooth(pts, n=4):
    """A Catmull-Rom through the points (x, y, z), n pieces a span."""
    out = []
    Q = [pts[0]] + list(pts) + [pts[-1]]
    for i in range(1, len(Q) - 2):
        p0, p1, p2, p3 = Q[i - 1], Q[i], Q[i + 1], Q[i + 2]
        for k in range(n):
            t = k / n
            out.append(tuple(0.5 * (2 * p1[j] + (p2[j] - p0[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t * t +
                                    (3 * p1[j] - p0[j] - 3 * p2[j] + p3[j]) * t * t * t) for j in range(3)))
    out.append(tuple(pts[-1]))
    return out


def ball(b, c, h, r_mm, mat, seg=10):
    """A ball of radius r_mm at (c, h)."""
    profile = [(r_mm * math.sin(math.pi * j / 6.0), r_mm - r_mm * math.cos(math.pi * j / 6.0)) for j in range(1, 6)]
    lathe(b, c, h - r_mm, [(0.0, 0.0)] + profile + [(0.0, 2.0 * r_mm)], mat, seg=seg)


def rail(b, a, c, rod_y):
    """A tube out of the door at each foot, a bend over and down, and the rod a few mm below its feet."""
    H, R = T.Z['rail'], T.Z['rail_rod']
    r = T.RAIL_ROD_R * MM
    x0, y0 = a
    x1, y1 = c
    inset = 12.0
    b.cylinder(at((x0 + inset, rod_y), R), at((x1 - inset, rod_y), R), r, STEEL, seg=8)
    for fx_, fy_, sign in ((x0, y0, 1.0), (x1, y1, -1.0)):
        b.cylinder(at((fx_, fy_), 0.0), at((fx_, fy_), H - 3.0), r, STEEL, seg=8)
        joint = (fx_ + sign * inset, rod_y)
        b.cylinder(at((fx_, fy_), H - 3.0), at(joint, R), r, STEEL, seg=8)
        ball(b, (fx_, fy_), H - 3.0, T.RAIL_ROD_R + 0.4, STEEL, seg=10)
        ball(b, joint, R, T.RAIL_ROD_R + 0.2, STEEL, seg=8)
        lathe(b, (fx_, fy_), 0.0, [(T.RAIL_FOOT_R, 0.0), (T.RAIL_FOOT_R, 1.0), (T.RAIL_FOOT_R * 0.7, 2.0), (3.0, 2.0)], STEEL, seg=12)


def lobed(axis, R, ears, re, n=72):
    """The round bezel with its four ears, as an outline: a circle about the axis, and a circle of radius re about each ear's centre, seen from the axis."""
    out = []
    for k in range(n):
        th = 2.0 * math.pi * k / n
        c, si = math.cos(th), math.sin(th)
        best = R
        for ex, ey in ears:
            dx, dy = ex - axis[0], ey - axis[1]
            proj = dx * c + dy * si
            perp2 = dx * dx + dy * dy - proj * proj
            if proj > 0.0 and perp2 < re * re:
                best = max(best, proj + math.sqrt(re * re - perp2))
        out.append((axis[0] + best * c, axis[1] + best * si))
    return out


def build_hours(b):
    """The hours meter: a four-eared bezel standing off the door on its round body, a recess behind it, the counter window on the upper half of the black face."""
    H, Hd = T.Z['meter'], T.Z['dial']
    ax, ay = T.HOURS['axis']
    h = T.HOURS
    screws = [T.fix(s[0], s[1], H) for s in h['screws']]
    outline = lobed((ax, ay), h['flange_r'], screws, h['ear_r'])
    hole = [(ax + h['aperture_r'] * math.cos(2.0 * math.pi * k / 64.0), ay + h['aperture_r'] * math.sin(2.0 * math.pi * k / 64.0)) for k in range(64)]
    b.slab(tuple(at((0.0, 0.0), H)), U, V, [(x * MM, -y * MM) for x, y in outline], [[(x * MM, -y * MM) for x, y in hole]], 6.0 * MM, BLACK)
    def annulus(front, t, ro, ri, mat=BLACK):
        ring = lambda r: [((ax + r * math.cos(2.0 * math.pi * k / 48.0)) * MM, -(ay + r * math.sin(2.0 * math.pi * k / 48.0)) * MM) for k in range(48)]
        b.slab(tuple(at((0.0, 0.0), front)), U, V, ring(ro), [ring(ri)], t * MM, mat)

    Ro = h['flange_r'] - 5.0
    annulus(H - 6.0, H - 6.0 - (Hd - 1.0), Ro, h['aperture_r'])                       # the round body's wall
    annulus(H + 0.8, 1.6, h['ring_r'], h['aperture_r'])                                # the raised ring round the glass
    disc(b, (ax, ay), Ro, 0.0, Hd - 1.0, BLACK, seg=40)
    disc(b, (ax, ay), h['aperture_r'] + 0.5, Hd - 1.0, Hd, BLACK, seg=44)
    wr = h['window_r']
    corner, mid = h['window_corner_dy'], h['window_centre_dy']
    pts = [(ax + wr * math.cos(math.pi * k / 18.0), ay + corner - (wr + corner) * math.sin(math.pi * k / 18.0)) for k in range(19)]
    for k in range(1, 12):                                                                                   # back along the arc that rises to the middle
        t = k / 12.0
        x = wr * (1.0 - 2.0 * t)
        pts.append((ax - x, ay + mid + (corner - mid) * (x / wr) ** 2))
    poly_slab(b, list(reversed(pts)), Hd + 0.3, 0.3, WHITE)
    cx, cy = ax + h['drums_dx'], ay + h['drums_dy']
    box_mm(b, (cx - 21.0, cy - 5.5, cx + 21.0, cy + 5.5), Hd + 0.3, Hd + 0.6, DARK)
    for k in range(4):                                                                                       # the four drums; their figures are not drawn
        xk = cx - 15.5 + k * 10.3
        box_mm(b, (xk - 3.7, cy - 4.2, xk + 3.7, cy + 4.2), Hd + 0.6, Hd + 0.8, WHITE)
    for s in screws:
        screw_head(b, s, H - 0.1, r=5.0)
    for s in h['small_screws']:
        screw_head(b, T.fix(s[0], s[1], H), H - 0.1, r=3.0)


def build_volt(b):
    """The voltmeter: the square bezel, standing off the door on a smaller body, its recess; the dial, the needle and the green sticker on the recess's face."""
    H, Hd = T.Z['meter'], T.Z['dial']
    v = T.VOLT
    ax, ay = v['axis']
    x0, y0, x1, y1 = T.fix_rect(v['bezel'], H)
    x0 = 2.0 * ax - x1                                                   # the left edge on the flat includes the side wall the camera sees: the bezel is square about its axis
    inset = v['aperture_inset']
    w, ht = x1 - x0, y1 - y0
    mx, my = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    outer = rrect(mx * MM, -my * MM, w * MM, ht * MM, 8.0 * MM)
    hole = rrect(mx * MM, -my * MM, (w - 2.0 * inset) * MM, (ht - 2.0 * inset) * MM, 14.0 * MM)
    b.slab(tuple(at((0.0, 0.0), H)), U, V, outer, [hole], 6.0 * MM, BLACK)
    body = rrect(mx * MM, -my * MM, (w - 16.0) * MM, (ht - 16.0) * MM, 6.0 * MM)
    b.slab(tuple(at((0.0, 0.0), H - 6.0)), U, V, body, [hole], (H - 6.0 - Hd + 0.5) * MM, BLACK)
    rr_slab(b, (x0 + 8.0, y0 + 8.0, x1 - 8.0, y1 - 8.0), Hd - 0.5, Hd - 0.5, BLACK, radius=6.0)
    rr_slab(b, (x0 + inset - 1.0, y0 + inset - 1.0, x1 - inset + 1.0, y1 - inset + 1.0), Hd, 0.5, BLACK, radius=12.0)
    half = v['dial_half_w']
    dt, db = v['dial_top_dy'], v['dial_bottom_dy']
    pts = [(ax + half, ay + db)]
    pts += [(ax + half * math.cos(math.pi * k / 16.0), ay + db - 14.0 - (db - 14.0 - dt) * math.sin(math.pi * k / 16.0)) for k in range(17)]
    pts += [(ax - half, ay + db)]
    poly_slab(b, list(reversed(pts)), Hd + 0.3, 0.3, WHITE)
    thin_bar(b, (ax, ay + db + 6.0), (ax - 13.0, ay + dt + 10.0), 0.9, Hd + 0.3, Hd + 1.0, DARK)            # the needle
    box_mm(b, T.fix_rect(v['sticker'], Hd), Hd, Hd + 0.5, GREEN)
    for s in v['screws']:
        screw_head(b, T.fix(s[0], s[1], H), H - 0.1, r=5.0)


def build_dome(b):
    """The white half-dome over the voltmeter, swept up to its height."""
    d = T.DOME
    cx, base = (d['left'] + d['right']) / 2.0, d['base']
    a, rise, H = (d['right'] - d['left']) / 2.0, base - d['top'], T.Z['dome']
    n, m = 7, 16
    rows = []
    for j in range(n + 1):
        t = j / n
        s = math.cos(t * math.pi / 2.0)
        h = H * math.sin(t * math.pi / 2.0)
        row = []
        for k in range(m + 1):
            ang = math.pi * k / m
            row.append(tuple(at((cx + a * s * math.cos(ang), base - rise * s * math.sin(ang)), h)))
        rows.append(row)
    for j in range(n):
        for k in range(m):
            b.face([rows[j][k], rows[j][k + 1], rows[j + 1][k + 1], rows[j + 1][k]], DOOR, smooth=True)
    b.face(rows[0], DOOR)
    pts = [(cx + (a + 4.0) * math.cos(math.pi * k / 16.0), base - (rise + 4.0) * math.sin(math.pi * k / 16.0)) for k in range(17)]
    poly_slab(b, pts, 1.5, 1.5, DOOR)
    for s in d['ear_screws']:
        screw_head(b, s, 0.0, r=4.0)


def plug(b, i, X):
    """One of the six: the flange on the body's foot, the knurled ring, the shell, the cream ferrule."""
    x = lx(X)
    y = T.LOCAL_DOOR_Y - (T.PLUG_Z + T.DOOR_PROUD) / 1000.0
    R = T.PLUG_R
    c = Vector((x, y, BODY_BOTTOM))
    axis = Vector((0.0, 0.0, -1.0))

    def turned(profile, mat, h0=0.0, seg=24):
        b.lathe(c + axis * h0 * MM, axis, [(r * MM, h * MM) for r, h in profile], mat, seg=seg)

    turned([(R, 0.0), (R, 8.0)], STEEL)
    turned([(R, 0.0), (R, 12.0), (R * 1.05, 14.0), (R * 1.05, 36.0), (R, 38.0), (R * 0.92, 40.0)], OLIVE, 8.0)
    turned([(R * 0.92, 0.0), (R * 0.9, 42.0), (R * 0.78, 58.0), (R * 0.6, 64.0)], OLIVE, 48.0)
    if i in T.PLUG_BANDS:
        turned([(R * 0.94, 0.0), (R * 0.94, 5.0)], RED if T.PLUG_BANDS[i] == 'red' else BLUE, 42.0)
    turned([(22.0, 0.0), (22.0, 12.0), (19.0, 16.0), (18.0, 16.0)], CREAM, 106.0)
    return c + axis * 122.0 * MM


def hose(b, points, r, mat, seg=10):
    """A tube along the points with its section carried along (the Builder's tube picks a fresh frame at each ring, which twists on a bend through the vertical)."""
    pts = [Vector(q) for q in points]
    t = (pts[1] - pts[0]).normalized()
    ref = Vector((1.0, 0.0, 0.0)) if abs(t.x) < 0.9 else Vector((0.0, 1.0, 0.0))
    e1 = t.cross(ref).normalized()
    rings = []
    for i, q in enumerate(pts):
        if i == 0:
            nt = t
        elif i == len(pts) - 1:
            nt = (q - pts[i - 1]).normalized()
        else:
            nt = ((q - pts[i - 1]).normalized() + (pts[i + 1] - q).normalized()).normalized()
        e1 = (e1 - nt * e1.dot(nt)).normalized()
        e2 = nt.cross(e1).normalized()
        rings.append([b.bm.verts.new(P(*(q + (e1 * math.cos(2.0 * math.pi * k / seg) + e2 * math.sin(2.0 * math.pi * k / seg)) * r))) for k in range(seg)])
    idx = b.mat(mat)
    for a0, a1 in zip(rings, rings[1:]):
        for k in range(seg):
            f = b.bm.faces.new((a0[k], a0[(k + 1) % seg], a1[(k + 1) % seg], a1[k]))
            f.material_index = idx
            f.smooth = True
    for ring, flip in ((rings[0], True), (rings[-1], False)):
        f = b.bm.faces.new(list(reversed(ring)) if flip else ring)
        f.material_index = idx


def cable(b, i, start):
    """The braided sleeve, down to the plinth and back along it (SAM3D's run: the photos show the loops only from the front)."""
    x, y, z = start
    lean = -0.0055 * (i - 2.5)
    pts = [(x, y, z + 0.004), (x + lean * 0.3, y + 0.012, z - 0.045), (x + lean * 0.8, y + 0.05, PLINTH_TOP + 0.072), (x + lean * 1.6, y + 0.092, PLINTH_TOP + 0.03),
           (x + lean * 2.8, y + 0.105, PLINTH_TOP + 0.019), (x + lean * 4.0, y + 0.075, PLINTH_TOP + 0.019), (x + lean * 4.6, y - 0.005, PLINTH_TOP + 0.019), (x + lean * 4.7, -0.09, PLINTH_TOP + 0.019)]
    hose(b, smooth(pts, 6), 0.017, BRAID, seg=10)


def build_foot(b):
    y_rear = T.LOCAL_DOOR_Y - (T.DEPTH_MM + T.DOOR_PROUD) / 1000.0
    b.box(-0.325, 0.325, y_rear, y_rear + 0.016, PLINTH_TOP, BODY_BOTTOM, DARK)                 # the back behind the plugs (d85's dark bar and what stands under the body)
    b.cylinder((-0.31, y_rear + 0.024, -0.35), (0.31, y_rear + 0.024, -0.35), 0.008, DARK, seg=10)
    for k, X in enumerate(T.PLUG_X):
        cable(b, k, plug(b, k, X))


def build(B, finish, scale=1.0):
    """SM_Cabinet0096 with the interior script's Builder `B` and `finish`; its origin at the old mesh's. `scale` 1 is the trace's millimetres (the check renders against the flats), T.SCALE the game's."""
    import bpy
    b = B('SM_Cabinet0096')
    build_body(b)
    build_lid(b)
    build_side(b)
    for h in T.HINGES:
        hinge(b, h)
    for p in T.parts():
        DRAW[p['kind']](b, p)
    for (a, c), rod_y in zip(T.RAILS, T.RAIL_ROD_Y):
        rail(b, a, c, rod_y)
    build_hours(b)
    build_volt(b)
    build_dome(b)
    build_foot(b)
    obj = finish(b, reuse=True)
    # finish() puts the origin at the bounding box's centre; keep the old mesh's origin, so the layout's centre_m stays valid
    origin = Vector(P(*T.LOCAL_ORIGIN))
    centre = obj.location.copy()
    obj.data.transform(Matrix.Translation(centre - origin))
    obj.location = origin
    obj['bbox_centre'] = tuple(obj['body_centre'])
    obj['body_centre'] = T.LOCAL_ORIGIN
    if scale != 1.0:
        obj.data.transform(Matrix.Scale(scale, 4))
        obj['body_size'] = tuple(v * scale for v in obj['body_size'])
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    bpy.context.view_layer.update()
    return obj
