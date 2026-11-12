"""The ПАНЕЛЬ ИНДИКАЦИИ as a 3D model, from its trace (tools/blender/panel_ind_trace.py). Run inside Blender through
tools/blender/build_panel_ind.py.

Frame (as sl3m_model's): the face looks local +y; the face's right is local -x, z up; the face's centre at the origin, the
face plane at y = 0. In the cabin it stands at ind_panel's centre turned by the panel's facing (+90 degrees: it looks across
the cabin, +x).

The cabin draws the controls (АВАР. ОТКЛ. СЭП, УКЧ, the selector) and the lamp ДВЕРЬ ЗАКРЫТА on top: the mesh carries
everything else, the box, its frame and caps, the plates, ОТКАЗ, the screws and the plugs. What is the photo's and what is
SAM3D's: see the trace.
"""
import math

from mathutils import Vector

from build_cabin_interior import rrect

import panel_ind_trace as T

MM = 0.001
U = Vector((-1.0, 0.0, 0.0))     # the face's right in the model frame
V = Vector((0.0, 0.0, 1.0))      # up
N = Vector((0.0, 1.0, 0.0))      # out of the face
STEEL, DARK, BLACK, RED, PLATE, CREAM, BRASS, OLIVE = ('Metal_Steel', 'Metal_Dark', 'Bakelite_Black', 'Paint_Red',
                                                       'Paint_LightGrey', 'Paint_Letters', 'Metal_Brass', 'Vinyl_Olive')
BODY = 'Paint_Panel'


def at(c, h=0.0):
    a, b = T.to_m(c[0], c[1])
    return Vector((-a, h * MM, b))


def box_mm(b, r, h0, h1, mat):
    x0, y0, x1, y1 = r
    p0, p1 = at((max(x0, x1), max(y0, y1)), h0), at((min(x0, x1), min(y0, y1)), h1)
    b.box(min(p0.x, p1.x), max(p0.x, p1.x), p0.y, p1.y, min(p0.z, p1.z), max(p0.z, p1.z), mat)


def disc(b, c, r_mm, h0, h1, mat, seg=20):
    b.cylinder(at(c, h0), at(c, h1), r_mm * MM, mat, seg=seg)


def lathe(b, c, h0, profile, mat, seg=20):
    b.lathe(at(c, h0), N, [(r * MM, h * MM) for r, h in profile], mat, seg=seg)


def slab(b, outline_mm, h, t, mat, holes=()):
    """A plate with its outline (face mm); its front at h, t mm thick behind."""
    def m(p):
        return T.to_m(p[0], p[1])
    b.slab(tuple(at((T.W_MM / 2, T.H_MM / 2), h)), U, V, [m(p) for p in outline_mm], [[m(p) for p in hole] for hole in holes],
           t * MM, mat)


def rr_slab(b, r, h, t, mat, radius=4.0):
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


def screw(b, c, h, r=1.8):
    lathe(b, c, h, [(r, 0.0), (r, 0.8), (r * 0.75, 1.4), (0.0, 1.6)], STEEL, seg=10)
    thin_bar(b, (c[0] - r * 0.8, c[1]), (c[0] + r * 0.8, c[1]), 0.6, h + 1.2, h + 1.8, DARK)


def rect_outline(r):
    x0, y0, x1, y1 = r
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


# ---- the box and its parts

def build_box(b):
    rr_slab(b, (0.0, 0.0, T.W_MM, T.H_MM), 0.0, T.DEPTH_MM, BODY, radius=T.CORNER_MM)


def build_cap_frame(b):
    x0, y0, x1, y1 = T.CAP_FRAME
    r, w = T.CAP_FRAME_RADIUS, T.CAP_FRAME_RIM
    a, bb = T.to_m((x0 + x1) / 2, (y0 + y1) / 2)
    outer = rrect(a, bb, (x1 - x0) * MM, (y1 - y0) * MM, r * MM)
    inner = rrect(a, bb, (x1 - x0 - 2 * w) * MM, (y1 - y0 - 2 * w) * MM, (r - w) * MM)
    b.slab(tuple(at((T.W_MM / 2, T.H_MM / 2), T.CAP_FRAME_H)), U, V, outer, [inner], T.CAP_FRAME_H * MM, BODY)
    rr_slab(b, (x0 + w, y0 + w, x1 - w, y1 - w), 1.0, 1.0, DARK, radius=r - w)    # the dark ground the caps stand in
    for c in T.CAP_FRAME_SCREWS:
        screw(b, c, 1.0, r=2.2)
    for c in T.CAPS:
        if c.get('lamp'):
            lathe(b, c['c'], 1.0, [(T.CAP_D / 2, 0.0), (T.CAP_D / 2, 1.2)], STEEL, seg=24)   # the cabin draws its lamp
            continue
        r = T.CAP_D / 2
        lathe(b, c['c'], 1.0, [(r, 0.0), (r, 1.2), (r - 0.6, 1.6), (r - 0.6, T.CAP_H - 1.0), (r - 1.4, T.CAP_H),
                               (0.0, T.CAP_H)], BLACK, seg=24)


