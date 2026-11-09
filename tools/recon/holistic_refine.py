"""The joint solve adjusted again with what COLMAP could not know (docs/OSA_INTERIOR_HOLISTIC.md section 32).

A camera square-on to a rack cannot tell its distance from its focal length, so the solve came out stretched across the aisle: everything off SP-2M2's plane
runs about 15 % too far. Depth comparison is only as good as the cameras, so this runs before it. A bundle adjustment over the solve's own tracks with the priors:

  - SP-2M2's traced face, as the alignment already used (the frame);
  - each camera's focal length as the depth network estimates it from that photo alone (independent of the solve and of the model);
  - measured planes and lengths named in PRIORS, each with its source.

Run with the depth environment's Python (it has pycolmap and scipy):

    E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8 tools/recon/holistic_refine.py fov     # the network's own focal lengths
    E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8 tools/recon/holistic_refine.py ba      # writes out/recon/holistic/sfm/refined/
    python tools/recon/holistic_refine.py report                                               # what moved

Reads the solve; writes only under out/recon/holistic/sfm/refined/. tools/recon/holistic_view.py and holistic_depth.py use the refined cameras when they exist.
"""
import argparse
import json
import os
import subprocess
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SFM = os.path.join(REPO, 'out', 'recon', 'holistic', 'sfm')
OUT = os.path.join(SFM, 'refined')
MONO_FREE = os.path.join(OUT, 'mono_free')
CAMERAS, POINTS = os.path.join(SFM, 'cameras.json'), os.path.join(SFM, 'points.json')

# A plane in the body frame that a box of the first solve's points lies on. 'box' selects the points in the unrefined solve; the prior pulls them to axis = value.
PRIORS = [
    {'name': 'SL-2M face', 'axis': 0, 'value': -1.10, 'sigma_m': 0.02, 'box': [[-1.60, -1.25], [0.75, 2.55], [1.0, 2.1]],
     'source': 'g082-b, one depth map scaled by the seat alone: SL-1M -1.13; left post through g096-a -1.11; docs/OSA_INTERIOR_HOLISTIC.md sections 28, 31'},
    # The tracks fit a narrower cabin as well as the wide one (1.01 px against 0.99), but left alone they pay for it by shearing y and z. A depth error across the aisle leaves
    # what a camera sees sideways unchanged, and y and z were already right against the depth maps (walkway, bulkhead, roof: section 31), so they are held where they were.
    {'name': 'compartment y and z', 'hold': [1, 2], 'sigma_m': 0.03, 'box': [[-1.8, 1.1], [-1.3, 2.9], [0.4, 2.7]], 'source': 'section 31'},
    # And left with only the rack's face to go by, the narrowing comes out as a shear: the aisle's floor moved 0.3 m sideways at y 2.4. The narrowing is uniform about SP-2M2's plane.
    {'name': 'compartment across the aisle', 'narrow': {'axis': 0, 'about': 0.61, 'factor': 0.847}, 'sigma_m': 0.05, 'box': [[-1.8, 1.1], [-1.3, 2.9], [0.4, 2.7]],
     'source': 'the two checks of SL-1M/SL-2M\'s face and the post above, sections 28, 29'},
]
FOCAL_SIGMA = 0.06       # share of the focal length: the network's estimate is coarse, one prior per camera
PIXEL_SIGMA = 1.5        # robust scale of a reprojection, px


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def rodrigues(r):
    """Rotation matrices of rotation vectors, N x 3 to N x 3 x 3."""
    th = np.linalg.norm(r, axis=1, keepdims=True)
    k = r / np.maximum(th, 1e-12)
    th = th[:, 0]
    K = np.zeros((len(r), 3, 3))
    K[:, 0, 1], K[:, 0, 2], K[:, 1, 0] = -k[:, 2], k[:, 1], k[:, 2]
    K[:, 1, 2], K[:, 2, 0], K[:, 2, 1] = -k[:, 0], -k[:, 1], k[:, 0]
    I = np.eye(3)[None]
    return I + np.sin(th)[:, None, None] * K + (1 - np.cos(th))[:, None, None] * (K @ K)


