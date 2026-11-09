"""The holistic layer (docs/OSA_INTERIOR_HOLISTIC.md): the register, the plan calibration, the observations' checks and
the fuse step's estimates."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'recon'))
import holistic as H  # noqa: E402
import holistic_fuse as F  # noqa: E402


def test_register_is_well_formed():
    doc = H.load(H.REGISTER)
    assert H.check_register(doc) == []
    ids = [e['id'] for e in doc['components']]
    assert len(ids) == len(set(ids))
    assert len(ids) >= 150


def test_every_cabin_rack_and_shell_part_is_registered():
    doc = H.load(H.REGISTER)
    cabin_ids = {c for e in doc['components'] for c in e.get('cabin', [])}
    cab = H.load(H.CABIN)
    for sid in H.CABIN_SHELL:
        assert sid in cabin_ids, sid
    for pid in H.PANEL_REG:
        assert pid in cabin_ids, pid


def test_plan_calibration_agrees_with_the_cabin():
    # Fig. 3's boxes of components the cabin data places by other means: within 6 cm (docs/OSA_INTERIOR_HOLISTIC.md 6)
    doc = H.load(H.REGISTER)
    by = {e['id']: e for e in doc['components']}
    for rid in ('radio.r123m_right', 'box.zczz1a_1', 'box.cgw10p', 'box.awo5m1', 'console.pu303'):
        sheet, px = by[rid]['plan']['sheet'], by[rid]['plan']['px']
        x0, x1, y0, y1 = H.plan_box(sheet, px)
        cx, cy = by[rid]['box_m']['centre'][:2]
        panel = [p for p in H.load(H.CABIN)['panels'] if 'cabin:' + p['id'] == by[rid]['box_m']['from']]
        if panel:   # a panel's box reaches back behind its face; Fig. 3 draws the face
            cx, cy = panel[0]['centre_m'][:2]
        assert abs((x0 + x1) / 2 - cx) < 0.06 and abs((y0 + y1) / 2 - cy) < 0.06, (rid, (x0 + x1) / 2, (y0 + y1) / 2, cx, cy)
    for rid in ('radio.r123m_left', 'rack.0096_9m', 'phone.ta57', 'instrument.ga3'):   # the known disagreements are on record
        assert by[rid]['conflicts'], rid


def test_plan_sheets_meet():
    # SŁ-2M's front end is the same body y on both sheets
    assert abs(H.plan_to_body('p028', 960, 909)[1] - H.plan_to_body('p029', 95, 917)[1]) < 1e-9
    assert H.plan_to_body('p028', 960, 909)[0] == 0.0


def _obs(**over):
    c = {'id': 'rack.sp2m', 'class': 'rack', 'seen_as': 'grey rack', 'bbox': [0.1, 0.1, 0.6, 0.9],
         'zone': 'compartment.right_wall', 'relations': [['forward_of', 'rack.sp3m']], 'confidence': 'high'}
    c.update(over)
    return {'photo': 'test', 'camera': {}, 'components': [c]}


def test_observation_checks():
    ids = {e['id'] for e in H.load(H.REGISTER)['components']}
    assert H.check_observation(_obs(), ids) == []
    assert H.check_observation(_obs(id='new.bracket.x', **{'class': 'bracket'}), ids) == []
    assert H.check_observation(_obs(id='bracket.x'), ids)            # neither registered nor new.*
    assert H.check_observation(_obs(bbox=[0.6, 0.1, 0.2, 0.9]), ids)  # x0 > x1
    assert H.check_observation(_obs(zone='attic'), ids)
    assert H.check_observation(_obs(confidence='sure'), ids)
    assert H.check_observation(_obs(relations=[['near', 'rack.sp3m']]), ids)


def test_estimate_from_relations():
    comps = [
        {'id': 'a', 'class': 'rack', 'zone': 'compartment.right_wall', 'box_m': {'centre': [1.0, 1.0, 1.4], 'size': [0.3, 1.0, 1.2], 'sigma_m': 0.01}},
        {'id': 'b', 'class': 'fan', 'zone': 'compartment.right_wall', 'box_m': None, 'relations': [['above', 'a', 2]]},
        {'id': 'c', 'class': 'cable', 'zone': 'compartment.right_wall', 'box_m': None, 'relations': [['on', 'b', 1]]},
        {'id': 'd', 'class': 'plate', 'zone': 'cab.dash', 'box_m': None},
    ]
    by = {e['id']: e for e in comps}
    F._estimate(comps, by)
    b = by['b']['estimate']['centre']
    assert abs(b[0] - 1.0) < 1e-6 and abs(b[1] - 1.0) < 1e-6 and b[2] > 2.0 - 1e-6
    assert by['c']['estimate']['centre'] == by['b']['estimate']['centre']   # propagated
    assert by['d']['estimate']['from'] == 'zone' and by['d']['estimate']['centre'][1] == F.ZONE_Y['cab.dash']


def test_contradiction_found():
    comps = [
        {'id': 'a', 'class': 'box', 'zone': 'cab', 'box_m': {'centre': [0, 0, 1.0], 'size': [0.1, 0.1, 0.1], 'sigma_m': 0.01},
         'relations': [['above', 'b', 1]]},
        {'id': 'b', 'class': 'box', 'zone': 'cab', 'box_m': {'centre': [0, 0, 1.6], 'size': [0.1, 0.1, 0.1], 'sigma_m': 0.01}},
    ]
    F._contradictions(comps, {e['id']: e for e in comps})
    assert comps[0]['observed_conflicts'] and 'above b' in comps[0]['observed_conflicts'][0]


def test_observation_files_are_valid():
    doc = H.load(H.REGISTER)
    ids = {e['id'] for e in doc['components']}
    for o in H.observations():
        assert H.check_observation(o, ids) == [], o['photo']


def test_review_names_only_registered_ids():
    ids = {e['id'] for e in H.load(H.REGISTER)['components']}
    for parent, children in H.PARENTS.items():
        for c in [parent] + children:
            assert c in ids, c
    for rid in list(H.NOT_BUILT) + list(H.PLACES):
        assert rid in ids, rid
    for target in H.ALIASES.values():
        assert target.startswith('new.') or target in ids, target


def test_radios_sit_where_the_photos_put_them():
    # H3 (9 Nov 2026): the right R-123M on ZCzZ1A nr 2's lid (g090-a), the left one low under its steel tray (g091-d)
    cab = H.load(H.CABIN)
    shell = {p['id']: p for p in cab['shell']}
    reg = {e['id']: e for e in H.load(H.REGISTER)['components']}
    right, left = shell['radio_right'], shell['radio_left']
    box = reg['box.zczz1a_2']['box_m']
    assert abs(right['centre_m'][2] - right['size_m'][2] / 2 - (box['centre'][2] + box['size'][2] / 2)) < 0.05
    tray = reg['cover.radio_steel']['box_m']
    assert left['centre_m'][2] + left['size_m'][2] / 2 < tray['centre'][2]
    assert abs(left['centre_m'][1] - 3.03) < 0.01          # Fig. 3's plan


def test_late_vehicle_gives_no_relations():
    # g101 is a late Czech vehicle: it may confirm a component, never place one
    for e in H.load(H.REGISTER)['components']:
        assert not (e.get('from_observations') and all(len(s) > 2 for s in e.get('seen_in', [])) and e.get('seen_in')), e['id']
