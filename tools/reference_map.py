"""The Perspective Reference Map: photographs and drawings of the 9A33BM2 tied to the vehicle's body frame.

data/reference_map/osa_9a33bm2.json lists every reference image (where it is, what it shows, from which side), the
vehicle's datum dimensions with their sources, the components with the images that substantiate them, and the
measurements taken from the images. A measurement is reproducible: it names a plane of the vehicle seen in a photo
(the right side, the front...), the plane's control points (pixel -> metres in the plane, e.g. the wheel hubs, whose
places the chassis data fixes) and the pixel being measured. The plane's homography turns the pixel into metres.

    python tools/reference_map.py check                 # validate the map; print every measurement (metres)
    python tools/reference_map.py html [out/reference_map]   # the browsable map (thumbnails, planes, overlays)
    python tools/reference_map.py grid <image> x0 y0 x1 y1 [--scale 2] [--out file.png]
                                                        # a zoomed crop with a pixel grid, for picking points

Images live in reference/ (git-ignored: third-party photographs); the map stores their paths and sources only.
Body frame (m): x right, y forward, z up, origin on the ground under the vehicle's reference point
(data/cabins/README.md). Status of a value: observed | multi | inferred | placeholder (see the map's "status" key).
"""
import argparse
import html
import json
import os
import sys

import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(REPO, 'data', 'reference_map', 'osa_9a33bm2.json')
STATUSES = ('observed', 'multi', 'inferred', 'placeholder')


# ------------------------------------------------------------------------------------------------- geometry

def homography(pairs):
    """3x3 H with [a, b, 1] ~ H [u, v, 1] from >= 4 pairs (u, v, a, b): pixel -> plane metres (normalised DLT)."""
    p = np.asarray(pairs, dtype=float)
    if len(p) < 4:
        raise ValueError('a plane needs at least 4 control points')

    def norm(pts):
        c = pts.mean(axis=0)
        s = np.sqrt(2) / max(np.sqrt(((pts - c) ** 2).sum(axis=1)).mean(), 1e-12)
        return np.array([[s, 0, -s * c[0]], [0, s, -s * c[1]], [0, 0, 1]])

    tu, ta = norm(p[:, 0:2]), norm(p[:, 2:4])
    rows = []
    for u, v, a, b in p:
        u, v, _ = tu @ [u, v, 1]
        a, b, _ = ta @ [a, b, 1]
        rows.append([-u, -v, -1, 0, 0, 0, a * u, a * v, a])
        rows.append([0, 0, 0, -u, -v, -1, b * u, b * v, b])
    _, _, vt = np.linalg.svd(np.asarray(rows))
    h = vt[-1].reshape(3, 3)
    h = np.linalg.inv(ta) @ h @ tu
    return h / h[2, 2]


def apply(h, u, v):
    a, b, w = h @ [u, v, 1.0]
    return a / w, b / w


def plane_fit(plane):
    """The plane's homography and its control points' residuals (m)."""
    pts = plane['points']
    h = homography(pts)
    res = [float(np.hypot(*(np.subtract(apply(h, u, v), (a, b))))) for u, v, a, b in pts]
    return h, res


# ------------------------------------------------------------------------------------------------- the map

def load(path=MAP):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def measurements(m):
    """Every measurement in the map: (photo id, measurement dict, (a, b) metres in its plane)."""
    out = []
    for ph in m['images']:
        planes = {pid: plane_fit(pl)[0] for pid, pl in ph.get('planes', {}).items()}
        for ms in ph.get('measure', []):
            h = planes[ms['plane']]
            out.append((ph['id'], ms, apply(h, *ms['px'])))
    return out


