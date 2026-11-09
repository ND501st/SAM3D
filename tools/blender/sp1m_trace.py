"""SP-1M's face, traced from the book's drawing g090-b (wwp_gecko p. 90), checked against the photo g090-a. No bpy: sp1m_model.py and tools/tests/test_sp1m_trace.py read this.

1. **The drawing is the layout.** The photo has the radio in front of most of this rack, and only the top row and the right column's right part can be seen; the drawing is square-on and
   whole. Read off two crops at 1.55x: `_dl` (x0 40, y0 20) and `_dr` (x0 784, y0 20), both in the drawing's pixels.
2. **Scale:** K = 0.885 mm a pixel, chosen so the rack's width (left block's left edge to right block's right edge) equals SŁ-1M's 1.275 m, the facing rack of the same pair. That is an
   assumption: Fig. 3 gives both racks 1.31 m. It comes out 552 mm high against SŁ-1M's measured 554, which the drawing did not know about.
3. **The photo over the drawing:** what can be seen agrees part for part, with one difference. The drawing shows a row of hexagon connectors above a thin strip on 0065-2M; the photo shows,
   at that height, a plain band 30 mm tall from the strip up to the legends, with a black shadow line at its foot, and no connectors. Read here as a flap hinged at its foot: open in the
   drawing (the strip is its edge), closed in the photo. Modelled closed (`flap`), as photographed; the connectors under it are not modelled. That reading is SAM3D's.
   The photo also settles colours the drawing cannot: the big lenses of 0093-19 orange, the small lamps over them pale, 0065-2M's caps brown, three of its row's points small grey screws.

Frame: x mm to the right as one faces the rack, y mm down, the bottom-left block's left edge and the top row's top at (0, 0). `Z`: (c) SAM3D's choice, this rack having no two views of
any part. The hexagon the drawing draws round each latch does not show in the photo, so the plate is SŁ-1M's, measured there. Session of the photo: the book's own vehicle (g090-g091).
"""
K = 0.885
PX0, PY0 = 64.0, 64.0


def _dl(tx, ty):
    return ((40.0 + tx / 1.55 - PX0) * K, (20.0 + ty / 1.55 - PY0) * K)


def _dr(tx, ty):
    return ((784.0 + tx / 1.55 - PX0) * K, (20.0 + ty / 1.55 - PY0) * K)


W_MM = 1275.0
H_MM = 552.0

BLOCKS = {
    'L1': ((4.0, 0.0, 611.0, 171.0), '0093-14A'),
    'L2': ((3.0, 187.0, 610.0, 362.0), 'OK82-2M'),
    'L3': ((0.0, 377.0, 611.0, 552.0), '0092-9M'),
    'R1': ((657.0, 0.0, 1272.0, 178.0), '0093-19'),
    'R2': ((657.0, 194.0, 1275.0, 550.0), '0065-2M'),
}
BLOCK_IDS = {'sp1_0093_14a': 'L1', 'sp1_ok82_2m': 'L2', 'sp1_0092_9m': 'L3', 'sp1_0093_19': 'R1', 'sp1_0065_2m': 'R2'}

Z = {           # all (c)
    'screw': 2.0, 'latch': 14.0, 'latch_plate': 1.5, 'knob': 24.0, 'pot': 12.0, 'jack': 10.0, 'cap': 11.0, 'dome': 12.0, 'lamp': 6.0, 'dot': 1.5, 'strip': 5.0, 'flap': 3.0,
    'nameplate': 1.0,
}

FRAME_LEFT, FRAME_RIGHT = (-50.0, -8.0), (1283.0, 1327.0)
FRAME_TOP, FRAME_BOTTOM = (-31.0, -8.0), (560.0, 624.0)
DIVIDER = (622.0, 646.0)
RAILS = (((3.0, 611.0), (171.0, 187.0)), ((3.0, 611.0), (362.0, 377.0)), ((657.0, 1272.0), (178.0, 194.0)))

