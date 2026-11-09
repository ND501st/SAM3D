"""Tests for the reconstruction pipeline (tools/recon/): synthetic data only, so they run without the book's scans,
the reference mesh, a GPU or Blender."""
import json
import math
import os
import sys

import pytest

np = pytest.importorskip('numpy')
cv2 = pytest.importorskip('cv2')

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'recon'))
import book  # noqa: E402
import geometry as G  # noqa: E402
import osa_recon as R  # noqa: E402


# --------------------------------------------------------------------------------------------------------- book

def halftone(n=512, period=3.0, angle_deg=45.0):
    """A grey ramp printed as a dot screen of the given period and angle."""
    yy, xx = np.mgrid[:n, :n].astype(float)
    a = math.radians(angle_deg)
    u = (xx * math.cos(a) + yy * math.sin(a)) * 2 * math.pi / period
    v = (-xx * math.sin(a) + yy * math.cos(a)) * 2 * math.pi / period
    tone = xx / n
    screen = 0.25 * (np.cos(u) + np.cos(v)) + 0.5
    return np.where(screen < tone, 255, 0).astype(np.uint8)


def test_screen_period_found():
    per, angles = book.screen_period(halftone(period=3.0, angle_deg=45))
    assert per == pytest.approx(3.0, abs=0.2)
    assert any(abs(a - 45) <= 2 or abs(a - 135) <= 2 for a in angles)


def test_descreen_removes_the_screen_and_keeps_the_tone():
    img = halftone(period=3.0)
    per, _ = book.screen_period(img)
    clean = book.descreen(img, per, out_scale=1.0)
    assert book.screen_energy(clean, per) < 0.05 * book.screen_energy(img, per)
    # the printed tone (a left-to-right ramp) survives as the local mean
    assert clean[:, 400:460].mean() > clean[:, 50:110].mean() + 100


def test_needs_turn_rules():
    assert book.needs_turn(2, 'even') and not book.needs_turn(3, 'even')
    assert book.needs_turn(5, [5, 7]) and not book.needs_turn(6, [5, 7])
    assert not book.needs_turn(4, 'none')
    rule = {'rule': 'even', 'except': [124, 7]}  # an exception flips the rule either way
    assert book.needs_turn(122, rule) and not book.needs_turn(124, rule) and book.needs_turn(7, rule)


def page_with_photos(boxes, W=1600, H=1487, shadow=8):
    """A pale page with textured photos and drop shadows that fill the gutters."""
    rng = np.random.default_rng(1)
    img = np.full((H, W, 3), (196, 214, 200), np.uint8)  # pale green page (BGR)
    for x0, y0, x1, y1 in boxes:
        a, b, c, d = int(x0 * W), int(y0 * H), int(x1 * W), int(y1 * H)
        img[b + shadow:d + shadow, a + shadow:c + shadow] = (150, 165, 152)  # shadow: the page's hue, darker
        img[b:d, a:c] = rng.integers(0, 255, (d - b, c - a, 3), dtype=np.uint8)
    return img


def test_find_frames_separates_photos_with_shadowed_gutters():
    boxes = [(0.06, 0.05, 0.47, 0.45), (0.50, 0.05, 0.94, 0.45), (0.06, 0.50, 0.94, 0.92)]
    found = book.find_frames(page_with_photos(boxes))
    assert book.frame_recall(found, boxes, thr=0.85) == 1.0
    assert len(found) == 3


CATALOGUE_MD = """
## g030 · book p. 028 · Launcher Vehicle - Hull Rear
### g030-a (top left, bbox 0.09,0.06,0.52,0.46)
- Caption: water-jet cover
### g030-b (top right, bbox 0.53,0.06,0.95,0.46)
### Photo index
| photo id | subject | vehicle | az deg | height | coverage | parts | solve? | issues |
|---|---|---|---|---|---|---|---|---|
| g030-a | 9A33BM2 | CZ 2004 Doupov | 180 | ground | rear detail | water jet | N | gloom |
| g030-b | 9A33BM2 | CZ 2004 Doupov | 200 | eye | rear 3/4 | hull | Y | |
"""


def test_parse_catalogue_and_groups():
    recs = book.parse_catalogue(CATALOGUE_MD)
    assert [r['photo'] for r in recs] == ['g030-a', 'g030-b']
    assert recs[0]['bbox'] == [0.09, 0.06, 0.52, 0.46]
    assert recs[1]['solve'] and not recs[0]['solve']
    assert recs[1]['az_deg'] == 200
    assert book.groups_by_vehicle(recs) == {'cz 2004 doupov': ['g030-a', 'g030-b']}


