"""SŁ-3M's face, traced from the book's drawing and photos (wwp_gecko p. 99: the square-on drawing g099-c for the layout, g099-a and b
through the joint solve for the size and each block's place). No bpy: sl3m_model.py and tools/tests/test_sl3m_trace.py read this.

1. **The drawing** g099-c shows the whole face square-on: six blocks, every jack, fuse holder, lock and screw (`PARTS`, in the drawing's px).
2. **The photos give the size.** g099-b (1007 points) and g099-a (374) are flattened through their solved cameras onto the plane the solve's
   points give (234 of 261 within 15 mm, 6.5 mm rms). The blocks are 600 x 175 mm in two columns 34 mm apart. The cabin had 650 x 206 from Fig. 3.
3. **The drawing is fitted to the photos,** block by block: x scale 0.722 mm/px for the whole face, y scale 0.72 mm/px, each block's offset
   from its rows of low jacks read on g099-b (the low parts have almost no parallax). Across 9 checked parts the fit is within about 10 mm.
4. **Parallax measures heights.** Two cameras see a raised part at different places: the connector plate's right edge reads x 320 on b and
   238 on a, which only fits 60 mm proud at x 462; the door's free edge likewise stands about 60 mm out, so the dark panel of fuses
   (ПР1...ПР11) in the photos is **the drawing's small door, open**: its face turned toward the cameras, 148 mm tall like the drawing's hatch, spread 225 mm wide
   on the flattened plane by the oblique view. The top-left black jack stands 32 mm proud; the locks about 28 (their length on g099-a).

Frame: x mm to the right as the operator faces the rack (aft end at 0), y mm down, the top-left block's top-left corner at (0, 0). `Z`: (m) measured, (c) SAM3D's choice.

The photos are the book's session A vehicle (g097-g100: a 9A33BM2 or BM3, not identified).
"""
W_MM = 1234.0      # (m) two 600 mm columns and the 34 mm divider, the solve's scale
H_MM = 551.0       # (m) the three rows

ALPHA_X, ALPHA_Y, BETA_X = 0.722, 0.72, -77.0   # (m) the drawing's px to face mm, fitted on g099-b

BLOCKS = {      # (x0, y0, x1, y1) face mm (m, g099-a, b), its y offset from the drawing (m), the label
    'A1': ((0.0, 0.0, 600.0, 175.0), -33.0, '0093-28'),
    'A2': ((0.0, 192.0, 600.0, 363.0), -40.6, '0092-10M'),
    'A3': ((0.0, 387.0, 600.0, 551.0), -48.4, '0096-10M'),
    'B1': ((634.0, 0.0, 1234.0, 175.0), -35.4, '0092-8'),
    'B2': ((634.0, 184.0, 1234.0, 365.0), -47.2, 'OS94-10'),
    'B3': ((634.0, 384.0, 1234.0, 549.0), -51.2, '0092-4M'),
}
BLOCK_IDS = {'p_0093_28': 'A1', 'p_0092_10m': 'A2', 'p_0096_10m': 'A3', 'p_0092_8': 'B1', 'p_os94_10': 'B2', 'p_0092_4m': 'B3'}

Z = {
    'screw': 2.0,          # (c)
    'lamp': 8.0,           # (c)
    'cap': 12.0,           # (c) the row of red caps
    'jack': 8.0,           # (c)
    'jack_black': 32.0,    # (m) the black jack: its tip's shift on g099-b
    'lock': 28.0,          # (m) about: their length on g099-a at its obliquity
    'lock_plate': 2.0,     # (c)
    'fuse_base': 14.0,     # (c)
    'fuse_cap': 36.0,      # (c)
    'nameplate': 1.0,      # (c)
    'conn_plate': 60.0,    # (m) parallax between g099-a and b
    'plug': 110.0,         # (c) the top of each elbow's ribbed coupling; the elbow turns down in front of it (g099-a, b: olive, a taped collar, silver braid)
    'door': 78.0,          # (m) its width, set by the drawing; open about 95 degrees: its free edge 60-78 mm proud on the photos
}

