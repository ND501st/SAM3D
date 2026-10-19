"""The R-111's front, traced from a photograph and the set's own drawing (no bpy: the Blender builder, the overlap tests and
the PU-12M's cabin layout all read it).

How it was traced (docs/MODELLING_FROM_PHOTOS.md; tools/photo_trace.py with tools/traces/r111_front.json repeats it):

1. The main photo: an R-111 on a bench, valka.cz topic 38123 (`r111_photos`, 1824 x 1368 px, not kept), taken almost
   square-on. Eleven straight edges of the upper plate (its top edge, the label plate's, the meter's frame, the button
   frames) meet in one horizontal and one vertical vanishing point to within 0.6 degrees: the upper plate is one plane. The
   connector strip under it lies in a recess (`r111_to` p. 60: the panel's lower, recessed part) and was not used to flatten.
2. The second reference: the set's ТО, fig. 4 (`r111_to`), the front drawn square-on with all 34 controls numbered and
   named; flattened the same way (tools/traces/r111_drawing.json). It names every part and shows what the photo's covers
   hide. The photographed set is a later series: its ВИД РАБОТЫ has a ninth position (ВНЕШ. УПР.), its meter and the knobs
   round it stand a few mm from where fig. 4 draws them, and there is no louvre block top left. Positions here are the
   photo's; the labels the ТО's (the console's ВИД РАБОТЫ keeps the ТО's eight positions).
3. The scale: no published size of the front was found. The ČSLA plan (`csla_pu12_manual`, item 41) gives the set about
   0.60 m wide; the R-123M standing on it reads there at its true 481 mm, so the plan is good to about 5 %. The photo was
   fitted to the drawing's controls at that scale (0.339 mm a flattened pixel); its round parts come out round to 1-2 %, and
   its plate 577 mm wide and 314 mm high, the drawing's height (fig. 4 is drawn about 4 % wide).
4. Positions were read off the flattened photo on a 10 mm grid (about 2 mm) and the round parts fitted as circles (about
   0.5 mm: `photo_trace.py circles`). Heights out of the plate (`Z`) are read from the oblique photographs of `r111_photos`
   (radiotract.ru views): estimates good to about 5 mm.

Frame: x mm to the right as the operator faces the set, y mm down, from the top left of the trace's frame (the plate's left
edge is at x = 3). `Z` is out of the plate, mm. `to_m` turns (x, y) into the model's (a, b) metres: a right, b up, from
the plate's centre.
"""
import math

NAME = 'R111'
PLATE = (3.0, 0.0, 580.0, 314.3)          # the front plate (mm)
W_MM, H_MM = PLATE[2] - PLATE[0], PLATE[3] - PLATE[1]
CX, CY = (PLATE[0] + PLATE[2]) / 2, (PLATE[1] + PLATE[3]) / 2
PLATE_W_M, PLATE_H_M = W_MM / 1000.0, H_MM / 1000.0
DEPTH_M = 0.400                            # the casing behind the plate: csla_pu12_manual's plan (item 41: 0.44 m with the front's knobs), scaled
SX = SY = 0.001


def to_m(x, y):
    """Trace mm (x right, y down) -> (a, b) metres on the plate: a right, b up, from its centre."""
    return ((x - CX) * SX, (CY - y) * SY)


# How far each surface stands out of the plate (+) or sits back (-), mm.
Z = {
    'plate': 0.0,
    'lower_strip': -14.0,     # the recessed connector strip (r111_photos: a step under the upper plate)
    'frame': 4.0,             # the square frames round the buttons, the meter's frame
    'lamp_lens': 7.0,         # the hexagonal lamp lenses
    'column': 3.0,            # the preset buttons' slanted column plate
    'escutcheon': 2.5,        # the black oval and ring round the wing knobs
    'disc': 4.0,              # the clear dials under the wing knobs
    'label_plate': 1.5,
    'meter_face': -3.0,
    'subpanel': 4.0,          # the lower right plate (УСТАНОВКА ЧАСТОТЫ, НОМЕР ЗПЧ)
    'hood': 42.0,             # the sheet-metal hood over the preset mechanism's scale and eyepiece
    'hatch': 14.0,            # the preset mechanism's round hatch cover (dome)
    'handle': 24.0,           # the tube handles' axis
    'paint': 0.2,             # painted letters
}

