"""SP-3M-2's face, traced from the book's photos (wwp_gecko p. 98: g098-b, g098-c; the drawing g098-d for what the cover and
the cables hide). No bpy: the model (sp3m_model.py) and the tests (tools/tests/test_sp3m_trace.py) both read this.

The method, after docs/MODELLING_FROM_PHOTOS.md, with the joint solve in place of a one-photo rectification:

1. **Flatten through the solved cameras.** g098-a, b and c are all in the interior's joint solve (tools/recon/holistic_sfm.py:
   g098-b 951 points, g098-c 414). The solve's points on the white panel and the drawer give the face plane (137 of 154
   within 12 mm, 6 mm rms); the points on the instrument blocks give **a second plane 147.6 mm behind it** (190 of 240, 10.5 mm).
   Each photo, halftone notched out (`interior.descreen_notch`), is resampled onto each plane at 3 px/mm through its solved camera,
   radial distortion included. The two photos agree to about 7 mm before correction; half that difference (2.7, 2.6 mm) is taken out of
   each. Scale therefore comes from SP-2M2's face (1.89 m) through the solve, about 3 % (see the docs).
2. **The face's frame.** The flattened frame is rolled 2.0 degrees against the plate's own edges (both photos agree on it), so the
   reads are turned by `ROLL_DEG` and shifted to the plate's top-left corner.
3. **Parallax.** A part standing h mm proud is seen displaced from its foot, away from the point under the camera. Each read
   of a part's top is walked back along that line by its height (`read`). The 70 mm knobs on the right block land within 5 mm of the
   drawing g098-d's places after this; before it, 90 mm off.
4. Parts are read off the 3 px/mm tiles block by block, each with the photo it came from.

Frame: x mm to the right as the operator faces the rack, y mm down, the white panel's top-left corner at (0, 0). Depth: `plane`
'U' is the face plane (white panel, drawer) and 'L' the instrument blocks' plane, `D_LOWER` behind it. `Z` heights are out
of the part's own plane, (m) measured by parallax between g098-b and g098-c or by the solve, (c) SAM3D's choice.

The photos are the book's session A vehicle (g097-g100: a 9A33BM2 or BM3, not identified), not the hangar BM3-1 of g082-g096.
"""
import math

W_MM = 1240.0      # the panel's width: the plate's left edge to the drawer's frame (m, the solve)
H_MM = 570.0       # its height: the plate's top to the blocks' foot (m, the solve)
D_LOWER = 147.6    # (m) the instrument blocks' plane behind the face plane (10 mm rms across 190 points)

ROLL_DEG = 2.0     # (m) the flattened frame's roll against the plate's edges
ORIGIN = (272.0, 76.0)   # the plate's top-left corner in the rolled reading frame
SHIFT = {'b': (2.7, -2.6), 'c': (-2.7, 2.6)}   # half the two photos' difference (b minus c at the plate's corner and the chart)

# Under the camera on the flattened window (mm) and its distance to each plane (mm): the solved eyes.
FOOT = {'b': (-264.0, 502.0), 'c': (114.0, 198.0)}
DIST = {('b', 'U'): 826.0, ('b', 'L'): 966.0, ('c', 'U'): 776.0, ('c', 'L'): 920.0}

