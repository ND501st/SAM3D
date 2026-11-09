"""Reconstruction orchestrator: reference photographs + a reference mesh -> a sourced, game-ready vehicle model.

docs/OSA_EXTERIOR_RECONSTRUCTION.md is the plan this script runs. Every stage reads the outputs of the stages before it
from the run folder, writes its own, and records itself in `manifest.json`: a stage whose inputs (its config section,
its code, the outputs it depends on) haven't changed is skipped on the next run, so a run can be stopped and resumed
anywhere. Heavy tools run headless as subprocesses (COLMAP, SAM 2, Depth Anything V2, 2DGS, Blender, FreeCAD), each
with a timeout and a log; a missing tool makes its stage fall back (SAM 2 -> GrabCut; COLMAP -> cameras against the
prior) or skip (dense reconstruction, CAD), and says so.

    python tools/recon/osa_recon.py run    tools/recon/osa_9a33bm2.json             # everything that is due
    python tools/recon/osa_recon.py run    tools/recon/osa_9a33bm2.json --only frames,descreen
    python tools/recon/osa_recon.py run    tools/recon/osa_9a33bm2.json --force cameras
    python tools/recon/osa_recon.py status tools/recon/osa_9a33bm2.json
    python tools/recon/osa_recon.py tools  tools/recon/osa_9a33bm2.json             # which external tools are found

Error recovery, in order: (1) per-item isolation: one bad photo is recorded and the stage goes on, and the stage fails
only above the config's `max_item_failure`; (2) retries with backoff for transient failures, and on a CUDA
out-of-memory the stage's `degrade` is applied (half the resolution) before the retry; (3) a fallback strategy;
(4) an optional stage that cannot run is `skipped` with the reason, and stages that need it go on without it.
Run outputs go under out/recon/<name>/ (git-ignored); the book's scans stay in reference/ or where the config says.
"""
import argparse
import hashlib
import inspect
import json
import os
import shutil
import subprocess
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)


# ---------------------------------------------------------------------------------------------------- the framework

class StageSkipped(Exception):
    """An optional stage that cannot or need not run (its reason goes into the manifest)."""


class OutOfMemory(Exception):
    """A GPU stage ran out of memory: retry after `degrade`."""


class Stage:
    def __init__(self, name, fn, deps=(), optional=False, retries=1, degrade=None, title=''):
        self.name, self.fn, self.deps, self.optional = name, fn, tuple(deps), optional
        self.retries, self.degrade, self.title = retries, degrade, title

    def code_hash(self):
        return hashlib.sha1(inspect.getsource(self.fn).encode('utf-8')).hexdigest()[:12]


