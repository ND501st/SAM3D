"""The PU-12M interior's trace (tools/blender/pu12m_interior_trace.py) is consistent before anything is modelled on it: the
plan's scale gives the published dimensions, every item stands inside the hull clear of the others, each seat has room
for the head above it and its eye point over it, the mast is where the exterior has it, and the cabin layout written from
the trace (tools/pu12m_cabin.py) is up to date."""
import itertools
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / 'tools' / 'blender'))
sys.path.insert(0, str(ROOT / 'tools'))
import pu12m_interior_trace as I  # noqa: E402
import pu12m_trace as T  # noqa: E402

# Items that are built into one another: the mast's winch on the mast, the R-123M on the R-111, the bench and its back.
TOUCHING = {frozenset(p) for p in (('mast', 'winch'), ('r111', 'r123_fug2'), ('bench', 'bench_back'),
                                       ('r111_stand', 'r111_unit'))}


def corners(it):
    return [(x, y, z) for x in (it['x0'], it['x1']) for y in (it['y0'], it['y1']) for z in (it['z0'], it['z1'])]


def overlap(a, b, gap=0.0):
    return all(a[k + '0'] < b[k + '1'] + gap - 1e-9 and b[k + '0'] < a[k + '1'] + gap - 1e-9 for k in 'xyz')


def test_the_plans_scale_gives_the_published_dimensions():
    assert abs(I.px_x(I.PLAN_BOW_PX) - T.BOW_X) < 1e-9
    assert abs(I.px_x(I.PLAN_STERN_PX) - T.STERN_X) < 0.01
    assert abs(I.py_y(I.PLAN_CHINE_PY[1]) - I.py_y(I.PLAN_CHINE_PY[0]) - T.WIDTH) < 1e-9
    # the plan is drawn a little longer than wide against the published ratio; within 6 %
    assert 0.94 < I.SY / I.SX < 1.10


def test_every_item_is_inside_the_hull():
    for it in I.ITEMS:
        out = [c for c in corners(it) if not I.inside(*c)]
        assert not out, (it['id'], out[0])


def test_no_two_items_overlap():
    for a, b in itertools.combinations(I.ITEMS, 2):
        if frozenset((a['id'], b['id'])) in TOUCHING:
            continue
        assert not overlap(a, b), (a['id'], b['id'])


def test_every_item_has_its_plan_number_and_the_plan_is_covered():
    numbered = {it['no'] for it in I.ITEMS if it['no']}
    # the plan's items inside the compartment (the whips, 3 and 28, are outside; 24, the plotting board, is the IT-45's
    # glass; 14, the UO amplifiers, is inside the IT-45's block; 27 is not in the legend)
    expected = set(range(1, 54)) - {3, 14, 24, 27, 28}
    assert expected <= numbered, sorted(expected - numbered)


def test_the_seats_and_eye_points():
    for sid, s in I.STATIONS.items():
        x, y, z = s['eye']
        assert I.inside(x, y, z, margin=0.12), sid          # the head has room under the roof and beside the sides
        assert z + 0.12 < I.ceiling_z(x), sid
        for it in I.ITEMS:                                   # nothing where the head is
            head = {'x0': x - 0.10, 'x1': x + 0.10, 'y0': y - 0.09, 'y1': y + 0.09, 'z0': z - 0.12, 'z1': z + 0.12}
            assert not overlap(head, it), (sid, it['id'])
    for sid, seat in (('driver', 'seat_driver'), ('squad_leader', 'seat_squad_leader')):
        it, (x, y, _) = I.item(seat), I.STATIONS[sid]['eye']
        assert it['x0'] <= x <= it['x1'] and it['y0'] <= y <= it['y1'], sid
    bench = I.item('bench')
    for sid, (y0, y1) in zip(('battery_commander', 'operator', 'radio_operator'), I.BENCH['places']):
        x, y, _ = I.STATIONS[sid]['eye']
        assert y0 <= y <= y1 and bench['x0'] - 0.05 <= x <= bench['x1'], sid


def test_the_commander_faces_the_it45_and_the_plotter_his_map():
    s, (x, y, z) = I.IT45_SCREEN, I.STATIONS['battery_commander']['eye']
    d = s['x'] - x
    assert 0.45 < d < 0.85                                   # reading distance
    assert abs(s['y'] - y) < 0.15 and -0.5 < s['z'] - z < 0.0
    assert 0.38 < s['d'] < 0.46                              # a 45 cm tube
    mx, my, mz = I.STATIONS['operator']['eye']
    board = I.item('map_board')
    assert 0.35 < board['x1'] - mx < 0.6 and board['y0'] < my < board['y1'] and board['z0'] < mz
    keypad = I.item('dch')
    assert keypad['y0'] > s['y'] + s['d'] / 2 - 0.01         # the ДХ keypad at the tube's left (shilka_narod)
    fr = I.item('fr_block')
    assert fr['y1'] < s['y'] - s['d'] / 2                     # the ФР block at its right


def test_the_mast_is_where_the_exterior_has_it_and_clear_of_the_hatches():
    m = I.item('mast')
    assert abs(0.5 * (m['x0'] + m['x1']) - T.MAST_X) < 1e-9 and abs(0.5 * (m['y0'] + m['y1']) - T.MAST_Y) < 1e-9
    r = T.MAST_SECTIONS[0][0] + 0.05
    for hx, hy, lx, ly in T.ROOF_HATCHES:
        apart = abs(T.MAST_X - hx) > lx / 2 + r or abs(T.MAST_Y - hy) > ly / 2 + r
        assert apart, (hx, hy)


