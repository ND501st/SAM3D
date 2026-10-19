"""Write data/cabins/pu12m.json, the PU-12M's cabin layout, from its interior trace (docs/PU12_DEVELOPMENT.md P6).

The geometry comes from tools/blender/pu12m_interior_trace.py (the ČSLA plan's items, the seats' eye points) and the
interior meshes' manifest (art/pu12m_interior/manifest.json, tools/blender/build_pu12m_interior.py): each panel stands on
the face of the item that carries it, so the panels and the meshes cannot drift apart. What is on the panels (controls,
lamps, screens, fixtures) is written here, with its sources. Positions of fixtures on a face are read off the photographs
(shilka_narod: the ДХ keypad, the ФР block, the RTK-1) where there are photographs, else SAM3D's arrangement.

    python tools/pu12m_cabin.py            # writes data/cabins/pu12m.json
    python tools/pu12m_cabin.py --check    # exit 1 if the file differs from what the trace gives
"""
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools' / 'blender'))
import pu12m_interior_trace as I  # noqa: E402

OUT = ROOT / 'data' / 'cabins' / 'pu12m.json'
MANIFEST = ROOT / 'art' / 'pu12m_interior' / 'manifest.json'
FACING = {'rear': 180.0, 'front': 0.0, 'left': -90.0, 'right': 90.0, 'top': 180.0}
# A panel's plate takes its item's paint (rwd_mb3's colour photographs: the blocks blue-grey, the IT-45, the ДХ and the IT-2M
# light grey, the S-23 black).
MAT_COLOR = {'Paint_BlueGrey': 'bluegrey', 'Paint_LightGrey': 'lightgrey', 'Bakelite_Black': 'black', 'Paint_Radio': 'grey'}
VISUALS = 'docs/research/PU12_VISUALS_2026-10.md'

PLAN = 'csla_pu12_manual (the equipment plan, item %s)'


def r4(v):
    return round(v + 0.0, 4)


def body(p):
    return [r4(v) for v in I.to_body(*p)]


class Layout:
    def __init__(self):
        self.stations, self.panels, self.controls, self.lamps = [], [], [], []
        self.displays, self.blocks, self.fixtures, self.shell = [], [], [], []
        self._frames = {}

    # -- panels -------------------------------------------------------------------------------------------------
    def panel(self, pid, label, station, iid, size, at=(0.0, 0.0), notes='', face=None, color=None):
        """A panel on an item's face: `at` (m) right and up from the face's centre, as the person at it sees it. A
        "top" face looks up, read from the bench (its up is forward)."""
        it = I.item(iid)
        face = face or I.FACES[iid]
        cx, cy, cz = I.centre(it)
        f = FACING[face]
        rx, ry = math.sin(math.radians(f)), math.cos(math.radians(f))   # the panel's right, trace X and Y
        tilt = 0
        if face == 'rear':
            cx = it['x0']
        elif face == 'front':
            cx = it['x1']
        elif face == 'left':
            cy = it['y1']
        elif face == 'right':
            cy = it['y0']
        if face == 'top':
            c = (cx + at[1], cy - at[0], it['z1'])
            tilt = 90
        else:
            c = (cx + rx * at[0], cy + ry * at[0], cz + at[1])
        p = {'id': pid, 'label': label, 'station': station, 'centre_m': body(c), 'facing_deg': f, 'tilt_deg': tilt,
             'size_m': [r4(size[0]), r4(size[1])]}
        color = color or MAT_COLOR.get(it['mat'])
        if color:
            p['color'] = color
        p['notes'] = notes or 'On the face of %s (%s).' % (it['label'], PLAN % it['no'] if it['no'] else 'sam3d')
        self.panels.append(p)
        return pid

    def r123m_panel(self, pid, label, station, iid):
        """An R-123M's front plate: a bare panel on the Osa cabin's traced mesh (its controls where the set's are)."""
        it = I.item(iid)
        face = I.FACES[iid]
        f = FACING[face]
        nx, ny = {'rear': (-1, 0), 'front': (1, 0), 'left': (0, 1), 'right': (0, -1)}[face]
        cx, cy, cz = I.centre(it)
        ahead, up = I.R123M_PLATE
        c = (cx + nx * ahead, cy + ny * ahead, cz + up)
        self.panels.append({'id': pid, 'label': label, 'station': station, 'centre_m': body(c), 'facing_deg': f,
                            'tilt_deg': 0, 'size_m': [0.406, 0.225], 'bare': True,
                            'notes': 'The front plate of the R-123M mesh (shell %s; traced from photographs for the Osa '
                                     'cabin, tools/blender/r123m_trace.py). The set stands where %s puts it.'
                                     % (iid, PLAN % it['no'])})
        self.shell.append({'id': iid, 'label': it['label'], 'kind': 'mesh', 'mesh': 'SM_RadioR123M',
                           'centre_m': body((cx, cy, cz)), 'size_m': list(I.R123M_SIZE), 'yaw_deg': f,
                           'source': PLAN % it['no'], 'confidence': 'medium', 'notes': it['note'] or None})
        if self.shell[-1]['notes'] is None:
            del self.shell[-1]['notes']
        return pid

    # -- items on panels ----------------------------------------------------------------------------------------
    def control(self, cid, panel, at, style=None, look=None, units_per_turn=None, watch=None):
        c = {'control': cid, 'panel': panel, 'at_m': [r4(at[0]), r4(at[1])]}
        if style:
            c['style'] = style
        if units_per_turn:
            c['units_per_turn'] = units_per_turn
        if watch:
            c['watch'] = watch
        if look:
            c['look'] = look
        self.controls.append(c)

    def lamp(self, lid, label, panel, at, color, lit_when, source, confidence='low', notes=None, look=None):
        lp = {'id': lid, 'label': label, 'panel': panel, 'at_m': [r4(at[0]), r4(at[1])], 'color': color,
              'lit_when': lit_when, 'source': source, 'confidence': confidence}
        if notes:
            lp['notes'] = notes
        if look:
            lp['look'] = look
        self.lamps.append(lp)

    def fixture(self, panel, at, kind, size, color=None, label=None, ring=None):
        f = {'panel': panel, 'at_m': [r4(at[0]), r4(at[1])], 'kind': kind,
             'size_m': [r4(size[0]), r4(size[1])] if isinstance(size, (list, tuple)) else r4(size)}
        if color:
            f['color'] = color
        if ring:
            f['ring'] = ring
        if label:
            f['label'] = label
        self.fixtures.append(f)

    def nameplate(self, panel, at, label, width=0.06):
        self.fixture(panel, at, 'nameplate', (width, 0.014), label=label)

    def photo(self, panel, size, extent, items):
        """Fixtures read off a photograph: extent (x0, y0, x1, y1) is the panel's outline in the photograph's pixels;
        each item (x, y, kind, size, colour, label) is placed at the same place on the panel."""
        x0, y0, x1, y1 = extent
        for x, y, kind, sz, color, label in items:
            u = ((x - x0) / (x1 - x0) - 0.5) * size[0]
            v = (0.5 - (y - y0) / (y1 - y0)) * size[1]
            self.fixture(panel, (u, v), kind, sz, color, label)

    def at_photo(self, size, extent, x, y):
        x0, y0, x1, y1 = extent
        return (((x - x0) / (x1 - x0) - 0.5) * size[0], (0.5 - (y - y0) / (y1 - y0)) * size[1])


