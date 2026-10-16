"""The R-123M's front, traced in 3D from two photographs (no bpy: the Blender builder, the sketch tool and the cabin layout
all read it).

How it was traced (docs/MODELLING_FROM_PHOTOS.md; tools/photo_trace.py with tools/traces/r123m_front.json repeats it):

1. The photo: an R-123 in a museum, Wikimedia Commons "Funkstation_R-123_IMG_1390.JPG" (3000 x 2250 px; `r123_commons`; not
   kept in the repository). It is taken from above and to the right, close up: the plate's verticals lean up to 7 degrees
   and its horizontals up to 2. Twelve straight edges that lie in the plate (its top and bottom edges, the edges of the label
   plates, the fixator panel) were fitted with RANSAC; they meet in one horizontal and one vertical vanishing point to within
   0.65 degrees, so the plate is one plane, and the homography that sends those two points to infinity flattens it.
2. The aspect ratio comes out right without any further correction: the band selector's well, the best-fitted circle on the
   plate, is round to 0.6 % in the flattened image. The scale: the front is 225 mm high (the set's height, `r123_refs`),
   which makes it 406 mm wide; the set's 428 mm includes the bolts and fittings on its sides.
3. In the flattened, true-scale image (5 px/mm) the round openings were fitted as circles (wells, drum rim, lens: RANSAC
   on edge pixels, to about 0.3 mm) and the plates, screws, windows and holes read off a 5 mm grid (to about 1 mm). Where
   a fit caught a well's outer chamfer rather than its opening (mode, frequency set) the radius is the opening's, so the
   plates beside them clear it (tools/tests/test_r123m_trace.py checks nothing overlaps).
4. The second photo, an R-123 in the St Petersburg artillery museum (`r123_kuzmin`), mapped into the same frame through its
   wells (agreement 1-2 mm on the left and centre), gives what the first hides: the drum's cap closed, the frequency hood's
   shape, the pocket with its three bat handles. It is taken from well above (about 24 degrees), and the tops of the parts
   that stand out show as lit faces: their heights (`Z`) are read from those faces and are good to about 2 mm.

The photos show the R-123; the R-123M keeps its front (`r123m_qrz`).

Frame: x mm to the right as the operator faces the set, y mm down, from the front's top left corner (the flat plate's
corner, its rounding left off). `Z` is out of the plate, mm. `to_m` turns (x, y) into the model's (a, b) metres: a right,
b up, from the plate's centre.
"""
import math

W_MM, H_MM = 406.0, 225.0           # the front: 225 mm high (r123_refs), 406 mm wide in the flattened photo
PLATE_W_M, PLATE_H_M = W_MM / 1000.0, H_MM / 1000.0
SX = SY = 0.001                     # mm -> m (the trace is in mm)


def to_m(x, y):
    """Trace mm (x right, y down from the top left) -> (a, b) metres on the plate: a right, b up, from its centre."""
    return ((x - W_MM / 2) * SX, (H_MM / 2 - y) * SY)


def len_m(mm):
    return mm * SX


# How far each part stands out of the plate (+) or sits back in it (-), mm (read from `r123_kuzmin`, see above).
Z = {
    'label_plate': 1.2,      # black engraved plates, the arcs round the selector
    'voltage_plate': 1.5,
    'hood_top': 13.0,        # the frequency hood's front at the window
    'hood_lip_bottom': 8.0,  # its sloping lip, at the bottom edge (the lip carries ЧАСТОТА x 100 (кГц))
    'hood_glass': 4.0,       # the window's glass, set back in the hood
    'cap_rim': 12.0,         # the drum's cap, closed: its rim
    'cap_face': 11.0,
    'cap_recess': 6.0,       # the recess in the cap that holds the lock knob
    'fixator_recess': -3.0,  # the fixator buttons' panel lies in a recess
    'fixator_flap': 2.0,     # the hinged flap over its lower half
    'dwin_bezel': 3.0,       # the arched window's bezel
    'dwin_glass': -5.0,
    'lamp_plate': 4.0,       # the sub-band lamps' plate (I, II)
    'subband_plate': 1.5,
    'lens_ring': 5.0,        # the antenna-tuning lamp's lens ring
    'pocket': -10.0,         # the pocket under the voltage plate holding the three bat handles
    'lower_block': -16.0,    # the block under the plate's lower left (connector, toggle), set back
    'well_depth': -30.0,
    'nameplate': 1.0,
}