CORNER_R = 12.0
# The plate's outline: the upper plate all across, down the right side to the bottom; the recessed strip below on the left.
STRIP = (3.0, 242.0, 456.0, 314.3)
OUTLINE = [(3.0, 0.0), (580.0, 0.0), (580.0, 314.3), (472.0, 314.3), (456.0, 242.0), (3.0, 242.0)]
STRIP_OUTLINE = [(3.0, 242.0), (456.0, 242.0), (472.0, 314.3), (3.0, 314.3)]

# Raised (or sunk) slabs: id, outline (rect or points), z0, z1 (mm), material; `round` corner radius for rects.
REGIONS = [
    {'id': 'sau_block', 'rect': (134.0, 0.0, 182.0, 43.0), 'z': (0.0, 6.0), 'mat': 'Paint_Radio', 'round': 3.0},
    {'id': 'meter_frame', 'rect': (198.0, 29.0, 255.0, 100.0), 'z': (0.0, 4.0), 'mat': 'Paint_Radio', 'round': 4.0},
    {'id': 'um_frame', 'rect': (260.0, 59.0, 291.0, 96.0), 'z': (0.0, 4.0), 'mat': 'Paint_Radio', 'round': 2.0},
    {'id': 'sau_frame', 'rect': (292.0, 59.0, 322.0, 96.0), 'z': (0.0, 4.0), 'mat': 'Paint_Radio', 'round': 2.0},
    {'id': 'call_frame', 'rect': (342.0, 60.0, 369.0, 96.0), 'z': (0.0, 4.0), 'mat': 'Paint_Radio', 'round': 2.0},
    {'id': 'psh_frame', 'rect': (376.0, 60.0, 405.5, 96.0), 'z': (0.0, 4.0), 'mat': 'Paint_Radio', 'round': 2.0},
    {'id': 'levels_frame', 'rect': (338.0, 97.0, 407.0, 135.0), 'z': (0.0, 3.0), 'mat': 'Paint_Radio', 'round': 8.0},
    {'id': 'column', 'pts': [(405.0, 18.0), (424.0, 18.0), (439.0, 30.0), (439.0, 141.0), (405.0, 141.0)], 'z': (0.0, 3.0),
     'mat': 'Paint_Radio'},
    {'id': 'label_plate', 'rect': (35.0, 105.5, 105.0, 193.0), 'z': (0.0, 1.5), 'mat': 'Paint_White', 'round': 2.0},
    {'id': 'window_ogr', 'rect': (452.7, 54.5, 502.0, 71.0), 'z': (0.0, 2.0), 'mat': 'Paint_Radio', 'round': 6.0},
    {'id': 'hood', 'rect': (465.0, 78.0, 549.0, 160.0), 'z': (0.0, 42.0), 'mat': 'Paint_Radio', 'round': 6.0},
    {'id': 'hinge_block', 'rect': (449.0, 178.0, 471.0, 214.5), 'z': (0.0, 14.0), 'mat': 'Paint_Radio', 'round': 2.0},
    {'id': 'microtoggles', 'rect': (551.0, 80.0, 571.0, 188.0), 'z': (0.0, 3.0), 'mat': 'Paint_Radio', 'round': 3.0},
    {'id': 'subpanel', 'pts': [(472.0, 233.0), (567.0, 233.0), (567.0, 293.0), (458.0, 293.0), (458.0, 247.0)], 'z': (0.0, 4.0),
     'mat': 'Paint_Radio'},
    {'id': 'nameplate', 'rect': (134.5, 263.6, 155.0, 282.0), 'z': (-14.0, -12.8), 'mat': 'Metal_Dark', 'round': 1.0},
    {'id': 'zpch_window', 'rect': (492.7, 267.0, 500.0, 282.0), 'z': (4.0, 4.6), 'mat': 'Glass_Dark', 'round': 1.0},
]
# The black oval and ring the wing knobs stand in (escutcheons), as (centre, half-length, radius) and (centre, r_out, r_in).
OVAL = {'c0': (213.1, 177.6), 'c1': (262.0, 177.6), 'r_out': 26.5, 'r_in': 20.0}
RING = {'c': (365.5, 174.5), 'r_out': 25.5, 'r_in': 18.0}