class Run:
    """One reconstruction run: config, run folder, manifest, logs, external tools."""

    def __init__(self, cfg_path):
        self.cfg_path = os.path.abspath(cfg_path)
        with open(cfg_path, encoding='utf-8') as f:
            self.cfg = json.load(f)
        self.dir = os.path.join(REPO, self.cfg.get('out_dir', os.path.join('out', 'recon', self.cfg['name'])))
        os.makedirs(os.path.join(self.dir, 'logs'), exist_ok=True)
        self.manifest_path = os.path.join(self.dir, 'manifest.json')
        self.manifest = json.load(open(self.manifest_path, encoding='utf-8')) if os.path.exists(self.manifest_path) else {}
        self._log = None

    # paths and files
    def path(self, *parts):
        p = os.path.join(self.dir, *parts)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        return p

    def repo_path(self, p):
        return p if os.path.isabs(p) else os.path.join(REPO, p)

    def read(self, name):
        with open(self.path(name), encoding='utf-8') as f:
            return json.load(f)

    def write(self, name, obj):
        tmp = self.path(name) + '.tmp'
        with open(tmp, 'w', encoding='utf-8') as f:
            json.dump(obj, f, indent=1, ensure_ascii=False)
        os.replace(tmp, self.path(name))  # atomic: a crash never leaves half a file for the next stage

    def save_manifest(self):
        tmp = self.manifest_path + '.tmp'
        with open(tmp, 'w', encoding='utf-8') as f:
            json.dump(self.manifest, f, indent=1)
        os.replace(tmp, self.manifest_path)

    def log(self, msg):
        line = time.strftime('%H:%M:%S ') + msg
        print(line, flush=True)
        if self._log:
            self._log.write(line + '\n')
            self._log.flush()

    # external tools
    def tool(self, key):
        """The configured path of an external tool, or None if it isn't there."""
        p = self.cfg.get('tools', {}).get(key)
        if not p:
            return None
        p = self.repo_path(p) if not os.path.isabs(p) and os.sep in p else p
        return p if (os.path.exists(p) or shutil.which(p)) else None

    def call(self, args, timeout_s, stage, cwd=None):
        """Run a tool headless with a timeout; its output goes to the stage's log. Raises on failure, OutOfMemory on
        a CUDA out-of-memory message so the stage can degrade and retry."""
        self.log('$ ' + ' '.join(str(a) for a in args))
        logp = self.path('logs', stage + '.tool.log')
        with open(logp, 'a', encoding='utf-8', errors='replace') as lf:
            try:
                p = subprocess.run([str(a) for a in args], cwd=cwd, stdout=lf, stderr=subprocess.STDOUT, timeout=timeout_s)
            except subprocess.TimeoutExpired:
                raise RuntimeError('%s timed out after %d s' % (os.path.basename(str(args[0])), timeout_s))
        if p.returncode != 0:
            tail = open(logp, encoding='utf-8', errors='replace').read()[-2000:]
            if 'out of memory' in tail.lower():
                raise OutOfMemory(tail[-300:])
            raise RuntimeError('%s exited %d (log: %s)' % (os.path.basename(str(args[0])), p.returncode, logp))

    # stage bookkeeping
    def inputs_hash(self, st):
        h = hashlib.sha1()
        h.update(st.code_hash().encode())
        h.update(json.dumps(self.cfg.get('stages', {}).get(st.name, {}), sort_keys=True).encode())
        h.update(json.dumps({k: self.cfg.get(k) for k in ('sources', 'dimensions', 'tools')}, sort_keys=True).encode())
        for d in st.deps:
            h.update(str(self.manifest.get(d, {}).get('outputs_hash', '')).encode())
        return h.hexdigest()[:16]

    def item_failures(self, stage, failures, total):
        """Per-item isolation: record failures, fail the stage only above the configured share."""
        limit = self.cfg.get('max_item_failure', 0.5)
        if failures:
            self.write(os.path.join('reports', stage + '_failures.json'), failures)
        if total and len(failures) / total > limit:
            raise RuntimeError('%d of %d items failed (limit %.0f %%)' % (len(failures), total, 100 * limit))


def outputs_hash(run, outputs):
    h = hashlib.sha1()
    for p in sorted(outputs or []):
        full = run.path(p)
        if os.path.isfile(full):
            st = os.stat(full)
            h.update(('%s:%d:%d' % (p, st.st_size, int(st.st_mtime))).encode())
        elif os.path.isdir(full):
            for root, _, files in os.walk(full):
                for fn in sorted(files):
                    st = os.stat(os.path.join(root, fn))
                    h.update(('%s/%s:%d' % (root, fn, st.st_size)).encode())
    return h.hexdigest()[:16]