def ready(sub):
    return [{'subsystem': sub, 'is': 'ready'}]


def warming(sub):
    return [{'subsystem': sub, 'is': 'starting'}]


def build():
    L = Layout()
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))

    # ---------------------------------------------------------------------------------------------- stations
    labels = {'driver': 'Driver', 'squad_leader': 'Squad leader', 'operator': 'Operator-plotter',
              'battery_commander': 'Battery commander', 'radio_operator': 'Radio operator'}
    notes = {
        'driver': 'Front left, at the wheel (csla_pu12_manual: the squad leader and driver drawing; the plan\'s seat at '
                  '548-648 x 512-602 px). The GAK and its converter behind him, the PVS-3 and the ŠČV input board on the '
                  'side at his left.',
        'squad_leader': 'Front right (the plan\'s seat at 543-648 x 338-462 px): the KP-4 and the VO telescope ahead, the '
                        'IT-1 board, the PVS-2 and the DTP at his right, the GAK\'s panel and his R-123M (FuG-1) on the '
                        'side behind them.',
        'battery_commander': 'The bench\'s right place, at the IT-45 (csla_pu12_manual: the battery commander\'s '
                             'workplace; rwd_mb3: "Platz des Kommandeurs/Operators"): the tube ahead, the ДХ keypad at its '
                             'left, the ФР block at its right, the DT-2 marker on the desk, the IT-2M over the '
                             'tube, the RTK-1, the ŠČK and the R-123M of the battery net (FuG-4) on the desk at his right, the R-407 '
                             'behind him. The bench: the plan\'s three places across the compartment facing forward '
                             '(1288-1385 x 338-600 px).',
        'operator': 'The bench\'s middle place, the plotter\'s (rwd_mb3: "Platz des Planzeichners"): the air-situation map '
                    'ahead of him (the plan\'s item 24), the IT-45 at his right front. The ФР block is the commander\'s; '
                    'the plotter\'s work is the map: he plots what the tube and the voice nets give.',
        'radio_operator': 'The bench\'s left place: the R-111 with the R-123M (FuG-2) on top ahead of him, his table '
                          'between, the power boards (ŠČR, VS-27) and the PVS-1 on the side at his left '
                          '(csla_pu12_manual: the radio operator\'s workplace drawing).',
    }
    for sid, s in I.STATIONS.items():
        L.stations.append({'id': sid, 'label': labels[sid], 'eye_m': body(s['eye']), 'yaw_deg': s['yaw'],
                           'pitch_deg': s['pitch'], 'notes': notes[sid] + ' Eye 0.79 m above the cushion, 1.22 m above the floor (data/crew/body.json).'})

    # ---------------------------------------------------------------------------------------------- the driver
    p = L.panel('driver_panel', "Driver's panel", 'driver', 'dash', (0.54, 0.28),
                notes="The BTR-60PB's dashboard under the windscreen (csla_pu12_manual, the squad leader and driver "
                      "drawing: the wheel on a panel of instruments). The post's switches on it are SAM3D's.")
    L.control('pwr.mass', p, (-0.22, -0.08), 'toggle')
    L.control('pwr.g290', p, (0.22, -0.08), 'toggle')
    L.lamp('net27_on', 'СЕТЬ 27 В', p, (-0.22, -0.02), 'green', ready('net_27v'),
           'valka_pu12 (AZS-50 battery switch, two 6ST-75 batteries); the lamp is sam3d', notes='Lit while the 27 V '
           'network is on.')
    L.lamp('g290_on', 'Г-290', p, (0.22, -0.02), 'amber', ready('g290'),
           'flak11 (PU12-0: the G-290B generator driven by the engine); the lamp is sam3d')
    for u, v, lab in ((-0.10, 0.07, 'СПИДОМЕТР'), (0.10, 0.07, 'ТАХОМЕТР'), (-0.03, 0.08, None), (0.03, 0.08, None)):
        L.fixture(p, (u, v), 'gauge', 0.07 if lab else 0.045, 'black', lab)
    for u in (-0.15, -0.12, 0.12, 0.15):
        L.fixture(p, (u, 0.05), 'lamp', 0.009, 'red')
    L.nameplate(p, (-0.22, 0.05), 'ПУ-12М', 0.07)
    p = L.panel('pvs3_panel', 'PVS-3 intercom box', 'driver', 'pvs3', (0.20, 0.13))
    L.control('intercom.pvs3', p, (0.05, -0.02), 'toggle')
    L.nameplate(p, (-0.04, 0.045), 'ПВС-3')
    L.fixture(p, (-0.05, -0.02), 'socket', 0.028, 'chrome')
    L.fixture(p, (0.05, 0.02), 'knob', 0.018, 'black')
    p = L.panel('scv_panel', 'ŠČV input board', 'driver', 'scv', (0.27, 0.19),
                notes='The input board (ЩВ; csla_pu12_manual item 32, valka_pu12): the 220 V supply\'s selector and the '
                      'AB-1 set\'s cable. SAM3D: the set\'s own start is worked from here too (the set stands outside on '
                      'the ground; the crewman walks to it).')
    L.control('pwr.input', p, (-0.07, 0.0), 'rotary', {'kind': 'pointer_knob', 'size_m': 0.045, 'color': 'black'})
    L.control('pwr.ab1_set', p, (0.06, 0.035), 'toggle', {'kind': 'socket', 'size_m': 0.034, 'color': 'chrome'})
    L.control('pwr.ab1_start', p, (0.09, -0.05), 'toggle')
    for u, sub, lab in ((-0.115, 'ab1_on_line', 'АГРЕГАТ'), (-0.07, 'g290_on_line', 'Г-290'), (-0.025, 'external_on_line', 'СЕТЬ')):
        L.lamp('scv_' + sub, lab, p, (u, 0.065), 'white', ready(sub),
               'valka_pu12 (ŠČV input board), ru_pu12_text (supply from the set, the G-290B or a 220 V network); the '
               'lamps are sam3d', look={'kind': 'legend', 'size_m': 0.024, 'label': lab})
    L.lamp('ab1_running', 'АГРЕГАТ РАБОТАЕТ', p, (0.04, -0.05), 'green', ready('ab1'),
           'flak11 (PU12-0: the AB1-P30M1 set, 220 V 50 Hz); the lamp is sam3d')
    L.lamp('ab1_starting', 'ЗАПУСК', p, (0.005, -0.05), 'amber', warming('ab1'), 'sam3d (the set running up)')
    L.lamp('scv_220', '220 В', p, (-0.07, -0.065), 'red', ready('power_220'), 'sam3d (220 V on the board)')
    L.nameplate(p, (0.06, 0.08), 'ЩВ', 0.04)

    # ---------------------------------------------------------------------------------------------- the squad leader
    p = L.panel('kp4_panel', 'KP-4 coordinate and course plotter', 'squad_leader', 'kp4', (0.44, 0.20),
                notes='The KP-4 (csla_pu12_manual item 2; valka_pu12): it draws the route on the map in its window '
                      '(1:50 000 to 1:200 000, flak11). Its face is SAM3D\'s arrangement.')
    L.control('kp4.power', p, (0.18, -0.06), 'toggle')
    L.lamp('kp4_on', 'КП-4', p, (0.18, 0.0), 'green', ready('kp4'), 'valka_pu12 (KP-4); the lamp is sam3d')
    L.fixture(p, (-0.04, -0.005), 'window', (0.30, 0.13), 'white')
    L.nameplate(p, (-0.04, 0.083), 'КП-4', 0.05)
    for u in (0.13, 0.18):
        L.fixture(p, (u, 0.06), 'knob', 0.022, 'black')
    p = L.panel('gak_panel', "GAK's control panel", 'squad_leader', 'gak_panel', (0.23, 0.15))
    L.control('gak.power', p, (-0.08, -0.03), 'toggle')
    L.control('gak.enter_heading', p, (0.02, -0.03), 'button',
              {'kind': 'button', 'size_m': 0.022, 'color': 'black', 'ring': 'chrome', 'label': 'ВВОД'})
    L.lamp('gak_ready', 'ГАК ГОТОВ', p, (-0.08, 0.035), 'green', ready('gak'),
           'valka_pu12 (gyro direction indicator GAK, 1G13M); the lamp is sam3d')
    L.lamp('gak_running_up', 'РАЗГОН', p, (-0.045, 0.035), 'amber', warming('gak'), 'sam3d (the gyro running up)')
    L.lamp('post_oriented', 'ОРИЕНТИРОВАН', p, (0.02, 0.035), 'green', ready('oriented'),
           'flak11 (PU12-0: the navigation equipment set up with the aiming circle); the lamp is sam3d')
    L.fixture(p, (0.08, 0.0), 'pointer_knob', 0.04, 'black')
    L.nameplate(p, (0.0, 0.062), 'ГАК', 0.04)
    p = L.panel('dtp_panel', 'DTP coordinate sensor', 'squad_leader', 'dtp', (0.22, 0.18),
                notes='The DTP (csla_pu12_manual item 5): it puts the post\'s offset from a reference point into the '
                      'telecode (flak11 PU12-0). The windows and knobs are SAM3D\'s.')
    L.control('dtp.parallax', p, (0.07, -0.05), 'toggle')
    for i, u in enumerate((-0.075, -0.045, -0.015, 0.015)):
        L.fixture(p, (u, 0.03), 'window', (0.022, 0.03), 'black')
        L.fixture(p, (u, -0.02), 'knob', 0.018, 'black')
    L.nameplate(p, (0.0, 0.07), 'ДТП', 0.04)
    p = L.panel('pvs2_panel', 'PVS-2 intercom box', 'squad_leader', 'pvs2', (0.17, 0.13))
    L.control('intercom.pvs2', p, (0.04, -0.02), 'toggle')
    L.nameplate(p, (-0.03, 0.045), 'ПВС-2')
    L.fixture(p, (-0.04, -0.02), 'socket', 0.028, 'chrome')
    status_board(L, 'it1', 'IT-1M status board', 'squad_leader', 'it1', 'ИТ-1М', '4',
                 'At the vehicle commander\'s (the squad leader\'s) place (rwd_mb3: "Tableau IT-1M in PU-12M am Platz des '
                 'Kommandeurs"; csla_pu12_manual item 4).')
    p = L.r123m_panel('r123_fug1_panel', 'R-123M (FuG-1)', 'squad_leader', 'r123_fug1')
    radio_lamps(L, p, 'fug1')
    L.control('radio.fug1.power', p, (-0.112, -0.0935), 'toggle')
    p = L.panel('pab2_panel', 'PAB-2 aiming circle case', 'squad_leader', 'pab2_case', (0.26, 0.18),
                notes='The aiming circle\'s case (SAM3D: the plan doesn\'t show where it is stowed). Opening it stands for '
                      'taking the PAB-2 out and setting it up on the ground (nav.pab2).')
    L.control('nav.pab2', p, (0.0, 0.0), 'toggle', {'kind': 'latch', 'size_m': 0.04, 'color': 'chrome'})
    L.nameplate(p, (0.0, 0.06), 'ПАБ-2', 0.05)

    # ---------------------------------------------------------------------------------------------- the mast
    p = L.panel('winch_panel', 'Mast winch', 'radio_operator', 'winch', (0.15, 0.18),
                notes='The hand winch on the mast\'s fixed section (csla_pu12_manual, the mast drawings: the winch, '
                      'cables and pulleys). Cranking it stands for raising the mast and guying it (mast.raise).')
    L.control('mast.raise', p, (0.0, 0.0), 'toggle', {'kind': 'lever', 'size_m': [0.11, 0.02], 'color': 'black'})
    L.lamp('mast_raising', 'МАЧТА', p, (-0.05, 0.07), 'amber', warming('mast_up'), 'sam3d (the mast going up)')
    L.lamp('mast_up', 'МАЧТА ПОДНЯТА', p, (0.05, 0.07), 'green', ready('mast_up'), 'sam3d (raised and guyed)')

    # ---------------------------------------------------------------------------------------------- the commander's workplace
    s = I.IT45_SCREEN
    it45 = I.item('it45')
    p = L.panel('it45_panel', 'IT-45 indicator', 'battery_commander', 'it45', (0.52, 0.80),
                notes='The IT-45\'s face, light grey (rwd_mb3 and shilka_narod photographs: a 45 cm tube behind an amber '
                      'filter and a grid glass, a strip of knobs over it, the ДХ keypad at its left, the ФР block at its '
                      'right).')
    L.displays.append({'id': 'it45', 'label': 'IT-45 indicator', 'kind': 'aspd_scope', 'panel': p,
                       'at_m': [0.0, r4(s['z'] - I.centre(it45)[2])], 'size_m': [s['d'], s['d']], 'glass': True,
                       'sweep_period_s': 10.0, 'afterglow_s': 6.0,
                       'on_when': ready('aspd_warm'),
                       'source': 'rwd_mb3 (the IT-45 shows the primary picture and the telecode\'s data as digits, signs, '
                                 'characteristics and symbols; the ФР block makes the radial-circular sweep and the range '
                                 'marks; the speed vector and the time of arrival; the DT-2 marker); ru_pu12_text (number '
                                 '00-99, friend or foe, group, height zone H1-H3, speed vector; scales 35, 50, 100 km); '
                                 'shilka_narod, rwd_mb3 (photographs: the amber filter, the grid glass); viko_commons (the '
                                 'same orange glass on a Soviet remote PPI); radiolamp_elt (the radar screens\' long '
                                 'yellow and orange afterglows)',
                       'confidence': 'medium',
                       'notes': 'Amber long-persistence picture (the filter in the photographs, as the Soviet PPIs\'): the '
                                'sweep turning with the range marks lighting as it passes and fading, the telecode\'s '
                                'targets drawn by the character generator (number, zone mark, speed vector, friends barred), '
                                'the marker, the marked target brightened with its time of arrival; a cabled radar\'s '
                                'echoes painted by the sweep under their symbols. Symbols, the sweep\'s period (10 s) and the '
                                'afterglow (6 s: the long-afterglow screens of the Soviet tube table, radiolamp_elt) are '
                                'SAM3D\'s. ' + VISUALS})
    L.fixture(p, (-0.2, 0.37), 'nameplate', (0.05, 0.014), label='ИТ-45')
    dch(L)
    p = L.panel('dt2_panel', 'DT-2 coordinate pickup', 'battery_commander', 'di', (0.17, 0.17),
                notes='The marker\'s pickup on the desk (rwd_mb3: Geber der Koordinaten DT-2; the ČSLA plan\'s item 20, '
                      'DI). SAM3D works it as two wheels: the marker\'s azimuth and its range.')
    # The DT-2's wheels move the marker on the IT-45's tube: the commander watches the tube as he turns them.
    L.control('dt2.azimuth', p, (-0.04, -0.01), 'wheel', {'kind': 'knob', 'size_m': 0.045, 'color': 'black'}, units_per_turn=90,
              watch='it45')
    L.control('dt2.range', p, (0.04, -0.01), 'wheel', {'kind': 'knob', 'size_m': 0.045, 'color': 'black'}, units_per_turn=0.25,
              watch='it45')
    L.nameplate(p, (-0.04, 0.045), 'АЗ', 0.025)
    L.nameplate(p, (0.04, 0.045), 'Д', 0.025)
    L.nameplate(p, (0.0, -0.065), 'ДТ-2', 0.04)
    fr(L)
    sblock(L)
    p = L.panel('r_panel', 'R telecode transmitter', 'operator', 'telecode_tx', (0.26, 0.16), at=(0.0, 0.22))
    L.nameplate(p, (0.0, 0.055), 'Р', 0.03)
    L.lamp('tx_on', 'ПЕРЕДАТЧИК', p, (-0.07, 0.0), 'green', ready('aspd_warm'), 'sam3d (the ASPD-U block powered)')
    for u in (0.03, 0.07, 0.11):
        L.fixture(p, (u, -0.02), 'socket', 0.024, 'chrome')
    p = L.panel('p_panel', 'P telecode receiver', 'operator', 'telecode_rx', (0.26, 0.16), at=(0.0, 0.22))
    L.nameplate(p, (0.0, 0.055), 'П', 0.03)
    L.lamp('rx_telecode', 'ПРИЁМ ТК', p, (-0.07, 0.0), 'green', ready('telecode_reception'),
           'flak11 (PU12-0: the receiver decodes the air picture); the lamp is sam3d')
    for u in (0.03, 0.07, 0.11):
        L.fixture(p, (u, -0.02), 'socket', 0.024, 'chrome')

    # ---------------------------------------------------------------------------------------------- the battery commander
    rtk1(L)
    status_board(L, 'it2', 'IT-2M status board', 'battery_commander', 'it2', 'ИТ-2М', '18',
                 'At the commander\'s workplace (rwd_mb3: "Tableau IT-2M ... am Platz des Operators", over the IT-45; '
                 'csla_pu12_manual item 18).')

    # ---------------------------------------------------------------------------------------------- the plotter
    p = L.panel('map_panel', 'Air-situation map', 'operator', 'map_board', (0.37, 0.35), color='lightgrey',
                notes='The plotter\'s map board (csla_pu12_manual item 24, planšet vzdušné situace: drawn folded on the '
                      'centreline before his place; rwd_mb3: the Luftlagekarte, a plexiglass board on a hinge, etched with a '
                      'grid and range circles), swung open across the compartment facing him. ' + VISUALS)
    L.displays.append({'id': 'map', 'label': 'Air-situation map', 'kind': 'plot_board', 'panel': p, 'at_m': [0.0, 0.0],
                       'size_m': [0.36, 0.34],
                       'source': 'rwd_mb3 (Luftlagekarte: the targets\' tracks drawn by hand in the designation squares, '
                                 'azimuth and range taken off it; the photograph: a plexiglass board with a grid and range '
                                 'circles); data/command/pu12.json (the board\'s notation: number, height over the code)',
                       'confidence': 'medium',
                       'notes': 'The plotter\'s grease-pencil marks on the etched plexiglass: each target\'s last marks '
                                'joined into a track, its number and height over its code; the post at the centre. The '
                                'grid\'s spacing and the pencil\'s colour are SAM3D\'s. ' + VISUALS})
    p = L.panel('s23_panel', 'S-23 data transmission set', 'operator', 's23', (0.30, 0.18),
                notes='The S-23 (rwd_mb3: the PU-12M\'s data transmission set behind the driver, a black box with a row '
                      'of coloured lamps). What each lamp shows is SAM3D\'s.')
    for u, lid, lab, color, cond in ((-0.11, 's23_power', 'ПИТ', 'white', ready('equipment_power')),
                                     (-0.07, 's23_warm', 'ПРОГР', 'amber', warming('aspd_warm')),
                                     (-0.03, 's23_rx', 'ПРМ', 'green', ready('telecode_reception')),
                                     (0.01, 's23_tx', 'ПРД', 'green', ready('aspd_working'))):
        L.lamp(lid, lab, p, (u, 0.03), color, cond, 'rwd_mb3 (photograph: a row of coloured lamps); the lamps\' meanings '
               'are sam3d', look={'kind': 'lamp', 'size_m': 0.016})
    for u, color in ((0.05, 'red'), (0.09, 'amber'), (0.13, 'white')):
        L.fixture(p, (u, 0.03), 'lamp', 0.016, color)
    for u in (-0.11, -0.07, -0.03, 0.01, 0.05, 0.09):
        L.fixture(p, (u, -0.04), 'button', 0.014, 'white', ring='chrome')
    L.nameplate(p, (0.1, -0.065), 'С-23', 0.04)
    p = L.panel('skc_panel', "ŠČK commander's panel", 'battery_commander', 'skc', (0.24, 0.18),
                notes='The commander\'s panel (ЩК; csla_pu12_manual item 36). What its switches do is not published; '
                      'SAM3D gives it the lamps of the commander\'s nets.')
    for u, sub, lab, src in ((-0.08, 'battery_net', 'БАТАРЕЯ', 'flak11 (PU12-1: FuG-4, the net commanding the battery)'),
                             (-0.02, 'telecode_reception', 'ТК', 'flak11 (PU12-1: FuG-5 R-407, the regiment\'s automated fire control)'),
                             (0.04, 'fug3', 'ПОЛК', 'flak11 (PU12-1: FuG-3 R-111, the regiment\'s command net)')):
        L.lamp('skc_' + sub, lab, p, (u, 0.04), 'white', ready(sub), src + '; the lamp is sam3d',
               look={'kind': 'legend', 'size_m': 0.026, 'label': lab})
    for u in (-0.08, -0.04, 0.0, 0.04, 0.08):
        L.fixture(p, (u, -0.03), 'toggle', 0.012)
    L.nameplate(p, (0.0, 0.08), 'ЩК', 0.04)
    p = L.r123m_panel('r123_fug4_panel', 'R-123M (FuG-4)', 'battery_commander', 'r123_fug4')
    radio_lamps(L, p, 'fug4')
    L.control('radio.fug4.power', p, (-0.112, -0.0935), 'toggle')
    p = L.panel('r407_panel', 'R-407 radio (FuG-5)', 'battery_commander', 'r407', (0.26, 0.26), at=(0.0, 0.10),
                notes='The R-407 (csla_pu12_manual item 25; the commander\'s workplace drawing shows its front behind '
                      'the seats). Its controls are SAM3D\'s.')
    L.control('radio.fug5.power', p, (-0.09, -0.09), 'toggle')
    L.lamp('fug5_ready', 'Р-407 ГОТОВА', p, (-0.09, -0.05), 'green', ready('fug5'),
           'flak11 (PU12-1: FuG-5 R-407, reception of the regiment\'s automated fire control); the lamp is sam3d')
    L.lamp('fug5_warm', 'ПРОГРЕВ', p, (-0.06, -0.05), 'amber', warming('fug5'), 'sam3d (warming up)')
    L.fixture(p, (0.03, 0.03), 'diagram', (0.16, 0.14))
    L.fixture(p, (0.03, 0.03), 'knob', 0.04, 'black')
    L.nameplate(p, (0.0, 0.115), 'Р-407', 0.05)

    # ---------------------------------------------------------------------------------------------- the radio operator
    p = L.panel('r111_panel', 'R-111 radio (FuG-3)', 'radio_operator', 'r111', (0.56, 0.31),
                notes='The R-111 (csla_pu12_manual item 41; the radio operator\'s drawing: the big set at his right '
                      'with the R-123M on it). Its face is SAM3D\'s arrangement.')
    L.control('radio.fug3.power', p, (-0.23, -0.10), 'toggle')
    L.lamp('fug3_ready', 'Р-111 ГОТОВА', p, (-0.23, -0.05), 'green', ready('fug3'),
           'flak11 (PU12-1: FuG-3 R-111, the regiment\'s command net, 25-30 km); the lamp is sam3d')
    L.lamp('fug3_warm', 'ПРОГРЕВ', p, (-0.20, -0.05), 'amber', warming('fug3'), 'sam3d (warming up)')
    L.fixture(p, (-0.02, 0.06), 'window', (0.16, 0.05), 'white')
    for u in (-0.12, -0.04, 0.04, 0.12):
        L.fixture(p, (u, -0.06), 'knob', 0.03, 'black')
    for u in (0.19, 0.23):
        L.fixture(p, (u, 0.05), 'socket', 0.026, 'chrome')
    L.nameplate(p, (-0.2, 0.12), 'Р-111', 0.05)
    p = L.r123m_panel('r123_fug2_panel', 'R-123M (FuG-2)', 'radio_operator', 'r123_fug2')
    radio_lamps(L, p, 'fug2')
    L.control('radio.fug2.power', p, (-0.112, -0.0935), 'toggle')
    p = L.panel('pvs1_panel', 'PVS-1 intercom box', 'radio_operator', 'pvs1', (0.19, 0.13))
    L.control('intercom.pvs1', p, (0.05, -0.02), 'toggle')
    L.nameplate(p, (-0.03, 0.045), 'ПВС-1')
    L.fixture(p, (-0.05, -0.02), 'socket', 0.028, 'chrome')
    p = L.panel('vs27_panel', 'VS-27 rectifier', 'radio_operator', 'vs27', (0.25, 0.14))
    L.control('pwr.rectifier', p, (0.08, -0.03), 'toggle')
    L.lamp('rectifier_on', 'ЗАРЯД', p, (0.08, 0.03), 'green', ready('rectifier'),
           'valka_pu12 (network rectifier VS-27); the lamp is sam3d')
    L.fixture(p, (-0.04, 0.0), 'gauge', 0.06, 'black', 'А')
    L.nameplate(p, (-0.04, 0.055), 'ВС-27', 0.05)
    p = L.panel('scr_panel', 'ŠČR distribution board', 'radio_operator', 'scr', (0.34, 0.23),
                notes='The distribution board (ЩР; csla_pu12_manual item 44; the radio operator\'s drawing: two meters, '
                      'a switch between them, a plate below).')
    L.control('pwr.equipment', p, (0.0, -0.01), 'rotary', {'kind': 'pointer_knob', 'size_m': 0.04, 'color': 'black'})
    L.fixture(p, (-0.09, 0.03), 'gauge', 0.07, 'black', 'V')
    L.fixture(p, (0.09, 0.03), 'gauge', 0.07, 'black', 'A')
    L.fixture(p, (0.0, -0.075), 'window', (0.2, 0.04), 'white')
    L.lamp('scr_220', 'СЕТЬ 220 В', p, (-0.04, 0.075), 'red', ready('power_220'),
           'valka_pu12 (ŠČR distribution board); the lamp is sam3d')
    L.lamp('scr_equipment', 'АППАРАТУРА', p, (0.04, 0.075), 'green', ready('equipment_power'),
           'valka_pu12 (ŠČR distribution board); the lamp is sam3d')
    L.nameplate(p, (0.0, 0.1), 'ЩР', 0.04)

    # ---------------------------------------------------------------------------------------------- the shell
    src = {
        'SM_PU12MHull': ('Compartment lining: floor, sides, roof with its hatches, the side hatches, the windscreens\' '
                         'covers, the bow and the bulkhead to the engines',
                         'pu12m_trace.py (the exterior traced from photographs and the published dimensions), 3 cm in; '
                         'csla_pu12_manual (the plan: the bulkhead at x 1555 px)', 'medium',
                         'Floor 0.62 m above the ground and the lining\'s 3 cm are estimates.'),
        'SM_PU12MEquipment': ('The equipment the plan places: telecode cabinets, the IT-45\'s block, the keypad, the power '
                              'boards, radios\' power units, the GAK, the desk, the shelves',
                              'csla_pu12_manual (the equipment plan\'s 53 items, read on a 10 px grid; the workplace '
                              'drawings); shilka_narod (photographs)', 'medium',
                              'Plan positions within about 3 cm; heights from the drawings, about 15 % (estimates). '
                              'tools/blender/pu12m_interior_trace.py lists each item and every move.'),
        'SM_PU12MBench': ('The bench: three places across the compartment, facing forward',
                          'csla_pu12_manual (the plan: cushions 1288-1343 px, backs 1350-1385 px)', 'medium',
                          'The cushions deepened to 0.31 m (the plan draws 0.24 m) and their height are estimates.'),
        'SM_PU12MDriver': ('The driver\'s wheel column, gear lever and dashboard',
                           'csla_pu12_manual (the squad leader and driver drawing)', 'low', 'Sizes are estimates.'),
        'SM_PU12MSteering': ('The steering wheel', 'csla_pu12_manual (the squad leader and driver drawing)', 'low',
                             'Its size and tilt are estimates. The driver\'s hands rest on it (data/crew/body.json).'),
        'SM_PU12MPedals': ('The pedals', 'csla_pu12_manual (the squad leader and driver drawing)', 'low',
                           'Estimates. The driver\'s feet rest on them (data/crew/body.json).'),
        'SM_PU12MMastFoot': ('The mast\'s fixed section from the floor through the roof, its clamps and its hand winch',
                             'csla_pu12_manual (the plan\'s item 10; the mast drawings: the winch, cables and pulleys)',
                             'medium', 'The winch\'s size is an estimate.'),
    }
    for mesh, (label, source, conf, note) in src.items():
        m = manifest[mesh]
        L.shell.append({'id': mesh[len('SM_PU12M'):].lower() or 'hull', 'label': label, 'kind': 'mesh', 'mesh': mesh,
                        'centre_m': m['centre_m'], 'size_m': m['size_m'], 'source': source, 'confidence': conf,
                        'notes': note})
    seat = manifest['SM_PU12MFrontSeat']
    for sid, lab in (('seat_driver', "Driver's seat"), ('seat_squad_leader', "Squad leader's seat")):
        cx, cy, _ = I.centre(I.item(sid))
        bx, by, _ = I.to_body(cx, cy, 0.0)
        c = seat['centre_m']
        L.shell.append({'id': sid, 'label': lab, 'kind': 'mesh', 'mesh': 'SM_PU12MFrontSeat',
                        'centre_m': [r4(bx + c[0]), r4(by + c[1]), c[2]], 'size_m': seat['size_m'],
                        'source': 'csla_pu12_manual (the plan\'s front seats; the squad leader and driver drawing)',
                        'confidence': 'medium', 'notes': 'The BTR-60PB\'s seat; its shape is an estimate.'})
    for k, (x, y) in enumerate(I.DOME_LAMPS):
        L.shell.append({'id': 'dome_%d' % k, 'label': 'Dome lamp', 'kind': 'mesh', 'mesh': 'SM_DomeLamp',
                        'centre_m': body((x, y, I.ROOF_IN_Z - 0.031)), 'size_m': [0.18, 0.18, 0.062],
                        'source': 'sam3d (the BTR-60PB\'s dome lamps; places an estimate)', 'confidence': 'low'})
        L.shell.append({'id': 'light_%d' % k, 'label': 'Dome lamp light', 'kind': 'light',
                        'centre_m': body((x, y, I.ROOF_IN_Z - 0.08)), 'size_m': [2.4, 2.4, 2.4], 'color': 'white',
                        'source': 'sam3d (estimate)', 'confidence': 'low', 'notes': 'Brightness: the cabin\'s default.'})
    order = ['hull', 'equipment', 'bench', 'driver', 'steering', 'pedals', 'mastfoot', 'seat_driver', 'seat_squad_leader']
    L.shell.sort(key=lambda s: order.index(s['id']) if s['id'] in order else len(order))
    return L


