"""The PU-12M's fighting compartment traced into metres (docs/MODELLING_FROM_PHOTOS.md; docs/PU12_DEVELOPMENT.md P6).

No bpy: plain data for the interior's Blender model (build_pu12m_interior.py), the cabin layout (tools/pu12m_cabin.py
writes data/cabins/pu12m.json from it) and the tests (tools/tests/test_pu12m_interior_trace.py).

How it was traced:
* The plan: the ČSLA manual's equipment plan (csla_pu12_manual, "Schéma rozmístění jednotlivých zařízení", 53 numbered
  items; the valka_pu12 scan, 2087 x 1395 px). It is drawn with the bow to the left and the vehicle's right at the top.
  Scale from its hull outline: the bow's edge at x 242 px and the stern at 1928 px are the published length (7.22 m,
  4.282 mm per px along); the chine lines at y 154 and 780 px the published width (2.825 m, 4.513 mm per px across),
  as the exterior trace (pu12m_trace.py) scales it. Every item's rectangle or circle was read on a 10 px grid (about
  +-5 px: 2-3 cm); which number is which from the labels' leader lines.
* Heights: the plan has none. From the ČSLA workplace drawings (the battery commander's, the squad leader's and driver's,
  the radio operator's; perspective sketches) and the veteran's photographs (shilka_narod: the IT-45 with the ДХ keypad
  at its left and the ФР block at its right, the grid glass over the tube, the GAK behind the driver), scaled against
  the seats and the hull: about 15 %, SAM3D's estimates unless noted.
* The hull's inside: the exterior trace's stations 3 cm in (plate and lining, an estimate), the floor at 0.62 m (an
  estimate: the hull's bottom at 0.475 m and the floor plates over the drive line). Below the knee (1.25 m) the inside is
  only 1.9 m wide; above it the sides step out over the wheels to the chine (1.39 m) and slope in to the roof. Things the
  plan draws farther out than the lower sides stand on that ledge, at 1.33 m or higher.
* Where the plan and the hull disagree by a few centimetres (the plan is a view from above, drawn by hand), an item is
  moved in to clear the hull; each such move is noted.
* Who sits where behind the front seats, the IT boards, the air-situation map, the GAK's stand and the colours: the NVA
  technical catalogue's photographs and captions of the PU-12 and PU-12M (rwd_mb3: "Platz des Kommandeurs/Operators" at
  the tube, "Platz des Planzeichners" beside it, the radio operator's; the IT-2M over the commander's workplace; the GAK
  on a stand over its PT-200 converter; the lining cream, the blocks blue-grey, the IT-45, the ДХ and the IT-2M light
  grey). docs/research/PU12_VISUALS_2026-10.md.

Frame: the exterior trace's, X forward from the middle of the wheelbase, Y to the left, Z up from the ground; metres. The
cabin layout's body frame is x right = -Y, y forward = X, z up (to_body).
"""
import math

import pu12m_trace as T

# ---------------------------------------------------------------------------------------------- the plan's scale

PLAN_BOW_PX = 242.0
PLAN_STERN_PX = 1928.0
PLAN_CENTRE_PY = 467.0
PLAN_CHINE_PY = (154.0, 780.0)
SX = T.LENGTH / (PLAN_STERN_PX - PLAN_BOW_PX)            # m per px along the hull
SY = T.WIDTH / (PLAN_CHINE_PY[1] - PLAN_CHINE_PY[0])     # m per px across


def px_x(px):
    """The plan's x (px, bow to the left) to X (m, forward)."""
    return T.BOW_X - (px - PLAN_BOW_PX) * SX


def py_y(py):
    """The plan's y (px, the vehicle's right at the top) to Y (m, to the left)."""
    return (py - PLAN_CENTRE_PY) * SY


def to_body(x, y, z):
    """Trace (X forward, Y left, Z up) to the cabin's body frame (x right, y forward, z up)."""
    return (-y, x, z)


# ---------------------------------------------------------------------------------------------- the hull's inside

THICK = 0.03                  # plate and lining (estimate)
FLOOR_Z = 0.62                # estimate
ROOF_IN_Z = T.ROOF_Z - THICK
REAR_X = px_x(1555)           # the engine compartment's front bulkhead (the plan's line at x 1555 px): -1.89 m
FRONT_X = px_x(396)           # the bow's inside at the floor plates' front (x 396 px): 3.07 m


