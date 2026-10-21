"""A radio set as a 3D model, built from the 3D trace of its front: the PU-12M's R-111 (tools/blender/r111_trace.py) and
R-407 (r407_trace.py). The R-123M has its own builder (r123m_model.py); this one reads the plainer schema the two later
traces share: the plate's outline (and a recessed strip), raised regions, round parts by kind, tube handles, letters and
screws, with heights out of the plate.

Run inside Blender through tools/blender/build_pu12m_radios.py.

Frame (the model's own, as r123m_model's): the front looks +y; x is to the operator's left when he faces it (so the
plate's right is -x); z up; the plate's centre at the origin. The casing goes back to -DEPTH.

The cabin supplies the controls it simulates (the trace's `sim`): the model leaves a shaft stub for those (pass knobs=True
to draw them all, for a render of the set alone), lamp lenses no thicker than the bare panel's plane, and the meters' and
dials' faces open for the cabin's displays.
"""
import math
import os

import bpy
from mathutils import Vector

MM = 0.001
FONT = r'C:\Windows\Fonts\ARIALN.TTF'
U = Vector((-1.0, 0.0, 0.0))          # the plate's right
V = Vector((0.0, 0.0, 1.0))           # up
N = Vector((0.0, 1.0, 0.0))           # out of the plate

# The colours each set is painted (`r111_photos`: light hammer grey, black letters; `r407_photos`: dark grey, white
# letters), as the cabin's material slots.
SCHEME = {
    'R111': {'plate': 'Paint_LightGrey', 'letters': 'Bakelite_Black', 'casing': 'Paint_LightGrey'},
    'R407': {'plate': 'Paint_Radio', 'letters': 'Paint_Letters', 'casing': 'Vinyl_Olive'},
}


