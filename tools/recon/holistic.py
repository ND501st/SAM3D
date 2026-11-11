"""The holistic layer of the Osa interior pipeline (docs/OSA_INTERIOR_HOLISTIC.md): the component register, the
photos' observations, their fusion and the schematics.

    python tools/recon/holistic.py seed      # write data/cabins/osa_ak_components.json's seed (only if it doesn't exist)
    python tools/recon/holistic.py check     # the register and every observation file are well formed
    python tools/recon/holistic.py fuse      # merge the observations into the register's evidence (rewrites the register)
    python tools/recon/holistic.py draw      # schematics: out/recon/holistic/*.svg and index.html
    python tools/recon/holistic.py report    # docs/research/OSA_INTERIOR_COMPONENTS_2026-11.md

Standard library only (it runs anywhere the tests run).
"""
import argparse
import glob
import html
import json
import math
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REGISTER = os.path.join(REPO, 'data', 'cabins', 'osa_ak_components.json')
CABIN = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
OBS_DIR = os.path.join(REPO, 'tools', 'traces', 'osa_interior', 'observations')
OUT = os.path.join(REPO, 'out', 'recon', 'holistic')
REPORT = os.path.join(REPO, 'docs', 'research', 'OSA_INTERIOR_COMPONENTS_2026-11.md')
FIG = os.path.join(REPO, 'reference', 'sources', 'AntiaircraftOSA-AK-9K32M2rocketkit_extract')

CLASSES = {
    'structure': ['hull_wall', 'floor', 'roof', 'bulkhead', 'hatch', 'window', 'door', 'pylon', 'sponson', 'frame', 'lining'],
    'equipment': ['rack', 'block', 'console', 'box', 'radio', 'intercom', 'phone', 'instrument', 'fan', 'heater', 'filter',
                  'periscope', 'motor', 'power'],
    'crew': ['seat', 'bench', 'belt', 'cushion', 'footrest', 'curtain', 'handle', 'step'],
    'fittings': ['bracket', 'shelf', 'mount', 'clamp', 'hook', 'strap', 'cover', 'rail', 'plate'],
    'runs': ['cable', 'loom', 'conduit', 'hose', 'duct', 'pipe'],
    'light_controls': ['lamp', 'switch', 'connector', 'fuse_panel', 'control'],
    'loose': ['kit', 'unknown'],
}
CLASS_GROUP = {c: g for g, cs in CLASSES.items() for c in cs}
ZONES = ['cab', 'cab.dash', 'cab.left_wall', 'cab.right_wall', 'cab.roof', 'cab.floor', 'compartment.left_wall',
         'compartment.right_wall', 'compartment.aisle', 'compartment.floor', 'compartment.roof', 'compartment.rear',
         'rear_bulkhead', 'engine', 'outside']
RELATIONS = ['on', 'in', 'above', 'below', 'forward_of', 'aft_of', 'left_of', 'right_of', 'beside', 'behind',
             'in_front_of', 'connects', 'same_as']
CONFIDENCE = ['high', 'medium', 'low']
STATUS = ['missing', 'placed', 'modelled', 'traced', 'face_traced']

# Fig. 3 (mon_2112_80), calibrated 8 Nov 2026 against the cabin data (docs/OSA_INTERIOR_HOLISTIC.md section 6)
PLAN_MM_PX = 4.86
PLAN = {'p028': {'centre_py': 909.0, 'y0_px': 960.0}, 'p029': {'centre_py': 917.0, 'y0_px': 95.0}}
PLAN_Y0 = 2.505                      # SŁ-2M's front end, body y
ELEV_MM_PX = 5.74                    # the sections are squashed in height
ELEV = {('p028', 'A-A'): 262.0, ('p029', 'A-A'): 262.0, ('p028', 'B-B'): 1335.0, ('p029', 'B-B'): 1335.0}  # py of z 2.09
ELEV_TOP_Z = 2.09


def plan_to_body(sheet, px, py):
    c = PLAN[sheet]
    return (c['centre_py'] - py) * PLAN_MM_PX / 1000.0, PLAN_Y0 - (px - c['y0_px']) * PLAN_MM_PX / 1000.0


def plan_box(sheet, box):
    """A pixel box on a plan sheet -> (x0, x1, y0, y1) body metres."""
    xa, ya = plan_to_body(sheet, box[0], box[1])
    xb, yb = plan_to_body(sheet, box[2], box[3])
    return min(xa, xb), max(xa, xb), min(ya, yb), max(ya, yb)


def elev_z(sheet, section, py):
    return ELEV_TOP_Z - (py - ELEV[(sheet, section)]) * ELEV_MM_PX / 1000.0


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def save(path, doc):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(doc, f, ensure_ascii=False, indent=1)
        f.write('\n')


# ------------------------------------------------------------------------------------------------ the seed

def _panel_box(p, depth=0.06):
    """A cabin panel as an axis-aligned body box (its face plane plus a depth behind it)."""
    f = math.radians(p['facing_deg'])
    n = (math.sin(f), math.cos(f))
    r = (-math.cos(f), math.sin(f))
    w, h = p['size_m']
    cx, cy, cz = p['centre_m']
    xs = [cx + r[0] * s * w / 2 - n[0] * d for s in (-1, 1) for d in (0, depth)]
    ys = [cy + r[1] * s * w / 2 - n[1] * d for s in (-1, 1) for d in (0, depth)]
    return {'centre': [round((min(xs) + max(xs)) / 2, 4), round((min(ys) + max(ys)) / 2, 4), cz],
            'size': [round(max(xs) - min(xs), 4), round(max(ys) - min(ys), 4), h]}