def rotate(r, X):
    """Rodrigues' formula applied: rotation vectors r on points X, both N x 3."""
    th = np.linalg.norm(r, axis=1, keepdims=True)
    k = r / np.maximum(th, 1e-12)
    c, s = np.cos(th), np.sin(th)
    return X * c + np.cross(k, X) * s + k * np.sum(k * X, axis=1, keepdims=True) * (1 - c)


def project(cam, X, ci):
    """Pixels of points X seen by cameras ci; cam rows: rotation vector, t, log f, k1. (Principal points stay fixed outside.)"""
    Xc = rotate(cam[ci, :3], X) + cam[ci, 3:6]
    z = np.maximum(Xc[:, 2], 1e-3)
    n = Xc[:, :2] / z[:, None]
    f = np.exp(cam[ci, 6])
    r2 = np.sum(n * n, axis=1)
    return (f * (1 + cam[ci, 7] * r2))[:, None] * n


def huber_weight(r):
    """IRLS weight of a standardised residual (Huber, scale 1)."""
    a = np.abs(r)
    return np.where(a <= 1.0, 1.0, 1.0 / np.maximum(a, 1e-9))


def huber_cost(r):
    a = np.abs(r)
    return float(np.sum(np.where(a <= 1.0, 0.5 * a * a, a - 0.5)))