def test_the_cabin_layout_is_up_to_date():
    if not (ROOT / 'art' / 'pu12m_interior' / 'manifest.json').exists():
        return
    import pu12m_cabin
    assert pu12m_cabin.main(['--check']) == 0


def test_the_panels_stand_on_their_items_faces():
    import json
    layout = json.loads((ROOT / 'data' / 'cabins' / 'pu12m.json').read_text(encoding='utf-8'))
    for p in layout['panels']:
        if p.get('bare'):
            continue                                            # an R-123M's own plate, inside its mesh's box
        bx, by, bz = p['centre_m']
        x, y, z = by, -bx, bz                                   # body to trace
        f = math.radians(p['facing_deg'])
        up = p.get('tilt_deg', 0) == 90                         # a top face looks up
        nx, ny = (0.0, 0.0) if up else (math.cos(f), -math.sin(f))   # the face's normal, trace X and Y
        nz = 1.0 if up else 0.0
        near = [it for it in I.ITEMS
                if it['x0'] - 0.08 <= x <= it['x1'] + 0.08 and it['y0'] - 0.08 <= y <= it['y1'] + 0.08 and
                it['z0'] - 0.02 <= z <= it['z1'] + 0.02]
        assert near, p['id']
        # the point just behind the panel is inside an item, the point just in front of it is not
        behind = (x - 0.008 * nx, y - 0.008 * ny, z - 0.008 * nz)
        front = (x + 0.008 * nx, y + 0.008 * ny, z + 0.008 * nz)

        def within(pt):
            return any(it['x0'] <= pt[0] <= it['x1'] and it['y0'] <= pt[1] <= it['y1'] and it['z0'] <= pt[2] <= it['z1']
                       for it in I.ITEMS)
        assert within(behind) and not within(front), p['id']


def test_the_panels_lie_within_their_faces_and_apart():
    """Each corner of a panel has its item behind it (a panel hanging off its block's edge floats in the air), and two
    panels in one plane do not overlap (the ФР and S blocks' panels, one above the other)."""
    import json
    layout = json.loads((ROOT / 'data' / 'cabins' / 'pu12m.json').read_text(encoding='utf-8'))
    panels = [p for p in layout['panels'] if not p.get('bare')]

    def axes(p):
        f, t = math.radians(p['facing_deg']), math.radians(p.get('tilt_deg', 0))
        n = (math.sin(f) * math.cos(t), math.cos(f) * math.cos(t), math.sin(t))
        r = (-math.cos(f), math.sin(f), 0.0)
        u = (n[1] * r[2] - n[2] * r[1], n[2] * r[0] - n[0] * r[2], n[0] * r[1] - n[1] * r[0])
        return n, r, u

    def within(bx, by, bz):
        x, y, z = by, -bx, bz                                   # body to trace
        return any(it['x0'] <= x <= it['x1'] and it['y0'] <= y <= it['y1'] and it['z0'] <= z <= it['z1'] for it in I.ITEMS)

    for p in panels:
        n, r, u = axes(p)
        c = p['centre_m']
        w, h = p['size_m']
        for su in (-1, 1):
            for sv in (-1, 1):
                a, b = su * (w / 2 - 0.015), sv * (h / 2 - 0.015)
                pt = [c[k] + r[k] * a + u[k] * b - n[k] * 0.008 for k in range(3)]
                assert within(*pt), (p['id'], su, sv)
    for i, p in enumerate(panels):
        n, r, u = axes(p)
        for q in panels[i + 1:]:
            if (p['facing_deg'], p.get('tilt_deg', 0)) != (q['facing_deg'], q.get('tilt_deg', 0)):
                continue
            d = [q['centre_m'][k] - p['centre_m'][k] for k in range(3)]
            if abs(sum(d[k] * n[k] for k in range(3))) > 0.01:
                continue                                        # parallel, not in one plane
            du, dv = sum(d[k] * r[k] for k in range(3)), sum(d[k] * u[k] for k in range(3))
            apart_u = abs(du) >= (p['size_m'][0] + q['size_m'][0]) / 2 - 0.005
            apart_v = abs(dv) >= (p['size_m'][1] + q['size_m'][1]) / 2 - 0.005
            assert apart_u or apart_v, (p['id'], q['id'])


def test_the_radio_sets_stand_at_their_meshes_size():
    """The R-111's and the R-407's boxes in the trace are their meshes' (art/pu12m_interior/manifest.json, written by
    tools/blender/build_pu12m_radios.py), and their front plates where the meshes have them."""
    import json
    manifest = json.loads((ROOT / 'art' / 'pu12m_interior' / 'manifest.json').read_text(encoding='utf-8'))
    for iid, mesh, size, plate in (('r111', 'SM_RadioR111', I.R111_SIZE, I.R111_PLATE), ('r407', 'SM_RadioR407', I.R407_SIZE, I.R407_PLATE)):
        m = manifest[mesh]
        assert all(abs(a - b) < 1e-3 for a, b in zip(m['size_m'], size)), (mesh, m['size_m'], size)
        assert abs(m['plate_m']['ahead'] - plate[0]) < 1e-3 and abs(m['plate_m']['up'] - plate[1]) < 1e-3, mesh
        it = I.item(iid)
        w, d, h = size
        dims = sorted((round(it['x1'] - it['x0'], 4), round(it['y1'] - it['y0'], 4)))
        assert dims == sorted((round(w, 4), round(d, 4))) and abs(it['z1'] - it['z0'] - h) < 1e-6, iid
