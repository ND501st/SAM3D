"""What is wrong with the model, and what to change: the holistic layer's comparison that draws conclusions (docs/OSA_INTERIOR_HOLISTIC.md section 32).

Every photo with a camera is compared with the model from that camera, pixel by pixel: the photo's depth map (holistic_depth.py) against the model's depth, the photo's
surface normals against the model's. Where they disagree the pixels are grouped into findings: which part, what kind of error, how big, in which direction, and what change it
implies. Findings seen from several cameras are merged. Each movable part's pose is also solved against the photos' surfaces.

    E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8 tools/recon/holistic_diff.py all          # scene, views, findings, poses, surfaces, report
    ... holistic_diff.py scene                                                                     # the built cabin as triangles (Blender)
    ... holistic_diff.py views [photo ...]                                                         # per-view maps, sheets and findings
    ... holistic_diff.py findings                                                                  # merge across views, report
    ... holistic_diff.py poses                                                                     # each movable part against the photos
    ... holistic_diff.py surfaces                                                                  # floor, roof and walls as maps, photos beside model
    ... holistic_diff.py score                                                                     # the scores, against the baseline
    ... holistic_diff.py measure g096-a --poly .38,.34,.65,.34,.65,.75,.38,.75                     # a region of a registered photo in metres: its plane, yaw, tilt, extent
    ... holistic_diff.py render g096-a g092-a                                                       # the model rendered through each registered camera, beside the photo: renders/<photo>.jpg
    ... holistic_diff.py baseline                                                                  # keep this run's scores as what later edits are measured against

Out: out/recon/holistic/diff/: sheets/<photo>.jpg (photo, reference depth, model depth, difference, numbered findings), findings.json, poses.json, surfaces/, score.json.
The written report is docs/research/OSA_INTERIOR_DIFF_2026-11.md. What a photo shows that is not the vehicle as built (loose bags, cloth) is listed in the view's trace as
"loose" polygons, each with its why, and is not compared.
"""
import argparse
import json
import os
import subprocess
import sys
import time

import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import holistic_depth as D  # noqa: E402
import holistic_metric as M  # noqa: E402
import holistic_view as V  # noqa: E402

OUT = os.path.join(V.REPO, 'out', 'recon', 'holistic', 'diff')
SHEETS = os.path.join(OUT, 'sheets')
SCENE_NPZ = os.path.join(OUT, 'scene.npz')
REPORT = os.path.join(V.REPO, 'docs', 'research', 'OSA_INTERIOR_DIFF_2026-11.md')
LONG_SIDE = 800           # the comparison's working size
BAND_M, BAND_SHARE = 0.04, 0.025     # depths agree within the larger of 4 cm and 2.5 % of the range (about what a depth network can tell)
ANGLE_DEG = 30.0          # surfaces disagree in direction beyond this
MIN_AREA = 0.003          # a finding covers at least this share of the image
MIN_ON_MODEL = 0.4        # a view whose traced parts' pixels land on the model less than this is not registered: its sheet is written, its findings are not used
MAX_HOLDOUT_M = 0.05       # a view is used only if its fused map meets the pins held back from it this well (holistic_metric.py)
MIN_AGREE_UNANCHORED = 0.3     # a view seeing no traced part has only the solve's camera to go by: used only if it agrees at least this much
CELL = 0.10               # findings from different views overlap on a grid of this size
FLOORLIKE = ('hull',)


def camera_for(photo):
    """The camera comparisons use: the one the photo's depth map was tied to (the adjusted solve). A photo whose view measures its own camera (g082-b) is compared through that."""
    vp = V.view_path(photo)
    cj = os.path.join(V.OUT, photo, 'camera.json')
    if os.path.exists(vp) and os.path.exists(cj) and V.load(vp).get('camera_from') == 'measured':
        return V.Camera.from_dict(V.load(cj)['camera'])
    cams = V.load(V.SFM_CAMERAS)['cameras']
    return V.Camera.from_dict(cams[photo]) if photo in cams else None


def rays_of(cam, x, y):
    """Unit-z camera rays of pixels, with the lens term taken out (one fixed-point pass is enough for these small k1)."""
    nx, ny = (x - cam.cx) / cam.f, (y - cam.cy) / cam.f
    if cam.k1:
        r2 = nx * nx + ny * ny
        s = 1.0 / (1.0 + cam.k1 * r2)
        nx, ny = nx * s, ny * s
    return np.stack([nx, ny, np.ones_like(nx)], axis=-1)


def scene_job():
    """Ask Blender for the built cabin as triangles with each triangle's part."""
    os.makedirs(OUT, exist_ok=True)
    job = {'repo': V.REPO, 'export_triangles': SCENE_NPZ.replace('.npz', '.json'), 'camera': None, 'out': os.path.join(OUT, 'scene_'), 'passes': [], 'rays': []}
    path = os.path.join(OUT, 'scene_job.json')
    V.save(path, job)
    r = subprocess.run([V.BLENDER, '--background', '--factory-startup', '--python', V.SCENE, '--', path], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0 or not os.path.exists(job['export_triangles'] + '.bin'):
        print(r.stdout[-1500:], r.stderr[-1500:])
        raise SystemExit('the scene did not export')
    meta = V.load(job['export_triangles'])
    tri = np.fromfile(job['export_triangles'] + '.bin', dtype=np.float32).reshape(-1, 3, 3)
    part = np.fromfile(job['export_triangles'] + '.part', dtype=np.int32)
    np.savez_compressed(SCENE_NPZ, tri=tri, part=part, names=np.array(meta['parts']))
    print('scene: %d triangles, %d parts' % (len(tri), len(meta['parts'])))


class Scene:
    def __init__(self, hide=()):
        import trimesh
        from trimesh.ray.ray_pyembree import RayMeshIntersector
        z = np.load(SCENE_NPZ)
        self.names = [str(n) for n in z['names']]
        tri, part = z['tri'].astype(float), z['part']
        if hide:
            keep = np.array([self.names[p] not in hide for p in part])
            tri, part = tri[keep], part[keep]
        self.tri, self.part = tri, part
        e1, e2 = tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]
        n = np.cross(e1, e2)
        self.area = 0.5 * np.linalg.norm(n, axis=1)
        self.normal = n / np.maximum(2 * self.area[:, None], 1e-12)
        m = trimesh.Trimesh(vertices=tri.reshape(-1, 3), faces=np.arange(len(tri) * 3).reshape(-1, 3), process=False)
        self.ray = RayMeshIntersector(m)

    def cast(self, origin, dirs):
        """First hits: triangle index (-1 none) and distance along unit dirs."""
        o = np.broadcast_to(origin, dirs.shape).copy()
        idx = self.ray.intersects_first(o, dirs)
        t = np.full(len(dirs), np.nan)
        k = idx >= 0
        tr = self.tri[idx[k]]
        n = self.normal[idx[k]]
        t[k] = np.sum((tr[:, 0] - o[k]) * n, axis=1) / np.sum(dirs[k] * n, axis=1)
        return idx, t


