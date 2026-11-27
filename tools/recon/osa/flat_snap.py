"""The cabin data's items on a panel's view flat snapped to the parts SAM 2 found there, row by row: each row of a block
(items whose heights chain within 6 mm) takes the one shift that lands most of its items on parts of their size, then
each item goes to the part it lands on (within 4 mm), or moves with its row where SAM 2 found no part for it.

    flat_snap.py <flat id> [--reach 35] [--write]    # proposals: <id>_snap.jpg (old red, new green) and <id>_snap.json
    flat_snap.py <flat id> --skip 3,17,40 --write    # leave some items (the numbers of <id>_data.txt) where they are
    flat_snap.py <flat id> --affine                  # per block, the stretch and shift that carry the data onto the parts

A view flat is in the data's own millimetres (panel_flats.py), so a part's at_mm is the item's at_m in mm. Only the
place moves; sizes are reported, never changed here. Blocks, displays, nameplates, windows, legends and diagrams stay.
Run with E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8.
"""
import argparse
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import jsonedit as J  # noqa: E402
import panels as P  # noqa: E402
import panel_flats as F  # noqa: E402

STAY = ('block', 'nameplate', 'window', 'legend', 'diagram', 'bezel', 'latch', 'ppi', 'scope', 'crt', 'tube', 'dial')


def parts_of(fid):
    with open(os.path.join(F.OUT, fid + '_parts.json'), encoding='utf-8') as f:
        return json.load(f)['parts']


def part_size(p):
    return p.get('dia_mm') or max(p['box_mm'])


def _compatible(it, parts, lo=0.5, hi=2.0):
    s = max(it['size']) * 1000.0
    return np.array([lo <= part_size(p) / s <= hi for p in parts])


def rows_of(its, movable, gap_mm=6.0):
    """The movable items as rows: those of one block whose heights chain within gap_mm."""
    blocks = [(n, it) for n, it in enumerate(its) if it['what'] == 'block']

    def block_of(it):
        (u, v) = it['at']
        for n, b in blocks:
            (bu, bv), (bw, bh) = b['at'], b['size']
            if abs(u - bu) <= bw / 2 and abs(v - bv) <= bh / 2:
                return n
        return None
    by_block = {}
    for n in movable:
        by_block.setdefault(block_of(its[n]), []).append(n)
    rows = []
    for g, ns in by_block.items():
        ns = sorted(ns, key=lambda n: its[n]['at'][1])
        cur = [ns[0]]
        for n in ns[1:]:
            if (its[n]['at'][1] - its[cur[-1]]['at'][1]) * 1000.0 <= gap_mm:
                cur.append(n)
            else:
                rows.append((g, cur))
                cur = [n]
        rows.append((g, cur))
    return rows


def propose(fid, reach_mm=35.0, skip=(), by_block=True, snap_mm=4.0):
    """Each row's shift: of all item-to-part vectors within reach_mm, the one that puts most of the row's items within
    snap_mm of a part of their size (ties: the smaller residual, then the smaller shift). Then each item goes to the
    part it lands on, or moves with its row if none is there."""
    meta = F.meta(fid)
    pid = meta['panel']
    its = [it for it in P.items() if it['panel'] == pid]
    parts = parts_of(fid)
    pts = np.array([p['at_mm'] for p in parts], float)
    movable = [n for n, it in enumerate(its) if n not in skip and it['what'] not in ('block', 'display') and it['kind'] not in STAY]
    ok = {n: _compatible(its[n], parts) for n in movable}
    at = {n: np.array(its[n]['at'], float) * 1000.0 for n in movable}
    scored = []
    for g, row in rows_of(its, movable):
        cands = [np.zeros(2)]
        for n in row:
            d = pts[ok[n]] - at[n]
            cands += [v for v in d if np.hypot(*v) <= reach_mm]
        best = None
        for t in cands:
            hits, res = 0, 0.0
            for n in row:
                if not ok[n].any():
                    continue
                dd = np.linalg.norm(pts[ok[n]] - (at[n] + t), axis=1).min()
                if dd <= snap_mm:
                    hits += 1
                    res += dd
            key = (hits, -res, -float(np.hypot(*t)))
            if best is None or key > best[0]:
                best = (key, t)
        (hits, res, _), t = best
        need = 1 if len(row) == 1 else 2
        if hits >= need and not (len(row) == 1 and np.hypot(*t) > 10.0):
            scored.append((hits / len(row), hits, res, row, t))
    scored.sort(key=lambda s: (-s[0], -s[1], -s[2]))
    taken, out = set(), []
    for frac, hits, _, row, t in scored:
        for n in sorted(row, key=lambda n: its[n]['at'][0]):
            p = at[n] + t
            idx = np.where(ok[n])[0]
            k = None
            if len(idx):
                dd = np.linalg.norm(pts[idx] - p, axis=1)
                for j in np.argsort(dd):
                    if dd[j] > snap_mm:
                        break
                    if idx[j] not in taken:
                        k = int(idx[j])
                        taken.add(k)
                        break
            new = pts[k] if k is not None else p
            it = its[n]
            out.append(dict(n=n, what=it['what'], id=it['id'], label=it.get('label', ''), old_mm=[round(v, 1) for v in at[n]],
                            new_mm=[round(float(v), 1) for v in new], moved_mm=round(float(np.hypot(*(new - at[n]))), 1),
                            how='part %d' % parts[k]['n'] if k is not None else 'with its row', row_shift_mm=[round(float(v), 1) for v in t],
                            row_hits='%d/%d' % (hits, len(row)), size_mm=round(max(it['size']) * 1000, 1),
                            part_size_mm=round(part_size(parts[k]), 1) if k is not None else None))
    out.sort(key=lambda r: r['n'])
    return its, out


