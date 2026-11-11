"""Probe for the new process's camera stage (docs/OSA_MODELLING_PROCESS.md, stage 1): the joint solve adjusted again
with the faces traced in millimetres as fixed control, and tested on control it was not given.

The solve before holistic_refine.py is loaded into the cabin's frame. Every match between a millimetre flat and a photo
(camera_audit.find_face) becomes a fixed 3-D point seen in that photo. COLMAP's bundle adjustment then moves cameras,
focal lengths, lens terms and tie points; the control points stay. Runs:

    all       every control match in
    sp2m      SP-2M2 only: the 0096's door is then a check of its place and of the cameras that see it
    minus X   everything except view X's control: view X is then a view "with no traced face", and its control says how good such a view is

    python tools/recon/audit/probe_cameras.py [--pp]

Out: out/recon/audit/probe_cameras.json, control.npz (the matches, kept).
"""
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import camera_audit as CA  # noqa: E402
import common as A  # noqa: E402
from common import V  # noqa: E402

MODEL = os.path.join(A.REPO, 'out', 'recon', 'holistic', 'sfm', 'model', 'models', '0')
ALIGN = os.path.join(A.REPO, 'out', 'recon', 'holistic', 'sfm', 'cameras.json')
CONTROL = os.path.join(A.OUT, 'control.npz')
SKIP = ('g093-b',)        # square-on to one plane, its 35 tie points are false matches to the other wall: a face photo, not a camera
PER_FACE = 300


def control():
    """Every (view, face) with matches on the face: pixels and body points."""
    if os.path.exists(CONTROL):
        z = np.load(CONTROL, allow_pickle=True)
        return list(z['sets'])
    faces = A.faces()
    flats = {k: cv2.imread(f.image) for k, f in faces.items()}
    sets = []
    for name in A.registered_views():
        img = A.photo(name)
        cam = A.reg_camera(name, img.shape[1], img.shape[0])
        for fname, face in faces.items():
            n, fl, px, how = CA.find_face(face, flats[fname], img, cam)
            if n >= CA.MIN_ON_FACE:
                sets.append({'view': name, 'face': fname, 'px': px, 'X': face.body(fl), 'flat': fl})
                print('control', name, fname, n, flush=True)
    np.savez_compressed(CONTROL, sets=np.array(sets, dtype=object))
    return sets


def load():
    import pycolmap
    a = json.load(open(ALIGN))
    rec = pycolmap.Reconstruction(MODEL)
    rec.transform(pycolmap.Sim3d(a['scale'], pycolmap.Rotation3d(np.array(a['R'])), np.array(a['t'])))
    return rec, {im.name[:-4]: i for i, im in rec.images.items()}


def camera_of(rec, image_id):
    im = rec.images[image_id]
    c = rec.cameras[im.camera_id]
    M = im.cam_from_world().matrix() if callable(im.cam_from_world) else im.cam_from_world.matrix()
    f, cx, cy, k = c.params
    return V.Camera(f, cx, cy, M[:3, :3], M[:3, 3], c.width, c.height, k)


def adjust(sets, use, refine_pp=False, seed=0):
    """Bundle adjustment with the control sets for which use(set) is true. Returns the reconstruction and the names."""
    import pycolmap
    rec, names = load()
    cfg = pycolmap.BundleAdjustmentConfig()
    for name, i in names.items():
        if name not in SKIP:
            cfg.add_image(i)
    tie = list(rec.point3D_ids())
    rng = np.random.default_rng(seed)
    n_ctrl = 0
    for s in sets:
        if not use(s) or s['view'] in SKIP:
            continue
        im = rec.images[names[s['view']]]
        pick = rng.choice(len(s['px']), min(PER_FACE, len(s['px'])), replace=False)
        for j in pick:
            idx = im.num_points2D()
            im.points2D.append(pycolmap.Point2D(s['px'][j].astype(np.float64)))
            im.points2D.append(pycolmap.Point2D(s['px'][j].astype(np.float64)))      # COLMAP's adjuster wants two sightings of a point: the same one twice
            pid = rec.add_point3D(s['X'][j].astype(np.float64), pycolmap.Track([pycolmap.TrackElement(im.image_id, idx), pycolmap.TrackElement(im.image_id, idx + 1)]))
            cfg.add_constant_point(pid)
            n_ctrl += 1
    for pid in tie:
        cfg.add_variable_point(pid)
    o = pycolmap.BundleAdjustmentOptions()
    o.refine_focal_length, o.refine_extra_params, o.refine_principal_point, o.print_summary = True, True, refine_pp, False
    o.ceres.loss_function_type = pycolmap.LossFunctionType.CAUCHY
    o.ceres.loss_function_scale = 2.0
    o.ceres.solver_options.max_num_iterations = 200
    pycolmap.create_default_bundle_adjuster(o, cfg, rec).solve()
    return rec, names, n_ctrl


def residuals(rec, names, sets):
    out = {}
    for s in sets:
        if s['view'] in SKIP:
            continue
        cam = camera_of(rec, names[s['view']])
        d = np.linalg.norm(A.project(cam, s['X']) - s['px'], axis=1)
        out['%s %s' % (s['view'], s['face'])] = round(float(np.nanmedian(d)), 1)
    return out


def tie_error(rec, names):
    """Median reprojection error of the tie points, per view and overall (px)."""
    per = {}
    for name, i in names.items():
        if name in SKIP:
            continue
        im = rec.images[i]
        cam = camera_of(rec, i)
        e = []
        for p in im.points2D:
            if p.has_point3D() and rec.point3D(p.point3D_id).track.length() >= 2:
                e.append(np.linalg.norm(A.project(cam, rec.point3D(p.point3D_id).xyz)[0] - p.xy))
        if e:
            per[name] = float(np.median(e))
    return per


