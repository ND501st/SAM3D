"""Build the cabin's console parts in Blender from code and export them for Unreal.

Every part is modelled at 1 m across (its diameter, or width) with real proportions, standing out of the panel along
+Z from its base at Z = 0, and "up the face" along +Y. The cabin (unreal/.../SimCabinComponent) scales each instance by
the item's size from data/cabins/<system>.json and tints it with the item's colours, so one mesh serves every size.
Moving parts are separate meshes: toggle levers, knob bodies, button caps, lamp lenses, wheel handles.

    "C:/Program Files/Blender Foundation/Blender 4.2/blender.exe" --background --python tools/blender/build_cabin_parts.py
    ... -- --preview out/parts_preview.png      # also render a contact sheet
    ... -- --only SM_WheelDome                 # export only these parts (the .blend is saved whole)

Writes art/cabin_parts/<Part>.fbx (one mesh each) and art/cabin_parts/cabin_parts.blend (Git LFS).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, 'art', 'cabin_parts')
SEG = 48  # segments around round parts


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def new_object(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for p in mesh.polygons:
        p.use_smooth = True
    return obj


def lathe(bm, profile, seg=SEG, ribs=0, rib_depth=0.0, rib_rows=(), cap=True):
    """Revolve a profile [(r, z), ...] (from the axis out and back) around Z. Rows listed in rib_rows get `ribs`
    knurling ridges (radius modulated by rib_depth). Returns the new vertices row by row."""
    rows = []
    for i, (r, z) in enumerate(profile):
        row = []
        for k in range(seg):
            a = 2 * math.pi * k / seg
            rr = r
            if ribs and i in rib_rows:
                rr = r * (1.0 - rib_depth * (0.5 + 0.5 * math.cos(ribs * a)))
            row.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
        rows.append(row)
    for i in range(len(rows) - 1):
        for k in range(seg):
            a, b = rows[i][k], rows[i][(k + 1) % seg]
            c, d = rows[i + 1][(k + 1) % seg], rows[i + 1][k]
            bm.faces.new((a, b, c, d))
    # Close the ends: a fan to a centre vertex unless the profile touches the axis already or cap is off (a ring).
    if not cap:
        return rows
    for row, z, flip in ((rows[0], profile[0][1], True), (rows[-1], profile[-1][1], False)):
        if profile[0 if flip else -1][0] < 1e-6:
            continue
        centre = bm.verts.new((0.0, 0.0, z))
        for k in range(seg):
            a, b = row[k], row[(k + 1) % seg]
            bm.faces.new((centre, b, a) if flip else (centre, a, b))
    return rows


def box(bm, cx, cy, cz, sx, sy, sz):
    m = Matrix.Translation((cx, cy, cz)) @ Matrix.Diagonal((sx, sy, sz, 1.0))
    bmesh.ops.create_cube(bm, size=1.0, matrix=m)


def cylinder(bm, r, z0, z1, x=0.0, y=0.0, seg=SEG):
    m = Matrix.Translation((x, y, (z0 + z1) / 2))
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=z1 - z0, matrix=m)


def bevel_all(obj, width=0.02, segments=2, angle=35.0):
    mod = obj.modifiers.new('bevel', 'BEVEL')
    mod.width = width
    mod.segments = segments
    mod.limit_method = 'ANGLE'
    mod.angle_limit = math.radians(angle)
    mod = obj.modifiers.new('normals', 'WEIGHTED_NORMAL')
    mod.keep_sharp = True


def part(name, build, bevel=0.015):
    bm = bmesh.new()
    build(bm)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = new_object(name, bm)
    if bevel:
        bevel_all(obj, bevel)
    return obj


# ---------------------------------------------------------------------------------------------- the parts

def knob(bm):
    # SŁ-2M's potentiometer knob (samsim_panels, SAM Simulator's AK photo at 8x: ЯРКОСТЬ, КОНТРАСТ., ФОКУС): black
    # bakelite, a near-cylinder as tall as 0.7 of its width (17 mm on a 24 mm knob, 21 on a 30), a little wider at its
    # foot, a flat top rounded at the edge, and eight shallow flutes down its side.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.05), (0.475, 0.09), (0.46, 0.6), (0.445, 0.665), (0.4, 0.695), (0.33, 0.7),
               (0.0, 0.7)], seg=64, ribs=8, rib_depth=0.035, rib_rows=(2, 3, 4))


def knob_pointer(bm):
    # The bar ("chicken-head") pointer knob of the Soviet consoles (usaf_shaw_2017 -024, opisybroni 54539): a low
    # skirt, a short collar and a flat wedge that tapers to a point, reaching a little past the skirt.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.1), (0.44, 0.15), (0.34, 0.17), (0.3, 0.3), (0.0, 0.3)])
    outline = [(-0.27, -0.3), (0.27, -0.3), (0.2, 0.2), (0.07, 0.66), (-0.07, 0.66), (-0.2, 0.2)]
    low = [bm.verts.new((x, y, 0.3)) for x, y in outline]
    top = [bm.verts.new((x, y, 0.5)) for x, y in outline]
    bm.faces.new(list(reversed(low)))
    bm.faces.new(top)
    n = len(outline)
    for k in range(n):
        bm.faces.new((low[k], low[(k + 1) % n], top[(k + 1) % n], top[k]))


def knob_bar(bm):
    # The R-123M's bakelite T-bar knob (r123_kuzmin, r123_commons): a round boss and a bar across its whole face, flared
    # at the ends, the pointer end (+y) longer and tapering, a steel screw in the middle (the colour is the knob's).
    lathe(bm, [(0.0, 0.0), (0.34, 0.0), (0.34, 0.3), (0.3, 0.36), (0.0, 0.36)])
    outline = [(-0.08, 0.0), (-0.14, -0.4), (-0.1, -0.46), (0.1, -0.46), (0.14, -0.4), (0.08, 0.0), (0.12, 0.38),
               (0.05, 0.5), (-0.05, 0.5), (-0.12, 0.38)]
    low = [bm.verts.new((x, y, 0.06)) for x, y in outline]
    top = [bm.verts.new((x, y, 0.62)) for x, y in outline]
    bm.faces.new(list(reversed(low)))
    bm.faces.new(top)
    n = len(outline)
    for k in range(n):
        bm.faces.new((low[k], low[(k + 1) % n], top[(k + 1) % n], top[k]))
    lathe(bm, [(0.0, 0.62), (0.07, 0.62), (0.07, 0.66), (0.0, 0.67)], seg=12)


def trim(bm):
    # Slotted trimmer head in its collar.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.1), (0.36, 0.1), (0.36, 0.22), (0.3, 0.26), (0.0, 0.26)])
    box(bm, 0.0, 0.0, 0.27, 0.62, 0.1, 0.06)       # the slot's raised lips stand in for the cut


def toggle_base(bm):
    # Hex nut on a threaded bushing.
    m = Matrix.Translation((0, 0, 0.08))
    bmesh.ops.create_cone(bm, cap_ends=True, segments=6, radius1=0.5, radius2=0.5, depth=0.16, matrix=m)
    # the knurled ring nut that locks it in the panel, and the threaded bushing above
    lathe(bm, [(0.0, 0.16), (0.36, 0.16), (0.36, 0.3), (0.0, 0.3)], ribs=24, rib_depth=0.05, rib_rows=(1, 2))
    cylinder(bm, 0.2, 0.3, 0.46)


def toggle_lever(bm):
    # 1 m long from the pivot: a tapered rod and a ball tip.
    lathe(bm, [(0.0, 0.0), (0.11, 0.0), (0.085, 0.86), (0.0, 0.86)], seg=24)
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=0.13, matrix=Matrix.Translation((0, 0, 0.9)))


def lamp_bezel(bm):
    # The signal lamp's fitting (SL-2M's ПА, ТСЦ, НЛЦ, РУ, ЦУ, АЗ and the like; samsim_panels at 12x, 14 Nov 2026): a
    # chrome flange on the plate, a knurled barrel standing about 12 mm out (0.8 of the fitting's width), and a plain
    # cap at its end round the glass. No source names its type; its shape is read off the photo.
    lathe(bm, [(0.33, 0.0), (0.5, 0.0), (0.5, 0.07), (0.45, 0.10), (0.40, 0.12), (0.40, 0.64), (0.43, 0.67),
               (0.43, 0.76), (0.39, 0.80), (0.335, 0.80), (0.335, 0.70)], seg=96, ribs=30, rib_depth=0.07,
          rib_rows=(4, 5))


def lamp_lens(bm):
    # The glass at the barrel's end (tinted and lit by the cabin), about 10 mm across.
    # Faceted: the signal lamps' glass is cut in flats (usaf_shaw_2017 -024), so few segments, not a smooth dome.
    lathe(bm, [(0.0, 0.70), (0.335, 0.70), (0.335, 0.80), (0.29, 0.88), (0.17, 0.93), (0.0, 0.945)], seg=10)


def gauge_bezel(bm):
    # The instrument's chrome rim: a ring with a rolled lip, open in the middle so the printed dial shows.
    lathe(bm, [(0.425, 0.0), (0.5, 0.0), (0.5, 0.09), (0.47, 0.15), (0.44, 0.13), (0.425, 0.06)], cap=False)


def meter_housing(bm):
    # The black bakelite flange of a round panel meter, round the chrome bezel (a ring: the dial stays open).
    lathe(bm, [(0.49, 0.0), (0.58, 0.0), (0.58, 0.05), (0.52, 0.09), (0.49, 0.09)], cap=False)


def button_ring(bm):
    lathe(bm, [(0.36, 0.0), (0.5, 0.0), (0.5, 0.14), (0.46, 0.22), (0.38, 0.22), (0.36, 0.16)])


def button_cap(bm):
    lathe(bm, [(0.0, 0.0), (0.34, 0.0), (0.34, 0.26), (0.3, 0.33), (0.0, 0.35)])


def jack(bm):
    # Insulated test jack: a collar, the coloured insulator, the hole.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.1), (0.42, 0.12), (0.4, 0.55), (0.35, 0.6), (0.16, 0.6),
               (0.16, 0.45), (0.0, 0.45)])


def socket(bm):
    # Coaxial connector: knurled body, the dark insulator, the centre pin.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.45), (0.44, 0.5), (0.3, 0.5), (0.3, 0.4), (0.06, 0.4),
               (0.06, 0.55), (0.0, 0.55)], ribs=30, rib_depth=0.05, rib_rows=(1, 2))


def _latch_outline(seg=6):
    """The latch plate's outline, in units of the lock's size: an upright hexagon with straight sides and chamfered ends,
    its corners barely rounded (SAM Simulator's AK photo at 3x: the plate 30 x 46 mm round a 20 mm barrel on a 22 mm
    latch, the ends' flats a half of its width)."""
    hw, hs, he, hf, r = 0.68, 0.42, 1.05, 0.34, 0.07
    corners = [(-hf, -he), (hf, -he), (hw, -hs), (hw, hs), (hf, he), (-hf, he), (-hw, hs), (-hw, -hs)]
    out = []
    n = len(corners)
    for k, (x, y) in enumerate(corners):
        px, py = corners[k - 1]
        nx, ny = corners[(k + 1) % n]
        a = (px - x, py - y)
        b = (nx - x, ny - y)
        la, lb = math.hypot(*a), math.hypot(*b)
        a, b = (a[0] / la, a[1] / la), (b[0] / lb, b[1] / lb)
        half = math.acos(max(-1.0, min(1.0, a[0] * b[0] + a[1] * b[1]))) / 2
        t = r / math.tan(half)
        bis = (a[0] + b[0], a[1] + b[1])
        lbis = math.hypot(*bis)
        c = (x + bis[0] / lbis * r / math.sin(half), y + bis[1] / lbis * r / math.sin(half))
        a0 = math.atan2(y + a[1] * t - c[1], x + a[0] * t - c[0])
        a1 = math.atan2(y + b[1] * t - c[1], x + b[0] * t - c[0])
        d = (a1 - a0 + math.pi) % (2 * math.pi) - math.pi      # the short way round the fillet
        for j in range(seg + 1):
            aa = a0 + d * j / seg
            out.append((c[0] + r * math.cos(aa), c[1] + r * math.sin(aa)))
    return out


LATCH_SCREW_Y = 0.74     # the plate's two screws, above and below the barrel (SAM's AK photo: 16 mm on a 22 mm latch)


def latch_plate(bm):
    # Block lock's plate, painted with the block (SAM Simulator's AK photo), 0.06 thick.
    pts = _latch_outline()
    bottom = [bm.verts.new((x, y, 0.0)) for x, y in pts]
    top = [bm.verts.new((x, y, 0.06)) for x, y in pts]
    bm.faces.new(list(reversed(bottom)))
    bm.faces.new(top)
    n = len(pts)
    for k in range(n):
        bm.faces.new((bottom[k], bottom[(k + 1) % n], top[(k + 1) % n], top[k]))


def latch(bm):
    # Block lock (ОТКР. / ЗАКР.): a matt nickel barrel 0.9 across standing 0.4 out of its plate, its face a ring round a
    # recess the black core sits in, the plug's bit across it (SAM Simulator's AK photo).
    lathe(bm, [(0.0, 0.06), (0.45, 0.06), (0.45, 0.38), (0.42, 0.42), (0.31, 0.42), (0.30, 0.40), (0.30, 0.30), (0.0, 0.30)])
    box(bm, 0.0, 0.0, 0.405, 0.08, 0.44, 0.03)


def latch_core(bm):
    # The lock's black core in the barrel's face, and the slots of the plate's two screws.
    lathe(bm, [(0.0, 0.31), (0.295, 0.31), (0.295, 0.395), (0.0, 0.395)])
    for y in (LATCH_SCREW_Y, -LATCH_SCREW_Y):
        box(bm, 0.0, y, 0.1125, 0.3, 0.055, 0.013)


def latch_screws(bm):
    # The plate's two cheese-head screws, steel.
    for y in (LATCH_SCREW_Y, -LATCH_SCREW_Y):
        res = bmesh.ops.create_cone(bm, cap_ends=True, segments=20, radius1=0.17, radius2=0.15, depth=0.05)
        bmesh.ops.translate(bm, verts=res['verts'], vec=(0.0, y, 0.085))


# СМЕНА ЛИТ.'s locking bar (0004-11M1; samsim_panels, SAM Simulator's AK photo read at 8x, about 1.05 mm a pixel; its
# open state from SAM Simulator's sprites osa050-052: the bar swings out to the left on a hinge standing up the face, like a
# gate, and shows its back). Modelled at 1 = the bar's length (46 mm), the hinge's axis along +Y at X = 0. The bar
# (SM_LetterBar) has its origin on the axis, LETTER_BAR_AXIS_Z out of the panel, so the game turns it about there; the
# bracket (SM_LetterBarHinge) stands on the panel. Measured: the bar 46 mm long, 14 mm tall at the hinge, its lower edge
# rising from 15 mm out to the tip (7 mm tall there), lying just above the knob's pointer at НОРМ.; a round boss 14 mm
# across 22 mm along it; the bracket 9 mm wide, its foot running up the face to a screw 18 mm above the axis.
LETTER_BAR_MM = 46.0
LETTER_BAR_AXIS_Z = 16.0 / LETTER_BAR_MM


def _mm(*v):
    return tuple(x / LETTER_BAR_MM for x in v)


def _prism(bm, outline, z0, z1):
    low = [bm.verts.new((x, y, z0)) for x, y in outline]
    top = [bm.verts.new((x, y, z1)) for x, y in outline]
    bm.faces.new(list(reversed(low)))
    bm.faces.new(top)
    n = len(outline)
    for k in range(n):
        bm.faces.new((low[k], low[(k + 1) % n], top[(k + 1) % n], top[k]))


def _cyl_y(bm, r, y0, y1, x, z, seg=20):
    m = Matrix.Translation((x, (y0 + y1) / 2, z)) @ Matrix.Rotation(math.radians(90), 4, 'X')
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=y1 - y0, matrix=m)