def radio_lamps(L, p, sub):
    src = 'r123m_training (after switching on, the табло and dial lamps light; warm the set up for 10 min before calling)'
    L.lamp(sub + '_ready', 'R-123M %s: готова' % sub.upper(), p, (0.1462, -0.0183), 'green', ready(sub), src,
           notes='Lit when the set is warm and can receive. The real set has табло lamps showing the sub-band; this lamp '
                 'is SAM3D\'s (as on the Osa\'s sets).')
    L.lamp(sub + '_warm', 'R-123M %s: прогрев' % sub.upper(), p, (0.1767, -0.0183), 'amber', warming(sub), src)


def status_board(L, did, label, station, iid, name, item_no, where):
    """An IT-1M / IT-2M board as the photographs show them (rwd_mb3): a light plate with the lamp matrix and its legends
    (the display), a blank label strip under it, then the input buttons, the fuse, the +27 V toggle and СБРОС."""
    it = I.item(iid)
    w = (it['x1'] - it['x0']) - 0.01
    h = (it['z1'] - it['z0']) - 0.01
    p = L.panel(did + '_panel', label, station, iid, (w, h),
                notes=where + ' The plate\'s legends and lamps are drawn by its display; the buttons, fuse and toggle are '
                              'fixtures (the crew\'s entries are the post\'s own, scenario_runner StatusBoards).')
    L.displays.append({'id': did, 'label': label, 'kind': 'status_board', 'panel': p, 'at_m': [0.0, r4(0.19 * h)],
                       'size_m': [r4(w - 0.012), r4(0.58 * h)], 'on_when': ready('net_27v'),
                       'source': 'rwd_mb3 (Informationstableau IT-1: signal groups for the ammunition, the combat activity '
                                 'and the readiness, input buttons, СБРОС; photographs of the IT-1, IT-1M and IT-2M: '
                                 'КОМПЛЕКТ 1, 1/2, 1/4; РАБОТА ГОТОВ, ВИЖУ, ВЫП.; ТЕХ. СОСТОЯНИЕ РПК, БЧ, ШАССИ; '
                                 'rows of amber lamps); csla_pu12_manual item ' + item_no,
                       'confidence': 'medium',
                       'notes': 'A row per launcher in the order of the battery: its number; КОМПЛЕКТ lit for the share '
                                'of its missiles left (SAM3D: a full load, half or more, any left); РАБОТА ГОТОВ for ready, '
                                'ВИЖУ while it engages (target seen), ВЫП. once it has launched; ТЕХ. СОСТОЯНИЕ dark (no '
                                'faults modelled); the bottom row the support vehicles (dark). ' + VISUALS})
    L.fixture(p, (0.0, r4(-0.16 * h)), 'window', (r4(0.8 * w), r4(0.07 * h)), 'white')
    for k in range(6):
        L.fixture(p, (r4((k - 2.5) * 0.15 * w), r4(-0.31 * h)), 'button', 0.011, 'red', ring='chrome')
    L.fixture(p, (r4(-0.38 * w), r4(-0.43 * h)), 'lamp', 0.009, 'red')
    L.fixture(p, (r4(0.22 * w), r4(-0.43 * h)), 'toggle', 0.01)
    L.fixture(p, (r4(0.38 * w), r4(-0.43 * h)), 'button', 0.013, 'red', label='СБРОС')
    L.nameplate(p, (0.0, r4(0.47 * h)), name + ' № 0373' if name.startswith('ИТ-2') else name, 0.06)
    return p