D_LOWER = 0.0          # all on one plane (SP-3M's model code reads this)
FRAME_LEFT, FRAME_RIGHT = (-45.0, -8.0), (1242.0, 1290.0)
FRAME_TOP, FRAME_BOTTOM = (-20.0, -8.0), (558.0, 618.0)   # the top strip cut at the cabin's roof line (2.10 m)
DIVIDER = (604.0, 630.0)
POCKET_X = (40.0, 340.0, 660.0, 975.0, 1130.0)   # the bottom strip's five pockets, from g099-c
POCKET_W = 55.0
TAB_X = (70.0, 240.0, 410.0, 830.0, 1005.0, 1175.0)   # the top strip's tabs, from g099-c


def fx(px):
    return ALPHA_X * px + BETA_X


def fy(block, py):
    return ALPHA_Y * py + BLOCKS[block][1]


PART_LIST = []


def _p(kind, block, px, py, **kw):
    kw.setdefault('h', Z.get(kind, 0.0))
    PART_LIST.append(dict(kind=kind, block=block, plane='U', c=(fx(px), fy(block, py)), **kw))


def _plate(block, cx, cy, w_px=62.0, h_px=18.0, small=False):
    w, h = w_px * ALPHA_X, h_px * ALPHA_Y
    x, y = fx(cx), fy(block, cy)
    PART_LIST.append(dict(kind='nameplate', block=block, plane='U', r=(x - w / 2, y - h / 2, x + w / 2, y + h / 2), h=Z['nameplate'], colour='dark'))


def _fuse(block, px, py):
    _p('fuse_base', block, px, py, w=30.0, ht=42.0)
    x, y = fx(px), fy(block, py)
    PART_LIST.append(dict(kind='fuse_cap', block=block, plane='U', c=(x, y + 7.0), d=24.0, h=Z['fuse_cap']))
    PART_LIST.append(dict(kind='lamp', block=block, plane='U', c=(x, y - 11.0), d=12.0, h=16.0, colour='white', on_fuse=True))


def _lock(block, px, py, **kw):
    x, y = fx(px), fy(block, py)
    PART_LIST.append(dict(kind='lock_plate', block=block, plane='U', r=(x - 27.0, y - 17.0, x + 27.0, y + 17.0), h=Z['lock_plate']))
    PART_LIST.append(dict(kind='lock', block=block, plane='U', c=(x, y), d=26.0, h=Z['lock'], **kw))


# ---- 0093-28
_p('jack_black', 'A1', 230, 117, d=28.0)
_p('screw', 'A1', 230, 74, d=6.0)
for x in (225, 551):
    _p('screw', 'A1', x, 260, d=6.0)
for x in (434, 493, 551, 612, 671, 732, 792):
    _p('lamp', 'A1', x, 97, d=9.0, colour='white')
for x in (404, 464, 522, 580, 638, 700, 760):
    _p('screw', 'A1', x, 97, d=5.0)
for x in (432, 493, 552, 612, 671, 731, 792):
    _p('cap', 'A1', x, 121, d=19.0)
for x in (404, 463, 520, 580, 640, 700, 762):
    _p('jack', 'A1', x, 157, d=15.0)
for x in (460, 522, 580, 640, 700):
    _p('jack', 'A1', x, 200, d=15.0)
for px, py in ((886, 130), (843, 192), (885, 192)):
    _fuse('A1', px, py)
_lock('A1', 345, 248)
_lock('A1', 755, 245)
_plate('A1', 873, 60)
_plate('A1', 926, 60, 30, 16)

# ---- 0092-10M
_p('jack_black', 'A2', 219, 403, d=28.0)
for x, y in ((226, 347), (386, 345), (552, 345), (718, 348), (886, 352), (222, 532), (553, 535), (883, 537)):
    _p('screw', 'A2', x, y, d=6.0)
for px in (372, 534, 583, 744):
    _fuse('A2', px, 428)
for x in (263, 438, 522, 605, 670):
    _p('jack', 'A2', x, 523, d=15.0)
_p('jack', 'A2', 845, 527, d=15.0)
_lock('A2', 372, 523)
_lock('A2', 730, 523, wires=True)     # the red seal wire (g099-a)
_plate('A2', 798, 333, 75, 18)
_plate('A2', 853, 334, 30, 16)

# ---- 0096-10M: the plate of three big connectors, its small door and two locks turned upright
_lock('A3', 194, 712)
_lock('A3', 905, 713)
PART_LIST.append(dict(kind='conn_plate', block='A3', plane='U', r=(fx(262), fy('A3', 640), fx(718), fy('A3', 790)), h=Z['conn_plate']))
for px, r in ((335, 40.0), (485, 41.0), (632, 40.0)):
    PART_LIST.append(dict(kind='plug', block='A3', plane='U', c=(fx(px), fy('A3', 715)), d=2 * r, h=Z['plug'], base=Z['conn_plate']))
