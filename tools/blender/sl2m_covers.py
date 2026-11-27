"""SŁ-2M's hinged covers and levers, full size, for the cabin's parts library (build_cabin_parts.py imports this).

Each part is modelled in metres with its hinge's axis through the origin: shut, +Y runs across it from the hinge, +Z out
of the panel, +X along the hinge (data/cabins/osa_ak.json gives each control's "flap": where the hinge is, which way
+Y points on the panel, how far the axis stands out of the face and how far it swings; simcore::CabinFlap). A part that
stands on the panel and never moves (a hinge's lugs, a pivot's post) is modelled in the same frame at 0 degrees.

Measured on SAM Simulator's sprites, each cover shut and open (osa020/021 ВЫСОКОЕ, osa010/011 ПЕРЕДАТЧИК, osa034/035
БОРТ СПК, osa042/043 АВАРИЙНЫЙ ПУСК; cut from the same photographs as the AK console, about 1 mm a pixel, placed on the
panels by matching them to samsim_panels) and on samsim_panels itself at 8-12x (the elevation wheel's lock lever,
БАЛАНС ПА's lid). Depths: by eye from their shadows, SAM3D's estimate.
"""
import math

import bmesh
from mathutils import Matrix, Vector

MM = 0.001


def _outline_rr(x0, x1, y0, y1, r_near=0.0, r_far=0.0, seg=8):
    """A rectangle (anticlockwise) with its two far corners (y1) rounded r_far and near ones (y0) r_near."""
    pts = []

    def corner(cx, cy, r, a0):
        if r <= 0.0:
            pts.append((cx, cy))
            return
        ccx = cx - r if cx > x0 else cx + r          # the corner arc's centre
        ccy = cy - r if cy > y0 else cy + r
        for k in range(seg + 1):
            a = math.radians(a0 + 90.0 * k / seg)
            pts.append((ccx + r * math.cos(a), ccy + r * math.sin(a)))

    corner(x1, y0, r_near, 270)
    corner(x1, y1, r_far, 0)
    corner(x0, y1, r_far, 90)
    corner(x0, y0, r_near, 180)
    return pts


def _outline_bullet(x0, x1, y0, y1, seg=16):
    """Straight from the hinge (y0) and a half round at the far end: x0..x1 across, y1 the far tip."""
    r = 0.5 * (x1 - x0)
    cx, cy = 0.5 * (x0 + x1), y1 - r
    pts = [(x1, y0)]
    for k in range(seg + 1):
        a = math.radians(0.0 + 180.0 * k / seg)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    pts.append((x0, y0))
    return pts


def _shrink(pts, d):
    """Offset a convex outline inward by d (per vertex along the bisector)."""
    n = len(pts)
    out = []
    for i in range(n):
        p0, p1, p2 = Vector(pts[i - 1]), Vector(pts[i]), Vector(pts[(i + 1) % n])
        e0 = (p1 - p0).normalized() if (p1 - p0).length > 1e-9 else Vector((1, 0))
        e1 = (p2 - p1).normalized() if (p2 - p1).length > 1e-9 else e0
        n0, n1 = Vector((-e0.y, e0.x)), Vector((-e1.y, e1.x))      # inward for an anticlockwise outline
        b = (n0 + n1)
        b = b.normalized() if b.length > 1e-9 else n0
        c = max(0.3, b.dot(n0))
        q = p1 + b * (d / c)
        out.append((q.x, q.y))
    return out


def _prism(bm, outline, z0, z1):
    low = [bm.verts.new((x, y, z0)) for x, y in outline]
    top = [bm.verts.new((x, y, z1)) for x, y in outline]
    bm.faces.new(list(reversed(low)))
    bm.faces.new(top)
    n = len(outline)
    for k in range(n):
        bm.faces.new((low[k], low[(k + 1) % n], top[(k + 1) % n], top[k]))