# Fig. 3 items not in the cabin data, with their pixel boxes (read off the sheets on a 50 px grid, 8 Nov 2026)
PLAN_ITEMS = [
    ('instrument.dp3b', 'instrument', 'DP-3B (radiac meter)', ['DP-3B', 'ДП-3Б'], 'cab.right_wall', ('p028', [547, 733, 605, 775])),
    ('plate.lowering_board', 'plate', 'Tablica opuszczania (lowering board: ПРОХОД ПОД МОСТОМ)', ['ПРОХОД ПОД МОСТОМ', 'Tablica opuszczania'],
     'cab.dash', ('p028', [552, 805, 596, 825])),
    ('console.commander_front', 'console', "Commander's front panel (Fig. 3's tall box at the dash, right)", [], 'cab.dash', ('p028', [547, 795, 605, 898])),
    ('lock.launch_prohibit', 'switch', 'Zamek zakazu startu (launch-prohibit lock)', ['Zamek zakazu startu'],
     'compartment.left_wall', ('p028', [930, 1040, 970, 1062])),
    ('plate.signal_board', 'plate', 'Tablica sygnalizacji (signal board)', ['Tablica sygnalizacji'], 'compartment.left_wall',
     ('p028', [945, 1048, 975, 1066])),
    ('frame.post_sl1m_sl2m', 'frame', 'Post between SŁ-1M and SŁ-2M', [], 'compartment.left_wall', ('p028', [930, 1040, 962, 1190])),
    ('frame.post_sp1m_sp2m', 'frame', 'Post between SP-1M and SP-2M', [], 'compartment.right_wall', ('p028', [930, 635, 960, 780])),
    ('frame.post_sp2m_sp3m', 'frame', 'Post between SP-2M and SP-3M-2', [], 'compartment.right_wall', ('p029', [483, 645, 510, 787])),
    ('frame.post_sl2m_sl3m', 'frame', 'Post between SŁ-2M and SŁ-3M', [], 'compartment.left_wall', ('p029', [483, 1053, 510, 1195])),
    ('instrument.pb55', 'instrument', 'PB-55', ['PB-55', 'ПБ-55'], 'rear_bulkhead', ('p029', [805, 968, 833, 1028])),
    ('fan.rear_right', 'fan', 'Ventilator at the rear right (Fig. 3 "Wentylator")', ['Wentylator'], 'compartment.rear', ('p029', [805, 645, 888, 765])),
    ('fan.rear_left', 'fan', 'Ventilator at the rear left (Fig. 3 "Wentylator")', ['Wentylator'], 'compartment.rear', ('p029', [805, 1070, 888, 1190])),
    ('hull.door_rear', 'door', 'Door to the engine compartment', ['door'], 'rear_bulkhead', ('p029', [805, 795, 920, 910])),
    ('box.emu31aw3', 'box', 'EMU31AW-3', ['EMU31AW-3'], 'engine', ('p029', [958, 770, 1097, 815])),
    ('box.fuel_tank', 'box', 'Fuel tank (Zbiornik paliwa)', ['Zbiornik paliwa'], 'engine', ('p029', [985, 1055, 1240, 1180])),
    ('box.gas_turbine_9i120', 'box', 'Gas-turbine power unit 9I120', ['9I120', '9И120'], 'engine', ('p029', [1150, 645, 1305, 770])),
]
# Fig. 3's sections: items whose height the sections give (sheet, section, pixel box)
ELEV_ITEMS = [
    ('fan.roof_front_right', 'fan', 'Two ventilators at the roof, front right (A-A "Wentylatory 2 szt.")', ['Wentylatory'],
     'compartment.roof', ('p028', 'A-A', [703, 185, 757, 240])),
    ('fan.roof_front_left', 'fan', 'Two ventilators at the roof, front left (B-B "Wentylatory 2 szt.")', ['Wentylatory'],
     'compartment.roof', ('p029', 'B-B', [655, 1270, 735, 1330])),
    ('box.zczz1a_2', 'box', 'Spare-parts box ZCzZ1A nr 2 (under SP-1M)', ['ZCzZ1A Skrzynka nr 2'], 'compartment.right_wall',
     ('p028', 'A-A', [775, 405, 915, 447])),
    ('power.external_connection', 'connector', 'External power connection (Połączenie zasilania zewnętrznego)',
     ['Połączenie zasilania zewnętrznego'], 'compartment.left_wall', ('p028', 'B-B', [535, 1350, 590, 1392])),
    ('motor.0006_5', 'motor', '0006-5 (a round machine under SŁ-3M at the rear, B-B)', ['0006-5'], 'compartment.left_wall',
     ('p028', 'B-B', [775, 1545, 925, 1610])),
    ('box.kf_kf1', 'box', 'KF, KF1 (at the rear top of SP-3M-2, A-A)', ['KF', 'KF1'], 'compartment.rear', ('p029', 'A-A', [775, 262, 800, 420])),
    ('fan.rear_top', 'fan', 'Ventilator at the rear top right (A-A "Wentylator")', ['Wentylator'], 'compartment.rear',
     ('p029', 'A-A', [810, 380, 890, 450])),
    ('box.0005_6m', 'box', '0005-6M (rear right, A-A)', ['0005-6M'], 'compartment.rear', ('p029', 'A-A', [815, 275, 912, 350])),
    ('outside.0004_20m', 'box', '0004-20M (on the roof, under the turret: A-A)', ['0004-20M', 'СА0004-20М'], 'outside',
     ('p029', 'A-A', [430, 170, 755, 250])),
]
# Components the book's notes and the roadmap name that neither the cabin data nor Fig. 3 place
NAMED = [
    ('hull.hatch_compartment', 'hatch', 'Roof hatch over the compartment (early type)', 'compartment.roof', 'wwp_gecko g082-a'),
    ('hull.hatch_driver', 'hatch', "Driver's roof hatch", 'cab.roof', 'cabin data hull'),
    ('hull.hatch_commander', 'hatch', "Commander's roof hatch", 'cab.roof', 'cabin data hull'),
    ('hull.windscreen_left', 'window', 'Windscreen, left (driver)', 'cab.dash', 'cabin data glass'),
    ('hull.windscreen_right', 'window', 'Windscreen, right (commander)', 'cab.dash', 'cabin data glass'),
    ('hull.side_window_left', 'window', 'Side window, left', 'cab.left_wall', 'cabin data glass'),
    ('hull.side_window_right', 'window', 'Side window, right', 'cab.right_wall', 'cabin data glass'),
    ('hull.floor', 'floor', 'Floor plates', 'compartment.floor', 'cabin data hull'),
    ('hull.roof', 'roof', 'Roof lining', 'compartment.roof', 'cabin data hull'),
    ('hull.wall_left', 'hull_wall', 'Left side wall', 'compartment.left_wall', 'cabin data hull'),
    ('hull.wall_right', 'hull_wall', 'Right side wall', 'compartment.right_wall', 'cabin data hull'),
    ('hull.bulkhead_rear', 'bulkhead', 'Rear bulkhead (to the engine compartment)', 'rear_bulkhead', 'cabin data hull'),
    ('hull.dashboard', 'console', 'Dashboard under the windscreens', 'cab.dash', 'cabin data hull'),
    ('pylon.turret', 'pylon', "The turret's base and its fabric skirt in the compartment", 'compartment.rear', 'wwp_gecko g096-a, g100'),
    ('cover.floor_radio_well', 'cover', 'Floor cover over the radio well', 'compartment.floor', 'wwp_gecko g083-b, g093-c'),
    ('cover.floor_tunnel', 'cover', 'Floor and tunnel covers', 'compartment.floor', 'wwp_gecko g083-b, g093-c'),
    ('cushion.floor_mat', 'cushion', 'Floor mat', 'compartment.floor', 'wwp_gecko g083-b'),
    ('curtain.cab', 'curtain', 'Curtain between the cab and the compartment, and its rail', 'cab', 'wwp_gecko g083-c'),
    ('psu.bp26_left', 'power', 'БП-26 power unit of the left R-123M', 'cab.left_wall', 'wwp_gecko g090-c'),
    ('psu.bp26_right', 'power', 'БП-26 power unit of the right R-123M', 'cab.right_wall', 'wwp_gecko g091-b'),
    ('mount.r123m_left', 'mount', "Left R-123M's shock mount frame", 'cab.left_wall', 'wwp_gecko g090-c'),
    ('mount.r123m_right', 'mount', "Right R-123M's shock mount frame", 'cab.right_wall', 'wwp_gecko g091-b'),
    ('cover.radio_steel', 'cover', "The radios' steel covers (climbed on at the hatch)", 'cab', 'wwp_gecko g082 caption'),
    ('box.zip', 'box', 'ЗИП box (spares) in the cab', 'cab', 'wwp_gecko g090-d'),
    ('heater.cab', 'heater', 'Heater in the cab, right wall', 'cab.right_wall', 'wwp_gecko g087-c'),
    ('fuse_panel.cab_right', 'fuse_panel', 'Fuse panel on the cab right wall', 'cab.right_wall', 'wwp_gecko g087-c'),
    ('motor.wiper', 'motor', 'Wiper motor(s) over the windscreens', 'cab.roof', 'wwp_gecko g089'),
    ('periscope.blanks', 'periscope', 'Periscope blanks', 'cab.roof', 'wwp_gecko g089'),
    ('box.panel_indication', 'box', 'ПАНЕЛЬ ИНДИКАЦИИ box beside the guidance column', 'compartment.left_wall', 'wwp_gecko g095-b'),
    ('instrument.dial_strip', 'instrument', 'The eight small dials of the dial strip on SŁ-2M', 'compartment.left_wall', 'wwp_gecko g095-a'),
    ('fan.blower_rear', 'fan', 'Blower at the rear bulkhead', 'rear_bulkhead', 'wwp_gecko g100'),
    ('plate.gom', 'plate', 'ГОМ plate on the rear door', 'rear_bulkhead', 'wwp_gecko g100'),
    ('kit.headsets', 'kit', 'Headsets and their hooks', 'compartment.aisle', 'wwp_gecko notes'),
    ('kit.fire_extinguishers_cab', 'kit', 'Fire extinguishers in the cab', 'cab', 'roadmap I7'),
    ('belt.operators', 'belt', "The operators' seat belts", 'compartment.aisle', 'wwp_gecko g092-c, g093-a'),
    ('control.gear_lever', 'control', 'Gear lever', 'cab.floor', 'wwp_gecko g084-a'),
    ('control.handbrake', 'control', 'Handbrake', 'cab.floor', 'wwp_gecko g084-a'),
]

