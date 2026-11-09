"""The holistic layer's fuse, draw and report steps (run through tools/recon/holistic.py; docs/OSA_INTERIOR_HOLISTIC.md).

fuse: every observation's components are tied to the register by id. Each entry gets its evidence rebuilt from scratch
(`seen_in`, `relations` with how many photos say each), proposals (`new.*`) seen with at least medium confidence become
entries (`from_observations: true`, rebuilt every run: drop the flag to keep a hand edit), and every component with no
measured box gets an `estimate` from its relations to the boxed ones (rough by design: sigma 0.15-0.4 m). Relations that
contradict two measured boxes are listed as `conflicts`.
"""
import collections
import html
import math
import os

import holistic as H

FIG_CANDIDATES = [H.FIG, os.path.join(H.REPO, 'reference', 'sources', 'AntiaircraftOSA-AK-9K32M2rocketkit_extract'),
                  os.path.join('C:/dev/SAM3D', 'reference', 'sources', 'AntiaircraftOSA-AK-9K32M2rocketkit_extract')]
CONF_W = {'high': 1.0, 'medium': 0.6, 'low': 0.25}
WALL_X = {'compartment.left_wall': -1.15, 'compartment.right_wall': 1.15, 'cab.left_wall': -1.1, 'cab.right_wall': 1.1}
ZONE_Z = {'compartment.roof': 2.02, 'cab.roof': 1.95, 'compartment.floor': 0.66, 'cab.floor': 0.75}
ZONE_Y = {'cab': 4.0, 'cab.dash': 4.45, 'cab.left_wall': 3.6, 'cab.right_wall': 3.6, 'cab.roof': 4.0, 'cab.floor': 4.1,
          'rear_bulkhead': -0.95, 'compartment.rear': -0.6, 'engine': -2.0}
GAP = 0.12


def fig_dir():
    return next((d for d in FIG_CANDIDATES if os.path.isdir(d)), None)


def _alias(cid):
    """new.<class>.<name> -> <class>.<name>: the id an accepted proposal takes."""
    return cid[4:] if cid.startswith('new.') else cid


def _box(e):
    b = e.get('box_m')
    if b and b.get('centre') and None not in b['centre'] and b.get('size') and None not in b['size']:
        return b['centre'], b['size'], b.get('sigma_m', 0.05)
    return None


def _partial(e):
    """What a box or estimate knows per axis: ([x, y, z] or None each, sigma)."""
    b = e.get('box_m')
    if b and b.get('centre'):
        return list(b['centre']), b.get('sigma_m', 0.05)
    est = e.get('estimate')
    if est:
        return list(est['centre']), est['sigma_m']
    return None, None


def period(o):
    """Whether a photo shows the 1980s fit (the book's g101 is a late Czech vehicle: evidence, not proposals)."""
    v = o.get('vehicle', '').lower()
    return not ('late' in v or 'not the 1980s' in v or '2000s' in v)


