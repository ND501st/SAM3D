"""The lofted shell's numbers (data/cabins/osa_ak_shell.json, roadmap I1): every value sourced, and the shape agrees with
the cabin it closes over. The Blender builder reads the same file; no Blender here."""
import json
import math
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def _load(*p):
    with open(os.path.join(REPO, *p), encoding='utf-8') as f:
        return json.load(f)


SHELL = _load('data', 'cabins', 'osa_ak_shell.json')
CAB = _load('data', 'cabins', 'osa_ak.json')
CONF = {'high', 'medium', 'low'}


def lining(x):
    pts = sorted((p['x_m'], p['z_m']) for p in SHELL['roof_section']['half'])
    x = abs(x)
    if x <= pts[0][0]:
        return pts[0][1]
    for (x0, z0), (x1, z1) in zip(pts, pts[1:]):
        if x <= x1:
            return z0 + (z1 - z0) * (x - x0) / (x1 - x0)
    return pts[-1][1]


def test_every_value_has_a_source_and_a_confidence():
    for p in SHELL['roof_section']['half']:
        assert p['source'] and p['what'] and p['confidence'] in CONF, p
    for k in ('beam', 'turret_opening', 'wheel_arch_right'):
        assert SHELL[k]['source'] and SHELL[k]['what'] and SHELL[k]['confidence'] in CONF, k
    bh = SHELL['bulkhead']
    parts = [SHELL[k] for k in ('rear_cove', 'cab_front')] + [bh[k] for k in bh if k != 'about']
    parts += [v for k, v in SHELL['floor'].items() if k not in ('not_built', 'raised')] + SHELL['floor']['raised']
    for q in parts:
        assert q['source'] and q['what'] and q['confidence'] in CONF, q


def test_the_section_is_a_lining_over_the_racks():
    half = sorted(SHELL['roof_section']['half'], key=lambda p: p['x_m'])
    assert half[0]['x_m'] == 0.0 and half[-1]['x_m'] > 0.7
    zs = [p['z_m'] for p in half]
    assert zs == sorted(zs, reverse=True)                    # it rises inboard, to the panel at the centre line
    left = SHELL['roof_section']['left_outboard']             # raised over SŁ-2M's top rail (g094-a, g094-b, g095-a)
    assert left['z_m'] == lining(left['x_from_m']) and left['source'] and left['confidence'] in CONF
    for s in CAB['shell']:
        if s['id'].startswith('rack_'):
            top = s['centre_m'][2] + s['size_m'][2] / 2
            limit = left['z_m'] if s['centre_m'][0] < 0 else SHELL['roof_section']['outboard_z_m']
            assert top <= limit + 1e-6, s['id']
    sl2m = next(p for p in CAB['panels'] if p['id'] == 'so_panel')
    assert sl2m['centre_m'][2] + sl2m['size_m'][1] / 2 < left['z_m'] - 0.03    # the top rail fits under the lining


def test_nothing_hangs_clear_of_the_lining_and_nothing_goes_through_it():
    for s in CAB['shell']:
        if s.get('mesh') == 'SM_DomeLamp':
            x, y, z = s['centre_m']
            top = z + s['size_m'][2] / 2
            limit = lining(x) if y < SHELL['roof_section']['y_to_m'] else 2.10
            assert abs(top - limit) < 0.01, (s['id'], top, limit)


def test_the_beam_and_the_opening_sit_together():
    b, o = SHELL['beam'], SHELL['turret_opening']
    assert b['y_aft_m'] < b['y_forward_m'] and b['z_bottom_m'] >= SHELL['roof_section']['outboard_z_m'] - 1e-6
    assert b['y_aft_m'] <= o['centre_xy_m'][1] + o['half_y_m'] <= b['y_forward_m']      # the opening's forward rim is behind the band (g096-a, docs/OSA_INTERIOR_HOLISTIC.md section 33)
    assert o['half_x_m'] <= max(p['x_m'] for p in SHELL['roof_section']['half'])
    assert o['boot']['centre_xy_m'] == o['centre_xy_m']                         # the canvas boot hangs from it