def check(m, repo=REPO, quiet=False):
    """Problems with the map (a list of strings); prints the measurements unless quiet."""
    problems = []
    ids = [i['id'] for i in m['images']]
    if len(ids) != len(set(ids)):
        problems.append('duplicate image ids')
    have_reference = os.path.isdir(os.path.join(repo, 'reference'))
    for ph in m['images']:
        if have_reference and not os.path.exists(os.path.join(repo, ph['path'])):
            problems.append(f"{ph['id']}: missing file {ph['path']}")
        for key in ('source', 'variant', 'view', 'shows'):
            if key not in ph:
                problems.append(f"{ph['id']}: no {key}")
        for pid, pl in ph.get('planes', {}).items():
            try:
                _, res = plane_fit(pl)
            except ValueError as e:
                problems.append(f"{ph['id']}/{pid}: {e}")
                continue
            worst = max(res)
            if worst > pl.get('tolerance_m', 0.08):
                problems.append(f"{ph['id']}/{pid}: control points disagree by {worst:.3f} m")
        for ms in ph.get('measure', []):
            if ms['plane'] not in ph.get('planes', {}):
                problems.append(f"{ph['id']}: measurement {ms['id']} on unknown plane {ms['plane']}")
    for d in m['datums']:
        if d.get('status') not in STATUSES:
            problems.append(f"datum {d['id']}: status must be one of {STATUSES}")
        if not d.get('source'):
            problems.append(f"datum {d['id']}: no source")
    for c in m['components']:
        if c.get('status') not in STATUSES:
            problems.append(f"component {c['id']}: status must be one of {STATUSES}")
        for e in c.get('evidence', []):
            if e not in ids and not e.startswith('doc:'):
                problems.append(f"component {c['id']}: unknown evidence {e}")
    if not problems and not quiet:
        for pid, ms, (a, b) in measurements(m):
            print(f"{pid:>14} {ms['id']:<28} {ms['plane']:<10} ({a:7.3f}, {b:6.3f}) m  {ms.get('note', '')}")
    return problems


# ------------------------------------------------------------------------------------------------- crops and html

def grid(image, box, scale=2, out=None):
    from PIL import Image, ImageDraw
    im = Image.open(image).convert('RGB')
    x0, y0, x1, y1 = box
    c = im.crop(box).resize(((x1 - x0) * scale, (y1 - y0) * scale), Image.LANCZOS)
    d = ImageDraw.Draw(c)
    step = 10 if scale >= 4 else 20
    for x in range(x0 - x0 % step, x1, step):
        X = (x - x0) * scale
        major = x % 100 == 0
        d.line([(X, 0), (X, c.height)], fill=(255, 0, 0) if major else (255, 190, 190))
        if major:
            d.text((X + 2, 2), str(x), fill=(255, 0, 0))
    for y in range(y0 - y0 % step, y1, step):
        Y = (y - y0) * scale
        major = y % 100 == 0
        d.line([(0, Y), (c.width, Y)], fill=(0, 0, 255) if major else (190, 190, 255))
        if major:
            d.text((2, Y + 2), str(y), fill=(0, 0, 255))
    out = out or os.path.splitext(os.path.basename(image))[0] + f'_{x0}_{y0}.png'
    c.save(out)
    return out


def annotate(m, ph, out_png):
    """The photo with its planes' control points (blue) and measurements (red), labelled."""
    from PIL import Image, ImageDraw
    im = Image.open(os.path.join(REPO, ph['path'])).convert('RGB')
    d = ImageDraw.Draw(im)
    for pid, pl in ph.get('planes', {}).items():
        h, _ = plane_fit(pl)
        hi = np.linalg.inv(h)
        for u, v, a, b in pl['points']:
            d.ellipse([u - 4, v - 4, u + 4, v + 4], outline=(40, 120, 255), width=2)
        # a 1 m grid over the control points' extent
        a_s = [p[2] for p in pl['points']]
        b_s = [p[3] for p in pl['points']]
        for a in np.arange(np.floor(min(a_s)) - 1, np.ceil(max(a_s)) + 2, 1.0):
            d.line([apply(hi, a, b) for b in (min(b_s) - 0.5, max(b_s) + 1.5)], fill=(40, 120, 255))
        for b in np.arange(0.0, np.ceil(max(b_s)) + 2, 0.5):
            d.line([apply(hi, a, b) for a in (min(a_s) - 1, max(a_s) + 1)], fill=(40, 120, 255))
    for ms in ph.get('measure', []):
        u, v = ms['px']
        d.line([u - 6, v, u + 6, v], fill=(255, 40, 40), width=2)
        d.line([u, v - 6, u, v + 6], fill=(255, 40, 40), width=2)
        d.text((u + 6, v - 12), ms['id'], fill=(255, 40, 40))
    im.save(out_png)


