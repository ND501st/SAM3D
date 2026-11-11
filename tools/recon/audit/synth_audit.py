"""Synthetic round trip, steps 2-3 (handoff section 8.1), and the evaluation audit (section 11 A).

The model itself, rendered through each view's camera (synth_render.py), is given to the pipeline as if it were the
photograph: MoGe-2 depth, the tie to triangulated points (the real solve's points of that view, moved onto the model so
that the pins lie where real features were found), the fusion with the traced parts, the registration and the
per-pixel comparison, all through holistic_diff.compare_view itself. Camera and geometry are exact and known, so:

    lift     how far the depth the pipeline ends with is from the true depth, by kind of surface
    ceiling  what the score gives a model that is exactly right
    floor    how much the score moves for models known to be wrong (sensitivity), on the synthetic and the real maps

    python tools/recon/audit/synth_audit.py mono | run | lift | sense

Out: out/recon/audit/synth/: mono/, diff/ (sheets, ref), synth.json, lift.json, sense.json.
"""
import json
import os
import subprocess
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402
from common import DF, V  # noqa: E402
import holistic_depth as D  # noqa: E402

SY = os.path.join(A.OUT, 'synth')
PHOTOS, MONO, DIFF = os.path.join(SY, 'photos'), os.path.join(SY, 'mono'), os.path.join(SY, 'diff')


def views():
    return sorted(f[:-4] for f in os.listdir(PHOTOS) if f.endswith('.png') and '_' not in f)


def true_camera(k):
    return V.Camera.from_dict(json.load(open(os.path.join(PHOTOS, k + '.json')))['camera'])


def mono():
    os.makedirs(MONO, exist_ok=True)
    photos = []
    for k in views():
        c = true_camera(k)
        photos.append({'id': k, 'image': os.path.join(PHOTOS, k + '.png'), 'fov_x_deg': float(np.degrees(2 * np.arctan(0.5 * c.w / c.f))), 'out': os.path.join(MONO, k + '.npz')})
    path = os.path.join(MONO, 'job.json')
    with open(path, 'w', encoding='utf-8') as f:
        json.dump({'size': D.MAP_SIZE, 'photos': photos}, f)
    env = dict(os.environ, HF_HOME=r'E:\SAM3D-tools\models\hf', HF_HUB_DISABLE_SYMLINKS_WARNING='1')
    subprocess.run([D.MONO_PYTHON, '-X', 'utf8', os.path.join(A.RECON, 'holistic_mono_infer.py'), path], check=True, env=env)


def cast_z(cam, scene, w, h, stride=1):
    """True z along the axis, the triangle and the world point per pixel of a pinhole camera at w x h."""
    c = cam.scaled(w, h)
    y, x = np.mgrid[0:h:stride, 0:w:stride]
    ray = DF.rays_of(c, x.ravel() + 0.5, y.ravel() + 0.5)
    nrm = np.linalg.norm(ray, axis=1)
    d = (c.R.T @ (ray / nrm[:, None]).T).T
    tri, t = scene.cast(c.centre(), d)
    return (t / nrm).reshape(y.shape), tri.reshape(y.shape)


def scene_for(k, cache):
    hide = tuple(sorted(json.load(open(os.path.join(PHOTOS, k + '.json')))['hide']))
    if hide not in cache:
        cache[hide] = DF.Scene(hide)
    return cache[hide]


def synthetic_points(k, cam, scene):
    """The real solve's points of this view, moved along their rays onto the model: pins where real features were found, with the true depth."""
    X = _REAL_POINTS(k)
    if not len(X):
        return X
    px = cam.project(X)
    ok = np.isfinite(px).all(1) & (px[:, 0] >= 0) & (px[:, 0] < cam.w - 1) & (px[:, 1] >= 0) & (px[:, 1] < cam.h - 1)
    px = px[ok]
    ray = DF.rays_of(cam, px[:, 0], px[:, 1])
    d = (cam.R.T @ (ray / np.linalg.norm(ray, axis=1, keepdims=True)).T).T
    tri, t = scene.cast(cam.centre(), d)
    hit = tri >= 0
    return cam.centre() + d[hit] * t[hit][:, None]