def rotvec_matrix(w):
    """Rotation matrix of a rotation vector."""
    th = float(np.linalg.norm(w))
    if th < 1e-12:
        return np.eye(3)
    k = w / th
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + np.sin(th) * K + (1 - np.cos(th)) * K @ K


def camera_moved(cam, w, v, dlogf):
    """The camera turned by w about its own eye, its eye moved by v (world), its focal length scaled."""
    R = cam.R @ rotvec_matrix(w).T
    eye = cam.centre() + v
    return V.Camera(cam.f * np.exp(dlogf), cam.cx, cam.cy, R, -R @ eye, cam.w, cam.h, cam.k1)


ANCHORS = ('sp2m', 'sp3m', 'sl1m', 'sl2m', 'sl3m', 'sp1m', 'rack_', 'r124', 'r123', 'op_panel')      # the parts traced in millimetres: only these place a camera
FOCAL_GRID = (0.87, 0.94, 1.0, 1.07)           # of the solve's focal length: a lens the map, the scale and the distance cannot tell apart alone. The bundle adjustment holds it to 6 per cent; 0.8 was never right
MAX_EYE_MOVE_M = 0.30                          # a registration corrects the solve, it does not replace it: moved further than this from the solve's eye (the lens's shift along the axis included) or
MAX_TURN_DEG = 6.0                             # turned further than this, the ICP has found another fit (g092-a went 0.56 m and 20 degrees, and read as a wrong model) and the solve's camera stands


def icp_view(cam, z_ref, scene, valid, stride, iters, s0=1.0, anchored=None):
    """Projective ICP: the camera's place and turn and the map's scale so that the map lies on the model where it can, at a fixed focal length. Returns camera, scale, share of pixels on the model within 4 cm."""
    h, w = z_ref.shape
    y, x = np.mgrid[0:h:stride, 0:w:stride]
    ok = np.isfinite(z_ref[y, x]) & (valid[y, x] if valid is not None else True)
    x, y = x[ok].astype(float) + 0.5, y[ok].astype(float) + 0.5
    z = z_ref[y.astype(int), x.astype(int)]
    s, on, total = s0, 0.0, np.zeros(7)
    prior = np.array([1 / 0.035 ** 2] * 3 + [1 / 0.08 ** 2] * 3 + [1 / 0.05 ** 2])     # 2 degrees, 8 cm, 5 per cent: a registration, not a new solve
    for it in range(iters + 1):
        c = rays_of(cam, x, y)
        nrm = np.linalg.norm(c, axis=1)
        d = (cam.R.T @ (c / nrm[:, None]).T).T
        eye = cam.centre()
        tri, t = scene.cast(eye, d)
        hit = tri >= 0
        if anchored is not None:
            hit[hit] = anchored[scene.part[tri[hit]]]
        if hit.sum() < 200:
            return cam, s, 0.0
        q = (z[hit] * s * nrm[hit])[:, None] * d[hit]
        N = scene.normal[tri[hit]]
        e = np.sum(N * (d[hit] * t[hit][:, None] - q), axis=1)
        on = float(np.mean(np.abs(e) < 0.04))
        if it == iters:
            break
        keep = np.abs(e) < max(0.04, 0.15 * 0.85 ** it)
        if keep.sum() < 200:
            return cam, s, 0.0
        q_, N_, e_ = q[keep], N[keep], e[keep]
        J = np.column_stack([np.cross(q_, N_), N_, np.sum(N_ * q_, axis=1)])
        wgt = 1.0 / np.maximum(np.abs(e_) / 0.02, 1.0)
        indep = 1.0 / (0.03 ** 2 * max(1.0, len(e_) / 400.0))                       # the map's errors are smooth: a few hundred independent pixels at most
        H = indep * ((J * wgt[:, None]).T @ J)
        g = indep * ((J * wgt[:, None]).T @ e_) - prior * total
        dx = np.linalg.solve(H + np.diag(prior) + 1e-3 * np.diag(np.diag(H) + 1e-9), g)
        total += dx
        cam = camera_moved(cam, dx[:3], dx[3:6], 0.0)
        s = float(np.clip(s * (1 + float(dx[6])), s0 * 0.92, s0 * 1.08))        # a map's scale is known to a few per cent; left free it collapses the map onto one wall
    return cam, s, on


def within_limits(c, solve):
    """Is a registered camera still a correction of the solve's?"""
    moved = float(np.linalg.norm(c.centre() - solve.centre()))
    turned = float(np.degrees(np.arccos(np.clip((np.trace(c.R @ solve.R.T) - 1) / 2, -1, 1))))
    return moved <= MAX_EYE_MOVE_M and turned <= MAX_TURN_DEG


def register_view(cam, z_ref, scene, valid=None):
    """The photo's last registration to the model. For each focal length of a grid, the eye starts as far forward as keeps the scene's size in the image, then a trimmed ICP moves camera and map scale.
    The lens that puts the most of the map on the model wins: the model knows the near and far surfaces in metres, so its perspective settles what the map's scale cannot. A wrong part stays an outlier and shows in the difference."""
    med = float(np.nanmedian(z_ref))
    axis = cam.R[2]
    best = None
    anchored = np.array([n.startswith(ANCHORS) for n in scene.names])
    from_part = scene.cast(cam.centre(), (cam.R.T @ (lambda c: c / np.linalg.norm(c, axis=1, keepdims=True))(rays_of(cam, *[a.ravel().astype(float) for a in np.mgrid[0:cam.w:8, 0:cam.h:8]])).T).T)[0]
    share = float(np.mean(anchored[scene.part[from_part[from_part >= 0]]])) if (from_part >= 0).any() else 0.0
    if share < 0.08:                                                                   # too little to go by: the solve's camera stands
        return cam, z_ref, {'on_model': None, 'skipped': 'share of traced parts %.2f' % share}
    for ratio in FOCAL_GRID:
        eye0 = cam.centre() + axis * (1 - ratio) * med
        z = np.maximum(z_ref - (1 - ratio) * med, 0.1)                                 # the eye moved up the axis: every surface that much nearer
        c0 = V.Camera(cam.f * ratio, cam.cx, cam.cy, cam.R, -cam.R @ eye0, cam.w, cam.h, cam.k1)
        c, s, on = icp_view(c0, z, scene, valid, 5, 16, anchored=anchored)
        if within_limits(c, cam) and (best is None or on > best[2]):
            best = (c, s, on, ratio, z)
    if best is None:
        return cam, z_ref, {'on_model': None, 'skipped': 'no registration within the limits'}
    c, s, _, ratio, z = best
    c1, s1, on = icp_view(c, z, scene, valid, 2, 12, s, anchored=anchored)
    if within_limits(c1, cam):
        c, s = c1, s1
    else:
        return cam, z_ref, {'on_model': None, 'skipped': 'no registration within the limits'}
    moved = float(np.linalg.norm(c.centre() - cam.centre()))
    turned = float(np.degrees(np.arccos(np.clip((np.trace(c.R @ cam.R.T) - 1) / 2, -1, 1))))
    return c, z * s, {'on_model': round(float(on), 3), 'focal_ratio': ratio, 'eye_moved_m': round(moved, 3), 'turned_deg': round(turned, 2), 'map_scale': round(float(s), 3)}