def dch(L):
    """The ДХ characteristics keypad, after the photographs (shilka_narod, its outline 20-330 x 10-715 px in the crop; rwd_mb3
    in colour: a light grey plate, a blue window, ivory keys with black legends in black frames, red keys at the bottom
    corners and one in the upper block). The digits, ЦУ and СБР work (the console's post keypad); the rest are drawn."""
    size = (0.13, 0.40)
    ext = (20, 10, 330, 715)
    p = L.panel('dch_panel', 'ДХ characteristics keypad', 'battery_commander', 'dch', size,
                notes='The ДХ keypad (csla_pu12_manual item 15; rwd_mb3: Pult DCh, entering a target\'s number, height '
                      'zone, characteristics and the designation signal with an address). SAM3D works the digits (the '
                      'target\'s number), ЦУ and a digit after it (the launcher\'s place on the IT boards: he reads it out '
                      'on the battery net) and СБР (reset). Labels read at low resolution.')
    wx, wy = L.at_photo(size, ext, 215, 120)
    L.displays.append({'id': 'dch_window', 'label': 'ДХ window', 'kind': 'keypad_window', 'panel': p, 'at_m': [r4(wx), r4(wy)],
                       'size_m': [0.063, 0.04], 'on_when': ready('aspd_warm'),
                       'source': 'rwd_mb3 (photograph: a window of blue glass at the top of the ДХ); gost_21829_76 (a '
                                 'warning flashes at 0.5-1 Hz: ЦУ); what it shows is sam3d',
                       'confidence': 'low',
                       'notes': 'The number the keypad is on (typed, or the marker\'s target), ЦУ flashing once a second '
                                'while it waits for the launcher\'s place, the last designation. Blue-green vacuum-fluorescent '
                                'seven-segment digits with their dark segments faintly showing: SAM3D\'s reading of the blue '
                                'window. ' + VISUALS})
    items = [(175, 30, 'nameplate', (0.03, 0.012), None, 'ДХ')]
    for x in (118, 160, 203, 245):
        items.append((x, 190, 'toggle', 0.008, None, None))
    items.append((288, 190, 'lamp_button', 0.016, 'red', None))
    for x in (118, 160, 203, 245, 288):
        items.append((x, 240, 'toggle', 0.008, None, None))
    keys = [('ВР', '', 'ТС'), ('СР', 'СМ', '*'), ('СВ', 'Ч', 'ГР'), ('Н1', 'Н2', 'Н3'), ('Н', '22', None)]
    # The characteristics keys that work (rwd_mb3 pu12a: friend or foe, group, the height zone): controls, the rest drawn.
    working = {'СВ': 'friend', 'Ч': 'foe', 'ГР': 'group', 'Н1': 'h1', 'Н2': 'h2', 'Н3': 'h3'}
    for row, y in zip(keys, (300, 330, 362, 393, 425)):
        for lab, x in zip(row, (145, 190, 240)):
            if lab is None:
                continue
            if lab in working:
                L.control('dch.key.' + working[lab], p, L.at_photo(size, ext, x, y), 'button',
                          {'kind': 'button', 'size_m': 0.016, 'color': 'ivory', 'label': lab})
                continue
            items.append((x, y, 'button', 0.016, 'red' if lab == '*' else 'ivory', None if lab in ('', '*') else lab))
    items += [(175, 603, 'button', 0.016, 'ivory', 'М'), (228, 603, 'button', 0.016, 'ivory', 'ПВ'),
              (175, 640, 'button', 0.016, 'ivory', 'СЗТ'), (228, 640, 'button', 0.016, 'red', None)]
    L.photo(p, size, ext, items)
    for d, (x, y) in {'7': (118, 495), '8': (175, 495), '9': (228, 495), '4': (118, 530), '5': (175, 530), '6': (228, 530),
                      '1': (118, 568), '2': (175, 568), '3': (228, 568), '0': (118, 603)}.items():
        L.control('dch.key.' + d, p, L.at_photo(size, ext, x, y), 'button',
                  {'kind': 'button', 'size_m': 0.016, 'color': 'ivory', 'label': d})
    L.control('dch.key.cu', p, L.at_photo(size, ext, 240, 425), 'button',
              {'kind': 'button', 'size_m': 0.016, 'color': 'ivory', 'label': 'ЦУ'})
    L.control('dch.key.reset', p, L.at_photo(size, ext, 118, 640), 'button',
              {'kind': 'button', 'size_m': 0.016, 'color': 'red', 'label': 'СБР'})


