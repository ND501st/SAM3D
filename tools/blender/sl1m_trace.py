"""SŁ-1M's face, traced from the book's photo g091-a (wwp_gecko p. 91) through the joint solve, the drawing g091-c telling what each part is. No bpy:
sl1m_model.py and tools/tests/test_sl1m_trace.py read this.

1. **Flattened** through g091-a's solved camera on the plane of 592 of the solve's points (5.6 mm rms), 1.5 degrees of roll taken out: `out/sl1m/face.png`, about 1 px per mm.
2. **The photo is nearly square-on**, so every part is read straight off it, in mm. 0009-5M came out with a further 1.2 degrees of tilt along its rows (its screws and lamps slope
   together), taken out by `_r3`.
3. **Scale:** the solve's, against no yardstick on this rack. 1.275 m wide against Fig. 3's 1.31, and the right column's blocks 591 mm against SŁ-3M's 600. The solve does not place
   this rack in the cabin (its forward compartment is not to scale), so the rack keeps the cabin's place along the wall; its height is docs/OSA_INTERIOR_HOLISTIC.md section 22's.

Frame: x mm to the right as one faces the rack, y mm down, the top-left block's top-left corner at (0, 0). `Z`: (c) SAM3D's choice: this rack has only the one sharp view, so no height
is measured here. The photo's session is the book's own (g090-g091: the vehicle with SP-1M's ZIP box nr 2).
"""
W_MM = 1275.0
H_MM = 554.0

BLOCKS = {
    'L1': ((0.0, 0.0, 640.0, 302.0), '0004-12M'),
    'L2': ((0.0, 317.0, 640.0, 554.0), 'OK81-4M-II'),
    'R1': ((683.0, 6.0, 1274.0, 169.0), '0005-8M'),
    'R2': ((683.0, 181.0, 1274.0, 348.0), 'OS51-8M'),
    'R3': ((683.0, 362.0, 1275.0, 540.0), '0009-5M'),
}
BLOCK_IDS = {'l_0004_12m': 'L1', 'l_ok81_4m2': 'L2', 'l_0005_8m': 'R1', 'l_os51_8m': 'R2', 'l_0009_5m': 'R3'}

Z = {           # all (c)
    'screw': 2.0, 'latch': 14.0, 'latch_plate': 1.5, 'meter': 20.0, 'hood': 3.0, 'lamp': 8.0, 'dome': 12.0, 'jack': 12.0, 'knob': 24.0, 'toggle': 14.0, 'strip': 5.0,
    'cover': 32.0, 'button_ring': 8.0, 'button': 10.0, 'black_panel': 3.0, 'clamp': 12.0, 'window': 0.6, 'dial_ring': 18.0, 'dial_ear': 8.0, 'dial_block': 26.0, 'nameplate': 1.0,
    'data_plate': 1.0, 'tab': 1.5,
}
D_LOWER = 0.0

FRAME_LEFT, FRAME_RIGHT = (-80.0, -8.0), (1282.0, 1305.0)     # the frame the blocks stand in (g091-a)
FRAME_TOP, FRAME_BOTTOM = (-30.0, -8.0), (562.0, 640.0)
DIVIDER = (646.0, 677.0)

# the game draws these itself: the orientation controls and the ЗК-4 dial's face (places: the photo's parts; which part is which: SAM3D's reading of the captions)
# (12 Nov 2026) СОГЛАСОВАНИЕ on ПУ-11's bottom button and ШИРОТА's knob in its scale's middle (voenizdat_osa_1991 p48); МК — ГПК — АК the
# black panel's top-centre toggle (p50)
CONTROL_MM = {'orient.ak_zk': (992.0, 85.0), 'orient.relay': (1015.5, 85.0), 'orient.sync': (847.1, 131.0), 'orient.qc': (1219.0, 85.0),
              'orient.latitude': (872.0, 59.0), 'gyro.compass_mode': (841.3, 61.0)}