CABIN_SHELL = {  # cabin shell id -> (register id, class, zone)
    'bench': ('bench.operators', 'bench', 'compartment.aisle'),
    'seat_deputy': ('seat.deputy_commander', 'seat', 'compartment.aisle'),
    'seat_senior': ('seat.senior_operator', 'seat', 'compartment.aisle'),
    'seat_operator': ('seat.operator', 'seat', 'compartment.aisle'),
    'seat_driver': ('seat.driver', 'seat', 'cab'),
    'seat_commander': ('seat.commander', 'seat', 'cab'),
    'steering': ('control.steering_wheel', 'control', 'cab'),
    'pedals': ('control.pedals', 'control', 'cab.floor'),
    'cab_levers': ('control.cab_levers', 'control', 'cab.floor'),
    'cab_fittings': ('kit.sun_visors', 'kit', 'cab.roof'),
    'r124_a1': ('intercom.a1', 'intercom', 'cab.right_wall'),
    'r124_a2': ('intercom.a2', 'intercom', 'compartment.left_wall'),
    'r124_a3': ('intercom.a3', 'intercom', 'compartment.left_wall'),
    'r124_a4': ('intercom.a4', 'intercom', 'cab.left_wall'),
    'r124_a5': ('intercom.a5', 'intercom', 'compartment.left_wall'),
    'radio_left': ('radio.r123m_left', 'radio', 'cab.left_wall'),
    'radio_right': ('radio.r123m_right', 'radio', 'cab.right_wall'),
    'radio_tray_left': ('cover.radio_steel', 'cover', 'cab'),
    'phone': ('phone.ta57', 'phone', 'compartment.aisle'),
    'spares_1': ('box.zczz1a_1', 'box', 'compartment.right_wall'),
    'ga3': ('instrument.ga3', 'instrument', 'compartment.rear'),
    'cgw10p': ('box.cgw10p', 'box', 'compartment.aisle'),
    'awo5m1': ('box.awo5m1', 'box', 'compartment.rear'),
    'cabinet_0096': ('rack.0096_9m', 'rack', 'compartment.rear'),
    'dome_front': ('lamp.dome_front', 'lamp', 'compartment.roof'),
    'dome_rear': ('lamp.dome_rear', 'lamp', 'compartment.roof'),
    'dome_cab': ('lamp.dome_cab', 'lamp', 'cab.roof'),
    'lamp_blue': ('lamp.blue', 'lamp', 'rear_bulkhead'),
    'rack_sl1m': ('rack.sl1m', 'rack', 'compartment.left_wall'),
    'rack_sl2m': ('rack.sl2m', 'rack', 'compartment.left_wall'),
    'rack_sl3m': ('rack.sl3m', 'rack', 'compartment.left_wall'),
    'rack_sp1m': ('rack.sp1m', 'rack', 'compartment.right_wall'),
    'rack_sp2m': ('rack.sp2m', 'rack', 'compartment.right_wall'),
    'rack_sp3m': ('rack.sp3m', 'rack', 'compartment.right_wall'),
    'chest_5937': ('box.stowage_5937', 'box', 'compartment.right_wall'),
    'first_aid': ('kit.first_aid', 'kit', 'compartment.right_wall'),
}
STATUS_OF_MESH = {'SM_CabSeat': 'face_traced', 'SM_SP2M2Face': 'face_traced', 'SM_SP3MFace': 'face_traced', 'SM_SL3MFace': 'face_traced', 'SM_SL1MFace': 'face_traced', 'SM_SP1MFace': 'face_traced', 'SM_RadioR123M': 'traced'}
PLAN_OF = {  # Fig. 3 boxes of components the cabin data already has (checks)
    'radio.r123m_right': ('p028', [765, 760, 870, 790]), 'radio.r123m_left': ('p028', [793, 1010, 912, 1043]),
    'box.zczz1a_1': ('p028', [968, 790, 1040, 815]), 'box.cgw10p': ('p029', [445, 890, 520, 945]),
    'box.awo5m1': ('p029', [718, 895, 763, 948]), 'instrument.ga3': ('p029', [647, 997, 713, 1063]),
    'phone.ta57': ('p029', [118, 928, 157, 945]), 'bench.operators': ('p029', [108, 945, 467, 1028]),
    'rack.0096_9m': ('p029', [550, 905, 605, 995]), 'seat.commander': ('p028', [628, 795, 730, 897]),
    'seat.driver': ('p028', [628, 928, 730, 1030]), 'control.steering_wheel': ('p028', [540, 920, 610, 1030]),
    'console.pu303': ('p028', [557, 1057, 587, 1128]),
}
PANEL_REG = {  # cabin panel -> register id (racks and consoles)
    'gyro_panel': 'rack.sl1m', 'so_panel': 'console.sl2m_search', 'op_panel': 'console.sl2m_range', 'fo_panel': 'console.sl2m_guidance',
    'sl3m_panel': 'rack.sl3m', 'sp1m_panel': 'rack.sp1m', 'sp2m_panel': 'rack.sp2m', 'sp3m_panel': 'rack.sp3m',
    'pwr_cabinet': 'rack.0096_9m', 'pu303_panel': 'console.pu303', 'driver_panel': 'console.driver_dash',
    'commander_panel': 'console.commander_dash', 'aiming_circle_case': 'box.aiming_circle',
}
CONFLICTS = {  # found when Fig. 3 was calibrated against the cabin data (8 Nov 2026); a camera solve settles them (roadmap I1)
    'radio.r123m_left': 'Fig. 3 put it at y 3.03, the cabin data at 3.28 and z 1.24 by estimate; settled in H3 (9 Nov 2026): '
                        'y 3.03 from Fig. 3, low in a well under SL-1M from g091-d (z 0.80); moved 11 Nov 2026 through g082-b\'s '
                        'measured camera: behind the driver\'s seat on a shock tray on the deck, its front at x -0.34',
    'radio.r123m_right': 'the cabin data had it at z 0.80 by estimate; settled in H3 (9 Nov 2026): on the lid of ZCzZ1A nr 2 '
                         '(g090-a), z 1.24; moved 11 Nov 2026 through g082-b\'s measured camera: 0.38 m aft, its foot at z 1.64. '
                         'Fig. 3\'s plan puts its front\'s middle at y 3.20, g082-b at 2.83 (its front 410 px high at the aft end, '
                         'the ZIP box under it 0.355 m high there as g090-a shows it); the photo is used',
    'box.zczz1a_2': 'section A-A draws it 0.19 m high just under the blocks; g090-a was read as standing on the floor, about '
                    '0.47 m high (9 Nov 2026); g082-b\'s measured camera gives 0.355 m, its foot at z 1.17, the right R-123M '
                    'on its lid (11 Nov 2026, used)',
    'intercom.a1': 'the photos plate the commander\'s box А-2 with a Р-112 / ВС / Р-123 switch (g083-a, g087-c); the cabin calls '
                   'it A-1 (the R-124 kit\'s commander apparatus). Kept as A-1 until a source on the Osa\'s R-124 fit says '
                   'which apparatus the commander has',
    'block.s_0094_5': 'the dial strip\'s plate reads 0094-9 on the BM3-1 (g095-a); the AK keeps 0094-5 (decision D1) until an AK '
                      'source shows otherwise',
    'rack.0096_9m': 'Fig. 3 puts it at (-0.16, 0.16), the cabin data had (0.08, 0.125) from the view in SAM Simulator; settled in I1 (9 Nov 2026) for Fig. 3, which g096-a supports (about x -0.13). Its turn of -38 degrees is the AK drawing; the BM3-1 photos stand it square on a plinth (kept for the AK, decision D1; the square stance for I9)',
    'instrument.ga3': 'Fig. 3 puts it 0.37 m inboard of the cabin: it moved out with the left racks when the photos overruled the drawing (docs/OSA_INTERIOR_HOLISTIC.md section 28); no photo shows its own place (used: with the racks)',
    'phone.ta57': 'Fig. 3 puts it at y 2.30, the cabin shell part at 2.37; settled in I1 (9 Nov 2026): the part takes in the holder, 7 cm being within the holder itself (used: the cabin data)',
}
# ------------------------------------------------------------------------------------------------ H3, the review
# A person's review of the first pass (9 Nov 2026; docs/OSA_INTERIOR_HOLISTIC.md section 10). Applied by `fuse`.