Z = {
    'plate': 2.0,          # (c) the white panel's thickness on the frame
    'screw': 2.0,          # (c)
    'lamp': 8.0,           # (c)
    'lamp_large': 10.0,    # (c)
    'oval': 3.0,           # (c)
    'jack': 8.0,           # (c)
    'lock': 16.0,          # (c)
    'fuse_base': 14.0,     # (c) the orange holder
    'fuse_cap': 36.0,      # (c) the cream cap's top
    'fuse_small': 16.0,    # (c)
    'cover': 5.0,          # (c) the fuse cover
    'hinge': 7.0,          # (c)
    'seal': 8.0,           # (c)
    'nameplate': 1.0,      # (c)
    'handle': 30.0,        # (m) the drawer's and blocks' handles: their shift between the two photos, 24-34 mm
    'stud': 12.0,          # (c) the threaded posts along the blocks' tops
    'hex': 14.0,           # (c)
    'knob_black': 22.0,    # (c)
    'knob_fluted': 26.0,   # (c)
    'knob_cream': 40.0,    # (c)
    'knob_ribbed': 70.0,   # (m) the two big knobs on the right block: their shift against the drawing's places
    'chart': 3.0,          # (c)
    'data_plate': 1.5,     # (c)
    'strip': 8.0,          # (c) the orange terminal strips
    'conn_d': 8.0,         # (c)
    'meter': 8.0,          # (c)
    'scale': 5.0,          # (c)
    'coax': 45.0,          # (c) a coaxial plug's top
    'coax_flange': 40.0,   # (c)
    'socket': 6.0,         # (c)
    'elbow': 40.0,         # (c)
    'selector': 18.0,      # (c)
    'cube': 60.0,          # (c) the grey box on the right block
    'latch': 4.0,          # (c)
}

ROLL = math.radians(ROLL_DEG)


def rolled(x, y):
    """A flattened-window point (mm) in the face's frame, without parallax."""
    c, s = math.cos(ROLL), math.sin(ROLL)
    return (x * c + y * s - ORIGIN[0], -x * s + y * c - ORIGIN[1])


def read(photo, plane, x, y, h=0.0):
    """A part's top read at (x, y) on `photo`'s flattened `plane`, h mm proud of it: its foot in the face's frame."""
    fx, fy = FOOT[photo]
    D = DIST[(photo, plane)]
    k = (D - h) / D
    sx, sy = SHIFT[photo]
    return rolled(fx + (x - fx) * k + sx, fy + (y - fy) * k + sy)


def _rect(photo, plane, x0, y0, x1, y1, h=0.0):
    """A rectangle read at its corners: its upright bounds in the face's frame."""
    xm, ym = (x0 + x1) / 2.0, (y0 + y1) / 2.0        # each side at its middle, so the roll does not widen the bounds
    return (read(photo, plane, x0, ym, h)[0], read(photo, plane, xm, y0, h)[1],
            read(photo, plane, x1, ym, h)[0], read(photo, plane, xm, y1, h)[1])


# The blocks (x0, y0, x1, y1), mm in the face's frame, on their planes. Labels as the cabin's Fig. 3 table has them, left to right;
# which block is which is that order's assumption (the book does not name them on the photo).
PLATE = (0.0, 0.0, 574.0, 180.0)     # (m) corners of g098-b, c agree to 5 mm
COVER = (630.0, 0.0, 1210.0, 190.0)  # (m) the drawer's cover; its top and left from g098-b, its bottom from g098-c
FLAP = (-4.0, 365.0, 1190.0, 502.0)  # (m) the closed front flap, g098-b on the face plane; its right end is 20 mm uncertain
OPENING = (-5.0, 205.0, 1205.0, 560.0)   # (c) the recess's mouth, set by the plate, the flap and the blocks' foot
BLOCKS = {
    'P':  (PLATE, 'U', '0094-11M'),
    'D':  (COVER, 'U', '12RZM, 6RZM-2'),
    'L1': (_rect('c', 'L', 298, 323, 578, 640), 'L', '7RZM'),
    'L2': (_rect('c', 'L', 595, 330, 775, 650), 'L', '3RZM'),
    'L3': (_rect('c', 'L', 787, 340, 1040, 650), 'L', '2RZM-2'),
    'L4': (_rect('c', 'L', 1042, 345, 1392, 640), 'L', '4RZM-2'),
}
# The cabin's block ids, each with its bounds in the face's frame (the drawer is the two units behind one cover).
BLOCK_IDS = {
    'sp3_0094_11m': PLATE,
    'sp3_12rzm': (COVER[0], COVER[1], (COVER[0] + COVER[2]) / 2 - 0.5, COVER[3]),
    'sp3_6rzm_2': ((COVER[0] + COVER[2]) / 2 + 0.5, COVER[1], COVER[2], COVER[3]),
    'sp3_7rzm': BLOCKS['L1'][0], 'sp3_3rzm': BLOCKS['L2'][0], 'sp3_2rzm_2': BLOCKS['L3'][0], 'sp3_4rzm_2': BLOCKS['L4'][0],
}

