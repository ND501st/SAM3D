"""Write a panel's picks into the cabin and console data: every part picked on a metric flat (panel_flats.py) placed on
its panel of data/cabins/osa_ak.json at the size it measures, new controls added to data/procedures/osa_ak.json.

    panel_apply.py <picks> [--write]       # tools/traces/osa_interior/panels/<picks>.json; without --write, what would change

A picks file names its panels (their extent on the flat, in mm), what of theirs goes (their old fixtures, blocks) and
what stays where it is in the vehicle (keep, keep_body), and its items:

    {"is": "fixture" | "block" | "control" | "lamp" | "display", "part": "<flat>:<n>" | "at_mm": [u, v], "size": ...}

A part's centre and size come from the flat's measured parts (<flat>_parts.json), moved by the file's "offsets" for a
flat registered from a view whose parts stand off the plane. "size" is "dia" (the fitted circle), "box" (width and
height), "box_mean", "box_min" or "dia_or_box". A control or lamp already in the data is moved and given its look;
one with "new" is added (a control to the console too, with its source and confidence). An item with "panel" and
"at_m" goes there as given. The cabin file's layout is kept (jsonedit.py): only the changed values and items move.

Run with E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8.
"""
import argparse
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import core as C  # noqa: E402
import jsonedit as J  # noqa: E402
import panels as P  # noqa: E402

CABIN = P.CABIN
CONSOLE = P.CONSOLE
FLATS_OUT = os.path.join(C.OUT, 'panels', 'flats')
_parts = {}


def part(ref):
    flat, n = ref.split(':')
    if flat not in _parts:
        doc = C.load(os.path.join(FLATS_OUT, flat + '_parts.json'))
        _parts[flat] = {p['n']: p for p in doc['parts']}
    return flat, _parts[flat][int(n)]


def measure(it, offsets):
    """An item's centre (u, v) and size (a diameter, or [w, h]) in mm on the plane."""
    if 'at_mm' in it:
        return np.asarray(it['at_mm'], float), it.get('size_mm')
    flat, p = part(it['part'])
    at = np.asarray(p['at_mm'], float) + np.asarray(offsets.get(flat, [0.0, 0.0]), float)
    kind = it.get('size')
    box = p['box_mm']
    if kind == 'dia':
        if 'dia_mm' not in p:
            raise ValueError('%s has no fitted circle' % it['part'])
        size = p['dia_mm']
    elif kind == 'dia_or_box':
        size = p.get('dia_mm', 0.5 * (box[0] + box[1]))
    elif kind == 'box':
        size = list(box)
    elif kind == 'box_mean':
        size = 0.5 * (box[0] + box[1])
    elif kind == 'box_min':
        size = min(box)
    else:
        size = it.get('size_mm')
    return at, size


def unparallax(it, at, picks, plan):
    """A part that stands h_mm proud of the plane is seen on the flat where the camera's ray through it meets the
    plane, pushed away from the camera's foot: its true place is that point moved back along the ray, by h / d of the way
    to the eye (d the eye's distance from the plane). The eye is the flat's photo's certified camera ("eyes": {flat:
    [x, y, z] body m}). An item without h_mm, or measured on a flat with no eye, stays where it was seen."""
    h = it.get('h_mm', 0.0)
    flat = (it.get('part') or '').split(':')[0] or it.get('flat', '')
    eye = picks.get('eyes', {}).get(flat)
    if not h or eye is None:
        return at
    pid = next(iter(plan.new))
    n, _, _ = P.axes(plan.new[pid])
    X = np.asarray(plan.body_of_plane(at), float)
    E = np.asarray(eye, float)
    d = float((E - X) @ n)
    Xt = X + (E - X) * (h / 1000.0) / d
    Xt = Xt - n * float((Xt - X) @ n)          # back onto the plane: the part's foot
    u0, u1, v0, v1 = plan.new[pid]['_range']
    uc, vc = 0.5 * (u0 + u1), 0.5 * (v0 + v1)
    lu, lv = plan.at_on(pid, Xt)
    return np.array([lu * 1000.0 + uc, lv * 1000.0 + vc])