class Front:
    """One trace module `T` and the builder `b`: the helpers that place things on its plate."""

    def __init__(self, T, b):
        self.T, self.b = T, b
        self.scheme = SCHEME[T.NAME]

    # -- placing ------------------------------------------------------------------------------------------------
    def pos(self, a, bb, h=0.0):
        return Vector((-a, h, bb))

    def at(self, c, h=0.0):
        a, bb = self.T.to_m(*c)
        return self.pos(a, bb, h)

    def z(self, key):
        return self.T.surface_z(key) * MM

    def mat(self, name):
        """A trace's material word as the cabin's slot: the set's own paint ('plate', 'casing', 'letters'), and the trace's
        names for what the cabin already has."""
        if name in self.scheme:
            return self.scheme[name]
        if self.T.NAME == 'R111' and name == 'Paint_Radio':
            return self.scheme['plate']
        if self.T.NAME == 'R111' and name == 'Paint_Letters':
            return self.scheme['letters']
        return {'Paint_White': 'Paint_Panel', 'Glass_Dark': 'Glass', 'Bakelite_Brown': 'Vinyl_Brown',
                'Paint_Letters_White': 'Paint_Letters'}.get(name, name)

    # -- primitives ---------------------------------------------------------------------------------------------
    def flat(self, pts_m, h0, h1, mat, holes=()):
        """A slab from an outline in plate metres (a, b), between heights h0 < h1 (m) out of the plate."""
        self.b.slab(self.pos(0, 0, h1), U, V, pts_m, list(holes), h1 - h0, self.mat(mat))

    def poly_m(self, pts_mm):
        return [self.T.to_m(x, y) for x, y in pts_mm]

    def rrect_m(self, r, rad, seg=6):
        """A mm rectangle with rounded corners -> plate metres, anticlockwise."""
        a0, b0, a1, b1 = self.T.rect_m(r)
        q = min(rad * MM, (a1 - a0) / 2 - 1e-5, (b1 - b0) / 2 - 1e-5)
        pts = []
        for (cx, cy, t0) in ((a1 - q, b0 + q, -90), (a1 - q, b1 - q, 0), (a0 + q, b1 - q, 90), (a0 + q, b0 + q, 180)):
            for k in range(seg + 1):
                t = math.radians(t0 + 90 * k / seg)
                pts.append((cx + q * math.cos(t), cy + q * math.sin(t)))
        return pts

    def circle_m(self, c, r_mm, seg=32):
        ca, cb = self.T.to_m(*c)
        return [(ca + r_mm * MM * math.cos(2 * math.pi * k / seg), cb + r_mm * MM * math.sin(2 * math.pi * k / seg))
                for k in range(seg)]

    def disc(self, c, r_mm, h0, h1, mat, seg=28):
        self.b.cylinder(self.at(c, h0), self.at(c, h1), r_mm * MM, self.mat(mat), seg=seg)

    def ring(self, c, r_out, r_in, h0, h1, mat, seg=40):
        self.flat(self.circle_m(c, r_out, seg), h0, h1, mat, holes=[self.circle_m(c, r_in, seg)])

    def screw(self, c, h=0.0, r_mm=2.6, mat='Metal_Steel', slot_deg=25):
        a, bb = self.T.to_m(*c)
        r = r_mm * MM
        self.b.lathe(self.pos(a, bb, h - 0.0003), N, [(r, 0.0), (r, 0.0011), (r * 0.8, 0.0017), (0.0, 0.0018)], mat, seg=12)
        th = math.radians(slot_deg)
        ua, vb = U * math.cos(th) + V * math.sin(th), -U * math.sin(th) + V * math.cos(th)
        self.b.obox(self.pos(a, bb, h + 0.0017), ua, vb, N, (2 * r * 0.9, 0.00055, 0.0006), 'Metal_Dark')

    # -- letters ------------------------------------------------------------------------------------------------
    def text(self, text, c, h_mm, rot_deg, lift, mat, extrude_m=0.0004):
        verts, faces = text_mesh(text, h_mm * MM, extrude_m)
        if not faces:
            return
        ca, cb = self.T.to_m(*c)
        th = math.radians(rot_deg)
        cs, sn = math.cos(th), math.sin(th)
        idx = self.b.mat(self.mat(mat))
        import build_cabin_interior as C
        bmv = [self.b.bm.verts.new(C.P(*self.pos(ca + v.x * cs - v.y * sn, cb + v.x * sn + v.y * cs, lift + v.z + extrude_m / 2)))
               for v in verts]
        for fc in faces:
            try:
                f = self.b.bm.faces.new([bmv[i] for i in fc])
                f.material_index = idx
            except ValueError:
                pass


def _font():
    if os.path.exists(FONT):
        try:
            return bpy.data.fonts.load(FONT, check_existing=True)
        except Exception:
            pass
    return None


def text_mesh(text, height_m, extrude_m=0.0004):
    """Glyph geometry for `text`, centred, in a flat frame (x right, y up, z out): ([verts], [faces])."""
    curve = bpy.data.curves.new('radio_text', 'FONT')
    curve.body = text
    curve.size = height_m / 0.72          # cap height is about 0.72 of the em
    curve.extrude = extrude_m / 2.0
    curve.resolution_u = 3
    curve.align_x = 'CENTER'
    curve.align_y = 'CENTER'
    f = _font()
    if f is not None:
        curve.font = f
    obj = bpy.data.objects.new('radio_text', curve)
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


# ---------------------------------------------------------------------------------------------- the parts

def build_casing(F):
    T = F.T
    low = min(T.Z.get('lower_strip', 0.0), 0.0) * MM - 0.003
    hw, hh = T.PLATE_W_M / 2, T.PLATE_H_M / 2
    F.b.box(-hw, hw, -T.DEPTH_M, low, -hh, hh, F.mat('casing'))
    # the plate's lip all round, a little proud of the casing
    lip = 0.004
    for (x0, x1, z0, z1) in ((-hw - lip, hw + lip, hh, hh + lip), (-hw - lip, hw + lip, -hh - lip, -hh),
                             (-hw - lip, -hw, -hh, hh), (hw, hw + lip, -hh, hh)):
        F.b.box(x0, x1, -T.DEPTH_M + 0.02, low + 0.004, z0, z1, F.mat('casing'))
    if T.NAME == 'R407':
        build_r407_case(F, hw, hh)