# The rack's surround on the face plane (m unless marked). Tabs along the top (g098-d's pitch; g098-b's four on the right agree within 20 mm).
TAB_X = (48.0, 204.0, 360.0, 518.0, 670.0, 835.0, 1000.0, 1160.0)
TAB_W, TAB_H = 120.0, 15.0
CHANNEL_LEFT = (-86.0, -6.0)   # the channel upright at the rack's left end, g098-b
CHANNEL_RIGHT = (1215.0, 1265.0)   # the flange at its right end, g098-b
GUSSET_Y = (25.0, 80.0, 190.0, 230.0, 380.0, 485.0)   # the channel's sloped partitions, g098-b
TRAY = (600.0, 192.0, 1205.0, 261.0)   # under the drawer: its carrier's front lip (g098-b, c)
TRAY_DEPTH = 110.0     # (c) how far it runs back
RIB_PITCH, RIB_Y = 104.0, (565.0, 620.0)   # the ribbed shelf under the flap, g098-b (triangular gussets)
RIB_X = (-10.0, 1190.0)
WINDOW = (636.0, 120.0, 1190.0, 174.0)      # (m) the cover's window, g098-b, c
PART_LIST = []


def _p(kind, block, plane, photo, x, y, **kw):
    h = kw.pop('h', Z[kind] if kind in Z else 0.0)
    c = read(photo, plane, x, y, h)
    d = dict(kind=kind, block=block, plane=plane, photo=photo, c=c, h=h)
    d.update(kw)
    PART_LIST.append(d)


def _r(kind, block, plane, photo, x0, y0, x1, y1, **kw):
    h = kw.pop('h', Z[kind] if kind in Z else 0.0)
    r = _rect(photo, plane, x0, y0, x1, y1, h)
    d = dict(kind=kind, block=block, plane=plane, photo=photo, r=r, h=h)
    d.update(kw)
    PART_LIST.append(d)


def _handle(block, plane, photo, x, y0, y1):
    h = Z['handle']
    a = read(photo, plane, x, y0, h)
    b = read(photo, plane, x, y1, h)
    PART_LIST.append(dict(kind='handle', block=block, plane=plane, photo=photo, a=a, b=b, h=h))


# ---- the white panel 0094-11M (face plane)
for x, y in ((322.6, 110.2), (441.9, 115.0), (683.9, 123.7), (804.4, 127.7), (314.5, 240.5), (791.2, 258.4), (550.7, 250.0)):
    _p('screw', 'P', 'U', 'b', x, y, d=6.0)
_p('lamp', 'P', 'U', 'b', 375.1, 145.3, d=9.0, colour='white')
_p('lamp', 'P', 'U', 'b', 423.0, 146.4, d=9.0, colour='white')
_r('oval', 'P', 'U', 'b', 384.5, 138.5, 413.5, 153.5)                # the red plate between them, two holes
_p('lamp_large', 'P', 'U', 'b', 465.7, 148.6, d=18.0, colour='cream')
for x, y in ((339.3, 187.0), (376.0, 188.3), (414.7, 190.3), (452.0, 191.3)):
    _p('jack', 'P', 'U', 'c', x, y, d=17.0)
_p('lock', 'P', 'U', 'c', 414.3, 235.3, d=29.0)
_p('lock', 'P', 'U', 'c', 711.7, 245.0, d=27.0, wires=True)
# fuse cover (corners on g098-b): rounded top, two hinge lugs at its foot
_r('cover', 'P', 'U', 'b', 495.0, 100.0, 636.0, 241.0)
_p('seal', 'P', 'U', 'b', 585.4, 108.4, d=12.0)
_p('hinge', 'P', 'U', 'c', 513.3, 246.7, w=26.0, ht=18.0)
_p('hinge', 'P', 'U', 'c', 608.3, 250.7, w=26.0, ht=18.0)
for bx, by, cx, cy, lx, ly in ((691.7, 188.3, 703.3, 198.3, 699.3, 177.3), (773.3, 191.7, 787.7, 200.0, 782.7, 180.0)):
    _p('fuse_base', 'P', 'U', 'c', bx, by, w=40.0, ht=43.0)
    _p('fuse_cap', 'P', 'U', 'c', cx, cy, d=26.0)
    _p('lamp', 'P', 'U', 'c', lx, ly, d=13.0, h=16.0, colour='white', on_fuse=True)