def _ring(bm, outer, inner, z0, z1):
    """A wall between two outlines with the same vertex count, z0 to z1, closed top and bottom."""
    n = len(outer)
    lo = [bm.verts.new((x, y, z0)) for x, y in outer]
    lt = [bm.verts.new((x, y, z1)) for x, y in outer]
    io = [bm.verts.new((x, y, z0)) for x, y in inner]
    it = [bm.verts.new((x, y, z1)) for x, y in inner]
    for k in range(n):
        j = (k + 1) % n
        bm.faces.new((lo[k], lo[j], lt[j], lt[k]))       # outside
        bm.faces.new((it[k], it[j], io[j], io[k]))       # inside
        bm.faces.new((lt[k], lt[j], it[j], it[k]))       # top
        bm.faces.new((io[k], io[j], lo[j], lo[k]))       # bottom


def _tray(bm, outline, z_rim, z_top, t=1.2 * MM):
    """A pressed lid: the top plate (z_top - t to z_top) and its skirt down to z_rim, open underneath."""
    inner = _shrink(outline, t)
    _prism(bm, outline, z_top - t, z_top)
    _ring(bm, outline, inner, z_rim, z_top - t)


def _cyl(bm, r, a, b, seg=16):
    """A cylinder from point a to point b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    rot = d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    m = Matrix.Translation((a + b) / 2) @ rot
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=d.length, matrix=m)


def _disc(bm, r, x, y, z0, z1, seg=24, r_top=None):
    m = Matrix.Translation((x, y, (z0 + z1) / 2))
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r if r_top is None else r_top,
                          depth=z1 - z0, matrix=m)


def _box(bm, x0, x1, y0, y1, z0, z1):
    m = Matrix.Translation(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)) @ Matrix.Diagonal((x1 - x0, y1 - y0, z1 - z0, 1.0))
    bmesh.ops.create_cube(bm, size=1.0, matrix=m)


def _dome(bm, r, h, x, y, z, seg=32, rows=6):
    """A spherical cap r across, h high, standing on z at (x, y)."""
    R = (r * r + h * h) / (2 * h)
    prof = []
    for i in range(rows + 1):
        rr = r * (1 - i / rows)
        prof.append((rr, z + (math.sqrt(R * R - rr * rr) - (R - h))))
    verts = []
    for rr, zz in prof:
        row = [bm.verts.new((x + rr * math.cos(2 * math.pi * k / seg), y + rr * math.sin(2 * math.pi * k / seg), zz))
               for k in range(seg)]
        verts.append(row)
    for i in range(rows):
        for k in range(seg):
            a, b = verts[i][k], verts[i][(k + 1) % seg]
            c, d = verts[i + 1][(k + 1) % seg], verts[i + 1][k]
            if i + 1 == rows:
                if k == 0:
                    bm.faces.new(verts[rows])
                continue
            bm.faces.new((a, b, c, d))


def _text(bm, text, h, x, y, z, width=None):
    """Raised letters on +Z at (x, y), reading along +X, up +Y (r123m_model's glyphs)."""
    import r123m_model as R
    verts, faces = R.text_mesh(text, h, 0.0003)
    if not verts:
        return
    w = max(v.x for v in verts) - min(v.x for v in verts)
    k = (width / w) if width and w > 0 else 1.0
    bv = [bm.verts.new((x + v.x * k, y + v.y, z + v.z + 0.00015)) for v in verts]
    for fc in faces:
        try:
            bm.faces.new([bv[i] for i in fc])
        except ValueError:
            pass


# A lid's hinge sits at the height of its top plate, on lugs standing off the panel, so that opened through 180 degrees
# it hangs below the hinge inside out without running into the panel. Each lid is drawn with the panel at z = 0, then
# moved so that its knuckle's axis is the origin; its "lift" (the cabin's lift_m) is how far that axis stands out of
# the panel.
def _lid(bm, outline, top, knuckle_r, knuckle_w, foot=0.5 * MM):
    """The pressed lid standing on the panel (z 0) to `top`, its knuckle along its near edge at the top plate."""
    _tray(bm, outline, foot, top)
    _cyl(bm, knuckle_r, (-knuckle_w / 2, 0, top - knuckle_r), (knuckle_w / 2, 0, top - knuckle_r))


def _to_knuckle(bm, top, knuckle_r):
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0.0, 0.0, -(top - knuckle_r)))