# The front's outline: the flat plate, its top corners rounded; at its lower left a step up: the plate's left part ends at
# y = 164 and the set-back block below carries the connector and a toggle.
CORNER_R = {'top_left': 9.0, 'top_right': 9.0, 'bottom_right': 8.0, 'step': 4.0, 'notch': 5.0}
STEP_Y, NOTCH_X = 164.0, 110.0

# Round openings: a recessed well each, with the control's shaft in it (circle fits). `sim` names the cabin control that
# stands in the well (the cabin draws its knob); None: the model has the knob.
WELLS = [
    {'id': 'squelch',      'c': (53.8, 32.7),   'r': 14.7, 'sim': 'squelch', 'knob': 'bar'},
    {'id': 'mode',         'c': (98.5, 39.9),   'r': 22.3, 'sim': 'mode',    'knob': 'bar'},
    {'id': 'voltage',      'c': (36.0, 86.8),   'r': 22.5, 'sim': None,      'knob': 'bar'},
    {'id': 'freq_set',     'c': (90.5, 101.8),  'r': 21.8, 'sim': None,      'knob': 'bar'},
    {'id': 'antenna_tune', 'c': (272.0, 76.1),  'r': 36.8, 'sim': None,      'knob': 'red'},
    {'id': 'selector',     'c': (278.4, 162.1), 'r': 42.3, 'sim': 'preset',  'knob': 'bar'},
    {'id': 'volume',       'c': (364.2, 160.9), 'r': 24.1, 'sim': 'volume',  'knob': 'bar'},
]

# Blind holes (dark), (x, y, r).
HOLES = [(72.8, 11.0, 4.0), (10.0, 39.7, 4.3), (288.5, 9.6, 4.6), (396.0, 78.8, 4.5), (387.7, 214.6, 4.5),
         (120.2, 176.2, 4.0)]

# Slotted screws on the plate (x, y); the ones on raised parts are listed with those parts.
SCREWS = [
    (37.0, 12.0), (63.5, 12.0),                       # ШУМЫ plate
    (19.5, 16.3), (21.0, 46.3),                       # РЕГ. / ДЕЖ.ПР. strip
    (4.4, 57.0), (66.5, 53.7), (6.1, 101.7), (63.9, 103.3),   # voltage plate
    (77.2, 70.6), (117.2, 70.6),                      # mode plate
    (124.0, 7.0), (124.0, 52.0),                      # КОРРЕКТОР strip
    (71.7, 118.0), (112.2, 118.3), (71.9, 157.7), (111.5, 158.5),   # УСТАНОВКА ЧАСТОТЫ / КАЛИБРОВКА plate
    (213.0, 6.0), (213.0, 37.0),                      # ПОДДИАПАЗ strip by the hood
    (267.7, 32.7), (308.5, 60.4),                     # НАСТРОЙКА АНТЕННЫ arc
    (320.5, 37.5), (320.5, 67.0),                     # ПОДДИАПАЗ strip by the fixators
    (345.5, 78.5), (384.0, 78.5), (339.0, 103.0), (390.5, 103.0),   # round the arched window
    (341.5, 112.7), (386.9, 112.7),                   # ПОДДИАПАЗОН plate
    (347.0, 180.8), (380.0, 180.8),                   # ГРОМКОСТЬ arc
    (129.5, 197.5),                                   # lower left of the drum
]
# Big slotted brass heads: the two squelch regulators beside РЕГ. / ДЕЖ.ПР., the corrector by the hood, (x, y, r).
BRASS_HEADS = [(32.3, 26.3, 4.2), (32.8, 42.6, 4.2), (133.0, 32.7, 4.3)]

