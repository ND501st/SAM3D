"""SP-2M2's face as a 3D model, built from its 3D trace (tools/blender/sp2m_trace.py: the blocks' measured edges, 507
parts in mm from the flattened photo, and how far each stands out of the plate).

Run inside Blender through tools/blender/build_sp2m.py (the face alone: export, manifest, previews and the photo checks).
The rack's carcass round it (cabinet, posts, rails, the doors below) stays build_cabin_interior's SM_RackSP2M.

Frame (the model's own, as the R-123M's): the face looks body +y; x is to the operator's left when he faces it (so the
face's right is -x), z up; the face's centre at the origin, its plate plane (the blocks' fronts) at y = 0. In the cabin it
stands at the sp2m_panel's centre turned by the panel's facing.

What is the photo's and what is SAM3D's (see the trace): every position and size on the face is measured; the heights of
the tables, knobs, meters, socket field, locks, bezels, strips and rails are measured to about 3 mm; the other heights,
every part's shape in depth (caps, lenses, levers, the handles' grips) and the colours of parts the photo shows only as
a shade are SAM3D's.
"""
import math

from mathutils import Vector

from build_cabin_interior import rrect

import sp2m_trace as T

MM = 0.001
U = Vector((-1.0, 0.0, 0.0))          # the face's right in the model frame
V = Vector((0.0, 0.0, 1.0))           # up
N = Vector((0.0, 1.0, 0.0))           # out of the face

LENS = {'orange': 'Lens_Amber', 'brown': 'Lens_Amber', 'green': 'Lens_Green', 'red': 'Paint_Red', None: 'Lamp_Glass'}
JACK = {'black': 'Bakelite_Black', 'orange': 'Lens_Amber', 'red': 'Paint_Red', None: 'Bakelite_Black'}


def at(c, h=0.0):
    """A trace point (x, y mm) h mm out of the plate, in the model frame (m)."""
    a, b = T.to_m(c[0], c[1])
    return Vector((-a, h * MM, b))


def img_dir(deg):
    """A direction on the face given in the image (degrees anticlockwise from the right, y down): (dx, dy) mm per mm."""
    t = math.radians(deg)
    return math.cos(t), -math.sin(t)


def face_vec(dx, dy):
    """A trace-direction (x right, y down) as a model-frame vector."""
    return U * dx - V * dy


def box_mm(b, r, h0, h1, mat):
    x0, y0, x1, y1 = r
    p0, p1 = at((min(x0, x1), min(y0, y1)), h0), at((max(x0, x1), max(y0, y1)), h1)
    b.box(p0.x, p1.x, p0.y, p1.y, p0.z, p1.z, mat)


def disc(b, c, r_mm, h0, h1, mat, seg=24, r1_mm=None):
    b.cylinder(at(c, h0), at(c, h1), r_mm * MM, mat, seg=seg, r1=None if r1_mm is None else r1_mm * MM)


def lathe(b, c, h0, profile, mat, seg=24):
    """Turned part at c: profile [(r mm, h mm)] from h0 out."""
    b.lathe(at(c, h0), N, [(r * MM, h * MM) for r, h in profile], mat, seg=seg)


def bar_on_face(b, c, d, length, width, h0, h1, mat, from_centre=False):
    """A bar on the face from c along trace direction d (unit), length mm, width mm, between h0 and h1."""
    u = face_vec(*d).normalized()
    v = N.cross(u).normalized()
    off = 0.0 if from_centre else length / 2
    centre = at((c[0] + d[0] * off, c[1] + d[1] * off), (h0 + h1) / 2)
    b.obox(centre, u, v, N, (length * MM, width * MM, (h1 - h0) * MM), mat)


# ------------------------------------------------------------------------------------------------ the face

def build_face(b):
    # the recess behind and between the blocks (the frame's rails and the shadow band), and its back
    zf = T.Z['frame']
    box_mm(b, (0.0, -2.0, T.W_MM, T.H_MM), zf - 4.0, zf, 'Paint_Rack')
    for name, (r, _) in T.BLOCKS.items():
        x0, y0, x1, y1 = r
        # the block's front plate and its body back to the recess
        box_mm(b, (x0 + 0.8, y0 + 0.8, x1 - 0.8, y1 - 0.8), zf, 0.0, 'Paint_LightGrey')
        # the thin dark lip round its edge, at the recess (the photos' lip line)
        for e in ((x0, y0, x1, y0 + 0.8), (x0, y1 - 0.8, x1, y1), (x0, y0, x0 + 0.8, y1), (x1 - 0.8, y0, x1, y1)):
            box_mm(b, e, zf, -0.6, 'Metal_Dark')