# Observed ids that are one thing with another id: the observation's id -> the id kept.
ALIASES = {
    'new.plate.hb_sticker_seat_commander': 'new.plate.hb_sticker_commander_seat',
    'new.plate.type_cab_roof': 'new.plate.type_plate_roof',
    'new.cable.roof_rear_right': 'new.cable.roof_right_rear',
    'new.cover.radio_left_dust_cloth': 'new.cover.r123m_left_canvas',
    'new.cover.radio_right_dust_cloth': 'new.cover.r123m_right_canvas',
    'new.frame.seat_t_rod': 'new.control.seat_t_rod',
    'new.kit.t_key_rear_right': 'new.kit.keys_post_right',       # the T-key on the post between SP-2M and SP-3M-2
    'new.door.0096_9m_face': 'rack.0096_9m',                     # the cabinet's face (drawing g097-c) is the cabinet
    'new.unknown.turret_base_casting': 'pylon.turret',
}
# What is part of what (the child is built with, or on, its parent).
PARENTS = {
    'rack.0096_9m': ['box.0096_9m_plinth', 'cover.0096_9m_lid', 'lamp.0096_9m_lid_tube', 'plate.0096_9m_side',
                     'connector.0096_9m_plugs', 'loom.0096_9m_bottom', 'loom.cabinet_plugs'],
    'box.zczz1a_2': ['plate.zczz1a_2_nameplate', 'clamp.zczz1a_2_latch', 'cover.zczz1a_2_lid', 'kit.zczz1a_2_korobka_1',
                     'kit.zczz1a_2_korobka_2', 'kit.zczz1a_2_korobka_3', 'kit.zczz1a_2_pouch', 'shelf.zczz1a_2_p1',
                     'shelf.zczz1a_2_p2', 'shelf.zczz1a_2_p3', 'shelf.zczz1a_2_p4'],
    'rack.sp1m': ['block.sp1m_fwd_top', 'block.sp1m_fwd_mid', 'block.sp1m_fwd_bottom', 'block.sp1m_aft_top',
                  'block.sp1m_aft_tall', 'plate.sp1m_rack_plate'],
    'rack.sp3m': ['block.sp3m_top_right', 'cover.sp3m_front_flap', 'loom.under_sp3m'],
    'rack.sl1m': ['plate.sl1m_rack_plate'],
    'rack.sl2m': ['plate.sl2m_rack', 'fuse_panel.sl2m_front_lower'],
    'rack.sp2m': ['plate.sp2m_rack_plate', 'plate.sp2m_frame_top', 'connector.sp2m_frame_lower_front', 'strap.sp2m_bottom'],
    'hull.dashboard': ['console.centre_dash', 'console.driver_dash', 'console.commander_dash', 'handle.dash',
                       'handle.dash_right'],
    'kit.sun_visors': ['kit.sun_visor_left', 'kit.sun_visor_right'],
    'periscope.blanks': ['periscope.blank_left', 'periscope.blank_right'],
    'curtain.cab': ['rail.curtain_cab'],
    'radio.r123m_left': ['cover.r123m_left_canvas', 'cable.r123m_left_braided', 'psu.bp26_left', 'mount.r123m_left'],
    'radio.r123m_right': ['cover.r123m_right_canvas', 'strap.r123m_right_leather', 'cable.r123m_right_braided',
                          'cable.r123m_right_white', 'psu.bp26_right', 'mount.r123m_right'],
    'sponson.left_radio_well': ['strap.left_well', 'hook.left_well_eyebolt',
                                'bracket.left_well_strip', 'switch.left_well_red_lever_box', 'loom.left_well_floor'],
    'instrument.ga3': ['bracket.ga3_pedestal', 'rail.ga3_guard'],
    'hull.door_rear': ['lining.door_rear_seal', 'control.door_rear_latch_upper', 'control.door_rear_latch_lower',
                       'handle.door_rear', 'bracket.door_rear_hinges', 'plate.door_seal_positions', 'plate.door_warning',
                       'plate.gom'],
    'pylon.turret': ['cover.turret_column_skirt', 'lining.turret_ring_seal_strip', 'motor.turret_azimuth_drive',
                     'box.turret_underside_switchbox', 'control.turret_manual_rod', 'mount.turret_bump_stops',
                     'unknown.pylon_hanging_drum'],
    'box.fire_control': ['instrument.fire_pressure_gauge', 'kit.fire_system_bottles', 'shelf.fire_box_perforated',
                         'hose.fire_system_blue'],
    'heater.cab': ['box.heater_control', 'plate.heater_lever_diagram', 'plate.heater_instructions',
                   'duct.heater_commander_footwell', 'duct.heater_elbow_outlet', 'duct.heater_diffuser_cone',
                   'clamp.heater_duct_band'],
}
# Seen, but not part of the 1980s fit the game builds: the reason.
NOT_BUILT = {
    'plate.hb_sticker_commander_seat': 'a crew\'s advertising sticker, later than the 1980s fit',
    'plate.pinup_sticker': 'a crew\'s sticker',
    'plate.sticker_wir_bauen': 'a crew\'s sticker',
    'plate.seat_back_stickers': 'later stickers (the observer\'s note)',
    'plate.sticker_step_cover_left': 'a crew\'s sticker',
    'plate.type_plate_roof': 'the 9A33BM3-1\'s type plate; the AK carries its own (decision D1): build it with the AK\'s '
                             'type once a source gives it',
    'kit.black_cloth': 'loose cloth of the museum vehicle',
}
# Places a person set from the photos where neither the cabin data nor Fig. 3 gives them (centre, size, sigma, why).
PLACES = {
    'box.zczz1a_2': ([0.7175, 2.955, 1.3575], [0.135, 0.67, 0.375], 0.05,
                     'g082-b\'s measured camera (docs/OSA_INTERIOR_HOLISTIC.md section 42): its aft end and inboard face in '
                     'the right R-123M\'s front\'s plane, 0.355 m high under the set; its length Fig. 3\'s 0.67 m; the cabin '
                     'builds it in rack_sp1m'),
    'sponson.left_radio_well': ([-0.9967, 3.04, 0.82], [0.3, 0.72, 0.4], 0.08,
                                'the well under SL-1M the left R-123M stands in (g091-d), plan from Fig. 3'),
    'cover.radio_steel': ([-0.505, 2.925, 1.3785], [0.47, 0.75, 0.006], 0.05,
                          'the tread cover over the left R-123M and its BP-26 behind the driver\'s seat, through g082-b\'s '
                          'measured camera (docs/OSA_INTERIOR_HOLISTIC.md section 42); the cabin builds it as radio_tray_left'),
    'curtain.cab': ([0.0, 2.57, 1.38], [1.5, 0.05, 1.4], 0.1,
                    'hangs from the roof at the posts between SP-1M and SP-2M and between SL-1M and SL-2M: bunched '
                    'against the right wall just aft of the right R-123M (g090-a), at SP-2M\'s forward edge (g093-b), '
                    'over the left console\'s forward end (g094-a); it closes the operators off from the front'),
    'rail.curtain_cab': ([0.0, 2.57, 2.06], [1.5, 0.03, 0.03], 0.1, 'the curtain\'s rail under the roof (as curtain.cab)'),
}
RACK_OF_PREFIX = {'l_': 'rack.sl1m', 'p_': 'rack.sl3m', 'r_': 'console.sl2m_range', 's_': 'console.sl2m_search',
                  'g_': 'console.sl2m_guidance', 'sp1_': 'rack.sp1m', 'sp2_': 'rack.sp2m', 'sp3_': 'rack.sp3m'}