def block_box(it, offsets):
    """A block's rectangle (u0, v0, u1, v1) in mm: as given, a part's box, or set out from its four corner screws
    (TL, TR, BR, BL) by the inset."""
    if 'box_mm' in it:
        return list(it['box_mm'])
    if 'box_from_screws' in it:
        tl, tr, br, bl = [measure({'part': r}, offsets)[0] for r in it['box_from_screws']]
        iu, iv = it.get('inset_mm', [0.0, 0.0])
        return [0.5 * (tl[0] + bl[0]) - iu, 0.5 * (bl[1] + br[1]) - iv, 0.5 * (tr[0] + br[0]) + iu, 0.5 * (tl[1] + tr[1]) + iv]
    at, size = measure(dict(it, size='box'), offsets)
    return [at[0] - size[0] / 2, at[1] - size[1] / 2, at[0] + size[0] / 2, at[1] + size[1] / 2]


def num(v, d=4):
    return float(round(float(v), d))


def size_m(size):
    if size is None:
        return None
    if isinstance(size, (list, tuple, np.ndarray)):
        return [num(size[0] / 1000.0), num(size[1] / 1000.0)]
    return num(size / 1000.0)


def dumps(o):
    return json.dumps(o, ensure_ascii=False)


def line(d):
    """One item in the cabin file's style: { "key": value, ... }."""
    return '{ ' + ', '.join('%s: %s' % (dumps(k), dumps(v)) for k, v in d.items()) + ' }'


class Plan:
    """The panels' new frames, and every item's panel and place."""

    def __init__(self, picks, cab):
        self.picks, self.cab = picks, cab
        self.old = {p['id']: p for p in cab['panels']}
        self.new = {}
        plane = picks.get('plane', {})
        for pp in picks['panels']:
            p = dict(self.old[pp['id']])
            u0, u1 = pp['u_mm']
            v0, v1 = pp['v_mm']
            uc, vc = 0.5 * (u0 + u1), 0.5 * (v0 + v1)
            n, r, up = P.axes(p)
            c = np.asarray(p['centre_m'], float)
            if 'x_of_u0_m' in plane:
                if not np.allclose(r, [1, 0, 0], atol=1e-9):
                    raise ValueError('%s: x_of_u0_m needs a panel whose right is +x' % p['id'])
                c = c + r * (plane['x_of_u0_m'] + uc / 1000.0 - c[0]) + up * (vc / 1000.0 - pp.get('v_centre_mm', 0.0) / 1000.0)
                origin_u = plane['x_of_u0_m'] * 1000.0
            else:
                origin_u = 0.0
            p.update(centre_m=[num(c[0]), num(c[1]), num(c[2])], size_m=[num((u1 - u0) / 1000.0), num((v1 - v0) / 1000.0)])
            if 'notes' in pp:
                p['notes'] = pp['notes']
            p['_range'] = (u0, u1, v0, v1)
            p['_plane_u'] = origin_u
            self.new[p['id']] = p

    def panel_of(self, at):
        for pid, p in self.new.items():
            u0, u1, v0, v1 = p['_range']
            if u0 <= at[0] <= u1 and v0 <= at[1] <= v1:
                return pid
        raise ValueError('nothing of %s covers %s mm' % (list(self.new), at))

    def local(self, pid, at):
        """Plane mm -> the panel's at_m."""
        p = self.new[pid]
        X = self.body_of_plane(at)
        return self.at_on(pid, X)

    def body_of_plane(self, at):
        # the plane's mm in the body: through any of the new panels (they share one plane)
        pid = next(iter(self.new))
        p = self.new[pid]
        u0, u1, v0, v1 = p['_range']
        uc, vc = 0.5 * (u0 + u1), 0.5 * (v0 + v1)
        return P.body(p, [[(at[0] - uc) / 1000.0, (at[1] - vc) / 1000.0]])[0]

    def at_on(self, pid, X):
        p = self.new[pid]
        n, r, up = P.axes(p)
        d = np.asarray(X, float) - np.asarray(p['centre_m'], float)
        return [num(d @ r), num(d @ up)]

    def body_of_old(self, pid, at_m):
        return P.body(self.old[pid], [at_m])[0]