def test_the_band_shows_its_whole_height_because_the_lining_before_it_is_higher():
    b, p = SHELL['beam'], SHELL['roof_pocket']
    assert p['y_from_m'] == b['y_forward_m'] and p['y_to_m'] > p['y_from_m']
    assert p['z_m'] > max(q['z_m'] for q in SHELL['roof_section']['half'])      # above the roof panel, or the band would be hidden behind it
    assert p['x_from_m'] <= b.get('centre_x_m', 0) - b['half_x_m'] and b.get('centre_x_m', 0) + b['half_x_m'] <= p['x_to_m']
    assert p['confidence'] == 'low'                                             # only its lower bound is seen


def test_the_hull_entry_is_the_built_mesh():
    m = _load('art', 'cabin_interior', 'manifest.json')['SM_OsaHull']
    h = [s for s in CAB['shell'] if s['id'] == 'hull'][0]
    assert all(abs(a - b) < 1e-3 for a, b in zip(h['centre_m'], m['centre_m']))
    assert all(abs(a - b) < 1e-3 for a, b in zip(h['size_m'], m['size_m']))


def _part(pid):
    return [s for s in CAB['shell'] if s['id'] == pid][0]


def test_the_beam_stops_short_of_the_hidden_edge():
    half = sorted(SHELL['roof_section']['half'], key=lambda p: p['x_m'])
    assert half[-2]['x_m'] < SHELL['beam']['half_x_m'] < half[-1]['x_m']


def test_the_door_stands_high_with_its_fittings_on_it():
    bh = SHELL['bulkhead']
    d, f = bh['door'], bh['door_fittings']
    assert abs(d['x_outboard_m'] - 0.59) < 0.02 and d['x_outboard_m'] - d['width_m'] > -0.02     # the plan's opening, x 0 to 0.59
    assert 1.3 < d['height_m'] / d['width_m'] < 1.45                                            # g100-a's proportion
    bottom = d['z_top_m'] - d['height_m']
    assert bh['flap']['z_to_m'] < bh['lower_step']['z_top_m'] < bottom
    assert d['z_top_m'] + bh['door_frame']['width_m'] <= bh['ledge']['z_bottom_m'] + 1e-6
    for t in f['channels_from_top_m']:
        assert f['channel_height_m'] / 2 < t < d['height_m'] - f['channel_height_m'] / 2
    for row in f['hinge_pads_from_edge_top_m']:
        assert row[1] < d['width_m'] / 2                                                       # the hinges on the outboard side
    for row in f['dogs_from_edge_top_m']:
        assert row[0] > d['width_m'] / 2 and row[0] < d['width_m']                             # the latches on the centre line's


def test_the_blue_lamp_is_on_the_wall_clear_of_the_door_and_the_rack():
    bh = SHELL['bulkhead']
    d = bh['door']
    lamp, rack = _part('lamp_blue'), _part('rack_sp3m')
    x, _, z = lamp['centre_m']
    assert x > d['x_outboard_m'] or z - lamp['size_m'][2] / 2 > d['z_top_m']                   # the lamp is not on the door's plate
    assert x + lamp['size_m'][0] / 2 <= next(q for q in CAB['panels'] if q['id'] == 'sp3m_panel')['centre_m'][0]   # nor inside the rack, whose foot rakes inboard below it
    light = _part('light_blue')
    assert light['centre_m'][0] == x and light['centre_m'][2] == z


def test_the_cab_roof_runs_on_to_the_band_ahead_of_the_hatches():
    y_to = SHELL['roof_section']['y_to_m']
    cab = SHELL['cab_front']
    assert cab['y_m'] == y_to and 3.8 + 0.3 < y_to                                             # forward of the hatches (HATCH_Y, 0.6 long)
    assert y_to + cab['depth_m'] < cab['lip']['y_from_m'] < 4.55 + 0.05                        # the lip is ahead of the band, at the screens' head


def test_the_floor_parts_leave_the_aisle_walkable():
    f = SHELL['floor']
    boxes = [(min(p[0] for p in f['plinth_aft_right']['section_xz_m']), max(p[0] for p in f['plinth_aft_right']['section_xz_m']),
              f['plinth_aft_right']['y_from_m'], f['plinth_aft_right']['y_to_m']),
             (f['step_forward_right']['x_from_m'], f['step_forward_right']['x_to_m'], f['step_forward_right']['y_from_m'], f['step_forward_right']['y_to_m']),
             (f['arch_forward_right']['x_from_m'], f['arch_forward_right']['x_to_m'], 0.3, 0.85)]
    for w in CAB['walk']:
        (wx0, wy0), (wx1, wy1) = w['from_m'], w['to_m']
        for x0, x1, y0, y1 in boxes:
            assert x1 <= wx0 or x0 >= wx1 or y1 <= wy0 or y0 >= wy1, (w, x0, y0)


