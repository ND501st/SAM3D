"""Ingesting a printed book as reference photography (docs/OSA_EXTERIOR_RECONSTRUCTION.md, stages S0-S3).

A photo book is not a capture session: every photo is a cropped, halftone-printed reproduction, and the pages were
scanned one by one (the left-hand pages of *SA-8 Gecko in Detail* came out upside down). This module turns the scans
into clean photos with their metadata:

* `extract_pages`: each page's embedded scan, upright, plus a 1600 px overview.
* `find_frames`: the photo frames on a page by recursive XY-cut on the page's background, drop shadows included.
  A first guess only: on mosaic pages the shadows fill the gutters, so the catalogue's checked boxes win.
* `descreen`: the print's halftone screen removed by a low-pass at the screen's frequency (nothing finer than the
  screen was ever printed), then resampled to twice the screen's lines per inch.
* `screen_period`: the screen's period and angles from the spectrum (the book: ~3.0 px at 600 dpi, four angles).
* `parse_catalogue`: the readers' notes (`### g030-a (..., bbox x0,y0,x1,y1)` headings and the `### Photo index`
  tables) into one record per photo.

Pure Python with numpy, OpenCV and (for the PDFs) PyMuPDF; no bpy.
"""
import io
import json
import os
import re


def _np():
    import numpy as np
    return np


def _cv():
    import cv2
    return cv2


# ---------------------------------------------------------------------------------------------------------------- S0

def page_sources(pdfs, counts=None):
    """[(pdf, index_in_pdf)] for global pages 1..N in order."""
    import fitz
    out = []
    for p in pdfs:
        d = fitz.open(p)
        out.extend((p, i) for i in range(d.page_count))
    return out


def load_page(pdf, index, turn_180=False):
    """The page's embedded scan as an RGB numpy array (no resampling), turned if asked."""
    import fitz
    from PIL import Image
    Image.MAX_IMAGE_PIXELS = None
    d = fitz.open(pdf)
    imgs = d[index].get_images(full=True)
    if imgs:
        raw = d.extract_image(imgs[0][0])['image']
        im = Image.open(io.BytesIO(raw)).convert('RGB')
    else:  # a page without an embedded image: render it
        pix = d[index].get_pixmap(dpi=600)
        im = Image.frombytes('RGB', (pix.width, pix.height), pix.samples)
    if turn_180:
        im = im.rotate(180)
    return _np().asarray(im)


def needs_turn(global_page, rule):
    """The orientation rule from the config: 'even' (the WWP scans), 'odd', 'none', a list of page numbers, or
    {"rule": "even", "except": [124]} for the pages a rule gets wrong (the WWP back cover was scanned upright)."""
    if isinstance(rule, dict):
        return needs_turn(global_page, rule.get('rule', 'none')) != (global_page in rule.get('except', []))
    if isinstance(rule, list):
        return global_page in rule
    return {'even': global_page % 2 == 0, 'odd': global_page % 2 == 1}.get(rule, False)


def extract_pages(pdfs, out_dir, rule='even', view_px=1600, log=print):
    """Write view/NNN.jpg (upright overviews) and return the page index. The full-resolution page is not copied
    (124 pages would be ~6 GB as PNG): `load_page` reads it from the PDF's embedded JPEG when a photo is cut."""
    cv2 = _cv()
    os.makedirs(os.path.join(out_dir, 'view'), exist_ok=True)
    index = []
    for g, (pdf, k) in enumerate(page_sources(pdfs), start=1):
        view = os.path.join(out_dir, 'view', f'{g:03d}.jpg')
        turn = needs_turn(g, rule)
        if not os.path.exists(view):
            bgr = load_page(pdf, k, turn)[..., ::-1]
            s = view_px / max(bgr.shape[:2])
            cv2.imwrite(view, cv2.resize(bgr, (int(bgr.shape[1] * s), int(bgr.shape[0] * s)), interpolation=cv2.INTER_AREA))
            log(f'page {g:03d} <- {os.path.basename(pdf)} #{k + 1}{" (turned)" if turn else ""}')
        index.append({'page': g, 'pdf': pdf, 'index': k, 'turned': turn})
    return index


# ---------------------------------------------------------------------------------------------------------------- S1

def background_mask(img):
    """Page background and the drop shadows on it: the page's own hue at any lightness from shadow to paper, smooth."""
    np, cv2 = _np(), _cv()
    lab = cv2.cvtColor(img, cv2.COLOR_BGR2LAB).astype(np.float32)
    border = np.concatenate([lab[:12].reshape(-1, 3), lab[-12:].reshape(-1, 3),
                             lab[:, :12].reshape(-1, 3), lab[:, -12:].reshape(-1, 3)])
    bg = np.median(border, axis=0)
    hue_ok = (np.abs(lab[..., 1] - bg[1]) < 7) & (np.abs(lab[..., 2] - bg[2]) < 9)
    light_ok = (lab[..., 0] > bg[0] - 60) & (lab[..., 0] < bg[0] + 30)
    g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY).astype(np.float32)
    mu = cv2.blur(g, (3, 3))
    var = cv2.blur(g * g, (3, 3)) - mu * mu
    return hue_ok & light_ok & (var < 30)


