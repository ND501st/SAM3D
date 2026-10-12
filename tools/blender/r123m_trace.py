"""The R-123M's front panel, traced from a photograph (no bpy: the Blender builder, the sketch tool and the cabin layout
all read it).

Reference: an R-123 in a museum, Wikimedia Commons "Funkstation_R-123_IMG_1390.JPG" (3000 x 2250 px, shown here at 2000 x
1500; the file is not kept in the repository, docs/SOURCES.md `r123_commons`). The photo shows the older R-123; the R-123M's
panel is the same family (`r123m_training` names the same controls), so it stands in for this vehicle's sets, whose own
photographs are not in any source found. Positions were read off the photo with a 100 px grid and are good to about 10 px
(2 mm). Everything is in photo pixels; `to_m` turns them into metres on the plate.

The plate is 0.428 m wide (the set's 428 mm, `r123_refs`) and 0.206 m high: the set is 225 mm high, and the casing's top lid
(a ledge about 19 mm deep above the plate) takes the rest. The photo's width gives 0.220 mm per px across and its height
0.220 mm per px down, so the scale is the same both ways.

Plate coordinates: a to the right as the operator looks at it, b up, origin at the plate's centre.
"""
import math

REF_W, REF_H = 2000, 1500          # the photo as traced
PLATE_PX = (12, 258, 1958, 1195)   # left, top, right, bottom of the front plate (the casing's top lid is above it)
PLATE_W_M, PLATE_H_M = 0.428, 0.206   # the set is 428 x 225 mm; the plate is the casing less its 19 mm top lid
CX = 0.5 * (PLATE_PX[0] + PLATE_PX[2])
CY = 0.5 * (PLATE_PX[1] + PLATE_PX[3])
SX = PLATE_W_M / (PLATE_PX[2] - PLATE_PX[0])
SY = PLATE_H_M / (PLATE_PX[3] - PLATE_PX[1])


def to_m(x, y):
    """Photo pixel -> (a, b) metres on the plate: a right, b up, from its centre."""
    return ((x - CX) * SX, (CY - y) * SY)


def len_m(px, axis='x'):
    return px * (SX if axis == 'x' else SY)


# Round openings in the plate: a recessed well each, with the control's shaft in it. `r` is the opening's radius in px.
# `sim` names the cabin control that stands in the well (the cabin draws its knob); None: the model has the knob.
WELLS = [
    {'id': 'squelch',   'c': (265, 400),  'r': 66,  'sim': 'squelch',  'knob': 'three'},
    {'id': 'mode',      'c': (470, 445),  'r': 102, 'sim': 'mode',     'knob': 'lever'},
    {'id': 'voltage',   'c': (215, 625),  'r': 95,  'sim': None,       'knob': 'lever'},
    {'id': 'freq_set',  'c': (470, 700),  'r': 100, 'sim': None,       'knob': 'three'},
    {'id': 'antenna_tune', 'c': (1295, 600), 'r': 170, 'sim': None,    'knob': 'red'},
    {'id': 'selector',  'c': (1330, 945), 'r': 190, 'sim': 'preset',   'knob': 'lever_big'},
    {'id': 'volume',    'c': (1725, 950), 'r': 110, 'sim': 'volume',   'knob': 'lever'},
]

# Small round holes (blind, dark).
HOLES = [(1370, 315, 22), (1890, 625, 22), (1825, 1160, 22), (343, 310, 18), (638, 980, 18), (55, 425, 18)]

# Slotted screws (px centres), r ~ 12 px.
SCREWS = [(160, 365), (170, 440), (235, 850), (625, 305), (680, 340), (965, 345), (850, 948), (883, 948),
          (1525, 440), (1525, 575), (1600, 520), (1830, 520), (1612, 622), (1858, 628), (1625, 662), (1840, 668),
          (1640, 935), (1800, 935), (1595, 312), (1905, 322), (270, 322), (588, 285),
          (465, 865), (415, 905), (590, 905), (1270, 420), (1480, 540)]

