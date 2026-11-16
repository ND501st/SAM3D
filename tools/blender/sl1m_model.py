"""SŁ-1M's face as a 3D model, from its trace (tools/blender/sl1m_trace.py). Run inside Blender through tools/blender/build_sl1m.py; the carcass is build_cabin_interior's SM_RackSL1M.

Frame as SŁ-3M's (sl3m_model.py), whose helpers this module uses, pointed at this rack's trace: one rack per Blender run. The orientation controls and the ЗК-4 dial's face are
the game's (`data/cabins/osa_ak.json`), so the model leaves them out and draws only the dial's ring round them. The plates' legends are not drawn (decals, roadmap I10).
"""
import math

from build_cabin_interior import rrect

import sl1m_trace as T
import sl3m_model as M

M.T = T          # the helpers (at, box_mm, rr_slab ...) read the trace through this
MM = M.MM
U, V, N = M.U, M.V, M.N
CREAM, STEEL, DARK, BLACK, RED, PLATE, RACK = M.CREAM, M.STEEL, M.DARK, M.BLACK, M.RED, M.PLATE, M.RACK
AMBER = 'Lens_Amber'
LAMP = {'dark': DARK, 'amber': RED, 'cream': CREAM}
JACK = {'black': BLACK, 'steel': STEEL, 'red': RED}


def build_surround(b):
    xl0, xl1 = T.FRAME_LEFT
    xr0, xr1 = T.FRAME_RIGHT
    yt0, yt1 = T.FRAME_TOP
    yb0, yb1 = T.FRAME_BOTTOM
    M.box_mm(b, (xl0, yt0, xr1, yb1), -24.0, -20.0, DARK)
    M.box_mm(b, (xl0, yt0, xl1, yb1), -20.0, 3.0, PLATE)
    M.box_mm(b, (xr0, yt0, xr1, yb1), -20.0, 3.0, PLATE)
    M.box_mm(b, (xl1, yt0, xr0, yt1), -20.0, 3.0, PLATE)
    M.box_mm(b, (xl1, yb0, xr0, yb1), -20.0, 3.0, PLATE)
    M.box_mm(b, (T.DIVIDER[0], yt1, T.DIVIDER[1], yb0), -20.0, 4.0, PLATE)
    M.box_mm(b, (T.DIVIDER[0] + 12.0, yt1, T.DIVIDER[1] - 12.0, yb0), 4.0, 4.6, RACK)     # the channel's slot
    M.rr_slab(b, (530.5, -22.4, 611.0, -10.3), 4.0, 1.0, BLACK, radius=1.5)              # the rack's own plate on the top rail
    rivet = {'kind': 'rivet', 'd': 12.0, 'h': 4.0}
    for x, y in ((21.0, -20.0), (341.0, -16.0), (643.7, -12.0), (1000.0, -18.0), (1250.0, -18.0)):
        M.lathe(b, (x, y), 3.0, [(6.0, 0.0), (5.1, 2.8), (2.4, 4.0), (0.0, 4.0)], STEEL, seg=14)
    for y in range(40, 620, 130):
        M.lathe(b, (-60.0, float(y)), 3.0, [(6.0, 0.0), (5.1, 2.8), (2.4, 4.0), (0.0, 4.0)], STEEL, seg=14)
    for x in range(40, 1280, 170):
        M.lathe(b, (float(x), 606.0), 3.0, [(6.0, 0.0), (5.1, 2.8), (2.4, 4.0), (0.0, 4.0)], STEEL, seg=14)


def build_blocks(b):
    for name, (r, _) in T.BLOCKS.items():
        M.box_mm(b, r, -20.0, -2.0, PLATE)
        M.rr_slab(b, r, 0.0, 2.0, PLATE, radius=10.0)


def base(p):
    return p.get('base', 0.0)


def latch_plate(b, p):
    M.rr_slab(b, p['r'], p['h'], p['h'], CREAM, radius=9.0)


def latch(b, p):
    r = p['d'] / 2
    H = p['h']
    M.lathe(b, p['c'], 1.0, [(r, 0.0), (r, 4.0), (r * 0.85, 6.0), (r * 0.8, H - 1.0), (0.0, H - 1.0)], STEEL)
    M.thin_bar(b, (p['c'][0] - r * 0.25, p['c'][1] - r * 0.7), (p['c'][0] + r * 0.25, p['c'][1] + r * 0.7), 4.0, H, H + 2.5, DARK)