def seed():
    cab = load(CABIN)
    reg = {}

    def add(e):
        assert e['id'] not in reg, e['id']
        e.setdefault('names', [])
        e.setdefault('parent', None)
        e.setdefault('sources', [])
        e.setdefault('conflicts', [])
        e.setdefault('cabin', [])
        e.setdefault('notes', '')
        reg[e['id']] = e

    panels = {p['id']: p for p in cab['panels']}
    for sid, (rid, cls, zone) in CABIN_SHELL.items():
        s = next(p for p in cab['shell'] if p['id'] == sid)
        yaw = math.radians(s.get('yaw_deg', 0))
        sx, sy, sz = s['size_m']
        size = [round(abs(math.cos(yaw)) * sx + abs(math.sin(yaw)) * sy, 4),
                round(abs(math.sin(yaw)) * sx + abs(math.cos(yaw)) * sy, 4), sz]   # the turned part's upright box
        add({'id': rid, 'class': cls, 'name': s['label'], 'zone': zone,
             'box_m': dict({'centre': s['centre_m'], 'size': size, 'sigma_m': 0.03, 'from': 'cabin:' + sid},
                           **({'yaw_deg': s['yaw_deg'], 'size_local': [sx, sy, sz]} if s.get('yaw_deg') else {})),
             'cabin': [sid], 'status': STATUS_OF_MESH.get(s.get('mesh'), 'modelled'),
             'sources': [s.get('source', 'cabin data')] if s.get('source') else ['cabin data']})
    for pid, rid in PANEL_REG.items():
        p = panels[pid]
        if rid in reg:
            reg[rid]['cabin'].append(pid)
            continue
        zone = 'cab.dash' if pid in ('driver_panel', 'commander_panel', 'pu303_panel') else (
            'cab.right_wall' if pid == 'aiming_circle_case' else
            'compartment.rear' if pid == 'pwr_cabinet' else 'compartment.left_wall')
        add({'id': rid, 'class': 'console' if rid.startswith('console') else 'box' if rid.startswith('box') else 'rack',
             'name': p['label'], 'zone': zone, 'box_m': dict(_panel_box(p), sigma_m=0.03, **{'from': 'cabin:' + pid}),
             'cabin': [pid], 'status': 'modelled', 'sources': ['cabin data (' + pid + ')']})
    reg['rack.sp2m']['status'] = 'face_traced'
    reg['rack.sp2m']['sources'].append('wwp_gecko g092-a, g093-b (tools/blender/sp2m_trace.py)')
    for p in ('so_panel', 'op_panel', 'fo_panel'):
        reg[PANEL_REG[p]]['parent'] = 'rack.sl2m'
        reg[PANEL_REG[p]]['status'] = 'face_traced'
        reg[PANEL_REG[p]]['sources'].append('samsim_panels (1 mm a pixel)')
    for b in cab['blocks']:
        pref = next((k for k in sorted(RACK_OF_PREFIX, key=len, reverse=True) if b['id'].startswith(k)), None)
        if pref is None:
            continue
        p = panels[b['panel']]
        f = math.radians(p['facing_deg'])
        r = (-math.cos(f), math.sin(f))
        n = (math.sin(f), math.cos(f))
        cx, cy, cz = p['centre_m']
        cx, cy = cx - n[0] * 0.125, cy - n[1] * 0.125     # the block's 0.25 m body stands behind its face
        u, v = b['at_m']
        rid = 'block.' + b['id']
        add({'id': rid, 'class': 'block', 'name': b['label'], 'names': [b['label']], 'zone': reg[RACK_OF_PREFIX[pref]]['zone'],
             'parent': RACK_OF_PREFIX[pref],
             'box_m': {'centre': [round(cx + r[0] * u, 4), round(cy + r[1] * u, 4), round(cz + v, 4)],
                       'size': [round(abs(r[0]) * b['size_m'][0] + 0.25 * abs(r[1]), 4),
                                round(abs(r[1]) * b['size_m'][0] + 0.25 * abs(r[0]), 4), b['size_m'][1]],
                       'sigma_m': 0.003 if pref == 'sp2_' else 0.03, 'from': 'cabin:' + b['id']},
             'cabin': [b['id']], 'status': 'face_traced' if pref in ('sp2_', 'r_', 's_', 'g_') else 'placed',
             'sources': ['mon_2112_80 Fig. 3 (block names)']})
    for rid, cls, name, names, zone, (sheet, px) in PLAN_ITEMS:
        x0, x1, y0, y1 = plan_box(sheet, px)
        add({'id': rid, 'class': cls, 'name': name, 'names': names, 'zone': zone, 'plan': {'sheet': sheet, 'px': px},
             'box_m': {'centre': [round((x0 + x1) / 2, 3), round((y0 + y1) / 2, 3), None], 'size': [round(x1 - x0, 3), round(y1 - y0, 3), None],
                       'sigma_m': 0.05, 'from': 'plan'},
             'status': 'missing', 'sources': ['mon_2112_80 Fig. 3']})
    for rid, cls, name, names, zone, (sheet, sec, px) in ELEV_ITEMS:
        add({'id': rid, 'class': cls, 'name': name, 'names': names, 'zone': zone,
             'elev': {'sheet': sheet, 'section': sec, 'px': px},
             'box_m': {'centre': [None, None, round(elev_z(sheet, sec, (px[1] + px[3]) / 2), 3)], 'size': [None, None, None],
                       'sigma_m': 0.15, 'from': 'section'},
             'status': 'missing', 'sources': ['mon_2112_80 Fig. 3 section ' + sec]})
    for rid, cls, name, zone, src in NAMED:
        e = {'id': rid, 'class': cls, 'name': name, 'zone': zone, 'box_m': None, 'status': 'missing', 'sources': [src]}
        if src.startswith('cabin data'):
            e['status'] = 'modelled'
            e['cabin'] = ['hull' if 'glass' not in src else 'glass']
        add(e)
    for rid, (sheet, px) in PLAN_OF.items():
        reg[rid]['plan'] = {'sheet': sheet, 'px': px}
    reg['console.pu303']['names'] = ['ПУ-303', 'PU-303', 'Pulpit sterowniczy 9I120']
    for rid, text in CONFLICTS.items():
        reg[rid]['conflicts'].append(text)
    return {'about': 'The 9A33BM2 compartment\'s components, one entry each (docs/OSA_INTERIOR_HOLISTIC.md). Seeded 8 Nov 2026 '
                     'from data/cabins/osa_ak.json, mon_2112_80 Fig. 3 and the book\'s notes; evidence from the photos is '
                     'merged by tools/recon/holistic.py fuse.',
            'frame': 'body: x right, y forward, z up, metres', 'components': list(reg.values())}