def fr(L):
    """The ФР block, after the photograph (shilka_narod; its outline 15-450 x 20-520 px in the crop)."""
    size = (0.21, 0.24)
    face = I.item('fr_block')
    # its controls on the block's lower part, as the photograph shows them
    p = L.panel('fr_panel', 'ФР block', 'battery_commander', 'fr_block', size, at=(0.0, face['z0'] + 0.03 + size[1] / 2 - I.centre(face)[2]),
                notes='The ФР block (shilka_narod photograph: БЛОК ФР, a РАБОТА / КОНТРОЛЬ selector, the scale '
                      'selector, a power toggle, brightness knobs and a second РАБОТА / КОНТРОЛЬ toggle) at the tube\'s '
                      'right. The ASPD-U\'s power switch is SAM3D\'s reading of its ВКЛ toggle.')
    ext = (15, 20, 450, 520)
    L.control('fr.mode', p, L.at_photo(size, ext, 175, 180), 'rotary',
              {'kind': 'pointer_knob', 'size_m': 0.03, 'color': 'black'})
    L.control('fr.scale', p, L.at_photo(size, ext, 305, 180), 'rotary',
              {'kind': 'pointer_knob', 'size_m': 0.03, 'color': 'black'})
    L.control('aspd.power', p, L.at_photo(size, ext, 360, 215), 'toggle')
    # The primary radar's source switch (rwd_mb3 pu12e/pu12f: "Umschalter für die Quelle der Funkmess-Primärinformation",
    # on the back of the commander's rack beside the DTP): SAM3D puts it on the ФР block's face, in his reach.
    L.control('aspd.radar_source', p, L.at_photo(size, ext, 405, 450), 'rotary',
              {'kind': 'pointer_knob', 'size_m': 0.024, 'color': 'black'})
    src = 'shilka_narod (photograph of the ФР block: small lamps beside the selectors)'
    L.lamp('aspd_warming', 'ПРОГРЕВ', p, L.at_photo(size, ext, 100, 135), 'amber', warming('aspd_warm'),
           src + '; what each shows is sam3d')
    L.lamp('aspd_working', 'РАБОТА', p, L.at_photo(size, ext, 250, 135), 'green', ready('aspd_working'),
           src + '; what each shows is sam3d')
    L.lamp('fr_telecode', 'ТК', p, L.at_photo(size, ext, 110, 325), 'green', ready('telecode_reception'),
           src + '; what each shows is sam3d')
    L.lamp('fr_primary', 'РЛС', p, L.at_photo(size, ext, 220, 325), 'green', ready('primary_picture'),
           src + '; what each shows is sam3d')
    L.photo(p, size, ext, [
        (232, 95, 'nameplate', (0.05, 0.012), None, 'БЛОК ФР'),
        (130, 285, 'knob', 0.016, 'black', None), (220, 285, 'knob', 0.016, 'black', None),
        (300, 285, 'knob', 0.016, 'black', None), (130, 380, 'knob', 0.016, 'black', None),
        (215, 380, 'knob', 0.02, 'black', None), (320, 380, 'toggle', 0.009, None, None),
        (360, 330, 'lamp', 0.007, 'red', None),
        (232, 45, 'lever', (0.12, 0.012), 'chrome', None), (232, 495, 'lever', (0.12, 0.012), 'chrome', None),
    ])


