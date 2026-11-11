"""Score every panel and part of the assembled interior against the photographs: the model (every mesh, panel, fixture and
control, tools/blender/assemble_cabin.py) rendered through each trusted grade-A camera (tools/traces/osa_interior/
cameras.json), its edges compared with the photo's inside each part's own pixels.

    python tools/recon/osa/panel_score.py [VIEW ...] [--width 1600] [--crops N]

Per part and view: `model_px`, the median distance from the model's edges to the nearest photo edge (a part drawn in
the wrong place or a shape the photo does not have), `photo_px`, the median from the photo's edges to the model's (a
shape the model lacks; inflated by wear and lettering), `within3`, the share of model edges within 3 px of a photo
edge, and the same distances in mm at the part's distance from the eye. Writes out/recon/osa/panel_score/score.json,
a table on stdout (worst first) and, for the N worst parts, crops of the photo with the model's edges drawn on it
(<view>_<part>.jpg). Numbers come from the full-resolution renders, never from a displayed image.
Run `python tools/build_osa_interior.py --assemble-only --no-render` first when the cabin changed.
"""
import argparse
import json
import os
import subprocess
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(REPO, 'tools', 'recon'))
import holistic_view as V  # noqa: E402

TRUSTED = ['g092-a', 'g093-a', 'g094-a', 'g094-d', 'g095-c', 'g096-a', 'g097-a', 'g097-b', 'g098-a']   # grade A, solve residual under 5 px; g091-a's camera looks at the wrong rack
BLEND = os.path.join(REPO, 'out', 'osa_interior', 'osa_interior.blend')
OUT = os.path.join(REPO, 'out', 'recon', 'osa', 'panel_score')
MIN_EDGE_PX = 150          # a part needs this many of its own model edge pixels in a view to be scored there


def certified(view, width):
    c = json.load(open(os.path.join(REPO, 'tools', 'traces', 'osa_interior', 'cameras.json'), encoding='utf-8'))['views'][view]['camera']
    cam = V.Camera.from_dict(c)
    return cam.scaled(width, int(round(cam.h * width / cam.w)))