# Round parts: id, centre, radius, z (from, to), kind, `sim` = the cabin's control (the cabin draws it: the model leaves a
# shaft stub), `lamp` = the cabin's lamp, `display` = the cabin's display.
ROUND = [
    # knobs
    {'id': 'manual_um', 'c': (145.8, 177.5), 'r': 38.4, 'z': (0.0, 22.0), 'kind': 'spoked'},
    {'id': 'mode', 'c': (213.1, 178.1), 'r': 15.6, 'z': (0.0, 4.0), 'kind': 'dial_disc', 'sim': 'radio.fug3.mode'},
    {'id': 'indicator', 'c': (262.0, 176.9), 'r': 15.9, 'z': (0.0, 4.0), 'kind': 'dial_disc', 'sim': 'radio.fug3.indicator'},
    {'id': 'work', 'c': (365.5, 174.5), 'r': 16.5, 'z': (0.0, 4.0), 'kind': 'dial_disc', 'sim': 'radio.fug3.work'},
    {'id': 'tx_level', 'c': (352.0, 114.5), 'r': 13.0, 'z': (3.0, 21.0), 'kind': 'trilobe', 'sim': 'radio.fug3.tx_level'},
    {'id': 'rx_level', 'c': (388.4, 114.5), 'r': 13.0, 'z': (3.0, 21.0), 'kind': 'trilobe', 'sim': 'radio.fug3.rx_level'},
    {'id': 'subband', 'c': (531.5, 54.6), 'r': 22.5, 'z': (0.0, 6.0), 'kind': 'base', 'sim': 'radio.fug3.subband'},
    {'id': 'dial', 'c': (530.7, 255.0), 'r': 29.0, 'z': (4.0, 34.0), 'kind': 'knurled', 'sim': 'radio.fug3.dial'},
    # buttons and the noise suppressor's toggle (in their frames)
    {'id': 'um', 'c': (275.6, 77.5), 'r': 9.0, 'z': (4.0, 6.0), 'kind': 'button_well', 'sim': 'radio.fug3.um'},
    {'id': 'sau', 'c': (307.0, 77.5), 'r': 9.0, 'z': (4.0, 6.0), 'kind': 'button_well', 'sim': 'radio.fug3.sau'},
    {'id': 'call', 'c': (356.0, 78.0), 'r': 9.0, 'z': (4.0, 6.0), 'kind': 'button_well', 'sim': 'radio.fug3.call'},
    {'id': 'squelch', 'c': (391.0, 78.0), 'r': 9.0, 'z': (4.0, 6.0), 'kind': 'button_well', 'sim': 'radio.fug3.squelch'},
    {'id': 'zpch_pl', 'c': (421.0, 34.5), 'r': 6.5, 'z': (3.0, 5.0), 'kind': 'button_well', 'sim': 'radio.fug3.zpch_pl'},
    {'id': 'zpch_1', 'c': (421.0, 58.0), 'r': 6.5, 'z': (3.0, 5.0), 'kind': 'button_well', 'sim': 'radio.fug3.zpch_1'},
    {'id': 'zpch_2', 'c': (421.0, 82.0), 'r': 6.5, 'z': (3.0, 5.0), 'kind': 'button_well', 'sim': 'radio.fug3.zpch_2'},
    {'id': 'zpch_3', 'c': (421.0, 105.5), 'r': 6.5, 'z': (3.0, 5.0), 'kind': 'button_well', 'sim': 'radio.fug3.zpch_3'},
    {'id': 'zpch_4', 'c': (421.0, 131.0), 'r': 6.5, 'z': (3.0, 5.0), 'kind': 'button_well', 'sim': 'radio.fug3.zpch_4'},
    {'id': 'light', 'c': (560.0, 194.5), 'r': 4.0, 'z': (3.0, 8.0), 'kind': 'toggle_boss'},
    # connectors, caps, terminals
    {'id': 'sau_hf', 'c': (158.0, 17.5), 'r': 8.0, 'z': (6.0, 26.0), 'kind': 'hf_connector'},
    {'id': 'bp', 'c': (70.5, 282.0), 'r': 24.3, 'z': (-14.0, 18.0), 'kind': 'connector', 'flange': (43.0, 254.0, 97.5, 310.0)},
    {'id': 'avt_sau', 'c': (276.7, 275.0), 'r': 11.6, 'z': (-14.0, 6.0), 'kind': 'connector', 'flange': (257.5, 255.0, 296.0, 294.0)},
    {'id': 'pu', 'c': (363.0, 272.7), 'r': 22.0, 'z': (-14.0, 22.0), 'kind': 'cap_connector', 'flange': (338.0, 249.0, 387.0, 296.0)},
    {'id': 'mtg', 'c': (409.0, 271.0), 'r': 16.0, 'z': (-14.0, 16.0), 'kind': 'cap_connector', 'flange': (389.0, 253.0, 427.0, 289.0)},
    {'id': 'tlk', 'c': (215.6, 272.7), 'r': 16.1, 'z': (-14.0, -10.0), 'kind': 'cap_well', 'sim': 'radio.fug3.tlk'},
    {'id': 'line_minus', 'c': (364.0, 304.5), 'r': 6.0, 'z': (-14.0, 0.0), 'kind': 'terminal'},
    {'id': 'line_plus', 'c': (385.5, 304.5), 'r': 6.0, 'z': (-14.0, 0.0), 'kind': 'terminal'},
    {'id': 'earth', 'c': (130.0, 304.0), 'r': 5.5, 'z': (-14.0, -4.0), 'kind': 'terminal'},
    {'id': 'hatch', 'c': (502.7, 198.9), 'r': 33.0, 'z': (0.0, 14.0), 'kind': 'hatch'},
    {'id': 'zpch_screw', 'c': (474.5, 272.7), 'r': 6.5, 'z': (4.0, 6.0), 'kind': 'slot_screw'},
    {'id': 'pilot', 'c': (331.0, 11.0), 'r': 5.5, 'z': (0.0, 6.0), 'kind': 'red_lens'},
]
# Parts built onto one another: the hatch's dome hangs on its hinge block.
TOUCHING = [('hatch', 'hinge_block')]