def meter(b, p):
    x0, y0, x1, y1 = p['r']
    H = p['h']
    M.rr_slab(b, p['r'], H, H, BLACK, radius=6.0)
    w, h = x1 - x0, y1 - y0
    # the cream face, a window with a domed top
    fx0, fx1, fy0, fy1 = x0 + w * 0.10, x1 - w * 0.10, y0 + h * 0.10, y0 + h * 0.66
    M.box_mm(b, (fx0, fy0 + (fy1 - fy0) * 0.25, fx1, fy1), H, H + 0.5, CREAM)
    cx = (fx0 + fx1) / 2
    n = 16
    outline = []
    rx, ry = (fx1 - fx0) / 2, (fy1 - fy0) * 0.25
    for k in range(n + 1):
        t = math.pi * k / n
        outline.append(T.to_m(cx + rx * math.cos(t), fy0 + (fy1 - fy0) * 0.25 - ry * math.sin(t)))
    outline += [T.to_m(fx0, fy0 + (fy1 - fy0) * 0.25 + 0.1), T.to_m(fx1, fy0 + (fy1 - fy0) * 0.25 + 0.1)]
    b.slab(tuple(M.at((T.W_MM / 2, T.H_MM / 2), H + 0.5)), U, V, outline, [], 0.5 * MM, CREAM)
    M.thin_bar(b, (cx, fy1 - 2.0), (cx, fy0 + 8.0), 1.2, H + 0.5, H + 1.2, DARK)    # the needle
    M.disc(b, (cx, y0 + h * 0.84), w * 0.075, H, H + 3.0, STEEL, seg=14)            # the zero adjuster


def hood(b, p):
    """The pale half-round shield over a meter (g091-a, g091-c)."""
    x0, y0, x1, y1 = p['r']
    cx = (x0 + x1) / 2
    rx, ry = (x1 - x0) / 2, y1 - y0
    n = 18
    outline = [T.to_m(cx + rx * math.cos(math.pi * k / n), y1 - ry * math.sin(math.pi * k / n)) for k in range(n + 1)]
    b.slab(tuple(M.at((T.W_MM / 2, T.H_MM / 2), p['h'])), U, V, outline, [], p['h'] * MM, CREAM)


def lamp(b, p):
    r = p['d'] / 2
    h = base(p)
    if p.get('ring'):
        M.lathe(b, p['c'], h, [(r * 1.5, 0.0), (r * 1.5, 1.5), (r * 1.1, 2.0), (0.0, 2.0)], CREAM, seg=16)
    M.lathe(b, p['c'], h, [(r, 0.0), (r, 3.0), (r * 0.8, p['h'] * 0.8), (r * 0.45, p['h']), (0.0, p['h'])], LAMP[p['colour']], seg=16)


def dome(b, p):
    r = p['d'] / 2
    M.lathe(b, p['c'], 0.0, [(r, 0.0), (r, p['h'] * 0.45), (r * 0.8, p['h'] * 0.85), (r * 0.4, p['h']), (0.0, p['h'])], RED, seg=20)


def jack(b, p):
    r = p['d'] / 2
    H = p['h']
    h = base(p)
    mat = JACK[p['colour']]
    M.lathe(b, p['c'], h, [(r * 1.1, 0.0), (r * 1.1, 2.0), (r * 0.8, 3.0), (r * 0.8, H - 2.0), (r * 0.4, H), (0.0, H)], mat, seg=18)


def knob(b, p):
    r = p['d'] / 2
    H = p['h']
    cx, cy = p['c']
    M.lathe(b, p['c'], 0.0, [(r * 1.1, 0.0), (r * 1.1, 3.0), (r * 0.8, 4.0), (r * 0.8, H * 0.5), (0.0, H * 0.5)], BLACK, seg=18)
    t = math.radians(p['pointer'])
    dx, dy = math.sin(t) * r * 1.2, -math.cos(t) * r * 1.2
    M.thin_bar(b, (cx - dx, cy - dy), (cx + dx, cy + dy), r * 0.75, H * 0.45, H, BLACK)


def toggle(b, p):
    r = p['d'] / 2
    h = base(p)
    cx, cy = p['c']
    M.lathe(b, p['c'], h, [(r, 0.0), (r, 2.0), (r * 0.55, 3.0), (r * 0.55, 5.0), (0.0, 5.0)], STEEL, seg=14)
    b.cylinder(M.at(p['c'], h + 4.0), M.at((cx, cy - 6.0), h + p['h']), 1.7 * MM, STEEL, seg=8, r1=2.6 * MM)


def strip(b, p):
    x0, y0, x1, y1 = p['r']
    M.rr_slab(b, p['r'], p['h'], p['h'], RED, radius=3.0)
    n = max(4, int((y1 - y0) / 8.0))
    for k in range(n):
        y = y0 + (y1 - y0) * (k + 0.5) / n
        for f in (0.28, 0.72):
            M.disc(b, (x0 + (x1 - x0) * f, y), 1.7, p['h'], p['h'] + 0.4, DARK, seg=8)


def button_ring(b, p):
    r = p['d'] / 2
    M.lathe(b, p['c'], 0.0, [(r, 0.0), (r, 3.0), (r * 0.85, p['h']), (r * 0.62, p['h']), (r * 0.58, 4.0), (0.0, 4.0)], CREAM, seg=24)


def button(b, p):
    r = p['d'] / 2
    M.lathe(b, p['c'], 3.5, [(r, 0.0), (r, p['h'] - 5.0), (r * 0.6, p['h'] - 3.0), (0.0, p['h'] - 3.0)], RED, seg=20)