# Plates with engraved text. `rect` (x0, y0, x1, y1) px; `text` lines; `h` letter height px; `rot` degrees (anticlockwise).
PLATES = [
    {'id': 'shumy',      'rect': (165, 295, 310, 330), 'text': ['ШУМЫ'], 'h': 20},
    {'id': 'left_strip', 'rect': (90, 305, 150, 475), 'text': [], 'h': 14},
    {'id': 'korrektor',  'rect': (565, 290, 600, 490), 'text': ['КОРРЕКТОР'], 'h': 17, 'rot': 90},
    {'id': 'mode_arc',   'rect': (410, 505, 590, 590), 'text': ['ДУПЛЕКС Д.ПРИЕМ', 'СИМПЛЕКС'], 'h': 15},
    {'id': 'voltage',    'rect': (50, 480, 375, 800), 'text': ['КОНТРОЛЬ НАПРЯЖЕНИЙ', '1 РАБОТА 2'], 'h': 14, 'label_at': (215, 495)},
    {'id': 'freq_set',   'rect': (400, 800, 600, 860), 'text': ['УСТАНОВКА', 'ЧАСТОТЫ'], 'h': 17},
    {'id': 'calib',      'rect': (410, 870, 600, 930), 'text': ['КАЛИБРОВКА'], 'h': 15},
    {'id': 'freq_bezel_text', 'rect': (700, 425, 945, 475), 'text': ['ЧАСТОТА x100 (кГц)'], 'h': 18, 'plain': True},
    {'id': 'sub_a',      'rect': (990, 280, 1035, 440), 'text': ['ПОДДИАПАЗ'], 'h': 15, 'rot': 90},
    {'id': 'sub_b',      'rect': (1500, 425, 1550, 590), 'text': ['ПОДДИАПАЗ', 'I', 'II'], 'h': 14, 'rot': 90},
    {'id': 'antenna_tune_arc', 'rect': (1250, 410, 1480, 460), 'text': ['НАСТРОЙКА АНТЕННЫ'], 'h': 16, 'rot': -18},
    {'id': 'subband',    'rect': (1605, 750, 1830, 785), 'text': ['ПОДДИАПАЗОН'], 'h': 18},
    {'id': 'volume',     'rect': (1630, 1025, 1810, 1075), 'text': ['ГРОМКОСТЬ'], 'h': 18, 'rot': -6},
    # plates for the controls the cabin adds (not on the traced plate): power, antenna, headset
    {'id': 'power',      'rect': (440, 985, 560, 1015), 'text': ['ПИТАНИЕ'], 'h': 15},
    {'id': 'antenna_sel', 'rect': (40, 1125, 190, 1160), 'text': ['АНТЕННА'], 'h': 15},
    {'id': 'headset',    'rect': (1625, 1140, 1765, 1170), 'text': ['ГАРНИТУРА'], 'h': 15},
]

# Position numerals and small marks: (text, centre px, h px)
MARKS = [
    ('1', (1637, 370), 20), ('2', (1715, 372), 20), ('3', (1790, 372), 20), ('4', (1865, 377), 20),
]

# Window panes: glass, `rect` px.
WINDOWS = [
    {'id': 'freq', 'rect': (700, 280, 945, 400), 'bezel': (655, 240, 990, 500)},
    {'id': 'dwin', 'rect': (1640, 610, 1835, 730), 'bezel': None, 'round_bottom': True},
]
LENS = {'c': (1180, 400), 'r': 55}                 # the antenna-tuning indicator lamp's lens (concentric rings)
RED_LAMP = {'c': (180, 875), 'r': 22}
LAMP_WINDOWS = {'panel': (1590, 785, 1875, 880), 'c': [(1655, 835), (1800, 838)], 'r': 22}

