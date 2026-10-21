"""The R-407's front, traced from photographs (no bpy: the Blender builder, the overlap tests and the PU-12M's cabin layout
all read it).

How it was traced (docs/MODELLING_FROM_PHOTOS.md; tools/photo_trace.py with tools/traces/r407_front.json repeats it):

1. The main photo: an R-407 for sale (`r407_photos`, olx.ua, 960 x 1280 px, not kept), square-on and slightly from above.
   Eleven straight edges of the front plate (its four sides, the frequency dials' plate, the label plate) meet in one
   horizontal and one vertical vanishing point to within 0.7 degrees: the plate is one plane.
2. The labels: every word on the front read off otechestvoussr.ru's 1920 px close-ups (`r407_photos`); the switches'
   names and what each does from the set's instruction plate (`r407_plate`).
3. The scale: the ТО gives the set 250 x 420 x 510 mm with its protruding parts (`r407_to`). Every photograph shows the
   front taller than wide, so 420 mm is the width over the side latches and 510 mm the height over the folded handle and
   the feet; the flattened photo gives 0.478 and 0.458 mm a pixel from them, and the mean (0.468, about 3 %) makes the
   front plate 333 x 446 mm.
4. Positions were read off the flattened photo on a 10 mm grid (about 2 mm); heights out of the plate (`Z`) from the
   oblique photographs of `r407_photos` (estimates good to about 5 mm). The meter's housing stands well out of the plate,
   so its face in the flattened photo is magnified and shifted by parallax: its footprint is its face's corners
   back-projected through the photo's solved camera (out/traces/r407_front/camera.json; the focal length is not in the
   photo's data: a phone's 26 mm assumed), then kept clear of the dials' plate and the АПЧ switch.

Frame: x mm to the right as the operator faces the set, y mm down, from the plate's top left. `Z` is out of the plate, mm.
`to_m` turns (x, y) into the model's (a, b) metres: a right, b up, from the plate's centre.
"""
import math

NAME = 'R407'
PLATE = (0.0, 0.0, 332.9, 446.1)
W_MM, H_MM = PLATE[2] - PLATE[0], PLATE[3] - PLATE[1]
CX, CY = (PLATE[0] + PLATE[2]) / 2, (PLATE[1] + PLATE[3]) / 2
PLATE_W_M, PLATE_H_M = W_MM / 1000.0, H_MM / 1000.0
DEPTH_M = 0.250                            # r407_to: 250 mm
CASE_M = (0.420, 0.510)                    # r407_to: over the latches, over the handle and feet
SX = SY = 0.001


def to_m(x, y):
    """Trace mm (x right, y down) -> (a, b) metres on the plate: a right, b up, from its centre."""
    return ((x - CX) * SX, (CY - y) * SY)


Z = {
    'plate': 0.0,
    'dial_plate': 1.5,        # the frequency dials' plate
    'escutcheon': 3.0,        # the dials' oval, the rings round the knobs
    'frame': 3.0,             # the toggles' frames
    'label_plate': 1.5,
    'meter_box': 32.0,        # the meter's protruding housing
    'ott_cover': 30.0,        # the ОТТ equipment's sockets' cover
    'terminals': 12.0,        # the brown terminal strip
    'case_lip': 18.0,         # the case's rim round the plate
    'paint': 0.2,
}

CORNER_R = 8.0
OUTLINE = [(0.0, 0.0), (332.9, 0.0), (332.9, 446.1), (0.0, 446.1)]
STRIP_OUTLINE = None