def build_r407_case(F, hw, hh):
    """The R-407's pack case round the front: its rim standing proud of the plate, the side latches, the folded handle and
    the feet (r407_photos; the overall size `r407_to`: 420 x 510 mm)."""
    T = F.T
    rim = T.Z['case_lip'] * MM
    w = 0.012
    outer = [(-hw - w, -hh - w), (hw + w, -hh - w), (hw + w, hh + w), (-hw - w, hh + w)]
    inner = [(-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)]
    F.flat(outer, -0.03, rim, 'casing', holes=[inner])
    cw, ch = T.CASE_M
    for sx in (-1, 1):
        for zz in (0.133, -0.133):       # the latches' height (r407_photos)
            F.b.box(sx * (hw + w), sx * (cw / 2), -0.06, -0.01, zz - 0.035, zz + 0.035, 'Metal_Steel')
    top = hh + w
    F.b.box(-0.05, 0.05, -0.05, -0.02, top, top + 0.012, 'Metal_Steel')
    F.b.tube([Vector((-0.045, -0.035, top + 0.012)), Vector((-0.04, -0.035, top + 0.035)), Vector((0.04, -0.035, top + 0.035)),
              Vector((0.045, -0.035, top + 0.012))], 0.006, 'Vinyl_Brown')
    foot = (ch - (T.PLATE_H_M + 2 * w)) - 0.047
    for sx in (-1, 1):
        F.b.box(sx * (hw - 0.04), sx * (hw + w), -T.DEPTH_M + 0.02, 0.0, -hh - w - max(foot, 0.02), -hh - w, F.mat('casing'))


def build_plate(F):
    T = F.T
    holes = [F.circle_m((x, y), r, 16) for x, y, r in T.HOLES]
    low = T.Z.get('lower_strip', 0.0) * MM
    pts = F.poly_m(T.OUTLINE)
    if T.STRIP_OUTLINE:
        F.flat(pts, low - 0.003, 0.0, 'plate', holes=holes)
        F.flat(F.poly_m(T.STRIP_OUTLINE), low - 0.003, low, 'plate')
    else:
        F.flat(F.rrect_m(T.PLATE, T.CORNER_R), -0.003, 0.0, 'plate', holes=holes)
    for x, y, r in T.HOLES:
        F.disc((x, y), r * 0.98, -0.006, -0.005, 'Metal_Dark', seg=14)


def region_outline(F, reg):
    if 'pts' in reg:
        return F.poly_m(reg['pts'])
    return F.rrect_m(reg['rect'], reg.get('round', 0.5))


def build_regions(F):
    T = F.T
    for reg in T.REGIONS:
        z0, z1 = reg['z'][0] * MM, reg['z'][1] * MM
        holes = []
        if reg['id'] in ('meter_frame',):
            holes.append(F.rrect_m(T.METER_FACE, 3.0))
        if reg['id'] == 'dial_oval':
            for c in getattr(T, 'DIALS', {}).values():
                holes.append(F.circle_m((c[0], c[1]), c[2], 40))
        F.flat(region_outline(F, reg), z0, z1, reg['mat'], holes=holes)
    if hasattr(T, 'METER_FACE') and any(r['id'] == 'meter_frame' for r in T.REGIONS):
        F.flat(F.rrect_m(T.METER_FACE, 3.0), -0.006, T.Z['meter_face'] * MM, 'Paint_Panel')
    if T.OVAL:
        o = T.OVAL
        (x0, y0), (x1, _) = o['c0'], o['c1']
        pts = []
        for k in range(17):
            t = math.radians(90 + 180 * k / 16)
            pts.append(T.to_m(x0 + o['r_out'] * math.cos(t), y0 - o['r_out'] * math.sin(t)))
        for k in range(17):
            t = math.radians(-90 + 180 * k / 16)
            pts.append(T.to_m(x1 + o['r_out'] * math.cos(t), y0 - o['r_out'] * math.sin(t)))
        pts.reverse()
        F.flat(pts, 0.0, T.Z['escutcheon'] * MM, 'Bakelite_Black',
               holes=[F.circle_m(o['c0'], o['r_in'], 36), F.circle_m(o['c1'], o['r_in'], 36)])
    if T.RING:
        r = T.RING
        F.ring(r['c'], r['r_out'], r['r_in'], 0.0, T.Z['escutcheon'] * MM, 'Bakelite_Black')