_p('fuse_small', 'P', 'U', 'c', 738.3, 154.0, d=24.0)
_r('nameplate', 'P', 'U', 'b', 752.0, 109.0, 799.0, 122.0, colour='dark')
_r('nameplate', 'P', 'U', 'b', 811.0, 111.0, 836.0, 122.0, colour='dark')

# ---- the drawer: its cover, seals and handles (face plane; the window's contents behind glass in the model)
_p('seal', 'D', 'U', 'c', 1048.7, 140.4, d=22.0, colour='dark')
_p('seal', 'D', 'U', 'c', 1351.0, 123.0, d=14.0)
_handle('D', 'U', 'b', 880.0, 161.0, 265.0)
_handle('D', 'U', 'b', 1495.0, 177.0, 277.0)

# ---- block 1 (g098-c on the blocks' plane)
for x, y, d in ((319.3, 331.7, 18.0), (347.7, 331.7, 18.0)):
    _p('stud', 'L1', 'L', 'c', x, y, d=d)
for x, y in ((394.0, 335.0), (459.0, 339.0)):
    _p('hex', 'L1', 'L', 'c', x, y, d=22.0)
_p('stud', 'L1', 'L', 'c', 422.3, 337.7, d=12.0)
for x, y in ((373.3, 331.0), (436.7, 333.0), (486.7, 337.0), (442.3, 374.0), (441.7, 402.3), (369.0, 371.7), (369.0, 400.0), (409.3, 413.7),
             (464.0, 443.3), (526.7, 445.7)):
    _p('screw', 'L1', 'L', 'c', x, y, d=6.0)
_p('fuse_base', 'L1', 'L', 'c', 542.7, 345.0, w=30.0, ht=30.0, h=12.0)
_p('fuse_cap', 'L1', 'L', 'c', 548.0, 345.0, d=20.0, h=32.0)
_r('chart', 'L1', 'L', 'c', 447.3, 356.0, 555.0, 418.3)
_p('knob_black', 'L1', 'L', 'c', 431.7, 387.3, d=28.0)
_r('conn_d', 'L1', 'L', 'c', 341.0, 412.7, 359.0, 466.0)
_p('stud', 'L1', 'L', 'c', 312.7, 433.3, d=16.0, colour='cream')
_r('meter', 'L1', 'L', 'c', 370.0, 463.3, 451.7, 543.3)
_p('screw', 'L1', 'L', 'c', 412.3, 532.3, d=10.0, h=10.0, colour='brass')
_r('scale', 'L1', 'L', 'c', 475.0, 438.0, 517.0, 455.0)
_p('knob_fluted', 'L1', 'L', 'c', 501.7, 479.3, d=36.0)
_p('coax', 'L1', 'L', 'c', 498.3, 528.3, d=38.0, dir=(0.55, 0.75))
_p('socket', 'L1', 'L', 'c', 509.3, 578.3, d=30.0)
_p('elbow', 'L1', 'L', 'c', 497.0, 606.0, d=34.0, h=20.0)       # its foot; the body runs to the right
_p('selector', 'L1', 'L', 'c', 436.7, 576.7, d=28.0)
_handle('L1', 'L', 'c', 316.0, 452.0, 597.0)
_handle('L1', 'L', 'c', 556.0, 458.0, 585.0)
_p('coax', 'L1', 'L', 'c', 347.3, 498.3, d=36.0, dir=(0.6, 0.8))
_r('latch', 'L1', 'L', 'c', 300.0, 610.0, 317.0, 625.0)

# ---- block 2
for x, y in ((640.0, 346.7), (748.3, 348.3)):
    _p('stud', 'L2', 'L', 'c', x, y, d=18.0)
