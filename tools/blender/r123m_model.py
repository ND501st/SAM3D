"""The R-123M radio set as a 3D model, built from the traced front panel (tools/blender/r123m_trace.py).

Run inside Blender through tools/blender/build_r123m.py (the radio alone) or tools/blender/build_cabin_interior.py (the
whole compartment, which calls `build` for SM_RadioR123M).

The set: a casing 428 x 225 mm (r123_refs; 178 mm deep, 162 mm here so the knobs and the cap fit the 174 mm the cabin
plan gives), its front plate with the seven knob wells, the preset drum under its hinged cap, the frequency window in a
raised bezel, the antenna-tuning lamp, the four numbered fixators under their flap, the two sub-band lamp windows, the
engraved plates and the screws, all where the photograph has them; on a shock-mount frame 481 mm wide.

Frame (the model's own, front looks +y, as build_cabin_interior's `build_r123m`): x to the operator's left when he
faces the front (so the plate's right is -x), y forward, z up; origin at the casing's centre.

The cabin supplies the knobs of the functions it simulates (squelch, mode, preset selector, volume, power, antenna
selector, headset): the model leaves those wells empty, with a shaft stub. Pass knobs=True to draw them all, for a
render of the set on its own.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

import r123m_trace as T

# casing, metres
HALF_W = T.PLATE_W_M / 2             # 0.214
HALF_Z = 0.1125                      # the casing is 225 mm high
HALF_D = 0.0871                      # frame and protrusions: 174.2 mm deep in all
YP = 0.0731                          # the plate's front surface
WELL_DEPTH = 0.03
ZC = -HALF_Z + T.PLATE_H_M / 2       # the plate's centre height: the plate starts at the bottom of the casing
FRAME_HALF_W = 0.2405                # the shock-mount frame, 481 mm
FRAME_T = 0.015
FONT = r'C:\Windows\Fonts\ARIALN.TTF'

U = Vector((-1.0, 0.0, 0.0))          # the plate's right (the operator facing the front) in the model frame
V = Vector((0.0, 0.0, 1.0))
N = Vector((0.0, 1.0, 0.0))           # out of the plate


def P(x, y, z):
    """Body frame to Blender, as build_cabin_interior.P (the Builder takes Blender-frame vertices)."""
    return Vector((y, -x, z))


def pos(a, b, h=0.0):
    """A point on (or h out of) the plate: a right, b up from its centre."""
    return Vector((-a, YP + h, ZC + b))


def px(c):
    return T.to_m(*c)


def disc(b, c_px, r_m, h0, h1, mat, seg=28, smooth=True):
    a, bb = px(c_px)
    b.cylinder(pos(a, bb, h0), pos(a, bb, h1), r_m, mat, seg=seg, smooth=smooth)


def ring(b, c_px, r_out_m, r_in_m, h0, h1, mat, seg=48):
    """A flat ring (slab with a round hole)."""
    a, bb = px(c_px)
    outer = [(a + r_out_m * math.cos(2 * math.pi * k / seg), bb + r_out_m * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
    inner = [(a + r_in_m * math.cos(2 * math.pi * k / seg), bb + r_in_m * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
    b.slab(pos(0, 0, h1), U, V, outer, [inner], h1 - h0, mat)


def rect_slab(b, r_px, h0, h1, mat, holes=(), inset=0.0):
    a0, b0, a1, b1 = T.rect_m(r_px)
    outer = [(a0 + inset, b0 + inset), (a1 - inset, b0 + inset), (a1 - inset, b1 - inset), (a0 + inset, b1 - inset)]
    b.slab(pos(0, 0, h1), U, V, outer, list(holes), h1 - h0, mat)


def rect_pts(r_px, inset=0.0):
    a0, b0, a1, b1 = T.rect_m(r_px)
    return [(a0 + inset, b0 + inset), (a1 - inset, b0 + inset), (a1 - inset, b1 - inset), (a0 + inset, b1 - inset)]


def plate_box(b, centre_ab, size_ab, rot_deg, h0, h1, mat):
    """A thin oriented box lying on the plate: centre (a, b), size (along, across), turned rot_deg anticlockwise."""
    th = math.radians(rot_deg)
    ua = U * math.cos(th) + V * math.sin(th)
    vb = -U * math.sin(th) + V * math.cos(th)
    b.obox(pos(centre_ab[0], centre_ab[1], 0.5 * (h0 + h1)), ua, vb, N, (size_ab[0], size_ab[1], h1 - h0), mat)


def screw(b, c_px, h=0.0, r=0.0028):
    a, bb = px(c_px)
    b.cylinder(pos(a, bb, h - 0.0003), pos(a, bb, h + 0.0014), r, 'Metal_Steel', seg=10)
    th = math.radians(25)
    ua = U * math.cos(th) + V * math.sin(th)
    vb = -U * math.sin(th) + V * math.cos(th)
    b.obox(pos(a, bb, h + 0.0016), ua, vb, N, (2 * r * 0.95, 0.0006, 0.0006), 'Metal_Dark')


# ------------------------------------------------------------------------------------------------ text

def _font():
    if os.path.exists(FONT):
        try:
            return bpy.data.fonts.load(FONT, check_existing=True)
        except Exception:
            pass
    return None


def text_mesh(text, height_m, extrude_m=0.0004):
    """Glyph geometry for `text`, centred, in a flat frame (x right, y up, z out): ([verts], [faces])."""
    curve = bpy.data.curves.new('r123m_text', 'FONT')
    curve.body = text
    curve.size = height_m / 0.72          # cap height is about 0.72 of the em
    curve.extrude = extrude_m / 2.0
    curve.resolution_u = 3
    curve.align_x = 'CENTER'
    curve.align_y = 'CENTER'
    f = _font()
    if f is not None:
        curve.font = f
    obj = bpy.data.objects.new('r123m_text', curve)
    bpy.context.scene.collection.objects.link(obj)
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    verts = [Vector(v.co) for v in me.vertices]
    faces = [tuple(p.vertices) for p in me.polygons]
    ev.to_mesh_clear()
    bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.curves.remove(curve)
    return verts, faces


def engrave(b, text, centre_ab, height_m, rot_deg, lift, mat='Paint_Letters', extrude_m=0.0007):
    """Raised letters on the plate: centre (a, b), cap height, turned rot_deg anticlockwise, standing `lift` out of it."""
    verts, faces = text_mesh(text, height_m, extrude_m)
    if not faces:
        return
    th = math.radians(rot_deg)
    cs, sn = math.cos(th), math.sin(th)
    idx = b.mat(mat)
    bmv = []
    for v in verts:
        a = centre_ab[0] + v.x * cs - v.y * sn
        bb = centre_ab[1] + v.x * sn + v.y * cs
        bmv.append(b.bm.verts.new(P(*pos(a, bb, lift + v.z + extrude_m / 2))))
    for fc in faces:
        try:
            f = b.bm.faces.new([bmv[i] for i in fc])
            f.material_index = idx
        except ValueError:
            pass


# ------------------------------------------------------------------------------------------------ knobs

def knob_lever(b, c_px, r_m, angle_deg, length_m):
    a, bb = px(c_px)
    b.cylinder(pos(a, bb, -0.012), pos(a, bb, 0.012), r_m * 0.14, 'Bakelite_Black', seg=16)
    th = math.radians(angle_deg)
    ua = U * math.cos(th) + V * math.sin(th)
    vb = -U * math.sin(th) + V * math.cos(th)
    b.obox(pos(a, bb, 0.006), ua, vb, N, (length_m, r_m * 0.2, 0.011), 'Bakelite_Black')
    b.cylinder(pos(a, bb, 0.011), pos(a, bb, 0.0135), r_m * 0.1, 'Metal_Steel', seg=12)


def knob_three(b, c_px, r_m, angle_deg=90):
    a, bb = px(c_px)
    b.cylinder(pos(a, bb, -0.012), pos(a, bb, 0.012), r_m * 0.2, 'Bakelite_Black', seg=16)
    for k in range(3):
        th = math.radians(angle_deg + 120 * k)
        ua = U * math.cos(th) + V * math.sin(th)
        vb = -U * math.sin(th) + V * math.cos(th)
        c = pos(a + 0.5 * r_m * 0.8 * math.cos(th), bb + 0.5 * r_m * 0.8 * math.sin(th), 0.006)
        b.obox(c, ua, vb, N, (r_m * 0.8, r_m * 0.17, 0.011), 'Bakelite_Black')
    b.cylinder(pos(a, bb, 0.011), pos(a, bb, 0.0135), r_m * 0.08, 'Metal_Steel', seg=12)


def knob_red(b, c_px, r_cap_m):
    a, bb = px(c_px)
    # dark star-shaped base with five lobes, the red cap on it with its slot
    for k in range(5):
        th = math.radians(90 + 72 * k)
        ua = U * math.cos(th) + V * math.sin(th)
        vb = -U * math.sin(th) + V * math.cos(th)
        c = pos(a + 0.55 * r_cap_m * 1.7 * math.cos(th), bb + 0.55 * r_cap_m * 1.7 * math.sin(th), 0.003)
        b.obox(c, ua, vb, N, (r_cap_m * 1.7, r_cap_m * 0.8, 0.012), 'Bakelite_Black')
    b.cylinder(pos(a, bb, -0.01), pos(a, bb, 0.008), r_cap_m * 1.15, 'Bakelite_Black', seg=24)
    b.cylinder(pos(a, bb, 0.008), pos(a, bb, 0.0165), r_cap_m, 'Paint_Red', seg=28)
    b.obox(pos(a, bb, 0.0168), U, V, N, (r_cap_m * 1.5, r_cap_m * 0.22, 0.0014), 'Metal_Dark')
    b.cylinder(pos(a, bb, 0.0165), pos(a, bb, 0.019), r_cap_m * 0.14, 'Metal_Steel', seg=12)


# ------------------------------------------------------------------------------------------------ the set

def arc_pts(c_px, r_in_px, r_out_px, a0, a1, seg=14):
    cx, cy = px(c_px)
    pts = []
    for k in range(seg + 1):
        t = math.radians(a0 + (a1 - a0) * k / seg)
        pts.append((cx + r_out_px * T.SX * math.cos(t), cy + r_out_px * T.SX * math.sin(t)))
    for k in range(seg + 1):
        t = math.radians(a1 - (a1 - a0) * k / seg)
        pts.append((cx + r_in_px * T.SX * math.cos(t), cy + r_in_px * T.SX * math.sin(t)))
    return pts


def build_set(b, knobs=False):
    # ---- the shock-mount frame and the casing's body and lid
    b.box(-FRAME_HALF_W, FRAME_HALF_W, -HALF_D, HALF_D, -HALF_Z - FRAME_T, -HALF_Z, 'Metal_Dark')
    for sx in (-1, 1):                                       # side clamps
        b.box(sx * (HALF_W + 0.001), sx * FRAME_HALF_W, -0.06, 0.06, -HALF_Z - FRAME_T, -0.01, 'Metal_Dark')
        b.cylinder(Vector((sx * (FRAME_HALF_W - 0.006), 0.0, -0.05)), Vector((sx * FRAME_HALF_W, 0.0, -0.05)), 0.007, 'Metal_Steel', seg=12)
        b.cylinder(Vector((sx * (FRAME_HALF_W - 0.006), 0.0, -0.09)), Vector((sx * FRAME_HALF_W, 0.0, -0.09)), 0.007, 'Metal_Steel', seg=12)
    back = -HALF_D + 0.004
    b.box(-HALF_W, HALF_W, back, YP - WELL_DEPTH, -HALF_Z, HALF_Z, 'Paint_Radio')           # the casing behind the plate
    top_z0 = ZC + T.PLATE_H_M / 2                                                             # the lid above the plate
    b.box(-HALF_W - 0.001, HALF_W + 0.001, back, YP + 0.004, top_z0, HALF_Z, 'Paint_Radio')
    b.box(-HALF_W + 0.01, HALF_W - 0.01, YP + 0.004, YP + 0.0075, top_z0 + 0.004, HALF_Z - 0.002, 'Paint_Radio')   # the lid's raised ledge
    # ---- the front plate with its openings
    holes = []
    for w in T.WELLS:
        holes.append(T.circle_m(w['c'], w['r'], 48))
    holes.append(T.circle_m(T.DRUM['c'], T.DRUM['r_opening'], 56))
    for hx, hy, hr in T.HOLES:
        holes.append(T.circle_m((hx, hy), hr, 16))
    for win in T.WINDOWS:
        holes.append(rect_pts(win['rect']))
    holes.append(T.circle_m(T.LENS['c'], T.LENS['r'] * 0.9, 32))
    for c in T.LAMP_WINDOWS['c']:
        holes.append(T.circle_m(c, T.LAMP_WINDOWS['r'], 20))
    holes.append(T.circle_m(T.RED_LAMP['c'], T.RED_LAMP['r'] * 0.8, 16))
    holes.append(T.circle_m(T.CONNECTOR['c'], T.CONNECTOR['r'] * 0.7, 32))
    for c in T.SMALL_SOCKETS:
        holes.append(T.circle_m(c, 20, 18))
    b.slab(pos(0, 0, 0), U, V, T.plate_outline_m(), holes, WELL_DEPTH, 'Paint_Radio')
    # floors of the wells and holes: dark discs a little in front of the casing's face
    floor = -WELL_DEPTH + 0.0015
    for w in T.WELLS:
        a, bb = px(w['c'])
        b.cylinder(pos(a, bb, floor), pos(a, bb, floor + 0.001), w['r'] * T.SX * 0.99, 'Metal_Dark', seg=28)
        # a step ring half-way down, as the photo shows inside the wells
        ring(b, w['c'], w['r'] * T.SX * 0.99, w['r'] * T.SX * 0.84, -WELL_DEPTH * 0.55, -WELL_DEPTH * 0.5, 'Metal_Steel', seg=36)
        a, bb = px(w['c'])
        b.cylinder(pos(a, bb, floor), pos(a, bb, -0.004), 0.0035, 'Metal_Steel', seg=10)       # the shaft
    for hx, hy, hr in T.HOLES:
        a, bb = px((hx, hy))
        b.cylinder(pos(a, bb, floor), pos(a, bb, floor + 0.001), hr * T.SX * 0.99, 'Metal_Dark', seg=14)
    for c in T.SMALL_SOCKETS:
        a, bb = px(c)
        b.cylinder(pos(a, bb, floor), pos(a, bb, -0.006), 20 * T.SX * 0.98, 'Metal_Dark', seg=14)
        b.cylinder(pos(a, bb, -0.006), pos(a, bb, -0.0045), 8 * T.SX, 'Metal_Steel', seg=10)
    # ---- the preset drum under its cap
    a, bb = px(T.DRUM['c'])
    b.cylinder(pos(a, bb, floor), pos(a, bb, floor + 0.001), T.DRUM['r_opening'] * T.SX * 0.99, 'Metal_Dark', seg=40)
    ring(b, T.DRUM['c'], 135 * T.SX, 124 * T.SX, -0.014, -0.0125, 'Paint_Red', seg=48)
    b.cylinder(pos(a, bb, -0.0135), pos(a, bb, -0.0105), 118 * T.SX, 'Metal_Dark', seg=40)
    # the cap: closed, a shallow dome with a stepped rim, a diamond latch, a hinge block below
    b.cylinder(pos(a, bb, 0.0), pos(a, bb, 0.003), T.DRUM['r_opening'] * T.SX * 1.02, 'Metal_Steel', seg=56)
    b.cylinder(pos(a, bb, 0.003), pos(a, bb, 0.009), T.DRUM['r_cap'] * T.SX, 'Metal_Steel', seg=56)
    b.cylinder(pos(a, bb, 0.009), pos(a, bb, 0.0102), (T.DRUM['r_cap'] - 14) * T.SX, 'Metal_Steel', seg=56)
    hw, hh = T.DRUM['latch_w'] / 2 * T.SX, T.DRUM['latch_h'] / 2 * T.SX
    diamond = [(a - hw, bb), (a - hw * 0.4, bb - hh), (a + hw * 0.4, bb - hh), (a + hw, bb), (a + hw * 0.4, bb + hh), (a - hw * 0.4, bb + hh)]
    b.slab(pos(0, 0, 0.0126), U, V, diamond, [], 0.0024, 'Metal_Steel')
    for dx in (-0.006, 0.006):
        b.cylinder(pos(a + dx, bb, 0.0126), pos(a + dx, bb, 0.0142), 0.0028, 'Metal_Steel', seg=10)
    rect_slab(b, T.DRUM['hinge'], 0.0, 0.006, 'Metal_Steel')
    for sx_ in (850, 883):
        screw(b, (sx_, 948), 0.006)
    # ---- the frequency window in its bezel
    fw = T.WINDOWS[0]
    rect_slab(b, fw['bezel'], 0.0, 0.007, 'Paint_Radio', holes=[rect_pts(fw['rect'])])
    a0, b0, a1, b1 = T.rect_m(fw['rect'])
    b.box(-a1, -a0, YP - 0.006, YP - 0.0045, ZC + b0, ZC + b1, 'Glass')                 # the glass, set back in the window
    for k in range(1, 12):                                                              # the scale's ticks
        ta = a0 + (a1 - a0) * k / 12
        b.box(-ta - 0.0003, -ta + 0.0003, YP - 0.0045, YP - 0.0042, ZC + b0 + 0.005, ZC + b0 + 0.014, 'Metal_Dark')
    # ---- the antenna lamp's lens (concentric rings)
    a, bb = px(T.LENS['c'])
    ring(b, T.LENS['c'], T.LENS['r'] * T.SX * 1.12, T.LENS['r'] * T.SX * 0.9, 0.0, 0.0045, 'Metal_Steel', seg=36)
    b.cylinder(pos(a, bb, -0.012), pos(a, bb, -0.003), T.LENS['r'] * T.SX * 0.9, 'Glass', seg=32)
    ring(b, T.LENS['c'], T.LENS['r'] * T.SX * 0.6, T.LENS['r'] * T.SX * 0.52, -0.003, -0.0012, 'Metal_Steel', seg=28)
    ring(b, T.LENS['c'], T.LENS['r'] * T.SX * 0.3, T.LENS['r'] * T.SX * 0.22, -0.003, -0.0012, 'Metal_Steel', seg=20)
    # ---- the D-shaped window, the two sub-band lamp windows, the red lamp
    dw = T.WINDOWS[1]
    a0, b0, a1, b1 = T.rect_m(dw['rect'])
    b.box(-a1, -a0, YP - 0.012, YP - 0.0095, ZC + b0, ZC + b1, 'Glass')
    lp = T.LAMP_WINDOWS
    rect_slab(b, lp['panel'], 0.0, 0.004, 'Bakelite_Black', holes=[T.circle_m(c, lp['r'], 20) for c in lp['c']])
    for c in lp['c']:
        a, bb = px(c)
        b.cylinder(pos(a, bb, -0.012), pos(a, bb, -0.004), lp['r'] * T.SX * 0.95, 'Glass', seg=20)
    a, bb = px(T.RED_LAMP['c'])
    b.cylinder(pos(a, bb, -0.008), pos(a, bb, 0.0025), T.RED_LAMP['r'] * T.SX, 'Paint_Red', seg=18)
    ring(b, T.RED_LAMP['c'], T.RED_LAMP['r'] * T.SX * 1.25, T.RED_LAMP['r'] * T.SX * 0.99, 0.0, 0.0022, 'Metal_Steel', seg=20)
    # ---- the fixator window: raised panel, four numbered buttons, the flap with its finger notch
    fx = T.FIXATORS
    rect_slab(b, fx['panel'], 0.0, 0.004, 'Metal_Steel')
    for c in fx['buttons']:
        a, bb = px(c)
        b.cylinder(pos(a, bb, 0.004), pos(a, bb, 0.0075), fx['r'] * T.SX, 'Bakelite_Black', seg=22)
    rect_slab(b, fx['flap'], 0.004, 0.0062, 'Metal_Steel', holes=[rect_pts(fx['notch'], 0.0)])
    nc = T.rect_m(fx['notch'])
    b.box(-nc[2], -nc[0], YP + 0.004, YP + 0.0062, ZC + nc[1], ZC + nc[3], 'Metal_Dark')   # the notch's dark floor lies in the flap's plane
    a0, b0, a1, b1 = T.rect_m(fx['flap'])
    b.box(-(a1 - 0.03), -(a0 + 0.03), YP + 0.0062, YP + 0.0085, ZC + b1 - 0.002, ZC + b1 + 0.0035, 'Metal_Steel')   # the hinge strip
    # ---- the multi-pin socket and its housing, the small sockets' panel
    rect_slab(b, T.CONNECTOR['housing'], 0.0, 0.004, 'Metal_Dark', holes=[T.circle_m(T.CONNECTOR['c'], T.CONNECTOR['r'] * 0.7, 32)])
    a, bb = px(T.CONNECTOR['c'])
    ring(b, T.CONNECTOR['c'], T.CONNECTOR['r'] * T.SX * 0.9, T.CONNECTOR['r'] * T.SX * 0.7, -0.006, 0.0, 'Metal_Steel', seg=32)
    b.cylinder(pos(a, bb, floor), pos(a, bb, -0.006), T.CONNECTOR['r'] * T.SX * 0.7, 'Metal_Dark', seg=30)
    for k in range(10):                                                                 # the pins
        t = 2 * math.pi * k / 10
        b.cylinder(pos(a + 0.008 * math.cos(t), bb + 0.008 * math.sin(t), -0.012), pos(a + 0.008 * math.cos(t), bb + 0.008 * math.sin(t), -0.006), 0.0012, 'Metal_Steel', seg=6)
    rect_slab(b, T.SOCKET_PANEL, 0.0, 0.003, 'Bakelite_Black', holes=[T.circle_m(c, 20, 18) for c in T.SMALL_SOCKETS])
    # ---- plates: black with engraved letters; arcs round the big selector
    for pl in T.PLATES:
        if pl['id'] in ('left_strip', 'freq_bezel_text'):
            if pl['id'] == 'left_strip':
                a0, b0, a1, b1 = T.rect_m(pl['rect'])
                plate_box(b, ((a0 + a1) / 2, (b0 + b1) / 2), (a1 - a0, b1 - b0), 0, 0.0, 0.0012, 'Bakelite_Black')
            continue
        a0, b0, a1, b1 = T.rect_m(pl['rect'])
        if pl['id'] == 'voltage':
            pts = rect_pts(pl['rect'])
            b.slab(pos(0, 0, 0.0012), U, V, pts, [T.circle_m(T.WELLS[2]['c'], T.WELLS[2]['r'] * 1.02, 40)] + [T.circle_m(c, 20.5, 18) for c in T.SMALL_SOCKETS] + [rect_pts(T.SOCKET_PANEL)], 0.0012, 'Bakelite_Black')
        else:
            rot = pl.get('rot', 0)
            w, h = a1 - a0, b1 - b0
            plate_box(b, ((a0 + a1) / 2, (b0 + b1) / 2), (w, h), 0, 0.0, 0.0012, 'Bakelite_Black')
    for pl in T.PLATES:
        a0, b0, a1, b1 = T.rect_m(pl['rect'])
        cx, cy = (a0 + a1) / 2, (b0 + b1) / 2
        if pl.get('label_at'):
            cx, cy = px(pl['label_at'])
        lines = pl['text']
        hm = pl['h'] * T.SX
        for i, line in enumerate(lines):
            off = ((len(lines) - 1) / 2 - i) * hm * 1.45
            rot = pl.get('rot', 0)
            r = math.radians(rot)
            engrave(b, line, (cx - off * math.sin(r), cy + off * math.cos(r)), hm, rot, 0.0012 if not pl.get('plain') else 0.0004,
                    mat='Paint_Letters' if not pl.get('plain') else 'Metal_Dark')
    for arc in T.ARCS:
        pts = arc_pts(T.SELECTOR_C, T.ARC_R_IN, T.ARC_R_OUT, arc['a0'], arc['a1'])
        b.slab(pos(0, 0, 0.0012), U, V, pts, [], 0.0012, 'Bakelite_Black')
        cx, cy = px(T.SELECTOR_C)
        mid = math.radians((arc['a0'] + arc['a1']) / 2)
        rm = 0.5 * (T.ARC_R_IN + T.ARC_R_OUT) * T.SX
        rot = math.degrees(mid) - 90
        if 90 < (rot % 360) < 270:
            rot += 180
        engrave(b, arc['text'], (cx + rm * math.cos(mid), cy + rm * math.sin(mid)), 15 * T.SX, rot, 0.0012)
        for text, ang in arc['marks']:
            t = math.radians(ang)
            engrave(b, text, (cx + rm * math.cos(t), cy + rm * math.sin(t)), 14 * T.SX, 0, 0.0012)
            b.cylinder(pos(cx + rm * 0.9 * math.cos(t), cy + rm * 0.9 * math.sin(t), 0.0012), pos(cx + rm * 0.9 * math.cos(t), cy + rm * 0.9 * math.sin(t), 0.0024), 0.0022, 'Lamp_Glass', seg=8)
    for text, c, h in T.MARKS:
        a, bb = px(c)
        engrave(b, text, (a, bb), h * T.SX, 0, 0.0075 if h >= 20 else 0.0012, mat='Paint_Letters')
    # ---- screws and side bolts
    for sc in T.SCREWS:
        screw(b, sc, 0.0)
    for c in T.SIDE_BOLTS:                                   # bolt heads on the casing's right-hand side (the photo's right edge)
        _, bb = px(c)
        b.cylinder(Vector((-HALF_W, YP - 0.03, ZC + bb)), Vector((-HALF_W - 0.005, YP - 0.03, ZC + bb)), 0.005, 'Metal_Steel', seg=10)
    for sx in (-1, 1):
        for z in (ZC - 0.045, ZC - 0.095):
            b.cylinder(Vector((sx * HALF_W, YP - 0.04, z)), Vector((sx * (HALF_W + 0.006), YP - 0.04, z)), 0.0075, 'Metal_Steel', seg=12)
    # ---- the knobs the model owns, and (for a render) the ones the cabin supplies
    for w in T.WELLS:
        if w['sim'] is not None and not knobs:
            continue
        r = w['r'] * T.SX
        if w['knob'] == 'red':
            knob_red(b, w['c'], 68 * T.SX)
        elif w['knob'] == 'three':
            knob_three(b, w['c'], r * 0.9)
        elif w['knob'] == 'lever_big':
            knob_lever(b, w['c'], r, -55, 0.0726)
        else:
            knob_lever(b, w['c'], r, 60 if w['id'] != 'volume' else 120, r * 1.5)


def build(B, finish, knobs=False):
    """Build SM_RadioR123M with the interior script's Builder `B` and `finish`."""
    b = B('SM_RadioR123M')
    build_set(b, knobs=knobs)
    obj = finish(b, reuse=True)
    # hard edges where the faces meet at more than 30 degrees: the FBX carries one smoothing group, and Unreal would
    # average the plate's normals into its wells' walls (the plate then shades in streaks)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    return obj
