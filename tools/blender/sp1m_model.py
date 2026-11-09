"""SP-1M's face as a 3D model, from its trace (tools/blender/sp1m_trace.py). Run inside Blender through tools/blender/build_sp1m.py; the carcass is build_cabin_interior's SM_RackSP1M.

Frame and helpers as SŁ-1M's (sl1m_model.py, sl3m_model.py), pointed at this rack's trace: one rack per Blender run. No control of this rack is worked in the game, so the mesh has every part.
The plates' legends and the drawing's warning triangles are not drawn (decals, roadmap I10).
"""
import math

import sl1m_model as S
import sl3m_model as M
import sp1m_trace as T

M.T = T          # the helpers (at, box_mm, rr_slab ...) read the trace through this
CREAM, STEEL, DARK, BLACK, RED, PLATE, RACK = M.CREAM, M.STEEL, M.DARK, M.BLACK, M.RED, M.PLATE, M.RACK
BROWN = 'Paint_Red'
MM = M.MM


def bolt(b, c):
    M.lathe(b, c, -8.0, [(6.5, 0.0), (6.5, 3.0), (5.0, 4.0), (0.0, 4.0)], BLACK, seg=14)


def column(b, x0, x1, y0, y1, pockets):
    """A frame post with its recesses: the post in pieces, each recess a floor 8 mm below the post's back and a bolt."""
    y = y0
    for a, c in pockets:
        M.box_mm(b, (x0, y, x1, a), -20.0, 3.0, PLATE)
        M.box_mm(b, (x0, a, x1, c), -20.0, -8.0, RACK)
        bolt(b, ((x0 + x1) / 2, a + (c - a) * 0.62))
        y = c
    M.box_mm(b, (x0, y, x1, y1), -20.0, 3.0, PLATE)


def rail(b, x0, x1, y0, y1, pockets):
    x = x0
    for a, c in pockets:
        M.box_mm(b, (x, y0, a, y1), -20.0, 3.0, PLATE)
        M.box_mm(b, (a, y0, c, y1), -20.0, -8.0, RACK)
        bolt(b, ((a + c) / 2, y0 + (y1 - y0) * 0.72))
        x = c
    M.box_mm(b, (x, y0, x1, y1), -20.0, 3.0, PLATE)


def build_surround(b):
    xl0, xl1 = T.FRAME_LEFT
    xr0, xr1 = T.FRAME_RIGHT
    yt0, yt1 = T.FRAME_TOP
    yb0, yb1 = T.FRAME_BOTTOM
    M.box_mm(b, (xl0, yt0, xr1, yb1), -24.0, -20.0, DARK)
    column(b, xl0, xl1, yt0, yb1, T.POCKETS_LEFT)
    column(b, xr0, xr1, yt0, yb1, T.POCKETS_RIGHT)
    M.box_mm(b, (xl1, yt0, xr0, yt1), -20.0, 3.0, PLATE)
    rail(b, xl1, xr0, yb0, yb1, T.POCKETS_BOTTOM)
    M.box_mm(b, (T.DIVIDER[0], yt1, T.DIVIDER[1], yb0), -20.0, 4.0, PLATE)
    M.box_mm(b, (T.DIVIDER[0] + 8.0, yt1, T.DIVIDER[1] - 8.0, yb0), 4.0, 4.6, RACK)     # the channel's slot
    for (x0, x1), (y0, y1) in T.RAILS:
        M.box_mm(b, (x0, y0 + 4.0, x1, y1 - 4.0), -20.0, 1.0, PLATE)                    # the steel strip between two blocks, a shadow line each side
    M.rr_slab(b, T.PLATE_TOP, 4.0, 1.0, BLACK, radius=1.5)
    for x0, x1 in T.BRACKETS_TOP:
        M.box_mm(b, (x0, T.FRAME_TOP[0] + 3.0, x1, T.FRAME_TOP[1] - 2.0), 3.0, 7.0, RACK)
        M.lathe(b, ((x0 + x1) / 2, (T.FRAME_TOP[0] + T.FRAME_TOP[1]) / 2 + 0.5), 7.0, [(5.0, 0.0), (5.0, 2.0), (3.5, 3.0), (0.0, 3.0)], BLACK, seg=12)
    for c in T.RIVETS:
        M.lathe(b, c, 3.0, [(6.0, 0.0), (5.1, 2.8), (2.4, 4.0), (0.0, 4.0)], STEEL, seg=14)


def build_blocks(b):
    for name, (r, _) in T.BLOCKS.items():
        M.box_mm(b, r, -20.0, -2.0, PLATE)
        M.rr_slab(b, r, 0.0, 2.0, PLATE, radius=10.0)