def build(name):
    picks = C.load(os.path.join(P.PICKS, name + '.json'))
    cab, con = P.cabin(), P.console()
    plan = Plan(picks, cab)
    offsets = {k: v for k, v in picks.get('offsets', {}).items() if k != 'why'}
    out = {'panels': plan.new, 'controls': [], 'lamps': [], 'displays': [], 'blocks': [], 'fixtures': [], 'new_controls': [], 'new_lamps': []}
    cab_controls = {c['control']: c for c in cab['controls']}
    cab_lamps = {l['id']: l for l in cab['lamps']}
    con_ids = {c['id'] for c in con['controls']}
    for it in picks['items']:
        is_ = it['is']
        if it.get('keep_body'):
            old = cab_controls[it['id']] if is_ == 'control' else cab_lamps[it['id']]
            X = plan.body_of_old(old['panel'], old['at_m'])
            pid = old['panel'] if old['panel'] in plan.new else None
            entry = {'id': it['id'], 'panel': pid, 'at_m': plan.at_on(pid, X)}
            out[is_ + 's'].append(entry)
            continue
        if 'panel' in it and 'at_m' in it:
            entry = {'id': it['id'], 'panel': it['panel'], 'at_m': it['at_m']}
            if 'look' in it:
                entry['look'] = it['look']
            out[is_ + 's'].append(entry)
            continue
        if is_ == 'block':
            u0, v0, u1, v1 = block_box(it, offsets)
            at = np.array([0.5 * (u0 + u1), 0.5 * (v0 + v1)])
            pid = plan.panel_of(at)
            out['blocks'].append({'id': it['id'], 'panel': pid, 'label': it.get('label', ''), 'at_m': plan.local(pid, at),
                                  'size_m': [num((u1 - u0) / 1000.0), num((v1 - v0) / 1000.0)], 'color': it.get('color', '')})
            continue
        at, size = measure(it, offsets)
        at = unparallax(it, at, picks, plan)
        pid = plan.panel_of(at)
        at_m = plan.local(pid, at)
        if is_ == 'fixture':
            f = {'panel': pid, 'at_m': at_m, 'kind': it['kind'], 'size_m': size_m(size)}
            for k in ('color', 'ring', 'label', 'scale'):
                if it.get(k):
                    f[k] = it[k]
            if it.get('what'):
                f['notes'] = it['what'][0].upper() + it['what'][1:] + ' (%s).' % (it.get('part') or 'placed by hand')
            out['fixtures'].append(f)
        elif is_ == 'display':
            out['displays'].append({'id': it['id'], 'panel': pid, 'at_m': at_m,
                                    'size_m': size_m(size) if isinstance(size, (list, tuple)) else [size_m(size), size_m(size)]})
        elif is_ in ('control', 'lamp'):
            entry = {'id': it['id'], 'panel': pid, 'at_m': at_m}
            if 'look' in it:
                entry['look'] = it['look']
            if 'color' in it:
                entry['color'] = it['color']
            if 'new' in it and ((is_ == 'control' and it['id'] not in cab_controls) or (is_ == 'lamp' and it['id'] not in cab_lamps)):
                entry['new'] = it['new']
                if is_ == 'control' and it['id'] in con_ids:
                    entry['new_in_console'] = False
            out[is_ + 's'].append(entry)
    out['keep'] = []
    for k in picks.get('keep', []):
        for f in cab['fixtures']:
            if f['panel'] == k['panel']:
                X = plan.body_of_old(f['panel'], f['at_m'])
                if X[0] >= k.get('body_x_min_m', -1e9):
                    out['keep'].append((f, plan.at_on(f['panel'], X)))
    return picks, plan, out