# Blind holes (correction and the discriminator's zero), (x, y, r).
HOLES = [(449.0, 147.0, 3.0), (447.0, 249.0, 3.0)]
# Slotted screws (x, y) and their surface's z key.
SCREWS = [
    ((198.0 + 4, 29.0 + 4), 'frame'), ((255.0 - 4, 29.0 + 4), 'frame'), ((198.0 + 4, 100.0 - 4), 'frame'), ((255.0 - 4, 100.0 - 4), 'frame'),
    ((263.0, 62.0), 'frame'), ((288.0, 62.0), 'frame'), ((263.0, 93.0), 'frame'), ((288.0, 93.0), 'frame'),
    ((295.0, 62.0), 'frame'), ((319.0, 62.0), 'frame'), ((295.0, 93.0), 'frame'), ((319.0, 93.0), 'frame'),
    ((345.0, 63.0), 'frame'), ((366.0, 63.0), 'frame'), ((345.0, 93.0), 'frame'), ((366.0, 93.0), 'frame'),
    ((379.0, 63.0), 'frame'), ((402.5, 63.0), 'frame'), ((379.0, 93.0), 'frame'), ((402.5, 93.0), 'frame'),
    ((40.0, 110.5), 'label_plate'), ((100.0, 110.5), 'label_plate'), ((40.0, 188.0), 'label_plate'), ((100.0, 188.0), 'label_plate'),
    ((141.0, 11.0), 'plate'), ((175.0, 11.0), 'plate'), ((193.0, 13.0), 'plate'), ((313.0, 11.0), 'plate'), ((348.0, 11.0), 'plate'),
    ((441.0, 13.0), 'plate'), ((165.0, 102.0), 'plate'), ((313.0, 128.0), 'plate'), ((432.0, 151.0), 'plate'),
    ((470.0, 63.0), 'plate'), ((545.0, 165.0), 'plate'), ((458.0, 225.0), 'plate'), ((571.0, 204.0), 'plate'),
    ((20.0, 300.0), 'lower_strip'), ((115.0, 260.0), 'lower_strip'), ((182.0, 300.0), 'lower_strip'), ((247.0, 300.0), 'lower_strip'),
    ((300.0, 300.0), 'lower_strip'), ((330.0, 260.0), 'lower_strip'), ((437.0, 300.0), 'lower_strip'),
]
# The hexagonal lamp lenses (rect, colour, words, the halves' words) and the cabin's lamps in them.
LAMPS = [
    {'id': 'protect', 'rect': (112.0, 66.0, 174.5, 94.0), 'color': 'red', 'text': ['ЗАЩИТА', 'ВХОДА'], 'halves': None},
    {'id': 'tune', 'rect': (256.0, 25.5, 318.0, 51.0), 'color': 'amber', 'text': ['НАСТРОЙКА'], 'halves': None},
    {'id': 'call', 'rect': (354.5, 25.5, 416.0, 51.0), 'color': 'amber', 'text': None, 'halves': ['РАДИО', 'ЛИНИЯ']},
    {'id': 'rxtx', 'rect': (447.0, 27.0, 503.6, 51.0), 'color': 'amber', 'text': None, 'halves': ['ПРИЕМ', 'ПРД']},
]
# The tube handles: the path's points (mm) at the handle's axis height, the tube's radius, the brackets (rects).
TUBES = [
    {'id': 'handle_left', 'path': [(80.0, 5.5), (11.0, 5.5), (11.0, 236.0), (80.0, 236.0)], 'r': 4.0,
     'brackets': [(69.0, 0.0, 87.0, 11.0), (69.0, 230.0, 87.0, 242.0)]},
    {'id': 'handle_right', 'path': [(518.0, 5.5), (574.5, 5.5), (574.5, 303.6), (518.0, 303.6)], 'r': 4.0,
     'brackets': [(504.0, 0.0, 531.0, 11.0), (504.0, 298.0, 531.0, 309.0)]},
]
# The cover's hinge pins along the step (rods across, x0, x1, y, r).
PINS = [(158.0, 189.0, 246.0, 4.0), (411.0, 440.0, 241.0, 4.0)]
# The preset mechanism's hatch: the bar across its dome (x0, x1, width), and the eyepiece's lens on the hood's front.
HATCH_BAR = (476.0, 530.0, 6.0)
EYEPIECE = {'c': (507.0, 138.0), 'r': 8.0}

