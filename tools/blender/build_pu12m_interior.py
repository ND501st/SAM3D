"""Build the PU-12M's fighting compartment in Blender from its trace and export it for Unreal (docs/PU12_DEVELOPMENT.md P6).

Everything comes from tools/blender/pu12m_interior_trace.py (the ČSLA plan's items, their heights, the hull's inside); the
mesh helpers are the Osa cabin's (build_cabin_interior.py: Builder, materials named after Unreal's MI_Cabin_<name>,
origin at the centre of the bounding box, body frame x right, y forward, z up). The cabin layout (data/cabins/pu12m.json,
written by tools/pu12m_cabin.py) places them and draws the panels, controls, lamps and screens on the equipment's faces.
Two of the Osa cabin's meshes are reused: the R-123M (three of them) and the dome lamp.

    "C:/Program Files/Blender Foundation/Blender 4.2/blender.exe" --background --python tools/blender/build_pu12m_interior.py
    ... -- --preview out/pu12m_interior/view      # also render the stations' views (PNG)

Writes art/pu12m_interior/<Mesh>.fbx and manifest.json (centres and sizes, body frame).
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_cabin_interior as C  # noqa: E402
import pu12m_interior_trace as I  # noqa: E402
import pu12m_trace as T  # noqa: E402

REPO = C.REPO
OUT = os.path.join(REPO, 'art', 'pu12m_interior')
C.COLLECTION = 'PU12MInterior'
C.PREVIEW['Paint_Light'] = (0.78, 0.79, 0.76)
LINING = 0.08          # the lining mesh's thickness, outwards from the inside face (Lumen's distance fields want a solid)
INSET = 0.004          # equipment faces stand this far behind their panels' planes (the panels' plates cover them)


def b3(x, y, z):
    """Trace (X forward, Y left, Z up) to the body frame (x right, y forward, z up)."""
    return (-y, x, z)


def body_box(b, it, mat=None, shrink=0.0):
    b.box(-it['y1'] + shrink, -it['y0'] - shrink, it['x0'] + shrink, it['x1'] - shrink, it['z0'], it['z1'], mat or it['mat'])


# ---------------------------------------------------------------------------------------------- the lining

def ring(x, outward=0.0):
    """The inside section at X as a closed loop [(body x, z)], from the floor round the walls to the roof; `outward` m
    further out (the lining's outer face)."""
    zf, zc = I.floor_z(x), I.ceiling_z(x)
    sec = I.section(x)
    zk = min(max(sec[1][0], zf + 0.01), zc - 0.03)
    zch = min(max(sec[2][0], zk + 0.01), zc - 0.02)
    half = [(I.half_width(x, zf), zf), (I.half_width(x, zk), zk), (I.half_width(x, zch), zch), (I.half_width(x, zc), zc)]
    right = [(w + outward, z) for w, z in half]
    right[0] = (right[0][0], zf - outward)
    right[-1] = (right[-1][0], zc + outward)
    left = [(-w, z) for w, z in reversed(right)]
    return right + left   # body x positive (right) first, from the floor up, then the left from the roof down


def build_lining():
    b = C.Builder('SM_PU12MHull')
    x_front = I.FRONT_X + 0.45
    xs = [I.REAR_X + i * (x_front - I.REAR_X) / 24 for i in range(25)]
    for st in T.STATIONS:
        if I.REAR_X < st[0] < x_front and all(abs(st[0] - x) > 0.03 for x in xs):
            xs.append(st[0])
    xs.sort()
    inner = [ring(x) for x in xs]
    outer = [ring(x, LINING) for x in xs]
    n = len(inner[0])

    def v(x, p):
        return b.bm.verts.new(C.P(p[0], x, p[1]))

    vi = [[v(x, p) for p in r] for x, r in zip(xs, inner)]
    vo = [[v(x, p) for p in r] for x, r in zip(xs, outer)]
    for k in range(len(xs) - 1):
        for i in range(n):
            j = (i + 1) % n
            seg_mat = 'Floor_Steel' if i == n - 1 else 'Paint_Light'
            # the inside face looks inwards; the outside face outwards
            f = b.bm.faces.new((vi[k][i], vi[k + 1][i], vi[k + 1][j], vi[k][j]))
            f.material_index = b.mat(seg_mat)
            f = b.bm.faces.new((vo[k][j], vo[k + 1][j], vo[k + 1][i], vo[k][i]))
            f.material_index = b.mat('Paint_Light')
    # the ends: the rear bulkhead and the bow, each a ring between the inside and outside loops plus a cap
    for k, flip in ((0, False), (len(xs) - 1, True)):
        for i in range(n):
            j = (i + 1) % n
            quad = (vi[k][i], vi[k][j], vo[k][j], vo[k][i])
            f = b.bm.faces.new(tuple(reversed(quad)) if flip else quad)
            f.material_index = b.mat('Paint_Light')
    # the bulkhead plate itself and the bow's cap (closing the inside), set just inside the lining's ends
    x = xs[0] + 0.002
    b.slab((0, x, 0), (0, 0, 1), (1, 0, 0), [(pz, px) for px, pz in ring(x)], [], 0.04, 'Paint_Light')
    x = xs[-1] - 0.002
    b.slab((0, x, 0), (1, 0, 0), (0, 0, 1), [(px, pz) for px, pz in ring(x)], [], 0.04, 'Paint_Light')
    hatches(b)
    windscreens(b)
    # the floor plates' seams and a rubber mat in the aisle
    for x in (-1.2, -0.2, 0.8, 1.8):
        b.box(-0.85, 0.85, x - 0.006, x + 0.006, I.FLOOR_Z, I.FLOOR_Z + 0.004, 'Metal_Dark')
    b.box(-0.30, 0.30, -0.65, 1.95, I.FLOOR_Z, I.FLOOR_Z + 0.008, 'Rubber_Mat')
    return C.finish(b, sharp_deg=30)


def hatches(b):
    """The roof hatches (closed, seen from below) and the side hatches, after the exterior trace."""
    zr = I.ROOF_IN_Z
    for hx, hy, lx, ly in T.ROOF_HATCHES:
        x, y = -hy, hx
        b.box(x - ly / 2 - 0.03, x + ly / 2 + 0.03, y - lx / 2 - 0.03, y + lx / 2 + 0.03, zr - 0.012, zr, 'Paint_Rack')
        b.box(x - ly / 2, x + ly / 2, y - lx / 2, y + lx / 2, zr - 0.03, zr - 0.012, 'Paint_Light')
        b.cylinder((x - 0.12, y, zr - 0.06), (x + 0.12, y, zr - 0.06), 0.012, 'Metal_Dark', seg=10)  # the handle
        for s in (-1, 1):
            b.box(x + s * 0.12 - 0.012, x + s * 0.12 + 0.012, y - 0.012, y + 0.012, zr - 0.06, zr - 0.03, 'Metal_Dark')
    for hx, hz, lx, lz in T.SIDE_HATCHES:
        for side in (1, -1):
            # on the sloping upper side: a frame and a plate in the side's plane
            z0, z1 = hz - lz / 2, hz + lz / 2
            w0, w1 = I.half_width(hx, z0), I.half_width(hx, z1)
            if w0 is None or w1 is None:
                continue
            p0 = Vector((side * (w0 - 0.005), hx, z0))
            p1 = Vector((side * (w1 - 0.005), hx, z1))
            up = (p1 - p0).normalized()
            along = Vector((0, 1, 0))
            n = along.cross(up).normalized()
            if n.x * side > 0:
                n = -n                 # into the compartment
            centre = (p0 + p1) / 2
            b.obox(centre + n * 0.008, along, up, n, (lx + 0.06, (p1 - p0).length + 0.06, 0.016), 'Paint_Rack')
            b.obox(centre + n * 0.02, along, up, n, (lx, (p1 - p0).length, 0.012), 'Paint_Light')
            b.obox(centre + n * 0.05 - up * 0.05, along, up, n, (0.22, 0.025, 0.025), 'Metal_Dark')


def windscreens(b):
    """The two windscreens in the upper bow plate, their armoured covers closed (the squad leader and driver drawing)."""
    for wx, wy in I.WINDSCREENS:
        z = I.ceiling_z(wx) - 0.02
        along = Vector((-1, 0, 0))
        slope = Vector((0, 0.24, I.ceiling_z(wx + 0.12) - I.ceiling_z(wx - 0.12))).normalized()
        n = along.cross(slope)
        if n.z > 0:
            n = -n
        c = Vector((-wy, wx, z))
        b.obox(c, along, slope, n, (0.46, 0.26, 0.02), 'Paint_Rack')
        b.obox(c + n * 0.012, along, slope, n, (0.40, 0.20, 0.01), 'Glass')
    # the vision blocks along the top of the bow plate (the exterior's VISION_BLOCKS; the drawing's průzory): a prism's
    # dark glass in a steel housing under the plate
    for vx, vy in T.VISION_BLOCKS:
        z = I.ceiling_z(vx)
        b.box(-vy - 0.07, -vy + 0.07, vx - 0.05, vx + 0.05, z - 0.09, z + 0.01, 'Paint_Rack')
        b.box(-vy - 0.055, -vy + 0.055, vx - 0.056, vx - 0.05, z - 0.075, z - 0.02, 'Glass')


# ---------------------------------------------------------------------------------------------- equipment

EQUIPMENT_SKIP = {'seat_squad_leader', 'seat_driver', 'bench', 'bench_back', 'mast', 'winch', 'dash', 'r123_fug1',
                  'r123_fug2', 'r123_fug4'}


def face_inset(b, it):
    """A box whose working face stands INSET behind the item's boundary (the panel's plate covers it)."""
    x0, x1, y0, y1 = it['x0'], it['x1'], it['y0'], it['y1']
    face = I.FACES.get(it['id'])
    if face == 'rear':
        x0 += INSET
    elif face == 'front':
        x1 -= INSET
    elif face == 'left':
        y1 -= INSET
    elif face == 'right':
        y0 += INSET
    b.box(-y1, -y0, x0, x1, it['z0'], it['z1'], it['mat'])
    return x0, x1, y0, y1


def handles(b, it, face):
    """Two carrying handles on a cabinet's face (the drawings show them on most blocks)."""
    cx, cy, cz = I.centre(it)
    h = 0.35 * (it['z1'] - it['z0'])
    for s in (-1, 1):
        if face in ('left', 'right'):
            y = it['y1'] + 0.02 if face == 'left' else it['y0'] - 0.02
            x = cx + s * 0.4 * (it['x1'] - it['x0'])
            b.tube([b3(x, y + (-0.02 if face == 'left' else 0.02), cz - h), b3(x, y, cz - h), b3(x, y, cz + h),
                    b3(x, y + (-0.02 if face == 'left' else 0.02), cz + h)], 0.006, 'Metal_Steel', seg=8)
        else:
            x = it['x0'] - 0.02 if face == 'rear' else it['x1'] + 0.02
            y = cy + s * 0.4 * (it['y1'] - it['y0'])
            b.tube([b3(x + (0.02 if face == 'rear' else -0.02), y, cz - h), b3(x, y, cz - h), b3(x, y, cz + h),
                    b3(x + (0.02 if face == 'rear' else -0.02), y, cz + h)], 0.006, 'Metal_Steel', seg=8)


def build_equipment():
    b = C.Builder('SM_PU12MEquipment')
    for it in I.ITEMS:
        if it['id'] in EQUIPMENT_SKIP:
            continue
        iid, face = it['id'], I.FACES.get(it['id'])
        if it['shape'] == 'cylinder':
            cx, cy, _ = I.centre(it)
            r = 0.5 * (it['x1'] - it['x0'])
            if iid == 'gak':
                # the gyro: a cylinder on a square foot with a domed top (shilka_narod photograph)
                b.box(-cy - r - 0.04, -cy + r + 0.04, cx - r - 0.04, cx + r + 0.04, it['z0'], it['z0'] + 0.06, 'Paint_Rack')
                b.cylinder(b3(cx, cy, it['z0'] + 0.06), b3(cx, cy, it['z1'] - 0.10), r, 'Paint_Equipment', seg=32)
                b.lathe(b3(cx, cy, it['z1'] - 0.10), (0, 0, 1),
                        [(r, 0.0), (0.97 * r, 0.04), (0.85 * r, 0.075), (0.55 * r, 0.095), (0.0, 0.10)], 'Paint_Equipment')
                b.box(-cy - 0.06, -cy + 0.06, cx - r - 0.05, cx - r + 0.01, it['z0'] + 0.15, it['z0'] + 0.25, 'Paint_Rack')
            elif iid == 'di':
                # the marker: a round housing in the desk with a ball handle
                b.cylinder(b3(cx, cy, it['z0']), b3(cx, cy, it['z0'] + 0.025), r, 'Paint_Rack', seg=32)
                b.cylinder(b3(cx, cy, it['z0'] + 0.025), b3(cx, cy, it['z1'] - 0.02), 0.012, 'Metal_Steel', seg=12)
                b.lathe(b3(cx, cy, it['z1'] - 0.02), (0, 0, 1), [(0.0, -0.02), (0.02, -0.01), (0.02, 0.0), (0.0, 0.012)],
                        'Bakelite_Black', seg=16)
            elif iid == 'vo':
                b.cylinder(b3(cx, cy, it['z0']), b3(cx, cy, it['z1']), r * 0.6, 'Paint_Rack', seg=20)
                b.cylinder(b3(cx, cy, it['z0'] + 0.02), b3(cx + 0.09, cy, it['z0'] + 0.08), 0.03, 'Rubber_Black', seg=14)
            else:
                b.cylinder(b3(cx, cy, it['z0']), b3(cx, cy, it['z1']), r, it['mat'], seg=20)
            continue
        if iid == 'desk':
            # the top on the ledge, a front lip and two legs to the floor at its inner edge
            b.box(-it['y1'], -it['y0'], it['x0'], it['x1'], it['z0'], it['z1'], it['mat'])
            b.box(-it['y1'], -it['y1'] + 0.02, it['x0'], it['x1'], it['z0'] - 0.05, it['z0'], 'Paint_Rack')
            for x in (it['x0'] + 0.08, it['x1'] - 0.08):
                b.box(-it['y1'] + 0.005, -it['y1'] + 0.035, x - 0.015, x + 0.015, I.FLOOR_Z, it['z0'], 'Paint_Rack')
            continue
        if iid == 'table51':
            b.box(-it['y1'], -it['y0'], it['x0'], it['x1'], it['z0'], it['z1'], it['mat'])
            b.box(-it['y1'] + 0.01, -it['y1'] + 0.03, it['x0'] + 0.02, it['x1'] - 0.02, I.FLOOR_Z, it['z0'], 'Paint_Rack')
            continue
        if iid in ('r111_stand', 'rear_shelf'):
            # a frame: four posts and a top
            b.box(-it['y1'], -it['y0'], it['x0'], it['x1'], it['z1'] - 0.025, it['z1'], it['mat'])
            for x in (it['x0'] + 0.015, it['x1'] - 0.015):
                for y in (it['y0'] + 0.015, it['y1'] - 0.015):
                    b.box(-y - 0.012, -y + 0.012, x - 0.012, x + 0.012, it['z0'], it['z1'] - 0.025, it['mat'])
            continue
        if iid == 'it45':
            # the tube's housing: a box, its back (to the bench) dished round the screen, a hood over the top
            face_inset(b, it)
            s = I.IT45_SCREEN
            x = it['x0'] + INSET - 0.002
            b.torus(b3(x - 0.004, s['y'], s['z']), (0, 1, 0), s['d'] / 2 + 0.03, 0.018, 'Paint_Panel', seg=48, rseg=10)
            b.box(-it['y1'], -it['y0'], it['x0'] - 0.10, it['x0'] + INSET, it['z1'] - 0.012, it['z1'], 'Paint_Panel')
            continue
        x0, x1, y0, y1 = face_inset(b, it)
        if iid in ('telecode_tx', 'telecode_rx', 'r407', 'r111', 'scr', 'bp48', 'vs27'):
            handles(b, dict(it, x0=x0, x1=x1, y0=y0, y1=y1), face)
        if iid in ('telecode_tx', 'telecode_rx'):
            # the cabinet's two doors (the drawings' diagonal hatching): seams on its face
            yl = y1 + 0.002 if face == 'left' else y0 - 0.002
            for z in (it['z0'] + 0.04, 0.5 * (it['z0'] + it['z1']), it['z1'] - 0.04):
                b.box(-yl - 0.002, -yl + 0.002, x0 + 0.02, x1 - 0.02, z - 0.003, z + 0.003, 'Metal_Dark')
        if iid.startswith('battery'):
            for k in (-1, 1):
                cx = 0.5 * (it['x0'] + it['x1']) + k * 0.1
                cy = 0.5 * (it['y0'] + it['y1'])
                b.cylinder(b3(cx, cy, it['z1']), b3(cx, cy, it['z1'] + 0.025), 0.012, 'Metal_Dark', seg=10)
    return C.finish(b, bevel=0.003)


def build_bench():
    b = C.Builder('SM_PU12MBench')
    bench, back = I.item('bench'), I.item('bench_back')
    places = I.BENCH['places']
    for y0, y1 in places:
        # a cushion per place on a frame, its back behind it
        cy = 0.5 * (y0 + y1)
        w = abs(y1 - y0) - 0.012
        b.rounded_box(Vector((-cy, 0.5 * (bench['x0'] + bench['x1']), I.SEAT_Z - 0.04)),
                      (w, bench['x1'] - bench['x0'], 0.08), 0.03, 'Vinyl_Olive')
        b.rounded_box(Vector((-cy, 0.5 * (back['x0'] + back['x1']) + 0.03, back['z0'] + 0.08)),
                      (w, 0.07, back['z1'] - back['z0'] - 0.10), 0.03, 'Vinyl_Olive',
                      u=Vector((1, 0, 0)), v=Vector((0, 0, 1)))
    # the frame: a tube along the front edge and the back, posts to the floor
    y0, y1 = I.BENCH['y']
    xf, xb = bench['x1'] - 0.02, back['x0'] + 0.02
    b.tube([b3(xf, y0, I.SEAT_Z - 0.09), b3(xf, y1, I.SEAT_Z - 0.09)], 0.014, 'Paint_Rack')
    b.tube([b3(xb, y0, I.SEAT_Z - 0.09), b3(xb, y1, I.SEAT_Z - 0.09)], 0.014, 'Paint_Rack')
    for y in (y0 + 0.03, 0.5 * (y0 + y1), y1 - 0.03):
        b.tube([b3(xf, y, I.SEAT_Z - 0.09), b3(xf, y, I.FLOOR_Z)], 0.014, 'Paint_Rack')
        b.tube([b3(xb, y, I.SEAT_Z - 0.09), b3(xb, y, I.FLOOR_Z)], 0.014, 'Paint_Rack')
        b.tube([b3(xb - 0.02, y, I.SEAT_Z - 0.09), b3(xb - 0.02, y, back['z1'] - 0.04)], 0.012, 'Paint_Rack')
    return C.finish(b)


def build_front_seat():
    """The BTR-60PB's front seat: a cushion and a back on a tube frame. Built round its own centre, facing forward."""
    b = C.Builder('SM_PU12MFrontSeat')
    w, d = 0.42, 0.40
    zc = I.SEAT_Z
    b.rounded_box(Vector((0, 0.02, zc - 0.045)), (w, d, 0.09), 0.04, 'Vinyl_Olive')
    b.rounded_box(Vector((0, -0.19, zc + 0.27)), (w - 0.02, 0.07, 0.46), 0.04, 'Vinyl_Olive',
                  u=Vector((1, 0, 0)), v=Vector((0, 0, 1)))
    for x in (-0.17, 0.17):
        b.tube([(x, 0.17, zc - 0.09), (x, 0.17, I.FLOOR_Z), ], 0.014, 'Paint_Rack')
        b.tube([(x, -0.16, I.FLOOR_Z), (x, -0.16, zc - 0.09), (x, -0.22, zc + 0.45)], 0.014, 'Paint_Rack')
    b.tube([(-0.17, 0.17, zc - 0.09), (0.17, 0.17, zc - 0.09)], 0.012, 'Paint_Rack')
    return C.finish(b, reuse=True)


def wheel_axis():
    tilt = math.radians(I.WHEEL['tilt_deg'])
    return Vector((0, -math.sin(tilt), math.cos(tilt)))   # the wheel's axis, leaning back toward the driver


def build_steering():
    """The wheel (its own part: the driver's hands rest on it, crew_body.hpp HandsOn "steering")."""
    b = C.Builder('SM_PU12MSteering')
    wl = I.WHEEL
    axis = wheel_axis()
    centre = Vector(b3(wl['x'], wl['y'], wl['z']))
    b.torus(centre, axis, wl['r'], 0.014, 'Bakelite_Black', seg=48, rseg=10)
    e1 = Vector((1, 0, 0))
    e2 = axis.cross(e1).normalized()
    for k in range(3):
        a = math.radians(90 + 120 * k)
        d = e1 * math.cos(a) + e2 * math.sin(a)
        b.cylinder(centre, centre + d * wl['r'], 0.008, 'Metal_Dark', seg=8)
    b.cylinder(centre - axis * 0.02, centre + axis * 0.03, 0.04, 'Bakelite_Black', seg=16)
    return C.finish(b)


def build_pedals():
    """Clutch, brake and accelerator (their own part: the driver's feet, crew_body.hpp FeetOn "pedals")."""
    b = C.Builder('SM_PU12MPedals')
    for dy, w in ((0.14, 0.07), (0.0, 0.07), (-0.15, 0.05)):
        y = I.WHEEL['y'] + dy
        x = 2.88
        z = I.floor_z(x) + 0.10
        b.obox(Vector(b3(x, y, z)), Vector((1, 0, 0)), Vector((0, math.cos(0.9), math.sin(0.9))),
               Vector((0, -math.sin(0.9), math.cos(0.9))), (w, 0.12, 0.012), 'Rubber_Black')
        b.cylinder(b3(x + 0.05, y, z + 0.05), b3(x + 0.12, y, z + 0.20), 0.008, 'Metal_Dark', seg=8)
    return C.finish(b)


def build_driver():
    """The rest of the driver's place: the wheel's column, the dash under the windscreen, the gear lever."""
    b = C.Builder('SM_PU12MDriver')
    wl = I.WHEEL
    axis = wheel_axis()
    centre = Vector(b3(wl['x'], wl['y'], wl['z']))
    dash = I.item('dash')
    col_end = Vector(b3(dash['x0'] + 0.02, wl['y'], dash['z0'] + 0.04))
    b.cylinder(centre - axis * 0.02, col_end, 0.03, 'Paint_Rack', seg=14)
    # the dash: a box, its face to the driver
    b.box(-dash['y1'], -dash['y0'], dash['x0'] + INSET, dash['x1'], dash['z0'], dash['z1'], 'Paint_Interior')
    b.box(-dash['y1'] - 0.01, -dash['y0'] + 0.01, dash['x0'] - 0.02, dash['x1'], dash['z1'], dash['z1'] + 0.02, 'Rubber_Black')
    # the gear lever and the hand brake between the seats
    gx, gy = 2.45, 0.5 * (I.py_y(557) + I.py_y(400))
    b.cylinder(b3(gx, gy, I.FLOOR_Z), b3(gx - 0.05, gy, I.FLOOR_Z + 0.45), 0.01, 'Metal_Dark', seg=8)
    b.lathe(b3(gx - 0.05, gy, I.FLOOR_Z + 0.45), (0, 0, 1), [(0.0, -0.01), (0.022, 0.0), (0.022, 0.02), (0.0, 0.035)],
            'Bakelite_Black', seg=14)
    b.box(-gy - 0.06, -gy + 0.06, gx - 0.08, gx + 0.08, I.FLOOR_Z, I.FLOOR_Z + 0.04, 'Rubber_Black')
    return C.finish(b)


def build_mast_foot():
    """The mast's fixed section inside the hull: on a floor plate, two clamps to the hull, the hand winch on its inboard
    side with its drum and crank (csla_pu12_manual, the mast drawings)."""
    b = C.Builder('SM_PU12MMastFoot')
    mx, my, r = T.MAST_X, T.MAST_Y, I.MAST_R
    b.cylinder(b3(mx, my, I.FLOOR_Z), b3(mx, my, I.FLOOR_Z + 0.02), r + 0.06, 'Paint_Rack', seg=28)
    b.cylinder(b3(mx, my, I.FLOOR_Z + 0.02), b3(mx, my, I.ROOF_IN_Z + 0.02), r, 'Metal_Steel', seg=28)
    for z in (1.35, 1.85):
        b.cylinder(b3(mx, my, z - 0.03), b3(mx, my, z + 0.03), r + 0.012, 'Paint_Rack', seg=28)
        w = I.half_width(mx, z)
        b.box(-(my - r * 0.5), w - 0.002, mx - 0.02, mx + 0.02, z - 0.02, z + 0.02, 'Paint_Rack')
    b.cylinder(b3(mx, my, I.ROOF_IN_Z - 0.02), b3(mx, my, I.ROOF_IN_Z), r + 0.05, 'Paint_Rack', seg=28)
    wn = I.WINCH
    sx, sy, sz = wn['size']
    b.box(-(wn['y'] + sy / 2), -(wn['y'] - sy / 2) + INSET, wn['x'] - sx / 2, wn['x'] + sx / 2, wn['z'] - sz / 2,
          wn['z'] + sz / 2, 'Paint_Rack')
    b.cylinder(b3(wn['x'] - sx / 2 - 0.02, wn['y'], wn['z']), b3(wn['x'] + sx / 2 + 0.02, wn['y'], wn['z']), 0.035,
               'Metal_Steel', seg=18)
    return C.finish(b)


def build_all():
    return [build_lining(), build_equipment(), build_bench(), build_front_seat(), build_steering(), build_pedals(),
            build_driver(), build_mast_foot()]


def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    loc = obj.location.copy()
    obj.location = (0, 0, 0)
    path = os.path.join(OUT, obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                             axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True, mesh_smooth_type='FACE',
                             add_leaf_bones=False, bake_anim=False, use_tspace=True)
    obj.location = loc
    return path


def place_copies(objs):
    """For the preview only: the front seats and the reused Osa meshes where the layout puts them."""
    seat = next(o for o in objs if o.name == 'SM_PU12MFrontSeat')
    for i, sid in enumerate(('seat_driver', 'seat_squad_leader')):
        it = I.item(sid)
        o = seat if i == 0 else seat.copy()
        if i:
            bpy.data.collections[C.COLLECTION].objects.link(o)
        cx, cy, _ = I.centre(it)
        o.location = C.P(-cy, cx, o.location.z)
    for x, y in I.DOME_LAMPS:
        light = bpy.data.objects.new('lamp', bpy.data.lights.new('lamp', 'POINT'))
        light.data.energy = 30
        light.data.color = (1.0, 0.88, 0.7)
        light.data.shadow_soft_size = 0.06
        light.location = C.P(-y, x, I.ROOF_IN_Z - 0.08)
        bpy.data.collections[C.COLLECTION].objects.link(light)


def preview(objs, prefix):
    place_copies(objs)
    sc = bpy.context.scene
    world = bpy.data.worlds.new('preview') if sc.world is None else sc.world
    sc.world = world
    world.color = (0.02, 0.022, 0.025)
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items]
    sc.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in engines else 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1280, 720
    cam = bpy.data.objects.new('preview_cam', bpy.data.cameras.new('preview_cam'))
    bpy.data.collections[C.COLLECTION].objects.link(cam)
    sc.camera = cam
    cam.data.clip_start = 0.01
    paths = []
    views = {k: (v['eye'], v['yaw'], v['pitch'], 14) for k, v in I.STATIONS.items()}
    views['aisle_aft'] = ((2.0, 0.05, 1.70), 180.0, -12.0, 12)
    views['aisle_fwd'] = ((-1.6, 0.0, 1.70), 0.0, -10.0, 12)
    views['plan'] = None
    for name, v in views.items():
        if v is None:
            cam.data.type = 'ORTHO'
            cam.data.ortho_scale = 7.6
            cam.location = (0.4, 0.0, 6.0)
            cam.data.clip_start = 6.0 - (I.ROOF_IN_Z - 0.06)   # under the roof: the plan's view
            cam.rotation_euler = (0, 0, math.pi)                # the bow to the left, the right side at the top
            sc.render.resolution_x, sc.render.resolution_y = 1600, 700
        else:
            (x, y, z), yaw, pitch, lens = v
            cam.data.type = 'PERSP'
            cam.data.clip_start = 0.01
            cam.location = C.P(-y, x, z)
            cam.rotation_euler = (math.radians(90 + pitch), 0, math.radians(-90 - yaw))
            cam.data.lens = lens
            sc.render.resolution_x, sc.render.resolution_y = 1280, 720
        sc.render.filepath = '%s_%s.png' % (prefix, name)
        bpy.ops.render.render(write_still=True)
        paths.append(sc.render.filepath)
    return paths


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    C.COLLECTION = 'PU12MInterior'
    coll = bpy.data.collections.get(C.COLLECTION)
    if coll is None:
        coll = bpy.data.collections.new(C.COLLECTION)
        bpy.context.scene.collection.children.link(coll)
    for o in list(coll.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    os.makedirs(OUT, exist_ok=True)
    objs = build_all()
    manifest = {}
    for obj in objs:
        export(obj)
        # a reused mesh's centre is in its own frame (the layout adds where each copy stands)
        manifest[obj.name] = {'size_m': [round(v, 4) for v in obj['body_size']],
                              'centre_m': [round(v, 4) for v in obj['body_centre']], 'reuse': bool(obj['reuse'])}
    with open(os.path.join(OUT, 'manifest.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(manifest, f, indent=1, sort_keys=True)
        f.write('\n')
    print('exported', len(objs), 'meshes to', OUT)
    if '--preview' in argv:
        prefix = os.path.abspath(argv[argv.index('--preview') + 1])
        os.makedirs(os.path.dirname(prefix), exist_ok=True)
        for p in preview(objs, prefix):
            print('preview', p)


if __name__ == '__main__':
    main()