def knob(b, p):
    """The black barrel of 0093-14А (g090-a)."""
    r = p['d'] / 2
    H = p['h']
    M.lathe(b, p['c'], 0.0, [(r, 0.0), (r, 2.0), (r * 0.92, 3.0), (r * 0.92, H - 3.0), (r * 0.75, H), (0.0, H)], BLACK, seg=24)


def pot(b, p):
    """A trimmer: a black collar and a steel screw cap above it, on a figure-of-eight plate."""
    r, q = p['d'] / 2, p['top_d'] / 2
    c, t = p['c'], p['top']
    M.disc(b, c, r + 2.5, 0.0, 1.0, CREAM, seg=20)
    M.disc(b, t, q + 2.5, 0.0, 1.0, CREAM, seg=18)
    M.box_mm(b, (c[0] - q, t[1], c[0] + q, c[1]), 0.0, 1.0, CREAM)
    M.lathe(b, c, 1.0, [(r, 0.0), (r, 5.0), (r * 0.55, 6.0), (r * 0.55, 3.0), (0.0, 3.0)], BLACK, seg=22)
    M.lathe(b, t, 1.0, [(q, 0.0), (q, 2.0), (q * 0.75, 3.0), (q * 0.75, p['h'] - 1.0), (q * 0.55, p['h'] - 1.0), (0.0, p['h'] - 1.0)], STEEL, seg=18)
    M.thin_bar(b, (t[0] - q * 0.6, t[1]), (t[0] + q * 0.6, t[1]), 1.5, p['h'] - 1.0, p['h'] - 0.6, DARK)


def jack(b, p):
    """A black collar with a light centre and a dark hole (g090-a)."""
    r = p['d'] / 2
    H = p['h']
    M.lathe(b, p['c'], 0.0, [(r, 0.0), (r, H * 0.7), (r * 0.85, H), (r * 0.62, H), (r * 0.62, H - 1.5), (0.0, H - 1.5)], BLACK, seg=22)
    M.disc(b, p['c'], r * 0.58, H - 1.5, H - 1.0, STEEL, seg=18)
    M.disc(b, p['c'], r * 0.2, H - 1.0, H - 0.8, DARK, seg=10)


def cap(b, p):
    r = p['d'] / 2
    H = p['h']
    mat = BLACK if p['colour'] == 'black' else BROWN
    M.lathe(b, p['c'], 0.0, [(r, 0.0), (r, H * 0.45), (r * 0.82, H * 0.7), (r * 0.82, H), (0.0, H)], mat, seg=18)
    M.disc(b, p['c'], r * 0.35, H, H + 0.3, DARK, seg=10)


def lamp(b, p):
    r = p['d'] / 2
    H = p['h']
    M.lathe(b, p['c'], 0.0, [(r, 0.0), (r, 1.5), (r * 0.8, H * 0.7), (r * 0.4, H), (0.0, H)], CREAM, seg=14)


def dot(b, p):
    M.lathe(b, p['c'], 0.0, [(p['d'] / 2, 0.0), (p['d'] / 2 * 0.9, p['h']), (0.0, p['h'])], DARK if p['colour'] == 'dark' else RACK, seg=12)


def flap(b, p):
    """The closed flap over a row of connectors: a plain plate and the shadow line under its hinge."""
    x0, y0, x1, y1 = p['r']
    M.rr_slab(b, p['r'], p['h'], p['h'], PLATE, radius=1.5)
    M.box_mm(b, (x0, y1, x1, y1 + 2.0), 0.0, 0.5, DARK)
    b.cylinder(M.at((x0 + 6.0, y1 - 1.5), p['h'] * 0.5), M.at((x1 - 6.0, y1 - 1.5), p['h'] * 0.5), 2.2 * MM, STEEL, seg=8)


DRAW = {'screw': M.screw, 'latch_plate': S.latch_plate, 'latch': S.latch, 'knob': knob, 'pot': pot, 'jack': jack, 'cap': cap, 'dome': S.dome, 'lamp': lamp, 'dot': dot, 'strip': S.strip,
        'flap': flap, 'nameplate': S.nameplate}


def build(B, finish):
    """SM_SP1MFace with the interior script's Builder `B` and `finish`."""
    import bpy
    b = B('SM_SP1MFace')
    build_surround(b)
    build_blocks(b)
    for p in T.parts():
        DRAW[p['kind']](b, p)
    obj = finish(b, reuse=True)
    mod = obj.modifiers.new('hard_edges', 'EDGE_SPLIT')
    mod.split_angle = math.radians(30)
    mod.use_edge_angle = True
    mod.use_edge_sharp = False
    bpy.context.view_layer.update()
    return obj