def test_parse_catalogue_reads_the_bullets_without_a_table():
    md = """
## g017 · book p. 015 · Launcher Vehicle Walk Around
### g017-a (top, full width, bbox 0.08,0.04,0.95,0.53)
- Subject: 9A33BM2 in low transport mode
- Vehicle: Czech Army, on a flat wagon
- View: az ~225 (left rear quarter), camera at track level
- For reconstruction: whole-vehicle camera-solve candidate: Y (left side + rear)
### g017-b (bottom left, bbox 0.08,0.67,0.44,0.93)
- View: detail, from the roof
- For reconstruction: whole-vehicle camera-solve candidate: N
"""
    a, b = book.parse_catalogue(md)
    assert a['az_deg'] == 225 and a['solve'] and a['subject'].startswith('9A33BM2')
    assert 'az_deg' not in b and b['solve'] is False
    # a free-text vehicle note never becomes a grouping key: each photo stays alone until the table names it
    assert len(book.groups_by_vehicle([a, b])) == 2


# ----------------------------------------------------------------------------------------------------- geometry

def box_mesh(sx, sy, sz):
    V = np.array([[x, y, z] for x in (-sx / 2, sx / 2) for y in (-sy / 2, sy / 2) for z in (0, sz)], float)
    F = np.array([[0, 1, 3], [0, 3, 2], [4, 6, 7], [4, 7, 5], [0, 4, 5], [0, 5, 1],
                  [2, 3, 7], [2, 7, 6], [0, 2, 6], [0, 6, 4], [1, 5, 7], [1, 7, 3]])
    return V, F


def test_prior_axes_keep_the_frame_right_handed():
    V = np.array([[1.0, 0, 0], [0, 1.0, 0], [0, 0, 1.0]])
    F = np.array([[0, 1, 2]])
    P = G.Prior(V, F, {}, {}, axes=('-x', 'z', 'y'))
    # source +Z (the bow) becomes +y, source +Y (up) becomes +z, source +X becomes the vehicle's left (-x)
    assert P.V[2].tolist() == pytest.approx([0.0, 1.0, 0.0])
    assert P.V[1].tolist() == pytest.approx([0.0, 0.0, 1.0])
    assert P.V[0].tolist() == pytest.approx([-1.0, 0.0, 0.0])
    with pytest.raises(ValueError):
        G.Prior(V, F, {}, {}, axes=('x', 'z', 'y'))  # a mirror: refused


def test_pnp_recovers_a_camera_with_unknown_focal():
    rng = np.random.default_rng(3)
    X = rng.uniform([-1.4, -4.5, 0], [1.4, 4.5, 2.6], size=(14, 3))
    truth = G.camera_from_view(300, 1.6, 15, 1200, 800, fov_deg=38)
    truth.cx += 60  # a cropped photo: the principal point is off centre
    x, _ = truth.project(X)
    cam, rms = G.pnp(X, x, 1200, 800)
    assert rms < 0.5
    assert cam.f == pytest.approx(truth.f, rel=0.03)
    C_true, C_fit = -truth.R.T @ truth.t, -cam.R.T @ cam.t
    assert np.linalg.norm(C_true - C_fit) < 0.3


def test_splat_matches_the_exact_silhouette():
    V, F = box_mesh(2.8, 9.0, 2.2)
    S, _ = G.surface_samples(V, F, 60000)
    cam = G.camera_from_view(320, 1.6, 14, 320, 240, fov_deg=40)
    exact = G.silhouette(V, F, cam, 320, 240)
    fast = G.splat(S, cam, 320, 240, close_px=3)
    iou, _ = G.overlay_score(fast, exact)
    assert iou > 0.95


def test_fit_camera_improves_a_rough_start():
    V, F = box_mesh(2.8, 9.0, 2.2)
    P = G.Prior.__new__(G.Prior)
    P.V, P.F, P.part, P.pivots = V, F, np.array(['hull'] * len(F), dtype=object), {}
    P.sampled(40000)
    w, h = 240, 160
    truth = G.camera_from_view(300, 1.7, 16, w, h, fov_deg=35)
    target = G.splat(P.posed({}), truth, w, h)
    start = G.camera_from_view(318, 1.6, 14, w, h, fov_deg=40)
    iou0, _ = G.overlay_score(G.splat(P.posed({}), start, w, h), target)
    cam, _, _ = G.fit_camera(P, target, start, iters=200)
    iou1, _ = G.overlay_score(G.splat(P.posed({}), cam, w, h), target)
    assert iou1 > max(0.93, iou0 + 0.05)