def fit_plane(P):
    """Centroid, unit normal, and rms off the plane."""
    c = np.median(P, axis=0)
    u, s, vt = np.linalg.svd(P - c, full_matrices=False)
    return c, vt[2], float(s[2] / np.sqrt(len(P)))


def angle_between(a, b):
    return float(np.degrees(np.arccos(np.clip(abs(np.dot(a, b)), 0.0, 1.0))))


def classify(s):
    """A finding's kind and its conclusion in words, from its measured quantities."""
    r, ang, d, area = s['median_r_m'], s['plane_angle_deg'], s['along_normal_m'], s['share']
    part, hull = s['part'], s['part'] in FLOORLIKE
    pos = 'x %.2f y %.2f z %.2f' % tuple(s['photo_centre_m'])
    planar_both = s['photo_rms_m'] < 0.035 and s['model_rms_m'] < 0.035
    n = s['photo_normal']
    axis = 'floor-like' if abs(n[2]) > 0.8 else ('wall facing along x' if abs(n[0]) > 0.8 else ('wall facing along y' if abs(n[1]) > 0.8 else 'sloped'))
    if planar_both and ang >= 12.0:
        return 'turned', '%s: the surface is at the wrong angle by %.0f degrees at %s (photo: a %s plane). Yaw difference %+.0f, tilt difference %+.0f.' % (part, ang, pos, axis, s['yaw_delta_deg'], s['tilt_delta_deg'])
    if r > 0 and hull and abs(n[2]) > 0.7 and s['photo_centre_m'][2] > s['model_centre_m'][2]:
        return 'raise', 'hull: the floor here is higher in the photos by %.2f m at %s: a raised surface the model does not have.' % (s['photo_centre_m'][2] - s['model_centre_m'][2], pos)
    if r > 0 and s['surface_gap_m'] > 0.08:
        return 'missing', '%s: something stands %.2f m in front of the model here at %s: geometry the model lacks (photo: a %s surface).' % (part, r, pos, axis)
    if r < 0 and s['surface_gap_m'] > 0.08:
        return 'excess', '%s: the model stands %.2f m in front of what the photo sees at %s: too big or in the wrong place.' % (part, -r, pos)
    if abs(d) >= 0.02:
        v = np.array(s['model_normal']) * d
        return 'moved', '%s: the surface should move %.3f m along its normal at %s (x %+.3f, y %+.3f, z %+.3f).' % (part, abs(d), pos, v[0], v[1], v[2])
    return 'shape', '%s: the surface differs in shape at %s (median %+.3f m along the ray).' % (part, pos, r)


_VIEW_POINTS = {}


def view_points(photo):
    """The joint solve's triangulated points this photo saw."""
    if not _VIEW_POINTS:
        for x, y, z, ph in V.load(D.POINTS):
            for k in ph:
                _VIEW_POINTS.setdefault(k, []).append([x, y, z])
    return np.array(_VIEW_POINTS.get(photo, []), float).reshape(-1, 3)


def better_camera(solve, registered):
    """Which camera's fused map meets its withheld pins better; the solve's unless the registration clearly wins."""
    a, b = solve.get('holdout_after_m'), registered.get('holdout_after_m')
    if b is None:
        return 'solve'
    if a is None:
        return 'registered'
    return 'registered' if b < a * 0.95 else 'solve'