DIAL_C, DIAL_R = (1146.4, 91.0), 42.0
LAT_SCALE_C = (872.0, 59.0)     # the ШИРОТА dial the game draws, on the black panel beside its knob, where the panel's arc is

PART_LIST = []


def _p(kind, block, x, y, **kw):
    kw.setdefault('h', Z.get(kind, 0.0))
    PART_LIST.append(dict(kind=kind, block=block, plane='U', c=(x, y), **kw))


def _r(kind, block, x0, y0, x1, y1, **kw):
    kw.setdefault('h', Z.get(kind, 0.0))
    PART_LIST.append(dict(kind=kind, block=block, plane='U', r=(x0, y0, x1, y1), **kw))


def _screws(block, pts, d=6.0):
    for x, y in pts:
        _p('screw', block, x, y, d=d)


def _latch(block, x, y):
    _r('latch_plate', block, x - 14.0, y - 21.0, x + 14.0, y + 21.0)
    _p('latch', block, x, y, d=22.0)


def _plates(block, a, b):
    _r('nameplate', block, *a)
    if b:
        _r('nameplate', block, *b)


def _r3(x, y):
    return x, y + 0.0213 * (x - 985.0)


# ---- 0004-12M: the two meters
_latch('L1', 36.8, 150.0)
_latch('L1', 614.7, 151.0)
_r('meter', 'L1', 183.0, 56.0, 291.0, 160.5)
_r('meter', 'L1', 371.6, 59.0, 479.0, 160.5)
_r('hood', 'L1', 196.0, 24.0, 280.0, 58.0)
_r('hood', 'L1', 384.0, 27.0, 466.0, 60.0)
_p('lamp', 'L1', 95.0, 81.6, d=14.0, colour='dark')
_p('lamp', 'L1', 95.0, 121.6, d=18.0, colour='amber')
_p('toggle', 'L1', 331.0, 63.0, d=12.0)
_p('lamp', 'L1', 332.0, 122.0, d=11.0, colour='dark')
_p('lamp', 'L1', 117.0, 212.6, d=12.0, colour='dark', ring=True)
_p('jack', 'L1', 164.0, 212.6, d=17.0, colour='black')
_p('lamp', 'L1', 252.6, 212.6, d=12.0, colour='dark', ring=True)
_p('jack', 'L1', 305.0, 212.6, d=17.0, colour='black')
_p('lamp', 'L1', 137.0, 249.0, d=12.0, colour='dark', ring=True)
_p('lamp', 'L1', 177.0, 249.0, d=12.0, colour='dark', ring=True)
_p('knob', 'L1', 433.0, 239.0, d=32.0, pointer=-40.0)
_p('knob', 'L1', 557.0, 239.0, d=32.0, pointer=-40.0)
_p('toggle', 'L1', 488.0, 246.0, d=12.0)
_screws('L1', [(93.7, 24.7), (170.0, 26.0), (330.5, 29.0), (487.0, 30.5), (557.0, 30.5), (584.0, 51.6), (65.8, 46.3), (170.0, 184.0), (330.5, 184.0), (487.0, 185.0),
               (67.0, 255.0), (94.7, 278.0), (213.0, 276.0), (330.5, 276.0), (445.0, 275.0), (556.0, 275.0), (583.0, 254.0)])
_screws('L1', [(205.0, 30.0), (270.0, 30.0), (394.0, 31.6), (458.0, 32.6)], d=5.0)
_plates('L1', (565.8, 17.4, 610.5, 28.0), (613.0, 18.0, 632.0, 28.0))