# the recesses in the frame, each with a bolt (the drawing's hatched pockets)
POCKETS_LEFT = ((47.0, 91.0), (194.0, 238.0), (345.0, 389.0), (500.0, 544.0))
POCKETS_RIGHT = ((42.0, 95.0), (198.0, 242.0), (351.0, 401.0), (500.0, 548.0))
POCKETS_BOTTOM = ((2.0, 42.0), (298.0, 344.0), (604.0, 650.0), (911.0, 960.0), (1220.0, 1271.0))
BRACKETS_TOP = ((30.0, 64.0), (302.0, 333.0), (618.0, 648.0), (911.0, 957.0), (1200.0, 1234.0))
PLATE_TOP = (668.0, -26.0, 748.0, -13.0)        # the dark plate on the top rail

_RIVET_Y_TOP = -22.0
RIVETS = [(x, _RIVET_Y_TOP) for x in [_dl(t, 30)[0] for t in (168, 263, 365, 468, 680, 792, 885, 985)] + [_dr(t, 30)[0] for t in (250, 348, 452, 680, 780, 875, 965)]]
RIVETS += [_dl(t, 1073) for t in (160, 237, 313, 382, 457, 538, 675, 775, 877, 968, 1063)] + [_dl(t, 1140) for t in (153, 247, 344, 449, 545, 680, 779, 874, 968, 1070)]
RIVETS += [_dr(t, 1078) for t in (45, 138, 242, 345, 443, 596, 688, 793, 893, 988)] + [_dr(t, 1146) for t in (48, 140, 243, 345, 445, 589, 688, 790, 890, 985)]
RIVETS += [(-42.0, y) for y in (-9.0, 117.0, 175.0, 268.0, 323.0, 410.0, 469.0, 556.0)]
RIVETS += [(1314.0, y) for y in (28.0, 120.0, 175.0, 272.0, 327.0, 419.0, 475.0, 565.0)] + [(1281.0, -19.0), (1282.0, 616.0)]

PART_LIST = []


def _p(kind, block, c, **kw):
    kw.setdefault('h', Z.get(kind, 0.0))
    PART_LIST.append(dict(kind=kind, block=block, plane='U', c=c, **kw))


def _r(kind, block, a, b, **kw):
    kw.setdefault('h', Z.get(kind, 0.0))
    PART_LIST.append(dict(kind=kind, block=block, plane='U', r=(a[0], a[1], b[0], b[1]), **kw))


def _screws(block, pts, d=6.0):
    for c in pts:
        _p('screw', block, c, d=d)


def _latch(block, c):
    x, y = c
    _r('latch_plate', block, (x - 14.0, y - 21.0), (x + 14.0, y + 21.0))
    _p('latch', block, c, d=22.0)


# ---- 0093-14А: the knob, five trimmers (two rings on a figure of eight), six jacks
_latch('L1', _dl(97, 213))
_latch('L1', _dl(1057, 213))
_p('knob', 'L1', _dl(208, 143), d=25.0)
for x in (507, 573, 640, 752, 818):
    _p('pot', 'L1', _dl(x, 187), d=23.0, top=_dl(x, 158), top_d=17.0)
for x, y in ((511, 267), (576, 266), (641, 267), (752, 266), (818, 267), (885, 267)):
    _p('jack', 'L1', _dl(x, y), d=16.0)
_screws('L1', [_dl(*q) for q in ((184, 102), (383, 102), (577, 102), (775, 100), (969, 101), (183, 323), (377, 326), (575, 323), (771, 325), (968, 325))])
_r('nameplate', 'L1', _dl(980, 72), _dl(1050, 95))
_r('nameplate', 'L1', _dl(1057, 72), _dl(1095, 95))

# ---- ОК82-2М: the two connector strips
_latch('L2', _dl(87, 545))
_latch('L2', _dl(1052, 545))
_r('strip', 'L2', _dl(244, 453), _dl(285, 607))
_r('strip', 'L2', _dl(822, 453), _dl(860, 607))
_p('jack', 'L2', _dl(664, 645), d=13.0)
_screws('L2', [_dl(*q) for q in ((177, 436), (372, 435), (573, 435), (764, 439), (964, 438), (373, 464), (546, 464), (766, 464), (369, 596), (543, 598), (765, 597), (177, 658), (371, 657),
                                 (565, 659), (768, 657), (964, 660))])
_r('nameplate', 'L2', _dl(972, 413), _dl(1052, 432))
_r('nameplate', 'L2', _dl(1060, 413), _dl(1095, 432))