def test_the_0096_cabinet_stands_as_the_photos_have_it_and_its_panel_is_its_door():
    c = _part('cabinet_0096')
    assert c['yaw_deg'] == -16.4 and 'D1' in c['notes']          # its door as a control plate solved with certified cameras (docs/research/OSA_REBUILD_C_2026-11.md); -15.7 before, from the joint solve's points on its face (docs/OSA_INTERIOR_HOLISTIC.md section 33)
    p = [x for x in CAB['panels'] if x['id'] == 'pwr_cabinet'][0]
    assert p['bare'] and p['facing_deg'] == c['yaw_deg']
    n = (math.sin(math.radians(c['yaw_deg'])), math.cos(math.radians(c['yaw_deg'])))
    before = (p['centre_m'][0] - c['centre_m'][0]) * n[0] + (p['centre_m'][1] - c['centre_m'][1]) * n[1]
    assert 0.05 < before < 0.1                                   # the door stands before the mesh's origin, in the direction it faces
    sys.path.insert(0, os.path.join(REPO, 'tools', 'blender'))
    import cabinet0096_trace as T
    for ctl, key in (('pwr.equipment', 'pwr.equipment'), ('stab.power', 'stab.power'), ('drv.az', 'drv.az'), ('drv.el', 'drv.el'), ('soc.rotation_lock', 'soc.rotation_lock'), ('tov.supply', 'tov.supply')):
        at = [x['at_m'] for x in CAB['controls'] if x['control'] == ctl][0]
        assert at == list(T.panel_at(key)), ctl                  # on the traced buttons


def test_the_floor_follows_the_photos_section():
    f = SHELL['floor']
    sp, tr, bb = f['spine'], f['trough_right'], f['bench_base_bays']
    assert sp['x_to_m'] == tr['x_from_m']                       # the dark strip starts where the sheet ends
    assert sp['z_top_m'] > tr['z_top_m'] + 0.1                  # the walkway stands above the dark strip (the depth maps, docs/OSA_INTERIOR_HOLISTIC.md section 31)
    assert 0.65 < sp['forward_step']['z_top_m'] < sp['z_top_m'] < 0.86   # two levels, read off the depth comparison (section 33)
    assert sp['x_from_m'] < sp['channel']['x_from_m'] < sp['channel']['x_to_m'] < sp['x_to_m'] and 0 < sp['channel']['drop_m'] < 0.15
    assert bb['x_face_m'] == sp['x_from_m']                     # the sheet runs to the bench's base
    assert sp['z_top_m'] < bb['z_top_m'] <= 0.98                # stands on the walkway, up to the bench's top
    r0, r1 = sp['ribbed_from_right']
    assert 0 < r0 < r1 < 1
    for k in ('spine', 'trough_right', 'bench_base_bays', 'stair_plates_aft', 'hatch_round_aft_left', 'arch_forward_right'):
        assert 'wwp_gecko' in f[k]['source'] or 'solve' in f[k]['source'], k


def test_what_the_depth_comparison_found_on_the_floor_is_closed_and_stands_on_it():
    def area(sec):
        return 0.5 * sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(sec, sec[1:] + sec[:1]))
    for r in SHELL['floor']['raised']:
        secs = [r[k] for k in ('section_yz_m', 'section_xz_m', 'section_at_x_from_yz_m', 'section_at_x_to_yz_m') if k in r]
        assert len({len(q) for q in secs}) == 1, r['id']        # a loft's two sections join point to point
        for sec in secs:
            assert area(sec) > 0.005, r['id']                   # anticlockwise, so its faces look outwards
            assert min(p[1] for p in sec) < SHELL['floor']['spine']['z_top_m'], r['id']
        assert 'holistic_diff' in r['source'] and 'wwp_gecko' in r['source'], r['id']


def test_the_stair_descends_towards_the_front():
    st = SHELL['floor']['stair_plates_aft']['steps']
    assert st[0]['y_to_m'] == st[1]['y_from_m'] and st[0]['z_top_m'] > st[1]['z_top_m'] > 0.632