# ---- ОК81-4М-II: the long connector strip, the button, the white cover
_latch('L2', 35.0, 436.0)
_latch('L2', 613.0, 428.0)
_r('strip', 'L2', 122.6, 374.7, 139.5, 503.7)
_screws('L2', [(130.5, 368.0), (130.5, 509.0)], d=7.0)
_p('jack', 'L2', 361.0, 373.0, d=15.0, colour='steel')
_p('button_ring', 'L2', 358.4, 479.0, d=45.0)
_p('button', 'L2', 358.4, 479.0, d=24.0)
_r('tab', 'L2', 284.0, 425.8, 302.6, 457.4)
_r('cover', 'L2', 424.0, 342.0, 588.4, 480.0)
_screws('L2', [(89.5, 340.5), (210.5, 339.5), (329.0, 338.0), (559.5, 337.0), (89.5, 529.0), (210.5, 524.7), (328.4, 522.0), (445.0, 520.0), (559.0, 518.0)])
_plates('L2', (565.8, 322.0, 608.4, 333.7), (612.0, 322.0, 630.5, 334.0))
_r('tab', 'L2', 594.7, 338.0, 606.8, 350.0, colour='outline')

# ---- 0005-8M: the black panel, the two rows, the ЗК-4 dial
_latch('R1', 721.0, 87.0)
_latch('R1', 1255.5, 87.0)
_r('black_panel', 'R1', 749.5, 26.4, 909.0, 146.0)
for x, y in ((779.0, 59.0), (773.6, 93.0), (891.0, 95.5), (832.7, 92.0)):
    _p('toggle', 'R1', x, y, d=12.0, base=Z['black_panel'])
_p('jack', 'R1', 832.7, 116.0, d=12.0, colour='steel', base=Z['black_panel'])
_p('jack', 'R1', 773.6, 122.7, d=12.0, colour='black', base=Z['black_panel'])
_screws('R1', [(759.0, 32.0), (900.0, 32.0), (759.0, 140.0), (900.0, 140.0)], d=7.0)
for x in (916.0, 934.0, 953.0, 973.6, 994.0, 1013.6, 1035.0):
    _p('dome', 'R1', x, 40.0, d=19.0)
_p('dome', 'R1', 1073.6, 40.5, d=23.0)
for x in (928.0, 949.0, 971.0, 1041.0):     # the row's last: where orient.sync stood until 12 Nov 2026, what it is not known
    _p('jack', 'R1', x, 85.0, d=17.0, colour='black')
for x, c in ((909.0, 'amber'), (928.0, 'amber'), (947.0, 'amber'), (965.5, 'amber'), (984.5, 'amber'), (1002.0, 'amber')):
    _p('lamp', 'R1', x, 142.7, d=15.0, colour=c)
_p('lamp', 'R1', 1022.7, 142.7, d=10.0, colour='dark')
_p('jack', 'R1', 1057.7, 142.7, d=17.0, colour='black')
_p('jack', 'R1', 1078.0, 142.7, d=17.0, colour='black')
_r('dial_block', 'R1', 1072.7, 70.5, 1096.0, 106.8)
_p('lamp', 'R1', 1201.8, 63.6, d=11.0, colour='dark')
_p('lamp', 'R1', 1201.8, 118.0, d=11.0, colour='dark')
_r('nameplate', 'R1', 1215.5, 134.5, 1262.0, 158.0, colour='black')
_screws('R1', [(742.7, 38.6), (742.7, 59.0), (714.5, 116.0), (946.0, 123.6), (982.7, 123.6), (1019.0, 123.6), (1220.0, 41.8), (1248.6, 69.0), (1248.6, 116.0)], d=5.0)
_plates('R1', (1207.0, 19.5, 1249.5, 29.5), None)
_p('jack', 'R1', 1262.0, 112.0, d=8.0, colour='red')