def section(x):
    """The hull's outside section at X: [(z, half-width)] bottom, knee, chine, top, between the exterior's stations."""
    st = T.STATIONS
    if x <= st[0][0]:
        return list(st[0][1:])
    for (x0, *a), (x1, *b) in zip(st, st[1:]):
        if x0 <= x <= x1:
            f = (x - x0) / (x1 - x0) if x1 > x0 else 0.0
            return [(pa[0] + f * (pb[0] - pa[0]), pa[1] + f * (pb[1] - pa[1])) for pa, pb in zip(a, b)]
    return list(st[-1][1:])


def floor_z(x):
    """The floor (or the lower bow plate rising in front of it) at X."""
    return max(FLOOR_Z, section(x)[0][0] + THICK)


def ceiling_z(x):
    """The roof (or the upper bow plate coming down in front) at X."""
    return min(ROOF_IN_Z, section(x)[-1][0] - THICK)


def half_width(x, z):
    """The inside's half-width at X and height Z, or None outside the floor-to-roof range there."""
    if z < floor_z(x) - 1e-9 or z > ceiling_z(x) + 1e-9:
        return None
    pts = section(x)
    if z <= pts[0][0]:
        return pts[0][1] - THICK
    for (z0, w0), (z1, w1) in zip(pts, pts[1:]):
        if z0 <= z <= z1:
            return w0 + (w1 - w0) * ((z - z0) / (z1 - z0) if z1 > z0 else 1.0) - THICK
    return pts[-1][1] - THICK


def inside(x, y, z, margin=0.0):
    """Whether a point is inside the fighting compartment (margin metres clear of its surfaces)."""
    if not REAR_X + margin <= x <= FRONT_X + 0.7:
        return False
    w = half_width(x, z)
    return w is not None and abs(y) <= w - margin and floor_z(x) + margin - 1e-9 <= z <= ceiling_z(x) - margin + 1e-9


def ledge_z(y):
    """How high something must stand to reach out to |Y| on the ledge over the wheels (amidships)."""
    knee_z, knee_w = T.KNEE_Z, T.KNEE_HALF_W - THICK
    chine_z, chine_w = 1.39, T.HALF_W - THICK
    if abs(y) <= knee_w:
        return FLOOR_Z
    return knee_z + (abs(y) - knee_w) / (chine_w - knee_w) * (chine_z - knee_z)


# ---------------------------------------------------------------------------------------------- items

ITEMS = []


def _add(iid, no, label, x0, x1, y0, y1, z0, z1, shape='box', mat='Paint_Equipment', note=''):
    lo_x, hi_x = sorted((x0, x1))
    lo_y, hi_y = sorted((y0, y1))
    ITEMS.append({'id': iid, 'no': no, 'label': label, 'x0': lo_x, 'x1': hi_x, 'y0': lo_y, 'y1': hi_y,
                  'z0': z0, 'z1': z1, 'shape': shape, 'mat': mat, 'note': note})


def rect(iid, no, label, px0, px1, py0, py1, z0, z1, dy=0.0, dx=0.0, **kw):
    """An item from its plan rectangle (px), moved dx, dy (m) where the hull needs it."""
    _add(iid, no, label, px_x(px0) + dx, px_x(px1) + dx, py_y(py0) + dy, py_y(py1) + dy, z0, z1, **kw)


def circle(iid, no, label, cx, cy, r_px, z0, z1, dy=0.0, dx=0.0, **kw):
    """A round item (a vertical cylinder) from its plan circle (px)."""
    x, y, r = px_x(cx) + dx, py_y(cy) + dy, r_px * 0.5 * (SX + SY)
    _add(iid, no, label, x - r, x + r, y - r, y + r, z0, z1, shape='cylinder', **kw)


def item(iid):
    for it in ITEMS:
        if it['id'] == iid:
            return it
    raise KeyError(iid)


def centre(it):
    return (0.5 * (it['x0'] + it['x1']), 0.5 * (it['y0'] + it['y1']), 0.5 * (it['z0'] + it['z1']))


