"""The cabin's panels against the photographs: every control, lamp, display, block and fixture of data/cabins/osa_ak.json
drawn where the data puts it, on a photo through its certified camera or on a metric flat of one panel.

    panels.py overlay <photo> [panel ...] [--pad 80] [--zoom 1.0]   # the data's items on the photo: out/recon/osa/panels/<photo>_<panels>.jpg
    panels.py list [panel]                                           # what the data has on each panel
    panels.py flat <panel>                                           # a panel's metric flat (panel_flats.json) and the data on it
    panels.py parts <panel> [--min 4] [--max 140]                    # numbered parts found on the flat, in mm
    panels.py apply <panel> [--write]                                # the picks (tools/traces/osa_interior/panels/<panel>.json) into the cabin data

Sizes are the cabin's own: a look's size_m, or the style's footprint as SimCabinComponent.cpp draws it.
Run with E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8.
"""
import json
import os
import sys

import numpy as np

import core as C

CABIN = os.path.join(C.REPO, 'data', 'cabins', 'osa_ak.json')
CONSOLE = os.path.join(C.REPO, 'data', 'procedures', 'osa_ak.json')
OUT = os.path.join(C.OUT, 'panels')
FLATS = os.path.join(C.TRACES, 'panel_flats.json')
PICKS = os.path.join(C.TRACES, 'panels')

# footprints in metres, as the Unreal cabin draws a control that has no look (tools/panel_sheet.py, SimCabinComponent.cpp)
FOOTPRINT_M = {'toggle': 0.030, 'rotary': 0.045, 'key': 0.032, 'button': 0.035, 'wheel': 0.120, 'cover': 0.050, 'lamp': 0.030}
COLOURS = {'control': (0, 220, 255), 'lamp': (60, 60, 255), 'display': (255, 160, 0), 'block': (200, 200, 200), 'fixture': (80, 255, 80)}


def cabin():
    return C.load(CABIN)


def console():
    return C.load(CONSOLE)


def axes(p):
    """simcore::CabinPanel's Normal, Right, Up (cabin.hpp)."""
    f, t = np.radians(p['facing_deg']), np.radians(p.get('tilt_deg', 0.0))
    h = np.array([np.sin(f), np.cos(f), 0.0])
    n = h * np.cos(t) + np.array([0, 0, 1.0]) * np.sin(t)
    r = np.array([-np.cos(f), np.sin(f), 0.0])
    return n, r, np.cross(n, r)


def body(p, uv):
    n, r, u = axes(p)
    uv = np.atleast_2d(np.asarray(uv, float))
    return np.asarray(p['centre_m'], float) + uv[:, :1] * r + uv[:, 1:2] * u


def default_style(c, controls):
    kind = c.get('kind', 'switch')
    if kind == 'button':
        return 'button'
    if kind == 'wheel':
        return 'wheel'
    for o in controls:
        if o.get('guard') == c['id'] and o.get('kind') != 'wheel':
            return 'cover'
    if kind == 'key':
        return 'key'
    return 'rotary' if len(c.get('position_labels', [])) > 2 else 'toggle'


def _size(entry, default):
    s = entry.get('look', {}).get('size_m', entry.get('size_m'))
    if s is None:
        return default, default
    return (s, s) if isinstance(s, (int, float)) else tuple(s)


def items(cab=None, con=None):
    """Everything the data draws on the panels: dicts with what, id, panel, at (u, v), size (w, h), kind, round."""
    cab, con = cab or cabin(), con or console()
    cons = {c['id']: c for c in con.get('controls', [])}
    out = []
    for e in cab.get('controls', []):
        c = cons.get(e['control'], {})
        style = e.get('style') or default_style(c, con.get('controls', [])) if c else e.get('style', 'toggle')
        kind = e.get('look', {}).get('kind', style)
        w, h = _size(e, FOOTPRINT_M.get(style, 0.03))
        out.append(dict(what='control', id=e['control'], panel=e['panel'], at=tuple(e['at_m']), size=(w, h), kind=kind, style=style,
                        label=e.get('look', {}).get('label') or c.get('label', ''), round=w == h, color=e.get('look', {}).get('color', '')))
    for e in cab.get('lamps', []):
        w, h = _size(e, FOOTPRINT_M['lamp'])
        out.append(dict(what='lamp', id=e['id'], panel=e['panel'], at=tuple(e['at_m']), size=(w, h), kind=e.get('look', {}).get('kind', 'lamp'),
                        label=e.get('label', ''), round=w == h, color=e.get('color', '')))
    for e in cab.get('displays', []):
        w, h = e['size_m']
        out.append(dict(what='display', id=e['id'], panel=e['panel'], at=tuple(e['at_m']), size=(w, h), kind=e['kind'], label=e.get('label', ''),
                        round=e['kind'] in ('ppi', 'dial') and w == h, color=''))
    for e in cab.get('blocks', []):
        w, h = e['size_m']
        out.append(dict(what='block', id=e['id'], panel=e['panel'], at=tuple(e['at_m']), size=(w, h), kind='block', label=e.get('label', ''), round=False, color=e.get('color', '')))
    for k, e in enumerate(cab.get('fixtures', [])):
        w, h = _size(e, 0.02)
        rnd = w == h and e['kind'] not in ('nameplate', 'window', 'legend', 'diagram', 'bezel', 'latch', 'oval')
        out.append(dict(what='fixture', id='fx%d' % k, panel=e['panel'], at=tuple(e['at_m']), size=(w, h), kind=e['kind'], label=e.get('label', ''), round=rnd, color=e.get('color', '')))
    return out