# ------------------------------------------------------------------------------------------------ checks

def check_register(doc):
    errs = []
    ids = set()
    for e in doc['components']:
        i = e.get('id', '?')
        if i in ids:
            errs.append('duplicate id ' + i)
        ids.add(i)
        if e.get('class') not in CLASS_GROUP:
            errs.append('%s: unknown class %r' % (i, e.get('class')))
        if e.get('zone') not in ZONES:
            errs.append('%s: unknown zone %r' % (i, e.get('zone')))
        if e.get('status') not in STATUS:
            errs.append('%s: unknown status %r' % (i, e.get('status')))
        if not e.get('sources'):
            errs.append('%s: no source' % i)
    for e in doc['components']:
        if e.get('parent') and e['parent'] not in ids:
            errs.append('%s: parent %s not in the register' % (e['id'], e['parent']))
        for rel in e.get('relations', []):
            if rel[0] not in RELATIONS or rel[1] not in ids:
                errs.append('%s: bad relation %r' % (e['id'], rel))
    return errs


def check_observation(o, ids):
    errs = []
    name = o.get('photo', '?')
    for k in ('photo', 'camera', 'components'):
        if k not in o:
            errs.append('%s: no %s' % (name, k))
    seen = set()
    for c in o.get('components', []):
        cid = c.get('id', '')
        tag = '%s/%s' % (name, cid)
        if not cid or (cid not in ids and not cid.startswith('new.')):
            errs.append('%s: id neither in the register nor new.*' % tag)
        if cid in seen:
            errs.append('%s: listed twice' % tag)
        seen.add(cid)
        if c.get('class') not in CLASS_GROUP:
            errs.append('%s: unknown class %r' % (tag, c.get('class')))
        if c.get('zone') not in ZONES:
            errs.append('%s: unknown zone %r' % (tag, c.get('zone')))
        if c.get('confidence') not in CONFIDENCE:
            errs.append('%s: confidence %r' % (tag, c.get('confidence')))
        b = c.get('bbox')
        if not (isinstance(b, list) and len(b) == 4 and 0 <= b[0] < b[2] <= 1 and 0 <= b[1] < b[3] <= 1):
            errs.append('%s: bbox %r' % (tag, b))
        for rel in c.get('relations', []):
            if not (isinstance(rel, list) and len(rel) == 2 and rel[0] in RELATIONS):
                errs.append('%s: relation %r' % (tag, rel))
    return errs


def observations():
    return [load(p) for p in sorted(glob.glob(os.path.join(OBS_DIR, '*.json')))]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('cmd', choices=['seed', 'check', 'fuse', 'draw', 'report'])
    ap.add_argument('--force', action='store_true')
    a = ap.parse_args()
    if a.cmd == 'seed':
        if os.path.exists(REGISTER) and not a.force:
            sys.exit('the register exists; --force to overwrite it')
        doc = seed()
        save(REGISTER, doc)
        print('seeded', len(doc['components']), 'components')
    elif a.cmd == 'check':
        doc = load(REGISTER)
        errs = check_register(doc)
        ids = {e['id'] for e in doc['components']}
        obs = observations()
        for o in obs:
            errs += check_observation(o, ids)
        for e in errs:
            print(e)
        print('%d components, %d observation files, %d problem(s)' % (len(ids), len(obs), len(errs)))
        sys.exit(1 if errs else 0)
    else:
        import holistic_fuse as F
        getattr(F, a.cmd)()


if __name__ == '__main__':
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    main()