def _runs(prof, thr, min_len):
    out, start = [], None
    for i, v in enumerate(list(prof) + [0.0]):
        if v >= thr and start is None:
            start = i
        if v < thr and start is not None:
            if i - start >= min_len:
                out.append((start, i))
            start = None
    return out


def _xy_cut(bg, x0, y0, x1, y1, depth, out, W, H):
    np = _np()
    sub = bg[y0:y1, x0:x1]
    if sub.size == 0:
        return
    rows = np.where(sub.mean(axis=1) < 0.85)[0]
    cols = np.where(sub.mean(axis=0) < 0.85)[0]
    if len(rows) == 0 or len(cols) == 0:
        return
    x0, x1, y0, y1 = x0 + cols[0], x0 + cols[-1] + 1, y0 + rows[0], y0 + rows[-1] + 1
    sub = bg[y0:y1, x0:x1]
    best = None
    for axis in (0, 1):
        prof = sub.mean(axis=axis)
        n = len(prof)
        for a, b in _runs(prof, 0.80, 3):
            if a < 0.04 * n or b > 0.96 * n:
                continue
            if best is None or (b - a) > best[3]:
                best = (axis, a, b, b - a)
    if best and depth < 8:
        axis, a, b, _ = best
        if axis == 0:
            _xy_cut(bg, x0, y0, x0 + a, y1, depth + 1, out, W, H)
            _xy_cut(bg, x0 + b, y0, x1, y1, depth + 1, out, W, H)
        else:
            _xy_cut(bg, x0, y0, x1, y0 + a, depth + 1, out, W, H)
            _xy_cut(bg, x0, y0 + b, x1, y1, depth + 1, out, W, H)
        return
    w, h = x1 - x0, y1 - y0
    fill = 1.0 - float(bg[y0:y1, x0:x1].mean())
    if w * h >= 0.01 * W * H and w >= 0.07 * W and h >= 0.05 * H and fill > 0.85:
        out.append([round(x0 / W, 4), round(y0 / H, 4), round(x1 / W, 4), round(y1 / H, 4)])


def find_frames(img):
    """Photo frames on an upright page image (BGR), as [x0, y0, x1, y1] fractions, top to bottom, left to right."""
    H, W = img.shape[:2]
    out = []
    _xy_cut(background_mask(img), 0, 0, W, H, 0, out, W, H)
    out.sort(key=lambda b: (round(b[1], 1), b[0]))
    return out