def _lugs(bm, lift, x_in, x_out, pin_r=1.0 * MM):
    """The hinge's two lugs from the panel (z = -lift) up past the knuckle's axis, and the pin through them."""
    for s in (-1, 1):
        x0, x1 = sorted((s * x_in, s * x_out))
        _box(bm, x0, x1, -4 * MM, 3 * MM, -lift, 3 * MM)
    _cyl(bm, pin_r, (-x_out - 0.5 * MM, 0, 0), (x_out + 0.5 * MM, 0, 0), seg=10)


def lift_of(d):
    """How far a lid's knuckle axis stands out of the panel (the cabin's flap lift_m)."""
    return d['top'] - d['kr']


# ------------------------------------------------------------------------------------------------- АВАРИЙНЫЙ ПУСК
# 0004-11M's emergency launch cover (osa042 shut, osa043 open; samsim_panels, where it hangs open onto the elevation
# wheel's rim; 14 Nov 2026 at 5x: 44 mm across, a round pressing 32 mm across 48 mm from the hinge on its inside, rivets
# at 62 and 82 mm): a pressed lid 44 mm wide, 86 mm from its
# hinge along the bottom to its rounded top, over the АВАР. ПУСК button and the small lamp under it (the two legend
# lamps to its left stay in view). Red АВАРИЙНЫЙ / ПУСК across its middle, 44 and 34 mm up; a square catch 9 mm up
# from the hinge. Open it hangs from the hinge inside out, showing the deep pressing that clears the button.
AVAR = {'w': 44 * MM, 'l': 86 * MM, 'r': 11 * MM, 'top': 15 * MM, 'kr': 2.6 * MM}


def cover_avar(bm):
    a = AVAR
    _lid(bm, _outline_rr(-a['w'] / 2, a['w'] / 2, 2 * MM, a['l'], 0.0, a['r']), a['top'], a['kr'], 30 * MM)
    _box(bm, -3.5 * MM, 3.5 * MM, 6 * MM, 12 * MM, a['top'] - 0.2 * MM, a['top'] + 1.0 * MM)     # the catch
    # inside: the round pressing over the button (a rib under the top plate) and two rivets through it
    ring = [(16 * MM * math.cos(2 * math.pi * k / 40), 48 * MM + 16 * MM * math.sin(2 * math.pi * k / 40)) for k in range(40)]
    _ring(bm, ring, _shrink(ring, 1.4 * MM), a['top'] - 3.2 * MM, a['top'] - 1.0 * MM)
    for y in (62 * MM, 82 * MM):
        _disc(bm, 1.8 * MM, 0.0, y, a['top'] - 2.2 * MM, a['top'] + 0.5 * MM, seg=12)
    _to_knuckle(bm, a['top'], a['kr'])


def cover_avar_text(bm):
    a = AVAR
    _text(bm, 'АВАРИЙНЫЙ', 5.5 * MM, 0.0, 44 * MM, a['top'], width=36 * MM)
    _text(bm, 'ПУСК', 5.5 * MM, 0.0, 34 * MM, a['top'])
    _to_knuckle(bm, a['top'], a['kr'])


def cover_avar_hinge(bm):
    _lugs(bm, lift_of(AVAR), 15.2 * MM, 18 * MM)


# ------------------------------------------------------------------------------------------------------ БОРТ СПК
# The guard over БОРТ СПК ВКЛ. (osa034 shut, osa035 open; samsim_panels shows it open): a lid 38 mm wide and 46 mm up from
# its hinge, the far corners rounded 11 mm, a thumb tab at its top; the hinge a barrel across its foot, 38 mm long,
# between two lugs on the plate. Open, it hangs below the barrel (as on SAM's photo), its inside out with the spring.
SPK = {'w': 38 * MM, 'l': 46 * MM, 'r': 11 * MM, 'top': 15 * MM, 'kr': 2.6 * MM}


def cover_spk(bm):
    a = SPK
    _lid(bm, _outline_rr(-a['w'] / 2, a['w'] / 2, 2.5 * MM, a['l'], 0.0, a['r']), a['top'], a['kr'], 38 * MM)
    _box(bm, -4 * MM, 4 * MM, a['l'] - 1 * MM, a['l'] + 4 * MM, a['top'] - 1.2 * MM, a['top'])  # the tab
    # the spring's loop on its inside (osa035), pressed against the top plate
    for s in (-1, 1):
        _cyl(bm, 0.5 * MM, (s * 5 * MM, 3 * MM, a['top'] - 1.6 * MM), (0, 12 * MM, a['top'] - 1.6 * MM), seg=6)
    _to_knuckle(bm, a['top'], a['kr'])