def letter_bar(bm):
    # The pressed bar, 2 mm thick, its knuckle round the pin, and the boss on its face (origin on the hinge's axis).
    outline = [_mm(x, y) for x, y in ((2.5, -7.0), (15.0, -7.0), (42.0, 0.0), (44.0, 3.0), (43.0, 7.0), (2.5, 7.0))]
    _prism(bm, outline, *_mm(-1.0, 1.0))
    _cyl_y(bm, 3.0 / LETTER_BAR_MM, *_mm(-6.5, 6.5), 0.0, 0.0)
    lathe_at = [(r / LETTER_BAR_MM, z / LETTER_BAR_MM) for r, z in ((0.0, 0.8), (7.0, 0.8), (7.0, 3.6), (6.0, 4.6), (0.0, 5.0))]
    rows = lathe(bm, lathe_at, seg=32)
    for row in rows:
        for vtx in row:
            vtx.co.x += 21.6 / LETTER_BAR_MM
            vtx.co.y += -1.3 / LETTER_BAR_MM


def letter_bar_hinge(bm):
    # The bracket on the panel: a foot plate from under the hinge up to its screw, the block the pin stands in, the pin's
    # two heads above and below the bar's knuckle, and the cheese-head screw.
    box(bm, *_mm(0.5, 6.5, 0.75), *_mm(7.0, 27.0, 1.5))
    box(bm, *_mm(0.0, 0.0, 6.5), *_mm(8.0, 14.0, 10.0))
    _cyl_y(bm, 3.0 / LETTER_BAR_MM, *_mm(-9.0, -6.6), 0.0, LETTER_BAR_AXIS_Z)
    _cyl_y(bm, 3.0 / LETTER_BAR_MM, *_mm(6.6, 9.0), 0.0, LETTER_BAR_AXIS_Z)
    box(bm, *_mm(0.0, -7.8, (11.5 + 16.0) / 2), *_mm(6.0, 2.4, 16.0 - 11.5))
    box(bm, *_mm(0.0, 7.8, (11.5 + 16.0) / 2), *_mm(6.0, 2.4, 16.0 - 11.5))
    res = bmesh.ops.create_cone(bm, cap_ends=True, segments=20, radius1=3.5 / LETTER_BAR_MM, radius2=3.1 / LETTER_BAR_MM,
                                depth=2.0 / LETTER_BAR_MM)
    bmesh.ops.translate(bm, verts=res['verts'], vec=_mm(2.6, 17.7, 2.5))