# Plates with engraved letters. `rect` (x0, y0, x1, y1) mm; `text` lines; `h` cap height mm; `rot` degrees anticlockwise
# (90: reads upwards, -90: downwards); `at` the text's centre when it isn't the plate's; `cut` a well (id) the plate is
# cut round; `line` a rule across it at that y; `z` the surface it sits on when not the plate.
PLATES = [
    {'id': 'shumy',      'rect': (35.5, 7.0, 68.0, 17.0), 'text': ['ШУМЫ'], 'h': 3.0},
    {'id': 'reg_strip',  'rect': (16.5, 11.5, 25.4, 50.0), 'text': ['РЕГ.', 'ДЕЖ.ПР.'], 'h': 2.4, 'rot': 90},
    {'id': 'korrektor',  'rect': (121.0, 2.5, 127.0, 57.0), 'text': ['КОРРЕКТОР'], 'h': 3.0, 'rot': -90},
    {'id': 'mode_plate', 'rect': (72.5, 57.0, 118.5, 75.5), 'text': [], 'h': 2.6, 'cut': 'mode'},
    {'id': 'freq_plate', 'rect': (70.5, 117.0, 115.5, 163.0), 'text': [], 'h': 2.8, 'cut': 'freq_set', 'line': 138.5},
    {'id': 'sub_a',      'rect': (208.5, 2.0, 218.5, 40.0), 'text': ['ПОДДИАПАЗ'], 'h': 2.8, 'rot': -90, 'at': (215.5, 21.0)},
    {'id': 'sub_b',      'rect': (315.4, 33.5, 327.7, 71.0), 'text': ['ПОДДИАПАЗ'], 'h': 2.8, 'rot': 90, 'at': (318.5, 52.0)},
    {'id': 'subband',    'rect': (337.0, 107.5, 392.0, 117.5), 'text': ['ПОДДИАПАЗОН'], 'h': 3.0},
    # plates for the controls the cabin adds (not on the real front): power and antenna on the lower block, headset
    {'id': 'power',      'rect': (78.0, 214.0, 104.0, 220.0), 'text': ['ПИТАНИЕ'], 'h': 2.6, 'z': 'lower_block'},
    {'id': 'antenna_sel', 'rect': (27.0, 214.0, 53.0, 220.0), 'text': ['АНТЕННА'], 'h': 2.6, 'z': 'lower_block'},
    {'id': 'headset',    'rect': (313.0, 216.0, 339.0, 222.0), 'text': ['ГАРНИТУРА'], 'h': 2.4},
]
# Free-standing engraved words (on a plate already listed or on the voltage plate): (text, (x, y), h, rot, on-what z key).
LABELS = [
    ('КОНТРОЛЬ НАПРЯЖЕНИЙ', (36.7, 54.0), 2.5, 0, 'voltage_plate'),
    ('1 РАБОТА 2', (36.7, 59.2), 2.5, 0, 'voltage_plate'),
    ('600в', (9.6, 69.6), 2.2, 0, 'voltage_plate'), ('250в', (8.6, 78.8), 2.2, 0, 'voltage_plate'),
    ('150в', (9.0, 92.0), 2.2, 0, 'voltage_plate'), ('1,2в', (10.5, 102.5), 2.2, 0, 'voltage_plate'),
    ('1,2в', (58.5, 69.6), 2.2, 0, 'voltage_plate'), ('6,3в', (59.0, 78.8), 2.2, 0, 'voltage_plate'),
    ('150в', (61.5, 92.0), 2.2, 0, 'voltage_plate'),
    ('ПЕРЕДАЧА', (3.6, 86.0), 2.2, 90, 'voltage_plate'), ('ПРИЕМ', (66.0, 75.5), 2.2, -90, 'voltage_plate'),
    ('ДУПЛЕКС', (84.0, 66.5), 2.4, -28, 'label_plate'), ('Д.ПРИЕМ', (112.0, 66.5), 2.4, 28, 'label_plate'),
    ('СИМПЛЕКС', (98.3, 72.5), 2.4, 0, 'label_plate'),
    ('УСТАНОВКА', (92.5, 128.5), 2.6, 0, 'label_plate'), ('ЧАСТОТЫ', (92.5, 133.3), 2.6, 0, 'label_plate'),
    ('КАЛИБРОВКА', (93.0, 159.0), 2.6, 0, 'label_plate'),
    ('I', (211.5, 12.0), 2.6, 0, 'label_plate'), ('II', (211.5, 30.0), 2.6, 0, 'label_plate'),
    ('I', (324.0, 45.0), 2.6, 0, 'label_plate'), ('II', (324.0, 60.0), 2.6, 0, 'label_plate'),
]
VOLT_TICKS = [150, 170, 200, 228, 30, 10, -20]     # degrees round the voltage well where the scale's ticks stand
VOLT_PLATE = (1.0, 51.0, 68.0, 108.0)              # the voltage plate's rect (cut round the voltage well)

# The voltage plate's pocket under it (x0, y0, x1, y1), and the three bat-handle switches in it (x, y).
POCKET = (5.0, 108.0, 65.5, 138.6)
BAT_HANDLES = [(20.5, 122.0), (35.5, 122.0), (52.5, 122.0)]