# Seats. The cushions at GOST's seat height above the floor (data/crew/body.json: 0.43 m, gost_12_2_032_78); the bench's
# three places across the compartment facing forward, the cushions deepened to 0.31 m (the plan draws 0.24 m), their backs
# where the plan has them.
SEAT_Z = FLOOR_Z + 0.43
rect('seat_squad_leader', None, "Squad leader's seat", 543, 648, 352, 448, FLOOR_Z, SEAT_Z + 0.55, mat='Vinyl_Olive',
     note='plan 543-648 x 338-462 px with its arm; 0.43 m wide here')
rect('seat_driver', None, "Driver's seat", 548, 648, 512, 602, FLOOR_Z, SEAT_Z + 0.55, mat='Vinyl_Olive')
BENCH = {'x_front': px_x(1288) + 0.05, 'x_back': px_x(1350), 'back_x': (px_x(1385), px_x(1350)),
         'y': (py_y(338), py_y(600)), 'places': [(py_y(338), py_y(417)), (py_y(422), py_y(511)), (py_y(511), py_y(600))]}
_add('bench', None, 'Bench (three places)', BENCH['x_back'], BENCH['x_front'], BENCH['y'][0], BENCH['y'][1],
     FLOOR_Z, SEAT_Z, mat='Vinyl_Olive', note='plan cushions 1288-1343 px, backs 1350-1385 px, across 338-600 px')
_add('bench_back', None, 'Bench back', BENCH['back_x'][0], BENCH['back_x'][1], BENCH['y'][0], BENCH['y'][1],
     SEAT_Z, SEAT_Z + 0.50, mat='Vinyl_Olive')

# The squad leader's place (front right): csla_pu12_manual's squad leader and driver drawing.
rect('kp4', 2, 'KP-4 coordinate and course plotter', 437, 513, 300, 405, 1.36, 1.58,
     note='on a bracket ahead of the squad leader; the drawing: a drum unit at the height of his chest')
circle('vo', 1, 'VO orientation telescope', 467, 327, 18, 1.58, 1.84,
       note='from the KP-4 up to the bow plate')
rect('it1', 4, 'IT-1 status board', 530, 585, 262, 297, 1.30, 1.58,
     note='a tall lamp panel at his right (the drawing: five columns of lamps)')
rect('dtp', 5, 'DTP coordinate sensor', 560, 617, 305, 330, FLOOR_Z + 0.08, FLOOR_Z + 0.30)
rect('pvs2', 6, 'PVS-2 intercom box', 600, 645, 265, 297, 1.18, 1.34, dy=0.02, note='moved in 2 cm to clear the lower side')
rect('gak_panel', 7, "GAK's control panel", 630, 688, 205, 228, 1.45, 1.62)
rect('r123_fug1', 8, 'R-123M (FuG-1, air picture net)', 665, 745, 237, 287, 1.50, 1.744, shape='r123m',
     note="the set faces inboard; the plan's 0.34 m is its width less the mounting")
rect('bp_fug1', 9, 'R-123M power unit', 665, 745, 245, 287, 1.30, 1.46,
     note='the plan draws it as a strip beside the set; SAM3D puts it under the set')
circle('water_1', 52, 'Water tank', 675, 318, 9, FLOOR_Z, FLOOR_Z + 0.32, mat='Metal_Dark')
circle('water_2', 52, 'Water tank', 710, 318, 9, FLOOR_Z, FLOOR_Z + 0.32, mat='Metal_Dark')
rect('telecode_tx', 11, 'R telecode transmitter', 835, 995, 308, 395, FLOOR_Z, 1.30, mat='Paint_BlueGrey',
     note='a cabinet (the drawing: a tall cabinet with doors at the right, behind the mast)')
rect('kv', 12, 'KV cable entries', 958, 1025, 208, 248, 1.36, 1.50, dy=0.012,
     note="on the ledge; the plan's ellipse as a box")

# The driver's place (front left).
rect('pvs3', 29, 'PVS-3 intercom box', 595, 652, 667, 708, 1.36, 1.52)
circle('gak', 30, 'GAK gyro direction indicator', 752, 640, 34, 0.84, 1.30, dy=-0.05, mat='Paint_LightGrey',
       note='the gyro on a stand behind the driver, its converter under it (rwd_mb3 photographs; shilka_narod); moved in 5 cm '
            'to clear the lower side')