def fuse(write=True):
    doc = H.load(H.REGISTER)
    comps = [e for e in doc['components'] if not e.get('from_observations')]
    for e in comps:
        for k in ('seen_in', 'relations', 'estimate', 'observed_conflicts', 'not_built'):
            e.pop(k, None)
    by_id = {e['id']: e for e in comps}
    obs = H.observations()

    # proposals: gather and accept
    prop = collections.defaultdict(list)
    for o in obs:
        if not period(o):
            continue          # a later vehicle's fit: its things are not proposed for the 1980s compartment
        for c in o.get('components', []):
            cid = H.ALIASES.get(c['id'], c['id'])
            if cid.startswith('new.') and c['zone'] not in ('outside', 'engine'):
                prop[cid].append((o['photo'], c))
    accepted = {}
    for pid, seen in sorted(prop.items()):
        best = max(CONF_W[c['confidence']] for _, c in seen)
        rid = _alias(pid)
        if best < CONF_W['medium'] or rid in by_id:
            if rid in by_id:
                accepted[pid] = rid
            continue
        cls = collections.Counter(c['class'] for _, c in seen).most_common(1)[0][0]
        zone = collections.Counter(c['zone'] for _, c in seen).most_common(1)[0][0]
        name = max(seen, key=lambda pc: CONF_W[pc[1]['confidence']])[1].get('seen_as', rid)
        e = {'id': rid, 'class': cls, 'name': name, 'names': [], 'zone': zone, 'parent': None, 'box_m': None,
             'cabin': [], 'status': 'missing', 'sources': ['wwp_gecko ' + p for p in sorted({p for p, _ in seen})],
             'conflicts': [], 'notes': 'proposed by the photos\' observers as ' + pid, 'from_observations': True}
        comps.append(e)
        by_id[rid] = e
        accepted[pid] = rid

    def resolve(cid):
        cid = H.ALIASES.get(cid, cid)
        return accepted.get(cid, cid) if cid.startswith('new.') else cid

    # evidence
    rel_count = collections.defaultdict(collections.Counter)
    for o in obs:
        for c in o.get('components', []):
            rid = resolve(c['id'])
            e = by_id.get(rid)
            if e is None:
                continue
            e.setdefault('seen_in', []).append([o['photo'], c['confidence']] + ([] if period(o) else ['late']))
            rels = list(c.get('relations', []))
            if c.get('on'):
                rels.append(['on', c['on']])
            if not period(o):
                continue      # a later vehicle's layout says nothing about the 1980s fit's
            for rel, other in rels:
                other = resolve(other)
                if other in by_id and other != rid:
                    rel_count[rid][(rel, other)] += 1
    for rid, cnt in rel_count.items():
        by_id[rid]['relations'] = [[r, o, n] for (r, o), n in sorted(cnt.items())]
    for e in comps:
        if e.get('seen_in'):
            src = 'wwp_gecko photos (%d)' % len(e['seen_in'])
            e['sources'] = [s for s in e['sources'] if not s.startswith('wwp_gecko photos (')] + [src]

    _review(by_id)
    _estimate(comps, by_id)
    _contradictions(comps, by_id)
    doc['components'] = comps
    if write:
        H.save(H.REGISTER, doc)
        print('fused %d observation files: %d components (%d proposals accepted of %d), %d estimated, %d contradictions'
              % (len(obs), len(comps), sum(1 for e in comps if e.get('from_observations')), len(prop),
                 sum(1 for e in comps if e.get('estimate')), sum(len(e.get('observed_conflicts', [])) for e in comps)))
    return doc


def _review(by_id):
    """H3's decisions (holistic.py: PLACES, PARENTS, NOT_BUILT)."""
    for rid, (centre, size, sigma, why) in H.PLACES.items():
        e = by_id[rid]
        e['box_m'] = {'centre': centre, 'size': size, 'sigma_m': sigma, 'from': 'H3: ' + why}
        if 'the cabin builds it in ' in why:
            e['status'] = 'modelled'
            e['cabin'] = sorted(set(e.get('cabin', [])) | {why.split('the cabin builds it in ')[1].split()[0].rstrip(';,.')})
    for parent, children in H.PARENTS.items():
        for child in children:
            if child in by_id and parent in by_id:
                by_id[child]['parent'] = parent
    for rid, why in H.NOT_BUILT.items():
        if rid in by_id:
            by_id[rid]['not_built'] = why


def _anchor(rel, c, s):
    """Where a relation to a box (centre c, size s; None where unknown) puts the component, axis by axis (None: says
    nothing)."""
    sx, sy, sz = [v if v is not None else 0.0 for v in s]

    def off(i, d):
        return None if c[i] is None else c[i] + d

    x, y, z = c
    return {
        'on': [x, y, z], 'in': [x, y, z], 'same_as': [x, y, z], 'beside': [x, y, z], 'connects': [x, y, z],
        'above': [x, y, off(2, sz / 2 + GAP)], 'below': [x, y, off(2, -sz / 2 - GAP)],
        'forward_of': [x, off(1, sy / 2 + GAP), z], 'aft_of': [x, off(1, -sy / 2 - GAP), z],
        'left_of': [off(0, -sx / 2 - GAP), y, z], 'right_of': [off(0, sx / 2 + GAP), y, z],
        'in_front_of': [None, y, z], 'behind': [None, y, z],
    }.get(rel)