# Curved plates round a well: centre (well id), radii mm, angles degrees anticlockwise from the right (up is +90);
# `studs` (text, angle) the white position studs at r_stud.
ARCS = [
    {'id': 'fiksir', 'well': 'selector', 'r': (44.0, 53.5), 'a': (118, 191), 'text': 'ФИКСИР.', 'text_a': 150,
     'studs': [('1', 171.0), ('2', 125.7)], 'r_stud': 49.5},
    {'id': 'volny', 'well': 'selector', 'r': (44.0, 54.0), 'a': (-9, 63), 'text': 'ВОЛНЫ', 'text_a': 30,
     'studs': [('3', 52.3), ('4', 7.9)], 'r_stud': 49.5},
    {'id': 'smooth', 'well': 'selector', 'r': (44.5, 60.0), 'a': (234, 306), 'text': 'ПЛАВНЫЙ\nПОДДИАПАЗОН', 'text_a': 270,
     'text_r': 52.5, 'studs': [('I', 249.2), ('II', 291.0)], 'r_stud': 49.5},
    {'id': 'antenna_arc', 'well': 'antenna_tune', 'r': (39.5, 46.5), 'a': (19, 99), 'text': 'НАСТРОЙКА АНТЕННЫ',
     'text_a': 59, 'studs': [], 'r_stud': 43.0},
    {'id': 'volume_arc', 'well': 'volume', 'r': (25.5, 31.5), 'a': (222, 318), 'text': 'ГРОМКОСТЬ', 'text_a': 270,
     'studs': [], 'r_stud': 28.5},
]

# The frequency hood: its body (x0, y0, x1, y1) from the plate's top edge, the window in it, the sloping lip below with
# its label and two screws.
HOOD = {'body': (142.0, -3.0, 206.0, 30.0), 'window': (148.5, 2.7, 200.8, 29.6), 'lip': (137.0, 30.0, 209.0, 56.0),
        'lip_r': 5.0, 'label': ('ЧАСТОТА x100 (кГц)', (173.8, 42.0), 3.4), 'screws': [(142.3, 39.6), (204.6, 39.6)]}

# The preset drum: its opening (the brass rim) and the cap over it, closed; the hinge under it; the plate under that.
DRUM = {'c': (174.5, 107.3), 'r_opening': 43.8, 'r_cap': 46.5, 'hinge': (152.0, 153.0, 186.0, 170.0),
        'hinge_pins': [(170.8, 165.4), (178.5, 165.4)], 'plate': (141.0, 180.0, 206.0, 204.0),
        'plate_screws': [(147.0, 185.5), (200.5, 185.5)]}
# The cap's face (`r123_kuzmin`), as fractions of r_cap and degrees anticlockwise from the right: a black strip round its
# upper left with ОТКР. and ЗАКР. and three screws, a recess in the middle with the round lock knob and its lever.
CAP = {'strip_r': (0.72, 0.88), 'strip_a': (62, 182), 'screws_a': [69, 128, 176],
       'texts': [('ОТКР.', 160), ('ЗАКР.', 92)], 'recess_r': 0.62, 'knob_r': 0.34, 'lever_a': 108, 'nub_a': -58}

# The antenna-tuning lamp's lens (concentric rings).
LENS = {'c': (249.5, 29.4), 'r': 12.45}
RED_LAMP = {'c': (17.3, 154.6), 'r': 5.0}
BRASS_IN_HOLE = [(30.4, 146.9, 4.5), (83.5, 148.5, 4.5)]   # a brass slotted shaft in a hole: by the red lamp; КАЛИБРОВКА

# The preset fixators: a recess (frame), the panel with the four numbered buttons and its screws and rivets, the hinged flap
# over the lower half with its spring and an arched finger notch.
FIXATORS = {'recess': (328.5, 5.8, 401.5, 72.0), 'panel': (331.0, 8.5, 399.5, 36.0),
            'buttons': [(343.0, 20.4), (358.2, 20.4), (373.1, 20.4), (388.5, 20.4)], 'r': 5.1,
            'screws': [(333.5, 12.0), (365.5, 12.0), (396.5, 12.0), (333.5, 30.4), (366.0, 30.4), (396.5, 30.4)],
            'rivets': [(350.5, 12.0), (380.5, 12.0), (350.5, 30.4), (380.5, 30.4)],
            'flap': (330.0, 35.5, 400.0, 70.4), 'flap_tab': (344.0, 39.5, 388.0),   # the flap's top is lower between x0..x1
            'spring': (366.0, 38.0, 47.0), 'notch': (355.4, 54.2, 376.9, 70.4)}
MARKS = [('1', (343.0, 20.4), 4.0), ('2', (358.2, 20.4), 4.0), ('3', (373.1, 20.4), 4.0), ('4', (388.5, 20.4), 4.0)]

# The arched window (sub-band dial) and its bezel; the sub-band lamps' plate with the two lenses (the cabin's lamps).
DWIN = {'rect': (344.6, 75.8, 385.4, 101.0), 'bezel_grow': 3.5}
LAMP_PLATE = {'rect': (333.0, 118.5, 394.0, 142.3), 'c': [(349.2, 130.8), (379.7, 130.8)], 'r': 4.5,
              'screws': [(338.0, 122.5), (389.0, 122.5), (338.0, 138.5), (389.0, 138.5), (363.5, 130.5)]}

