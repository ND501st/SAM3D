"""The shell's section (docs/OSA_INTERIOR_HOLISTIC.md section 25): heights above the aisle floor from tools/traces/osa_interior/shell_section.json,
beside the model's, and what the exterior's profiles say about the floor's height above the road. Writes nothing; prints the table.

    python -E -P -X utf8 tools/recon/shell_section.py
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TRACE = os.path.join(HERE, '..', 'traces', 'osa_interior', 'shell_section.json')


def load(path=TRACE):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def slope(pair):
    (y0, z0), (y1, z1) = pair
    return (z1 - z0) / (y1 - y0)


def pitch(trace):
    """The forward group's slope along y: the mean of the rack's foot and top."""
    g = trace['forward_group_pitch']
    return (slope(g['foot']) + slope(g['top'])) / 2.0


def levelled(trace, y, z):
    return z - pitch(trace) * y


def mat_level(trace):
    """The aisle mat's height in the levelled solve."""
    m = trace['mat_level']
    y, z = m['camera']
    return levelled(trace, y, z) - m['camera_over_mat_m'] / trace['solve_scale']['k']


def over_mat(trace, y, z):
    """Metres above the aisle mat: the solve's difference, levelled and put to scale."""
    return (levelled(trace, y, z) - mat_level(trace)) * trace['solve_scale']['k']


def mat_model_m(trace):
    """The seat deck's mat in the cabin's frame: the readings are heights above it."""
    d = trace['datum']
    return d['aisle_model_m'] + d['deck_above_aisle_m']


def yardsticks(trace):
    """(mean k, each k): the solve's scale from each check that gives one."""
    k = [c.get('ratio', c.get('true_m', 0.0) / c['solve_m'] if 'solve_m' in c else 0.0) for c in trace['solve_scale']['checks']]
    return sum(k) / len(k), k


def aisle_from_roof(trace):
    """(low, high): the aisle's height above the road that keeps the lining under the hull's outer roof."""
    d = trace['datum']
    top = d['outer_roof_max_m'] - d['lining_clear_of_outer_roof_m']
    return top - d['roof_above_aisle_m'][1], top - d['roof_above_aisle_m'][0]


def interior_rows(trace):
    rows = []
    base = mat_model_m(trace)
    for p in trace['interior']:
        model = None if p['model_m'] is None else p['model_m'] - base
        rows.append({'id': p['id'], 'label': p['label'], 'photo': over_mat(trace, p['y'], p['z']), 'model': model})
    for p in trace['single_view']:
        rows.append({'id': p['id'], 'label': p['label'], 'photo': p['over_mat_m'], 'model': p['model_m'] - base})
    return rows


def exterior_heights(profile, wheel_m):
    scale = (profile['ground'] - profile['tyre_top']) / wheel_m
    return {k: (profile['ground'] - v) / scale for k, v in profile['rows'].items()}


def exterior_mean(trace):
    ex = trace['exterior']
    each = [exterior_heights(p, ex['wheel_m']) for p in ex['profiles']]
    return {k: sum(e[k] for e in each) / len(each) for k in each[0]}, each


def floor_from_windows(trace):
    """The aisle floor above the road if the windscreen stands as high as the side windows: one value from each window edge."""
    mean, _ = exterior_mean(trace)
    r = {x['id']: x['photo'] for x in interior_rows(trace)}
    return mean['window_foot'] - r['screen_foot'], mean['window_top'] - r['screen_top']


def main():
    t = load()
    print('pitch of the forward group: %.4f; mat level in the levelled solve: %.3f; scale k: %.2f' % (pitch(t), mat_level(t), t['solve_scale']['k']))
    print('%-34s %10s %10s %10s' % ('above the seat deck', 'photos', 'model', 'difference'))
    for r in interior_rows(t):
        if r['model'] is None:
            print('%-34s %+10.2f %10s %10s' % (r['label'], r['photo'], '-', '-'))
        else:
            print('%-34s %+10.2f %+10.2f %+10.2f' % (r['label'], r['photo'], r['model'], r['model'] - r['photo']))
    mean, each = exterior_mean(t)
    print('\n%-16s %s %8s %8s' % ('above the road', '  '.join('%7s' % p['photo'] for p in t['exterior']['profiles']), 'mean', 'model'))
    for k in mean:
        m = t['exterior']['model_m'].get(k)
        print('%-16s %s %8.2f %8s' % (k, '  '.join('%7.2f' % e[k] for e in each), mean[k], '-' if m is None else '%.2f' % m))
    a, b = floor_from_windows(t)
    print('\nseat mat above the road, from the windscreen against the side windows: %.2f and %.2f' % (a, b))
    lo, hi = aisle_from_roof(t)
    print('aisle above the road, from the hull roof: %.2f to %.2f; deck above aisle implied: %.2f (used %.2f)' % (lo, hi, (a + b) / 2 - (lo + hi) / 2, t['datum']['deck_above_aisle_m']))
    k, each_k = yardsticks(t)
    print('solve scale: %.3f from %s' % (k, ['%.3f' % v for v in each_k]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