def execute(run, stages, only=None, force=(), stop_after=None):
    order = {s.name: s for s in stages}
    for st in stages:
        if only and st.name not in only:
            continue
        rec = run.manifest.get(st.name, {})
        ih = run.inputs_hash(st)
        blocked = [d for d in st.deps if run.manifest.get(d, {}).get('status') not in ('done', 'skipped')]
        if blocked and not st.optional:
            run.log('[%s] blocked by %s' % (st.name, ', '.join(blocked)))
            run.manifest[st.name] = dict(rec, status='blocked', blocked_by=blocked)
            run.save_manifest()
            continue
        skipped = [d for d in st.deps if run.manifest.get(d, {}).get('status') == 'skipped']
        if skipped:  # a stage whose input was skipped has nothing to work on: it skips too, with the reason
            run.log('[%s] skipped: needs %s' % (st.name, ', '.join(skipped)))
            run.manifest[st.name] = {'status': 'skipped', 'note': 'needs %s (skipped)' % ', '.join(skipped),
                                     'inputs_hash': ih, 'outputs': [], 'outputs_hash': '', 'metrics': {}}
            run.save_manifest()
            continue
        if rec.get('status') == 'done' and rec.get('inputs_hash') == ih and st.name not in force:
            run.log('[%s] up to date' % st.name)
            continue
        run._log = open(run.path('logs', st.name + '.log'), 'a', encoding='utf-8')
        run.log('[%s] %s' % (st.name, st.title))
        attempt, opts = 0, dict(run.cfg.get('stages', {}).get(st.name, {}))
        while True:
            attempt += 1
            t0 = time.time()
            try:
                result = st.fn(run, opts) or {}
                status = 'done'
                note = result.get('note', '')
            except StageSkipped as e:
                result, status, note = {}, 'skipped', str(e)
            except OutOfMemory as e:
                if st.degrade and attempt <= st.retries:
                    opts = st.degrade(opts)
                    run.log('[%s] out of memory, retrying degraded: %s' % (st.name, opts))
                    continue
                result, status, note = {}, 'failed', 'out of memory: ' + str(e)[:200]
            except Exception as e:  # noqa: BLE001 - every failure is recorded, then retried or reported
                run.log(traceback.format_exc())
                if attempt <= st.retries:
                    wait = 2 ** attempt
                    run.log('[%s] attempt %d failed (%s); retrying in %d s' % (st.name, attempt, e, wait))
                    time.sleep(wait)
                    continue
                result, status, note = {}, 'failed', str(e)[:500]
            break
        outs = result.get('outputs', [])
        run.manifest[st.name] = {
            'status': status, 'note': note, 'attempts': attempt, 'seconds': round(time.time() - t0, 1),
            'inputs_hash': ih, 'outputs': outs, 'outputs_hash': outputs_hash(run, outs),
            'metrics': result.get('metrics', {}), 'finished': time.strftime('%Y-%m-%d %H:%M:%S'),
        }
        run.save_manifest()
        run.log('[%s] %s%s' % (st.name, status, (': ' + note) if note else ''))
        run._log.close()
        run._log = None
        if status == 'failed' and not st.optional:
            run.log('stopping: %s failed' % st.name)
            return False
        if stop_after and st.name == stop_after:
            break
    return True


# --------------------------------------------------------------------------------------------------------- stages

def st_pages(run, o):
    """S0: the book's scans to upright page images."""
    import book
    src = run.cfg['sources']['book']
    pdfs = [run.repo_path(p) for p in src['pdfs']]
    missing = [p for p in pdfs if not os.path.exists(p)]
    if missing:
        raise RuntimeError('scans not found: %s' % missing)
    index = book.extract_pages(pdfs, run.path('book'), src.get('turn', 'even'), o.get('view_px', 1600), run.log)
    run.write('book/index.json', index)
    return {'outputs': ['book/index.json'], 'metrics': {'pages': len(index)}}


def st_catalogue(run, o):
    """S1: the photo catalogue from the readers' notes (ids, frames, subject, vehicle, view)."""
    import book
    notes = run.repo_path(run.cfg['sources']['book']['notes'])
    recs = book.load_catalogue(notes)
    if not recs:
        raise RuntimeError('no photo records in %s' % notes)
    groups = book.groups_by_vehicle(recs)
    run.write('catalogue.json', recs)
    run.write('groups.json', groups)
    with_box = sum(1 for r in recs if 'bbox' in r)
    return {'outputs': ['catalogue.json', 'groups.json'],
            'metrics': {'photos': len(recs), 'with_bbox': with_box, 'vehicles': len(groups),
                        'solve_candidates': sum(1 for r in recs if r.get('solve'))}}