# The lower block (set back): the multi-pin connector's housing and socket.
CONNECTOR = {'housing': (15.0, 165.0, 69.0, 198.0), 'c': (46.0, 180.0), 'r': 13.0}
NAMEPLATE = {'rect': (343.0, 199.0, 383.0, 220.0), 'text': ['Р-123М']}
SIDE_BOLTS = [4.0, 105.0, 166.0]       # y of the bolt heads on the casing's right side

# Where the cabin's controls stand (x, y), and which ones stand on the set-back lower block (the model gives those a
# bushing out to the plate's plane, so the cabin's toggle and knob meet it).
CONTROL_MM = {
    'squelch': (53.8, 32.7), 'mode': (98.5, 39.9), 'preset': (278.4, 162.1), 'volume': (364.2, 160.9),
    'power': (91.0, 206.0), 'antenna': (40.0, 205.0), 'headset': (326.0, 207.0),
    'lamp_ready': (349.2, 130.8), 'lamp_warm': (379.7, 130.8),
}
ON_LOWER_BLOCK = ('power', 'antenna')


def well(wid):
    return next(w for w in WELLS if w['id'] == wid)


def rect_m(r):
    """A mm rectangle (x0, y0, x1, y1) -> (a0, b0, a1, b1) metres, a0 < a1, b0 < b1."""
    x0, y0, x1, y1 = r
    a0, b1 = to_m(x0, y0)
    a1, b0 = to_m(x1, y1)
    return (a0, b0, a1, b1)


def circle_m(c, r, seg=40):
    cx, cy = to_m(*c)
    rr = r * SX
    return [(cx + rr * math.cos(2 * math.pi * k / seg), cy + rr * math.sin(2 * math.pi * k / seg)) for k in range(seg)]


def arch_m(r, seg=16, grow=0.0):
    """A mm rectangle with a round top (a half circle as wide as it), flat bottom -> points anticlockwise, metres; grow
    widens it all round (mm)."""
    a0, b0, a1, b1 = rect_m((r[0] - grow, r[1] - grow, r[2] + grow, r[3] + grow))
    rr = (a1 - a0) / 2
    cb = max(b0, b1 - rr)
    pts = [(a0, b0), (a1, b0)]
    for k in range(seg + 1):
        t = math.pi * k / seg
        pts.append(((a0 + a1) / 2 + rr * math.cos(t), cb + (b1 - cb) * math.sin(t)))
    return pts


def rounded_rect_m(r, rad, seg=6):
    """A mm rectangle with rounded corners (rad mm) -> points anticlockwise, metres."""
    a0, b0, a1, b1 = rect_m(r)
    q = rad * SX
    pts = []
    for (cx, cy, t0) in ((a1 - q, b0 + q, -90), (a1 - q, b1 - q, 0), (a0 + q, b1 - q, 90), (a0 + q, b0 + q, 180)):
        for k in range(seg + 1):
            t = math.radians(t0 + 90 * k / seg)
            pts.append((cx + q * math.cos(t), cy + q * math.sin(t)))
    return pts


def _corner(pts, cx, cy, r, t0, t1, seg):
    for k in range(seg + 1):
        t = math.radians(t0 + (t1 - t0) * k / seg)
        pts.append(to_m(cx + r * math.cos(t), cy - r * math.sin(t)))


def plate_outline_m(seg=6):
    """The flat plate's outline, metres, anticlockwise (as seen from the front) from the notch's bottom: the rounded top
    corners, the step at the lower left and the notch under it."""
    R = CORNER_R
    pts = []
    _corner(pts, NOTCH_X + 3.0, H_MM - 3.0, 3.0, 180, 270, 2)                              # notch, bottom
    _corner(pts, W_MM - R['bottom_right'], H_MM - R['bottom_right'], R['bottom_right'], 270, 360, seg)
    _corner(pts, W_MM - R['top_right'], R['top_right'], R['top_right'], 0, 90, seg)
    _corner(pts, R['top_left'], R['top_left'], R['top_left'], 90, 180, seg)
    _corner(pts, R['step'], STEP_Y - R['step'], R['step'], 180, 270, seg)                 # the step's outer corner
    _corner(pts, NOTCH_X - R['notch'], STEP_Y + R['notch'], R['notch'], 90, 0, seg)       # its inner (concave) corner
    return pts


def control_m(name):
    return to_m(*CONTROL_MM[name])