rect('pt200', 31, 'PT-200C-III converter', 750, 818, 583, 620, FLOOR_Z, 0.80,
     note='under the GAK on its stand (rwd_mb3: "hinter dem Fahrersitz", the converter lying under the gyro)')
rect('scv', 32, 'ŠČV input board', 788, 858, 666, 705, 1.52, 1.74,
     note="on the sloping side above the RK-2 (the plan draws them overlapping); the plan's ellipse as a box")
rect('rk2', 33, 'RK-2 junction box', 835, 895, 683, 718, 1.36, 1.50)
rect('telecode_rx', 34, 'P telecode receiver', 850, 990, 540, 627, FLOOR_Z, 1.30, mat='Paint_BlueGrey')
rect('baf', 35, 'BAF-M antenna filters', 940, 1000, 683, 718, 1.36, 1.50)
rect('ssp', 49, 'SSP follow-up drive', 990, 1010, 540, 627, FLOOR_Z, 1.20, mat='Paint_BlueGrey')
# The S-23 data transmission set of the PU-12M, behind the driver over the GAK (rwd_mb3: "hinter dem Fahrersitz, PU-12M,
# 7 Datenübertragungsgerät S-23", a black box with a row of coloured lamps). Not in the ČSLA plan: its place is SAM3D's.
_add('s23', None, 'S-23 data transmission set', 1.40, 1.72, 0.76, 0.92, 1.42, 1.62, mat='Bakelite_Black',
     note='SAM3D place, after the rwd_mb3 photograph')

# The battery commander's workplace (right side, middle): the IT-45 at the front of the desk, facing back to the bench.
rect('rtk1', 16, 'RTK-1 radiotelephone concentrator', 1018, 1040, 335, 428, 1.80, 1.98, mat='Paint_BlueGrey',
     note='above the front of the desk (the drawing: top left), over the IT-45 as the commander sees it, facing back')
rect('it45', 19, 'IT-45 indicator (with the UO amplifiers, 14, under it)', 1060, 1175, 293, 415, 1.10, 1.78,
     mat='Paint_LightGrey', note="the tube block, light grey (rwd_mb3 photograph); the plan's D (1175-1200 px) is the "
                                 "DT-2 marker's housing at its foot")
rect('dch', 15, 'ДХ characteristics keypad', 1125, 1190, 418, 450, 1.20, 1.64, mat='Paint_LightGrey',
     note='left of the tube (shilka_narod, rwd_mb3 photographs), its face in the plane of the screen')
rect('fr_block', 13, 'ФР block and the S synchronisation block (13)', 1085, 1175, 243, 293, 1.30, 1.70, mat='Paint_BlueGrey',
     note='right of the tube (shilka_narod photograph); the plan draws S dashed under it')
rect('desk', None, "The commander's desk", 1175, 1360, 190, 318, 1.34, 1.37, dy=0.05, mat='Paint_Panel',
     note='on the ledge (its top 0.35 m above the bench); moved in 5 cm to clear the side')
rect('rk3', 17, 'RK-3 junction box', 1075, 1160, 202, 228, 1.36, 1.48, dy=0.012, note='under the side hatch')
rect('it2', 18, 'IT-2M status board', 1190, 1245, 203, 228, 1.66, 1.88, dy=0.08, mat='Paint_LightGrey',
     note='high over the desk at the commander\'s right (rwd_mb3: the IT-2M over the commander\'s workplace); moved in 8 cm '
          'to clear the sloping side')
circle('di', 20, 'DI information marker', 1243, 276, 22, 1.37, 1.43, note="in the desk top (the plan's circle is 31 px)")
rect('fan_right', 50, 'Fan', 1265, 1290, 196, 218, 1.86, 1.96, dy=0.15, note='high on the sloping wall, above the ŠČK')
rect('skc', 36, 'ŠČK commander’s panel', 1258, 1318, 225, 250, 1.64, 1.84)
rect('r123_fug4', 22, 'R-123M (FuG-4, battery command net)', 1290, 1360, 202, 250, 1.375, 1.619, shape='r123m', dx=-0.04,
     note='on the desk, facing inboard')