def build_lamps(F):
    """The hexagonal lamp lenses, no thicker than the bare panel's plane (the cabin's lamps light them)."""
    T = F.T
    for lp in T.LAMPS:
        x0, y0, x1, y1 = lp['rect']
        h = y1 - y0
        pts = [(x0 + 0.35 * h, y0), (x1, y0), (x1, y1 - 0.0 * h), (x1 - 0.35 * h, y1), (x0, y1), (x0, y0 + 0.0)]
        pts = [(x0 + 0.3 * h, y0), (x1 - 0.05 * h, y0), (x1, y0 + 0.5 * h), (x1 - 0.3 * h, y1), (x0 + 0.05 * h, y1),
               (x0, y0 + 0.5 * h)]
        mat = 'Paint_Red' if lp['color'] == 'red' else 'Lens_Amber'
        F.flat(F.poly_m(pts), 0.0, 0.0035, mat)
        mid = ((x0 + x1) / 2, (y0 + y1) / 2)
        for sx in (x0 + 0.12 * (x1 - x0), x1 - 0.12 * (x1 - x0)):
            F.screw((sx, mid[1]), 0.0035, r_mm=2.0)
        if lp['text']:
            n = len(lp['text'])
            for i, t in enumerate(lp['text']):
                F.text(t, (mid[0], mid[1] + (i - (n - 1) / 2) * 6.0), 3.6, 0, 0.0035, 'Bakelite_Black')
        if lp['halves']:
            F.flat(F.poly_m([(mid[0] - 0.4, y0 + 1), (mid[0] + 0.4, y0 + 1), (mid[0] + 0.4, y1 - 1), (mid[0] - 0.4, y1 - 1)]),
                   0.0035, 0.0038, 'Bakelite_Black')
            for i, t in enumerate(lp['halves']):
                cx = (x0 + mid[0]) / 2 + 2 if i == 0 else (mid[0] + x1) / 2 - 2
                F.text(t, (cx, mid[1]), 3.4, 0, 0.0035, 'Bakelite_Black')