def sblock(L):
    """The S synchronisation block above the ФР block: its face is not in the photographs (SAM3D's arrangement)."""
    face = I.item('fr_block')
    p = L.panel('s_panel', 'S synchronisation block', 'battery_commander', 'fr_block', (0.21, 0.10),
                at=(0.0, face['z0'] + 0.34 - I.centre(face)[2]),   # over the ФР block's panel (fr: 0.03-0.27 m up)
                notes='The S block (csla_pu12_manual item 13; the commander\'s workplace drawing: the blocks with rows of '
                      'switches over the tube). Its face is SAM3D\'s arrangement.')
    L.nameplate(p, (-0.06, 0.03), 'БЛОК С', 0.05)
    for u in (-0.08, -0.05, -0.02, 0.01, 0.04, 0.07):
        L.fixture(p, (u, -0.015), 'toggle', 0.009)
    for u in (0.04, 0.07):
        L.fixture(p, (u, 0.03), 'lamp', 0.008, 'green')


def rtk1(L):
    """The RTK-1, after the photograph (shilka_narod; its outline 20-504 x 20-355 px in the crop)."""
    size = (0.40, 0.22)
    p = L.panel('rtk1_panel', 'RTK-1 radiotelephone concentrator', 'battery_commander', 'rtk1', size,
                notes='The RTK-1 (csla_pu12_manual item 16; flak11 PU12-1: field telephone lines and an acoustic signal), '
                      'as the photograph shows it (shilka_narod): windows with the lines\' names, lever switches, knobs. '
                      'Its power switch is SAM3D\'s.')
    ext = (20, 20, 504, 355)
    L.control('rtk1.power', p, L.at_photo(size, ext, 460, 230), 'toggle')
    L.lamp('rtk1_on', 'РТК-1', p, L.at_photo(size, ext, 460, 150), 'green', ready('rtk1'),
           'flak11 (PU12-1: RTK-1); the lamp is sam3d')
    items = [(90, 50, 'nameplate', (0.06, 0.012), None, 'РТК-1')]
    for x in (270, 302, 334, 366, 398):
        items.append((x, 95, 'window', (0.022, 0.022), 'white', None))
    for x in (105, 140, 175, 210):
        items.append((x, 160, 'window', (0.026, 0.014), 'white', None))
    for x in (295, 330, 370, 410):
        items.append((x, 180, 'lever', (0.03, 0.01), 'black', None))
        items.append((x + 15, 260, 'knob', 0.014, 'black', None))
    items += [(135, 215, 'knob', 0.03, 'black', None), (230, 250, 'pointer_knob', 0.03, 'black', None),
              (195, 315, 'socket', 0.04, 'chrome', None)]
    for x in (300, 330, 360, 390):
        items.append((x, 60, 'lamp', 0.007, 'red', None))
    L.photo(p, size, ext, items)