def propose_affine(fid, skip=(), reach_mm=70.0, tol_mm=5.0, seed=0):
    """For a flat whose millimetres are stretched (a panel far from the camera's control): each block's items carried
    onto the parts by the block's own stretch and shift (u' = a u + c, v' = b v + d), found by RANSAC over item-part
    pairs of matching size. An inlier lying off its part by more than 2 mm is moved there, in the data's millimetres
    (the fit undone); an item no part fits stays."""
    rng = np.random.default_rng(seed)
    meta = F.meta(fid)
    pid = meta['panel']
    its = [it for it in P.items() if it['panel'] == pid]
    parts = parts_of(fid)
    pts = np.array([p['at_mm'] for p in parts], float)
    movable = [n for n, it in enumerate(its) if n not in skip and it['what'] not in ('block', 'display') and it['kind'] not in STAY]
    blocks = [(n, it) for n, it in enumerate(its) if it['what'] == 'block']
    groups = {}
    for n in movable:
        (u, v) = its[n]['at']
        g = next((b for b, bi in blocks if abs(u - bi['at'][0]) <= bi['size'][0] / 2 and abs(v - bi['at'][1]) <= bi['size'][1] / 2), None)
        groups.setdefault(g, []).append(n)
    out = []
    for g, ns in groups.items():
        at = {n: np.array(its[n]['at'], float) * 1000.0 for n in ns}
        ok = {n: _compatible(its[n], parts) for n in ns}
        pairs = [(n, int(k)) for n in ns for k in np.where(ok[n])[0] if np.hypot(*(pts[k] - at[n])) <= reach_mm]
        if len(pairs) < 2:
            continue
        best = None
        for _ in range(3000):
            (n1, k1), (n2, k2) = [pairs[i] for i in rng.choice(len(pairs), 2, replace=False)]
            if n1 == n2:
                continue
            du, dv = at[n2] - at[n1]
            if abs(du) < 20 or abs(dv) < 8:
                continue
            a = (pts[k2][0] - pts[k1][0]) / du
            b = (pts[k2][1] - pts[k1][1]) / dv
            if not (0.8 < a < 1.2 and 0.8 < b < 1.2):
                continue
            c, d = pts[k1][0] - a * at[n1][0], pts[k1][1] - b * at[n1][1]
            inl = {}
            for n in ns:
                q = np.array([a * at[n][0] + c, b * at[n][1] + d])
                idx = np.where(ok[n])[0]
                if len(idx):
                    dd = np.linalg.norm(pts[idx] - q, axis=1)
                    j = int(np.argmin(dd))
                    if dd[j] <= tol_mm:
                        inl[n] = (int(idx[j]), float(dd[j]))
            key = (len(inl), -sum(r for _, r in inl.values()))
            if best is None or key > best[0]:
                best = (key, (a, b, c, d), inl)
        if best is None or best[0][0] < 3:
            continue
        _, (a, b, c, d), inl = best
        # refit on the inliers (least squares per axis)
        X = np.array([at[n] for n in inl]); Y = np.array([pts[k] for k, _ in inl.values()])
        a, c = np.polyfit(X[:, 0], Y[:, 0], 1) if np.ptp(X[:, 0]) > 20 else (a, c)
        b, d = np.polyfit(X[:, 1], Y[:, 1], 1) if np.ptp(X[:, 1]) > 8 else (b, d)
        name = its[g]['id'] if g is not None else '-'
        print('block %s: u x%.3f %+.1f, v x%.3f %+.1f, %d of %d items on parts' % (name, a, c, b, d, len(inl), len(ns)))
        taken = set()
        for n in sorted(inl, key=lambda n: inl[n][1]):
            k = inl[n][0]
            if k in taken:
                continue
            taken.add(k)
            new = np.array([(pts[k][0] - c) / a, (pts[k][1] - d) / b])
            it = its[n]
            out.append(dict(n=n, what=it['what'], id=it['id'], label=it.get('label', ''), old_mm=[round(v, 1) for v in at[n]],
                            new_mm=[round(float(v), 1) for v in new], moved_mm=round(float(np.hypot(*(new - at[n]))), 1),
                            how='part %d' % parts[k]['n'], row_shift_mm=[round(a, 3), round(b, 3)], row_hits='%d/%d' % (len(inl), len(ns)),
                            size_mm=round(max(it['size']) * 1000, 1), part_size_mm=round(part_size(parts[k]), 1), block=name,
                            fit=[float(a), float(b), float(c), float(d)]))
    out.sort(key=lambda r: r['n'])
    return its, out