def test_mirrored_photo_is_detected():
    # an asymmetric body: a box with a raised block on its left rear
    V1, F1 = box_mesh(2.8, 9.0, 2.0)
    V2, F2 = box_mesh(1.0, 3.0, 1.2)
    V2 += [-0.9, -2.5, 2.0]
    V, F = np.vstack([V1, V2]), np.vstack([F1, F2 + 8])
    P = G.Prior.__new__(G.Prior)
    P.V, P.F, P.part, P.pivots = V, F, np.array(['hull'] * len(F), dtype=object), {}
    P.sampled(40000)
    w, h = 240, 160
    truth = G.camera_from_view(240, 2.0, 15, w, h, fov_deg=35)
    photo = np.ascontiguousarray(G.splat(P.posed({}), truth, w, h)[:, ::-1])  # printed mirrored
    start = G.camera_from_view(240, 2.0, 15, w, h, fov_deg=35)
    _, _, _, mirrored = G.fit_camera_or_mirror(P, photo, start, iters=150)
    assert mirrored


# ------------------------------------------------------------------------------------------------- orchestrator

def make_run(tmp_path, stages_cfg=None):
    cfg = {'name': 'test', 'out_dir': str(tmp_path / 'run'), 'stages': stages_cfg or {}, 'max_item_failure': 0.5}
    p = tmp_path / 'cfg.json'
    p.write_text(json.dumps(cfg))
    return R.Run(str(p))


def test_stages_skip_when_inputs_unchanged_and_rerun_when_forced(tmp_path):
    calls = []

    def a(run, o):
        calls.append('a')
        run.write('a.json', {'v': 1})
        return {'outputs': ['a.json']}

    def b(run, o):
        calls.append('b')
        return {'outputs': []}
    stages = [R.Stage('a', a), R.Stage('b', b, deps=('a',))]
    run = make_run(tmp_path)
    assert R.execute(run, stages)
    assert R.execute(R.Run(run.cfg_path), stages)  # a fresh process: the manifest says both are done
    assert calls == ['a', 'b']
    assert R.execute(R.Run(run.cfg_path), stages, force={'a'})
    assert calls == ['a', 'b', 'a']  # b's inputs (a's outputs) didn't change, so b stays done


def test_retry_then_success_and_failure_stops_dependants(tmp_path, monkeypatch):
    monkeypatch.setattr(R.time, 'sleep', lambda s: None)
    n = {'flaky': 0}

    def flaky(run, o):
        n['flaky'] += 1
        if n['flaky'] < 2:
            raise RuntimeError('transient')
        return {}

    def broken(run, o):
        raise RuntimeError('always')

    def after(run, o):
        raise AssertionError('must not run')
    run = make_run(tmp_path)
    ok = R.execute(run, [R.Stage('flaky', flaky, retries=2), R.Stage('broken', broken, retries=1),
                         R.Stage('after', after, deps=('broken',))])
    assert not ok
    assert run.manifest['flaky']['status'] == 'done' and run.manifest['flaky']['attempts'] == 2
    assert run.manifest['broken']['status'] == 'failed'
    assert 'after' not in run.manifest or run.manifest['after']['status'] != 'done'


def test_out_of_memory_degrades_then_optional_skip_does_not_block(tmp_path):
    seen = []

    def gpu(run, o):
        seen.append(o.get('max_px', 1024))
        if o.get('max_px', 1024) > 512:
            raise R.OutOfMemory('CUDA out of memory')
        return {}

    def optional(run, o):
        raise R.StageSkipped('tool not configured')

    def later(run, o):
        return {'note': 'ran'}
    run = make_run(tmp_path)
    ok = R.execute(run, [R.Stage('gpu', gpu, retries=2, degrade=R.degrade_half),
                         R.Stage('opt', optional, optional=True), R.Stage('later', later)])
    assert ok and seen == [1024, 512]
    assert run.manifest['opt']['status'] == 'skipped'
    assert run.manifest['later']['status'] == 'done'


def test_a_stage_on_a_skipped_input_skips_too(tmp_path):
    def optional(run, o):
        raise R.StageSkipped('no traces yet')

    def uses_it(run, o):
        raise AssertionError('must not run without its input')
    run = make_run(tmp_path)
    ok = R.execute(run, [R.Stage('opt', optional, optional=True), R.Stage('uses', uses_it, deps=('opt',), optional=True)])
    assert ok and run.manifest['uses']['status'] == 'skipped' and 'opt' in run.manifest['uses']['note']


def test_item_failures_over_the_limit_fail_the_stage(tmp_path):
    run = make_run(tmp_path)
    run.item_failures('x', [{'photo': 'g001-a'}], 4)  # 25 %: fine
    with pytest.raises(RuntimeError):
        run.item_failures('x', [{'photo': 'a'}, {'photo': 'b'}, {'photo': 'c'}], 4)


def test_the_config_names_every_stage_and_sources_its_dimensions():
    cfg_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'recon', 'osa_9a33bm2.json')
    cfg = json.load(open(cfg_path, encoding='utf-8'))
    names = {s.name for s in R.STAGES}
    assert set(cfg.get('stages', {})) <= names
    for key, d in cfg['dimensions'].items():
        assert 'value' in d and d.get('source'), key  # every dimension cites its source (CLAUDE.md data rules)