def write(name, dry=True):
    picks, plan, out = build(name)
    text = J.read(CABIN)
    ctext = J.read(CONSOLE)
    con = json.loads(ctext)
    cab = json.loads(text)
    report = []
    # the panels
    for pid, p in plan.new.items():
        sel = ['panels', {'id': pid}]
        for i in range(3):
            text = J.set_value(text, sel + ['centre_m', i], p['centre_m'][i], 4)
        text = J.replace_value(text, sel + ['size_m'], dumps(p['size_m']))
        if 'notes' in p:
            text = J.replace_value(text, sel + ['notes'], dumps(p['notes']))
        report.append('panel %s: centre %s size %s' % (pid, p['centre_m'], p['size_m']))
    # what goes
    removing = picks.get('remove', [])
    keep_ids = {id(f) for f, _ in out['keep']}
    kept = [dict(f, at_m=at) for f, at in out['keep']]

    def gone(kind):
        pans = {r['panel'] for r in removing if r['what'] == kind}
        return lambda it: it.get('panel') in pans
    old_fx = [f for f in cab['fixtures'] if gone('fixtures')(f)]
    text = J.remove_items(text, ['fixtures'], gone('fixtures'))
    text = J.remove_items(text, ['blocks'], gone('blocks'))
    report.append('removed %d fixtures (%d kept where they are), blocks of %s' % (len(old_fx), len(kept), sorted({r['panel'] for r in removing if r['what'] == 'blocks'})))
    # controls, lamps, displays already in the data
    for kind, key, sel_key in (('controls', 'control', 'control'), ('lamps', 'id', 'id'), ('displays', 'id', 'id')):
        news = []
        for e in out[kind]:
            if 'new' in e:
                news.append(e)
                continue
            sel = [kind, {sel_key: e['id']}]
            text = J.replace_value(text, sel + ['panel'], dumps(e['panel']))
            text = J.replace_value(text, sel + ['at_m'], dumps(e['at_m']))
            if 'size_m' in e:
                text = J.replace_value(text, sel + ['size_m'], dumps(e['size_m']))
            if 'look' in e:
                cur = J.resolve(text, sel)[1]
                text = J.replace_value(text, sel + ['look'], dumps(e['look'])) if 'look' in cur else J.add_key(text, sel, 'look', dumps(e['look']))
            if 'color' in e:
                cur = J.resolve(text, sel)[1]
                text = J.replace_value(text, sel + ['color'], dumps(e['color'])) if 'color' in cur else J.add_key(text, sel, 'color', dumps(e['color']))
            report.append('%s %s -> %s %s' % (key, e['id'], e['panel'], e['at_m']))
        if kind == 'controls' and news:
            rows = []
            for e in news:
                d = {'control': e['id'], 'panel': e['panel'], 'at_m': e['at_m']}
                if 'look' in e:
                    d['look'] = e['look']
                rows.append(line(d))
                if e.get('new_in_console', True):
                    nd = dict({'id': e['id']}, **e['new'])
                    ctext = J.insert_items(ctext, ['controls'], [line(nd)], before=_after(con['controls'], e['new']['station']))
                    con = json.loads(ctext)
                report.append('new control %s on %s %s' % (e['id'], e['panel'], e['at_m']))
            text = J.insert_items(text, ['controls'], rows)
        if kind == 'lamps' and news:
            rows = []
            for e in news:
                n = e['new']
                d = {'id': e['id'], 'label': n['label'], 'panel': e['panel'], 'at_m': e['at_m'], 'color': e.get('color', 'amber'), 'lit_when': n['lit_when'],
                     'source': n['source'], 'confidence': n['confidence'], 'notes': n['notes']}
                if 'look' in e:
                    d['look'] = e['look']
                rows.append(line(d))
                report.append('new lamp %s on %s %s' % (e['id'], e['panel'], e['at_m']))
            text = J.insert_items(text, ['lamps'], rows)
    text = J.insert_items(text, ['blocks'], [line(dict(id=b['id'], panel=b['panel'], label=b['label'], at_m=b['at_m'], size_m=b['size_m'], **({'color': b['color']} if b['color'] else {})))
                                             for b in out['blocks']])
    report.append('%d blocks' % len(out['blocks']))
    rows = [line(f) for f in out['fixtures']] + [line(f) for f in kept]
    text = J.insert_items(text, ['fixtures'], rows)
    report.append('%d fixtures (+%d kept)' % (len(out['fixtures']), len(kept)))
    print('\n'.join(report))
    if not dry:
        J.write(CABIN, text)
        J.write(CONSOLE, ctext)
        print('written', CABIN, CONSOLE)
    return text, ctext


def _after(controls, station):
    """Where a new control of a seat goes in the console's list: after that seat's last control."""
    last = max((i for i, c in enumerate(controls) if c.get('station') == station), default=len(controls) - 1)
    return last + 1


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('picks')
    ap.add_argument('--write', action='store_true')
    a = ap.parse_args()
    write(a.picks, dry=not a.write)