def around_eye(eye):
    """Shell parts the camera's eye stands inside (the photographer leant into a rack's space: g094-a's eye is 14 cm
    inside SP-1M's box), and a hidden rack's face with it; the hull and the lights' volumes never count."""
    cab = json.load(open(os.path.join(REPO, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8'))
    out = []
    for s in cab['shell']:
        if 'centre_m' not in s or 'size_m' not in s or s['id'] == 'hull' or s['id'].startswith('light_'):
            continue
        if (np.abs(eye - np.array(s['centre_m'])) - np.array(s['size_m']) / 2).max() < 0.0:
            out.append(s['id'])
            if s['id'].startswith('rack_'):
                out.append(s['id'][5:] + '_face')
    return out


def render(view, cam):
    os.makedirs(OUT, exist_ok=True)
    state = V.load(V.view_path(view)).get('state', {}) if os.path.exists(V.view_path(view)) else {}
    prefix = os.path.join(OUT, view + '_')
    hide = state.get('hide', []) + around_eye(cam.centre())
    if hide:
        print(view, 'hides', hide, flush=True)
    job = {'blend': BLEND, 'camera': cam.blender(), 'out': prefix, 'passes': ['model', 'id'], 'hide': hide}
    jp = prefix + 'job.json'
    V.save(jp, job)
    r = subprocess.run([V.BLENDER, '--background', '--factory-startup', '--python-exit-code', '3', '--python',
                        os.path.join(REPO, 'tools', 'blender', 'assembled_render.py'), '--', jp],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        raise RuntimeError('%s did not render: %s' % (view, r.stderr[-800:]))
    return prefix


def edges(img):
    g = cv2.GaussianBlur(cv2.cvtColor(img, cv2.COLOR_BGR2GRAY), (3, 3), 0)
    return cv2.Canny(g, 40, 110)


def part_centres():
    """Each part's centre in the body frame, for its distance from the eye (panels, shell parts)."""
    cab = json.load(open(os.path.join(REPO, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8'))
    out = {p['id']: np.array(p['centre_m']) for p in cab['panels']}
    for s in cab['shell']:
        if 'centre_m' in s:
            out[s['id']] = np.array(s['centre_m'])
    return out


def score_view(view, width, centres):
    cam = certified(view, width)
    prefix = render(view, cam)
    photo = cv2.resize(cv2.imread(V.photo_file(view, 'clean')), (cam.w, cam.h), interpolation=cv2.INTER_AREA)
    model = cv2.imread(prefix + 'model.png')
    ids = cv2.cvtColor(cv2.imread(prefix + 'id.png'), cv2.COLOR_BGR2RGB)
    by_part = json.load(open(prefix + 'id.json', encoding='utf-8'))['by_part']
    ep, em = edges(photo), edges(model)
    dt_p = cv2.distanceTransform(255 - ep, cv2.DIST_L2, 3)
    dt_m = cv2.distanceTransform(255 - em, cv2.DIST_L2, 3)
    eye = cam.centre()
    rows = []
    for part, rgb in by_part.items():
        mask = np.all(ids == np.array(rgb, np.uint8), axis=-1).astype(np.uint8)
        if mask.sum() < 400:
            continue
        inner = cv2.erode(mask, np.ones((5, 5), np.uint8))           # a part's outline against what is behind it counts too, but not the next part's
        mm_ = (em > 0) & (cv2.dilate(mask, np.ones((3, 3), np.uint8)) > 0)
        pm_ = (ep > 0) & (inner > 0)
        if mm_.sum() < MIN_EDGE_PX:
            continue
        dm, dp = dt_p[mm_], dt_m[pm_] if pm_.sum() else np.array([np.nan])
        c = centres.get(part)
        mm_per_px = float(np.linalg.norm(c - eye)) * 1000.0 / cam.f if c is not None else float('nan')
        ys, xs = np.nonzero(mask)
        rows.append({'view': view, 'part': part, 'model_px': round(float(np.median(dm)), 2), 'photo_px': round(float(np.nanmedian(dp)), 2),
                     'within3': round(float((dm <= 3).mean()), 3), 'mm_per_px': round(mm_per_px, 2),
                     'model_mm': round(float(np.median(dm)) * mm_per_px, 1), 'edge_px': int(mm_.sum()),
                     'box': [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())]})
    over = photo.copy()
    over[em > 0] = (0, 255, 255)
    cv2.imwrite(prefix + 'over.jpg', over, [cv2.IMWRITE_JPEG_QUALITY, 90])
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('views', nargs='*')
    ap.add_argument('--width', type=int, default=1600)
    ap.add_argument('--crops', type=int, default=12)
    a = ap.parse_args(argv)
    centres = part_centres()
    rows = []
    for v in a.views or TRUSTED:
        got = score_view(v, a.width, centres)
        rows += got
        print(v, len(got), 'parts scored', flush=True)
    rows.sort(key=lambda r: -r['model_mm'] if r['model_mm'] == r['model_mm'] else 0)
    os.makedirs(OUT, exist_ok=True)
    V.save(os.path.join(OUT, 'score.json'), {'width': a.width, 'rows': rows})
    print('%-8s %-24s %8s %8s %8s %8s %7s' % ('view', 'part', 'model_px', 'photo_px', 'within3', 'model_mm', 'edges'))
    for r in rows:
        print('%-8s %-24s %8.2f %8.2f %8.3f %8.1f %7d' % (r['view'], r['part'], r['model_px'], r['photo_px'], r['within3'], r['model_mm'], r['edge_px']))
    for r in rows[:a.crops]:
        over = cv2.imread(os.path.join(OUT, r['view'] + '_over.jpg'))
        x0, y0, x1, y1 = r['box']
        pad = 20
        crop = over[max(0, y0 - pad):y1 + pad, max(0, x0 - pad):x1 + pad]
        cv2.imwrite(os.path.join(OUT, '%s_%s.jpg' % (r['view'], r['part'])), crop, [cv2.IMWRITE_JPEG_QUALITY, 90])
    return 0


if __name__ == '__main__':
    sys.exit(main())