def st_frames(run, o):
    """S2: every photo cut out of its page at full resolution: the catalogue's box, else the detector's."""
    import cv2
    import book
    cat = run.read('catalogue.json')
    by_page = {}
    for r in cat:
        by_page.setdefault(r['page'], []).append(r)
    index = {e['page']: e for e in run.read('book/index.json')}
    failures, n, recall = [], 0, []
    for page, recs in sorted(by_page.items()):
        e = index.get(page)
        view = cv2.imread(run.path('book', 'view', '%03d.jpg' % page))
        if e is None or view is None:
            failures.append({'page': page, 'error': 'page not extracted'})
            continue
        todo = [r for r in recs if not os.path.exists(run.path('photos', 'raw', r['photo'] + '.jpg'))]
        full = book.load_page(e['pdf'], e['index'], e['turned'])[..., ::-1] if todo else None
        found = book.find_frames(view)
        truth = [r['bbox'] for r in recs if 'bbox' in r]
        recall.append(book.frame_recall(found, truth))
        for r in recs:
            n += 1
            if r not in todo:
                continue
            H, W = full.shape[:2]
            box = r.get('bbox')
            if not box:  # take the detected frame nearest the reader's "where" (top left ...), else give up
                failures.append({'photo': r['photo'], 'error': 'no bbox in the catalogue'})
                continue
            pad = o.get('inset', 0.004)  # stay inside the frame's printed edge
            x0, y0 = int((box[0] + pad) * W), int((box[1] + pad) * H)
            x1, y1 = int((box[2] - pad) * W), int((box[3] - pad) * H)
            if x1 - x0 < 50 or y1 - y0 < 50:
                failures.append({'photo': r['photo'], 'error': 'box too small'})
                continue
            # JPEG at 95: the scan is a JPEG already, and PNG crops of ~450 photos would take ~5 GB
            cv2.imwrite(run.path('photos', 'raw', r['photo'] + '.jpg'), full[y0:y1, x0:x1].copy(),  # contiguous for OpenCV
                        [cv2.IMWRITE_JPEG_QUALITY, 95])
    run.item_failures('frames', failures, n)
    return {'outputs': ['photos/raw'],
            'metrics': {'photos': n - len(failures), 'failed': len(failures),
                        'detector_recall_at_iou_0.8': round(sum(recall) / max(1, len(recall)), 3)}}


