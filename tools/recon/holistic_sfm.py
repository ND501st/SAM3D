"""The holistic layer's cross-reference between the book's interior photos: they are matched to each other, not to the cabin.

The book's launcher interior is two sessions, by the catalogue on two vehicles (they share a solved model, which suggests one, unproven): g082-g096 in a hangar on an ex-NVA 9A33BM3-1, and
g097-g100, "session A", on a 9A33BM2/BM3 not identified. A photo is tied to the others of its own session; a match across the
two is the same type, not the same steel. The reloading vehicle (9T217, the cab plates g110-g111) is similar but differs and
is never used here, nor is the later Czech launcher of g101. ALIKED and LightGlue (hloc) find the matches, COLMAP solves the cameras together, and the sparse points that
result are the compartment's own shape, owing nothing to data/cabins/. Run it with the tools' Python, which has the GPU:

    E:\\SAM3D-tools\\venv\\Scripts\\python.exe tools/recon/holistic_sfm.py run
    python tools/recon/holistic_sfm.py summary          # needs only the standard library

It reads the measuring pipeline's outputs and writes only under out/recon/holistic/sfm/. See docs/OSA_INTERIOR_HOLISTIC.md
section 16.
"""
import argparse
import json
import math
import os
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PHOTOS = os.path.join(REPO, 'out', 'recon', 'dryrun', 'photos', 'clean')
OUT = os.path.join(REPO, 'out', 'recon', 'holistic', 'sfm')
COLMAP = r'E:/SAM3D-tools/colmap/bin/colmap.exe'

# The book's plates that are drawings, or of another vehicle, are not photos of this session.
DRAWINGS = {'g084-d', 'g090-b', 'g091-c', 'g093-d', 'g094-c', 'g097-c', 'g097-d', 'g098-d', 'g099-c'}
LATER_VEHICLE = {'g101-a', 'g101-b', 'g101-c', 'g101-d', 'g101-e', 'g101-f'}     # the Czech 2000s fit (section 9)
FIRST_PAGE, LAST_PAGE = 82, 100


def session_of(name):
    """Which vehicle a photo of pp. 82-100 is (out/recon/dryrun/catalogue.json): 'hangar' (9A33BM3-1) or 'A' (9A33BM2/BM3)."""
    return 'hangar' if int(name[1:4]) <= 96 else 'A'


def session_photos():
    """The interior session's photos: pp. 82-100 of the book, without its drawings."""
    out = []
    for f in sorted(os.listdir(PHOTOS)):
        n, ext = os.path.splitext(f)
        if ext.lower() not in ('.jpg', '.png') or not n.startswith('g'):
            continue
        if FIRST_PAGE <= int(n[1:4]) <= LAST_PAGE and n not in DRAWINGS and n not in LATER_VEHICLE:
            out.append(f)
    return out


def cmd_run(args):
    from pathlib import Path

    import pycolmap
    from hloc import extract_features, match_features, pairs_from_exhaustive, reconstruction

    out = Path(OUT)
    out.mkdir(parents=True, exist_ok=True)
    images = Path(PHOTOS)
    names = session_photos()
    print('%d photos' % len(names), flush=True)
    t0 = time.time()
    feat = dict(extract_features.confs['aliked-n16'])
    feat['model'] = dict(feat['model'], max_num_keypoints=args.keypoints)
    feat['preprocessing'] = dict(feat['preprocessing'], resize_max=args.resize)
    feat['output'] = 'feats-aliked'
    match = dict(match_features.confs['aliked+lightglue'])
    features = extract_features.main(feat, images, out, image_list=names)
    print('features %.0f s' % (time.time() - t0), flush=True)
    pairs = out / 'pairs.txt'
    pairs_from_exhaustive.main(pairs, image_list=names)
    matches = out / 'matches.h5'
    match_features.main(match, pairs, features=features, matches=matches)
    print('matched %.0f s' % (time.time() - t0), flush=True)
    try:
        reconstruction.main(out / 'model', images, pairs, features, matches, camera_mode=pycolmap.CameraMode.PER_IMAGE,
                            image_list=names, image_options={'camera_model': 'SIMPLE_RADIAL'})
    except TypeError:
        pass                      # hloc 1.5's own mapping call is out of step with pycolmap 4; the database is written by then
    print('database %.0f s' % (time.time() - t0), flush=True)
    return cmd_map(args)