_REAL_POINTS = DF.view_points


def run():
    """holistic_diff.compare_view on the synthetic photographs."""
    cams = {k: true_camera(k) for k in views()}
    cache, pts, al = {}, {}, {}
    for k, c in cams.items():
        sc = scene_for(k, cache)
        pts[k] = synthetic_points(k, c, sc)
        d, _ = np.load(os.path.join(MONO, k + '.npz'))['depth'].astype(float), None
        h, w = d.shape
        cm = c.scaled(w, h)
        px, zs = cm.project(pts[k]), cm.cam(pts[k])[:, 2] if len(pts[k]) else np.zeros(0)
        if len(pts[k]):
            zm = d[np.clip(px[:, 1].astype(int), 0, h - 1), np.clip(px[:, 0].astype(int), 0, w - 1)]
            g = np.isfinite(zm) & (zm > 0.05)
            coef, mad = D.fit_scale(2.0 * px[g, 0] / w - 1.0, 2.0 * px[g, 1] / h - 1.0, zs[g] / zm[g])
        else:
            coef, mad = np.zeros(6), 0.0
        al[k] = {'coef': coef.tolist(), 'mad': mad, 'points': int(len(pts[k]))}
    # the pipeline's own comparison, pointed at the synthetic files
    DF.OUT, DF.SHEETS, D.MONO = DIFF, os.path.join(DIFF, 'sheets'), MONO
    DF.camera_for = lambda photo: cams[photo]
    DF.view_points = lambda photo: pts[photo]
    real_photo_file = V.photo_file
    V.photo_file = lambda photo, kind='clean': os.path.join(PHOTOS, photo + '.png') if os.path.exists(os.path.join(PHOTOS, photo + '.png')) else real_photo_file(photo, kind)
    cam_dicts = {k: c.to_dict() for k, c in cams.items()}
    out = {}
    for k in views():
        score, findings, _ = DF.compare_view(k, cache, cam_dicts, al)
        score.pop('camera', None)
        out[k] = {'score': score, 'findings': len(findings), 'findings_area': round(float(sum(f['share'] for f in findings)), 3), 'pins': al[k]['points']}
        print('%-7s agree %5.1f %% (all pixels %5.1f %%)  median %.3f m  used %s  findings %2d  %s' % (k, 100 * score['agree'], 100 * score.get('agree_all', score['agree']), score['median_abs_m'] or 0, score['used'], len(findings), score['register']), flush=True)
    with open(os.path.join(SY, 'synth.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)
    used = [v['score']['agree'] for v in out.values() if v['score']['used']]
    print('a model that is exactly right scores: mean agreement %.1f %% over %d used views (all %d views: %.1f %%)' % (100 * np.mean(used), len(used), len(out), 100 * np.mean([v['score']['agree'] for v in out.values()])))


def lift():
    """The depth the pipeline ended with against the true depth, per kind of surface."""
    cache, rows = {}, {}
    tot = {}
    sj = json.load(open(os.path.join(SY, 'synth.json')))
    for k in views():
        reg = sj[k]['score']['register']
        if reg.get('kept') != 'solve' and reg.get('eye_moved_m', 0) > 0.05:      # the registration replaced the true camera: the map is in another camera's frame, and says nothing about the lift
            print(k, 'left out: the registration moved the exactly right camera by %.2f m (focal length x %.2f)' % (reg['eye_moved_m'], reg['focal_ratio']))
            rows[k] = {'registration_moved_true_camera_m': reg['eye_moved_m'], 'focal_ratio': reg['focal_ratio']}
            continue
        ref = np.load(os.path.join(DIFF, 'ref', k + '.npz'))
        z_ref = ref['z'].astype(float)
        h, w = z_ref.shape
        cam = true_camera(k)
        sc = scene_for(k, cache)
        z_true, tri = cast_z(cam, sc, w, h)
        d = np.load(os.path.join(MONO, k + '.npz'))['depth'].astype(float)
        z_mono = cv2.resize(d, (w, h), interpolation=cv2.INTER_NEAREST)
        hit = tri >= 0
        ok = hit & np.isfinite(z_ref) & np.isfinite(z_true)
        traced = np.zeros((h, w), bool)
        names = np.array([n.startswith(DF.ANCHORS) for n in sc.names])
        traced[hit] = names[sc.part[tri[hit]]]
        gy, gx = np.gradient(np.where(hit, z_true, np.nan))
        step = np.hypot(np.nan_to_num(gx, nan=1.0), np.nan_to_num(gy, nan=1.0)) > 0.05          # a jump of 5 cm between neighbouring pixels: an edge of a part
        near_edge = cv2.dilate(step.astype(np.uint8), np.ones((13, 13), np.uint8)) > 0             # within 6 px of one
        s = np.nanmedian(z_true[ok] / z_mono[ok]) if ok.any() else 1.0
        e_one = np.abs(z_mono * s - z_true)                    # the network's map with one scale: its shape error
        e = np.abs(z_ref - z_true)
        cls = {'pinned (traced parts)': ok & traced, 'free, flat': ok & ~traced & ~near_edge, 'free, within 6 px of a part edge': ok & ~traced & near_edge}
        rows[k] = {'one_scale_median_m': round(float(np.median(e_one[ok])), 4)}
        for name, m in cls.items():
            if m.sum() > 200:
                rows[k][name] = {'share': round(float(m.sum() / ok.sum()), 3), 'median_m': round(float(np.median(e[m])), 4), 'p90_m': round(float(np.percentile(e[m], 90)), 4), 'within_band': round(float(np.mean(e[m] <= np.maximum(DF.BAND_M, DF.BAND_SHARE * z_true[m]))), 3)}
                tot.setdefault(name, []).append(e[m])
        tot.setdefault('one scale', []).append(e_one[ok])
        print(k, rows[k], flush=True)
    summary = {name: {'median_m': round(float(np.median(np.concatenate(v))), 4), 'p90_m': round(float(np.percentile(np.concatenate(v), 90)), 4)} for name, v in tot.items()}
    print('ALL', summary)
    with open(os.path.join(SY, 'lift.json'), 'w', encoding='utf-8') as f:
        json.dump({'views': rows, 'all': summary}, f, indent=1)


# ------------------------------------------------------------------------------------------------ sensitivity
class SoupScene(DF.Scene):
    """A scene from triangles in hand (a known-wrong variant of the model)."""

    def __init__(self, tri, part, names):
        import trimesh
        from trimesh.ray.ray_pyembree import RayMeshIntersector
        self.names, self.tri, self.part = list(names), tri, part
        n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        self.area = 0.5 * np.linalg.norm(n, axis=1)
        self.normal = n / np.maximum(2 * self.area[:, None], 1e-12)
        self.ray = RayMeshIntersector(trimesh.Trimesh(vertices=tri.reshape(-1, 3), faces=np.arange(len(tri) * 3).reshape(-1, 3), process=False))


def box_tris(c, u, v, n, size):
    """Twelve triangles of a box: centre, three unit axes, sizes along them."""
    c, u, v, n = (np.asarray(q, float) for q in (c, u, v, n))
    P = np.array([c + sx * 0.5 * size[0] * u + sy * 0.5 * size[1] * v + sz * 0.5 * size[2] * n for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])
    q = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    return np.array([[P[a], P[b], P[c_]] for a, b, c_, d in q] + [[P[a], P[c_], P[d]] for a, b, c_, d in q])


LEG_BOX = ((0.36, 0.80), (0.26, 0.50), (0.62, 1.50))      # the right post's leg, the cover behind it, the latch and bracket (data/cabins/osa_ak_shell.json posts.right)


def variants(base):
    """Models known to be wrong, each one a change the last session made or undid, or a part taken away."""
    tri0, part0, names = base.tri, base.part, base.names
    pid = {n: i for i, n in enumerate(names)}

    def of(*parts):
        return np.isin(part0, [pid[p] for p in parts])

    def moved(sel, fn):
        t = tri0.copy()
        t[sel] = fn(t[sel].reshape(-1, 3)).reshape(-1, 3, 3)
        return t, part0

    def in_box(box):
        lo, hi = np.array([b[0] for b in box]), np.array([b[1] for b in box])
        return (part0 == pid['hull']) & np.all((tri0 >= lo) & (tri0 <= hi), axis=(1, 2))
    out = {}
    seats = of('seat_operator', 'seat_senior', 'seat_deputy')
    out['seats 0.25 m too high (the bounding-box trap, section 38)'] = moved(seats, lambda p: p + [0, 0, 0.25])
    cab = of('cabinet_0096')
    cc = tri0[cab].reshape(-1, 3).mean(0)
    a = np.radians(-22.3)
    Rz = np.array([[np.cos(a), np.sin(a), 0], [-np.sin(a), np.cos(a), 0], [0, 0, 1.0]])
    out['0096 cabinet turned back to the plan\'s 38 degrees (section 33)'] = moved(cab, lambda p: (Rz @ (p - cc).T).T + cc)
    out['0096 cabinet 0.10 m too far forward'] = moved(cab, lambda p: p + [0, 0.10, 0])
    leg = in_box(LEG_BOX)
    out['right post\'s leg and cover raked 45 degrees, not 15 (section 40)'] = moved(leg, lambda p: p + np.stack([(np.tan(np.radians(45)) - np.tan(np.radians(15))) * (p[:, 2] - 1.48), 0 * p[:, 0], 0 * p[:, 0]], 1))
    out['right post\'s leg, cover and latch taken away'] = (tri0[~leg], part0[~leg])
    p0, p1 = np.array([0.705, 0.55, 1.9]), np.array([0.705, -0.35, 1.13])
    u = (p1 - p0) / np.linalg.norm(p1 - p0)
    strut = box_tris((p0 + p1) / 2, u, np.cross([1.0, 0, 0], u), [1.0, 0, 0], (np.linalg.norm(p1 - p0), 0.045, 0.010))
    out['the strut that is not there, put back (section 39)'] = (np.vstack([tri0, strut]), np.concatenate([part0, np.full(len(strut), pid['hull'])]))
    for name in ('bench', 'first_aid', 'chest_5937', 'ga3'):
        sel = of(name)
        out['%s taken away' % name] = (tri0[~sel], part0[~sel])
    out['the whole 0096 cabinet taken away'] = (tri0[~cab], part0[~cab])
    return {k: SoupScene(t.astype(float), p, names) for k, (t, p) in out.items()}, int(leg.sum())


def score_against(ref, scene, loose=()):
    """holistic_diff.compare_view's agreement of a scene with a stored reference map, through the reference's camera."""
    z_ref, n_ref = ref['z'].astype(float), ref['n'].astype(float)
    h, w = z_ref.shape
    cam = V.Camera(float(ref['f']), float(ref['cx']), float(ref['cy']), ref['R'], ref['t'], int(ref['w']), int(ref['h']), float(ref['k1']))
    y, x = np.mgrid[0:h, 0:w]
    ray = DF.rays_of(cam, x.astype(float) + 0.5, y.astype(float) + 0.5).reshape(-1, 3)
    nrm = np.linalg.norm(ray, axis=1, keepdims=True)
    dirs = (cam.R.T @ (ray / nrm).T).T
    tri, t = scene.cast(cam.centre(), dirs)
    z_mod = (t / nrm[:, 0]).reshape(h, w)
    tri = tri.reshape(h, w)
    hit = tri >= 0
    n_mod = np.full((h, w, 3), np.nan)
    nm = scene.normal[tri[hit]]
    nm[np.sum(nm * dirs[hit.ravel()], axis=1) > 0] *= -1
    n_mod[hit] = nm
    part = np.full((h, w), -1, int)
    part[hit] = scene.part[tri[hit]]
    valid = np.isfinite(z_ref) & hit
    for q in loose:
        m_ = np.zeros((h, w), np.uint8)
        cv2.fillPoly(m_, [np.array([[a * w, b * h] for a, b in q['poly']], np.int32)], 1)
        valid &= m_ == 0
    r = z_mod - z_ref
    tol = np.maximum(DF.BAND_M, DF.BAND_SHARE * np.nan_to_num(z_mod, nan=1.0))
    ang = np.degrees(np.arccos(np.abs(np.clip(np.nansum(n_ref * n_mod, axis=2), -1, 1))))
    agree = valid & (np.abs(r) <= tol) & ~(ang > DF.ANGLE_DEG)
    traced = np.array([n.startswith(DF.ANCHORS) for n in scene.names])
    free = valid & (part >= 0) & ~traced[np.maximum(part, 0)]
    n_valid = max(1, int(valid.sum()))
    if free.sum() >= 0.05 * n_valid:
        return float((agree & free).sum()) / int(free.sum())
    return float(agree.sum()) / n_valid


def sense():
    """How much the score moves for known-wrong models: on the real photos' maps and on the synthetic ones."""
    real_scores = V.load(os.path.join(os.path.dirname(A.REF), 'score.json'))['views']
    out = {}
    for label, refdir, vs in (('real photos', A.REF, [k for k, v in real_scores.items() if v['used']]), ('synthetic photos', os.path.join(DIFF, 'ref'), None)):
        if not os.path.isdir(refdir):
            continue
        if vs is None:
            sj = json.load(open(os.path.join(SY, 'synth.json')))
            vs = [k for k, v in sj.items() if v['score']['used']]
        refs = {k: dict(np.load(os.path.join(refdir, k + '.npz'))) for k in vs}
        views_meta = {k: (V.load(V.view_path(k)) if os.path.exists(V.view_path(k)) else {}) for k in vs}
        bases = {}
        table = {}

        def run_variant(make):
            sc = {}
            for k in vs:
                hide = tuple(sorted(views_meta[k].get('state', {}).get('hide', [])))
                if hide not in bases:
                    bases[hide] = DF.Scene(hide)
                sc[k] = score_against(refs[k], make(bases[hide], hide), views_meta[k].get('loose', []))
            return sc
        base = run_variant(lambda b, hide: b)
        if label == 'real photos':
            print('check: the stored scores are reproduced within %.4f' % max(abs(base[k] - real_scores[k]['agree']) for k in vs))
        var_cache = {}

        def variant_scene(b, hide, name):
            if hide not in var_cache:
                var_cache[hide] = variants(b)[0]
            return var_cache[hide][name]
        names = list(variants(DF.Scene())[0])
        print('\n%s: %d views, mean agreement %.2f %%' % (label, len(vs), 100 * np.mean(list(base.values()))))
        for name in names:
            sc = run_variant(lambda b, hide: variant_scene(b, hide, name))
            dv = {k: sc[k] - base[k] for k in vs}
            worst = min(dv, key=dv.get)
            best = max(dv, key=dv.get)
            table[name] = {'mean_change_pct': round(100 * float(np.mean(list(dv.values()))), 2), 'views_down': int(sum(v < -0.002 for v in dv.values())), 'views_up': int(sum(v > 0.002 for v in dv.values())),
                           'largest_fall': [worst, round(100 * dv[worst], 2)], 'largest_rise': [best, round(100 * dv[best], 2)]}
            print('  %-66s mean %+5.2f %%   down in %2d views, up in %2d   largest fall %s %+5.2f, largest rise %s %+5.2f' % (name, table[name]['mean_change_pct'], table[name]['views_down'], table[name]['views_up'], worst, 100 * dv[worst], best, 100 * dv[best]), flush=True)
        out[label] = {'base_mean_pct': round(100 * float(np.mean(list(base.values()))), 2), 'views': len(vs), 'variants': table}
    with open(os.path.join(SY, 'sense.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)


if __name__ == '__main__':
    {'mono': mono, 'run': run, 'lift': lift, 'sense': sense}[sys.argv[1]]()