# ---- 0092-9М: three rows
_latch('L3', _dl(90, 880))
_latch('L3', _dl(1060, 883))
for x, y in ((412, 843), (494, 844), (573, 843), (654, 843), (733, 844)):
    _p('cap', 'L3', _dl(x, y), d=18.0, colour='black')
for x, y in ((412, 916), (496, 917), (576, 918), (655, 918), (733, 918)):
    _p('jack', 'L3', _dl(x, y), d=20.0)
for x, y in ((372, 980), (454, 981), (530, 981), (611, 982), (692, 981), (773, 982)):
    _p('jack', 'L3', _dl(x, y), d=16.0)
_screws('L3', [_dl(*q) for q in ((175, 762), (376, 767), (575, 771), (773, 772), (971, 772), (176, 993), (575, 993), (968, 995))])
_r('nameplate', 'L3', _dl(984, 748), _dl(1052, 768))
_r('nameplate', 'L3', _dl(1060, 750), _dl(1095, 768))

# ---- 0093-19: the seven lenses, each with a small lamp and a point; eight jacks
_latch('R1', _dr(87, 220))
_latch('R1', _dr(1057, 220))
for x, lens in zip((363, 418, 473, 526, 617, 669, 721), (365, 417, 471, 524, 613, 665, 721)):
    _p('lamp', 'R1', _dr(x, 236), d=10.0, colour='cream')
    _p('dome', 'R1', _dr(lens, 263), d=19.5)
for x in (342, 396, 450, 502, 590, 644, 698):
    _p('dot', 'R1', _dr(x, 236), d=6.0, colour='dark')
for x, y in ((327, 320), (417, 320), (471, 320), (521, 320), (617, 320), (667, 320), (720, 320), (813, 319)):
    _p('jack', 'R1', _dr(x, y), d=16.0)
_screws('R1', [_dr(*q) for q in ((173, 112), (370, 109), (568, 108), (766, 111), (964, 114), (171, 331), (370, 331), (570, 331), (768, 331), (969, 329))])
_r('nameplate', 'R1', _dr(972, 82), _dr(1042, 102))
_r('nameplate', 'R1', _dr(1052, 82), _dr(1090, 102))

# ---- 0065-2М: the two closed flaps, the knob, the two rows
_latch('R2', _dr(92, 718))
_latch('R2', _dr(1067, 720))
_r('flap', 'R2', _dr(152, 618), _dr(465, 671))
_r('flap', 'R2', _dr(694, 618), _dr(1018, 671))
_p('cap', 'R2', _dr(505, 690), d=18.0, colour='black')
for x in (199, 265, 333, 401, 469, 537, 671, 960):
    _p('cap', 'R2', _dr(x, 745), d=11.0, colour='brown')
for x in (702, 806, 909):
    _p('dot', 'R2', _dr(x, 745), d=7.0, colour='grey')
for x in (197, 265, 332, 401, 469, 537, 603, 671, 738, 806, 890, 959):
    _p('cap', 'R2', _dr(x, 795), d=11.0, colour='brown')
_screws('R2', [_dr(*q) for q in ((178, 451), (377, 450), (577, 452), (777, 451), (977, 453), (140, 500), (1015, 505), (141, 716), (1015, 720), (135, 935), (1015, 938), (177, 983), (377, 986),
                                 (578, 988), (778, 985), (978, 988), (806, 690))])
_r('nameplate', 'R2', _dr(988, 425), _dr(1052, 445))
_r('nameplate', 'R2', _dr(1060, 425), _dr(1095, 445))


def parts(kind=None, block=None):
    return [p for p in PART_LIST if (kind is None or p['kind'] == kind) and (block is None or p['block'] == block)]


def to_m(x, y):
    """Face mm to metres on the cabin panel: a to the right, b up, from the panel's centre."""
    return ((x - W_MM / 2.0) * 0.001, (H_MM / 2.0 - y) * 0.001)


def rect_m(r):
    a, b = to_m((r[0] + r[2]) / 2.0, (r[1] + r[3]) / 2.0)
    return (a, b), ((r[2] - r[0]) * 0.001, (r[3] - r[1]) * 0.001)