REGIONS = [
    {'id': 'meter_box', 'rect': (36.0, 22.0, 86.0, 73.0), 'z': (0.0, 32.0), 'mat': 'Paint_Radio', 'round': 4.0},
    {'id': 'dial_plate', 'rect': (87.4, 39.0, 273.0, 148.7), 'z': (0.0, 1.5), 'mat': 'Paint_Radio', 'round': 2.0},
    {'id': 'dial_oval', 'rect': (111.6, 53.5, 247.0, 121.0), 'z': (1.5, 4.5), 'mat': 'Paint_Radio', 'round': 33.0},
    {'id': 'label_plate', 'rect': (281.0, 69.0, 329.4, 139.0), 'z': (0.0, 1.5), 'mat': 'Metal_Brass', 'round': 2.0},
    {'id': 'nameplate', 'rect': (26.0, 127.0, 60.0, 147.0), 'z': (0.0, 1.2), 'mat': 'Metal_Dark', 'round': 2.0},
    {'id': 'ogr', 'rect': (118.0, 19.7, 157.0, 28.4), 'z': (0.0, 2.0), 'mat': 'Bakelite_Black', 'round': 4.0},
    {'id': 'band_frame', 'rect': (33.5, 256.0, 61.6, 299.8), 'z': (0.0, 3.0), 'mat': 'Paint_Radio', 'round': 12.0},
    {'id': 'power_frame', 'rect': (214.0, 390.0, 244.0, 436.9), 'z': (0.0, 3.0), 'mat': 'Paint_Radio', 'round': 13.0},
    {'id': 'ott_cover', 'rect': (182.6, 288.5, 255.0, 361.0), 'z': (0.0, 30.0), 'mat': 'Paint_Radio', 'round': 8.0},
    {'id': 'ott_tabs', 'rect': (190.0, 361.0, 248.0, 369.0), 'z': (0.0, 8.0), 'mat': 'Paint_Radio', 'round': 1.0},
    {'id': 'terminals', 'rect': (21.3, 411.0, 153.5, 446.0), 'z': (0.0, 12.0), 'mat': 'Bakelite_Brown', 'round': 4.0},
    {'id': 'connector_flange', 'rect': (257.0, 370.7, 298.0, 409.5), 'z': (0.0, 3.0), 'mat': 'Metal_Steel', 'round': 2.0},
]
OVAL = None
RING = None