def write_html(m, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    meas = {}
    for pid, ms, ab in measurements(m):
        meas.setdefault(pid, []).append((ms, ab))
    rows = []
    rows.append('<!doctype html><meta charset="utf-8"><title>Osa reference map</title><style>'
                'body{font:14px system-ui;margin:16px;max-width:1500px} img{max-width:100%} '
                'table{border-collapse:collapse} td,th{border:1px solid #bbb;padding:3px 6px;vertical-align:top} '
                '.observed{background:#dff3df}.multi{background:#e9f6d0}.inferred{background:#fff3cf}'
                '.placeholder{background:#f8d9d9} .card{display:inline-block;width:480px;margin:6px;vertical-align:top}'
                '</style>')
    rows.append(f"<h1>{html.escape(m['title'])}</h1><p>{html.escape(m['purpose'])}</p>")
    rows.append('<h2>Datums</h2><table><tr><th>id</th><th>value</th><th>status</th><th>source</th><th>notes</th></tr>')
    for d in m['datums']:
        rows.append(f"<tr class='{d['status']}'><td>{d['id']}</td><td>{d['value']} {d.get('unit', '')}</td>"
                    f"<td>{d['status']}</td><td>{html.escape(d['source'])}</td>"
                    f"<td>{html.escape(d.get('notes', ''))}</td></tr>")
    rows.append('</table><h2>Components</h2><table><tr><th>component</th><th>status</th><th>evidence</th>'
                '<th>notes</th></tr>')
    for c in m['components']:
        ev = ', '.join(f"<a href='#{e}'>{e}</a>" if not e.startswith('doc:') else html.escape(e)
                       for e in c.get('evidence', []))
        rows.append(f"<tr class='{c['status']}'><td>{c['id']}<br><small>{html.escape(c['name'])}</small></td>"
                    f"<td>{c['status']}</td><td>{ev}</td><td>{html.escape(c.get('notes', ''))}</td></tr>")
    rows.append('</table><h2>Images by view</h2>')
    for ph in m['images']:
        src = os.path.join(REPO, ph['path'])
        img_html = ''
        if os.path.exists(src):
            png = os.path.join(out_dir, ph['id'] + '.jpg')
            if ph.get('planes') or ph.get('measure'):
                annotate(m, ph, png)
            else:
                from PIL import Image
                Image.open(src).convert('RGB').save(png, quality=85)
            img_html = f"<a href='{ph['id']}.jpg'><img src='{ph['id']}.jpg'></a>"
        overlay = os.path.join(out_dir, ph['id'] + '_overlay.jpg')
        if os.path.exists(overlay):
            img_html += f"<br><a href='{ph['id']}_overlay.jpg'><img src='{ph['id']}_overlay.jpg'></a>"
        ms_html = ''.join(f"<li>{ms['id']}: ({a:.3f}, {b:.3f}) m {html.escape(ms.get('note', ''))}</li>"
                          for ms, (a, b) in meas.get(ph['id'], []))
        rows.append(f"<div class='card' id='{ph['id']}'><b>{ph['id']}</b> - {html.escape(ph['view'])} - "
                    f"{html.escape(ph['variant'])}<br>{img_html}<br><small>{html.escape(ph['source'])}</small>"
                    f"<br>Shows: {html.escape(', '.join(ph['shows']))}<ul>{ms_html}</ul>"
                    f"<small>{html.escape(ph.get('notes', ''))}</small></div>")
    rows.append('<h2>Conflicts and open questions</h2><ul>')
    for c in m.get('conflicts', []):
        rows.append(f"<li><b>{c['id']}</b> ({c['state']}): {html.escape(c['text'])}</li>")
    rows.append('</ul>')
    path = os.path.join(out_dir, 'index.html')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(rows))
    return path


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('check')
    h = sub.add_parser('html')
    h.add_argument('out', nargs='?', default=os.path.join(REPO, 'out', 'reference_map'))
    g = sub.add_parser('grid')
    g.add_argument('image')
    g.add_argument('box', nargs=4, type=int)
    g.add_argument('--scale', type=int, default=2)
    g.add_argument('--out')
    a = ap.parse_args(argv)
    if a.cmd == 'grid':
        print(grid(a.image, a.box, a.scale, a.out))
        return 0
    m = load()
    problems = check(m, quiet=a.cmd != 'check')
    for p in problems:
        print('PROBLEM:', p)
    if a.cmd == 'html':
        print(write_html(m, a.out))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
