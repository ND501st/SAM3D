"""The R-123M radio set as a 3D model, built from the 3D trace of its front (tools/blender/r123m_trace.py: positions in mm
from a flattened photograph, and how far each part stands out of the plate).

Run inside Blender through tools/blender/build_r123m.py (the radio alone) or tools/blender/build_cabin_interior.py (the
whole compartment, which calls `build` for SM_RadioR123M).

The set: a casing 406 x 225 mm in front (the bolts on its sides make the 428 mm of `r123_refs`), 150 mm deep behind the
plate (the set is 178 mm deep with its front fittings; the cabin plan gives 174 mm, mon_2112_80 Fig. 3), on a shock-mount
frame 481 mm wide. The front: the flat plate with its seven knob wells, the step at its lower left and the set-back block
under it (connector, toggle), the frequency window's hood standing 13 mm out, the drum's cap 12 mm out, the fixators in
their recess under the hinged flap, the arched window, the lamp plates, the black engraved plates and arcs, the screws.

Frame (the model's own, front looks +y, as build_cabin_interior's `build_r123m`): x to the operator's left when he faces
the front (so the plate's right is -x), y forward, z up; the plate's centre at (0, YP, ZC).

The cabin supplies the knobs of the functions it simulates (squelch, mode, preset selector, volume, power, antenna
selector, headset): the model leaves those wells empty, with a shaft stub, and gives the power and antenna controls a
bushing from the set-back block out to the plate's plane. Pass knobs=True to draw them all, for a render of the set alone.
"""
import math
import os

import bpy
from mathutils import Vector

import r123m_trace as T

MM = 0.001
HALF_W = T.PLATE_W_M / 2             # 0.203
HALF_Z = T.PLATE_H_M / 2             # 0.1125: the plate is the whole front
DEPTH = 0.150                        # the casing behind the plate
YP = 0.0745                          # the plate's front surface (the knobs and the cap stay inside the cabin's 0.179 m)
WELL_DEPTH = -T.Z['well_depth'] * MM
ZC = 0.0                             # the plate's centre height
FRAME_HALF_W = 0.2405                # the shock-mount frame, 481 mm
FRAME_T = 0.015
FONT = r'C:\Windows\Fonts\ARIALN.TTF'

U = Vector((-1.0, 0.0, 0.0))          # the plate's right (the operator facing the front) in the model frame
V = Vector((0.0, 0.0, 1.0))
N = Vector((0.0, 1.0, 0.0))           # out of the plate


def z(key):
    return T.Z[key] * MM


def pos(a, b, h=0.0):
    """A point on (or h out of) the plate: a right, b up from its centre."""
    return Vector((-a, YP + h, ZC + b))


def at(c, h=0.0):
    """A trace point (x, y mm) h out of the plate."""
    a, b = T.to_m(*c)
    return pos(a, b, h)


def P(x, y, z_):
    """Body frame to Blender, as build_cabin_interior.P (the Builder's raw vertices are Blender-frame)."""
    return Vector((y, -x, z_))


def turned(rot_deg):
    th = math.radians(rot_deg)
    return U * math.cos(th) + V * math.sin(th), -U * math.sin(th) + V * math.cos(th)


def disc(b, c, r_mm, h0, h1, mat, seg=28):
    b.cylinder(at(c, h0), at(c, h1), r_mm * MM, mat, seg=seg)