def outline(it, n=28):
    """An item's footprint in panel coordinates (u, v), closed."""
    (u, v), (w, h) = it['at'], it['size']
    if it['round']:
        a = np.linspace(0, 2 * np.pi, n + 1)
        return np.c_[u + 0.5 * w * np.cos(a), v + 0.5 * h * np.sin(a)]
    return np.array([[u - w / 2, v - h / 2], [u + w / 2, v - h / 2], [u + w / 2, v + h / 2], [u - w / 2, v + h / 2], [u - w / 2, v - h / 2]])


def draw_items(img, its, to_px, scale=1.0, names=False, only=None):
    import cv2
    for it in its:
        if only and it['what'] not in only:
            continue
        p = to_px(it)
        if p is None or not np.isfinite(p).all():
            continue
        col = COLOURS[it['what']]
        cv2.polylines(img, [np.round(p * scale).astype(np.int32)], False, col, 1, cv2.LINE_AA)
        if names and it['what'] != 'block':
            c = np.round(p[:-1].mean(0) * scale).astype(int)
            cv2.putText(img, str(it.get('n', it['id']))[-14:], (int(c[0]) + 3, int(c[1]) - 3), 0, 0.33, col, 1, cv2.LINE_AA)
    return img


def overlay(photo, panel_ids=None, pad=80, zoom=1.0, names=False, kind='clean'):
    import cv2
    cab = cabin()
    cam = C.camera(photo)
    pans = {p['id']: p for p in cab['panels']}
    its = [i for i in items(cab) if (not panel_ids or i['panel'] in panel_ids)]
    img = C.photo(photo, kind)
    h, w = img.shape[:2]
    box = [w, h, 0, 0]
    seen = []

    def to_px(it):
        p = C.project(cam, body(pans[it['panel']], outline(it)))
        if not np.isfinite(p).all():
            return None
        if p[:, 0].max() < 0 or p[:, 1].max() < 0 or p[:, 0].min() > w or p[:, 1].min() > h:
            return None
        box[0], box[1] = min(box[0], p[:, 0].min()), min(box[1], p[:, 1].min())
        box[2], box[3] = max(box[2], p[:, 0].max()), max(box[3], p[:, 1].max())
        seen.append(it['panel'])
        return p
    canvas = draw_items(img.copy(), its, to_px, names=names)
    if not seen:
        print('nothing of', panel_ids, 'is in', photo)
        return None
    x0, y0 = max(0, int(box[0] - pad)), max(0, int(box[1] - pad))
    x1, y1 = min(w, int(box[2] + pad)), min(h, int(box[3] + pad))
    pair = np.hstack([img[y0:y1, x0:x1], canvas[y0:y1, x0:x1]])
    if zoom != 1.0:
        pair = cv2.resize(pair, None, fx=zoom, fy=zoom, interpolation=cv2.INTER_AREA if zoom < 1 else cv2.INTER_CUBIC)
    os.makedirs(OUT, exist_ok=True)
    out = os.path.join(OUT, '%s_%s.jpg' % (photo, '+'.join(panel_ids) if panel_ids else 'all'))
    cv2.imwrite(out, pair, [cv2.IMWRITE_JPEG_QUALITY, 90])
    print(out, pair.shape, 'box', (x0, y0, x1, y1), 'panels', sorted(set(seen)))
    return out


def listing(panel=None):
    cab = cabin()
    its = items(cab)
    for p in cab['panels']:
        if panel and p['id'] != panel:
            continue
        mine = [i for i in its if i['panel'] == p['id']]
        by = {}
        for i in mine:
            by.setdefault(i['what'], []).append(i)
        print('%-18s %-22s %.3f x %.3f m  centre %s facing %s tilt %s  %s' % (p['id'], p['station'], p['size_m'][0], p['size_m'][1], p['centre_m'], p['facing_deg'], p.get('tilt_deg', 0),
                                                                           ' '.join('%s:%d' % (k, len(v)) for k, v in by.items())))
        if panel:
            for i in sorted(mine, key=lambda i: (i['what'], -i['at'][1], i['at'][0])):
                print('   %-8s %-12s %-28s at %+.4f %+.4f  size %.3f x %.3f  %s %s' % (i['what'], i['kind'], i['id'][:28], i['at'][0], i['at'][1], i['size'][0], i['size'][1], i['color'], i['label'][:40]))


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd')
    ap.add_argument('args', nargs='*')
    ap.add_argument('--pad', type=int, default=80)
    ap.add_argument('--zoom', type=float, default=1.0)
    ap.add_argument('--names', action='store_true')
    ap.add_argument('--raw', action='store_true')
    a = ap.parse_args()
    if a.cmd == 'overlay':
        overlay(a.args[0], a.args[1:] or None, a.pad, a.zoom, a.names, 'raw' if a.raw else 'clean')
    elif a.cmd == 'list':
        listing(a.args[0] if a.args else None)
    else:
        sys.exit('unknown command ' + a.cmd)