def cover_spk_hinge(bm):
    _lugs(bm, lift_of(SPK), 19.2 * MM, 23 * MM, pin_r=1.2 * MM)


# ------------------------------------------------------------------------------------------------------ ВЫСОКОЕ
# The cap over ВЫСОКОЕ ВКЛ. (osa020 shut, osa021 open; samsim_panels at 5x shows it shut): a deep cup 26 mm tall and 29 mm
# from its hinge, its far end square with the corners cut; the hinge, a dark bracket, stands up the face at its left end,
# so it swings out to the left and shows its inside.
HV = {'h': 26 * MM, 'l': 29 * MM, 'top': 13 * MM, 'kr': 2.4 * MM}


def cover_hv(bm):
    a = HV
    _lid(bm, _outline_rr(-a['h'] / 2, a['h'] / 2, 2.5 * MM, a['l'], 0.0, 3 * MM, seg=1), a['top'], a['kr'], 22 * MM)
    _to_knuckle(bm, a['top'], a['kr'])


def cover_hv_hinge(bm):
    _lugs(bm, lift_of(HV), 11.2 * MM, 14 * MM)
    _disc(bm, 2.2 * MM, 15.0 * MM, 0, -0.5 * MM, 0.5 * MM, seg=10)


# --------------------------------------------------------------------------------------------------- ПЕРЕДАТЧИК
# The teardrop guard over ПЕРЕДАТЧИК ВКЛ. (osa011 shut over the button, osa010 swung clear of it): a plate turning in the
# face on a pivot at its point, 1.5 mm thick with a rolled rim, the round 29 mm across 37 mm from the pivot; it stands
# 10 mm off the plate on the pivot's post.
TX = {'R': 14.5 * MM, 'L': 37 * MM, 'r0': 4.5 * MM, 'lift': 10 * MM}