def _estimate(comps, by_id):
    for _ in range(3):  # propagate from boxed components outwards
        for e in comps:
            if _box(e):
                continue
            acc = [[], [], []]
            for rel, other, n in e.get('relations', []):
                o = by_id[other]
                oc, osig = _partial(o)
                if oc is None:
                    continue
                size = (o.get('box_m') or {}).get('size') or [0.2, 0.2, 0.2]
                a = _anchor(rel, oc, size)
                if a is None:
                    continue
                for i in range(3):
                    if a[i] is not None:
                        acc[i].append((a[i], n / (osig + 0.05)))
            # what a partial box already gives (Fig. 3's plan x, y or a section's z) wins over relations
            pc, _ = _partial(e) if e.get('box_m') else (None, None)
            centre = []
            for i in range(3):
                if pc is not None and pc[i] is not None:
                    centre.append(pc[i])
                elif acc[i]:
                    centre.append(sum(v * w for v, w in acc[i]) / sum(w for _, w in acc[i]))
                else:
                    centre.append(None)
            z = e['zone']
            if centre[0] is None and z in WALL_X:
                centre[0] = WALL_X[z]
            if centre[2] is None and z in ZONE_Z:
                centre[2] = ZONE_Z[z]
            if centre[1] is None and z in ZONE_Y:
                centre[1] = ZONE_Y[z]
            if all(v is None for v in centre):
                continue
            n_rel = sum(len(a) for a in acc)
            sig = 0.15 if n_rel >= 4 else 0.25 if n_rel >= 1 else 0.4
            e['estimate'] = {'centre': [None if v is None else round(v, 3) for v in centre], 'sigma_m': sig,
                             'from': 'relations (%d)' % n_rel if n_rel else 'zone'}


def _contradictions(comps, by_id):
    test = {'above': (2, 1), 'below': (2, -1), 'forward_of': (1, 1), 'aft_of': (1, -1), 'right_of': (0, 1), 'left_of': (0, -1)}
    for e in comps:
        a = _box(e)
        if not a:
            continue
        out = []
        for rel, other, n in e.get('relations', []):
            b = _box(by_id[other])
            if rel not in test or not b:
                continue
            i, sgn = test[rel]
            d = (a[0][i] - b[0][i]) * sgn
            slack = (a[1][i] + b[1][i]) / 2 + a[2] + b[2]
            if d < -slack * 0.5:
                out.append('%d photo(s) say %s %s, the boxes put it %.2f m the other way' % (n, rel, other, -d))
        if out:
            e['observed_conflicts'] = out


# ------------------------------------------------------------------------------------------------ schematics

GROUP_COL = {'structure': '#8a8f98', 'equipment': '#3b7dd8', 'crew': '#c9822b', 'fittings': '#7a5cc2', 'runs': '#2f9e6a',
             'light_controls': '#d23f5b', 'loose': '#9c9c3a'}
VIEWS = {
    # name: (title, horizontal axis (index, sign), vertical axis (index, sign), ranges, filter)
    'plan': ('Plan from above (forward up)', (0, 1), (1, 1), ((-1.45, 1.45), (-3.3, 5.0)), lambda c, e: True),
    'left_wall': ('Left wall, seen from the aisle (forward to the right)', (1, 1), (2, 1), ((-1.3, 5.0), (0.55, 2.25)),
                  lambda c, e: c[0] is not None and c[0] < -0.25 and (c[1] is None or c[1] > -1.3)),
    'right_wall': ('Right wall, seen from the aisle (forward to the left)', (1, -1), (2, 1), ((-1.3, 5.0), (0.55, 2.25)),
                   lambda c, e: c[0] is not None and c[0] > 0.25 and (c[1] is None or c[1] > -1.3)),
    'front': ('Front: the cab seen from the compartment (right to the right)', (0, 1), (2, 1), ((-1.45, 1.45), (0.55, 2.25)),
              lambda c, e: c[1] is not None and c[1] > 2.9),
    'rear': ('Rear: the bulkhead seen from the aisle (right to the left)', (0, -1), (2, 1), ((-1.45, 1.45), (0.55, 2.25)),
             lambda c, e: c[1] is not None and -1.3 < c[1] < 0.3),
    'roof': ('Roof seen from below (right to the left, forward up)', (0, -1), (1, 1), ((-1.45, 1.45), (-1.3, 5.0)),
             lambda c, e: (c[2] is not None and c[2] > 1.75) or e['zone'] in ('compartment.roof', 'cab.roof')),
}
K = 140.0      # px a metre
PAD = 30.0