def wheel_dish(bm):
    # The hand wheel: a shallow dish with a rounded rim, on a hub.
    lathe(bm, [(0.0, 0.0), (0.12, 0.0), (0.12, 0.06), (0.4, 0.1), (0.47, 0.16), (0.5, 0.22), (0.48, 0.26),
               (0.44, 0.24), (0.38, 0.17), (0.12, 0.13), (0.0, 0.13)])


def wheel_dome(bm):
    # The domed hand wheel (SL-2M's range wheel): a white drum with a flat face, six screws round it, and a dome in its
    # middle (samsim_panels, the AK photo at 4x: the face 132 px across, the screws 62 px out, the dome 71 px across:
    # 0.47 and 0.27 of the width; heights by eye from its shading, about +-5 mm). The face 0.22 high where the crank
    # stands (0.4 of the width out, SimCabinComponent::AnimatePart), the dome's crown 0.41.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.185), (0.49, 0.21), (0.47, 0.22), (0.30, 0.22), (0.28, 0.225),
               (0.265, 0.245), (0.25, 0.30), (0.21, 0.35), (0.15, 0.385), (0.08, 0.403), (0.0, 0.408)])
    for k in range(6):
        a = math.radians(90 + 60 * k + 15)
        res = bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=0.022, radius2=0.018, depth=0.012)
        bmesh.ops.translate(bm, verts=res['verts'], vec=(0.47 * math.cos(a), 0.47 * math.sin(a), 0.226))