def screw(b, c, h=0.0, r_mm=3.1):
    lathe(b, c, h - 0.3, [(r_mm, 0.0), (r_mm, 1.1), (r_mm * 0.8, 1.7), (0.0, 1.9)], 'Metal_Steel', seg=12)
    bar_on_face(b, c, img_dir(25), 2 * r_mm * 0.9, 0.55, h + 1.6, h + 2.2, 'Metal_Dark', from_centre=True)


def build_parts(b):
    Z = T.Z
    for p in T.parts():
        k = p['kind']
        c = p.get('c')
        if k == 'screw':
            screw(b, c, 0.0, p.get('d', 6.2) / 2)
        elif k == 'lock':
            r = p.get('d', 27.0) / 2
            lathe(b, c, 0.0, [(r, 0.0), (r, Z['lock'] - 1.2), (r - 1.2, Z['lock']), (0.0, Z['lock'])], 'Metal_Steel')
            bar_on_face(b, c, (0.0, 1.0), r * 1.2, 3.0, Z['lock'] - 0.6, Z['lock'] + 0.4, 'Metal_Dark', from_centre=True)
        elif k == 'key':
            disc(b, c, 3.0, 0.0, 3.0, 'Metal_Steel', seg=12)
            bar_on_face(b, (c[0], c[1] + 2.0), (0.0, 1.0), 10.0, 5.0, 1.0, 3.0, 'Metal_Steel')
        elif k == 'knob':
            w, h = p['size']
            r = min(w, h) / 2
            lathe(b, c, 0.0, [(r, 0.0), (r, Z['knob_skirt'] - 1.5), (r - 1.5, Z['knob_skirt']), (r * 0.55, Z['knob_skirt'])],
                  'Bakelite_Black')
            d = (1.0, 0.0) if w >= h else (0.0, 1.0)
            t = math.radians(p.get('angle', 0) or 0)
            d = (d[0] * math.cos(t) - d[1] * math.sin(t), d[0] * math.sin(t) + d[1] * math.cos(t))
            bar_on_face(b, c, d, max(w, h), r * 0.6, Z['knob_skirt'], Z['knob'], 'Bakelite_Black', from_centre=True)
            bar_on_face(b, c, d, max(w, h) * 0.45, 1.2, Z['knob'], Z['knob'] + 0.3, 'Paint_Letters')
        elif k in ('cyl_knob', 'knob_round'):
            r, h = p['d'] / 2, Z[k]
            lathe(b, c, 0.0, [(r * 0.7, 0.0), (r * 0.7, 3.0), (r, 3.0), (r, h - 1.5), (r - 1.5, h), (0.0, h)], 'Bakelite_Black')
        elif k in ('fuse', 'fuse_black'):
            w, h = p['size']
            box_mm(b, (c[0] - w / 2, c[1] - h / 2, c[0] + w / 2, c[1] + h / 2), 0.0, Z['fuse_base'],
                   'Bakelite_Black' if k == 'fuse_black' else 'Paint_Equipment')
            r = min(w, h) * 0.32
            lathe(b, c, Z['fuse_base'], [(r, 0.0), (r, Z['fuse_cap'] - Z['fuse_base'] - 2.0),
                                          (r * 0.8, Z['fuse_cap'] - Z['fuse_base']), (0.0, Z['fuse_cap'] - Z['fuse_base'])],
                  'Bakelite_Black')
        elif k == 'lamp':
            r = p.get('d', 10.8) / 2
            lathe(b, c, 0.0, [(r, 0.0), (r, 4.0), (r * 0.82, 4.0)], 'Metal_Steel')
            lathe(b, c, 4.0, [(r * 0.82, 0.0), (r * 0.8, 3.5), (r * 0.55, 5.4), (0.0, Z['lamp'] - 4.0)],
                  LENS.get(p.get('colour'), 'Lamp_Glass'))
        elif k == 'jack':
            r = p.get('d', 20.1) / 2
            lathe(b, c, 0.0, [(r, 0.0), (r, Z['jack'] - 1.0), (r - 1.0, Z['jack']), (4.0, Z['jack']), (4.0, Z['jack'] - 3.0),
                              (0.0, Z['jack'] - 3.0)], JACK.get(p.get('colour'), 'Bakelite_Black'))
        elif k in ('toggle', 'toggle_guarded'):
            disc(b, c, 6.0, 0.0, Z['toggle_nut'], 'Metal_Steel', seg=6)
            b.cylinder(at(c, Z['toggle_nut']), at((c[0], c[1] + 3.0), Z['toggle_bat']), 2.2 * MM, 'Metal_Steel', seg=10,
                       r1=1.4 * MM)
            if k == 'toggle_guarded':
                for s in (-1, 1):
                    bar_on_face(b, (c[0] + s * 9.0, c[1]), (0.0, 1.0), 24.0, 1.5, 0.0, Z['toggle_bat'] + 2.0, 'Paint_Red',
                                from_centre=True)
        elif k == 'strip':
            x0, y0, x1, y1 = p['rect']
            # the tall ones are the brown terminal strips with their labels' leader lines (g093-b, L2); the short ones
            # the dark label strips between the lamps
            tall = abs(y1 - y0) > 3 * abs(x1 - x0)
            box_mm(b, p['rect'], 0.0, Z['strip'], 'Vinyl_Brown' if tall else 'Metal_Dark')
        elif k == 'plate':
            box_mm(b, p['rect'], 0.0, Z['plate'], 'Metal_Dark')
        elif k == 'bezel':
            h = Z['bezel']
            lathe(b, c, 0.0, [(31.3, 0.0), (31.3, h - 1.5), (29.8, h), (19.5, h), (19.5, 0.5)], 'Bakelite_Black', seg=48)
            disc(b, c, 19.6, 0.0, 0.5, 'Paint_Letters', seg=48)          # the scale behind the window
            disc(b, c, 19.6, h - 1.6, h - 1.0, 'Glass', seg=48)
            disc(b, p['plug'], 6.0, 0.0, 6.0, 'Bakelite_Black', seg=16)
            for s in p['studs']:
                disc(b, s, 2.5, 0.0, 4.0, 'Metal_Steel', seg=10)
        elif k in ('meter', 'meter_white'):
            x0, y0, x1, y1 = p['rect']
            ix0, iy0, ix1, iy1 = p['inner']
            box_mm(b, p['rect'], 0.0, Z['meter'], 'Bakelite_Black')
            if k == 'meter':
                # the housing's rim round the scale
                for e in ((x0, y0, x1, y0 + 7), (x0, y1 - 7, x1, y1), (x0, y0, x0 + 7, y1), (x1 - 7, y0, x1, y1)):
                    box_mm(b, e, Z['meter'], Z['meter_rim'], 'Bakelite_Black')
            if iy1 <= y0 + 1.0:
                # the light shade over the meter's top (g093-b, OR-11M): a trapezoid, narrower at the top
                (a0, b0, a1, b1) = T.rect_m(p['inner'])
                inset = (a1 - a0) * 0.12
                outline = [(a0, b0), (a1, b0), (a1 - inset, b1), (a0 + inset, b1)]
                b.slab(at((T.W_MM / 2, T.H_MM / 2), Z['meter_hood']), U, V, outline, [], Z['meter_hood'] * MM, 'Paint_LightGrey')
                # the scale: white, its top an arc, over the zero screw's dark lower third
                sx0, sy0, sx1 = x0 + 8, y0 + 8, x1 - 8
                sy1 = y0 + (y1 - y0) * 0.66
                box_mm(b, (sx0, sy0 + (sx1 - sx0) * 0.12, sx1, sy1), Z['meter'], Z['meter'] + 0.3, 'Paint_Letters')
                disc(b, ((sx0 + sx1) / 2, sy0 + (sx1 - sx0) * 0.5), (sx1 - sx0) / 2 * 0.98, Z['meter'], Z['meter'] + 0.3,
                     'Paint_Letters', seg=40)
                disc(b, ((x0 + x1) / 2, y0 + (y1 - y0) * 0.83), 7.0, Z['meter'], Z['meter'] + 1.5, 'Metal_Dark', seg=16)
            else:
                box_mm(b, p['inner'], Z['meter'], Z['meter'] + 0.3, 'Paint_Letters')
            gz = Z['meter_rim'] - 2.0 if k == 'meter' else Z['meter'] + 2.0
            box_mm(b, (x0 + 5, y0 + 5, x1 - 5, y1 - 5), gz - 0.5, gz, 'Glass')
        elif k == 'sockets':
            box_mm(b, p['rect'], 0.0, Z['sockets'], 'Metal_Dark')
            for hc in p['holes']:
                lathe(b, hc, Z['sockets'], [(p['hole_d'] / 2, 0.0), (p['hole_d'] / 2, 0.8), (p['hole_d'] / 2 - 2.0, 0.8),
                                            (p['hole_d'] / 2 - 2.0, 0.6), (0.0, 0.6)], 'Rubber_Black', seg=20)
        elif k == 'table':
            # the chart table (a placard with rounded corners on two feet and a stud: g093-b, L2)
            x0, y0, x1, y1 = p['rect']
            (a0, b0, a1, b1) = T.rect_m(p['rect'])
            outline = rrect((a0 + a1) / 2, (b0 + b1) / 2, a1 - a0, b1 - b0, 0.012)
            b.slab(at((T.W_MM / 2, T.H_MM / 2), Z['table']), U, V, outline, [], Z['table_t'] * MM, 'Paint_LightGrey')
        elif k == 'foot':
            disc(b, c, p.get('d', 15.0) / 2, 0.0, Z['table'] - Z['table_t'], 'Metal_Steel', seg=16)
        elif k == 'stud':
            disc(b, c, p.get('d', 7.0) / 2, 0.0, Z['table'] + 1.5, 'Metal_Steel', seg=12)
        elif k == 'handle':
            # a C-shaped pull handle (g093-b: pairs of them either side of the jacks): two legs out of the face and the
            # grip between them
            x0, y0, x1, y1 = p['rect']
            w, h = abs(x1 - x0), abs(y1 - y0)
            r = min(w, h) / 2 * 0.55
            if h >= w:
                cx = (x0 + x1) / 2
                ends = ((cx, min(y0, y1) + r), (cx, max(y0, y1) - r))
            else:
                cy = (y0 + y1) / 2
                ends = ((min(x0, x1) + r, cy), (max(x0, x1) - r, cy))
            g = Z['handle'] - r
            for e in ends:
                disc(b, e, r * 1.7, 0.0, 2.0, 'Metal_Steel', seg=12)
                disc(b, e, r, 0.0, g + r * 0.9, 'Metal_Steel', seg=12)
                b.lathe(at(e, g), N, [(0.0, -r * MM), (r * 0.7 * MM, -r * 0.7 * MM), (r * MM, 0.0), (r * 0.7 * MM, r * 0.7 * MM),
                                     (0.0, r * MM)], 'Metal_Steel', seg=12)
            b.cylinder(at(ends[0], g), at(ends[1], g), r * MM, 'Metal_Steel', seg=12)
        elif k == 'bar':
            x0, y0, x1, y1 = p['rect']
            if abs(y1 - y0) < 3.0:
                y0, y1 = (y0 + y1) / 2 - 1.5, (y0 + y1) / 2 + 1.5
            box_mm(b, (x0, y0, x1, y1), 0.0, Z['bar'], 'Metal_Steel')
        elif k == 'lever_lock':
            disc(b, c, 9.0, 0.0, 4.0, 'Metal_Steel', seg=20)
            bar_on_face(b, c, img_dir(p['angle']), 30.0, 7.0, 4.0, Z['lever_lock'], 'Metal_Steel')
        elif k == 'plug':
            box_mm(b, p['rect'], 0.0, Z['plug'], 'Bakelite_Black')
        elif k == 'cable':
            x0, y0, x1, y1 = p['rect']
            r = min(abs(x1 - x0), abs(y1 - y0)) / 4
            if abs(x1 - x0) >= abs(y1 - y0):
                pts = [(min(x0, x1), (y0 + y1) / 2), (max(x0, x1), (y0 + y1) / 2)]
            else:
                pts = [((x0 + x1) / 2, min(y0, y1)), ((x0 + x1) / 2, max(y0, y1))]
            b.tube([at(pts[0], Z['cable'] - r), at(pts[1], Z['cable'] - r)], r * MM, 'Rubber_Black', seg=12)
        elif k == 'button':
            w, h = p['size']
            disc(b, c, min(w, h) / 2, 0.0, 3.0, 'Metal_Steel', seg=20)
            disc(b, c, min(w, h) / 2 * 0.7, 3.0, Z['button'], LENS.get(p.get('colour'), 'Lens_Green'), seg=20)
        elif k == 'socket_round':
            r = p['d'] / 2
            lathe(b, c, 0.0, [(r, 0.0), (r, 3.0), (r * 0.7, 3.0), (r * 0.7, Z['socket_round']), (r * 0.45, Z['socket_round']),
                              (r * 0.45, Z['socket_round'] - 5.0), (0.0, Z['socket_round'] - 5.0)], 'Metal_Steel', seg=28)
        elif k == 'divider':
            x0, y0, x1, y1 = p['rect']
            box_mm(b, ((x0 + x1) / 2 - 1.5, y0, (x0 + x1) / 2 + 1.5, y1), 0.0, Z['divider'], 'Metal_Dark')
        elif k == 'frame_connector':
            w, h = p['size']
            box_mm(b, (c[0] - w / 2, c[1] - h / 2, c[0] + w / 2, c[1] + h / 2), T.Z['frame'], 4.0, 'Metal_Dark')
            lathe(b, c, 4.0, [(18.0, 0.0), (18.0, Z['frame_connector'] - 4.0), (12.0, Z['frame_connector'] - 4.0),
                              (12.0, 10.0), (0.0, 10.0)], 'Metal_Steel', seg=28)
        else:
            raise ValueError('no shape for ' + k)


def build(B, finish):
    """Build SM_SP2M2Face with the interior script's Builder `B` and `finish`."""
    import bpy
    b = B('SM_SP2M2Face')
    build_face(b)
    build_parts(b)
    obj = finish(b, reuse=True)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    bpy.context.view_layer.update()
    return obj