def _geom(e):
    """(centre, size, kind) with kind measured / plan / estimate."""
    b = e.get('box_m')
    c = list(b['centre']) if b and b.get('centre') else [None, None, None]
    s = list(b['size']) if b and b.get('size') else [None, None, None]
    est = e.get('estimate')
    kind = 'measured' if _box(e) else 'plan' if b else 'estimate'
    if est:
        c = [c[i] if c[i] is not None else est['centre'][i] for i in range(3)]
    sig = est['sigma_m'] if est and kind == 'estimate' else (b or {}).get('sigma_m', 0.3)
    s = [s[i] if s[i] is not None else max(0.08, min(0.3, sig)) for i in range(3)]
    return c, s, kind


def _svg_view(name, comps):
    title, (hi, hs), (vi, vs), ((h0, h1), (v0, v1)), keep = VIEWS[name]
    W = (h1 - h0) * K + 2 * PAD
    Hh = (v1 - v0) * K + 2 * PAD + 20

    def X(v):
        return PAD + ((v - h0) if hs > 0 else (h1 - v)) * K

    def Y(v):
        return PAD + 20 + ((v1 - v) if vs > 0 else (v - v0)) * K

    out = ['<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 %.0f %.0f" '
           'width="%.0f" height="%.0f" font-family="sans-serif">' % (W, Hh, W, Hh),
           '<rect width="100%%" height="100%%" fill="#fbfaf7"/><text x="%.0f" y="20" font-size="14">%s</text>' % (PAD, html.escape(title))]
    out.append('<clipPath id="frame-%s"><rect x="%.1f" y="%.1f" width="%.1f" height="%.1f"/></clipPath>'
               % (name, PAD, PAD + 20, (h1 - h0) * K, (v1 - v0) * K))
    fd = fig_dir()
    if name == 'plan' and fd:
        s = H.PLAN_MM_PX / 1000.0
        for sheet, (ylo, yhi) in (('p029', (-9, 2.505)), ('p028', (2.505, 9))):
            c = H.PLAN[sheet]
            # body x = (cpy - py) s, y = Y0 - (px - px0) s  ->  svg X = PAD + (x - h0) K, svg Y = PAD + 20 + (v1 - y) K
            a, cc, e_ = 0.0, -s * K, PAD + (c['centre_py'] * s - h0) * K
            b, d, f = s * K, 0.0, PAD + 20 + (v1 - H.PLAN_Y0 - c['y0_px'] * s) * K
            href = 'file:///' + os.path.join(fd, 'fig_' + sheet, 'full.png').replace('\\', '/')
            out.append('<clipPath id="half-%s"><rect x="0" y="%.1f" width="%.1f" height="%.1f"/></clipPath>'
                       % (sheet, Y(min(yhi, v1)), W, max(0.0, Y(max(ylo, v0)) - Y(min(yhi, v1)))))
            out.append('<g clip-path="url(#frame-plan)"><g clip-path="url(#half-%s)"><image href="%s" xlink:href="%s" '
                       'width="1600" height="1810" opacity="0.33" transform="matrix(%g %g %g %g %g %g)"/></g></g>'
                       % (sheet, href, href, a, b, cc, d, e_, f))
    # grid every 0.5 m
    g = []
    v = math.floor(h0 * 2) / 2
    while v <= h1:
        g.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>' % (X(v), Y(v0), X(v), Y(v1)))
        g.append('<text x="%.1f" y="%.1f" font-size="9" fill="#888">%g</text>' % (X(v) + 2, Y(v0) + 11, v))
        v += 0.5
    v = math.floor(v0 * 2) / 2
    while v <= v1:
        g.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>' % (X(h0), Y(v), X(h1), Y(v)))
        g.append('<text x="%.1f" y="%.1f" font-size="9" fill="#888">%g</text>' % (X(h0) + 2, Y(v) - 2, v))
        v += 0.5
    out.append('<g stroke="#ddd" stroke-width="0.6">%s</g>' % ''.join(g))
    items = []
    for e in comps:
        if e['class'] in ('hull_wall', 'floor', 'roof') or e['zone'] == 'outside':
            continue
        c, s, kind = _geom(e)
        if c[hi] is None or c[vi] is None or not keep(c, e):
            continue
        items.append((s[hi] * s[vi], e, c, s, kind))
    items.sort(key=lambda t: -t[0])  # big ones under small ones
    for _, e, c, s, kind in items:
        x0, x1 = sorted((X(c[hi] - s[hi] / 2), X(c[hi] + s[hi] / 2)))
        y0, y1 = sorted((Y(c[vi] - s[vi] / 2), Y(c[vi] + s[vi] / 2)))
        col = GROUP_COL[H.CLASS_GROUP[e['class']]]
        dash = {'measured': '', 'plan': ' stroke-dasharray="5 3"', 'estimate': ' stroke-dasharray="1.5 2.5"'}[kind]
        width = 1.6 if kind == 'measured' else 1.1
        tip = '%s (%s, %s)\n%s\nposition: %s\nsources: %s' % (e['id'], e['class'], e['status'], e['name'], kind,
                                                            '; '.join(e.get('sources', [])))
        out.append('<g><title>%s</title><rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s" fill-opacity="%.2f" '
                   'stroke="%s" stroke-width="%.1f"%s/>' % (html.escape(tip), x0, y0, max(1.0, x1 - x0), max(1.0, y1 - y0),
                                                          col, 0.18 if kind == 'measured' else 0.08, col, width, dash))
        if x1 - x0 > 18 or kind != 'measured' or e['class'] != 'block':
            out.append('<text x="%.1f" y="%.1f" font-size="7.5" fill="%s">%s</text>'
                       % (x0 + 1.5, y0 + 8, col, html.escape(e['id'].split('.', 1)[-1][:28])))
        out.append('</g>')
    # legend
    lx, ly = PAD, Hh - 8
    for gname, col in GROUP_COL.items():
        out.append('<rect x="%.0f" y="%.0f" width="10" height="8" fill="%s"/><text x="%.0f" y="%.0f" font-size="9">%s</text>'
                   % (lx, ly - 8, col, lx + 13, ly, gname))
        lx += 13 + 7 * len(gname) + 10
    out.append('<text x="%.0f" y="%.0f" font-size="9">solid: measured; dashed: Fig. 3; dotted: estimated from the photos\' relations</text>'
               % (lx, ly))
    out.append('</svg>')
    return '\n'.join(out), len(items)