def build_round(F, knobs):
    T = F.T
    for p in T.ROUND:
        c, r = p['c'], p['r']
        z0, z1 = p['z'][0] * MM, p['z'][1] * MM
        kind = p['kind']
        sim = p.get('sim') is not None
        if kind == 'spoked':
            F.ring(c, r, r - 3.0, z0, z1, 'Paint_LightGrey' if T.NAME == 'R111' else 'Paint_Radio', seg=56)
            F.disc(c, 9.0, z0, z1 + 0.002, 'Paint_Panel', seg=24)
            ca, cb = T.to_m(*c)
            for k in range(16):
                th = 2 * math.pi * k / 16
                ua = U * math.cos(th) + V * math.sin(th)
                va = -U * math.sin(th) + V * math.cos(th)
                mid = (r + 9.0) / 2 * MM
                F.b.obox(F.pos(ca + mid * math.cos(th), cb + mid * math.sin(th), (z0 + z1) / 2), ua, va, N,
                         ((r - 9.0) * MM, 2.2 * MM, (z1 - z0) * 0.8), F.mat('plate'))
        elif kind == 'dial_disc':
            F.disc(c, r, z0, z1, 'Glass', seg=40)
            F.disc(c, 3.5, z1, z1 + 0.004, 'Metal_Steel', seg=12)
        elif kind == 'base':
            F.disc(c, r, z0, z1, 'plate', seg=40)
            F.disc(c, 4.0, z1, z1 + 0.005, 'Metal_Steel', seg=12)
        elif kind in ('knurled', 'trilobe'):
            if sim and not knobs:
                F.disc(c, 4.0, z0, z0 + 0.006, 'Metal_Steel', seg=12)
            else:
                F.disc(c, r, z0, z1, 'Bakelite_Black', seg=36)
                ca, cb = T.to_m(*c)
                for k in range(18):
                    th = 2 * math.pi * k / 18
                    F.b.cylinder(F.pos(ca + r * MM * math.cos(th), cb + r * MM * math.sin(th), z0),
                                 F.pos(ca + r * MM * math.cos(th), cb + r * MM * math.sin(th), z1 - 0.002), 1.4 * MM,
                                 'Bakelite_Black', seg=6)
        elif kind == 'button_well':
            F.ring(c, r, r * 0.72, z0, z1, 'Metal_Dark', seg=28)
            F.disc(c, r * 0.72, z0 - 0.004, z0 - 0.003, 'Metal_Dark', seg=24)
            if knobs:
                F.disc(c, r * 0.68, z0 - 0.003, z1 + 0.004, 'Bakelite_Black', seg=24)
        elif kind in ('toggle_boss', 'boss', 'lever_boss'):
            F.disc(c, r, z0, z1 if kind != 'lever_boss' else z0 + 0.004, 'Metal_Steel' if kind != 'boss' else 'plate', seg=20)
            if knobs and kind == 'lever_boss':
                L, ang = T.LEVERS[p['id']]
                ca, cb = T.to_m(*c)
                th = math.radians(ang)
                ua = U * math.cos(th) + V * math.sin(th)
                F.b.obox(F.pos(ca + L / 2 * MM * math.cos(th), cb + L / 2 * MM * math.sin(th), z0 + 0.012), ua,
                         -U * math.sin(th) + V * math.cos(th), N, (L * MM, 9 * MM, 0.008), 'Bakelite_Black')
        elif kind == 'ring':
            F.ring(c, r, r * 0.62, z0, z1, 'plate', seg=40)
            if knobs:
                F.disc(c, r * 0.55, z1, z1 + 0.016, 'Bakelite_Black', seg=24)
        elif kind == 'hf_connector':
            F.disc(c, r, z0, z1, 'Metal_Steel', seg=24)
            F.disc(c, r * 0.55, z1, z1 + 0.001, 'Paint_Panel', seg=16)
            F.disc(c, 1.2, z1 + 0.001, z1 + 0.002, 'Metal_Steel', seg=8)
        elif kind in ('connector', 'cap_connector'):
            if 'flange' in p:
                F.flat(F.rrect_m(p['flange'], 3.0), z0, z0 + 0.003, 'Metal_Steel')
                for fx, fy in ((p['flange'][0] + 4, p['flange'][1] + 4), (p['flange'][2] - 4, p['flange'][1] + 4),
                               (p['flange'][0] + 4, p['flange'][3] - 4), (p['flange'][2] - 4, p['flange'][3] - 4)):
                    F.screw((fx, fy), z0 + 0.003, r_mm=1.8)
            F.disc(c, r, z0 + 0.003, z1 - (0.006 if kind == 'cap_connector' else 0.0), 'Metal_Steel', seg=32)
            if kind == 'cap_connector':
                F.disc(c, r * 1.02, z1 - 0.012, z1, 'Bakelite_Black', seg=32)
            else:
                F.disc(c, r * 0.8, z1 - 0.004, z1 - 0.003, 'Metal_Dark', seg=28)
        elif kind == 'cap_well':
            F.ring(c, r, r * 0.8, z0, z1, 'Metal_Steel', seg=32)
        elif kind == 'terminal':
            F.disc(c, r, z0, z1 - 0.004, 'Bakelite_Black' if T.NAME == 'R111' else 'Vinyl_Brown', seg=16)
            F.disc(c, r * 0.4, z1 - 0.004, z1, 'Metal_Steel', seg=10)
        elif kind == 'hatch':
            a, bb = T.to_m(*c)
            rr = r * MM
            F.b.lathe(F.pos(a, bb, z0), N, [(rr, 0.0), (rr, 0.004), (rr * 0.92, 0.009), (rr * 0.6, z1 - z0), (0.0, z1 - z0)],
                      F.mat('plate'), seg=48)
            x0, x1, w = T.HATCH_BAR
            F.flat(F.poly_m([(x0, c[1] - w / 2), (x1, c[1] - w / 2), (x1, c[1] + w / 2), (x0, c[1] + w / 2)]), z1 - 0.002,
                   z1 + 0.004, 'Metal_Steel')
        elif kind == 'slot_screw':
            F.disc(c, r, z0, z1, 'Metal_Steel', seg=20)
            F.screw(c, z1, r_mm=r * 0.9, slot_deg=40)
        elif kind == 'red_lens':
            a, bb = T.to_m(*c)
            F.b.lathe(F.pos(a, bb, z0), N, [(r * MM, 0.0), (r * MM, 0.002), (r * 0.7 * MM, z1 - z0), (0.0, z1 - z0)],
                      'Paint_Red', seg=20)
        elif kind == 'dome':
            F.disc(c, r, -0.004, -0.003, 'Paint_Panel', seg=40)
            F.ring(c, r + 2.0, r, z0, z0 + 0.003, 'Metal_Steel', seg=40)
        elif kind == 'plug':
            F.disc(c, r, z0, z1, 'plate', seg=28)
            F.disc(c, r * 0.35, z1, z1 + 0.0005, 'Metal_Dark', seg=12)
        elif kind == 'lamp_holder':
            F.ring(c, r, r * 0.6, z0, z1, 'Paint_Panel', seg=28)
        elif kind == 'cap':
            F.disc(c, r, z0, z1, 'Bakelite_Black', seg=32)
        else:
            raise ValueError('unknown kind ' + kind)