_p('stud', 'L2', 'L', 'c', 695.0, 356.7, d=12.0)
_p('knob_cream', 'L2', 'L', 'c', 663.3, 390.0, d=26.0, h=30.0)
_p('stud', 'L2', 'L', 'c', 643.3, 436.7, d=16.0)
_p('fuse_base', 'L2', 'L', 'c', 741.7, 393.3, w=30.0, ht=42.0, h=12.0)
_p('fuse_cap', 'L2', 'L', 'c', 745.0, 402.0, d=24.0, h=32.0)
_r('cube', 'L2', 'L', 'c', 656.0, 420.0, 688.0, 440.0, h=18.0)
_r('cube', 'L2', 'L', 'c', 700.0, 405.0, 722.0, 432.0, h=14.0)
_r('data_plate', 'L2', 'L', 'c', 623.3, 451.7, 733.3, 490.0)
_r('strip', 'L2', 'L', 'c', 640.0, 505.0, 725.0, 523.3)
_p('coax_flange', 'L2', 'L', 'c', 673.3, 550.0, d=50.0, dir=(0.8, 0.45), flange=44.0)
_p('coax', 'L2', 'L', 'c', 636.7, 608.3, d=26.0, dir=(0.7, 0.3))
_p('coax', 'L2', 'L', 'c', 676.7, 603.3, d=22.0, dir=(0.7, 0.3))
for x, y in ((625.7, 400.7), (606.7, 509.3), (605.0, 546.7), (741.7, 550.0), (726.7, 440.7)):
    _p('screw', 'L2', 'L', 'c', x, y, d=6.0)
_handle('L2', 'L', 'c', 618.0, 440.0, 590.0)
_handle('L2', 'L', 'c', 762.0, 445.0, 592.0)

# ---- block 3
_r('data_plate', 'L3', 'L', 'b', 795.0, 373.0, 827.0, 433.0)
_p('knob_cream', 'L3', 'L', 'c', 876.7, 410.0, d=34.0, skirt=50.0)
_p('fuse_base', 'L3', 'L', 'c', 928.3, 390.0, w=32.0, ht=42.0, h=12.0)
_p('fuse_cap', 'L3', 'L', 'c', 935.0, 396.0, d=24.0, h=32.0)
_p('fuse_base', 'L3', 'L', 'c', 963.3, 398.3, w=32.0, ht=42.0, h=12.0)
_p('fuse_cap', 'L3', 'L', 'c', 970.0, 404.0, d=24.0, h=32.0)
_p('stud', 'L3', 'L', 'c', 836.7, 351.7, d=18.0)
_p('knob_fluted', 'L3', 'L', 'c', 953.3, 445.0, d=38.0, colour='brown')
_p('jack', 'L3', 'L', 'c', 917.3, 442.7, d=12.0)
_p('coax', 'L3', 'L', 'c', 820.0, 516.7, d=38.0, dir=(0.8, 0.3))
_p('elbow', 'L3', 'L', 'c', 866.7, 516.7, d=32.0)
_p('coax', 'L3', 'L', 'c', 926.7, 533.0, d=32.0, dir=(0.6, 0.6))
_p('coax', 'L3', 'L', 'c', 976.7, 523.0, d=34.0, dir=(0.6, 0.6))
_r('data_plate', 'L3', 'L', 'c', 845.0, 568.0, 938.0, 587.0)
_r('strip', 'L3', 'L', 'c', 847.0, 547.0, 914.0, 562.0)
_handle('L3', 'L', 'c', 822.0, 447.0, 590.0)
_handle('L3', 'L', 'c', 1020.0, 457.0, 593.0)