rect('bp_fug4', 23, 'R-123M power unit', 1300, 1355, 268, 318, FLOOR_Z, FLOOR_Z + 0.26, dy=0.012, note='under the desk')
# The air-situation map (the plan's item 24, "planšet vzdušné situace"): a square plexiglass board on a hinge (rwd_mb3's
# photograph of the Luftlagekarte), drawn in the plan folded edge-on, 0.40 m long, on the centreline before the middle
# place (1218-1311 x 468-482 px). SAM3D shows it swung open to face the plotter, hanging from a bracket at the roof.
MAP_BOARD = {'x': px_x(1218) - 0.003, 'y0': -0.18, 'y1': 0.16, 'z0': 1.32, 'z1': 1.70}
_add('map_board', 24, 'Air-situation map board', MAP_BOARD['x'] - 0.018, MAP_BOARD['x'], MAP_BOARD['y0'], MAP_BOARD['y1'],
     MAP_BOARD['z0'], MAP_BOARD['z1'], mat='Metal_Steel', note="open, facing the plotter; the plan draws it folded")
rect('bp3', 21, 'BP-3 power unit', 1382, 1430, 262, 285, 1.05, 1.30, note='on a bracket on the side, beside the R-407')
rect('r407', 25, 'R-407 radio (FuG-5, telecode)', 1375, 1440, 285, 327, FLOOR_Z, FLOOR_Z + 0.52,
     note='behind the operator, its face inboard (the drawing: lower right)')
rect('sau', 26, 'SAU antenna tuner', 1386, 1460, 355, 430, FLOOR_Z, FLOOR_Z + 0.28,
     note="under the bench's back")

# The radio operator's place (left side, rear).
rect('r111', 41, 'R-111 radio (FuG-3, regiment net)', 1085, 1188, 505, 640, 0.86, 1.21,
     note='on a stand, facing the bench (the drawing: right foreground, the R-123M on top)')
rect('r111_stand', None, 'R-111 stand', 1085, 1188, 505, 640, FLOOR_Z, 0.86, mat='Paint_Rack')
rect('r111_power', 37, 'R-111 power unit', 1072, 1085, 505, 605, FLOOR_Z, 0.95)
rect('r123_fug2', 40, 'R-123M (FuG-2, covered unit net)', 1150, 1188, 515, 620, 1.22, 1.464, shape='r123m',
     note='on top of the R-111, facing the bench')
rect('bp_fug2', 39, 'R-123M power unit', 1095, 1165, 665, 720, 1.46, 1.62)
rect('rk1', 38, 'RK-1 junction box', 1078, 1135, 650, 688, 1.30, 1.44, dy=-0.03)
rect('pvs1', 42, 'PVS-1 intercom box', 1190, 1240, 660, 708, 1.50, 1.66)
rect('vs27', 43, 'VS-27 rectifier', 1255, 1320, 680, 722, 1.36, 1.52)
rect('scr', 44, 'ŠČR distribution board', 1265, 1352, 645, 700, 1.55, 1.80)
rect('fp', 45, 'FP filter', 1355, 1405, 608, 660, FLOOR_Z, FLOOR_Z + 0.23)
rect('battery_1', 46, '6ST-75 battery', 1395, 1462, 650, 718, 1.36, 1.60, mat='Rubber_Black', note='on the ledge')
rect('battery_2', 46, '6ST-75 battery', 1466, 1530, 650, 718, 1.36, 1.60, mat='Rubber_Black', note='on the ledge')
rect('rr361', 47, 'RR-361A voltage regulator', 1400, 1500, 690, 735, 1.64, 1.80, dy=-0.06,
     note='on the wall above the batteries (the drawing: upper left)')
rect('rear_shelf', None, 'Shelf', 1430, 1552, 460, 638, FLOOR_Z, 0.95, mat='Paint_Rack')
rect('bp48', 48, 'BP power unit', 1468, 1532, 500, 605, 0.95, 1.25)
rect('table51', 51, "Radio operator's table", 1190, 1240, 505, 555, 1.26, 1.29, mat='Paint_Panel')
rect('hours_1', 53, 'Hour counter', 1160, 1185, 718, 733, 1.38, 1.44, dy=-0.02)
rect('hours_2', 53, 'Hour counter', 1370, 1380, 708, 725, 1.38, 1.44)
rect('fan_left', 50, 'Fan', 1190, 1210, 712, 728, 1.70, 1.84, dy=-0.05)