# The preset drum: a round opening with a hinged cap; four fixators inside; the cap closed in the model.
DRUM = {'c': (850, 740), 'r_opening': 190, 'r_cap': 182, 'latch_w': 300, 'latch_h': 110,
        'hinge': (820, 910, 910, 985)}

# The preset fixator window: four numbered round buttons over a hinged flap with a finger notch.
FIXATORS = {'panel': (1565, 305, 1935, 600), 'buttons': [(1637, 370), (1715, 372), (1790, 372), (1865, 377)], 'r': 25,
            'flap': (1575, 435, 1925, 590), 'notch': (1700, 520, 1800, 595)}

# Curved plates round the big selector's well (angles anticlockwise from the right, degrees; radii px from its centre):
# ФИКСИР. (positions 1, 2), ВОЛНЫ (3, 4) and ПЛАВНЫЙ ПОДДИАПАЗОН (I, II).
ARCS = [
    {'id': 'fiksir', 'a0': 128, 'a1': 194, 'text': 'ФИКСИР.', 'marks': [('1', 188), ('2', 131)]},
    {'id': 'volny', 'a0': 0, 'a1': 52, 'text': 'ВОЛНЫ', 'marks': [('3', 46), ('4', 6)]},
    {'id': 'smooth', 'a0': 238, 'a1': 302, 'text': 'ПЛАВНЫЙ ПОДДИАПАЗОН', 'marks': [('I', 245), ('II', 295)]},
]
ARC_R_IN, ARC_R_OUT = 200, 248
SELECTOR_C = (1330, 945)

CONNECTOR = {'housing': (190, 920, 415, 1040), 'c': (300, 970), 'r': 62}   # the multi-pin socket at the bottom left
SMALL_SOCKETS = [(180, 755), (240, 755), (318, 765)]                       # the three sockets under КОНТРОЛЬ НАПРЯЖЕНИЙ
SOCKET_PANEL = (130, 710, 370, 800)
SIDE_BOLTS = [(1950, 820), (1945, 990), (1965, 285)]

# Where the cabin's controls stand on the plate (the knobs of the wired functions and the ones the cabin adds), px.
CONTROL_PX = {
    'squelch': (265, 400),
    'mode': (470, 445),
    'preset': (1330, 945),
    'volume': (1725, 950),
    'power': (500, 1052),
    'antenna': (115, 1085),
    'headset': (1695, 1118),
    'lamp_ready': (1655, 835),
    'lamp_warm': (1800, 838),
}

# Round plate corners (px radius) at the top left and top right; the bottom corners are nearly square.
CORNER_PX = 55


def rect_m(r):
    """A px rectangle -> (a0, b0, a1, b1) metres, a0 < a1, b0 < b1."""
    x0, y0, x1, y1 = r
    a0, b1 = to_m(x0, y0)
    a1, b0 = to_m(x1, y1)
    return (a0, b0, a1, b1)


def circle_m(c, r, seg=40):
    cx, cy = to_m(*c)
    rr = r * SX
    return [(cx + rr * math.cos(2 * math.pi * k / seg), cy + rr * math.sin(2 * math.pi * k / seg)) for k in range(seg)]


def plate_outline_m(seg=8):
    """The plate's outline, anticlockwise from the bottom left, the top corners rounded."""
    a0, b0, a1, b1 = rect_m(PLATE_PX)
    r = CORNER_PX * SX
    pts = [(a0, b0), (a1, b0)]
    for k in range(seg + 1):                      # top right corner
        t = math.radians(0 + 90 * k / seg)
        pts.append((a1 - r + r * math.cos(t), b1 - r + r * math.sin(t)))
    for k in range(seg + 1):                      # top left corner
        t = math.radians(90 + 90 * k / seg)
        pts.append((a0 + r + r * math.cos(t), b1 - r + r * math.sin(t)))
    return pts


def control_m(name):
    return to_m(*CONTROL_PX[name])