def draw(fid, its, props, zoom=0.5):
    meta = F.meta(fid)
    ppm, (ou, ov) = meta['px_per_mm'], meta['origin_mm']
    img = F.image(fid).copy()

    def px(mm):
        return int(round((mm[0] + ou) * ppm)), int(round((ov - mm[1]) * ppm))
    def on_flat(r, key):                               # an affine proposal's data mm drawn where its block's fit puts them
        if 'fit' not in r:
            return r[key]
        fa, fb, fc, fd = r['fit']
        return [fa * r[key][0] + fc, fb * r[key][1] + fd]
    for r in props:
        a, b = px(on_flat(r, 'old_mm')), px(on_flat(r, 'new_mm'))
        rad = max(3, int(r['size_mm'] * ppm / 2))
        cv2.circle(img, a, rad, (0, 0, 255), 2, cv2.LINE_AA)
        cv2.circle(img, b, rad, (0, 255, 0), 2, cv2.LINE_AA)
        cv2.line(img, a, b, (0, 255, 255), 2, cv2.LINE_AA)
        cv2.putText(img, str(r['n']), (b[0] + rad, b[1] - rad), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (255, 0, 255), 2, cv2.LINE_AA)
    moved = {r['n'] for r in props}
    for n, it in enumerate(its):                       # items left where they are, in white
        if n in moved or it['what'] in ('block',):
            continue
        cv2.polylines(img, [np.round(np.array([px(q * 1000) for q in P.outline(it)])).astype(np.int32)], False, (255, 255, 255), 1, cv2.LINE_AA)
    img = cv2.resize(img, None, fx=zoom, fy=zoom, interpolation=cv2.INTER_AREA)
    path = os.path.join(F.OUT, fid + '_snap.jpg')
    cv2.imwrite(path, img, [cv2.IMWRITE_JPEG_QUALITY, 88])
    return path


def write(props):
    """The moves into data/cabins/osa_ak.json, number by number, layout kept."""
    text = J.read(P.CABIN)
    for r in props:
        u, v = r['new_mm'][0] / 1000.0, r['new_mm'][1] / 1000.0
        if r['what'] == 'control':
            path = ['controls', {'control': r['id']}, 'at_m']
        elif r['what'] == 'lamp':
            path = ['lamps', {'id': r['id']}, 'at_m']
        else:
            path = ['fixtures', int(r['id'][2:]), 'at_m']
        text = J.set_value(text, path + [0], u, 4)
        text = J.set_value(text, path + [1], v, 4)
    J.write(P.CABIN, text)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('flat')
    ap.add_argument('--reach', type=float, default=35.0)
    ap.add_argument('--skip', default='')
    ap.add_argument('--zoom', type=float, default=0.5)
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--affine', action='store_true', help='per-block stretch and shift (a flat far from the control points of its camera)')
    a = ap.parse_args()
    skip = {int(s) for s in a.skip.split(',') if s.strip()}
    its, props = propose_affine(a.flat, skip) if a.affine else propose(a.flat, a.reach, skip)
    with open(os.path.join(F.OUT, a.flat + '_snap.json'), 'w', encoding='utf-8') as f:
        json.dump(props, f, ensure_ascii=False, indent=1)
    print(draw(a.flat, its, props, a.zoom))
    big = [r for r in props if r['moved_mm'] > 2.0]
    print('%d items placed, %d moved more than 2 mm (median %.1f mm)' % (len(props), len(big), np.median([r['moved_mm'] for r in props]) if props else 0))
    for r in props:
        print('%3d %-8s %-24s %5.1f mm  row %-12s %-5s %-13s size %4.1f/%s  %s' % (r['n'], r['what'], r['id'][:24], r['moved_mm'], r['row_shift_mm'], r['row_hits'], r['how'], r['size_mm'], r['part_size_mm'], r['label'][:28]))
    if a.write:
        write(props)
        print('written', P.CABIN)