def tab(b, p):
    x0, y0, x1, y1 = p['r']
    if p.get('colour') == 'outline':
        M.box_mm(b, p['r'], p['h'], p['h'] + 0.3, DARK)
        M.box_mm(b, (x0 + 1.2, y0 + 1.2, x1 - 1.2, y1 - 1.2), p['h'] + 0.3, p['h'] + 0.5, PLATE)
    else:
        M.rr_slab(b, p['r'], p['h'], p['h'], CREAM, radius=2.0)


def cover(b, p):
    x0, y0, x1, y1 = p['r']
    H = p['h']
    M.rr_slab(b, p['r'], H, H, CREAM, radius=14.0)
    for x in (459.5, 549.0):
        b.cylinder(M.at((x - 15.0, y1 + 6.0), 6.0), M.at((x + 15.0, y1 + 6.0), 6.0), 4.0 * MM, STEEL, seg=10)
    for x in (506.8, 520.0):
        for y in (359.0, 368.0):
            M.disc(b, (x, y), 2.0, H, H + 0.4, DARK, seg=8)


def black_panel(b, p):
    M.rr_slab(b, p['r'], p['h'], p['h'], BLACK, radius=9.0)


def clamp(b, p):
    cx, cy = p['c']
    w, ht = p['w'], p['ht']
    M.box_mm(b, (cx - w / 2, cy - ht / 2, cx + w / 2, cy - ht / 2 + 8.0), 0.0, p['h'], STEEL)
    M.box_mm(b, (cx - w / 2, cy + ht / 2 - 8.0, cx + w / 2, cy + ht / 2), 0.0, p['h'], STEEL)
    M.box_mm(b, (cx + w / 2 - 7.0, cy - ht / 2, cx + w / 2, cy + ht / 2), 0.0, p['h'] * 0.7, STEEL)
    if not p.get('game'):       # the game's toggle stands in the bracket in its place
        M.lathe(b, p['c'], 0.0, [(13.0, 0.0), (13.0, 5.0), (10.0, 7.0), (10.0, 16.0), (0.0, 16.0)], STEEL)


def window(b, p):
    M.rr_slab(b, p['r'], p['h'], p['h'], DARK, radius=11.0)


def dial_block(b, p):
    x0, y0, x1, y1 = p['r']
    M.box_mm(b, p['r'], 0.0, p['h'], BLACK)
    M.thin_bar(b, ((x0 + x1) / 2 - 6.0, (y0 + y1) / 2), ((x0 + x1) / 2 + 8.0, (y0 + y1) / 2), 4.0, p['h'], p['h'] + 3.0, CREAM)


def build_dial(b):
    """The ЗК-4 setter's body: a ring with four ears. Its face inside the ring is the game's display (qc_setter), its knob the game's control."""
    c = T.DIAL_C
    R = T.DIAL_R
    H = T.Z['dial_ring']
    M.lathe(b, c, 0.0, [(R, 0.0), (R, H), (R + 5.0, H), (R + 11.0, H * 0.55), (R + 11.0, 0.0)], BLACK, seg=48)
    for k in range(4):
        t = math.radians(45.0 + 90.0 * k)
        q = (c[0] + (R + 16.0) * math.cos(t), c[1] + (R + 16.0) * math.sin(t))
        M.disc(b, q, 9.0, 0.0, T.Z['dial_ear'], BLACK, seg=14)


def nameplate(b, p):
    M.rr_slab(b, p['r'], p['h'], 1.0, BLACK if p.get('colour') == 'black' else DARK, radius=1.0)


def data_plate(b, p):
    x0, y0, x1, y1 = p['r']
    M.rr_slab(b, p['r'], p['h'], p['h'], CREAM, radius=1.5)
    for k in range(1, 4):
        y = y0 + (y1 - y0) * k / 4
        M.thin_bar(b, (x0 + 8.0, y), (x1 - 8.0, y), 0.8, p['h'], p['h'] + 0.2, RACK)
    for x in (x0 + 7.0, x1 - 7.0):
        M.disc(b, (x, (y0 + y1) / 2), 2.5, p['h'], p['h'] + 1.0, STEEL, seg=8)


DRAW = {'screw': M.screw, 'latch_plate': latch_plate, 'latch': latch, 'meter': meter, 'hood': hood, 'lamp': lamp, 'dome': dome, 'jack': jack, 'knob': knob, 'toggle': toggle,
        'strip': strip, 'button_ring': button_ring, 'button': button, 'tab': tab, 'cover': cover, 'black_panel': black_panel, 'clamp': clamp, 'window': window, 'dial_block': dial_block,
        'nameplate': nameplate, 'data_plate': data_plate}


def build(B, finish):
    """SM_SL1MFace with the interior script's Builder `B` and `finish`."""
    import bpy
    b = B('SM_SL1MFace')
    build_surround(b)
    build_blocks(b)
    build_dial(b)
    for p in T.parts():
        if p.get('game') and p['kind'] != 'clamp':
            continue        # the cabin draws it (sl1m_trace: `game`)
        DRAW[p['kind']](b, p)
    obj = finish(b, reuse=True)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    bpy.context.view_layer.update()
    return obj