# ---- the right block: the narrow strip (its socket), the grey box, two big knobs, and its cut-away foot
_p('coax', 'L4', 'L', 'c', 1057.0, 452.0, d=22.0, h=20.0, dir=(0.0, 0.0), black=True)
_r('cube', 'L4', 'L', 'c', 1102.0, 400.0, 1202.0, 458.0, red=True)
_r('data_plate', 'L4', 'L', 'c', 1202.0, 407.0, 1242.0, 443.0)
_handle('L4', 'L', 'c', 1278.0, 405.0, 472.0)
_p('knob_ribbed', 'L4', 'L', 'c', 1326.7, 456.7, d=52.0)
_p('knob_ribbed', 'L4', 'L', 'c', 1376.0, 463.0, d=46.0, colour='brown', h=50.0)
_p('fuse_small', 'L4', 'L', 'c', 1300.0, 413.0, d=20.0, h=20.0)
_r('data_plate', 'L4', 'L', 'c', 1243.0, 480.0, 1290.0, 498.0)
_r('strip', 'L4', 'L', 'c', 1243.0, 500.0, 1330.0, 512.0)

# The cables in front of the blocks (g098-c; the flap hides their lower runs when closed): the flattened path on the blocks' plane, its
# height out of that plane, by eye.
CABLES = [
    # black coaxials
    dict(mat='Rubber_Black', r=4.5, pts=[('c', 347, 498, 45), ('c', 395, 575, 55), ('c', 470, 630, 58), ('c', 560, 660, 60), ('c', 640, 655, 62), ('c', 760, 610, 70), ('c', 920, 545, 72), ('c', 975, 523, 48)]),
    dict(mat='Rubber_Black', r=4.5, pts=[('c', 300, 512, 30), ('c', 360, 570, 45), ('c', 455, 640, 62), ('c', 580, 672, 62), ('c', 700, 640, 66), ('c', 820, 575, 62), ('c', 866, 518, 45)]),
    dict(mat='Rubber_Black', r=4.0, pts=[('c', 498, 528, 45), ('c', 540, 580, 55), ('c', 577, 630, 60), ('c', 560, 660, 62), ('c', 500, 670, 60)]),
    dict(mat='Rubber_Black', r=4.5, pts=[('c', 780, 550, 42), ('c', 775, 490, 44), ('c', 800, 440, 46), ('c', 845, 425, 48), ('c', 880, 450, 46), ('c', 890, 500, 44)]),
    dict(mat='Rubber_Black', r=3.5, pts=[('c', 1020, 480, 40), ('c', 1050, 540, 48), ('c', 1095, 600, 52), ('c', 1160, 660, 42), ('c', 1215, 650, 38), ('c', 1220, 600, 34)]),
    dict(mat='Rubber_Black', r=3.5, pts=[('c', 1035, 470, 42), ('c', 1085, 545, 50), ('c', 1130, 620, 46), ('c', 1175, 645, 40)]),
    # the silver-wrapped looms along the shelf
    dict(mat='Metal_Steel', r=10.0, pts=[('c', 360, 625, 28), ('c', 400, 672, 36), ('c', 480, 697, 40), ('c', 620, 695, 40), ('c', 770, 692, 44), ('c', 880, 665, 40), ('c', 985, 650, 30)]),
    dict(mat='Metal_Steel', r=11.0, pts=[('c', 1120, 590, 34), ('c', 1150, 640, 40), ('c', 1125, 680, 44), ('c', 1050, 700, 44), ('c', 960, 700, 42)]),
]


def parts(kind=None, block=None):
    return [p for p in PART_LIST if (kind is None or p['kind'] == kind) and (block is None or p['block'] == block)]


def to_m(x, y):
    """Face mm to metres on the cabin panel: a to the right, b up, from the panel's centre."""
    return ((x - W_MM / 2.0) * 0.001, (H_MM / 2.0 - y) * 0.001)


def rect_m(r):
    """(x0, y0, x1, y1) mm to the cabin's (centre a, b) and (width, height), metres."""
    a, b = to_m((r[0] + r[2]) / 2.0, (r[1] + r[3]) / 2.0)
    return (a, b), ((r[2] - r[0]) * 0.001, (r[3] - r[1]) * 0.001)


def cable_path(c):
    """A cable's polyline in the face's frame: (x, y, h out of the blocks' plane)."""
    out = []
    for photo, x, y, h in c['pts']:
        u, v = read(photo, 'L', x, y, h)
        out.append((u, v, h))
    return out