def ring(b, c, r_out_mm, r_in_mm, h0, h1, mat, seg=48):
    """A flat ring (a slab with a round hole), between heights h0 and h1 (m)."""
    a, bb = T.to_m(*c)
    ro, ri = r_out_mm * MM, r_in_mm * MM
    outer = [(a + ro * math.cos(2 * math.pi * k / seg), bb + ro * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
    inner = [(a + ri * math.cos(2 * math.pi * k / seg), bb + ri * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
    b.slab(pos(0, 0, h1), U, V, outer, [inner], h1 - h0, mat)


def flat(b, pts, h0, h1, mat, holes=()):
    """A slab from outline pts (plate metres) between heights h0 < h1 out of the plate."""
    b.slab(pos(0, 0, h1), U, V, pts, list(holes), h1 - h0, mat)


def rect_pts(r, inset=0.0):
    a0, b0, a1, b1 = T.rect_m(r)
    return [(a0 + inset, b0 + inset), (a1 - inset, b0 + inset), (a1 - inset, b1 - inset), (a0 + inset, b1 - inset)]


def rect_minus_circle(r, c, rad, step=0.6):
    """A mm rectangle's outline with a round bite out of it (circle c, rad mm) where it reaches in: plate metres. The
    rectangle's edge is walked in small steps and every point inside the circle is pushed out onto it."""
    x0, y0, x1, y1 = r
    pts = []
    for (ax, ay, bx, by) in ((x0, y1, x1, y1), (x1, y1, x1, y0), (x1, y0, x0, y0), (x0, y0, x0, y1)):
        n = max(1, int(math.hypot(bx - ax, by - ay) / step))
        for k in range(n):
            x, y = ax + (bx - ax) * k / n, ay + (by - ay) * k / n
            dx, dy = x - c[0], y - c[1]
            d = math.hypot(dx, dy)
            if d < rad:
                x, y = c[0] + dx / d * rad, c[1] + dy / d * rad
            pts.append(T.to_m(x, y))
    return pts


def sector_pts(c, r0, r1, a0, a1, seg=24, cap=True):
    """An annular sector round c (mm), radii r0 < r1 mm, angles a0 < a1 degrees anticlockwise (up +90), with round ends."""
    ca, cb = T.to_m(*c)
    pts = []
    rm, hw = (r0 + r1) / 2, (r1 - r0) / 2
    for k in range(seg + 1):
        t = math.radians(a0 + (a1 - a0) * k / seg)
        pts.append((ca + r1 * MM * math.cos(t), cb + r1 * MM * math.sin(t)))
    if cap:
        e = math.radians(a1)
        ex, ey = ca + rm * MM * math.cos(e), cb + rm * MM * math.sin(e)
        for k in range(1, 6):
            t = e + math.pi * k / 6
            pts.append((ex + hw * MM * math.cos(t), ey + hw * MM * math.sin(t)))
    for k in range(seg + 1):
        t = math.radians(a1 - (a1 - a0) * k / seg)
        pts.append((ca + r0 * MM * math.cos(t), cb + r0 * MM * math.sin(t)))
    if cap:
        e = math.radians(a0)
        ex, ey = ca + rm * MM * math.cos(e), cb + rm * MM * math.sin(e)
        for k in range(1, 6):
            t = e + math.pi + math.pi * k / 6
            pts.append((ex + hw * MM * math.cos(t), ey + hw * MM * math.sin(t)))
    return pts


def screw(b, c, h=0.0, r_mm=2.6, mat='Metal_Steel', slot_deg=25):
    a, bb = T.to_m(*c)
    r = r_mm * MM
    b.lathe(pos(a, bb, h - 0.0003), N, [(r, 0.0), (r, 0.0011), (r * 0.8, 0.0017), (0.0, 0.0018)], mat, seg=12)
    ua, vb = turned(slot_deg)
    b.obox(pos(a, bb, h + 0.0017), ua, vb, N, (2 * r * 0.9, 0.00055, 0.0006), 'Metal_Dark')


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


def _place_glyphs(b, verts, faces, frame, mat):
    idx = b.mat(mat)
    bmv = [b.bm.verts.new(P(*frame(v))) for v in verts]
    for fc in faces:
        try:
            f = b.bm.faces.new([bmv[i] for i in fc])
            f.material_index = idx
        except ValueError:
            pass


def engrave(b, text, c, h_mm, rot_deg, lift, mat='Paint_Letters', extrude_m=0.0006, tilt_deg=0.0):
    """Raised letters: centre c (trace mm), cap height, turned rot_deg anticlockwise, standing on a surface `lift` (m)
    out of the plate; tilt_deg leans the letters' plane back about their middle (a sloping face)."""
    verts, faces = text_mesh(text, h_mm * MM, extrude_m)
    if not faces:
        return
    ca, cb = T.to_m(*c)
    th, tl = math.radians(rot_deg), math.radians(tilt_deg)
    cs, sn = math.cos(th), math.sin(th)

    def frame(v):
        vy = v.y * math.cos(tl)
        a = ca + v.x * cs - vy * sn
        bb = cb + v.x * sn + vy * cs
        return pos(a, bb, lift + v.y * math.sin(tl) + v.z + extrude_m / 2)

    _place_glyphs(b, verts, faces, frame, mat)


def engrave_arc(b, text, centre, r_mm, mid_deg, h_mm, lift, mat='Paint_Letters', extrude_m=0.0006):
    """Letters set one by one along an arc round centre (trace mm), radius r_mm, centred on mid_deg. Below the centre
    (180..360 degrees) they read left to right with their tops towards it; elsewhere with their tops away from it."""
    below = 180.0 < (mid_deg % 360.0) < 360.0
    glyphs = []
    for ch in text:
        if ch == ' ':
            glyphs.append((None, None, 0.32 * h_mm))
            continue
        verts, faces = text_mesh(ch, h_mm * MM, extrude_m)
        w = (max(v.x for v in verts) - min(v.x for v in verts)) / MM if verts else 0.4 * h_mm
        glyphs.append((verts, faces, w + 0.14 * h_mm))
    total = sum(g[2] for g in glyphs)
    ca, cb = T.to_m(*centre)
    s = -total / 2
    for verts, faces, adv in glyphs:
        mid = s + adv / 2
        s += adv
        if verts is None:
            continue
        ang = math.radians(mid_deg) + (1 if below else -1) * mid / r_mm
        ax, ay = ca + r_mm * MM * math.cos(ang), cb + r_mm * MM * math.sin(ang)
        rot = ang + (math.pi / 2 if below else -math.pi / 2)
        cs, sn = math.cos(rot), math.sin(rot)
        gx = (max(v.x for v in verts) + min(v.x for v in verts)) / 2

        def frame(v, ax=ax, ay=ay, cs=cs, sn=sn, gx=gx):
            x = v.x - gx
            return pos(ax + x * cs - v.y * sn, ay + x * sn + v.y * cs, lift + v.z + extrude_m / 2)

        _place_glyphs(b, verts, faces, frame, mat)


def surface_h(c):
    """How far the surface at trace point c stands out of the plate (m): the top of the plate, strip or arc it lies on, so
    a screw there sits on it rather than in it."""
    x, y = c
    h = 0.0
    for pl in T.PLATES:
        x0, y0, x1, y1 = pl['rect']
        if x0 <= x <= x1 and y0 <= y <= y1:
            base = z(pl['z']) if pl.get('z') else 0.0
            h = max(h, base + (z('subband_plate') if pl['id'] == 'subband' else z('label_plate')))
    x0, y0, x1, y1 = T.VOLT_PLATE
    if x0 <= x <= x1 and y0 <= y <= y1:
        h = max(h, z('voltage_plate'))
    for arc in T.ARCS:
        wc = T.well(arc['well'])['c']
        r = math.hypot(x - wc[0], y - wc[1])
        ang = math.degrees(math.atan2(wc[1] - y, x - wc[0])) % 360.0
        a0, a1 = arc['a'][0] % 360.0, arc['a'][1] % 360.0
        inside = (a0 <= ang <= a1) if a0 <= a1 else (ang >= a0 or ang <= a1)
        if arc['r'][0] - 1.0 <= r <= arc['r'][1] + 1.0 and inside:
            h = max(h, z('label_plate'))
    return h


def holes_in(rect, extra=0.4):
    """The round holes (blind holes, brass shafts) that fall inside a plate's rect: cut them through it too."""
    x0, y0, x1, y1 = rect
    return [T.circle_m((x, y), r + extra, 16) for x, y, r in T.HOLES + T.BRASS_IN_HOLE if x0 < x < x1 and y0 < y < y1]


# ------------------------------------------------------------------------------------------------ knobs

def knob_bar(b, c, d_m, angle_deg, h0=-0.008):
    """The bakelite T-bar knob (`r123_kuzmin`), d_m across, its pointer end towards angle_deg: the cabin's SM_KnobBar shape
    (tools/blender/build_cabin_parts.py), so the model's knobs and the cabin's match."""
    a, bb = T.to_m(*c)
    b.lathe(pos(a, bb, h0), N, [(0.34 * d_m, 0.0), (0.34 * d_m, 0.3 * d_m), (0.3 * d_m, 0.36 * d_m), (0.0, 0.36 * d_m)],
            'Bakelite_Black', seg=28)
    th = math.radians(angle_deg - 90)                 # the outline's +y (the pointer) turned to angle_deg
    outline = [(-0.08, 0.0), (-0.14, -0.4), (-0.1, -0.46), (0.1, -0.46), (0.14, -0.4), (0.08, 0.0), (0.12, 0.38),
               (0.05, 0.5), (-0.05, 0.5), (-0.12, 0.38)]
    pts = [(a + d_m * (x * math.cos(th) - y * math.sin(th)), bb + d_m * (x * math.sin(th) + y * math.cos(th)))
           for x, y in outline]
    flat(b, pts, h0 + 0.06 * d_m, h0 + 0.62 * d_m, 'Bakelite_Black')
    b.cylinder(pos(a, bb, h0 + 0.62 * d_m), pos(a, bb, h0 + 0.66 * d_m), 0.07 * d_m, 'Metal_Steel', seg=12)


def knob_red(b, c, r_cap_m):
    """The antenna-tuning knob (`r123_kuzmin`): a black hub of four broad arms with round ends (a cross, turned a little),
    the red cap on it with a raised bar across its face and the shaft's end, and the lock latch hanging under it."""
    a, bb = T.to_m(*c)
    arm = r_cap_m * 1.85
    for k in range(4):
        ua, vb = turned(8 + 90 * k)
        th = math.radians(8 + 90 * k)
        b.obox(pos(a + 0.5 * arm * math.cos(th), bb + 0.5 * arm * math.sin(th), -0.002), ua, vb, N,
               (arm, r_cap_m * 0.75, 0.014), 'Bakelite_Black')
        e = (a + arm * math.cos(th), bb + arm * math.sin(th))
        b.cylinder(pos(e[0], e[1], -0.009), pos(e[0], e[1], 0.005), r_cap_m * 0.375, 'Bakelite_Black', seg=16)
    b.cylinder(pos(a, bb, -0.012), pos(a, bb, 0.007), r_cap_m * 1.12, 'Bakelite_Black', seg=28)
    b.lathe(pos(a, bb, 0.007), N, [(r_cap_m * 0.98, 0.0), (r_cap_m, 0.001), (r_cap_m, 0.0085), (r_cap_m * 0.93, 0.0095),
                                   (0.0, 0.0095)], 'Paint_Red', seg=32)
    b.obox(pos(a, bb, 0.017), V, -U, N, (r_cap_m * 1.75, r_cap_m * 0.42, 0.004), 'Paint_Red')     # the bar, top to bottom
    b.cylinder(pos(a, bb, 0.0165), pos(a, bb, 0.019), r_cap_m * 0.13, 'Metal_Steel', seg=12)
    b.obox(pos(a, bb - r_cap_m * 1.35, 0.004), V, -U, N, (r_cap_m * 0.9, r_cap_m * 0.22, 0.004), 'Metal_Steel')
    b.cylinder(pos(a, bb - r_cap_m * 1.8, 0.0), pos(a, bb - r_cap_m * 1.8, 0.008), r_cap_m * 0.2, 'Metal_Steel', seg=12)


def knob_bat(b, c, r_m, h0, h1):
    """A bat-handle switch: a black stalk with a round head, from h0 out to h1."""
    a, bb = T.to_m(*c)
    L = h1 - h0
    b.lathe(pos(a, bb, h0), N, [(r_m * 0.55, 0.0), (r_m * 0.55, 0.4 * L), (r_m * 0.75, 0.55 * L), (r_m, 0.75 * L),
                                (r_m * 0.9, 0.9 * L), (r_m * 0.5, 0.98 * L), (0.0, L)], 'Bakelite_Black', seg=18)


# ------------------------------------------------------------------------------------------------ the parts

def build_casing(b):
    back = YP - DEPTH
    # the shock-mount frame and its side clamps
    b.box(-FRAME_HALF_W, FRAME_HALF_W, back + 0.004, YP - 0.012, -HALF_Z - FRAME_T, -HALF_Z, 'Metal_Dark')
    mid = YP - DEPTH / 2
    for sx in (-1, 1):
        b.box(sx * (HALF_W + 0.011), sx * FRAME_HALF_W, mid - 0.06, mid + 0.06, -HALF_Z - FRAME_T, -HALF_Z + 0.1, 'Metal_Dark')
        for zz in (-0.05, -0.09):
            b.cylinder(Vector((sx * (FRAME_HALF_W - 0.006), mid, zz)), Vector((sx * FRAME_HALF_W, mid, zz)), 0.007,
                       'Metal_Steel', seg=12)
    # the casing behind the plate; its top with four ribs running back; the bolts on its sides (to 428 mm)
    b.box(-HALF_W, HALF_W, back, YP - WELL_DEPTH, -HALF_Z, HALF_Z, 'Paint_Radio')
    for xr in (-0.15, -0.05, 0.05, 0.15):
        b.box(xr - 0.004, xr + 0.004, back + 0.01, YP - 0.035, HALF_Z, HALF_Z + 0.004, 'Paint_Radio')
    for y_mm in T.SIDE_BOLTS:
        _, bz = T.to_m(0.0, y_mm)
        for sx in (-1, 1):
            b.cylinder(Vector((sx * HALF_W, YP - 0.02, ZC + bz)), Vector((sx * (HALF_W + 0.011), YP - 0.02, ZC + bz)),
                       0.0055, 'Metal_Steel', seg=6)
    # the block under the plate's lower left, set back, filling the space behind the step
    a0, b0, a1, b1 = T.rect_m((0.0, T.STEP_Y, T.NOTCH_X, T.H_MM))
    b.box(-a1, -a0, YP - WELL_DEPTH, YP + z('lower_block'), ZC + b0, ZC + b1, 'Paint_Radio')


def plate_holes():
    holes = []
    for w in T.WELLS:
        holes.append(T.circle_m(w['c'], w['r'], 48))
    holes.append(T.circle_m(T.DRUM['c'], T.DRUM['r_opening'], 56))
    for x, y, r in T.HOLES + T.BRASS_IN_HOLE:
        holes.append(T.circle_m((x, y), r, 16))
    holes.append(T.arch_m(T.DWIN['rect']))
    holes.append(T.circle_m(T.LENS['c'], T.LENS['r'] * 0.72, 28))
    for c in T.LAMP_PLATE['c']:
        holes.append(T.circle_m(c, T.LAMP_PLATE['r'], 20))
    holes.append(T.circle_m(T.RED_LAMP['c'], T.RED_LAMP['r'] * 0.8, 16))
    holes.append(T.rounded_rect_m(T.FIXATORS['recess'], 2.0))
    holes.append(T.rounded_rect_m(T.POCKET, 3.0))
    return holes


def build_plate(b):
    flat(b, T.plate_outline_m(), -WELL_DEPTH, 0.0, 'Paint_Radio', holes=plate_holes())
    floor = -WELL_DEPTH + 0.0015
    for w in T.WELLS:
        disc(b, w['c'], w['r'] * 0.99, floor, floor + 0.001, 'Duct_Grey')               # grey inside (r123_kuzmin)
        ring(b, w['c'], w['r'] * 0.99, w['r'] * 0.84, -WELL_DEPTH * 0.55, -WELL_DEPTH * 0.5, 'Metal_Steel', seg=36)
        disc(b, w['c'], 3.5, floor, -0.004, 'Metal_Steel', seg=10)                       # the shaft
    sel = T.well('selector')
    a, bb = T.to_m(*sel['c'])
    r = sel['r'] * MM
    b.lathe(pos(a, bb, -WELL_DEPTH + 0.002), N, [(r * 0.93, 0.0), (r * 0.93, 0.006), (r * 0.86, 0.009), (r * 0.35, 0.0115),
                                                  (0.0, 0.0115)], 'Metal_Steel', seg=48)
    for x, y, rr in T.HOLES:
        disc(b, (x, y), rr * 0.99, floor, floor + 0.001, 'Metal_Dark', seg=14)
    for x, y, rr in T.BRASS_IN_HOLE:                    # a brass slotted shaft in its hole
        disc(b, (x, y), rr * 0.98, -0.012, -0.011, 'Metal_Dark', seg=14)
        screw(b, (x, y), -0.0025, r_mm=rr * 0.8, mat='Metal_Brass', slot_deg=-30)
    # the pocket under the voltage plate and its three bat handles
    pa0, pb0, pa1, pb1 = T.rect_m(T.POCKET)
    b.box(-pa1, -pa0, YP + z('pocket') - 0.001, YP + z('pocket'), ZC + pb0, ZC + pb1, 'Metal_Dark')
    for c in T.BAT_HANDLES:
        knob_bat(b, c, 3.6 * MM, z('pocket'), 0.003)


def build_hood(b):
    """The frequency window's hood: a box standing 13 mm out (its top runs into the casing's top edge), the window in it
    with the scale behind, and the sloping lip under it carrying ЧАСТОТА x100 (кГц) and two screws."""
    hd = T.HOOD
    top = z('hood_top')
    flat(b, T.rounded_rect_m(hd['body'], 1.5), 0.0, top, 'Paint_Radio', holes=[T.rounded_rect_m(hd['window'], 1.0)])
    wa0, wb0, wa1, wb1 = T.rect_m(hd['window'])
    # the scale seen through the window: a pale card set back in it, a hairline, ticks every 25 kHz (long at each 100) and
    # the figures of six hundreds of kHz, the red cursor
    gl = z('hood_glass')
    b.box(-wa1, -wa0, YP + gl - 0.0015, YP + gl, ZC + wb0, ZC + wb1, 'Paint_Letters')
    for k in range(1, 24):
        ta = wa0 + (wa1 - wa0) * k / 24
        long_ = k % 4 == 2
        b.box(-ta - 0.00025, -ta + 0.00025, YP + gl, YP + gl + 0.0003, ZC + wb0 + 0.004,
              ZC + wb0 + (0.011 if long_ else 0.008), 'Metal_Dark')
    b.box(-wa1, -wa0, YP + gl, YP + gl + 0.0003, ZC + wb0 + 0.0038, ZC + wb0 + 0.0043, 'Metal_Dark')
    for k, fig in ((2, '245'), (6, '246'), (10, '247'), (14, '248'), (18, '249'), (22, '250')):
        x_mm = hd['window'][0] + (hd['window'][2] - hd['window'][0]) * k / 24
        engrave(b, fig, (x_mm, hd['window'][3] - 17.5), 4.2, 0, gl, mat='Metal_Dark', extrude_m=0.0003)
    xc = 0.5 * (wa0 + wa1)
    b.box(-xc - 0.0004, -xc + 0.0004, YP + gl + 0.0003, YP + gl + 0.0008, ZC + wb0 + 0.002, ZC + wb1 - 0.002, 'Paint_Red')
    # the lip: from the hood's front at its top edge down to 8 mm at its bottom, as a prism across it
    la0, lb0, la1, lb1 = T.rect_m(hd['lip'])
    low = z('hood_lip_bottom')
    section = [(YP, ZC + lb1), (YP + top, ZC + lb1), (YP + low, ZC + lb0 + 0.002), (YP + low - 0.002, ZC + lb0),
               (YP, ZC + lb0)]
    b.prism(section, -la1, -la0, 'Paint_Radio')
    slope = math.degrees(math.atan2(top - low, lb1 - lb0))

    def lip_h(y_mm):
        return top - (top - low) * (y_mm - hd['lip'][1]) / (hd['lip'][3] - hd['lip'][1])

    text, c, h = hd['label']
    engrave(b, text, c, h, 0, lip_h(c[1]) - 0.0003, mat='Metal_Dark', extrude_m=0.0006, tilt_deg=slope)
    for c in hd['screws']:
        screw(b, c, lip_h(c[1]) - 0.0004)


def build_drum(b):
    """The preset drum's opening (dark inside, the brass rim) and its cap, closed (`r123_kuzmin`): a flat disc with a
    rolled rim, a recess holding the round lock knob and its lever, a black strip round the upper left (ОТКР. -> ЗАКР.,
    three screws); the hinge bracket under it and the plate under that."""
    d = T.DRUM
    c = d['c']
    a, bb = T.to_m(*c)
    disc(b, c, d['r_opening'] * 0.99, -WELL_DEPTH + 0.0015, -WELL_DEPTH + 0.0025, 'Metal_Dark', seg=40)
    ring(b, c, d['r_opening'], d['r_opening'] - 1.6, -0.004, 0.0005, 'Metal_Brass', seg=56)     # the brass rim
    rc = d['r_cap'] * MM
    cap = T.CAP
    rr = cap['recess_r'] * rc
    rim, face, rec = z('cap_rim'), z('cap_face'), z('cap_recess')
    b.lathe(pos(a, bb, 0.0), N, [(rc * 1.0, 0.0), (rc * 1.02, 0.003), (rc, rim - 0.001), (rc * 0.96, rim),
                                 (rc * 0.92, face), (rr * 1.04, face), (rr, face - 0.001), (rr, rec), (0.0, rec)],
            'Paint_Radio', seg=64)
    rk = cap['knob_r'] * rc
    b.lathe(pos(a, bb, rec), N, [(rk, 0.0), (rk, 0.006), (rk * 0.94, 0.0075), (0.0, 0.0075)], 'Bakelite_Black', seg=36)
    for ang, length, width in ((cap['lever_a'], rk * 1.55, rk * 0.62), (cap['nub_a'], rk * 1.3, rk * 0.3)):
        th = math.radians(ang)
        ua, vb = turned(ang)
        b.obox(pos(a + 0.5 * length * math.cos(th), bb + 0.5 * length * math.sin(th), rec + 0.0035), ua, vb, N,
               (length, width, 0.006), 'Bakelite_Black')
    b.cylinder(pos(a, bb, rec + 0.0075), pos(a, bb, rec + 0.0085), rk * 0.15, 'Metal_Steel', seg=12)
    r0, r1 = cap['strip_r'][0] * d['r_cap'], cap['strip_r'][1] * d['r_cap']
    flat(b, sector_pts(c, r0, r1, cap['strip_a'][0], cap['strip_a'][1], cap=False), face, face + 0.0012, 'Bakelite_Black')
    rm = 0.5 * (r0 + r1)
    for ang in cap['screws_a']:
        t = math.radians(ang)
        screw(b, (c[0] + rm * math.cos(t), c[1] - rm * math.sin(t)), face + 0.0010, r_mm=2.2)
    for text, ang in cap['texts']:
        engrave_arc(b, text, c, rm, ang, 2.4, face + 0.0012)
    for k in range(9):                                   # the arrow between them: a row of dashes along the strip
        t = 146 - 5.5 * k
        tr = math.radians(t)
        p_ = (c[0] + rm * math.cos(tr), c[1] - rm * math.sin(tr))
        ua, vb = turned(t + 90)
        b.obox(at(p_, face + 0.0014), ua, vb, N, (0.0022 if k < 8 else 0.003, 0.0005, 0.0004), 'Paint_Letters')
    # the hinge: a bracket on the plate under the cap, two lugs and a knuckle across them, its pins
    flat(b, rect_pts(d['hinge']), 0.0, 0.004, 'Metal_Steel')
    h0, hb0, h1, hb1 = T.rect_m(d['hinge'])
    for side in (-1, 1):
        x_ = 0.5 * (h0 + h1) + side * 0.4 * (h1 - h0)
        b.box(-x_ - 0.0025, -x_ + 0.0025, YP + 0.004, YP + 0.010, ZC + 0.5 * (hb0 + hb1), ZC + hb1 + 0.002, 'Metal_Steel')
    b.cylinder(pos(h0 + 0.004, hb1, 0.0075), pos(h1 - 0.004, hb1, 0.0075), 0.0028, 'Metal_Steel', seg=12)
    for c_ in d['hinge_pins']:
        disc(b, c_, 1.6, 0.004, 0.0062, 'Metal_Steel', seg=10)
    flat(b, T.rounded_rect_m(d['plate'], 2.0), 0.0, 0.0015, 'Paint_Radio')
    for c_ in d['plate_screws']:
        screw(b, c_, 0.0015)


def build_fixators(b):
    """The preset fixators in their recess: the panel with the four numbered buttons, its screws and rivets, the hinged
    flap over its lower half (its top stepped down in the middle) with the spring and the arched finger notch."""
    fx = T.FIXATORS
    rec = z('fixator_recess')
    ra0, rb0, ra1, rb1 = T.rect_m(fx['recess'])
    b.box(-ra1, -ra0, YP + rec * 2 - 0.001, YP + rec * 2, ZC + rb0, ZC + rb1, 'Metal_Dark')      # the recess's floor
    panel_top = rec + 0.0005
    flat(b, rect_pts(fx['panel']), rec * 2, panel_top, 'Metal_Steel')
    for c in fx['buttons']:
        b.lathe(at(c, panel_top), N, [(fx['r'] * MM, 0.0), (fx['r'] * MM, 0.0022), (fx['r'] * 0.85 * MM, 0.003),
                                      (0.0, 0.003)], 'Bakelite_Black', seg=24)
    for t, c, h in T.MARKS:
        engrave(b, t, c, h, 0, panel_top + 0.003, mat='Paint_Letters', extrude_m=0.0003)
    for c in fx['screws']:
        screw(b, c, panel_top)
    for c in fx['rivets']:
        b.lathe(at(c, panel_top), N, [(1.3 * MM, 0.0), (1.2 * MM, 0.0008), (0.0, 0.0012)], 'Metal_Steel', seg=10)
    x0, y0, x1, y1 = fx['flap']
    tx0, ty, tx1 = fx['flap_tab']
    outline = [T.to_m(x, y) for x, y in ((x0, y1), (x1, y1), (x1, y0), (tx1, y0), (tx1, ty), (tx0, ty), (tx0, y0), (x0, y0))]
    flap = z('fixator_flap')
    flat(b, outline, flap - 0.0025, flap, 'Metal_Steel', holes=[T.arch_m(fx['notch'], seg=20)])
    flat(b, T.arch_m(fx['notch'], seg=20), panel_top - 0.0005, panel_top, 'Metal_Dark')
    sx, sy0, sy1 = fx['spring']
    for k in range(8):                                   # the spring: a coil along the hinge, by its slot
        disc(b, (sx - 3.5 + k, sy0 + 1.0), 0.9, flap, flap + 0.0012, 'Metal_Steel', seg=8)
    flat(b, rect_pts((sx - 0.6, sy0, sx + 0.6, sy1)), flap, flap + 0.0004, 'Metal_Dark')          # its slot
    b.cylinder(at((x0 + 2, y0), flap - 0.001), at((x1 - 2, y0), flap - 0.001), 0.0016, 'Metal_Steel', seg=10)   # the hinge


def build_windows_and_lamps(b):
    # the arched window: its bezel, the sub-band dial behind it with a pointer
    dw = T.DWIN
    flat(b, T.arch_m(dw['rect'], grow=dw['bezel_grow']), 0.0, z('dwin_bezel'), 'Paint_Radio', holes=[T.arch_m(dw['rect'])])
    a0, b0, a1, b1 = T.rect_m(dw['rect'])
    gl = z('dwin_glass')
    b.box(-a1, -a0, YP + gl - 0.0015, YP + gl, ZC + b0, ZC + b1, 'Paint_Letters')
    xc = 0.5 * (a0 + a1)
    b.box(-xc - 0.0005, -xc + 0.0005, YP + gl, YP + gl + 0.0005, ZC + b0 + 0.003, ZC + b1 - 0.006, 'Metal_Dark')
    # the sub-band lamps' plate: raised steel, two lens holes (the cabin's lamps), I and II, screws
    lp = T.LAMP_PLATE
    top = z('lamp_plate')
    flat(b, T.rounded_rect_m(lp['rect'], 2.0), 0.0, top, 'Metal_Steel', holes=[T.circle_m(c, lp['r'], 20) for c in lp['c']])
    for c in lp['screws']:
        screw(b, c, top, r_mm=2.2)
    for c, mark in zip(lp['c'], ('I', 'II')):
        engrave(b, mark, (c[0] + (7.5 if mark == 'I' else -8.0), c[1]), 3.5, 0, top, mat='Metal_Dark', extrude_m=0.0004)
    # the antenna-tuning lamp's lens: a ring standing out, the yellowed lens set in it with its rings
    L = T.LENS
    ring(b, L['c'], L['r'], L['r'] * 0.72, 0.0, z('lens_ring'), 'Metal_Steel', seg=36)
    disc(b, L['c'], L['r'] * 0.72, -0.006, z('lens_ring') - 0.0015, 'Paint_Letters', seg=32)
    for f in (0.5, 0.25):
        ring(b, L['c'], L['r'] * f, L['r'] * f - 0.8, z('lens_ring') - 0.0015, z('lens_ring') - 0.001, 'Metal_Steel', seg=24)
    # the red lamp
    R = T.RED_LAMP
    b.lathe(at(R['c'], -0.004), N, [(R['r'] * 0.8 * MM, 0.0), (R['r'] * 0.8 * MM, 0.0055), (R['r'] * 0.5 * MM, 0.007),
                                    (0.0, 0.0072)], 'Paint_Red', seg=18)
    ring(b, R['c'], R['r'] * 1.2, R['r'] * 0.8, 0.0, 0.0022, 'Metal_Steel', seg=20)


def build_plates(b):
    lp_ = z('label_plate')
    for pl in T.PLATES:
        base = z(pl['z']) if pl.get('z') else 0.0
        if pl.get('cut'):
            w = T.well(pl['cut'])
            pts = rect_minus_circle(pl['rect'], w['c'], w['r'] + 1.2)
        else:
            pts = T.rounded_rect_m(pl['rect'], 1.2)
        th = z('subband_plate') if pl['id'] == 'subband' else lp_
        flat(b, pts, base, base + th, 'Bakelite_Black', holes=holes_in(pl['rect']))
        top = base + th
        if pl.get('line'):
            a0, _, a1, _ = T.rect_m(pl['rect'])
            _, yb = T.to_m(0.0, pl['line'])
            b.box(-a1 + 0.001, -a0 - 0.001, YP + top, YP + top + 0.0003, ZC + yb - 0.0003, ZC + yb + 0.0003, 'Paint_Letters')
        c = pl.get('at', ((pl['rect'][0] + pl['rect'][2]) / 2, (pl['rect'][1] + pl['rect'][3]) / 2))
        rot = pl.get('rot', 0)
        r = math.radians(rot)
        for i, line in enumerate(pl['text']):
            off = (i - (len(pl['text']) - 1) / 2) * pl['h'] * 1.5        # mm, down the lines (turned with the text)
            engrave(b, line, (c[0] + off * math.sin(r), c[1] + off * math.cos(r)), pl['h'], rot, top)
    # the voltage plate, cut round its well, with its scale's ticks
    vw = T.well('voltage')
    flat(b, rect_minus_circle(T.VOLT_PLATE, vw['c'], vw['r'] + 1.0), 0.0, z('voltage_plate'), 'Bakelite_Black')
    for t, c, h, rot, on in T.LABELS:
        engrave(b, t, c, h, rot, z(on))
    for ang in T.VOLT_TICKS:
        t = math.radians(ang)
        r_t = vw['r'] * 1.12
        p_ = (vw['c'][0] + r_t * math.cos(t), vw['c'][1] - r_t * math.sin(t))
        ua, vb = turned(ang)
        b.obox(at(p_, z('voltage_plate') + 0.0003), ua, vb, N, (0.003, 0.0006, 0.0006), 'Paint_Letters')
    # the arcs round the selector, the antenna knob and the volume knob: their words, studs and figures
    for arc in T.ARCS:
        wc = T.well(arc['well'])['c']
        flat(b, sector_pts(wc, arc['r'][0], arc['r'][1], arc['a'][0], arc['a'][1]), 0.0, lp_, 'Bakelite_Black')
        below = 180 < arc['text_a'] < 360
        lines = arc['text'].split('\n')
        r_text = arc.get('text_r', 0.5 * (arc['r'][0] + arc['r'][1]))
        for i, line in enumerate(lines):
            rr = r_text + (i - (len(lines) - 1) / 2) * 5.0 * (1 if below else -1)
            engrave_arc(b, line, wc, rr, arc['text_a'], 2.6, lp_)
        for mark, ang in arc['studs']:
            t = math.radians(ang)
            sp = (wc[0] + arc['r_stud'] * math.cos(t), wc[1] - arc['r_stud'] * math.sin(t))
            b.lathe(at(sp, lp_), N, [(1.9 * MM, 0.0), (1.8 * MM, 0.0012), (0.0, 0.0019)], 'Paint_Letters', seg=12)
            # the figure beside its stud, on the side away from the words
            away = 1.0 if ang > arc['text_a'] else -1.0
            ta = math.radians(ang + away * math.degrees(4.6 / arc['r_stud']))
            engrave(b, mark, (wc[0] + arc['r_stud'] * math.cos(ta), wc[1] - arc['r_stud'] * math.sin(ta)), 2.4, 0, lp_)
    # the nameplate
    np_ = T.NAMEPLATE
    flat(b, T.rounded_rect_m(np_['rect'], 1.5), 0.0, z('nameplate'), 'Metal_Steel')
    cx, cy = (np_['rect'][0] + np_['rect'][2]) / 2, (np_['rect'][1] + np_['rect'][3]) / 2
    engrave(b, np_['text'][0], (cx, cy), 4.0, 0, z('nameplate'), mat='Metal_Dark', extrude_m=0.0004)
    for dx in (-16.5, 16.5):
        b.lathe(at((cx + dx, cy), z('nameplate')), N, [(1.2 * MM, 0.0), (0.0, 0.0010)], 'Metal_Steel', seg=8)


def build_lower_block(b):
    """The set-back block under the plate's lower left: the connector's housing and multi-pin socket, the bushings that
    bring the cabin's power toggle and antenna knob out to the plate's plane."""
    lb = z('lower_block')
    con = T.CONNECTOR
    flat(b, T.rounded_rect_m(con['housing'], 4.0), lb, lb + 0.004, 'Paint_Radio', holes=[T.circle_m(con['c'], con['r'], 32)])
    ring(b, con['c'], con['r'], con['r'] * 0.8, lb, lb + 0.008, 'Metal_Steel', seg=32)
    disc(b, con['c'], con['r'] * 0.8, lb - 0.004, lb - 0.003, 'Metal_Dark', seg=30)
    ca, cb = T.to_m(*con['c'])
    for k in range(10):
        t = 2 * math.pi * k / 10
        b.cylinder(pos(ca + 0.0075 * math.cos(t), cb + 0.0075 * math.sin(t), lb - 0.003),
                   pos(ca + 0.0075 * math.cos(t), cb + 0.0075 * math.sin(t), lb + 0.002), 0.0011, 'Metal_Steel', seg=6)
    for name in T.ON_LOWER_BLOCK:
        c = T.CONTROL_MM[name]
        b.lathe(at(c, lb), N, [(0.008, 0.0), (0.008, 0.003), (0.0055, 0.004), (0.0055, -lb - 0.0005), (0.0, -lb - 0.0005)],
                'Metal_Dark', seg=20)


def build_set(b, knobs=False):
    build_casing(b)
    build_plate(b)
    build_hood(b)
    build_drum(b)
    build_fixators(b)
    build_windows_and_lamps(b)
    build_plates(b)
    build_lower_block(b)
    for c in T.SCREWS:
        screw(b, c, surface_h(c))
    for x, y, r in T.BRASS_HEADS:
        screw(b, (x, y), surface_h((x, y)), r_mm=r, mat='Metal_Brass', slot_deg=-35)
    # the knobs the model owns, and (for a render) the ones the cabin supplies
    for w in T.WELLS:
        if w['sim'] is not None and not knobs:
            continue
        r = w['r'] * MM
        if w['knob'] == 'red':
            knob_red(b, w['c'], 0.011)
        elif w['id'] == 'selector':
            knob_bar(b, w['c'], r * 1.7, 125)
        else:
            knob_bar(b, w['c'], r * 1.6, 120 if w['id'] == 'voltage' else 70)


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