def build_selector(b):
    slab(b, T.sel_plate_outline(), 1.5, 1.5, BLACK)
    for c in T.SEL_PLATE_SCREWS:
        screw(b, c, 1.5)
    lathe(b, T.SEL_KNOB, 0.0, [(5.5, 0.0), (5.5, 2.0), (0.0, 2.0)], STEEL, seg=16)    # the bush the cabin's knob turns on


def build_otkaz(b):
    rr_slab(b, T.OTKAZ_PLATE, 1.5, 1.5, BLACK, radius=1.5)
    for c in T.OTKAZ_PLATE_SCREWS:
        screw(b, c, 1.5, r=1.5)
    R, L = T.OTKAZ_BEZEL_R, T.OTKAZ_LENS_R
    lathe(b, T.OTKAZ_LAMP, 0.0, [(R, 0.0), (R, 3.0), (R - 1.0, 4.5), (L + 0.5, 5.0), (L + 0.5, 4.0), (0.0, 4.0)], STEEL, seg=28)
    lathe(b, T.OTKAZ_LAMP, 4.0, [(L, 0.0), (L, 2.5), (L * 0.6, 4.5), (0.0, 5.0)], RED, seg=24)


def build_sep(b):
    rr_slab(b, T.SEP_PLATE, 1.5, 1.5, BLACK, radius=2.0)
    rr_slab(b, T.SEP_SUBPLATE, 2.5, 1.0, PLATE, radius=1.5)
    for c in T.SEP_PLATE_SCREWS:
        screw(b, c, 1.5)
    r0, r1 = T.GUARD_RING
    lathe(b, T.SEP_BUTTON, 2.5, [(r1, 0.0), (r1, 2.0), (r0, 2.5), (r0, 1.0), (0.0, 1.0)], BRASS, seg=24)
    lathe(b, T.UKCH_TOGGLE, 2.5, [(r1, 0.0), (r1, 2.0), (r0, 2.5), (r0, 1.0), (0.0, 1.0)], RED, seg=24)


def build_plugs(b):
    # Ш2 hangs under the box: its label plate across the front, the shell below, the cable down
    x0, y0, x1, y1 = T.PLUG_SH2_PLATE
    box_mm(b, (x0, y0, x1, y1), -T.DEPTH_MM + 8.0, -2.0, OLIVE)
    sx0, sy0, sx1, sy1 = T.PLUG_SH2_SHELL
    cx = (sx0 + sx1) / 2
    r = (sx1 - sx0) / 2
    zc = -T.DEPTH_MM / 2
    b.cylinder(at((cx, sy0), zc), at((cx, sy1), zc), r * MM, STEEL, seg=20)
    b.cylinder(at((cx, sy1), zc), at((cx, sy1 + 40.0), zc), 7.0 * MM, BLACK, seg=12)
    # Ш1 off the box's forward side (the face's right, beyond x = W): a squared plug body standing proud of the face, in
    # front of SŁ-1M's frame flange, held by a bracket from the box's side; its label plate towards the cabin, its cable down
    x0, y0, x1, y1 = T.PLUG_SH1
    h0, h1 = T.PLUG_SH1_H
    box_mm(b, (T.W_MM, y0 + 30.0, x0 + 4.0, y1 - 30.0), -12.0, h0 + 4.0, DARK)                  # the bracket
    box_mm(b, (x0 + 4.0, y0, x1, y1), h0, h1, STEEL)                                             # the body
    box_mm(b, (x0 + 10.0, y0 + 8.0, x1 - 6.0, y1 - 8.0), h1, h1 + 1.0, OLIVE)                    # its label plate
    b.cylinder(at(((x0 + x1) / 2, y1), (h0 + h1) / 2), at(((x0 + x1) / 2, y1 + 120.0), (h0 + h1) / 2), 7.0 * MM, BLACK, seg=12)

def build(B, finish):
    """SM_PanelInd with the interior script's Builder `B` and `finish`."""
    import bpy
    b = B('SM_PanelInd')
    build_box(b)
    build_cap_frame(b)
    build_selector(b)
    build_otkaz(b)
    build_sep(b)
    build_plugs(b)
    obj = finish(b, reuse=True)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    bpy.context.view_layer.update()
    return obj