def wheel_handle(bm):
    # The crank handle: 1 m long, a stem and a spinner grip.
    lathe(bm, [(0.0, 0.0), (0.12, 0.0), (0.12, 0.3), (0.2, 0.34), (0.22, 0.9), (0.16, 1.0), (0.0, 1.0)], seg=24)


def screw(bm):
    # Cheese-head panel screw with a slot.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.2), (0.42, 0.3), (0.0, 0.3)], seg=24)
    box(bm, 0.0, 0.0, 0.31, 0.8, 0.12, 0.03)


def key_barrel(bm):
    # ЗАПРЕТ ПУСКА's lock (samsim_panels at 12x, the project owner's reading): a grey metal barrel, its side rounding over
    # into a narrow flat rim, its face sloping in from the rim like a shallow cone down to the key cylinder (19 mm across)
    # at its middle; the cylinder's keyhole and the key are drawn by the control. Unit diameter, 0.40 tall (41 mm across,
    # 16 mm out at the rim).
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.30), (0.49, 0.36), (0.465, 0.39), (0.43, 0.40), (0.40, 0.395),
               (0.30, 0.32), (0.235, 0.27), (0.22, 0.265), (0.21, 0.28), (0.20, 0.295), (0.0, 0.295)], seg=96)


PARTS = [
    ('SM_KeyBarrel', key_barrel, 0.006),
    ('SM_Knob', knob, 0.012), ('SM_KnobPointer', knob_pointer, 0.012), ('SM_KnobBar', knob_bar, 0.012),
    ('SM_Trim', trim, 0.01),
    ('SM_ToggleBase', toggle_base, 0.015), ('SM_ToggleLever', toggle_lever, 0.0), ('SM_LampBezel', lamp_bezel, 0.01),
    ('SM_LampLens', lamp_lens, 0.0), ('SM_ButtonRing', button_ring, 0.01), ('SM_ButtonCap', button_cap, 0.0),
    ('SM_Jack', jack, 0.01), ('SM_Socket', socket, 0.01), ('SM_Latch', latch, 0.01), ('SM_LatchPlate', latch_plate, 0.006), ('SM_LatchCore', latch_core, 0.0),
    ('SM_LatchScrews', latch_screws, 0.006), ('SM_LetterBar', letter_bar, 0.004), ('SM_LetterBarHinge', letter_bar_hinge, 0.004),
    ('SM_WheelDish', wheel_dish, 0.0),
    ('SM_WheelDome', wheel_dome, 0.0), ('SM_WheelHandle', wheel_handle, 0.0), ('SM_Screw', screw, 0.0),
    ('SM_GaugeBezel', gauge_bezel, 0.006),
    ('SM_MeterHousing', meter_housing, 0.006),
]