def iou(a, b):
    ix = max(0.0, min(a[2], b[2]) - max(a[0], b[0]))
    iy = max(0.0, min(a[3], b[3]) - max(a[1], b[1]))
    inter = ix * iy
    union = (a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - inter
    return inter / union if union > 0 else 0.0


def frame_recall(found, truth, thr=0.8):
    """Share of the catalogue's frames that the detector found with IoU >= thr."""
    if not truth:
        return 1.0
    return sum(1 for t in truth if any(iou(t, f) >= thr for f in found)) / len(truth)


# ---------------------------------------------------------------------------------------------------------------- S2

def screen_period(gray):
    """(period_px, [angles_deg]) of the strongest periodic pattern above the image's own low frequencies."""
    np = _np()
    n = min(gray.shape)
    im = gray[:n, :n].astype(float)
    im -= im.mean()
    F = np.abs(np.fft.fftshift(np.fft.fft2(im * np.outer(np.hanning(n), np.hanning(n)))))
    c = n // 2
    yy, xx = np.mgrid[:n, :n]
    r = np.hypot(yy - c, xx - c)
    F[r < n / 12] = 0
    peaks = []
    for _ in range(8):
        i = np.unravel_index(int(np.argmax(F)), F.shape)
        fy, fx = i[0] - c, i[1] - c
        rr = math_hypot(fx, fy)
        if rr > 0:
            peaks.append((n / rr, (np.degrees(np.arctan2(fy, fx)) % 180.0), float(F[i])))
        F[max(0, i[0] - 3):i[0] + 4, max(0, i[1] - 3):i[1] + 4] = 0
    if not peaks:
        return None, []
    # the strongest peak is the screen's fundamental; a thresholded dot screen also has harmonics and cross terms
    # (at the period over root 2 and over 2), so the angles are read only from peaks at the fundamental's radius
    period = max(peaks, key=lambda p: p[2])[0]
    same = [p for p in peaks if abs(p[0] - period) < 0.08 * period]
    period = float(np.mean([p[0] for p in same]))
    angles = sorted({round(p[1]) for p in same})
    return period, angles


def math_hypot(a, b):
    return (a * a + b * b) ** 0.5


def screen_energy(gray, period):
    """Spectral energy in a band round the screen frequency, relative to the whole (a descreen check)."""
    np = _np()
    n = min(gray.shape)
    im = gray[:n, :n].astype(float)
    im -= im.mean()
    F = np.abs(np.fft.fftshift(np.fft.fft2(im))) ** 2
    c = n // 2
    yy, xx = np.mgrid[:n, :n]
    r = np.hypot(yy - c, xx - c)
    f0 = n / period
    band = (r > 0.85 * f0) & (r < 1.15 * f0)
    return float(F[band].sum() / max(F[r > 2].sum(), 1e-9))


def descreen(img, period_px=3.0, out_scale=None):
    """Low-pass just below the screen frequency, then resample so one output pixel is about half a screen period.
    Returns the cleaned image (same dtype)."""
    cv2 = _cv()
    sigma = period_px / 2.6
    blur = cv2.GaussianBlur(img, (0, 0), sigma)
    s = out_scale if out_scale else min(1.0, 2.0 / period_px)
    if s >= 0.999:
        return blur
    return cv2.resize(blur, (max(1, int(img.shape[1] * s)), max(1, int(img.shape[0] * s))), interpolation=cv2.INTER_AREA)


# ---------------------------------------------------------------------------------------------------------------- S3

HEAD = re.compile(r'^###\s+(g\d{3}-[a-z]+)\s*\(([^)]*)\)', re.M)
BBOX = re.compile(r'bbox\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)')
INDEX_COLS = ['photo', 'subject', 'vehicle', 'az_deg', 'height', 'coverage', 'parts', 'solve', 'issues']


def _num(s):
    m = re.search(r'-?\d+(\.\d+)?', s or '')
    return float(m.group(0)) if m else None


def parse_catalogue(text):
    """One record per photo from a reader's notes: id, page, bbox, and the index table's columns."""
    recs = {}
    heads = list(HEAD.finditer(text))
    for i, m in enumerate(heads):
        pid = m.group(1)
        rec = recs.setdefault(pid, {'photo': pid, 'page': int(pid[1:4])})
        b = BBOX.search(m.group(2))
        if b:
            rec['bbox'] = [float(b.group(i)) for i in range(1, 5)]
        rec['where'] = m.group(2).split(',')[0].strip()
        # the photo's own bullets, until the next heading: a fallback for notes without the index table (or a
        # reader still writing); the table, when there, overrides them
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        nxt = re.search(r'^##', text[m.end():end], re.M)
        body = text[m.end():m.end() + nxt.start()] if nxt else text[m.end():end]
        for key, field in (('subject', 'Subject'), ('view', 'View'), ('vehicle_note', 'Vehicle')):
            f = re.search(r'^-\s*' + field + r':\s*(.+)$', body, re.M)
            if f:
                rec[key] = f.group(1).strip()
        az = re.search(r'\baz\s*~?\s*(-?\d+)', rec.get('view', ''))
        if az:
            rec['az_deg'] = float(az.group(1))
        sv = re.search(r'solve candidate:\s*([YN])', body)
        if sv:
            rec['solve'] = sv.group(1) == 'Y'
    idx = text.find('### Photo index')
    if idx >= 0:
        for line in text[idx:].splitlines()[1:]:
            line = line.strip()
            if line.startswith('###'):
                break
            if not line.startswith('|') or set(line) <= set('|-: '):
                continue
            cells = [c.strip() for c in line.strip('|').split('|')]
            if not re.match(r'g\d{3}-[a-z]+', cells[0] if cells else ''):
                continue
            row = dict(zip(INDEX_COLS, cells + [''] * (len(INDEX_COLS) - len(cells))))
            rec = recs.setdefault(row['photo'], {'photo': row['photo'], 'page': int(row['photo'][1:4])})
            rec.update({k: row[k] for k in INDEX_COLS[1:]})
            rec['az_deg'] = _num(row['az_deg'])
            rec['solve'] = row['solve'].strip().upper().startswith('Y')
    return [recs[k] for k in sorted(recs)]


def load_catalogue(notes_dir):
    out = []
    for name in sorted(os.listdir(notes_dir)):
        if name.startswith('pages_') and name.endswith('.md'):  # the readers' finished files, not their drafts
            out.extend(parse_catalogue(open(os.path.join(notes_dir, name), encoding='utf-8').read()))
    return out


def groups_by_vehicle(records):
    """Photos grouped by the physical vehicle the readers named (empty or unknown vehicles stay alone)."""
    groups = {}
    for r in records:
        key = (r.get('vehicle') or '').strip().lower()
        key = re.sub(r'\s+', ' ', key)
        if not key or key in ('?', 'unknown', '-', 'n/a'):
            key = 'single:' + r['photo']
        groups.setdefault(key, []).append(r['photo'])
    return groups


def save_json(obj, path):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(obj, f, indent=1, ensure_ascii=False)