def bundle_adjust(C0, X0, oc, op, obs, anchor=(), A0=None, planes=(), focal=None, focal_sigma=FOCAL_SIGMA, iters=60, log=None):
    """Levenberg-Marquardt over cameras (rotation vector, t, log f, k1) and points, eliminating the points (the Schur complement). Priors: anchor points held at A0 (1 cm), planes
    (point indices, axis, value, sigma), a log focal length per camera (focal: NaN where none), k1 near its start. Returns C, X, the history of costs. Numpy only."""
    n, m = len(C0), len(X0)
    anchor = np.asarray(anchor, int)
    has_f = np.isfinite(focal) if focal is not None else np.zeros(n, bool)
    s = np.argsort(op, kind='stable')
    pairs_a, pairs_b, pairs_p = [], [], []
    cuts = np.flatnonzero(np.diff(op[s])) + 1
    for group in np.split(s, cuts):
        a, b = np.meshgrid(group, group, indexing='ij')
        pairs_a.append(a.ravel())
        pairs_b.append(b.ravel())
        pairs_p.append(np.full(a.size, op[group[0]]))
    pairs_a, pairs_b, pairs_p = np.concatenate(pairs_a), np.concatenate(pairs_b), np.concatenate(pairs_p)

    def cost(C, X):
        total = huber_cost(np.linalg.norm(project(C, X[op], oc) - obs, axis=1) / PIXEL_SIGMA)
        if len(anchor):
            total += 0.5 * float(np.sum(((X[anchor] - A0) / 0.01) ** 2))
        for idx, ax, val, sg in planes:
            total += huber_cost((X[idx, ax] - val) / sg)
        if has_f.any():
            total += huber_cost((C[has_f, 6] - focal[has_f]) / focal_sigma)
        return total + 0.5 * float(np.sum(((C[:, 7] - C0[:, 7]) / 0.1) ** 2))

    C, X = C0.copy(), X0.copy()
    best, lam, hist = cost(C, X), 1e-3, []
    for it in range(iters):
        p0 = project(C, X[op], oc)
        res = (p0 - obs) / PIXEL_SIGMA
        w = huber_weight(np.linalg.norm(res, axis=1))
        Jc = np.zeros((len(oc), 2, 8))
        for j in range(8):
            d = C.copy()
            d[:, j] += 1e-6
            Jc[:, :, j] = (project(d, X[op], oc) - p0) / 1e-6 / PIXEL_SIGMA
        Jp = np.zeros((len(oc), 2, 3))
        for j in range(3):
            d = X.copy()
            d[:, j] += 1e-6
            Jp[:, :, j] = (project(C, d[op], oc) - p0) / 1e-6 / PIXEL_SIGMA
        Jct, Jpt = np.transpose(Jc, (0, 2, 1)) * w[:, None, None], np.transpose(Jp, (0, 2, 1)) * w[:, None, None]
        U, V = np.zeros((n, 8, 8)), np.zeros((m, 3, 3))
        gc, gp = np.zeros((n, 8)), np.zeros((m, 3))
        np.add.at(U, oc, Jct @ Jc)
        np.add.at(V, op, Jpt @ Jp)
        np.add.at(gc, oc, -(Jct @ res[..., None])[..., 0])
        np.add.at(gp, op, -(Jpt @ res[..., None])[..., 0])
        W = Jct @ Jp
        if len(anchor):
            V[anchor] += np.eye(3) / 0.01 ** 2
            gp[anchor] -= (X[anchor] - A0) / 0.01 ** 2
        for idx, ax, val, sg in planes:
            q = (X[idx, ax] - val) / sg
            wq = huber_weight(q)
            V[idx, ax, ax] += wq / sg ** 2
            gp[idx, ax] -= wq * q / sg
        if has_f.any():
            q = (C[has_f, 6] - focal[has_f]) / focal_sigma
            wq = huber_weight(q)
            i = np.flatnonzero(has_f)
            U[i, 6, 6] += wq / focal_sigma ** 2
            gc[i, 6] -= wq * q / focal_sigma
        U[:, 7, 7] += 1 / 0.1 ** 2
        gc[:, 7] -= (C[:, 7] - C0[:, 7]) / 0.1 ** 2
        for _ in range(12):
            Vd = V.copy()
            Vd[:, np.arange(3), np.arange(3)] *= 1 + lam
            Vd[:, np.arange(3), np.arange(3)] += 1e-9
            Vi = np.linalg.inv(Vd)
            S = np.zeros((n, n, 8, 8))
            S[np.arange(n), np.arange(n)] = U
            S[np.arange(n), np.arange(n), :, :] += np.eye(8)[None] * (1e-9 + lam * np.diagonal(U, axis1=1, axis2=2)[:, None, :])
            WT = np.transpose(W[pairs_b], (0, 2, 1))
            np.add.at(S, (oc[pairs_a], oc[pairs_b]), -(W[pairs_a] @ Vi[pairs_p] @ WT))
            rhs = gc.copy()
            np.add.at(rhs, oc, -(W @ (Vi[op] @ gp[op][..., None]))[..., 0])
            A = np.transpose(S, (0, 2, 1, 3)).reshape(8 * n, 8 * n)
            dc = np.linalg.solve(A, rhs.ravel()).reshape(n, 8)
            b = gp.copy()
            np.add.at(b, op, -(np.transpose(W, (0, 2, 1)) @ dc[oc][..., None])[..., 0])
            dp = (Vi @ b[..., None])[..., 0]
            Cn, Xn = C + dc, X + dp
            c = cost(Cn, Xn)
            if c < best:
                break
            lam *= 4.0
        else:
            hist.append(best)
            break
        gain = best - c
        C, X, best = Cn, Xn, c
        lam = max(lam * 0.3, 1e-9)
        hist.append(best)
        if log and (it % 5 == 0):
            log('iteration %3d  cost %.1f  damping %.1e' % (it, best, lam))
        if gain < 1e-5 * best:
            break
    return C, X, hist


