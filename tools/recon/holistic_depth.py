"""The look-alike check's depth comparison (docs/OSA_INTERIOR_HOLISTIC.md section 29): each point the joint solve triangulated from the book's
photos, against the built cabin along the ray from the photo that saw it. The solve's points are the photos' own measured depth.

    python tools/recon/holistic_depth.py                 # the triangulated points against the cabin: out/recon/holistic/depth/
    python tools/recon/holistic_depth.py --summary       # print the last run's table
    python tools/recon/holistic_depth.py all             # the depth maps (section 31): mono, align, cross, model
    python tools/recon/holistic_depth.py mono            # MoGe-2 on every interior photo (its own environment, E:/SAM3D-tools/venv-depth)
    python tools/recon/holistic_depth.py align           # each map to its camera's triangulated points
    python tools/recon/holistic_depth.py cross           # each view against every other: the confirmed cloud
    python tools/recon/holistic_depth.py model           # the maps against the built cabin, every solved camera
    python tools/recon/holistic_depth.py slices          # the confirmed cloud on the model: sections and plans

A positive residual: the model's surface lies beyond the photo's point (the model lacks something there, or stands too far back).
Negative: the model stands in front of what the photo saw through that ray. Reads the solve and the cabin, writes only under out/.
"""
import argparse
import json
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import holistic_view as V  # noqa: E402

OUT = os.path.join(V.REPO, 'out', 'recon', 'holistic', 'depth')
POINTS = os.path.join(V.REPO, 'out', 'recon', 'holistic', 'sfm', 'points.json')
_REFINED = os.path.join(V.REPO, 'out', 'recon', 'holistic', 'sfm', 'refined', 'points.json')
if os.path.exists(_REFINED):      # with the cameras of tools/recon/holistic_refine.py
    POINTS = _REFINED
FORWARD = ('g082', 'g083', 'g084', 'g085', 'g086', 'g087', 'g088', 'g089', 'g090', 'g091')   # the cab group, not to scale (section 16)
BAND_M = 0.05


def compartment(photo):
    return not photo.startswith(FORWARD)


def residual(range_m, eye, point):
    """Model's range along the ray less the photo point's range."""
    return float(range_m) - float(np.linalg.norm(np.asarray(point, float) - np.asarray(eye, float)))


def rays(cameras, points):
    """(photo, point index, eye) and the rays, one per sighting from a compartment camera."""
    eyes = {}
    for k, c in cameras.items():
        if compartment(k):
            R, t = np.array(c['R'], float), np.array(c['t'], float)
            eyes[k] = -R.T @ t
    out, meta = [], []
    for i, (x, y, z, seen) in enumerate(points):
        for k in seen:
            if k in eyes:
                d = np.array([x, y, z], float) - eyes[k]
                out.append(['%s|%d' % (k, i), eyes[k].tolist(), d.tolist()])
                meta.append((k, i))
    return out, meta, eyes


def summarise(rows):
    """rows: (part, residual). Per part: count, median, share within the band."""
    by = {}
    for part, r in rows:
        by.setdefault(part or '(none)', []).append(r)
    table = []
    for part, v in by.items():
        a = np.array(v)
        table.append({'part': part, 'n': len(v), 'median_m': round(float(np.median(a)), 3), 'within_band': round(float(np.mean(np.abs(a) < BAND_M)), 3)})
    return sorted(table, key=lambda e: -e['n'])