# SŁ-2M's hinged covers and levers (sl2m_covers.py) are modelled full size in metres, their hinge at the origin; the
# cabin places them at scale 1 (simcore::CabinFlap). Their bevel is in metres too.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sl2m_covers  # noqa: E402

PARTS += [(name, build, 0.0 if name.endswith('Text') else 0.0004) for name, build in sl2m_covers.PARTS]


def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                             axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True, mesh_smooth_type='FACE',
                             add_leaf_bones=False, bake_anim=False)
    return path


def preview(objs, path):
    """A contact sheet: the parts in a row of rows, lit and rendered from in front and above."""
    cols = 5
    for i, obj in enumerate(objs):
        obj.location = Vector(((i % cols) * 1.5, -(i // cols) * 1.5, 0.0))
    mat = bpy.data.materials.new('preview')
    mat.use_nodes = True
    mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.55, 0.55, 0.52, 1)
    mat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.35
    for obj in objs:
        obj.data.materials.append(mat)
    rows = (len(objs) + cols - 1) // cols
    centre = Vector(((cols - 1) * 0.75, -(rows - 1) * 0.75, 0.3))
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    bpy.context.scene.collection.objects.link(cam)
    cam.location = centre + Vector((0.0, -6.0, 7.0))
    cam.rotation_euler = (centre - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = 38
    bpy.context.scene.camera = cam
    for loc, energy in (((4, -6, 8), 900), ((-4, 2, 6), 400)):
        light = bpy.data.objects.new('light', bpy.data.lights.new('light', 'AREA'))
        light.data.energy = energy
        light.data.size = 5
        light.location = centre + Vector(loc)
        light.rotation_euler = (centre - light.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.scene.collection.objects.link(light)
    world = bpy.data.worlds.new('w')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.05, 0.05, 0.06, 1)
    bpy.context.scene.world = world
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items.keys() else 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1400, 900
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    preview_path = argv[argv.index('--preview') + 1] if '--preview' in argv else None
    clear()
    os.makedirs(OUT, exist_ok=True)
    objs = [part(name, build, bev) for name, build, bev in PARTS]
    only = set(argv[argv.index('--only') + 1].split(',')) if '--only' in argv else None   # the others' FBX stay as they are
    for obj in objs:
        if only is None or obj.name in only:
            export(obj)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'cabin_parts.blend'))
    print('exported', len(objs), 'parts to', OUT)
    if preview_path:
        preview(objs, os.path.abspath(preview_path))
        print('preview', preview_path)


main()