def compare_view(photo, scene_cache, cams_for_alignment, al):
    """One photo. Returns its score and findings; writes its sheet."""
    import cv2
    z_ref, n_ref_cam, cam_solve = D.aligned(photo, cams_for_alignment, al)
    h0, w0 = z_ref.shape
    s = LONG_SIDE / max(w0, h0)
    w, h = round(w0 * s), round(h0 * s)
    cam = camera_for(photo)
    if cam is None:
        return None
    cam = cam.scaled(w, h)
    z_ref = cv2.resize(z_ref, (w, h), interpolation=cv2.INTER_NEAREST)
    n_ref_cam = cv2.resize(n_ref_cam, (w, h), interpolation=cv2.INTER_NEAREST)
    view = V.load(V.view_path(photo)) if os.path.exists(V.view_path(photo)) else {}
    hide = tuple(sorted(view.get('state', {}).get('hide', [])))
    if hide not in scene_cache:
        scene_cache[hide] = Scene(hide)
    scene = scene_cache[hide]
    keep_px = np.ones((h, w), bool)
    for q in view.get('loose', []):
        m_ = np.zeros((h, w), np.uint8)
        cv2.fillPoly(m_, [np.array([[a * w, b * h] for a, b in q['poly']], np.int32)], 1)
        keep_px &= m_ == 0
    traced = np.array([n.startswith(ANCHORS) for n in scene.names])

    def fuse_on(c, z):
        return M.fuse_map(z, M.point_pins(z, c, view_points(photo)), M.trace_pins(z, c, scene, traced, rays_of, keep_px), seed=zlib.crc32(photo.encode()))

    solve_cam, solve_z = cam, z_ref
    z_solve, fused_solve = fuse_on(solve_cam, solve_z)
    cam, z_ref, reg = register_view(cam, z_ref, scene, keep_px)
    z_ref, fused = fuse_on(cam, z_ref)
    if better_camera(fused_solve, fused) == 'solve':                                 # the pins held back decide between the solve's camera and the registered one
        cam, z_ref, fused = solve_cam, z_solve, fused_solve
        reg = dict(reg, kept='solve')
    y, x = np.mgrid[0:h, 0:w]
    ray_c = rays_of(cam, x.astype(float) + 0.5, y.astype(float) + 0.5).reshape(-1, 3)
    norm = np.linalg.norm(ray_c, axis=1, keepdims=True)
    dirs = (cam.R.T @ (ray_c / norm).T).T
    eye = cam.centre()
    tri, t = scene.cast(eye, dirs)
    z_mod = (t / norm[:, 0]).reshape(h, w)
    tri = tri.reshape(h, w)
    n_mod = np.full((h, w, 3), np.nan)
    hit = tri >= 0
    nm = scene.normal[tri[hit]]
    flip = np.sum(nm * dirs[hit.ravel()], axis=1) > 0
    nm[flip] *= -1
    n_mod[hit] = nm
    part = np.full((h, w), -1, int)
    part[hit] = scene.part[tri[hit]]
    n_ref = (cam.R.T @ n_ref_cam.reshape(-1, 3).T).T.reshape(h, w, 3)                # MoGe's normals: OpenCV's camera axes, facing the viewer
    valid = np.isfinite(z_ref) & hit
    loose = np.zeros((h, w), np.uint8)
    for q in view.get('loose', []):
        cv2.fillPoly(loose, [np.array([[a * w, b * h] for a, b in q['poly']], np.int32)], 1)
    valid &= loose == 0
    r = z_mod - z_ref
    tol = np.maximum(BAND_M, BAND_SHARE * np.nan_to_num(z_mod, nan=1.0))
    cosang = np.clip(np.nansum(n_ref * n_mod, axis=2), -1, 1)
    ang = np.degrees(np.arccos(np.abs(cosang)))
    near, far = valid & (r < -tol), valid & (r > tol)
    tilted = valid & (np.abs(r) <= tol) & (ang > ANGLE_DEG)
    agree = valid & ~near & ~far & ~tilted
    n_valid = max(1, int(valid.sum()))
    score = {'photo': photo, 'register': reg, 'camera': {'f': float(cam.f), 'R': cam.R.tolist(), 'eye': cam.centre().tolist()}, 'pixels': n_valid, 'agree': round(float(agree.sum()) / n_valid, 4), 'median_abs_m': round(float(np.median(np.abs(r[valid]))), 4) if valid.any() else None,
             'median_angle_deg': round(float(np.nanmedian(ang[valid])), 1) if valid.any() else None}
    free = valid & (part >= 0) & ~traced[np.maximum(part, 0)]                        # the traced parts' pixels are pinned to the model, so they say nothing about it
    if free.sum() >= 0.05 * n_valid:
        score['agree_all'], score['agree'] = score['agree'], round(float((agree & free).sum()) / int(free.sum()), 4)
        score['median_abs_m'] = round(float(np.median(np.abs(r[free]))), 4)
    score['fuse'] = fused
    findings = []
    X_ref = eye + (dirs * norm * z_ref.reshape(-1, 1)).reshape(h, w, 3)
    X_mod = eye + (dirs * norm * z_mod.reshape(-1, 1)).reshape(h, w, 3)
    kernel = np.ones((3, 3), np.uint8)
    canvas_labels = np.zeros((h, w), int)
    for mask in (far, near, tilted):
        m = cv2.morphologyEx(mask.astype(np.uint8), cv2.MORPH_OPEN, kernel)
        count, lab = cv2.connectedComponents(m, connectivity=8)
        for c in range(1, count):
            comp = lab == c
            if comp.sum() < MIN_AREA * w * h:
                continue
            ids, cnt = np.unique(part[comp], return_counts=True)
            for pid, nn in zip(ids, cnt):             # one finding per part it falls on
                if pid < 0 or nn < MIN_AREA * w * h:
                    continue
                sel = comp & (part == pid)
                Pr, Pm = X_ref[sel], X_mod[sel]
                if len(Pr) > 4000:
                    i = np.random.default_rng(0).choice(len(Pr), 4000, replace=False)
                    Pr, Pm = Pr[i], Pm[i]
                cr, nr_, rr = fit_plane(Pr)
                cm, nm_, rm = fit_plane(Pm)
                if np.dot(nr_, eye - cr) < 0:
                    nr_ = -nr_
                if np.dot(nm_, eye - cm) < 0:
                    nm_ = -nm_
                d = float(np.median(np.sum((Pr - Pm) * nm_, axis=1)))
                yr, ym = np.arctan2(nr_[1], nr_[0]), np.arctan2(nm_[1], nm_[0])
                yaw = float(np.degrees((yr - ym + np.pi) % (2 * np.pi) - np.pi))
                tilt = float(np.degrees(np.arcsin(np.clip(nr_[2], -1, 1)) - np.arcsin(np.clip(nm_[2], -1, 1))))
                f = {'photo': photo, 'part': scene.names[pid], 'pixels': int(sel.sum()), 'share': round(float(sel.sum()) / (w * h), 4), 'median_r_m': round(float(np.median(r[sel])), 3),
                     'photo_centre_m': [round(float(v), 3) for v in cr], 'model_centre_m': [round(float(v), 3) for v in cm], 'photo_normal': [round(float(v), 3) for v in nr_],
                     'model_normal': [round(float(v), 3) for v in nm_], 'photo_rms_m': round(rr, 4), 'model_rms_m': round(rm, 4), 'plane_angle_deg': round(angle_between(nr_, nm_), 1),
                     'yaw_delta_deg': round(yaw, 1), 'tilt_delta_deg': round(tilt, 1), 'along_normal_m': round(d, 3), 'surface_gap_m': round(float(np.median(np.abs(np.sum((Pr - Pm) * nm_, axis=1)))), 3),
                     'bbox_photo_m': [[round(float(v), 3) for v in np.percentile(Pr, 5, axis=0)], [round(float(v), 3) for v in np.percentile(Pr, 95, axis=0)]],
                     'cells': sorted({tuple(q) for q in np.floor(Pr / CELL).astype(int).tolist()})}
                f['kind'], f['says'] = classify(f)
                ys, xs = np.nonzero(sel)
                f['px'] = [int(np.mean(xs)), int(np.mean(ys))]
                f['n'] = len(findings) + 1
                findings.append(f)
                canvas_labels[sel] = f['n']
    findings.sort(key=lambda q: -q['pixels'])
    draw_sheet(photo, w, h, z_ref, z_mod, r, tol, valid, ang, canvas_labels, findings, score)
    on = reg.get('on_model')
    held = fused.get('holdout_after_m')
    score['used'] = bool(held is not None and held <= max(MAX_HOLDOUT_M, BAND_SHARE * float(np.nanmedian(z_ref))))     # the agreement band's own rule: the larger of a fixed distance and a share of the range
    os.makedirs(os.path.join(OUT, 'ref'), exist_ok=True)
    np.savez_compressed(os.path.join(OUT, 'ref', photo + '.npz'), z=z_ref.astype(np.float32), f=cam.f, cx=cam.cx, cy=cam.cy, R=cam.R, t=cam.t, w=w, h=h, k1=cam.k1, n=n_ref.astype(np.float32))
    return score, findings, {'X': X_ref[valid][::3].astype(np.float32), 'N': n_ref[valid][::3].astype(np.float32), 'Xm': X_mod[valid][::3].astype(np.float32), 'Nm': n_mod[valid][::3].astype(np.float32)}