def _hull_circles(circles, seg=48):
    """The convex outline round circles [(x, y, r)], anticlockwise."""
    pts = []
    for cx, cy, r in circles:
        pts += [(cx + r * math.cos(2 * math.pi * k / seg), cy + r * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
    pts = sorted(set((round(x, 7), round(y, 7)) for x, y in pts))

    def cross(o, p, q):
        return (p[0] - o[0]) * (q[1] - o[1]) - (p[1] - o[1]) * (q[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def _teardrop():
    return _hull_circles([(0.0, 0.0, TX['r0']), (0.0, TX['L'], TX['R'])])


def cover_tx(bm):
    out = _teardrop()
    _prism(bm, out, -0.75 * MM, 0.75 * MM)
    _ring(bm, out, _shrink(out, 1.0 * MM), 0.75 * MM, 1.6 * MM)          # the rolled rim


def cover_tx_post(bm):
    _disc(bm, 3.0 * MM, 0, 0, -TX['lift'], -0.75 * MM, seg=16)
    _disc(bm, 3.2 * MM, 0, 0, 0.75 * MM, 2.2 * MM, seg=16, r_top=2.8 * MM)   # the screw's head over the plate
    _box(bm, -2.4 * MM, 2.4 * MM, -0.35 * MM, 0.35 * MM, 2.1 * MM, 2.4 * MM)


# ------------------------------------------------------------------------------------------------ АСТИГМАТ. I, II
# OS81-9M's astigmatism trimmers each lie under a cream plate turned in the face on a slotted screw at one end (samsim_panels
# at 10x, about 1.12 px a millimetre there: an elongated hexagon 32 x 15 mm, pointed at both ends, its screw 7 mm across
# near one point and a pressed round boss near the other, over the trimmer's hole; I turns on its aft screw, II on its
# forward one). It stands 3 mm off the plate on the screw's spacer (SAM3D's estimate). That it is a cover over the
# trimmer is SAM3D's reading of the photo: no document describes it.
ASTIG = {'w': 15 * MM, 'tip0': 7.5 * MM, 'tip1': 25.5 * MM, 'boss': 18 * MM, 'lift': 3 * MM}


def cover_astig(bm):
    a = ASTIG
    h = a['w'] / 2
    out = [(0.0, -a['tip0']), (h, -1.0 * MM), (h, a['boss'] + 1.0 * MM), (0.0, a['tip1']), (-h, a['boss'] + 1.0 * MM),
           (-h, -1.0 * MM)]
    _prism(bm, out, -0.5 * MM, 0.5 * MM)
    _dome(bm, 3.0 * MM, 0.8 * MM, 0.0, a['boss'], 0.5 * MM)            # the pressed boss over the trimmer


def cover_astig_post(bm):
    _disc(bm, 2.5 * MM, 0, 0, -ASTIG['lift'], -0.5 * MM, seg=16)          # the spacer under the plate
    _disc(bm, 3.5 * MM, 0, 0, 0.5 * MM, 1.7 * MM, seg=16, r_top=3.0 * MM)   # the screw's head
    _box(bm, -3.0 * MM, 3.0 * MM, -0.35 * MM, 0.35 * MM, 1.6 * MM, 1.9 * MM)   # its slot


# ------------------------------------------------------------------------------------------------ БАЛАНС ПА's lid
# OS52-14M's БАЛАНС ПА button sits in a square shroud (both columns: samsim_panels; the senior operator's open, the deputy
# commander's shut by a lid). The lid: 19 mm across, 27 mm down from its hinge along the shroud's top edge.
BAL = {'w': 19 * MM, 'l': 27 * MM, 'lift': 11 * MM}


def cover_bal(bm):
    a = BAL
    out = _outline_rr(-a['w'] / 2, a['w'] / 2, 1.0 * MM, a['l'], 0.0, 1.5 * MM)
    _tray(bm, out, -1.2 * MM, 0.6 * MM)
    _cyl(bm, 1.6 * MM, (-8 * MM, 0, 0), (8 * MM, 0, 0), seg=12)
    _box(bm, -3 * MM, 3 * MM, a['l'] - 0.5 * MM, a['l'] + 2.5 * MM, -0.4 * MM, 0.6 * MM)   # the lift tab


# ------------------------------------------------------------------------------------------- the hand wheels' locks
# OS52-12M's and OS52-14M's hand wheels each have their crank folded down across the hub, a knurled collar on it (samsim_panels
# at 4x: a stack of knurled rings 9 mm across, about 20 mm long, from the hub up towards the rim). SAM Simulator unlocks a
# wheel by right-clicking it (samsim_osa p22, p23); no other lock shows on the photo, so the collar is taken for the lock,
# turned a quarter to free the wheel (SAM3D's reading).
def wheel_lock_collar(bm):
    for k in range(5):                                   # five knurled rings with a waist between each
        y0 = -10 * MM + 4 * k * MM
        _cyl(bm, 4.5 * MM, (0, y0 + 0.3 * MM, 0), (0, y0 + 3.2 * MM, 0), seg=20)
        _cyl(bm, 3.6 * MM, (0, y0 + 3.2 * MM, 0), (0, y0 + 4.3 * MM, 0), seg=16)
    _box(bm, -6 * MM, 6 * MM, 10 * MM, 12 * MM, -1.2 * MM, 1.2 * MM)     # the cross-bar at its top (the T on the photo)


PARTS = [
    ('SM_CoverAvar', cover_avar), ('SM_CoverAvarText', cover_avar_text), ('SM_CoverAvarHinge', cover_avar_hinge),
    ('SM_CoverSpk', cover_spk), ('SM_CoverSpkHinge', cover_spk_hinge),
    ('SM_CoverHv', cover_hv), ('SM_CoverHvHinge', cover_hv_hinge),
    ('SM_CoverTx', cover_tx), ('SM_CoverTxPost', cover_tx_post),
    ('SM_CoverBal', cover_bal),
    ('SM_CoverAstig', cover_astig), ('SM_CoverAstigPost', cover_astig_post),
    ('SM_WheelLockCollar', wheel_lock_collar),
]