def draw():
    doc = H.load(H.REGISTER)
    comps = doc['components']
    os.makedirs(H.OUT, exist_ok=True)
    parts = []
    for name in VIEWS:
        svg, n = _svg_view(name, comps)
        with open(os.path.join(H.OUT, name + '.svg'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(svg)
        parts.append('<h2>%s <small>(%d components)</small></h2>\n%s' % (html.escape(VIEWS[name][0]), n, svg))
        print('drew', name, n)
    rows = ''.join('<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td></tr>'
                   % (html.escape(e['id']), e['class'], e['zone'], e['status'], _geom(e)[2], len(e.get('seen_in', [])))
                   for e in comps)
    page = ('<!doctype html><meta charset="utf-8"><title>Osa interior schematics</title><style>body{font-family:sans-serif;'
            'margin:16px;background:#fbfaf7}svg{max-width:100%%;height:auto;border:1px solid #ccc}table{border-collapse:collapse;'
            'font-size:12px}td{border:1px solid #ddd;padding:2px 5px}</style><h1>Osa (9A33BM2) interior: the holistic '
            'schematics</h1><p>From data/cabins/osa_ak_components.json (tools/recon/holistic.py draw). Hover a box for its '
            'sources. Rough by design: a gauge of where things sit.</p>%s<h2>Components</h2><table><tr><th>id</th><th>class'
            '</th><th>zone</th><th>status</th><th>position</th><th>photos</th></tr>%s</table>') % ('\n'.join(parts), rows)
    with open(os.path.join(H.OUT, 'index.html'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(page)
    print('wrote', os.path.join(H.OUT, 'index.html'))


# ------------------------------------------------------------------------------------------------ report

def _best_photo(e, obs_by_photo):
    best = None
    for photo, conf, *_ in e.get('seen_in', []):
        for c in obs_by_photo[photo].get('components', []):
            if c['id'] in (e['id'], 'new.' + e['id']):
                b = c['bbox']
                score = CONF_W[conf] * (1.0 if period(obs_by_photo[photo]) else 0.3) * (b[2] - b[0]) * (b[3] - b[1])
                if best is None or score > best[0]:
                    best = (score, photo)
    return best[1] if best else ''


def report():
    doc = H.load(H.REGISTER)
    comps = doc['components']
    obs = H.observations()
    obs_by_photo = {o['photo']: o for o in obs}
    st = collections.Counter(e['status'] for e in comps)
    grp = collections.Counter(H.CLASS_GROUP[e['class']] for e in comps)
    L = ['# The Osa interior\'s components: coverage report', '',
         'Generated by `python tools/recon/holistic.py report` from `data/cabins/osa_ak_components.json` and the '
         'observation files in `tools/traces/osa_interior/observations/` (%d photos). The method is '
         '`docs/OSA_INTERIOR_HOLISTIC.md`; the roadmap that uses it is `docs/OSA_MODEL_ROADMAP.md`.' % len(obs), '',
         '## Totals', '', '| | count |', '|---|---|', '| components | %d |' % len(comps)]
    L += ['| status `%s` | %d |' % (s, st.get(s, 0)) for s in H.STATUS]
    L += ['| group `%s` | %d |' % (g, n) for g, n in sorted(grp.items())]
    L += ['| proposals accepted from the photos | %d |' % sum(1 for e in comps if e.get('from_observations')),
          '| components seen in at least one photo | %d |' % sum(1 for e in comps if e.get('seen_in')), '']
    queue = [e for e in comps if H.STATUS.index(e['status']) < H.STATUS.index('traced') and e.get('seen_in')
             and e['class'] not in ('hull_wall', 'floor', 'roof', 'cable', 'loom')]
    queue.sort(key=lambda e: (-sum(CONF_W[s[1]] for s in e['seen_in']), e['id']))
    L += ['## The trace queue', '', 'Components below `traced` that the photos show, most-seen first, with the photo that '
          'shows each biggest and surest (the pipeline\'s next faces).', '',
          '| id | class | status | photos | best photo |', '|---|---|---|---|---|']
    L += ['| `%s` | %s | %s | %d | %s |' % (e['id'], e['class'], e['status'], len(e['seen_in']), _best_photo(e, obs_by_photo))
          for e in queue]
    conf = [e for e in comps if e.get('conflicts') or e.get('observed_conflicts')]
    L += ['', '## Conflicts between sources', '']
    for e in conf:
        for c in e.get('conflicts', []) + e.get('observed_conflicts', []):
            L.append('- `%s`: %s' % (e['id'], c))
    if not conf:
        L.append('None.')
    nb = [e for e in comps if e.get('not_built')]
    L += ['', '## Seen but not built (H3)', '']
    L += ['- `%s`: %s' % (e['id'], e['not_built']) for e in nb] or ['None.']
    L += ['', '## Every component', '', '| id | class | zone | parent | status | position from | photos | sources |',
          '|---|---|---|---|---|---|---|---|']
    for e in sorted(comps, key=lambda e: (e['zone'], e['id'])):
        b = e.get('box_m')
        pos = b['from'] if b else (e['estimate']['from'] + ' (estimate)' if e.get('estimate') else 'none')
        L.append('| `%s` | %s | %s | %s | %s | %s | %d | %s |' % (e['id'], e['class'], e['zone'], e.get('parent') or '',
                                                                 e['status'], pos,
                                                            len(e.get('seen_in', [])), '; '.join(e.get('sources', []))))
    unacc = collections.defaultdict(set)
    for o in obs:
        for c in o.get('components', []):
            if c['id'].startswith('new.') and not any(x['id'] == c['id'][4:] for x in comps):
                unacc[c['id']].add(o['photo'])
    L += ['', '## Proposals not accepted (low confidence only)', '']
    L += ['- `%s` (%s)' % (k, ', '.join(sorted(v))) for k, v in sorted(unacc.items())] or ['None.']
    L += ['', '## The observers\' doubts', '']
    for o in obs:
        for d in o.get('doubts', []):
            L.append('- %s: %s' % (o['photo'], d))
    L += ['', '## Seen but not in the register', '']
    for o in obs:
        for d in o.get('not_in_register', []):
            L.append('- %s: %s' % (o['photo'], d))
    with open(H.REPORT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(L) + '\n')
    print('wrote', H.REPORT, len(L), 'lines')

