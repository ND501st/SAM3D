"""SM_CabSeat from its trace (cab_seat_trace.py): the driver's and commander's seat. Built facing forward in the seat's own frame:
x across, y forward, z up, the origin on the cushion's top, the seat's middle. `stand_m` is the height of that top above the floor,
which the arms and the rails reach down to: the raised area under the seats (docs/OSA_INTERIOR_HOLISTIC.md section 24)."""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cab_seat_trace as T
from build_cabin_interior import rrect

MM = 0.001
Y_CUSHION_REAR = -0.206          # the cushion's rear edge, the seat's frame; its front at +0.254
HINGE = Vector((0.0, Y_CUSHION_REAR - 0.072, 0.015))   # the pan's foot, behind the cushion's rear edge
L = math.radians(T.LEAN_DEG)
U = Vector((1.0, 0.0, 0.0))
V = Vector((0.0, -math.sin(L), math.cos(L)))    # up the back
N = U.cross(V)                                  # out of the pan's rear face, down and to the rear
PAN = 'Paint_Interior'
PAD = 'Vinyl_Olive'
STEEL = 'Metal_Steel'


def P(x, y, depth=0.0):
    """A point of the back: mm across and down from its top edge, mm behind the pan's rear face."""
    return HINGE + U * ((x - T.W_MM / 2) * MM) + V * ((T.H_MM - y) * MM) + N * (depth * MM)


def strip(b, p0, p1, width, thick, mat, across=U):
    """A flat bar from p0 to p1, `width` across `across`."""
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    L_ = d.length
    v = d / L_
    n = across.cross(v).normalized()
    b.obox((p0 + p1) * 0.5, across, v, n, (width, L_, thick), mat)


def dome(b, c, axis, r, mat, seg=8):
    b.lathe(c, axis, [(0.0, 0.0), (r, 0.0), (r * 0.82, r * 0.42), (r * 0.42, r * 0.78), (0.0, r * 0.9)], mat, seg=seg)


def ring(c, u, v, w, h, r, seg=5):
    """A closed outline round c in the plane of u, v."""
    o = rrect(0.0, 0.0, w, h, r, seg=seg)
    pts = [c + u * a + v * b_ for a, b_ in o]
    return pts + [pts[0]]


def build_cushion(b):
    w, d = T.W_MM * MM, T.D_CUSHION_MM * MM
    yc = Y_CUSHION_REAR + d / 2
    r = T.CORNER_MM * MM
    c = Vector((0.0, yc, 0.0))
    # the pad, crowned; the pan under it; the rim
    b.rounded_box(c + Vector((0, 0, -0.040)), (w, d, 0.060), r, PAD, seg=5)
    b.rounded_box(c + Vector((0, 0, -0.004)), (w - 0.09, d - 0.09, 0.012), r * 0.8, PAD, seg=5)
    b.rounded_box(c + Vector((0, 0, -0.088)), (w, d, 0.016), r, PAN, seg=5)
    b.tube(ring(c + Vector((0, 0, -0.071)), Vector((1, 0, 0)), Vector((0, 1, 0)), w + 0.002, d + 0.002, r), 0.0065, PAD, seg=8)
    # the rim's domed rivets, along the front and the sides (g088-a)
    for i in range(7):
        x = -0.21 + i * 0.07
        dome(b, Vector((x, yc + d / 2 + 0.0015, -0.071)), Vector((0, 1, 0)), 0.0052, STEEL)
    for s in (-1, 1):
        for i in range(5):
            dome(b, Vector((s * (w / 2 + 0.0015), yc - 0.16 + i * 0.08, -0.071)), Vector((s, 0, 0)), 0.0052, STEEL)


def build_back(b):
    r = T.CORNER_MM * MM
    w, h = T.W_MM * MM, T.H_MM * MM
    mid = P(T.W_MM / 2, T.H_MM / 2)
    # the pad: an upholstered block, its front crowned
    b.rounded_box(P(T.W_MM / 2, T.H_MM / 2, (T.Z['pad_rear'] + T.Z['pad_front'] + 10.0) / 2.0), (w, h, 0.060), r, PAD, u=U, v=V, seg=5)
    b.rounded_box(P(T.W_MM / 2, T.H_MM / 2, T.Z['pad_front'] + 7.0), (w - 0.09, h - 0.09, 0.014), r * 0.8, PAD, u=U, v=V, seg=5)
    # the pan, its rear face at depth 0
    o = rrect(0.0, 0.0, w, h, r, seg=5)
    b.slab(mid, U, V, o, [], abs(T.Z['pad_rear']) * MM, PAN)
    # the rolled rim, and the pad's welt where it meets the pan
    b.tube(ring(P(T.W_MM / 2, T.H_MM / 2, T.Z['pan_rim'] * 0.5), U, V, w, h, r), 0.006, PAN, seg=8)
    b.tube(ring(P(T.W_MM / 2, T.H_MM / 2, T.Z['pad_rear']), U, V, w + 0.002, h + 0.002, r), 0.0065, PAD, seg=8)
    # the rim's rivets on the rear face, and the top edge's on the welt (g088-a shows those from the front)
    for x, y in T.rivets():
        dome(b, P(x, y, 0.8), N, T.RIVET_R_MM * MM, STEEL)
    for i in range(7):
        x = 40.0 + i * 70.0
        dome(b, P(x, -2.0, T.Z['pad_rear']), V, 0.0052, STEEL)
    # the pressed boss, its wing bolt
    x0, y0, x1, y1 = T.BOSS
    bc = P((x0 + x1) / 2, (y0 + y1) / 2)
    b.slab(bc + N * (T.Z['boss'] * MM), U, V, rrect(0.0, 0.0, (x1 - x0) * MM, (y1 - y0) * MM, 0.016, seg=4), [], T.Z['boss'] * MM + 0.002, PAN)
    wc = P(T.WING[0], T.WING[1], T.Z['boss'])
    b.cylinder(wc, wc + N * ((T.Z['wing'] - T.Z['boss']) * MM), 0.006, STEEL, seg=10)
    b.obox(wc + N * ((T.Z['wing'] - T.Z['boss'] - 2.0) * MM), V, N, U, (0.034, 0.008, 0.004), STEEL)
    # the hoop, and the tabs that carry it
    lo = T.Z['hoop']
    n = len(T.HOOP)
    key = [(x, y, T.Z['lug'] + (lo - T.Z['lug']) * min(1.0, min(i, n - 1 - i) / 2.0)) for i, (x, y) in enumerate(T.HOOP)]
    pts = []
    for i in range(n - 1):                     # Catmull-Rom through the traced points, the bent tube's own curve
        p0, p1, p2, p3 = key[max(i - 1, 0)], key[i], key[i + 1], key[min(i + 2, n - 1)]
        for k in range(5):
            t = k / 5.0
            pts.append(P(*[0.5 * (2 * b1 + (c1 - a1) * t + (2 * a1 - 5 * b1 + 4 * c1 - d1) * t * t + (3 * b1 - a1 - 3 * c1 + d1) * t * t * t)
                           for a1, b1, c1, d1 in zip(p0, p1, p2, p3)]))
    pts.append(P(*key[-1]))
    b.tube(pts, T.HOOP_R_MM * MM, PAN, seg=10)
    for x, y in T.LUGS:
        c = P(x, y, T.Z['lug'] * 0.5)
        b.obox(c, U, V, N, (T.LUG_MM * MM, T.LUG_MM * MM, T.Z['lug'] * MM), PAN)
        dome(b, P(x, y, T.Z['lug']), N, 0.0045, STEEL)