ROUND = [
    # the frequency dials (the cabin's dial displays) and the two caps between them
    {'id': 'rx_dial', 'c': (148.0, 85.8), 'r': 22.0, 'z': (4.5, 12.0), 'kind': 'dome', 'display': 'rx'},
    {'id': 'tx_dial', 'c': (213.0, 85.8), 'r': 22.0, 'z': (4.5, 12.0), 'kind': 'dome', 'display': 'tx'},
    {'id': 'cap_top', 'c': (180.5, 62.6), 'r': 7.5, 'z': (4.5, 14.0), 'kind': 'slot_screw'},
    {'id': 'cap_bottom', 'c': (180.5, 106.0), 'r': 7.5, 'z': (4.5, 14.0), 'kind': 'slot_screw'},
    # tuning knobs, the СТОПОР levers, the correction plugs, the calibrator
    {'id': 'rx_freq', 'c': (101.0, 172.0), 'r': 22.5, 'z': (0.0, 30.0), 'kind': 'knurled', 'sim': 'radio.fug5.rx_freq'},
    {'id': 'tx_freq', 'c': (260.0, 172.0), 'r': 22.5, 'z': (0.0, 30.0), 'kind': 'knurled', 'sim': 'radio.fug5.tx_freq'},
    {'id': 'rx_lock', 'c': (82.0, 140.5), 'r': 5.0, 'z': (0.0, 26.0), 'kind': 'lever_boss', 'sim': 'radio.fug5.rx_lock'},
    {'id': 'tx_lock', 'c': (264.0, 205.0), 'r': 5.0, 'z': (0.0, 26.0), 'kind': 'lever_boss', 'sim': 'radio.fug5.tx_lock'},
    {'id': 'korr_rx', 'c': (155.0, 160.0), 'r': 10.5, 'z': (0.0, 12.0), 'kind': 'plug'},
    {'id': 'korr_tx', 'c': (306.8, 163.0), 'r': 12.0, 'z': (0.0, 12.0), 'kind': 'plug'},
    {'id': 'calibrator', 'c': (197.0, 182.0), 'r': 8.0, 'z': (0.0, 4.0), 'kind': 'button_well', 'sim': 'radio.fug5.calibrator'},
    # switches in their rings
    {'id': 'apch', 'c': (26.0, 87.4), 'r': 13.0, 'z': (0.0, 3.0), 'kind': 'boss', 'sim': 'radio.fug5.apch'},
    {'id': 'mode', 'c': (46.5, 233.6), 'r': 16.8, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.mode'},
    {'id': 'meter_sw', 'c': (44.5, 332.0), 'r': 16.8, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.meter'},
    {'id': 'rx_level', 'c': (98.0, 234.6), 'r': 17.7, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.rx_level'},
    {'id': 'measure_level', 'c': (146.5, 234.6), 'r': 17.7, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.measure_level'},
    {'id': 'tx_level', 'c': (194.0, 234.6), 'r': 17.7, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.tx_level'},
    {'id': 'call', 'c': (146.5, 277.8), 'r': 17.7, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.call'},
    {'id': 'measure', 'c': (146.5, 332.0), 'r': 17.7, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.measure'},
    {'id': 'pu', 'c': (298.7, 265.9), 'r': 17.7, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.pu'},
    {'id': 'lines', 'c': (298.0, 321.7), 'r': 17.7, 'z': (0.0, 3.0), 'kind': 'ring', 'sim': 'radio.fug5.lines'},
    {'id': 'band', 'c': (47.0, 277.8), 'r': 6.0, 'z': (3.0, 6.0), 'kind': 'toggle_boss', 'sim': 'radio.fug5.band'},
    {'id': 'power', 'c': (228.7, 414.3), 'r': 6.0, 'z': (3.0, 6.0), 'kind': 'toggle_boss', 'sim': 'radio.fug5.power'},
    # the lamps (the cabin's), sockets, caps, the terminals
    {'id': 'lamp_radio', 'c': (282.6, 219.7), 'r': 12.0, 'z': (0.0, 16.0), 'kind': 'lamp_holder', 'lamp': 'call_radio'},
    {'id': 'lamp_wire', 'c': (314.8, 219.7), 'r': 12.0, 'z': (0.0, 16.0), 'kind': 'lamp_holder', 'lamp': 'call_wire'},
    {'id': 'handset_cap', 'c': (31.6, 381.4), 'r': 16.8, 'z': (0.0, 16.0), 'kind': 'cap'},
    {'id': 'connector', 'c': (277.7, 390.0), 'r': 15.0, 'z': (3.0, 26.0), 'kind': 'connector'},
    {'id': 'ant_cap', 'c': (313.0, 24.0), 'r': 15.0, 'z': (0.0, 18.0), 'kind': 'cap'},
    {'id': 'corner_cap', 'c': (8.4, 21.3), 'r': 7.5, 'z': (0.0, 6.0), 'kind': 'cap'},
    {'id': 'term_1', 'c': (33.5, 431.0), 'r': 8.0, 'z': (12.0, 22.0), 'kind': 'terminal'},
    {'id': 'term_2', 'c': (58.4, 431.0), 'r': 8.0, 'z': (12.0, 22.0), 'kind': 'terminal'},
    {'id': 'term_3', 'c': (83.5, 431.0), 'r': 8.0, 'z': (12.0, 22.0), 'kind': 'terminal'},
    {'id': 'term_4', 'c': (110.0, 431.0), 'r': 8.0, 'z': (12.0, 22.0), 'kind': 'terminal'},
    {'id': 'term_5', 'c': (135.8, 431.0), 'r': 8.0, 'z': (12.0, 22.0), 'kind': 'terminal'},
    {'id': 'ott_latch', 'c': (218.0, 279.0), 'r': 4.0, 'z': (0.0, 34.0), 'kind': 'toggle_boss'},
]
TOUCHING = [('ott_latch', 'ott_cover')]
HOLES = [(122.9, 23.9, 2.5), (134.2, 23.9, 2.5), (143.9, 23.9, 2.5)]
SCREWS = [
    ((40.5, 26.5), 'meter_box'), ((81.5, 26.5), 'meter_box'), ((40.5, 68.5), 'meter_box'), ((81.5, 68.5), 'meter_box'),
    ((113.0, 24.0), 'plate'), ((161.0, 24.0), 'plate'), ((180.0, 34.0), 'plate'),
    ((92.0, 43.0), 'dial_plate'), ((268.5, 43.0), 'dial_plate'), ((92.0, 144.5), 'dial_plate'), ((268.5, 144.5), 'dial_plate'),
    ((180.5, 136.5), 'dial_plate'),
    ((119.0, 87.4), 'dial_oval'), ((241.5, 87.4), 'dial_oval'), ((152.0, 57.5), 'dial_oval'), ((208.4, 57.5), 'dial_oval'),
    ((152.0, 116.5), 'dial_oval'), ((208.4, 116.5), 'dial_oval'),
    ((285.0, 73.0), 'label_plate'), ((325.0, 73.0), 'label_plate'), ((285.0, 135.0), 'label_plate'), ((325.0, 135.0), 'label_plate'),
    ((17.4, 169.7), 'plate'), ((13.2, 196.5), 'plate'), ((176.0, 169.7), 'plate'), ((176.0, 199.0), 'plate'),
    ((171.3, 221.7), 'plate'), ((227.7, 221.7), 'plate'), ((16.5, 234.6), 'plate'), ((16.5, 333.6), 'plate'),
    ((120.5, 255.0), 'plate'), ((172.0, 257.8), 'plate'), ((216.0, 254.0), 'plate'), ((146.5, 305.6), 'plate'),
    ((132.6, 376.5), 'plate'), ((260.0, 273.3), 'plate'), ((277.0, 363.6), 'plate'), ((245.5, 375.6), 'plate'),
    ((277.7, 418.5), 'plate'), ((10.0, 433.6), 'plate'), ((321.3, 432.7), 'plate'), ((314.8, 186.0), 'plate'),
    ((318.0, 360.0), 'plate'), ((14.0, 365.0), 'plate'),
]
LAMPS = []
TUBES = []
PINS = []
# The СТОПОР levers' arms (from the boss, its length and the angle it points at, degrees anticlockwise from the right).
LEVERS = {'rx_lock': (24.0, 120.0), 'tx_lock': (22.0, -60.0)}

TEXT = [
    ('ВНИМАНИЕ!', (142.6, 46.0), 7.0, 0, 'dial_plate', 'Paint_Red'),
    ('ПРОТИВНИК ПОДСЛУШИВАЕТ', (210.0, 47.5), 3.6, 0, 'dial_plate', 'Paint_Red'),
    ('ПРИЕМНИК', (97.0, 92.3), 3.6, -90, 'dial_plate', 'Paint_Letters_White'),
    ('ПЕРЕДАТЧИК', (263.5, 92.3), 3.6, -90, 'dial_plate', 'Paint_Letters_White'),
    ('УСТАНОВКА ЧАСТОТЫ x 100 КГЦ', (179.4, 128.5), 3.6, 0, 'dial_plate', 'Paint_Letters_White'),
    ('ВЫКЛ.', (17.0, 110.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('АПЧ', (44.0, 110.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('Р-407', (43.0, 133.0), 4.0, 0, 'nameplate', 'Paint_Letters_White'),
    ('КОРР.', (134.0, 154.0), 3.0, 0, 'plate', 'Paint_Letters_White'), ('ПРМ', (134.0, 159.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('КОРР.', (287.0, 154.0), 3.0, 0, 'plate', 'Paint_Letters_White'), ('ПРД', (287.0, 159.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('КАЛИБРАТОР', (197.0, 157.5), 3.0, 0, 'plate', 'Paint_Letters_White'), ('СВЕТ', (197.0, 163.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('СТОПОР', (70.0, 175.0), 3.0, -90, 'plate', 'Paint_Letters_White'),
    ('СТОПОР', (230.0, 176.0), 3.0, -90, 'plate', 'Paint_Letters_White'),
    ('УРОВНИ', (146.0, 205.0), 4.0, 0, 'plate', 'Paint_Letters_White'),
    ('ПРИЕМ', (98.7, 213.5), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ИЗМЕРИТ.', (146.5, 213.5), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ПЕРЕДАЧА', (194.0, 213.5), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ДЕЖ. ПРИЕМ', (43.0, 202.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('СИМПЛЕКС', (45.5, 207.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ДУПЛЕКС', (16.5, 218.0), 2.8, 0, 'plate', 'Paint_Letters_White'),
    ('ТА', (69.0, 218.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('УЗКАЯ', (77.0, 266.5), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ШИРОКАЯ', (79.0, 294.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ВЫЗОВ', (114.8, 277.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ИЗМЕРИТ.', (112.5, 316.0), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('УРОВЕНЬ', (112.5, 321.5), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ИЗМЕРИТ.', (14.0, 312.5), 2.6, 0, 'plate', 'Paint_Letters_White'),
    ('ПЕРЕДАЧА', (46.5, 306.5), 2.6, 0, 'plate', 'Paint_Letters_White'),
    ('ПРИЕМ', (72.5, 312.0), 2.6, 0, 'plate', 'Paint_Letters_White'),
    ('НАКАЛ', (75.0, 324.5), 2.6, 0, 'plate', 'Paint_Letters_White'),
    ('ТОК АНТ.', (78.0, 336.5), 2.6, 0, 'plate', 'Paint_Letters_White'),
    ('КОНТРОЛЬ', (297.0, 244.0), 2.8, 0, 'plate', 'Paint_Letters_White'),
    ('РАДИО', (275.5, 262.0), 2.8, 90, 'plate', 'Paint_Letters_White'),
    ('ПРОВОД', (322.0, 262.0), 2.8, -90, 'plate', 'Paint_Letters_White'),
    ('ТРАНЗИТ', (281.0, 300.4), 2.8, 0, 'plate', 'Paint_Letters_White'),
    ('ИВ.', (313.0, 300.4), 2.8, 0, 'plate', 'Paint_Letters_White'),
    ('2-ПР.', (268.0, 306.5), 2.6, 0, 'plate', 'Paint_Letters_White'),
    ('ОКОНЕЧН.', (266.0, 312.0), 2.4, 0, 'plate', 'Paint_Letters_White'),
    ('4-ПР.', (325.5, 306.5), 2.4, 0, 'plate', 'Paint_Letters_White'),
    ('ТВ.', (325.5, 312.0), 2.6, 0, 'plate', 'Paint_Letters_White'),
    ('ВКЛ.', (202.6, 392.4), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ПИТАНИЕ', (196.5, 414.3), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('ВЫКЛ.', (200.3, 435.3), 3.0, 0, 'plate', 'Paint_Letters_White'),
    ('4-ПР. ПРИЕМ', (57.0, 396.0), 2.8, 0, 'plate', 'Paint_Letters_White'),
    ('2-ПР.', (65.0, 401.0), 2.8, 0, 'plate', 'Paint_Letters_White'),
    ('4-ПР. ПЕРЕДАЧА', (101.0, 404.0), 2.8, 0, 'plate', 'Paint_Letters_White'),
    ('КОРПУС', (138.0, 404.0), 2.8, 0, 'plate', 'Paint_Letters_White'),
]

CONTROL_MM = {
    'radio.fug5.power': (228.7, 414.3), 'radio.fug5.mode': (46.5, 233.6), 'radio.fug5.apch': (26.0, 87.4),
    'radio.fug5.band': (47.0, 277.8), 'radio.fug5.pu': (298.7, 265.9), 'radio.fug5.lines': (298.0, 321.7),
    'radio.fug5.meter': (44.5, 332.0), 'radio.fug5.rx_freq': (101.0, 172.0), 'radio.fug5.tx_freq': (260.0, 172.0),
    'radio.fug5.rx_lock': (82.0, 140.5), 'radio.fug5.tx_lock': (264.0, 205.0), 'radio.fug5.calibrator': (197.0, 182.0),
    'radio.fug5.call': (146.5, 277.8), 'radio.fug5.measure': (146.5, 332.0), 'radio.fug5.rx_level': (98.0, 234.6),
    'radio.fug5.measure_level': (146.5, 234.6), 'radio.fug5.tx_level': (194.0, 234.6),
}
CONTROL_Z = {'radio.fug5.band': 'frame', 'radio.fug5.power': 'frame'}
LAMP_MM = {'call_radio': (282.6, 219.7), 'call_wire': (314.8, 219.7)}
METER_FACE = (44.0, 28.0, 78.0, 52.0)
DIALS = {'rx': (148.0, 85.8, 20.0), 'tx': (213.0, 85.8, 20.0)}


def rect_m(r):
    x0, y0, x1, y1 = r
    a0, b1 = to_m(x0, y0)
    a1, b0 = to_m(x1, y1)
    return (a0, b0, a1, b1)


def surface_z(key):
    if key in Z:
        return Z[key]
    for reg in REGIONS:
        if reg['id'] == key:
            return reg['z'][1]
    raise KeyError(key)


def region(rid):
    return next(r for r in REGIONS if r['id'] == rid)


def round_part(pid):
    return next(p for p in ROUND if p['id'] == pid)


def region_points(reg):
    if 'pts' in reg:
        return list(reg['pts'])
    x0, y0, x1, y1 = reg['rect']
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def control_m(cid):
    return to_m(*CONTROL_MM[cid])


def polygon_area(pts):
    return 0.5 * abs(sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts))))


def circle_pts(c, r, seg=32):
    return [(c[0] + r * math.cos(2 * math.pi * k / seg), c[1] + r * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