# The mast's foot: the fixed section stands on the floor (pu12m_trace.MAST_FOOT_Z) and goes up through the roof; its
# winch on the inboard side (csla_pu12_manual, the mast drawings: a hand winch on the fixed section's lower part).
MAST_R = T.MAST_SECTIONS[0][0]
_add('mast', 10, 'Telescopic mast (fixed section)', T.MAST_X - MAST_R, T.MAST_X + MAST_R, T.MAST_Y - MAST_R,
     T.MAST_Y + MAST_R, T.MAST_FOOT_Z, ROOF_IN_Z, shape='cylinder', mat='Metal_Steel')
WINCH = {'x': T.MAST_X, 'y': T.MAST_Y + MAST_R + 0.07, 'z': 1.08, 'size': (0.16, 0.14, 0.20)}  # x, y, z sizes
_add('winch', 10, 'Mast winch', WINCH['x'] - 0.08, WINCH['x'] + 0.08, WINCH['y'] - 0.07, WINCH['y'] + 0.07,
     WINCH['z'] - 0.10, WINCH['z'] + 0.10, mat='Paint_Rack')

# The driver's controls (the BTR-60PB's, from the drawing): the wheel on its column, the dash with his instruments.
DRIVER_Y = py_y(557)
WHEEL = {'x': 2.72, 'y': DRIVER_Y, 'z': 1.36, 'r': 0.22, 'tilt_deg': 50}   # its plane 50 degrees from flat (estimate)
_add('dash', None, "Driver's instrument panel", 2.86, 3.02, DRIVER_Y - 0.30, DRIVER_Y + 0.28, 1.30, 1.62,
     mat='Paint_Interior', note='the BTR-60PB dash under the windscreen (the drawing)')

# Where the PAB-2 aiming circle is stowed: a case on the ledge behind the squad leader (SAM3D; not in the plan).
_add('pab2_case', None, 'PAB-2 aiming circle case', 1.18, 1.48, -1.10, -0.95, 1.36, 1.58, mat='Paint_Rack')

# Lamps on the roof (SAM3D: the BTR-60PB's dome lamps; positions an estimate).
DOME_LAMPS = [(2.05, 0.0), (0.60, 0.0), (-0.95, 0.0)]

# Two windscreens with their armoured covers closed, in the upper bow plate before the front seats (the drawing).
WINDSCREENS = [(2.62, py_y(557)), (2.62, py_y(400))]

# Paint (rwd_mb3's colour photographs): the ASPD-U's cabinets and blocks, the power boards and the junction boxes
# blue-grey; the IT-45, the ДХ, the IT-2M and the GAK light grey; the radios hammer grey; the lining cream.
BLUE_GREY = {'fr_block', 'telecode_tx', 'telecode_rx', 'ssp', 'kv', 'rk1', 'rk2', 'rk3', 'bp_fug1', 'bp_fug2', 'bp_fug4',
             'bp3', 'bp48', 'vs27', 'scr', 'pvs1', 'pvs2', 'pvs3', 'baf', 'rr361', 'gak_panel', 'it1', 'skc', 'dtp', 'kp4', 'pt200',
             'fp', 'r111_power', 'sau', 'scv', 'rtk1'}
for _it in ITEMS:
    if _it['id'] in BLUE_GREY:
        _it['mat'] = 'Paint_BlueGrey'
    elif _it['id'] in ('r111', 'r407'):
        _it['mat'] = 'Paint_Radio'