def build_stand(b, stand_m):
    foot = -stand_m
    # the hinge arms: flat bars from the pan's side past the pivot, then straight to the rails
    for x in T.ARMS_X:
        top = P(x, T.ARM_TOP_MM, 6.0)
        piv = P(x, T.PIVOT_Y_MM, 6.0)
        strip(b, top, piv, T.ARM_W_MM * MM, 0.006, PAN)
        strip(b, piv, (piv.x, piv.y, foot + 0.02), T.ARM_W_MM * MM, 0.006, PAN)
        b.cylinder(piv - N * 0.006, piv + N * 0.012, 0.011, STEEL, seg=12)
        # the rail along the floor
        b.box(piv.x - 0.019, piv.x + 0.019, piv.y - 0.04, 0.30, foot, foot + 0.022, 'Metal_Dark')
    # the cross rod and its two inner brackets
    x0, x1, y = T.ROD
    a, c = P(x0, y, 6.0), P(x1, y, 6.0)
    b.cylinder(a, c, T.ROD_R_MM * MM, STEEL, seg=10)
    for q in (a, c):
        strip(b, q, (q.x, q.y, foot + 0.02), 0.03, 0.005, PAN)
    # the cushion's frame: two rails under the pan, the two front legs and a tie
    for x in (-0.20, 0.20):
        b.box(x - 0.012, x + 0.012, Y_CUSHION_REAR - 0.04, 0.24, -0.116, -0.096, PAN)
        if stand_m > 0.25:
            b.tube([(x, 0.22, -0.11), (x, 0.24, foot + 0.15), (x, 0.24, foot + 0.02)], 0.011, PAN, seg=10)
        else:                                  # a short post on the raised area
            b.cylinder((x, 0.22, -0.10), (x, 0.22, foot), 0.011, PAN, seg=10)
    if stand_m > 0.25:
        b.tube([(-0.20, 0.24, foot + 0.15), (0.20, 0.24, foot + 0.15)], 0.009, PAN, seg=10)


def build_bag(b):
    x0, y0, x1, y1 = T.BAG
    d = T.Z['hoop'] + T.HOOP_R_MM + T.BAG_THICK_MM / 2.0
    b.rounded_box(P((x0 + x1) / 2, (y0 + y1) / 2, d), ((x1 - x0) * MM, (y1 - y0) * MM, T.BAG_THICK_MM * MM), 0.05, 'Canvas', u=U, v=V, seg=4)
    fx0, fy0, fx1, fy1 = T.FLAP
    fc = ((fx0 + fx1) / 2, (fy0 + fy1) / 2)
    outer = d + T.BAG_THICK_MM / 2.0
    b.rounded_box(P(fc[0], fc[1], outer + 2.0), ((fx1 - fx0) * MM, (fy1 - fy0) * MM, 0.006), 0.03, 'Canvas', u=U, v=V, seg=4)
    sx0, sy0, sx1, sy1 = T.STRAP
    b.obox(P((sx0 + sx1) / 2, (sy0 + sy1) / 2, outer + 7.0), U, V, N, ((sx1 - sx0) * MM, (sy1 - sy0) * MM, 0.004), 'Vinyl_Brown')


def build(B, finish, stand_m):
    b = B('SM_CabSeat')
    build_cushion(b)
    build_back(b)
    build_stand(b, stand_m)
    build_bag(b)
    vs = [v.co for v in b.bm.verts]
    lo = [min(c[i] for c in vs) for i in range(3)]
    hi = [max(c[i] for c in vs) for i in range(3)]
    print('SM_CabSeat local bounds: lo %s hi %s centre %s size %s' % ([round(v, 4) for v in lo], [round(v, 4) for v in hi],
                                                                     [round((a + c) / 2, 4) for a, c in zip(lo, hi)], [round(c - a, 4) for a, c in zip(lo, hi)]))
    return finish(b, reuse=True)