def run():
    import cv2
    cameras = V.load(V.SFM_CAMERAS)['cameras']
    points = V.load(POINTS)
    ry, meta, eyes = rays(cameras, points)
    os.makedirs(OUT, exist_ok=True)
    sample = V.load(os.path.join(V.OUT, 'g096-a', 'render_job.json'))
    prefix = os.path.join(OUT, 'depth_').replace('\\', '/')
    job = dict(sample, out=prefix, passes=[], rays=ry)
    path = os.path.join(OUT, 'job.json')
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(job, f)
    subprocess.run([V.BLENDER, '--background', '--factory-startup', '--python', V.SCENE, '--', path], check=True, capture_output=True, text=True)
    hits = V.load(prefix + 'cast.json')
    rows, per_photo = [], {}
    for k, i in meta:
        h = hits.get('%s|%d' % (k, i))
        if not h:
            continue
        r = residual(h['range_m'], eyes[k], points[i][:3])
        rows.append((h['part'], r))
        per_photo.setdefault(k, []).append((i, r, h['part']))
    table = summarise(rows)
    with open(os.path.join(OUT, 'summary.json'), 'w', encoding='utf-8') as f:
        json.dump({'band_m': BAND_M, 'sightings': len(rows), 'parts': table}, f, indent=1)
    for k, v in per_photo.items():
        im = cv2.imread(V.photo_file(k, 'clean'))
        c = V.Camera.from_dict(cameras[k])
        h, w = im.shape[:2]
        c = c.scaled(w, h) if (c.w, c.h) != (w, h) else c
        for i, r, _ in v:
            p = c.project(np.array(points[i][:3], float))[0]
            col = (60, 200, 60) if abs(r) < BAND_M else ((60, 60, 255) if r < 0 else (255, 120, 40))
            cv2.circle(im, (int(p[0]), int(p[1])), max(4, w // 300), col, -1)
        s = 1100.0 / im.shape[1]
        cv2.imwrite(os.path.join(OUT, k + '.jpg'), cv2.resize(im, None, fx=s, fy=s, interpolation=cv2.INTER_AREA), [cv2.IMWRITE_JPEG_QUALITY, 84])
    return table


# The depth maps (section 31): every interior photo through MoGe-2, tied to the joint solve, checked view against view and against the model.
MONO = os.path.join(OUT, 'mono')
MONO_PYTHON = os.environ.get('SAM3D_DEPTH_PYTHON', r'E:\SAM3D-tools\venv-depth\Scripts\python.exe')
INTERIOR = tuple('g%03d' % i for i in range(82, 102))
MAP_SIZE = 1600        # the maps' longest side
AGREE = 0.04           # two views agree when their ranges differ by less than this share
GRID = 128             # rays along the longest side per view, for the model
MIN_SHARE = 0.5        # views confirmed less than this stay out of the fused cloud
SECTIONS_Y = (-0.7, -0.3, 0.15, 0.45, 0.8, 1.3, 1.8, 2.3, 2.7, 3.1)    # y of the cross-sections
PLANS_Z = ((0.68, 0.95), (1.0, 1.3), (1.35, 1.6), (1.65, 1.9), (1.95, 2.2))


def interior_photos():
    d = os.path.dirname(V.photo_file('g096-a', 'clean'))
    return sorted(f[:-4] for f in os.listdir(d) if f.startswith(INTERIOR) and f.endswith(('.png', '.jpg')))


def mono():
    os.makedirs(MONO, exist_ok=True)
    cams = V.load(V.SFM_CAMERAS)['cameras']
    photos = []
    for k in interior_photos():
        fov = None
        if k in cams:
            fov = float(np.degrees(2 * np.arctan(0.5 * cams[k]['size_px'][0] / cams[k]['f_px'])))
        photos.append({'id': k, 'image': V.photo_file(k, 'clean'), 'fov_x_deg': fov, 'out': os.path.join(MONO, k + '.npz')})
    path = os.path.join(MONO, 'job.json')
    with open(path, 'w', encoding='utf-8') as f:
        json.dump({'size': MAP_SIZE, 'photos': photos}, f)
    env = dict(os.environ, HF_HOME=r'E:\SAM3D-tools\models\hf', HF_HUB_DISABLE_SYMLINKS_WARNING='1')
    subprocess.run([MONO_PYTHON, '-X', 'utf8', os.path.join(HERE, 'holistic_mono_infer.py'), path], check=True, env=env)


def poly_terms(u, v):
    """The low-order field the map's scale may vary by across the image (u, v in -1..1)."""
    return np.stack([np.ones_like(u), u, v, u * u, u * v, v * v], axis=1)


def fit_scale(u, v, ratio, iterations=8):
    """log(triangulated range / map's range) as a smooth field across the image, robustly; few points, one number."""
    lr = np.log(ratio)
    if len(lr) < 60:
        c = np.zeros(6)
        c[0] = float(np.median(lr))
        return c, float(np.median(np.abs(lr - c[0])))
    T, w = poly_terms(u, v), np.ones(len(lr))
    c = np.zeros(6)
    for _ in range(iterations):
        c = np.linalg.lstsq(T * w[:, None], lr * w, rcond=None)[0]
        r = lr - T @ c
        mad = max(1e-4, float(np.median(np.abs(r))))
        w = 1.0 / np.maximum(1.0, np.abs(r) / (2.5 * mad))
    return c, float(np.median(np.abs(lr - T @ c)))


def scale_at(c, x, y, w, h):
    return np.exp(poly_terms(2.0 * x / w - 1.0, 2.0 * y / h - 1.0) @ c)


def load_map(k):
    z = np.load(os.path.join(MONO, k + '.npz'))
    return z['depth'].astype(float), z['normal'].astype(float) / 1000.0


def camera_on_map(cams, k, shape):
    c = V.Camera.from_dict(cams[k])
    return c.scaled(shape[1], shape[0])


def align():
    cams = V.load(V.SFM_CAMERAS)['cameras']
    points = V.load(POINTS)
    seen = {}
    for i, (x, y, z, ph) in enumerate(points):
        for k in ph:
            seen.setdefault(k, []).append([x, y, z])
    out = {}
    for k in cams:
        if not os.path.exists(os.path.join(MONO, k + '.npz')) or k not in seen:
            continue
        d, _ = load_map(k)
        h, w = d.shape
        c = camera_on_map(cams, k, d.shape)
        X = np.array(seen[k], float)
        Xc = c.cam(X)
        px = c.project(X)
        ok = np.isfinite(px).all(1) & (px[:, 0] >= 0) & (px[:, 0] < w - 1) & (px[:, 1] >= 0) & (px[:, 1] < h - 1)
        px, zs = px[ok], Xc[ok, 2]
        zm = d[px[:, 1].astype(int), px[:, 0].astype(int)]
        g = np.isfinite(zm) & (zm > 0.05)
        px, zs, zm = px[g], zs[g], zm[g]
        coef, mad = fit_scale(2.0 * px[:, 0] / w - 1.0, 2.0 * px[:, 1] / h - 1.0, zs / zm)
        out[k] = {'coef': coef.tolist(), 'mad': round(mad, 4), 'points': int(len(zs)), 'metric_scale': round(float(np.median(zs / zm)), 3)}
    with open(os.path.join(OUT, 'align.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)
    return out


def aligned(k, cams, al):
    """The map as z along the camera's axis in the solve's frame, and its camera."""
    d, n = load_map(k)
    h, w = d.shape
    y, x = np.mgrid[0:h, 0:w]
    s = scale_at(np.array(al[k]['coef']), x.ravel().astype(float), y.ravel().astype(float), w, h).reshape(h, w)
    return d * s, n, camera_on_map(cams, k, d.shape)


def group(k):
    return 'cab' if not compartment(k) else 'compartment'


def lift(z, c, stride):
    h, w = z.shape
    y, x = np.mgrid[stride // 2:h:stride, stride // 2:w:stride]
    zz = z[y, x]
    g = np.isfinite(zz) & (zz > 0.05)
    x, y, zz = x[g].astype(float), y[g].astype(float), zz[g]
    Xc = np.stack([(x - c.cx) / c.f * zz, (y - c.cy) / c.f * zz, zz], axis=1)
    return (c.R.T @ (Xc - c.t).T).T, x, y


def cross():
    """Each view's points looked at from every other view of its group: confirmed where another map puts a surface there too."""
    cams, al = V.load(V.SFM_CAMERAS)['cameras'], V.load(os.path.join(OUT, 'align.json'))
    maps = {k: aligned(k, cams, al) for k in al}
    rows, table = [], {}
    for k, (z, n, c) in maps.items():
        X, _, _ = lift(z, c, 8)
        agree, looked = np.zeros(len(X), int), np.zeros(len(X), int)
        for o, (zo, _, co) in maps.items():
            if o == k or group(o) != group(k):
                continue
            px, zc = co.project(X), co.cam(X)[:, 2]
            hh, ww = zo.shape
            ok = np.isfinite(px).all(1) & (px[:, 0] >= 0) & (px[:, 0] < ww) & (px[:, 1] >= 0) & (px[:, 1] < hh)
            i = np.flatnonzero(ok)
            zz = zo[px[i, 1].astype(int), px[i, 0].astype(int)]
            f = np.isfinite(zz)
            i, zz = i[f], zz[f]
            front = zz > zc[i] * (1 - AGREE)          # another surface nearer in that view hides the point: says nothing
            looked[i[front]] += 1
            agree[i[front & (np.abs(zz - zc[i]) < AGREE * zc[i])]] += 1
        conf = agree >= 1
        table[k] = {'points': int(len(X)), 'looked_at': int((looked > 0).sum()), 'confirmed': int(conf.sum()),
                    'share': round(float(conf.sum()) / max(1, int((looked > 0).sum())), 3)}
        if table[k]['share'] >= MIN_SHARE:            # a view most of whose points no other view confirms has a bad pose
            rows.append(np.column_stack([X[conf], agree[conf], np.full(conf.sum(), 0 if group(k) == 'compartment' else 1)]))
    cloud = np.vstack(rows)
    np.save(os.path.join(OUT, 'cloud.npy'), cloud.astype(np.float32))
    with open(os.path.join(OUT, 'cross.json'), 'w', encoding='utf-8') as f:
        json.dump(table, f, indent=1)
    return table


def turbo(a, lo, hi):
    import cv2
    v = np.clip((np.nan_to_num(a, nan=lo) - lo) / max(1e-6, hi - lo), 0, 1)
    return cv2.applyColorMap((v * 255).astype(np.uint8), cv2.COLORMAP_TURBO)


def diverging(r, band):
    """Blue: the model's surface beyond the photo's; red: in front of it; white within the band; grey no data."""
    v = np.clip(np.nan_to_num(r) / (4 * band), -1, 1)
    im = np.full(r.shape + (3,), 255, np.uint8).astype(float)
    pos, neg = v > 0, v < 0
    im[pos, 1] = 255 * (1 - v[pos])
    im[pos, 2] = 255 * (1 - v[pos])
    im[neg, 0] = 255 * (1 + v[neg])
    im[neg, 1] = 255 * (1 + v[neg])
    im[~np.isfinite(r)] = 120
    return im.astype(np.uint8)


def sample_model_job(prefix, rays):
    """The cabin cast by Blender: {key: hit}."""
    sample = V.load(os.path.join(V.OUT, 'g096-a', 'render_job.json'))
    job = dict(sample, out=prefix, passes=[], rays=rays)
    path = prefix + 'job.json'
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(job, f)
    subprocess.run([V.BLENDER, '--background', '--factory-startup', '--python', V.SCENE, '--', path], check=True, capture_output=True, text=True)
    return V.load(prefix + 'cast.json')


def model():
    """Every solved camera: photo, its depth map, the model's from the same camera, and the difference. The compartment's by part."""
    import cv2
    cams, al = V.load(V.SFM_CAMERAS)['cameras'], V.load(os.path.join(OUT, 'align.json'))
    os.makedirs(os.path.join(OUT, 'maps'), exist_ok=True)
    rays, grids = [], {}
    for k in al:
        z, _, c = aligned(k, cams, al)
        h, w = z.shape
        stride = max(1, max(h, w) // GRID)
        y, x = np.mgrid[stride // 2:h:stride, stride // 2:w:stride]
        d = (c.R.T @ np.stack([(x.ravel() - c.cx) / c.f, (y.ravel() - c.cy) / c.f, np.ones(x.size)], axis=0)).T
        e = c.centre().tolist()
        for i in range(x.size):
            rays.append(['%s|%d' % (k, i), e, d[i].tolist()])
        grids[k] = (x, y)
    hits = sample_model_job(os.path.join(OUT, 'model_'), rays)
    rows, cloud = [], []
    per = {}
    for k in al:
        z, _, c = aligned(k, cams, al)
        x, y = grids[k]
        mz, mr = np.full(x.shape, np.nan), np.full(x.shape, np.nan)
        zr = np.full(x.shape, np.nan)
        for i, (j, q) in enumerate(np.ndindex(*x.shape)):
            h = hits.get('%s|%d' % (k, i))
            ray = np.array([(x[j, q] - c.cx) / c.f, (y[j, q] - c.cy) / c.f, 1.0])
            zz = z[y[j, q], x[j, q]]
            if not np.isfinite(zz):
                continue
            pr = zz * np.linalg.norm(ray)
            zr[j, q] = zz
            if h:
                mz[j, q] = h['range_m'] / np.linalg.norm(ray)
                r = h['range_m'] - pr
                mr[j, q] = r
                if compartment(k):
                    rows.append((h['part'], r))
                cloud.append(h['body_m'])
        per[k] = {'median_m': round(float(np.nanmedian(mr)), 3) if np.isfinite(mr).any() else None, 'within_band': round(float(np.mean(np.abs(mr[np.isfinite(mr)]) < BAND_M)), 3) if np.isfinite(mr).any() else None, 'mad_to_points': al[k]['mad'], 'group': group(k)}
        im = cv2.imread(V.photo_file(k, 'clean'))
        H = 420
        W = round(im.shape[1] * H / im.shape[0])
        lo, hi = float(np.nanpercentile(zr, 2)), float(np.nanpercentile(zr, 98))
        rs = lambda a: cv2.resize(a, (W, H), interpolation=cv2.INTER_NEAREST)
        sheet = np.hstack([cv2.resize(im, (W, H), interpolation=cv2.INTER_AREA), rs(turbo(zr, lo, hi)), rs(turbo(mz, lo, hi)), rs(diverging(mr, BAND_M))])
        cv2.imwrite(os.path.join(OUT, 'maps', k + '.jpg'), sheet, [cv2.IMWRITE_JPEG_QUALITY, 85])
    table = summarise(rows)
    np.save(os.path.join(OUT, 'model_cloud.npy'), np.array(cloud, np.float32))
    with open(os.path.join(OUT, 'model.json'), 'w', encoding='utf-8') as f:
        json.dump({'views': per, 'parts': table}, f, indent=1)
    return table, per


def slices():
    """The confirmed cloud (colour by how many views agree) on the model's own hits (grey), as cross-sections and plans: out/recon/holistic/depth/slices/."""
    import cv2
    d = os.path.join(OUT, 'slices')
    os.makedirs(d, exist_ok=True)
    cloud, m = np.load(os.path.join(OUT, 'cloud.npy')), np.load(os.path.join(OUT, 'model_cloud.npy'))
    cloud = cloud[cloud[:, 4] == 0]
    px = 260

    def panel(a, b, ca, cb, x0, x1, y0, y1, flip, title):
        W, H = int((x1 - x0) * px) + 80, int((y1 - y0) * px) + 60
        im = np.full((H, W, 3), 255, np.uint8)
        def to(u, v):
            return (40 + ((u - x0) * px).astype(int), (H - 30 - ((v - y0) * px).astype(int)))
        for t in np.arange(np.ceil(x0 * 5) / 5, x1 + 1e-6, 0.2):
            x = int(40 + (t - x0) * px)
            cv2.line(im, (x, 30), (x, H - 30), (225, 225, 225), 1)
            cv2.putText(im, '%.1f' % t, (x - 14, H - 12), 0, 0.38, (90, 90, 90), 1, cv2.LINE_AA)
        for t in np.arange(np.ceil(y0 * 5) / 5, y1 + 1e-6, 0.2):
            y = int(H - 30 - (t - y0) * px)
            cv2.line(im, (40, y), (W - 40, y), (225, 225, 225), 1)
            cv2.putText(im, '%.1f' % t, (2, y + 4), 0, 0.38, (90, 90, 90), 1, cv2.LINE_AA)
        def clip(u, v):
            k = (u >= x0) & (u <= x1) & (v >= y0) & (v <= y1)
            return k
        k = clip(a, b)
        xp, yp = to(a[k], b[k])
        im[np.clip(yp, 0, H - 1), np.clip(xp, 0, W - 1)] = (150, 150, 150)
        k = clip(ca[0], ca[1])
        xp, yp = to(ca[0][k], ca[1][k])
        col = cb[k]
        im[np.clip(yp, 0, H - 1), np.clip(xp, 0, W - 1)] = np.stack([(col >= 3) * 200, (col == 2) * 150, (col == 1) * 230 + (col >= 3) * 30], axis=1).astype(np.uint8)
        cv2.putText(im, title, (46, 22), 0, 0.55, (0, 0, 0), 1, cv2.LINE_AA)
        return im
    for y in SECTIONS_Y:
        c, mm = cloud[np.abs(cloud[:, 1] - y) < 0.05], m[np.abs(m[:, 1] - y) < 0.05]
        cv2.imwrite(os.path.join(d, 'section_y%+.2f.png' % y), panel(mm[:, 0], mm[:, 2], (c[:, 0], c[:, 2]), c[:, 3], -1.6, 1.6, 0.5, 2.3, False, 'section at y %.2f  x across, z up: grey model, colour photos' % y))
    for lo, hi in PLANS_Z:
        c, mm = cloud[(cloud[:, 2] >= lo) & (cloud[:, 2] < hi)], m[(m[:, 2] >= lo) & (m[:, 2] < hi)]
        cv2.imwrite(os.path.join(d, 'plan_z%.2f-%.2f.png' % (lo, hi)), panel(mm[:, 0], mm[:, 1], (c[:, 0], c[:, 1]), c[:, 3], -1.6, 1.6, -1.3, 3.3, False, 'plan z %.2f-%.2f  x across, y forward: grey model, colour photos' % (lo, hi)))
    return d


def print_table(table):
    print('%-22s %7s %9s %8s' % ('part', 'points', 'median m', 'in band'))
    for e in table[:40]:
        print('%-22s %7d %+9.3f %8.0f%%' % (e['part'], e['n'], e['median_m'], 100 * e['within_band']))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('stage', nargs='?', choices=['points', 'mono', 'align', 'cross', 'model', 'slices', 'all'], default='points')
    ap.add_argument('--summary', action='store_true')
    a = ap.parse_args(argv)
    if a.stage in ('mono', 'all'):
        mono()
    if a.stage in ('align', 'all'):
        al = align()
        print('aligned %d maps; median spread to the triangulated points %.1f %%' % (len(al), 100 * float(np.median([v['mad'] for v in al.values()]))))
    if a.stage in ('cross', 'all'):
        t = cross()
        print('cross-checked %d views; median share confirmed by another view %.0f %%' % (len(t), 100 * float(np.median([v['share'] for v in t.values()]))))
    if a.stage in ('model', 'all'):
        table, _ = model()
        print_table(table)
    if a.stage in ('slices', 'all'):
        print('slices in', slices())
    if a.stage != 'points':
        return 0
    table = V.load(os.path.join(OUT, 'summary.json'))['parts'] if a.summary else run()
    print_table(table)
    return 0


if __name__ == '__main__':
    sys.exit(main())