def build_tubes(F):
    T = F.T
    zh = T.Z.get('handle', 24.0)
    for tb in T.TUBES:
        pts = [F.at(p, zh * MM) for p in tb['path']]
        pts = [F.at(tb['path'][0], 0.004)] + pts + [F.at(tb['path'][-1], 0.004)]
        F.b.tube(pts, tb['r'] * MM, 'Metal_Steel', seg=12)
        for br in tb['brackets']:
            F.flat(F.rrect_m(br, 2.0), 0.0, 0.008, 'Metal_Steel')
    for x0, x1, y, r in T.PINS:
        a0, bb = T.to_m(x0, y)
        a1, _ = T.to_m(x1, y)
        F.b.cylinder(F.pos(a0, bb, r * MM), F.pos(a1, bb, r * MM), r * MM, 'Metal_Steel', seg=12)
    if hasattr(T, 'EYEPIECE'):
        e = T.EYEPIECE
        F.disc(e['c'], e['r'], T.Z['hood'] * MM, T.Z['hood'] * MM + 0.004, 'Bakelite_Black', seg=24)


def build_letters_and_screws(F):
    T = F.T
    for s, key in T.SCREWS:
        F.screw(s, F.z(key) if key != 'frame' else frame_z(T, s))
    for text, c, h, rot, key, mat in T.TEXT:
        lift = F.z(key) if key != 'frame' else frame_z(T, c)
        F.text(text, c, h, rot, lift, mat)


def frame_z(T, c):
    for reg in T.REGIONS:
        if reg['id'].endswith('frame') and 'rect' in reg:
            x0, y0, x1, y1 = reg['rect']
            if x0 <= c[0] <= x1 and y0 <= c[1] <= y1:
                return reg['z'][1] * MM
    return 0.0


def build_set(b, T, knobs=False):
    F = Front(T, b)
    build_casing(F)
    build_plate(F)
    build_regions(F)
    build_lamps(F)
    build_round(F, knobs)
    build_tubes(F)
    build_letters_and_screws(F)
    return F


def build(B, finish, T, knobs=False):
    """Build SM_Radio<NAME> with the interior script's Builder `B` and `finish`."""
    b = B('SM_Radio' + T.NAME)
    build_set(b, T, knobs=knobs)
    obj = finish(b, reuse=True)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    return obj