# Letters: (text, (x, y) centre, cap height mm, rotation degrees anticlockwise, surface z key, material). The photographed
# set's words (r111_photos), checked against fig. 4 (r111_to).
TEXT = [
    ('ВНИМАНИЕ!', (69.0, 38.0), 3.6, 0, 'plate', 'Paint_Letters'),
    ('ПРИ РАБОТЕ ДВУХ Р/СТ', (69.0, 45.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('НА ОДНУ ИЛИ НА БЛИЗКО', (69.0, 50.5), 2.6, 0, 'plate', 'Paint_Letters'),
    ('РАСПОЛОЖЕННЫЕ АНТЕННЫ', (69.0, 56.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('ЗАПРЕЩАЕТСЯ РАБОТАТЬ', (69.0, 61.5), 2.6, 0, 'plate', 'Paint_Letters'),
    ('ПРИ РАЗНОСЕ ЧАСТОТ', (69.0, 67.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('МЕНЕЕ 10%', (69.0, 72.5), 2.6, 0, 'plate', 'Paint_Letters'),
    ('САУ', (158.0, 48.0), 3.0, 0, 'plate', 'Paint_Letters'),
    ('УМ', (275.6, 100.0), 3.0, 0, 'plate', 'Paint_Letters'),
    ('САУ', (307.0, 100.0), 3.0, 0, 'plate', 'Paint_Letters'),
    ('ВЫЗОВ', (356.0, 56.5), 2.8, 0, 'plate', 'Paint_Letters'),
    ('ВЫЗОВ', (385.0, 21.0), 2.8, 0, 'plate', 'Paint_Letters'),
    ('ПШ', (370.5, 78.0), 2.8, -90, 'frame', 'Paint_Letters'),
    ('ПРД.  УРОВНИ  ПР.', (370.0, 139.5), 3.0, 0, 'plate', 'Paint_Letters'),
    ('ПОДГОТОВ. ЧАСТОТ.', (432.5, 82.0), 2.6, -90, 'column', 'Paint_Letters'),
    ('МОЩНОСТЬ', (213.0, 134.0), 3.0, 0, 'plate', 'Paint_Letters'),
    ('ДЕЖ.', (192.0, 139.0), 2.6, 0, 'plate', 'Paint_Letters'), ('ПР.', (192.0, 144.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('ОТКЛ.', (186.0, 154.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('1%', (205.0, 140.0), 2.6, 0, 'plate', 'Paint_Letters'), ('20%', (219.0, 140.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('100%', (232.5, 140.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('ПР.УР.', (249.0, 140.0), 2.6, 0, 'plate', 'Paint_Letters'), ('ПРД.УР.', (284.0, 139.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('НАСТР.', (299.0, 155.0), 2.6, 0, 'plate', 'Paint_Letters'), ('БУМ', (299.0, 160.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('НАСТ.', (299.0, 170.0), 2.6, 0, 'plate', 'Paint_Letters'), ('САУ', (299.0, 175.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('БОРТ.', (299.0, 185.0), 2.6, 0, 'plate', 'Paint_Letters'), ('СЕТЬ', (299.0, 190.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('РЕЖ.РАБОТЫ', (210.0, 210.5), 3.0, 0, 'plate', 'Paint_Letters'),
    ('ИНДИКАТОР', (263.0, 210.5), 3.0, 0, 'plate', 'Paint_Letters'),
    ('РУЧНАЯ', (146.0, 221.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('НАСТРОЙКА УМ', (146.0, 226.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('ВНЕШ.', (330.0, 147.0), 2.4, 0, 'plate', 'Paint_Letters'), ('УПР.', (330.0, 151.5), 2.4, 0, 'plate', 'Paint_Letters'),
    ('ОЖИД.', (332.0, 157.0), 2.4, 0, 'plate', 'Paint_Letters'),
    ('ПРМ.', (330.0, 172.0), 2.4, 0, 'plate', 'Paint_Letters'),
    ('ПРД.', (330.0, 186.0), 2.4, 0, 'plate', 'Paint_Letters'),
    ('АВТОМ.', (333.0, 199.0), 2.4, 0, 'plate', 'Paint_Letters'), ('РЕТР.', (333.0, 203.5), 2.4, 0, 'plate', 'Paint_Letters'),
    ('РУЧ. РЕТР.', (321.0, 176.0), 2.4, 90, 'plate', 'Paint_Letters'),
    ('СЛУЖ.', (366.0, 205.0), 2.4, 0, 'plate', 'Paint_Letters'), ('СВЯЗЬ', (366.0, 209.5), 2.4, 0, 'plate', 'Paint_Letters'),
    ('800 ГЦ', (402.0, 171.5), 2.6, 0, 'plate', 'Paint_Letters'),
    ('ТЛФ.', (400.0, 186.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('ДИСТ.', (401.0, 198.0), 2.4, 0, 'plate', 'Paint_Letters'), ('УПР.', (401.0, 202.5), 2.4, 0, 'plate', 'Paint_Letters'),
    ('ВИД РАБОТЫ', (366.0, 216.5), 3.0, 0, 'plate', 'Paint_Letters'),
    ('ДИСКР.', (449.0, 140.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('КВ.КАЛИБР.', (507.0, 25.0), 2.6, 0, 'plate', 'Paint_Letters'), ('250', (507.0, 30.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('25', (500.0, 36.5), 2.6, 0, 'plate', 'Paint_Letters'),
    ('ДИАПАЗОН', (548.0, 25.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('20-36', (553.0, 30.5), 2.6, 0, 'plate', 'Paint_Letters'), ('36-52', (563.0, 37.0), 2.6, 0, 'plate', 'Paint_Letters'),
    ('20-36 МГЦ', (561.0, 85.0), 2.2, 0, 'microtoggles', 'Paint_Letters'),
    ('ДИАПАЗОН', (561.0, 135.0), 2.4, -90, 'microtoggles', 'Paint_Letters'),
    ('36-52 МГЦ', (561.0, 182.0), 2.2, 0, 'microtoggles', 'Paint_Letters'),
    ('УСТАН.', (566.0, 208.0), 3.0, 0, 'plate', 'Paint_Letters'), ('ЗПЧ', (566.0, 214.0), 3.0, 0, 'plate', 'Paint_Letters'),
    ('КОРР.', (447.0, 238.0), 2.4, 0, 'plate', 'Paint_Letters'), ('ПРД.', (447.0, 242.5), 2.4, 0, 'plate', 'Paint_Letters'),
    ('УСТАНОВКА', (531.0, 286.5), 3.0, 0, 'subpanel', 'Paint_Letters'),
    ('ЧАСТОТЫ', (531.0, 291.0), 3.0, 0, 'subpanel', 'Paint_Letters'),
    ('НОМЕР ЗПЧ', (491.0, 304.5), 3.0, 0, 'plate', 'Paint_Letters'),
    ('ВНИМАНИЕ, ПРОТИВНИК ПОДСЛУШИВАЕТ!', (252.0, 235.5), 4.0, 0, 'plate', 'Paint_Red'),
    ('БП.', (106.0, 282.0), 3.0, 0, 'lower_strip', 'Paint_Letters'),
    ('Р-111', (144.7, 271.0), 3.4, 0, 'nameplate', 'Paint_Letters_White'),
    ('ТЛК.', (215.6, 252.5), 3.0, 0, 'lower_strip', 'Paint_Letters'),
    ('ОТКЛ.', (215.6, 293.0), 3.0, 0, 'lower_strip', 'Paint_Letters'),
    ('АВТ.', (276.7, 299.0), 2.8, 0, 'lower_strip', 'Paint_Letters'), ('САУ.', (276.7, 304.0), 2.8, 0, 'lower_strip', 'Paint_Letters'),
    ('ПУ', (333.0, 272.0), 3.0, 0, 'lower_strip', 'Paint_Letters'),
    ('МТГ', (431.0, 292.0), 2.8, 0, 'lower_strip', 'Paint_Letters'),
    ('- ЛИНИЯ +', (375.0, 312.5), 2.4, 0, 'lower_strip', 'Paint_Letters'),
]

# Where the cabin's controls, lamps and displays stand (x, y mm), and on which surface (z key) when not the plate.
CONTROL_MM = {
    'radio.fug3.mode': (213.1, 178.1), 'radio.fug3.indicator': (262.0, 176.9), 'radio.fug3.work': (365.5, 174.5),
    'radio.fug3.tx_level': (352.0, 114.5), 'radio.fug3.rx_level': (388.4, 114.5),
    'radio.fug3.um': (275.6, 77.5), 'radio.fug3.sau': (307.0, 77.5), 'radio.fug3.call': (356.0, 78.0),
    'radio.fug3.squelch': (391.0, 78.0),
    'radio.fug3.zpch_pl': (421.0, 34.5), 'radio.fug3.zpch_1': (421.0, 58.0), 'radio.fug3.zpch_2': (421.0, 82.0),
    'radio.fug3.zpch_3': (421.0, 105.5), 'radio.fug3.zpch_4': (421.0, 131.0),
    'radio.fug3.subband': (531.5, 54.6), 'radio.fug3.dial': (530.7, 255.0), 'radio.fug3.tlk': (215.6, 272.7),
}
CONTROL_Z = {'radio.fug3.tlk': 'lower_strip', 'radio.fug3.um': 'frame', 'radio.fug3.sau': 'frame', 'radio.fug3.call': 'frame',
             'radio.fug3.squelch': 'frame', 'radio.fug3.zpch_pl': 'column', 'radio.fug3.zpch_1': 'column',
             'radio.fug3.zpch_2': 'column', 'radio.fug3.zpch_3': 'column', 'radio.fug3.zpch_4': 'column',
             'radio.fug3.tx_level': 'column', 'radio.fug3.rx_level': 'column', 'radio.fug3.dial': 'subpanel'}
# The cabin's lamps: the lenses' halves and the preset window's digits (SAM3D: the window's one digit as four).
LAMP_MM = {
    'protect': (143.0, 80.0), 'tuning_um': (271.0, 38.0), 'tuning_sau': (303.0, 38.0),
    'call_radio': (370.0, 38.0), 'call_line': (401.0, 38.0), 'rx': (462.0, 39.0), 'tx': (490.0, 39.0),
    'zpch_1': (494.5, 270.5), 'zpch_2': (498.2, 270.5), 'zpch_3': (494.5, 278.5), 'zpch_4': (498.2, 278.5),
}
# The meter's face (the cabin's meter display), (x0, y0, x1, y1).
METER_FACE = (207.0, 38.0, 249.0, 89.0)


def rect_m(r):
    """A mm rectangle (x0, y0, x1, y1) -> (a0, b0, a1, b1) metres, a0 < a1, b0 < b1."""
    x0, y0, x1, y1 = r
    a0, b1 = to_m(x0, y0)
    a1, b0 = to_m(x1, y1)
    return (a0, b0, a1, b1)


def surface_z(key):
    """A z key's height (mm): the Z table's, or a region's top."""
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
    """A region's outline in mm (anticlockwise as drawn: the order doesn't matter to the tests)."""
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