def cmd_map(args):
    """COLMAP's incremental mapper over the verified matches (one camera per print: they are cropped differently)."""
    from pathlib import Path

    import pycolmap

    model = Path(OUT) / 'model'
    db = model / 'database.db'
    opts = pycolmap.IncrementalPipelineOptions()
    opts.min_num_matches = args.min_matches
    opts.ba_refine_principal_point = False
    opts.multiple_models = True
    opts.min_model_size = 3
    opts.ba_refine_extra_params = True
    opts.mapper.min_focal_length_ratio = 0.5
    opts.mapper.max_focal_length_ratio = 2.2
    opts.mapper.filter_min_tri_angle = 2.0
    opts.mapper.init_min_tri_angle = 8.0
    opts.mapper.max_extra_param = 0.3
    opts.mapper.init_min_num_inliers = 60
    opts.mapper.abs_pose_min_num_inliers = 15
    opts.mapper.abs_pose_min_inlier_ratio = 0.15
    (model / 'models').mkdir(exist_ok=True)
    recs = pycolmap.incremental_mapping(db, PHOTOS, model / 'models', options=opts)
    for k, r in recs.items():
        print(k, r.summary(), flush=True)
    return 0 if recs else 1


def cmd_dense(args):
    """COLMAP's PatchMatch stereo on the solved photos, for the roof's and the floor's real surfaces. Prints whose focal length
    the solve left implausible (a tight crop) are left out: their depth would be wrong."""
    import subprocess

    import pycolmap

    model = os.path.join(OUT, 'model', 'models', '0')
    dense = os.path.join(OUT, 'dense')
    rec = pycolmap.Reconstruction(model)
    keep = []
    for im in rec.images.values():
        cam = rec.cameras[im.camera_id]
        k = cam.params[0] / max(cam.width, cam.height)
        if args.f_min <= k <= args.f_max:
            keep.append(im.name)
        else:
            print('left out %s, f/max %.2f' % (im.name, k), flush=True)

    def colmap(*cmd):
        print(' '.join(cmd), flush=True)
        subprocess.run([COLMAP] + list(cmd), check=True)

    colmap('image_undistorter', '--image_path', PHOTOS, '--input_path', model, '--output_path', dense,
           '--output_type', 'COLMAP', '--max_image_size', str(args.size))
    with open(os.path.join(dense, 'stereo', 'patch-match.cfg'), 'w', encoding='utf-8') as f:
        for n in sorted(keep):
            f.write('%s\n__auto__, %d\n' % (n, args.sources))
    with open(os.path.join(dense, 'stereo', 'fusion.cfg'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(sorted(keep)) + '\n')
    colmap('patch_match_stereo', '--workspace_path', dense, '--workspace_format', 'COLMAP',
           '--PatchMatchStereo.geom_consistency', 'true', '--PatchMatchStereo.max_image_size', str(args.size),
           '--PatchMatchStereo.gpu_index', '0', '--PatchMatchStereo.allow_missing_files', 'true')
    colmap('stereo_fusion', '--workspace_path', dense, '--workspace_format', 'COLMAP', '--input_type', 'geometric',
           '--output_path', os.path.join(dense, 'fused.ply'), '--StereoFusion.min_num_pixels', '3',
           '--StereoFusion.max_reproj_error', '2', '--StereoFusion.max_depth_error', '0.02')
    return 0


def similarity(A, B):
    """Umeyama: s, R, t with B ~ s R A + t, R a proper rotation (it stays one for coplanar points)."""
    import numpy as np

    ma, mb = A.mean(0), B.mean(0)
    a, b = A - ma, B - mb
    U, D, Vt = np.linalg.svd(b.T @ a / len(A))
    S = np.eye(3)
    if np.linalg.det(U) * np.linalg.det(Vt) < 0:
        S[2, 2] = -1
    R = U @ S @ Vt
    sa = (a ** 2).sum() / len(A)
    k = float(np.trace(np.diag(D) @ S) / sa)
    return k, R, mb - k * R @ ma


def face_points(rec, panel, photo='g092-a'):
    """Every solved point the photo sees on SP-2M2's face, with its body-frame place: the photo's camera solved on that traced
    face (out/recon/holistic/views/g092-a/camera.json, 1044 inliers) casts it onto the face's plane."""
    import numpy as np

    with open(os.path.join(REPO, 'out', 'recon', 'holistic', 'views', photo, 'camera.json'), encoding='utf-8') as f:
        c = json.load(f)['camera']
    im = [i for i in rec.images.values() if i.name[:-4] == photo][0]
    cam = rec.cameras[im.camera_id]
    k = c['size_px'][0] / cam.width
    R = np.array(c['R'])
    C = -R.T @ np.array(c['t'])
    W, H = panel['size_m']
    f_ = math.radians(panel['facing_deg'])
    n = np.array([math.sin(f_), math.cos(f_), 0.0])
    r = np.array([-math.cos(f_), math.sin(f_), 0.0])
    up = np.cross(n, r)
    O = np.array(panel['centre_m'])
    A, B = [], []
    for pt in rec.points3D.values():
        for el in pt.track.elements:
            if el.image_id != im.image_id:
                continue
            x, y = np.array(im.points2D[el.point2D_idx].xy) * k
            d = R.T @ np.array([(x - c['pp_px'][0]) / c['f_px'], (y - c['pp_px'][1]) / c['f_px'], 1.0])
            X = C + d * ((O - C) @ n) / (d @ n)
            if abs((X - O) @ r) <= W / 2 and abs((X - O) @ up) <= H / 2:
                A.append(pt.xyz)
                B.append(X)
    return np.array(A), np.array(B)


def cmd_align(args):
    """The solve into the body frame by SP-2M2's traced face (1.89 m across, Fig. 3), and only that: every other number in
    the result is the photos' own. Writes cameras.json and points.json."""
    import numpy as np
    import pycolmap

    rec = pycolmap.Reconstruction(os.path.join(OUT, 'model', 'models', '0'))
    with open(os.path.join(REPO, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8') as f:
        panel = [p for p in json.load(f)['panels'] if p['id'] == 'sp2m_panel'][0]
    A, B = face_points(rec, panel)
    print('%d solved points on the face' % len(A))
    keep = np.ones(len(A), bool)
    for thr in (0.10, 0.05, 0.03, 0.02):
        k, R, t = similarity(A[keep], B[keep])
        e = np.linalg.norm(k * (R @ A.T).T + t - B, axis=1)
        keep = e < thr
    print('scale %.4f, %d inliers, rms %.1f mm' % (k, keep.sum(), 1000 * math.sqrt((e[keep] ** 2).mean())))
    cams = {}
    for im in rec.images.values():
        c = rec.cameras[im.camera_id]
        w2c = im.cam_from_world() if callable(im.cam_from_world) else im.cam_from_world
        Rc = np.array(w2c.rotation.matrix())
        tc = np.array(w2c.translation)
        Rb = Rc @ R.T
        C = k * R @ (-Rc.T @ tc) + t
        fp, cx, cy, kk = c.params
        cams[im.name[:-4]] = {'f_px': float(fp), 'pp_px': [float(cx), float(cy)], 'R': Rb.tolist(), 't': (-Rb @ C).tolist(),
                              'size_px': [int(c.width), int(c.height)], 'k1': float(kk), 'eye_m': C.round(3).tolist(),
                              'points': int(im.num_points3D), 'f_over_max': round(float(fp) / max(c.width, c.height), 3)}
    pts = [[*(k * R @ p.xyz + t).round(4).tolist(), [rec.images[e.image_id].name[:-4] for e in p.track.elements]] for p in rec.points3D.values()]
    with open(os.path.join(OUT, 'cameras.json'), 'w', encoding='utf-8') as f:
        json.dump({'how': 'COLMAP on the book photos (tools/recon/holistic_sfm.py), to the body frame by SP-2M2 traced, through g092-a',
                   'scale': k, 'R': R.tolist(), 't': t.tolist(), 'face_points': int(keep.sum()), 'cameras': cams}, f, indent=1)
    with open(os.path.join(OUT, 'points.json'), 'w', encoding='utf-8') as f:
        json.dump(pts, f)
    for n, c in sorted(cams.items()):
        print('%-7s eye %-24s f/max %.2f  points %d' % (n, c['eye_m'], c['f_over_max'], c['points']))
    return 0


def read_ply(path):
    """COLMAP's fused.ply: positions and colours as arrays."""
    import numpy as np

    with open(path, 'rb') as f:
        n, props = 0, []
        while True:
            line = f.readline().decode('ascii').strip()
            if line.startswith('element vertex'):
                n = int(line.split()[2])
            elif line.startswith('property'):
                props.append((line.split()[2], {'float': '<f4', 'double': '<f8', 'uchar': 'u1'}[line.split()[1]]))
            elif line == 'end_header':
                break
        v = np.frombuffer(f.read(), dtype=props, count=n)
    return np.c_[v['x'], v['y'], v['z']].astype(float), np.c_[v['red'], v['green'], v['blue']]


def relief_map(X, C, pick, lo, hi, cell=0.02):
    """A plan of the points as heights and as a colour photograph from straight above, one level per cell (`pick`, the
    percentile of the cell's heights: a low one for the floor, so what stands on it falls away)."""
    import numpy as np

    x0, y0 = -1.8, -1.4
    nx, ny = int(3.6 / cell), int(6.4 / cell)
    i = ((X[:, 0] - x0) / cell).astype(int)
    j = ((X[:, 1] - y0) / cell).astype(int)
    keep = (i >= 0) & (i < nx) & (j >= 0) & (j < ny) & (X[:, 2] >= lo) & (X[:, 2] <= hi)
    cells = {}
    for a, b, k in zip(i[keep], j[keep], np.flatnonzero(keep)):
        cells.setdefault((a, b), []).append(k)
    Z = np.full((ny, nx), np.nan)
    N = np.zeros((ny, nx), int)
    RGB = np.zeros((ny, nx, 3), np.uint8)
    for (a, b), k in cells.items():
        z = X[k, 2]
        m = np.percentile(z, pick)
        Z[b, a] = m
        N[b, a] = len(k)
        RGB[b, a] = np.median(C[k][np.abs(z - m) <= max(0.02, z.std())], axis=0)
    return Z, N, RGB


def draw_map(Z, RGB, lo, hi, path, title, scale=5):
    """The map at 2 cm a cell, the forward direction up, a line every 20 cm, the numbers in metres."""
    import cv2
    import numpy as np

    v = np.nan_to_num((Z - lo) / (hi - lo), nan=0.0)
    h = cv2.applyColorMap((np.clip(v, 0, 1) * 255).astype(np.uint8), cv2.COLORMAP_TURBO)
    h[np.isnan(Z)] = 255
    photo = RGB[:, :, ::-1].copy()
    photo[np.isnan(Z)] = 255
    both = []
    for g in (h, photo):
        g = cv2.resize(g[::-1], None, fx=scale, fy=scale, interpolation=cv2.INTER_NEAREST)
        for x in np.arange(-1.8, 1.81, 0.2):
            c = int((x + 1.8) / 0.02 * scale)
            cv2.line(g, (c, 0), (c, g.shape[0]), (150, 150, 150), 1)
            cv2.putText(g, '%.1f' % x, (c + 2, 16), 0, 0.4, (0, 0, 0), 1)
        for y in np.arange(-1.4, 5.0, 0.2):
            r = g.shape[0] - int((y + 1.4) / 0.02 * scale)
            cv2.line(g, (0, r), (g.shape[1], r), (150, 150, 150), 1)
            cv2.putText(g, '%.1f' % y, (2, r - 2), 0, 0.4, (0, 0, 0), 1)
        both.append(g)
    out = np.hstack(both)
    cv2.putText(out, title, (40, 40), 0, 0.7, (0, 0, 0), 2)
    cv2.imwrite(path, out)


def cmd_relief(args):
    """The fused points in the body frame, as maps of the floor and the roof: heights beside a photograph from directly above
    or below, and numbers across the width at every 20 cm along the compartment. Writes relief.json."""
    import numpy as np

    with open(os.path.join(OUT, 'cameras.json'), encoding='utf-8') as f:
        a = json.load(f)
    X, C = read_ply(os.path.join(OUT, 'dense', 'fused.ply'))
    X = a['scale'] * (np.array(a['R']) @ X.T).T + np.array(a['t'])
    print('%d points, z 1/99 percent %s' % (len(X), np.percentile(X[:, 2], [1, 99]).round(3)))
    Zf, Nf, Cf = relief_map(X, C, 25, 0.3, 1.1)
    Zr, Nr, Cr = relief_map(X, C, 75, 1.75, 2.5)
    draw_map(Zf, Cf, 0.45, 1.05, os.path.join(OUT, 'floor_map.png'), 'floor: height 0.45-1.05 m')
    draw_map(Zr, Cr, 1.85, 2.4, os.path.join(OUT, 'roof_map.png'), 'roof: height 1.85-2.40 m')
    out = {'floor': {}, 'roof': {}}
    for name, Z in (('floor', Zf), ('roof', Zr)):
        for y in np.arange(-1.0, 4.2, 0.2):
            j = int((y + 1.4) / 0.02)
            strip = Z[j - 2:j + 3]
            row = {}
            for x in np.arange(-1.4, 1.41, 0.1):
                i = int((x + 1.8) / 0.02)
                c = strip[:, i - 2:i + 3]
                if np.isfinite(c).sum() >= 3:
                    row['%.1f' % x] = round(float(np.nanmedian(c)), 3)
            out[name]['%.1f' % y] = row
    with open(os.path.join(OUT, 'relief.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f)
    return 0


def cmd_summary(args):
    p = os.path.join(OUT, 'model')
    print(sorted(os.listdir(p)) if os.path.isdir(p) else 'no model yet')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run')
    r.add_argument('--keypoints', type=int, default=8192)
    r.add_argument('--resize', type=int, default=2400)
    r.add_argument('--min-matches', type=int, default=25)
    r.set_defaults(fn=cmd_run)
    m = sub.add_parser('map')
    m.add_argument('--min-matches', type=int, default=25)
    m.set_defaults(fn=cmd_map)
    d = sub.add_parser('dense')
    d.add_argument('--size', type=int, default=2000)
    d.add_argument('--sources', type=int, default=12)
    d.add_argument('--f-min', type=float, default=0.6)
    d.add_argument('--f-max', type=float, default=2.2)
    d.set_defaults(fn=cmd_dense)
    al = sub.add_parser('align')
    al.set_defaults(fn=cmd_align)
    r = sub.add_parser('relief')
    r.set_defaults(fn=cmd_relief)
    s = sub.add_parser('summary')
    s.set_defaults(fn=cmd_summary)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == '__main__':
    sys.exit(main())