def st_descreen(run, o):
    """S3: the halftone screen out of every photo (low-pass at the measured screen frequency, resampled)."""
    import cv2
    import book
    src = run.path('photos', 'raw')
    names = sorted(n for n in os.listdir(src) if n.endswith('.jpg'))
    periods, failures = [], []
    for name in names:
        out = run.path('photos', 'clean', name[:-4] + '.png')
        if os.path.exists(out) and not o.get('redo'):
            continue
        img = cv2.imread(os.path.join(src, name))
        g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        h, w = g.shape
        patch = g[h // 2 - 256:h // 2 + 256, w // 2 - 256:w // 2 + 256] if min(h, w) > 600 else g
        per, _ = book.screen_period(patch)
        if not per or not (2.0 < per < 8.0):  # not a halftone (a drawing, or a screen too fine to see)
            per = o.get('period_px', 3.0)
        periods.append(per)
        cv2.imwrite(out, book.descreen(img, per))
    run.item_failures('descreen', failures, len(names))
    return {'outputs': ['photos/clean'],
            'metrics': {'photos': len(names), 'median_period_px': round(sorted(periods)[len(periods) // 2], 2) if periods else None}}


def st_masks(run, o):
    """S4: the vehicle's mask in each whole-vehicle photo: SAM 2 when installed, else GrabCut."""
    import cv2
    import geometry as G
    cat = {r['photo']: r for r in run.read('catalogue.json')}
    names = [n for n in sorted(os.listdir(run.path('photos', 'clean'))) if cat.get(n[:-4], {}).get('solve')]
    sam2 = run.tool('sam2_python')
    if sam2 and o.get('use_sam2', True):
        listp = run.path('masks', 'todo.txt')
        open(listp, 'w').write('\n'.join(os.path.join(run.path('photos', 'clean'), n) for n in names))
        run.call([sam2, os.path.join(HERE, 'ext_sam2_masks.py'), listp, run.path('masks'), '--prompt', 'vehicle'],
                 o.get('timeout_s', 3600), 'masks')
        method = 'sam2'
    else:
        method = 'grabcut'
        for n in names:
            out = run.path('masks', n)
            if not os.path.exists(out):
                cv2.imwrite(out, G.grabcut_mask(cv2.imread(run.path('photos', 'clean', n))))
    return {'outputs': ['masks'], 'metrics': {'masks': len(names), 'method': method},
            'note': '' if method == 'sam2' else 'SAM 2 not configured: GrabCut masks (check them)'}


def st_depth(run, o):
    """S5 (optional): monocular depth and normals per photo (Depth Anything V2): priors for flat plates and thin parts."""
    py = run.tool('depth_python')
    if not py:
        raise StageSkipped('depth_python not configured (Depth Anything V2 venv)')
    run.call([py, os.path.join(HERE, 'ext_depth.py'), run.path('photos', 'clean'), run.path('depth'),
              '--encoder', o.get('encoder', 'vitl'), '--max-px', str(o.get('max_px', 1024))], o.get('timeout_s', 3600), 'depth')
    return {'outputs': ['depth']}


def st_prior(run, o):
    """S6: the reference mesh in the vehicle frame, cut into parts and scaled to the sourced dimensions."""
    import numpy as np
    import geometry as G
    ref = run.cfg['sources']['reference_mesh']
    V, F, groups = G.load_obj(run.repo_path(ref['obj']))
    P = G.Prior(V, F, groups, ref.get('parts', {}), tuple(ref.get('axes', ['-x', 'z', 'y'])), ref.get('front_sign', 1.0))
    raw = P.size().tolist()
    dims = run.cfg.get('dimensions', {})
    s = P.scaled_to(length_m=dims.get('hull_length_m', {}).get('value'), width_m=dims.get('hull_width_m', {}).get('value'))
    P.sampled(o.get('samples', 150000))
    if 'turret_pivot_m' in ref:
        P.pivots['turret'] = ref['turret_pivot_m']
    np.savez_compressed(run.path('prior.npz'), S=P.S.astype(np.float32), S_part=P.S_part.astype(str),
                        size=P.size(), pivots=json.dumps(P.pivots))
    note = ''
    if abs(s[0] - 1) > 0.05 or abs(s[1] - 1) > 0.05:
        note = 'reference mesh rescaled by %.3f x %.3f: check its overall size against the sources' % (s[0], s[1])
    return {'outputs': ['prior.npz'], 'note': note,
            'metrics': {'raw_size_m': [round(x, 3) for x in raw], 'scale': [round(x, 4) for x in s.tolist()],
                        'samples': int(len(P.S))}}


def load_prior(run):
    import numpy as np
    import geometry as G
    z = np.load(run.path('prior.npz'))
    P = G.Prior.__new__(G.Prior)
    P.S, P.S_part, P.pivots = z['S'].astype(float), z['S_part'].astype(object), json.loads(str(z['pivots']))
    P.V, P.F, P.part = P.S, None, P.S_part
    return P


def st_cameras(run, o):
    """S7: a camera for every whole-vehicle photo: from keypoints (PnP) when marked, else by silhouette against the
    prior from the catalogue's view, mirrored photos found on the way. COLMAP runs first on vehicle groups big
    enough for it, and its cameras win where it registers a photo."""
    import cv2
    import geometry as G
    cat = {r['photo']: r for r in run.read('catalogue.json')}
    P = load_prior(run)
    kp_path = run.repo_path(run.cfg['sources']['book'].get('keypoints', '')) if run.cfg['sources']['book'].get('keypoints') else None
    keypoints = json.load(open(kp_path, encoding='utf-8')) if kp_path and os.path.exists(kp_path) else {}
    named3d = run.cfg.get('keypoints_3d', {})
    cams, failures, n = {}, [], 0
    if run.tool('colmap') and o.get('use_colmap', True):
        cams.update(colmap_groups(run, o))
    scale = o.get('fit_px', 480)  # silhouette fits work on a small copy; the camera is scaled back
    for name in sorted(os.listdir(run.path('masks'))):
        pid = name[:-4]
        if not name.endswith('.png') or pid in cams:
            continue
        n += 1
        try:
            img = cv2.imread(run.path('photos', 'clean', name))
            h, w = img.shape[:2]
            if pid in keypoints and len(keypoints[pid]) >= 6:
                pts = [(named3d[k], v) for k, v in keypoints[pid].items() if k in named3d]
                cam, rms = G.pnp([p[0] for p in pts], [p[1] for p in pts], w, h)
                cams[pid] = dict(cam.to_dict(), method='pnp', rms_px=round(rms, 2), mirrored=False)
                continue
            k = scale / max(w, h)
            sw, sh = int(w * k), int(h * k)
            mask = cv2.resize(cv2.imread(run.path('masks', name), cv2.IMREAD_GRAYSCALE), (sw, sh), interpolation=cv2.INTER_NEAREST)
            edt = G.edges_distance(cv2.resize(cv2.cvtColor(img, cv2.COLOR_BGR2GRAY), (sw, sh), interpolation=cv2.INTER_AREA))
            r = cat.get(pid, {})
            az = r.get('az_deg')
            if az is None:
                raise RuntimeError('no view azimuth in the catalogue')
            best = None
            for dist in o.get('start_distances_m', [10, 16, 25]):
                start = G.camera_from_view(az, r.get('height') or 'eye', dist, sw, sh, o.get('start_fov_deg', 40))
                cam, art, c, mirrored = G.fit_camera_or_mirror(P, mask, start, edt, iters=o.get('iters', 250),
                                                               free_art=tuple(o.get('free_articulation', [])))
                if best is None or c < best[2]:
                    best = (cam, art, c, mirrored)
            cam, art, c, mirrored = best
            cam.f, cam.cx, cam.cy, cam.w, cam.h = cam.f / k, cam.cx / k, cam.cy / k, w, h
            iou, ch = G.overlay_score(G.splat(P.posed(art), cam, w, h, close_px=5),
                                      cv2.imread(run.path('masks', name), cv2.IMREAD_GRAYSCALE))
            cams[pid] = dict(cam.to_dict(), method='silhouette', iou=round(iou, 3), articulation=art, mirrored=mirrored)
            if iou < o.get('min_iou', 0.75):
                failures.append({'photo': pid, 'error': 'silhouette IoU %.2f below %.2f' % (iou, o.get('min_iou', 0.75))})
        except Exception as e:  # noqa: BLE001 - one photo never stops the run
            failures.append({'photo': pid, 'error': str(e)[:300]})
    run.write('cameras.json', cams)
    run.item_failures('cameras', failures, n)
    return {'outputs': ['cameras.json'],
            'metrics': {'cameras': len(cams), 'failed': len(failures),
                        'mirrored': sum(1 for c in cams.values() if c.get('mirrored'))}}


def colmap_groups(run, o):
    """COLMAP on each vehicle group with enough photos; returns {photo: camera} for the photos it registered."""
    groups = run.read('groups.json')
    out = {}
    for key, photos in groups.items():
        if len(photos) < o.get('colmap_min_views', 12):
            continue
        work = run.path('colmap', key.replace(' ', '_').replace(':', '_')[:40])
        img_dir = os.path.join(work, 'images')
        os.makedirs(img_dir, exist_ok=True)
        for p in photos:
            src = run.path('photos', 'clean', p + '.png')
            if os.path.exists(src):
                shutil.copy(src, img_dir)
        db, sparse = os.path.join(work, 'db.db'), os.path.join(work, 'sparse')
        os.makedirs(sparse, exist_ok=True)
        cm = run.tool('colmap')
        try:
            run.call([cm, 'feature_extractor', '--database_path', db, '--image_path', img_dir,
                      '--ImageReader.camera_model', 'SIMPLE_RADIAL', '--ImageReader.single_camera', '0'], 1800, 'cameras')
            run.call([cm, 'exhaustive_matcher', '--database_path', db], 3600, 'cameras')
            run.call([cm, 'mapper', '--database_path', db, '--image_path', img_dir, '--output_path', sparse], 3600, 'cameras')
        except RuntimeError as e:
            run.log('COLMAP gave nothing for %s (%s): cameras against the prior instead' % (key, e))
            continue
        # COLMAP's cameras are in its own frame and scale: they are registered to the prior in st_fit (ICP of
        # the sparse points to the prior's surface, scale free), so here only the photo list is kept.
        model = os.path.join(sparse, '0')
        if os.path.isdir(model):
            out.update({p: {'method': 'colmap', 'model': model} for p in photos})
    return out


def st_dense(run, o):
    """S8 (optional): a dense surface (2DGS) for a capture session with enough views; book photos never have them."""
    groups = run.read('groups.json')
    big = {k: v for k, v in groups.items() if len(v) >= o.get('min_views', 40)}
    if not big:
        raise StageSkipped('no vehicle group has %d views: dense reconstruction needs a walk-round capture, not a book'
                           % o.get('min_views', 40))
    py = run.tool('gs2d_python')
    if not py:
        raise StageSkipped('gs2d_python not configured (2D Gaussian Splatting venv)')
    for key in big:
        work = run.path('colmap', key.replace(' ', '_').replace(':', '_')[:40])
        run.call([py, os.path.join(run.tool('gs2d_repo') or '.', 'train.py'), '-s', work, '-m', run.path('dense', key[:40]),
                  '--depth_ratio', '1', '--lambda_normal', '0.05', '-r', str(o.get('downscale', 2))], o.get('timeout_s', 14400), 'dense')
    return {'outputs': ['dense']}


def degrade_half(o):
    o = dict(o)
    o['downscale'] = o.get('downscale', 2) * 2
    o['max_px'] = int(o.get('max_px', 1024) / 2)
    return o


def st_fit(run, o):
    """S9: the prior's main dimensions refined on all cameras together (silhouettes), held by the sourced
    dimensions as priors. Writes fit.json: per-axis scale and the residual per photo."""
    import cv2
    import numpy as np
    import geometry as G
    cams = {k: v for k, v in run.read('cameras.json').items() if v.get('method') in ('silhouette', 'pnp')}
    if len(cams) < o.get('min_cameras', 3):
        raise StageSkipped('only %d cameras: the fit needs at least %d' % (len(cams), o.get('min_cameras', 3)))
    P = load_prior(run)
    V0 = P.S.copy()
    k = o.get('fit_px', 400)
    data = []
    for pid, c in cams.items():
        m = cv2.imread(run.path('masks', pid + '.png'), cv2.IMREAD_GRAYSCALE)
        if c.get('mirrored'):
            m = np.ascontiguousarray(m[:, ::-1])
        s = k / max(m.shape)
        cam = G.Camera.from_dict(c)
        cam.f, cam.cx, cam.cy = cam.f * s, cam.cx * s, cam.cy * s
        data.append((cam, cv2.resize(m, (int(m.shape[1] * s), int(m.shape[0] * s)), interpolation=cv2.INTER_NEAREST), c.get('articulation', {})))
    dims = run.cfg.get('dimensions', {})

    def cost(x):
        P.S = V0 * np.array([1 + x[0], 1 + x[1], 1 + x[2]])
        e = 0.0
        for cam, m, art in data:
            e += 1 - G.overlay_score(G.splat(P.posed(art), cam, m.shape[1], m.shape[0]), m)[0]
        e /= len(data)
        for i, key in enumerate(('hull_width_m', 'hull_length_m', 'height_march_m')):
            d = dims.get(key)
            if d and d.get('sigma_m'):
                size = (V0[:, i].max() - V0[:, i].min()) * (1 + x[i])
                e += 0.01 * ((size - d['value']) / d['sigma_m']) ** 2
        return e
    x, c = G.nelder_mead(cost, [0.0, 0.0, 0.0], [0.01, 0.01, 0.01], iters=o.get('iters', 120))
    run.write('fit.json', {'scale': [1 + v for v in x], 'cost': c, 'cameras': list(cams)})
    return {'outputs': ['fit.json'], 'metrics': {'scale': [round(1 + v, 4) for v in x], 'cost': round(c, 4)}}


def st_build(run, o):
    """S10: the game model in Blender, headless: the hull and parts from the fitted trace, parametric wheels from
    the tyre's sourced size, UVs, LODs, FBX into art/ (tools/recon/blender_build.py)."""
    blender = run.tool('blender')
    if not blender:
        raise StageSkipped('Blender not configured')
    args = [blender, '--background', '--factory-startup', '--python', os.path.join(HERE, 'blender_build.py'), '--',
            '--config', run.cfg_path, '--run', run.dir, '--out', run.repo_path(o.get('out_dir', os.path.join('out', 'recon', run.cfg['name'], 'build')))]
    run.call(args, o.get('timeout_s', 1800), 'build')
    return {'outputs': ['build'], 'metrics': run.read('build/report.json') if os.path.exists(run.path('build', 'report.json')) else {}}


def st_verify(run, o):
    """S11: the acceptance test (docs/MODELLING_FROM_PHOTOS.md section 6): the model from each photo's solved camera
    over the photo; silhouette IoU and the outline's distance to the photo's edges, per photo, into a report."""
    import cv2
    import numpy as np
    import geometry as G
    cams = run.read('cameras.json')
    P = load_prior(run)
    if os.path.exists(run.path('fit.json')):
        P.S = P.S * np.array(run.read('fit.json')['scale'])
    rows = []
    for pid, c in sorted(cams.items()):
        if c.get('method') not in ('silhouette', 'pnp'):
            continue
        img = cv2.imread(run.path('photos', 'clean', pid + '.png'))
        if c.get('mirrored'):
            img = np.ascontiguousarray(img[:, ::-1])
        cam = G.Camera.from_dict(c)
        h, w = img.shape[:2]
        sil = G.splat(P.posed(c.get('articulation', {})), cam, w, h, close_px=5)
        m = cv2.imread(run.path('masks', pid + '.png'), cv2.IMREAD_GRAYSCALE)
        if c.get('mirrored'):
            m = np.ascontiguousarray(m[:, ::-1])
        iou, ch = G.overlay_score(sil, m, G.edges_distance(cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)))
        over = img.copy()
        cnts, _ = cv2.findContours(sil, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        cv2.drawContours(over, cnts, -1, (0, 0, 255), 2)
        cv2.imwrite(run.path('verify', pid + '.jpg'), over)
        rows.append({'photo': pid, 'iou': round(iou, 3), 'edge_px': round(ch, 2) if ch is not None else None,
                     'method': c['method'], 'mirrored': c.get('mirrored', False)})
    run.write('reports/verify.json', rows)
    ok = [r for r in rows if r['iou'] >= o.get('pass_iou', 0.85)]
    return {'outputs': ['verify', 'reports/verify.json'],
            'metrics': {'photos': len(rows), 'pass': len(ok), 'median_iou': sorted(r['iou'] for r in rows)[len(rows) // 2] if rows else None}}


def st_cad(run, o):
    """S12 (optional): the plate hull as a STEP solid through FreeCAD's OpenCASCADE (the hull is planes, so its B-rep
    is exact: no NURBS fitting)."""
    fc = run.tool('freecadcmd')
    if not fc:
        raise StageSkipped('freecadcmd not configured (STEP output is optional; the game needs only the FBX)')
    planes = run.path('build', 'hull_planes.json')
    if not os.path.exists(planes):
        raise StageSkipped('no hull_planes.json from the build stage yet')
    run.call([fc, os.path.join(HERE, 'freecad_step.py'), planes, run.path('cad', 'hull.step')], o.get('timeout_s', 900), 'cad')
    return {'outputs': ['cad']}


STAGES = [
    Stage('pages', st_pages, title='S0 scans -> upright pages'),
    Stage('catalogue', st_catalogue, title='S1 the readers\' notes -> photo catalogue'),
    Stage('frames', st_frames, deps=('pages', 'catalogue'), title='S2 photos cut from the pages'),
    Stage('descreen', st_descreen, deps=('frames',), title='S3 halftone removed'),
    Stage('masks', st_masks, deps=('descreen',), retries=1, title='S4 vehicle masks (SAM 2 / GrabCut)'),
    Stage('depth', st_depth, deps=('descreen',), optional=True, retries=1, degrade=degrade_half, title='S5 depth and normal priors'),
    Stage('prior', st_prior, title='S6 reference mesh -> scaled, parted prior'),
    Stage('cameras', st_cameras, deps=('masks', 'prior'), title='S7 cameras (COLMAP / PnP / silhouette)'),
    Stage('dense', st_dense, deps=('cameras',), optional=True, retries=1, degrade=degrade_half, title='S8 dense surface (capture sessions only)'),
    Stage('fit', st_fit, deps=('cameras', 'prior'), optional=True, title='S9 dimensions refined on all photos'),
    Stage('build', st_build, deps=('fit',), optional=True, title='S10 game model in Blender'),
    Stage('verify', st_verify, deps=('cameras', 'prior'), title='S11 overlays on the photos'),
    Stage('cad', st_cad, deps=('build',), optional=True, title='S12 STEP of the plate hull'),
]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('cmd', choices=['run', 'status', 'tools', 'list'])
    ap.add_argument('config')
    ap.add_argument('--only', default='')
    ap.add_argument('--force', default='')
    ap.add_argument('--until', default='')
    a = ap.parse_args(argv)
    run = Run(a.config)
    if a.cmd == 'list':
        for s in STAGES:
            print('%-10s %s%s' % (s.name, s.title, ' (optional)' if s.optional else ''))
        return 0
    if a.cmd == 'tools':
        for k in sorted(run.cfg.get('tools', {})):
            print('%-14s %s' % (k, run.tool(k) or 'not found'))
        return 0
    if a.cmd == 'status':
        for s in STAGES:
            r = run.manifest.get(s.name, {})
            print('%-10s %-8s %s %s' % (s.name, r.get('status', '-'), json.dumps(r.get('metrics', {}))[:110], r.get('note', '')[:80]))
        return 0
    only = set(x for x in a.only.split(',') if x) or None
    force = set(x for x in a.force.split(',') if x)
    return 0 if execute(run, STAGES, only, force, a.until or None) else 1


if __name__ == '__main__':
    sys.exit(main())