for x, y in ((225, 623), (386, 623), (884, 623), (220, 810), (384, 805), (714, 810), (882, 810)):
    _p('screw', 'A3', x, y, d=6.0)
_plate('A3', 865, 607, 67, 16)
_plate('A3', 918, 607, 30, 14)
for x, y in ((285, 665), (387, 663), (438, 663), (538, 662), (585, 662), (690, 662), (285, 767), (387, 768), (434, 770), (536, 770), (585, 770), (686, 770)):
    _p('screw', 'A3', x, y, d=5.0, base=Z['conn_plate'])      # the connector plate's own screws
DOOR_HINGE = (fx(828), max(fy('A3', 600), BLOCKS['A3'][0][1] + 2.0), fy('A3', 805))    # (x, y0, y1) the door's hinge line
FUSE_ROWS = 5                                            # the fuses on its face, two columns (g099-a)

# ---- 0092-8
_p('jack_black', 'B1', 1104, 110, d=28.0)
for x, y in ((1083, 76), (1252, 76), (1417, 76), (1584, 80), (1085, 262), (1410, 263), (1747, 266), (1417, 197)):
    _p('screw', 'B1', x, y, d=6.0)
for px in (1318, 1420, 1523, 1625):
    _fuse('B1', px, 138)
for x, y in ((1261, 188), (1369, 188), (1471, 190), (1575, 190)):
    _p('jack', 'B1', x, y, d=15.0)
_lock('B1', 1210, 250)
_lock('B1', 1625, 253)
_plate('B1', 1707, 65, 66, 20)
_plate('B1', 1775, 66, 30, 18)

# ---- OS94-10
_p('jack_black', 'B2', 1112, 388, d=28.0)
for x, y in ((1083, 350), (1250, 350), (1415, 347), (1585, 352), (1749, 354), (1083, 532), (1417, 537), (1749, 540)):
    _p('screw', 'B2', x, y, d=6.0)
for px in (1405, 1473, 1544, 1622, 1705):
    _fuse('B2', px, 415)
for x in (1121, 1285, 1334, 1384, 1455, 1518, 1716):
    _p('jack', 'B2', x, 528, d=15.0)
_lock('B2', 1208, 522)
_lock('B2', 1620, 525)
_plate('B2', 1733, 338, 62, 16)
_plate('B2', 1786, 339, 30, 15)

# ---- 0092-4M
_p('jack_black', 'B3', 1085, 687, d=28.0)
for x, y in ((1082, 628), (1250, 632), (1415, 630), (1581, 632), (1747, 633), (1080, 813), (1410, 812), (1744, 816)):
    _p('screw', 'B3', x, y, d=6.0)
for px in (1232, 1388, 1552, 1600):
    _fuse('B3', px, 708)
for x in (1122, 1298, 1382, 1467, 1532):
    _p('jack', 'B3', x, 803, d=15.0)
_lock('B3', 1233, 803)
_lock('B3', 1595, 805)
_plate('B3', 1660, 620, 72, 18)
_plate('B3', 1720, 620, 30, 16)


# ---- the rivets down each right-hand block's left margin (g099-a; hidden or absent on the left column)
for name in ('B1', 'B2', 'B3'):
    r = BLOCKS[name][0]
    for k in range(5):
        PART_LIST.append(dict(kind='rivet', block=name, plane='U', c=(r[0] + 27.0, r[1] + (r[3] - r[1]) * (k + 0.5) / 5.0), d=12.0, h=4.0))


def parts(kind=None, block=None):
    return [p for p in PART_LIST if (kind is None or p['kind'] == kind) and (block is None or p['block'] == block)]


def to_m(x, y):
    """Face mm to metres on the cabin panel: a to the right, b up, from the panel's centre."""
    return ((x - W_MM / 2.0) * 0.001, (H_MM / 2.0 - y) * 0.001)


def rect_m(r):
    a, b = to_m((r[0] + r[2]) / 2.0, (r[1] + r[3]) / 2.0)
    return (a, b), ((r[2] - r[0]) * 0.001, (r[3] - r[1]) * 0.001)