def layout_json():
    L = build()
    doc = {
        'system_id': 'pu12m',
        'status': 'The PU-12M\'s fighting compartment from the ČSLA manual\'s equipment plan and workplace drawings '
                  '(csla_pu12_manual, the valka_pu12 scans) and a veteran\'s photographs (shilka_narod): five places '
                  '(driver front left, squad leader front right, the operator, the battery commander and the radio '
                  'operator on the bench across the middle facing forward), the equipment where the plan puts it.',
        'notes': 'Written by tools/pu12m_cabin.py from tools/blender/pu12m_interior_trace.py and '
                 'art/pu12m_interior/manifest.json: edit those and run it again, not this file. Body frame: metres, x '
                 'right, y forward, z up, from the ground under the middle of the wheelbase (the exterior model\'s '
                 'origin). Lamp sources use the source ids of data/procedures/pu12m.json. ceiling_m: the roof\'s '
                 'lining (1.40 m over the floor), under which a crewman getting up for a low panel crouches.',
        'ceiling_m': round(I.ROOF_IN_Z, 3),
        # Up from his seat (a player walking in the cabin): his stooping eye under the roof, and the floor he can walk
        # on (SAM3D estimates from the layout).
        'stand_eye_m': 1.93,
        'walk': [
            {'from_m': [-0.6, -0.86], 'to_m': [0.7, -0.62],
             'notes': "The knee room between the bench and the boards in front of it (the IT-45, the map, the radio "
                      "operator's sets, their faces at y = -0.27 to -0.47): a crewman gets up and moves along the bench. "
                      "SAM3D estimate from the cabin layout."},
            {'from_m': [-0.18, 0.1], 'to_m': [0.18, 1.45],
             'notes': "The aisle between the operator's P and R units (their faces at x = -0.33 and 0.32). SAM3D estimate."},
            {'from_m': [-0.15, 1.45], 'to_m': [0.05, 2.45], 'notes': "Between the driver's and the squad leader's seats. SAM3D estimate."},
        ],
        'stations': L.stations, 'panels': L.panels, 'controls': L.controls, 'lamps': L.lamps, 'displays': L.displays,
        'blocks': L.blocks, 'fixtures': L.fixtures, 'shell': L.shell,
    }
    if not doc['blocks']:
        del doc['blocks']
    return doc


def dump(doc):
    """One item per line inside the lists, as data/cabins/osa_ak.json is laid out."""
    out = ['{']
    keys = list(doc)
    for i, k in enumerate(keys):
        v = doc[k]
        end = ',' if i < len(keys) - 1 else ''
        if isinstance(v, list):
            out.append('  %s: [' % json.dumps(k))
            for j, item in enumerate(v):
                out.append('    %s%s' % (json.dumps(item, ensure_ascii=False), ',' if j < len(v) - 1 else ''))
            out.append('  ]' + end)
        else:
            out.append('  %s: %s%s' % (json.dumps(k), json.dumps(v, ensure_ascii=False), end))
    out.append('}')
    return '\n'.join(out) + '\n'


def main(argv):
    text = dump(layout_json())
    if '--check' in argv:
        same = OUT.exists() and OUT.read_text(encoding='utf-8') == text
        print('data/cabins/pu12m.json is %s' % ('up to date' if same else 'NOT what the trace gives: run tools/pu12m_cabin.py'))
        return 0 if same else 1
    OUT.write_text(text, encoding='utf-8', newline='\n')
    print('wrote', OUT.relative_to(ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