def cmd_fov(_args):
    """MoGe-2 with no field of view given, on every solved photo: the focal length the photo itself suggests."""
    sys.path.insert(0, HERE)
    cams = load(CAMERAS)['cameras']
    os.makedirs(MONO_FREE, exist_ok=True)
    photos = [{'id': k, 'image': os.path.join(REPO, 'out', 'recon', 'dryrun', 'photos', 'clean', k + '.png'), 'fov_x_deg': None, 'out': os.path.join(MONO_FREE, k + '.npz')} for k in cams]
    path = os.path.join(MONO_FREE, 'job.json')
    with open(path, 'w', encoding='utf-8') as f:
        json.dump({'size': 1200, 'photos': photos}, f)
    env = dict(os.environ, HF_HOME=r'E:\SAM3D-tools\models\hf', HF_HUB_DISABLE_SYMLINKS_WARNING='1')
    subprocess.run([sys.executable, '-X', 'utf8', os.path.join(HERE, 'holistic_mono_infer.py'), path], check=True, env=env)
    out = {}
    for k, c in cams.items():
        fov = float(np.load(os.path.join(MONO_FREE, k + '.npz'))['fov_x_deg'])
        w = c['size_px'][0]
        f = 0.5 * w / np.tan(np.radians(fov) / 2)
        out[k] = {'f_px': round(float(f), 1), 'fov_x_deg': round(fov, 2), 'solve_f_px': round(c['f_px'], 1), 'ratio': round(float(f / c['f_px']), 3)}
    with open(os.path.join(OUT, 'fov.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)
    r = np.array([v['ratio'] for v in out.values()])
    print('network focal / solve focal: median %.3f, 10-90 %% %.3f to %.3f' % (np.median(r), *np.percentile(r, [10, 90])))
    return 0


def read_tracks():
    """The COLMAP model's cameras and tracks, in the body frame cameras.json was aligned to."""
    import pycolmap
    align = load(CAMERAS)
    k, R0, t0 = align['scale'], np.array(align['R']), np.array(align['t'])
    rec = pycolmap.Reconstruction(os.path.join(SFM, 'model', 'models', '0'))
    names, index = [], {}
    for im in rec.images.values():
        index[im.image_id] = len(names)
        names.append(im.name[:-4])
    P, obs_c, obs_p, obs_uv = [], [], [], []
    for p in rec.points3D.values():
        if len(p.track.elements) < 3:
            continue
        i = len(P)
        P.append(k * R0 @ p.xyz + t0)
        for e in p.track.elements:
            im = rec.images[e.image_id]
            obs_c.append(index[e.image_id])
            obs_p.append(i)
            obs_uv.append(im.points2D[e.point2D_idx].xy)
    return names, np.array(P), np.array(obs_c), np.array(obs_p), np.array(obs_uv, float)


def cmd_ba(args):
    from scipy.optimize import least_squares
    from scipy.sparse import lil_matrix
    os.makedirs(OUT, exist_ok=True)
    data = load(CAMERAS)
    cams = data['cameras']
    names, P, oc, op, uv = read_tracks()
    n = len(names)
    print('%d cameras, %d points, %d observations' % (n, len(P), len(oc)), flush=True)
    C0 = np.zeros((n, 8))
    pp = np.zeros((n, 2))
    from scipy.spatial.transform import Rotation
    for i, k in enumerate(names):
        c = cams[k]
        C0[i, :3] = Rotation.from_matrix(np.array(c['R'])).as_rotvec()
        C0[i, 3:6] = c['t']
        C0[i, 6] = np.log(c['f_px'])
        C0[i, 7] = c.get('k1', 0.0)
        pp[i] = c['pp_px']
    obs = uv - pp[oc]
    fov = load(os.path.join(OUT, 'fov.json')) if os.path.exists(os.path.join(OUT, 'fov.json')) else {}
    fprior = np.array([np.log(fov[k]['f_px']) if k in fov else np.nan for k in names])
    use_f = np.isfinite(fprior) if args.focal else np.zeros(n, bool)
    # the priors: indices of points on each plane, chosen once from the unrefined solve
    planes = []
    for pr in (PRIORS if args.planes else []):
        b = np.array(pr['box'])
        idx = np.flatnonzero(np.all((P >= b[:, 0]) & (P <= b[:, 1]), axis=1))
        if 'narrow' in pr:
            q = pr['narrow']
            planes.append((idx, q['axis'], q['about'] + (P[idx, q['axis']] - q['about']) * q['factor'], pr['sigma_m']))
        elif 'hold' in pr:
            planes.extend((idx, ax, P[idx, ax].copy(), pr['sigma_m']) for ax in pr['hold'])
        else:
            planes.append((idx, pr['axis'], pr['value'], pr['sigma_m']))
        print('prior %s: %d points' % (pr['name'], len(idx)), flush=True)
    # the frame: the points on SP-2M2's face stay where the alignment put them (x, y, z)
    b = np.array([[0.5, 0.75], [0.6, 2.55], [0.8, 2.1]])
    anchor = np.flatnonzero(np.all((P >= b[:, 0]) & (P <= b[:, 1]), axis=1))
    A0 = P[anchor].copy()
    print('frame anchor: %d points on SP-2M2' % len(anchor), flush=True)

    m = len(P)
    t0 = time.time()
    f = fprior.copy()
    f[~use_f] = np.nan
    C, X, hist = bundle_adjust(C0, P, oc, op, obs, anchor, A0, planes, f, iters=args.iters, log=lambda t: print(t, flush=True))
    print('solved in %.0f s, %d iterations; cost %.0f to %.0f' % (time.time() - t0, len(hist), hist[0], hist[-1]), flush=True)
    e = np.linalg.norm(project(C, X[op], oc) - obs, axis=1)
    print('reprojection median %.2f px, 90 %% %.2f px' % (np.median(e), np.percentile(e, 90)))
    out = {}
    for i, k in enumerate(names):
        c = dict(cams[k])
        Rm = Rotation.from_rotvec(C[i, :3]).as_matrix()
        c.update(R=Rm.tolist(), t=C[i, 3:6].tolist(), f_px=float(np.exp(C[i, 6])), k1=float(C[i, 7]), eye_m=(-Rm.T @ C[i, 3:6]).round(3).tolist())
        out[k] = c
    with open(os.path.join(OUT, 'cameras.json'), 'w', encoding='utf-8') as f:
        json.dump({'how': 'holistic_refine.py: the joint solve adjusted with focal-length and plane priors', 'cameras': out}, f, indent=1)
    seen = [[] for _ in range(m)]
    for c_, p_ in zip(oc, op):
        seen[p_].append(names[c_])
    with open(os.path.join(OUT, 'points.json'), 'w', encoding='utf-8') as f:
        json.dump([[*X[i].round(4).tolist(), seen[i]] for i in range(m)], f)
    return cmd_report(args)


def cmd_report(_args):
    a, b = load(CAMERAS)['cameras'], load(os.path.join(OUT, 'cameras.json'))['cameras']
    print('%-8s %8s %8s %6s   eye before -> after' % ('photo', 'f before', 'f after', 'ratio'))
    for k in sorted(a):
        print('%-8s %8.0f %8.0f %6.3f   %s -> %s' % (k, a[k]['f_px'], b[k]['f_px'], b[k]['f_px'] / a[k]['f_px'], a[k]['eye_m'], b[k]['eye_m']))
    P0, P1 = np.array([p[:3] for p in load(POINTS) if len(p[3]) >= 3]), np.array([p[:3] for p in load(os.path.join(OUT, 'points.json'))])
    for name, box in (('SP-2M2 face', [[0.5, 0.75], [0.6, 2.55], [0.8, 2.1]]), ('SL-2M face', [[-1.6, -1.0], [0.75, 2.55], [1.0, 2.1]]), ('bulkhead', [[-1.2, 0.8], [-1.3, -0.9], [1.0, 2.1]]), ('roof', [[-0.8, 0.5], [-1.0, 2.5], [1.95, 2.5]])):
        b_ = np.array(box)
        k = np.all((P1 >= b_[:, 0]) & (P1 <= b_[:, 1]), axis=1)
        print('%-12s n %5d  median x %.3f y %.3f z %.3f' % (name, k.sum(), *np.median(P1[k], axis=0)))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['fov', 'ba', 'report'])
    ap.add_argument('--no-focal', dest='focal', action='store_false', help='leave out the focal-length priors')
    ap.add_argument('--no-planes', dest='planes', action='store_false', help='leave out the plane priors (to see what the focal lengths do alone)')
    ap.add_argument('--iters', type=int, default=80)
    a = ap.parse_args(argv)
    return {'fov': cmd_fov, 'ba': cmd_ba, 'report': cmd_report}[a.cmd](a)


if __name__ == '__main__':
    sys.exit(main())