def test_the_roof_details_hang_under_the_lining():
    ins = SHELL['roof_details']['insulators']
    (x0, y0, z0), (x1, y1, z1) = ins['line_xyz_m']
    assert ins['count'] == 6 and y0 > y1                        # forward to aft along the right edge, as in g098-a
    for x, z in ((x0, z0), (x1, z1)):
        assert z + ins['stem_m'] < lining(x) + 0.08
    for q in SHELL['roof_details'].values():
        if isinstance(q, dict):
            assert q['source'] and q['what'] and q['confidence'] in CONF


def test_the_posts_stand_between_the_racks():
    for side, sgn in (('right', 1), ('left', -1)):
        p = SHELL['posts'][side]
        assert 0.15 < p['y_m'][0] < p['y_m'][1] < 0.7
        z = [q[1] for q in p['x_z_m']]
        assert z == sorted(z) and z[0] == 0.62 and z[-1] >= 2.1
        assert all(sgn * q[0] > 0.3 for q in p['x_z_m'])
        foot, knee = p['x_z_m'][1], [q for q in p['x_z_m'] if q[1] > 1.5][0]
        assert 0.25 < sgn * (knee[0] - foot[0]) / (knee[1] - foot[1]) < 0.5   # the leg rakes inboard going down (0.475 from the depth maps, section 31)
        assert sgn * p['back_x_m'] > max(sgn * q[0] for q in p['x_z_m'])
        assert p['source'] and p['what'] and p['confidence'] in CONF


def test_what_is_on_the_door_lies_on_the_door():
    bh = SHELL['bulkhead']
    d, dd = bh['door'], bh['door_detail']
    x1, x0, z1 = d['x_outboard_m'], d['x_outboard_m'] - d['width_m'], d['z_top_m']
    z0 = z1 - d['height_m']
    for k in ('red_plate_xz_m', 'black_plate_xz_m'):
        q = dd[k]
        assert x0 < q[0] < q[1] < x1 and z0 < q[2] < q[3] < z1, k
    for x, z in dd['screws_xz_m'] + dd['studs_xz_m']:
        assert x0 < x < x1 and z0 < z < z1
    for q in dd['keepers_xz_m']:                                # the keepers are on the frame, past the latch edge's seal
        assert q[0] < x0 + 0.02


def test_the_hood_and_the_blower_stand_beside_the_door_and_clear_of_each_other():
    bh = SHELL['bulkhead']
    x_latch = bh['door']['x_outboard_m'] - bh['door']['width_m']
    h, b = bh['hood'], bh['blower']
    assert h['x_to_m'] <= x_latch and b['x_to_m'] < x_latch
    assert h['z_bottom_m'] > b['z_to_m']                        # the hood is above the blower's opening
    for q in (h, b, bh['door_detail']):
        assert 'wwp_gecko' in q['source'] and q['confidence'] in CONF


def test_the_boot_hangs_inside_the_opening_and_clear_of_the_door():
    o = SHELL['turret_opening']
    bo = o['boot']
    assert bo['r_top_m'] < o['half_x_m'] and bo['r_top_m'] < o['half_y_m']
    assert bo['r_bottom_m'] <= bo['r_top_m'] and bo['z_bottom_m'] < bo['z_top_m']
    assert bo['centre_xy_m'][1] - bo['r_top_m'] > -1.035 + 0.1   # stands forward of the bulkhead's fittings
    assert 'wwp_gecko' in bo['source'] and bo['confidence'] in CONF
    assert 'wwp_gecko' in o['base']['source'] and o['base']['confidence'] in CONF


def test_the_lower_wall_rake_agrees_with_what_was_cast():
    w = SHELL['lower_side_wall']
    assert 0.3 < w['rake_x_per_z'] < 0.4 and w['source'] and w['what'] and w['confidence']
    # the first-aid box's two triangulated edges lie along that wall
    assert abs((0.504 - 0.427) / (1.360 - 1.145) - w['rake_x_per_z']) < 0.03

def test_post_legs_lie_aft_of_the_upright_parts():
    # the depth check: the legs' points in the joint solve lie aft of the upright part (docs/OSA_INTERIOR_HOLISTIC.md section 29)
    shell = json.load(open(os.path.join(REPO, 'data', 'cabins', 'osa_ak_shell.json'), encoding='utf-8'))
    for side in ('left', 'right'):
        p = shell['posts'][side]
        assert p['leg_y_m'][0] < p['y_m'][0]
        assert min(z for _, z in p['x_z_m']) < p['knee_z_m'] < max(z for _, z in p['x_z_m'])