def draw_sheet(photo, w, h, z_ref, z_mod, r, tol, valid, ang, labels, findings, score):
    import cv2
    im = cv2.resize(cv2.imread(V.photo_file(photo, 'clean')), (w, h), interpolation=cv2.INTER_AREA)
    v = np.concatenate([z_ref[np.isfinite(z_ref)], z_mod[np.isfinite(z_mod)]])
    lo, hi = float(np.percentile(v, 2)), float(np.percentile(v, 98))
    a, b = D.turbo(z_ref, lo, hi), D.turbo(z_mod, lo, hi)
    c = D.diverging(np.where(valid, r, np.nan), 0.20)
    d = cv2.addWeighted(im, 0.55, np.zeros_like(im), 0.0, 0)
    colours = {'turned': (0, 165, 255), 'raise': (255, 80, 200), 'missing': (255, 120, 0), 'excess': (0, 0, 255), 'moved': (0, 220, 220), 'shape': (180, 180, 180)}
    for f in findings:
        m = (labels == f['n']).astype(np.uint8)
        cnts, _ = cv2.findContours(m, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        cv2.drawContours(d, cnts, -1, colours[f['kind']], 2)
        cv2.putText(d, str(f['n']), tuple(f['px']), 0, 0.6, (0, 0, 0), 3, cv2.LINE_AA)
        cv2.putText(d, str(f['n']), tuple(f['px']), 0, 0.6, (255, 255, 255), 1, cv2.LINE_AA)
    sheet = np.vstack([np.hstack([im, a]), np.hstack([b, c]), np.hstack([d, np.full_like(d, 30)])])
    x0, y0 = w + 10, 2 * h + 22
    cv2.putText(sheet, '%s  agree %.0f %%  median %.3f m' % (photo, 100 * score['agree'], score['median_abs_m'] or 0), (x0, y0), 0, 0.55, (255, 255, 255), 1, cv2.LINE_AA)
    for i, f in enumerate(findings[:int((h - 40) / 18)]):
        cv2.putText(sheet, ('%d %s %s' % (f['n'], f['kind'], f['says']))[:int(w / 7.2)], (x0, y0 + 20 + 18 * i), 0, 0.4, colours[f['kind']], 1, cv2.LINE_AA)
    os.makedirs(SHEETS, exist_ok=True)
    cv2.imwrite(os.path.join(SHEETS, photo + '.jpg'), sheet, [cv2.IMWRITE_JPEG_QUALITY, 86])


def merge_findings(found):
    """Findings of the same kind on the same part sharing grid cells are one finding seen from several cameras."""
    n = len(found)
    parent = list(range(n))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    sets = [set(map(tuple, f['cells'])) for f in found]
    for i in range(n):
        for j in range(i + 1, n):
            if found[i]['part'] == found[j]['part'] and found[i]['kind'] == found[j]['kind'] and found[i]['photo'] != found[j]['photo']:
                common = len(sets[i] & sets[j])
                if common >= 0.2 * min(len(sets[i]), len(sets[j])) and common >= 3:
                    parent[root(i)] = root(j)
    groups = {}
    for i in range(n):
        groups.setdefault(root(i), []).append(found[i])
    out = []
    for g in groups.values():
        g.sort(key=lambda q: -q['pixels'])
        lead = g[0]
        views = sorted({q['photo'] for q in g})
        out.append({'kind': lead['kind'], 'part': lead['part'], 'says': lead['says'], 'views': views, 'n_views': len(views), 'pixels': int(sum(q['pixels'] for q in g)),
                    'median_r_m': round(float(np.median([q['median_r_m'] for q in g])), 3), 'along_normal_m': round(float(np.median([q['along_normal_m'] for q in g])), 3),
                    'plane_angle_deg': round(float(np.median([q['plane_angle_deg'] for q in g])), 1), 'photo_centre_m': [round(float(v), 3) for v in np.median([q['photo_centre_m'] for q in g], axis=0)],
                    'photo_normal': lead['photo_normal'], 'model_normal': lead['model_normal'], 'bbox_photo_m': lead['bbox_photo_m'], 'lead_view': lead['photo'], 'lead_n': lead['n']})
    out.sort(key=lambda q: -(q['pixels'] * (1 + 0.5 * (q['n_views'] - 1))))
    for i, f in enumerate(out):
        f['id'] = 'F%03d' % (i + 1)
    return out


def sample_part(tri, area, n, rng):
    """Points and normals sampled on a part's triangles, by area."""
    p = area / area.sum()
    i = rng.choice(len(tri), n, p=p)
    u, v = rng.random(n), rng.random(n)
    f = u + v > 1
    u[f], v[f] = 1 - u[f], 1 - v[f]
    t = tri[i]
    return t[:, 0] + (t[:, 1] - t[:, 0]) * u[:, None] + (t[:, 2] - t[:, 0]) * v[:, None], i


def solve_pose(M, N, cloud, start_yaw, iters=30, trim=0.08):
    """Trimmed point-to-plane ICP: a yaw about the part's centre and a translation taking the part's surface onto the photos'. Returns yaw (deg), t, inliers, rms."""
    from scipy.spatial import cKDTree
    tree = cKDTree(cloud)
    c = M.mean(axis=0)
    yaw, t = np.radians(start_yaw), np.zeros(3)
    inl, rms = 0, 1.0
    for it in range(iters):
        ca, sa = np.cos(yaw), np.sin(yaw)
        Rz = np.array([[ca, -sa, 0], [sa, ca, 0], [0, 0, 1.0]])
        P = (Rz @ (M - c).T).T + c + t
        Nn = (Rz @ N.T).T
        d, j = tree.query(P)
        lim = max(trim, 0.25 * 0.85 ** it)
        k = d < lim
        inl = int(k.sum())
        if inl < 30:
            break
        q = cloud[j[k]]
        e = np.sum((q - P[k]) * Nn[k], axis=1)
        rms = float(np.sqrt(np.mean(e * e)))
        a = np.cross(P[k] - c - t, Nn[k])[:, 2]
        A = np.column_stack([a, Nn[k]])
        dx = np.linalg.lstsq(A.T @ A + 1e-6 * np.eye(4), A.T @ e, rcond=None)[0]
        yaw += float(dx[0])
        t += dx[1:]
    return float(np.degrees(yaw)), t, inl, rms


def solve_part_poses(scene, cloud_by_part, cloud_all):
    rng = np.random.default_rng(1)
    out = []
    for pid, name in enumerate(scene.names):
        k = scene.part == pid
        if name in FLOORLIKE or k.sum() < 12:
            continue
        a = scene.area[k]
        if a.sum() < 0.05:
            continue
        M, i = sample_part(scene.tri[k], a, 3000, rng)
        N = scene.normal[k][i]
        lo, hi = M.min(axis=0) - 0.35, M.max(axis=0) + 0.35
        C = cloud_all[np.all((cloud_all >= lo) & (cloud_all <= hi), axis=1)]
        if len(C) < 300:
            continue
        if len(C) > 60000:
            C = C[rng.choice(len(C), 60000, replace=False)]
        best = None
        base_yaw, base_t, base_in, base_rms = 0.0, np.zeros(3), None, None
        from scipy.spatial import cKDTree
        d0, _ = cKDTree(C).query(M)
        base_in = int((d0 < 0.04).sum())
        for y0 in (0.0, -40, -20, 20, 40):
            yaw, t, inl, rms = solve_pose(M, N, C, y0)
            if abs(yaw) > 50 or np.linalg.norm(t) > 0.35:                              # a correction, not a new place
                continue
            ca, sa = np.cos(np.radians(yaw)), np.sin(np.radians(yaw))
            P = (np.array([[ca, -sa, 0], [sa, ca, 0], [0, 0, 1.0]]) @ (M - M.mean(axis=0)).T).T + M.mean(axis=0) + t
            d, _ = cKDTree(C).query(P)
            good = int((d < 0.04).sum())
            if best is None or good > best[0]:
                best = (good, yaw, t.copy(), rms)
        if best is None:
            continue
        good, yaw, t, rms = best
        out.append({'part': name, 'points_near': len(C), 'inliers_before': base_in, 'inliers_after': good, 'share_before': round(base_in / 3000, 3), 'share_after': round(good / 3000, 3), 'yaw_deg': round(yaw, 1),
                    'move_m': [round(float(v), 3) for v in t], 'rms_m': round(rms, 4), 'centre_m': [round(float(v), 3) for v in M.mean(axis=0)], 'worth_it': bool(good > 1.25 * base_in + 100 and (abs(yaw) > 4 or np.linalg.norm(t) > 0.03))})
    out.sort(key=lambda q: q['share_before'] - q['share_after'])
    return out


def views_stage(names):
    cams, al = V.load(V.SFM_CAMERAS)['cameras'], V.load(os.path.join(D.OUT, 'align.json'))
    photos = names or sorted(k for k in al if D.compartment(k))         # the cab's group is not to scale (section 30)
    cache, scores, found, clouds = {}, {}, [], []
    for k in photos:
        t0 = time.time()
        res = compare_view(k, cache, cams, al)
        if res is None:
            continue
        s, f, c = res
        scores[k] = s
        if s['used']:
            found.extend(f)
            clouds.append(np.hstack([c['X'], c['N'], c['Xm'], c['Nm']]))
        print('%-7s agree %3.0f %%  median %.3f m  angle %5.1f  findings %2d  %.1fs  %s' % (k, 100 * s['agree'], s['median_abs_m'] or 0, s['median_angle_deg'] or 0, len(f), time.time() - t0, s['register']), flush=True)
    os.makedirs(OUT, exist_ok=True)
    V.save(os.path.join(OUT, 'per_view.json'), {'scores': scores, 'found': found})
    np.save(os.path.join(OUT, 'cloud.npy'), np.vstack(clouds))              # photo point, photo normal, model point, model normal: the same pixels
    return scores


def findings_stage():
    d = V.load(os.path.join(OUT, 'per_view.json'))
    merged = merge_findings(d['found'])
    V.save(os.path.join(OUT, 'findings.json'), {'band_m': BAND_M, 'findings': merged})
    write_score(d['scores'])
    write_report(merged, d['scores'])
    return merged


def write_score(scores):
    used = [v for v in scores.values() if v['used']]
    held = [v['fuse'] for v in scores.values() if v.get('fuse', {}).get('holdout_after_m') is not None]
    t = {'holdout_before_m': round(float(np.median([q['holdout_before_m'] for q in held])), 4) if held else None, 'holdout_after_m': round(float(np.median([q['holdout_after_m'] for q in held])), 4) if held else None, 'mean_agree': round(float(np.mean([v['agree'] for v in used])), 4), 'median_abs_m': round(float(np.median([v['median_abs_m'] for v in used])), 4), 'views_used': len(used), 'views_not_registered': sorted(k for k, v in scores.items() if not v['used']), 'views': scores}
    V.save(os.path.join(OUT, 'score.json'), t)
    return t


def score_stage():
    t = V.load(os.path.join(OUT, 'score.json'))
    bp = os.path.join(OUT, 'baseline.json')
    b = V.load(bp) if os.path.exists(bp) else None
    used = [k for k, v in t['views'].items() if v['used']]
    same = b and [k for k in used if k in b['views']]                                  # the same views on both sides, or the difference is only which views registered
    print('mean agreement %.1f %%, median miss %.3f m over %d registered views' % (100 * t['mean_agree'], t['median_abs_m'], t['views_used']) + ('   (same views at the baseline: %.1f %%, %.3f m)' % (100 * np.mean([b['views'][k]['agree'] for k in same]), np.median([b['views'][k]['median_abs_m'] for k in same])) if same else ''))
    print('not used:', ' '.join(t['views_not_registered']))
    for k, v in sorted(t['views'].items()):
        o = b['views'].get(k) if b else None
        print('%-7s %5.1f %%%s%s' % (k, 100 * v['agree'], '   was %5.1f %%' % (100 * o['agree']) if o else '', '' if v['used'] else '   (not used)'))


def write_report(merged, scores):
    lines = ['# What differs between the Osa model and the photos', '',
             'Written by `tools/recon/holistic_diff.py` (docs/OSA_INTERIOR_HOLISTIC.md section 32). Each finding is a region where the photo\'s depth or surface direction disagrees with the model from the same camera, merged across cameras. Generated; do not edit.', '',
             '| Score | |', '|---|---|', '| Mean share of pixels agreeing | %.1f %% |' % (100 * float(np.mean([v['agree'] for v in scores.values() if v['used']]))), '| Views compared | %d |' % sum(1 for v in scores.values() if v['used']), '| Views not registered, not used | %s |' % (', '.join(sorted(k for k, v in scores.items() if not v['used'])) or 'none'), '', '## Findings, largest first', '',
             '| Id | Kind | Part | Views | Pixels | Conclusion |', '|---|---|---|---|---|---|']
    for f in merged[:80]:
        lines.append('| %s | %s | %s | %s | %d | %s |' % (f['id'], f['kind'], f['part'], ', '.join(f['views'][:5]) + (' ...' if f['n_views'] > 5 else ''), f['pixels'], f['says']))
    p = os.path.join(OUT, 'poses.json')
    if os.path.exists(p):
        lines += ['', '## Parts whose pose the photos would change', '', '| Part | Fit before | Fit after | Turn (deg) | Move x, y, z (m) |', '|---|---|---|---|---|']
        for q in V.load(p)['parts']:
            if q['worth_it']:
                lines.append('| %s | %.0f %% | %.0f %% | %+.1f | %+.3f, %+.3f, %+.3f |' % (q['part'], 100 * q['share_before'], 100 * q['share_after'], q['yaw_deg'], *q['move_m']))
    with open(REPORT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


def poses_stage():
    cloud = np.load(os.path.join(OUT, 'cloud.npy'))[:, :3].astype(float)
    res = solve_part_poses(Scene(), None, cloud)
    V.save(os.path.join(OUT, 'poses.json'), {'parts': res})
    for q in res:
        if q['worth_it']:
            print('%-22s fit %3.0f %% -> %3.0f %%  turn %+6.1f  move %s' % (q['part'], 100 * q['share_before'], 100 * q['share_after'], q['yaw_deg'], q['move_m']))
    return res


def surfaces_stage():
    """The floor, roof and walls as seen from above, from the side and from the front: photo beside model and their difference, measured on one grid."""
    import cv2
    d = os.path.join(OUT, 'surfaces')
    os.makedirs(d, exist_ok=True)
    cloud = np.load(os.path.join(OUT, 'cloud.npy')).astype(float)
    C = 0.05
    specs = [('floor', 2, 0, 1, (-1.45, 1.05), (-1.15, 3.2), (0.45, 1.5), 'low', 2, 0.6, 'z of what faces up: x across, y forward'),
             ('roof', 2, 0, 1, (-1.45, 1.05), (-1.15, 3.2), (1.7, 2.6), 'high', 2, -0.6, 'z of what faces down'),
             ('wall_right', 0, 1, 2, (-1.15, 3.2), (0.45, 2.4), (0.2, 1.2), 'high', 0, -0.6, 'x of what faces the aisle from the right: y along, z up'),
             ('wall_left', 0, 1, 2, (-1.15, 3.2), (0.45, 2.4), (-1.7, -0.5), 'low', 0, 0.6, 'x of what faces the aisle from the left: y along, z up'),
             ('bulkhead', 1, 0, 2, (-1.45, 1.05), (0.45, 2.4), (-1.35, -0.4), 'low', 1, 0.6, 'y of what faces forward: x across, z up')]
    out = {}

    def grid(P, axis, a, b, ra, rb, rc, mode):
        k = (P[:, a] >= ra[0]) & (P[:, a] < ra[1]) & (P[:, b] >= rb[0]) & (P[:, b] < rb[1]) & (P[:, axis] >= rc[0]) & (P[:, axis] < rc[1])
        P = P[k]
        na, nb = int((ra[1] - ra[0]) / C), int((rb[1] - rb[0]) / C)
        ia, ib = np.minimum(((P[:, a] - ra[0]) / C).astype(int), na - 1), np.minimum(((P[:, b] - rb[0]) / C).astype(int), nb - 1)
        G = np.full((nb, na), np.nan)
        key = ib * na + ia
        order = np.argsort(key)
        key, vals = key[order], P[order, axis]
        cuts = np.flatnonzero(np.diff(key)) + 1
        for ks, v in zip(np.split(key, cuts), np.split(vals, cuts)):
            if len(v) >= 8:
                G[ks[0] // na, ks[0] % na] = np.percentile(v, 15 if mode == 'low' else 85)
        return G

    def ticks(im, ra, rb, up):
        h = im.shape[0]
        for v in np.arange(np.ceil(ra[0] * 2) / 2, ra[1], 0.5):
            x = int((v - ra[0]) / C * up)
            cv2.line(im, (x, 0), (x, h), (255, 255, 255), 1)
            cv2.putText(im, '%.1f' % v, (x + 2, h - 4), 0, 0.38, (255, 255, 255), 1, cv2.LINE_AA)
        for v in np.arange(np.ceil(rb[0] * 2) / 2, rb[1], 0.5):
            y = h - int((v - rb[0]) / C * up)
            cv2.line(im, (0, y), (im.shape[1], y), (255, 255, 255), 1)
            cv2.putText(im, '%.1f' % v, (2, y - 2), 0, 0.38, (255, 255, 255), 1, cv2.LINE_AA)
        return im

    for name, axis, a, b, ra, rb, rc, mode, nax, sign, what in specs:
        kp = cloud[:, 3 + nax] * np.sign(sign) > abs(sign)
        km = cloud[:, 9 + nax] * np.sign(sign) > abs(sign)
        Gp, Gm = grid(cloud[kp, :3], axis, a, b, ra, rb, rc, mode), grid(cloud[km, 6:9], axis, a, b, ra, rb, rc, mode)
        diff = Gm - Gp
        out[name] = {'axis': 'xyz'[axis], 'across': 'xyz'[a], 'along': 'xyz'[b], 'range_across': ra, 'range_along': rb, 'cell_m': C, 'what': what, 'photo': np.round(Gp, 3).tolist(), 'model': np.round(Gm, 3).tolist()}
        both = np.concatenate([Gp[np.isfinite(Gp)], Gm[np.isfinite(Gm)]])
        lo, hi = float(np.percentile(both, 2)), float(np.percentile(both, 98))
        up = 9
        ims = [ticks(cv2.resize(q, None, fx=up, fy=up, interpolation=cv2.INTER_NEAREST), ra, rb, up) for q in (D.turbo(np.flipud(Gp), lo, hi), D.turbo(np.flipud(Gm), lo, hi), D.diverging(np.flipud(diff), 0.10))]
        gap = np.full((ims[0].shape[0], 8, 3), 255, np.uint8)
        sheet = np.hstack([ims[0], gap, ims[1], gap, ims[2]])
        cv2.putText(sheet, '%s  photos | model | difference  (%s  %.2f to %.2f)' % (name, what, lo, hi), (6, 16), 0, 0.45, (255, 255, 255), 2, cv2.LINE_AA)
        cv2.imwrite(os.path.join(d, name + '.png'), sheet)
    V.save(os.path.join(d, 'surfaces.json'), out)
    return d


def registered_camera(photo):
    z = np.load(os.path.join(OUT, 'ref', photo + '.npz'))
    return z, V.Camera(float(z['f']), float(z['cx']), float(z['cy']), z['R'], z['t'], int(z['w']), int(z['h']), float(z['k1']))


def lift_polygon(photo, poly, scene=None):
    """A polygon of a registered photo (as shares of its width and height) as points in the cabin, and the model's points at the same pixels (with scene)."""
    import cv2
    z, cam = registered_camera(photo)
    m = np.zeros((cam.h, cam.w), np.uint8)
    cv2.fillPoly(m, [np.array([[a * cam.w, b * cam.h] for a, b in poly], np.int32)], 1)
    y, x = np.nonzero(m & np.isfinite(z['z']))
    c = rays_of(cam, x + 0.5, y + 0.5)
    nrm = np.linalg.norm(c, axis=1)
    d = (cam.R.T @ (c / nrm[:, None]).T).T
    P = cam.centre() + d * (z['z'][y, x] * nrm)[:, None]
    if scene is None:
        return P
    tri, t = scene.cast(cam.centre(), d)
    k = tri >= 0
    return P, cam.centre() + d[k] * t[k][:, None], scene.part[tri[k]]


def ransac_plane(P, tol=0.02, tries=400, rng=None):
    """The dominant plane of points: centroid, normal, inlier mask."""
    rng = rng or np.random.default_rng(0)
    best = None
    for _ in range(tries):
        a, b, c = P[rng.choice(len(P), 3, replace=False)]
        n = np.cross(b - a, c - a)
        L = np.linalg.norm(n)
        if L < 1e-6:
            continue
        n /= L
        k = np.abs((P - a) @ n) < tol
        if best is None or k.sum() > best.sum():
            best = k
    c, n, _ = fit_plane(P[best])
    k = np.abs((P - c) @ n) < tol
    c, n, _ = fit_plane(P[k])
    return c, n, k


def plane_words(P):
    c, n, k = ransac_plane(P)
    if abs(n[2]) >= 0.5 and n[2] < 0:
        n = -n
    elif abs(n[2]) < 0.5 and n[np.argmax(np.abs(n[:2]))] < 0 and abs(n[1]) >= abs(n[0]):
        n = -n
    yaw = float(np.degrees(np.arctan2(-n[0], n[1])))                                   # 0 faces +y; positive turns it toward -x
    tilt = float(np.degrees(np.arcsin(np.clip(n[2], -1, 1))))
    q = np.percentile(P[k], [5, 95], axis=0)
    return {'centre': [round(float(v), 3) for v in c], 'normal': [round(float(v), 3) for v in n], 'yaw_deg': round(yaw, 1), 'tilt_deg': round(tilt, 1), 'on_plane': round(float(k.mean()), 2), 'range': [[round(float(v), 3) for v in q[0]], [round(float(v), 3) for v in q[1]]]}


def measure(photo, poly, scene=None):
    """A region of a registered photo in metres, beside the model through the same pixels."""
    scene = scene or Scene()
    P, Pm, pid = lift_polygon(photo, poly, scene)
    a, b = plane_words(P), plane_words(Pm) if len(Pm) >= 20 else None
    ids, n = np.unique(pid, return_counts=True)
    parts = [(scene.names[i], int(c)) for i, c in sorted(zip(ids, n), key=lambda q: -q[1])[:3]]
    print('%s  photo %s\n  model %s\n  model parts there: %s' % (photo, a, b, parts))
    return a, b


RENDER_FILL = 0.62


def render_stage(names, width=1200):
    """The model rendered in Blender from each photo's registered camera, beside the photo."""
    import cv2
    d = os.path.join(OUT, 'renders')
    os.makedirs(d, exist_ok=True)
    for k in names or [q for q, v in V.load(os.path.join(OUT, 'score.json'))['views'].items() if v['used']]:
        _, cam = registered_camera(k)
        cam = cam.scaled(width, int(round(cam.h * width / cam.w)))
        state = V.load(V.view_path(k)).get('state', {}) if os.path.exists(V.view_path(k)) else {}
        job = {'camera': cam.blender(), 'out': os.path.join(d, k + '_'), 'passes': ['model'], 'fill': True, 'repo': V.REPO, 'rays': [], 'hide': state.get('hide', [])}
        jp = os.path.join(d, k + '_job.json')
        V.save(jp, job)
        r = subprocess.run([V.BLENDER, '--background', '--factory-startup', '--python', V.SCENE, '--', jp], capture_output=True, text=True, encoding='utf-8', errors='replace')
        if r.returncode != 0 or not os.path.exists(job['out'] + 'model.png'):
            print(k, 'did not render:', r.stderr[-800:])
            continue
        im = cv2.resize(cv2.imread(V.photo_file(k, 'clean')), (cam.w, cam.h), interpolation=cv2.INTER_AREA)
        lit = np.maximum(cv2.imread(job['out'] + 'model.png'), (cv2.imread(job['out'] + 'model_flat.png') * RENDER_FILL).astype(np.uint8))      # surfaces facing down are not black: the shapes of a roof must show
        cv2.imwrite(os.path.join(d, k + '.jpg'), np.hstack([im, cv2.resize(lit, (cam.w, cam.h))]), [cv2.IMWRITE_JPEG_QUALITY, 88])
        print(k, 'rendered', flush=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('stage', choices=['scene', 'views', 'findings', 'poses', 'surfaces', 'score', 'baseline', 'measure', 'render', 'all'])
    ap.add_argument('--poly', help='measure: x,y,x,y,... as shares of the photo')
    ap.add_argument('photos', nargs='*')
    a = ap.parse_args(argv)
    if a.stage == 'render':
        render_stage(a.photos)
        return 0
    if a.stage == 'measure':
        v = [float(q) for q in a.poly.split(',')]
        for ph in a.photos:
            measure(ph, list(zip(v[::2], v[1::2])))
        return 0
    if a.stage in ('scene', 'all'):
        scene_job()
    if a.stage in ('views', 'all'):
        views_stage(a.photos)
    if a.stage in ('poses', 'all'):
        poses_stage()
    if a.stage in ('surfaces', 'all'):
        print('surfaces in', surfaces_stage())
    if a.stage in ('findings', 'all'):
        print(len(findings_stage()), 'findings')
    if a.stage in ('score', 'all'):
        score_stage()
    if a.stage == 'baseline':
        V.save(os.path.join(OUT, 'baseline.json'), V.load(os.path.join(OUT, 'score.json')))
        print('baseline kept')
    return 0


if __name__ == '__main__':
    sys.exit(main())