def rigid_fit(rec, names, sets, face):
    """The face as a rigid plate, free to move and scale, fitted to its matches through the given cameras: what is left is the cameras' own disagreement; the move is how far the cabin has the face from where these cameras put it."""
    from scipy.optimize import least_squares
    ss = [s for s in sets if s['face'] == face and s['view'] not in SKIP]
    cams = [camera_of(rec, names[s['view']]) for s in ss]
    c0 = np.mean(np.vstack([s['X'] for s in ss]), axis=0)

    def moved(q, X):
        R = cv2.Rodrigues(q[:3])[0]
        return np.exp(q[6]) * (R @ (X - c0).T).T + c0 + q[3:6]

    def fun(q):
        return np.concatenate([(A.project(c, moved(q, s['X'])) - s['px']).ravel() for c, s in zip(cams, ss)])
    r = least_squares(fun, np.zeros(7), loss='huber', f_scale=3.0)
    per = {s['view']: round(float(np.median(np.linalg.norm((A.project(c, moved(r.x, s['X'])) - s['px']), axis=1))), 1) for c, s in zip(cams, ss)}
    return {'left_px': per, 'moved_m': [round(float(v), 3) for v in r.x[3:6]], 'turned_deg': round(float(np.degrees(np.linalg.norm(r.x[:3]))), 2), 'scale': round(float(np.exp(r.x[6])), 4)}, (lambda X: moved(r.x, X))


def main(argv):
    pp = '--pp' in argv
    sets = control()
    out = {'principal_point_free': pp}
    rec, names = load()
    out['solve as it was'] = {'control_px': residuals(rec, names, sets), 'tie_px': round(float(np.median(list(tie_error(rec, names).values()))), 2)}
    print('solve as it was      ', out['solve as it was'])
    # 1. the frame's face alone as fixed control; the door, placed through the old cameras, is only looked at
    rec, names, n = adjust(sets, lambda s: s['face'] == 'sp2m', pp)
    fit, move = rigid_fit(rec, names, sets, 'cab0096')
    out['SP-2M2 fixed'] = {'control_px': residuals(rec, names, sets), 'tie_px': round(float(np.median(list(tie_error(rec, names).values()))), 2), 'the 0096 door as a free rigid plate': fit}
    print('SP-2M2 fixed         ', out['SP-2M2 fixed'])
    # 2. each SP-2M2 view in turn without its control: what a view with no traced face can expect
    out['SP-2M2 fixed, one view left out'] = {}
    for v in sorted({s['view'] for s in sets if s['face'] == 'sp2m' and s['view'] not in SKIP}):
        r2, n2, _ = adjust(sets, lambda s, v=v: s['face'] == 'sp2m' and s['view'] != v, pp)
        out['SP-2M2 fixed, one view left out'][v] = residuals(r2, n2, sets)['%s sp2m' % v]
        print('left out %-8s its SP-2M2 then lands %5.1f px off' % (v, out['SP-2M2 fixed, one view left out'][v]), flush=True)
    # 3. the door as a rigid plate whose place is solved: fitted, put in as control where it was fitted, adjusted, fitted again
    cur = sets
    for it in range(3):
        cur = [dict(s, X=move(s['X'])) if s['face'] == 'cab0096' else s for s in cur]
        rec, names, n = adjust(cur, lambda s: True, pp)
        fit, move = rigid_fit(rec, names, cur, 'cab0096')
    te = tie_error(rec, names)
    c0 = np.mean(np.vstack([s['X'] for s in sets if s['face'] == 'cab0096']), axis=0)
    c1 = np.mean(np.vstack([s['X'] for s in cur if s['face'] == 'cab0096']), axis=0)
    w0 = [s for s in sets if s['face'] == 'cab0096'][0]
    w1 = [s for s in cur if s['face'] == 'cab0096'][0]
    scale = float(np.linalg.norm(w1['X'][0] - w1['X'][-1]) / np.linalg.norm(w0['X'][0] - w0['X'][-1]))
    out['SP-2M2 fixed, the door a rigid plate solved with the cameras'] = {'control_px': residuals(rec, names, cur), 'tie_px': round(float(np.median(list(te.values()))), 2), 'tie_px_worst': [max(te, key=te.get), round(max(te.values()), 2)],
                                                                             'door_moved_m': (c1 - c0).round(3).tolist(), 'door_scale': round(scale, 4), 'last_fit': fit}
    print('door solved with it  ', out['SP-2M2 fixed, the door a rigid plate solved with the cameras'])
    cams = {name: camera_of(rec, i).to_dict() for name, i in names.items() if name not in SKIP}
    moved_by = {}
    old = V.load(ALIGN)['cameras']
    for name, c in cams.items():
        moved_by[name] = round(float(np.linalg.norm(V.Camera.from_dict(c).centre() - V.Camera.from_dict(old[name]).centre())), 3)
    out['cameras moved from the solve as it was, m'] = {'median': float(np.median(list(moved_by.values()))), 'max': max(moved_by.values())}
    with open(os.path.join(A.OUT, 'probe_cameras_solved%s.json' % ('_pp' if pp else '')), 'w', encoding='utf-8') as f:
        json.dump({'how': 'probe_cameras.py: the joint solve adjusted with SP-2M2 as fixed control and the 0096 door as a rigid plate', 'cameras': cams}, f, indent=1)
    with open(os.path.join(A.OUT, 'probe_cameras%s.json' % ('_pp' if pp else '')), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)


if __name__ == '__main__':
    main(sys.argv[1:])