# The side of each item that carries its front (its panel, its controls): toward the person who works it.
FACES = {
    'kp4': 'rear', 'it1': 'left', 'dtp': 'left', 'pvs2': 'left', 'gak_panel': 'left', 'r123_fug1': 'left',
    'telecode_tx': 'left', 'kv': 'left', 'pvs3': 'right', 'gak': 'right', 'scv': 'right', 'rk2': 'right',
    'telecode_rx': 'right', 'baf': 'right', 'rtk1': 'rear', 'it45': 'rear', 'dch': 'rear', 'fr_block': 'rear',
    'rk3': 'left', 'it2': 'left', 'skc': 'left', 'r123_fug4': 'left', 'r407': 'left', 'r111': 'rear', 'r123_fug2': 'rear',
    'bp_fug2': 'right', 'rk1': 'right', 'pvs1': 'right', 'vs27': 'right', 'scr': 'right', 'fp': 'rear', 'rr361': 'right',
    'bp48': 'front', 'winch': 'left', 'dash': 'rear', 'pab2_case': 'left', 'map_board': 'rear', 's23': 'right', 'di': 'top',
}

# The R-123M's mesh (art/cabin_interior SM_RadioR123M, traced from photographs): its box, and where its front plate is.
R123M_SIZE = (0.481, 0.169, 0.244)    # width, depth, height
R123M_PLATE = (0.0655, 0.0055)        # the front plate's plane ahead of the box's centre, its centre above it


def _fit_r123m():
    """Stand each R-123M's real box at its plan place: its back against the plan's outer edge, centred along it."""
    for it in ITEMS:
        if it['shape'] != 'r123m':
            continue
        w, d, _ = R123M_SIZE
        face = FACES[it['id']]
        cx, cy = 0.5 * (it['x0'] + it['x1']), 0.5 * (it['y0'] + it['y1'])
        if face == 'left':        # on the right wall, facing inboard: the back at the plan's outer (right) edge
            it['y0'], it['y1'] = it['y0'] + 0.01, it['y0'] + 0.01 + d
            it['x0'], it['x1'] = cx - w / 2, cx + w / 2
        elif face == 'rear':      # facing back: the front at the plan's rear edge
            it['x0'], it['x1'] = it['x0'], it['x0'] + d
            it['y0'], it['y1'] = cy - w / 2, cy + w / 2


_fit_r123m()

# ---------------------------------------------------------------------------------------------- the stations

# Eye points: the body model's sitting eye above a cushion (data/crew/body.json: 0.79 m, so 1.22 m above the floor), over
# its back third. Yaw clockwise from forward (body frame), pitch up positive.
EYE_ABOVE_SEAT = 0.79
STATIONS = {
    'driver': {'eye': (2.16, py_y(557), SEAT_Z + EYE_ABOVE_SEAT), 'yaw': 0.0, 'pitch': -8.0},
    'squad_leader': {'eye': (2.16, py_y(400), SEAT_Z + EYE_ABOVE_SEAT), 'yaw': 0.0, 'pitch': -22.0},
    # rwd_mb3: the commander (or the operator) at the tube, the plotter beside him at the map (the ČSLA drawing calls the
    # tube's desk the battery commander's workplace).
    'battery_commander': {'eye': (BENCH['x_back'] + 0.10, 0.5 * sum(BENCH['places'][0]), SEAT_Z + EYE_ABOVE_SEAT),
                          'yaw': 9.0, 'pitch': -14.0},
    'operator': {'eye': (BENCH['x_back'] + 0.10, 0.5 * sum(BENCH['places'][1]), SEAT_Z + EYE_ABOVE_SEAT), 'yaw': 6.0,
                 'pitch': -16.0},
    'radio_operator': {'eye': (BENCH['x_back'] + 0.10, 0.5 * sum(BENCH['places'][2]), SEAT_Z + EYE_ABOVE_SEAT),
                       'yaw': -8.0, 'pitch': -16.0},
}

# ---------------------------------------------------------------------------------------------- the IT-45's screen

# The tube's face: in the plane of the block's back (towards the bench), centred in it (rwd_mb3's photograph of the PU-12M's
# workplace). Diameter 0.40 m: the photograph against the ДХ keypad beside it, 3 times its width (about 0.13 m), and the
# name: Soviet tubes are named for their diameter in cm, IT-45 a 45 cm tube with its bezel over the rim. Scaled, about 10 %.
_IT45 = item('it45')
IT45_SCREEN = {'x': _IT45['x0'], 'y': 0.5 * (_IT45['y0'] + _IT45['y1']), 'z': 1.43, 'd': 0.40}