# ---- ОС51-8М: two strips, the meter, the selector, the data plate
_latch('R2', 722.7, 271.0)
_latch('R2', 1253.0, 264.0)
_p('jack', 'R2', 1262.0, 244.5, d=9.0, colour='red')
_r('strip', 'R2', 789.5, 225.5, 808.6, 291.0)
_r('strip', 'R2', 1184.0, 223.0, 1203.0, 287.7)
_screws('R2', [(798.6, 220.5), (798.6, 296.8), (1194.0, 217.7), (1194.0, 292.0)], d=7.0)
_r('meter', 'R2', 879.5, 224.5, 957.7, 298.0)
_p('knob', 'R2', 1107.7, 250.5, d=36.0, pointer=35.0)
_r('data_plate', 'R2', 895.5, 314.0, 1077.0, 341.4)
_screws('R2', [(766.8, 206.4), (876.8, 206.0), (985.5, 206.0), (1093.0, 206.0), (1200.0, 206.0), (766.8, 330.5), (876.8, 329.5), (1091.8, 326.8), (1199.0, 325.5)])
_plates('R2', (1207.0, 190.5, 1266.0, 199.0), None)

# ---- 0009-5М: the three clamps, the two windows, the lamp row
_latch('R3', *_r3(723.6, 457.0))
_latch('R3', *_r3(1257.7, 446.8))
x0, y0 = _r3(766.8, 407.0)
_r('nameplate', 'R3', x0, y0, x0 + 46.8, y0 + 23.0, colour='black')
for x, y in ((911.0, 402.7), (1068.0, 401.8), (1079.0, 502.7)):
    _p('clamp', 'R3', *_r3(x, y), w=55.0, ht=41.0)
for x0, x1, y0, y1 in ((945.0, 970.5, 400.5, 435.5), (998.6, 1024.5, 399.5, 433.6)):
    a = _r3(x0, y0)
    b = _r3(x1, y1)
    _r('window', 'R3', a[0], a[1], b[0], b[1])
for x, y in ((761.0, 506.4), (776.8, 506.4), (792.7, 505.5), (809.0, 505.0), (1146.0, 497.7), (1161.0, 497.3), (1177.0, 497.3), (1192.0, 496.8), (1208.0, 496.8)):
    _p('lamp', 'R3', *_r3(x, y), d=10.0, colour='dark')
for x, y in ((836.4, 505.0), (891.0, 502.7), (941.0, 502.3), (973.6, 501.8), (1036.4, 500.5), (1125.0, 499.0)):
    _p('dome', 'R3', *_r3(x, y), d=23.0)
for x, y in ((863.0, 504.0), (916.0, 502.7), (1006.8, 501.0)):
    _p('jack', 'R3', *_r3(x, y), d=21.0, colour='black')
for px, py in ((185, 218), (122, 212), (183, 325), (183, 381), (267, 313), (348, 313), (363, 268), (430, 312), (512, 309), (593, 307), (674, 305), (755, 303), (835, 300),
               (915, 299), (995, 298), (1075, 298), (1155, 297), (1232, 295), (1292, 313), (1290, 190), (1232, 147), (1077, 113), (1178, 113), (1090, 180), (1187, 178),
               (992, 182), (985, 215), (985, 250), (857, 225), (857, 262), (493, 266), (635, 120), (798, 118), (703, 150), (735, 150), (612, 236), (700, 236), (733, 236),
               (818, 234), (227, 128), (312, 126), (362, 137), (183, 163)):
    _p('screw', 'R3', *_r3(660.0 + px / 2.2, 330.0 + py / 2.2), d=5.0)
_plates('R3', (1205.0, 368.0, 1247.0, 376.5), None)


def parts(kind=None, block=None):
    return [p for p in PART_LIST if (kind is None or p['kind'] == kind) and (block is None or p['block'] == block)]


def to_m(x, y):
    """Face mm to metres on the cabin panel: a to the right, b up, from the panel's centre."""
    return ((x - W_MM / 2.0) * 0.001, (H_MM / 2.0 - y) * 0.001)


def rect_m(r):
    a, b = to_m((r[0] + r[2]) / 2.0, (r[1] + r[3]) / 2.0)
    return (a, b), ((r[2] - r[0]) * 0.001, (r[3] - r[1]) * 0.001)
