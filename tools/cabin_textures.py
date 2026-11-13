"""Bake the 3D cabin's textures from the layout file: the console panels' faces and the tiling detail maps.

    python tools/cabin_textures.py                    # everything, into art/cabin_textures/
    python tools/cabin_textures.py --only panels      # or: details
    python tools/cabin_textures.py --preview out/panel_faces    # also write small JPEG previews

Panel faces, T_Panel_<panel id>.png: the block plates' satin enamel (the console's cream-grey, or a block's own
colour such as PU-303's black) with the wear the cabin photos show (darker towards the bottom of each block and
round the controls the crew handles, fine scratches), and every printed or engraved marking the layout gives: each
control's, lamp's and fixture's label above it in a condensed sans-serif (the panels' lettering in usaf_shaw_2017
-024 and samsim_panels: black capitals about 3.5 mm high). A label "A — B" on a two-position switch is split as on
the real panels, A beside the up position and B below; a rotary's "A — B — C" goes round its knob at its positions,
with tick marks (when the parts match the console's position labels). Printed diagrams are ruled outlines (their
contents can't be read in any source). The texture covers the panel's whole rectangle, (0, 0) at its top left; the
cabin maps each block's corner onto it. Displays, windows, nameplates and legend lamps are not drawn: the cabin
models them.

Detail maps (tiling, 1 m of the cabin's box-projected UVs per repeat unless noted): T_Cabin_Grime (three
independent masks: R large stains, G mottling, B fine specks), T_Cabin_PaintN (orange peel and shallow dents),
T_Cabin_RibN (ribbed rubber mat, ribs along U), T_Cabin_VinylN (leatherette grain, 0.25 m per repeat).
"""
import argparse
import json
import math
import os
import re
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, 'art', 'cabin_textures')
FONT = r'C:\Windows\Fonts\ARIALN.TTF'          # Arial Narrow: close to the panels' condensed capitals, has Cyrillic
FONT_BOLD = r'C:\Windows\Fonts\ARIALNB.TTF'
FALLBACK_FONTS = ['/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf', '/Library/Fonts/Arial Narrow.ttf']

PLATE = (196, 194, 182)      # the blocks' enamel (sRGB), from samsim_panels and the cabin photos in daylight
INK = (24, 24, 22)           # engraved letters filled black
INK_ON_BLACK = (205, 203, 192)
DIAGRAM_INK = (52, 48, 40)
LOOK_COLOURS = {'black': (22, 22, 21), 'white': PLATE, 'grey': (150, 153, 146)}   # grey: the cab dashboards (rwd_mb3 9a33_31/32)
LOOK_COLOURS.update({'bluegrey': (112, 124, 135), 'lightgrey': (161, 163, 165)})   # Unreal's LookColor in sRGB
CAP_HEIGHT_M = 0.0040        # letter height (samsim_panels at 1 mm per pixel: 3-4 mm; SŁ-2M on SAM's AK photo at 3x: 3.6-4.2)
LABEL_BOLD = True            # the engraved capitals are heavy (SAM Simulator's AK photo at 3x: strokes about 0.6 mm)
STROKE_EM = 0.045            # and a stroke round them: heavier and wider than Arial Narrow Bold (ПЕЛЕНГ on the photo at 12x
                             # 8% wider than drawn without it, its strokes thicker; the project owner's review, 14 Nov 2026)


def stroke_of(f):
    return max(0, int(round(STROKE_EM * f.size)))
TOGGLE_LEVER_M = 0.008       # a toggle's lever leans up past its bush: the legend above stands clear of its tip
GAP_M = 0.003                # between an item and its label


def font_path(bold=False):
    for f in ([FONT_BOLD] if bold else []) + [FONT] + FALLBACK_FONTS:
        if os.path.exists(f):
            return f
    sys.exit('no condensed font found (Arial Narrow on Windows)')


# ---------------------------------------------------------------------------------------------- noise

def tile_noise(n, beta, seed, lo=1.0, hi=None):
    """Periodic (tiling) noise n x n with a 1/f^beta spectrum between frequencies lo and hi, scaled to 0..1."""
    rng = np.random.default_rng(seed)
    white = rng.standard_normal((n, n))
    f = np.fft.fftfreq(n) * n
    fx, fy = np.meshgrid(f, f)
    r = np.sqrt(fx ** 2 + fy ** 2)
    r[0, 0] = 1.0
    amp = r ** (-beta)
    amp[r < lo] = 0.0
    if hi is not None:
        amp[r > hi] = 0.0
    amp[0, 0] = 0.0
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * amp))
    out -= out.min()
    return out / max(out.max(), 1e-9)


def normal_map(height, strength):
    """Tangent-space normal map (OpenGL-style green up, as Unreal flips on import) from a tiling height field."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * strength
    nx, ny, nz = -dx, dy, np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    rgb = np.stack([nx / length, ny / length, nz / length], axis=-1) * 0.5 + 0.5
    return Image.fromarray(np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGB')


def bake_details(out_dir):
    n = 1024
    grime = np.stack([tile_noise(n, 1.6, 1, lo=2), tile_noise(n, 1.1, 2, lo=6), tile_noise(n, 0.4, 3, lo=60)], axis=-1)
    Image.fromarray((grime * 255).astype(np.uint8), 'RGB').save(os.path.join(out_dir, 'T_Cabin_Grime.png'))
    # paint: orange peel (fine) and a few shallow dents (coarse)
    peel = tile_noise(n, 0.8, 4, lo=80, hi=300)
    dents = tile_noise(n, 2.2, 5, lo=3, hi=30)
    normal_map(peel * 0.6 + dents * 4.0, 6.0).save(os.path.join(out_dir, 'T_Cabin_PaintN.png'))
    # ribbed rubber: ribs every 1/128 of the repeat along U, slightly irregular and worn
    x = np.arange(n) / n
    ribs = np.abs(np.sin(np.pi * 128 * x))[None, :].repeat(n, axis=0) ** 0.6
    wear = tile_noise(n, 1.3, 6, lo=2, hi=40)
    normal_map(ribs * (0.7 + 0.3 * wear) + tile_noise(n, 0.5, 7, lo=100) * 0.15, 12.0).save(
        os.path.join(out_dir, 'T_Cabin_RibN.png'))
    # leatherette: cells from thresholded band-passed noise, softened
    cells = tile_noise(n, 0.0, 8, lo=60, hi=110)
    grain = np.clip((cells - 0.45) * 4.0, 0, 1)
    grain = np.asarray(Image.fromarray((grain * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)),
                       dtype=np.float32) / 255.0
    normal_map(grain + tile_noise(n, 1.5, 9, lo=4, hi=40) * 0.5, 5.0).save(os.path.join(out_dir, 'T_Cabin_VinylN.png'))
    return ['T_Cabin_Grime', 'T_Cabin_PaintN', 'T_Cabin_RibN', 'T_Cabin_VinylN']


# ---------------------------------------------------------------------------------------------- panel faces

def is_panel_legend(text):
    """A position label as printed on a panel (Cyrillic, roman numerals), not a description ('on', '0-15 km')."""
    return bool(text) and not re.search(r'[a-z]', text)


def similar(part, position):
    a = part.strip().rstrip('.').upper()
    b = position.strip().rstrip('.').upper()
    return bool(a) and bool(b) and (a.startswith(b[:4]) or b.startswith(a[:4]))


def split_parts(label):
    return [p.strip() for p in label.split('—') if p.strip()]


def match_word(part, position):
    """Where in `part` (a label's words) the position's legend starts, or -1: 'УГОЛ ЗРЕНИЯ ШИРОК.' matches
    'ШИРОКИЙ' at word 2."""
    words = part.split(' ')
    for i, w in enumerate(words):
        if similar(w, position.split(' ')[0]):
            return i
    return -1


def toggle_legends(parts, positions):
    """For a two-position switch: (title, legend beside the up position, legend beside the down position). The
    console's position 1 is up (simcore::CabinLayout::PositionAngleDeg); a label's parts are matched to the
    positions by their words, else taken top to bottom."""
    if len(parts) != 2:
        return '', '', ''
    if len(positions) == 2:
        hits = [[match_word(p, pos) for pos in positions] for p in parts]
        for a, b in ((0, 1), (1, 0)):            # part 0 is position a, part 1 is position b
            if hits[0][a] >= 0 and hits[1][b] >= 0:
                words0 = parts[0].split(' ')
                title = ' '.join(words0[:hits[0][a]])
                legend0 = ' '.join(words0[hits[0][a]:])
                legend1 = ' '.join(parts[1].split(' ')[hits[1][b]:])
                by_pos = {a: legend0, b: legend1}
                return title, by_pos[1], by_pos[0]
    return '', parts[0], parts[1]


class Face:
    """One panel's face texture at `ppm` pixels per metre."""

    def __init__(self, panel, ppm):
        self.p = panel
        self.ppm = ppm
        self.w = max(16, int(round(panel['size_m'][0] * ppm)))
        self.h = max(16, int(round(panel['size_m'][1] * ppm)))
        self.img = Image.new('RGB', (self.w, self.h), PLATE)
        self.draw = ImageDraw.Draw(self.img)
        self.dark = np.zeros((self.h, self.w), np.float32)      # grime to multiply in, 0..1
        self.fonts = {}

    def px(self, u, v):
        return ((u + self.p['size_m'][0] / 2) * self.ppm, (self.p['size_m'][1] / 2 - v) * self.ppm)

    def font(self, cap_m, bold=False):
        size = max(6, int(round(cap_m / 0.716 * self.ppm)))   # Arial Narrow's capitals are 0.716 em
        key = (size, bold)
        if key not in self.fonts:
            self.fonts[key] = ImageFont.truetype(font_path(bold), size)
        return self.fonts[key]

    # -- plates and wear ---------------------------------------------------------------------------------------
    def plates(self, blocks, seed):
        rng = np.random.default_rng(seed)
        if not blocks:
            blocks = [{'at_m': [0, 0], 'size_m': self.p['size_m'], 'color': ''}]
        arr = np.asarray(self.img, np.float32)
        yy, xx = np.mgrid[0:self.h, 0:self.w]
        mott = tile_noise(512, 1.4, seed, lo=3)
        mott = np.asarray(Image.fromarray((mott * 255).astype(np.uint8)).resize((self.w, self.h), Image.BILINEAR),
                          np.float32) / 255.0
        for b in blocks:
            (x0, y0), (x1, y1) = self.px(b['at_m'][0] - b['size_m'][0] / 2, b['at_m'][1] + b['size_m'][1] / 2), \
                self.px(b['at_m'][0] + b['size_m'][0] / 2, b['at_m'][1] - b['size_m'][1] / 2)
            x0, y0, x1, y1 = int(x0), int(y0), int(math.ceil(x1)), int(math.ceil(y1))
            colour = np.array(LOOK_COLOURS.get(b.get('color') or '', PLATE), np.float32)
            # each block a slightly different batch of paint (lightness only)
            colour = colour * (1.0 + rng.normal(0, 0.008))
            arr[y0:y1, x0:x1] = colour
            # a little darker towards the bottom of the block (dust)
            t = (yy[y0:y1, x0:x1] - y0) / max(1, y1 - y0)
            self.dark[y0:y1, x0:x1] += 0.035 * t ** 3 + 0.01
            # a thin worn line round the block's edge
            edge = np.minimum.reduce([xx[y0:y1, x0:x1] - x0, x1 - 1 - xx[y0:y1, x0:x1], yy[y0:y1, x0:x1] - y0,
                                      y1 - 1 - yy[y0:y1, x0:x1]]).astype(np.float32)
            self.dark[y0:y1, x0:x1] += 0.08 * np.exp(-edge / (0.0008 * self.ppm))
        self.dark += (mott - 0.5) * 0.035
        self.img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
        self.draw = ImageDraw.Draw(self.img)

    def handled(self, u, v, size_m, amount=0.035):
        """Finger grime round a control the crew handles (faint: the photos show clean, worn paint)."""
        cx, cy = self.px(u, v)
        r = max(size_m, 0.015) * 0.9 * self.ppm
        x0, x1 = int(max(0, cx - 2 * r)), int(min(self.w, cx + 2 * r))
        y0, y1 = int(max(0, cy - 2 * r)), int(min(self.h, cy + 2 * r))
        if x1 <= x0 or y1 <= y0:
            return
        yy, xx = np.mgrid[y0:y1, x0:x1]
        d = np.sqrt((xx - cx) ** 2 + ((yy - cy) * 0.8) ** 2) / r
        self.dark[y0:y1, x0:x1] += amount * np.exp(-d * d)

    def scratches(self, seed, count):
        rng = np.random.default_rng(seed + 100)
        for _ in range(count):
            x, y = rng.uniform(0, self.w), rng.uniform(0, self.h)
            ang = rng.uniform(0, math.pi)
            length = rng.uniform(0.002, 0.012) * self.ppm
            c = int(rng.uniform(-14, 10))
            col = tuple(int(np.clip(v + c, 0, 255)) for v in PLATE)
            self.draw.line([(x, y), (x + math.cos(ang) * length, y + math.sin(ang) * length)], fill=col, width=1)

    def finish(self):
        arr = np.asarray(self.img, np.float32)
        dark = np.clip(self.dark, 0, 0.45)
        arr *= (1.0 - dark)[..., None]
        return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))

    # -- markings ----------------------------------------------------------------------------------------------
    def ink_at(self, u, v):
        x, y = self.px(u, v)
        r, g, b = self.img.getpixel((int(min(max(x, 0), self.w - 1)), int(min(max(y, 0), self.h - 1))))
        return INK_ON_BLACK if r + g + b < 330 else INK

    def text(self, u, v, s, cap_m=CAP_HEIGHT_M, max_w_m=0.12, anchor='mm', bold=LABEL_BOLD):
        """Text centred on (u, v) (anchor 'mm'), on two lines if long, shrunk to fit max_w_m."""
        if not s:
            return
        lines = s.split(chr(10))
        f = self.font(cap_m, bold)
        if len(lines) == 1 and f.getlength(s) > max_w_m * self.ppm and ' ' in s:
            words = s.split(' ')
            best = min(range(1, len(words)), key=lambda k: abs(len(' '.join(words[:k])) - len(' '.join(words[k:]))))
            lines = [' '.join(words[:best]), ' '.join(words[best:])]
        widest = max(f.getlength(line) for line in lines)
        if widest > max_w_m * self.ppm:
            f = self.font(cap_m * max_w_m * self.ppm / widest, bold)
        line_h = f.size * 1.05
        x, y = self.px(u, v)
        y -= (len(lines) - 1) * line_h / 2
        ink = self.ink_at(u, v)
        for i, line in enumerate(lines):
            if line.strip() == '⊥' and anchor == 'mm':     # the earth sign: Arial Narrow has no U+22A5, so drawn
                hgt = 0.75 * f.size
                yy = y + i * line_h
                lw = max(1, int(round(0.11 * hgt)))
                self.draw.line([(x, yy - hgt / 2), (x, yy + hgt / 2)], fill=ink, width=lw)
                self.draw.line([(x - 0.45 * hgt, yy + hgt / 2), (x + 0.45 * hgt, yy + hgt / 2)], fill=ink, width=lw)
                continue
            self.draw.text((x, y + i * line_h), line, fill=ink, font=f, anchor=anchor, stroke_width=stroke_of(f), stroke_fill=ink)

    def text_turned(self, u, v, s, cap_m, angle_deg, bold=LABEL_BOLD):
        """Text centred on (u, v), turned anticlockwise (a chart's upright cells read bottom to top at 90)."""
        f = self.font(cap_m, bold)
        lines = s.split('\n')
        line_h = f.size * 1.05
        w = int(max(f.getlength(line) for line in lines)) + 4
        h = int(line_h * len(lines)) + 4
        mask = Image.new('L', (w, h), 0)
        d = ImageDraw.Draw(mask)
        for i, line in enumerate(lines):
            d.text((w / 2, 2 + line_h * (i + 0.5)), line, fill=255, font=f, anchor='mm', stroke_width=stroke_of(f), stroke_fill=255)
        mask = mask.rotate(angle_deg, expand=True, resample=Image.BICUBIC)
        x, y = self.px(u, v)
        ink = Image.new('RGB', mask.size, self.ink_at(u, v))
        self.img.paste(ink, (int(round(x - mask.width / 2)), int(round(y - mask.height / 2))), mask)

    def frame(self, u, v, w, h):
        (x0, y0), (x1, y1) = self.px(u - w / 2, v + h / 2), self.px(u + w / 2, v - h / 2)
        lw = max(1, int(round(0.0008 * self.ppm)))     # the charts' rules: about 0.8 mm (SAM's AK photo at 5x)
        self.draw.rectangle([x0, y0, x1, y1], outline=self.ink_at(u, v), width=lw)

    def arrowhead(self, u, v, r, a_deg, sense, ink):
        """A filled arrowhead on a printed arc at angle a_deg, pointing along the arc (sense +1: the way the angles
        grow, clockwise; -1: back), 4.5 mm long (SAM's AK photo: the locks' ОТКР. arrows)."""
        a = math.radians(a_deg)
        du, dv = math.sin(a), math.cos(a)                # radial
        tu, tv = dv * sense, -du * sense                 # along the arc
        L, Wd = 0.0045, 0.0019
        tip = (u + du * r + tu * L * 0.6, v + dv * r + tv * L * 0.6)
        back = (u + du * r - tu * L * 0.4, v + dv * r - tv * L * 0.4)
        pts = [tip, (back[0] + du * Wd, back[1] + dv * Wd), (back[0] - du * Wd, back[1] - dv * Wd)]
        self.draw.polygon([self.px(*q) for q in pts], fill=ink)

    def scale(self, u, v, diameter_m, angles_deg, marks, arrow=''):
        """A printed scale round a knob: an arc through the marks' angles, a tick and a figure at each (SAM's AK photo:
        КОМПЕНСАЦИЯ ВЕТРА's 0-270°, СКОРОСТЬ П А's - and +)."""
        ink = self.ink_at(u, v)
        r = diameter_m / 2
        lw = max(1, int(round(0.0005 * self.ppm)))
        a0, a1 = min(angles_deg), max(angles_deg)
        pts = []
        steps = max(8, int(abs(a1 - a0) / 3))
        for k in range(steps + 1):
            a = math.radians(a0 + (a1 - a0) * k / steps)
            pts.append(self.px(u + r * math.sin(a), v + r * math.cos(a)))
        if len(angles_deg) > 1:
            self.draw.line(pts, fill=ink, width=lw)
        if arrow in ('first', 'both'):
            self.arrowhead(u, v, r, angles_deg[0], 1 if angles_deg[0] > angles_deg[-1] else -1, ink)
        if arrow in ('last', 'both'):
            self.arrowhead(u, v, r, angles_deg[-1], 1 if angles_deg[-1] > angles_deg[0] else -1, ink)
        for a, mark in zip(angles_deg, marks):
            if arrow and not mark:
                continue                                # a lock's arc: no ticks at its ends, its words engraved apart
            ra = math.radians(a)
            du, dv = math.sin(ra), math.cos(ra)
            self.draw.line([self.px(u + du * r, v + dv * r), self.px(u + du * (r + 0.004), v + dv * (r + 0.004))], fill=ink,
                           width=lw)
            if mark:
                anchor = 'mm' if abs(du) < 0.3 else ('rm' if du < 0 else 'lm')
                rr = r + 0.006 if anchor == 'mm' else r + 0.0045
                self.text(u + du * rr, v + dv * rr + (0.001 if anchor == 'mm' else 0.0), mark, cap_m=CAP_HEIGHT_M * 0.9,
                          max_w_m=0.03, anchor=anchor)

    def label_at(self, where, u, v, w_m, h_m, s, **kw):
        """An item's label above it (the default), below it, or beside it, as engraved on the real plate."""
        if where in (None, '', 'above'):
            self.label_above(u, v, h_m, s, **kw)
        elif where == 'below':
            self.label_below(u, v, h_m, s, **kw)
        else:
            left = where == 'left'
            words = s.split(' ')
            if len(s) > 8 and len(words) > 1:           # beside an item, a long label stands on two lines (ВЕРТ. / СМЕЩ.)
                best = min(range(1, len(words)), key=lambda k: abs(len(' '.join(words[:k])) - len(' '.join(words[k:]))))
                s = ' '.join(words[:best]) + '\n' + ' '.join(words[best:])
            uu = u - w_m / 2 - GAP_M if left else u + w_m / 2 + GAP_M
            lines = s.split('\n')
            f = self.font(CAP_HEIGHT_M, LABEL_BOLD)
            line_h = f.size * 1.05
            x, y = self.px(uu, v)
            y -= (len(lines) - 1) * line_h / 2
            ink = self.ink_at(uu, v)
            for i, line in enumerate(lines):
                self.draw.text((x, y + i * line_h), line, fill=ink, font=f, anchor='rm' if left else 'lm', stroke_width=stroke_of(f),
                               stroke_fill=ink)

    anchors = ()                     # the labelled items' (u, v) on this face: a label keeps to the room between them

    def room(self, u, v, kw):
        """A label's width: no wider than the gap to the next labelled item on its row (switches 23 mm apart)."""
        gaps = [abs(a[0] - u) for a in self.anchors if abs(a[1] - v) < 0.02 and abs(a[0] - u) > 0.004]
        kw = dict(kw)
        if gaps:
            kw['max_w_m'] = min(kw.get('max_w_m', 0.12), 0.95 * min(gaps))
        return kw

    def label_above(self, u, v, height_m, s, **kw):
        kw = self.room(u, v, kw)
        n_lines = 2 if (len(s) > 18 or self.font(CAP_HEIGHT_M).getlength(s) > kw.get('max_w_m', 0.12) * self.ppm) and ' ' in s else 1
        self.text(u, v + height_m / 2 + GAP_M + n_lines * CAP_HEIGHT_M * 0.75, s, **kw)

    def label_below(self, u, v, height_m, s, **kw):
        kw = self.room(u, v, kw)
        self.text(u, v - height_m / 2 - GAP_M - CAP_HEIGHT_M * 0.75, s, **kw)

    def ticks(self, u, v, radius_m, angles_deg, labels):
        ink = self.ink_at(u, v)
        for a, lab in zip(angles_deg, labels):
            r = math.radians(a)
            du, dv = math.sin(r), math.cos(r)
            p0 = self.px(u + du * (radius_m + 0.001), v + dv * (radius_m + 0.001))
            p1 = self.px(u + du * (radius_m + 0.005), v + dv * (radius_m + 0.005))
            self.draw.line([p0, p1], fill=ink, width=max(1, int(0.0006 * self.ppm)))
            if abs(du) < 0.2:                           # the legend over the top of the knob: centred above its tick
                rr = radius_m + 0.0075
                self.text(u + du * rr, v + dv * rr + 0.0012, lab, cap_m=CAP_HEIGHT_M * 0.85, max_w_m=0.05)
            else:                                       # a side legend: beside its tick, reading away from the knob
                rr = radius_m + 0.0062
                self.text(u + du * rr, v + dv * rr, lab, cap_m=CAP_HEIGHT_M * 0.85, max_w_m=0.05,
                          anchor='rm' if du < 0 else 'lm')

    def diagram(self, u, v, w, h):
        (x0, y0), (x1, y1) = self.px(u - w / 2, v + h / 2), self.px(u + w / 2, v - h / 2)
        lw = max(1, int(0.0005 * self.ppm))
        self.draw.rectangle([x0, y0, x1, y1], outline=DIAGRAM_INK, width=lw)
        # A ruled table (the racks' printed tables, rwd_mb3 9a33_41/45: grids of small print); what the cells say
        # can't be read in any source, so each holds a short dash of "text" instead.
        rows = max(3, int(round(h / 0.012)))
        cols = max(2, int(round(w / 0.022)))
        for k in range(1, rows):
            y = y0 + (y1 - y0) * k / rows
            self.draw.line([(x0, y), (x1, y)], fill=DIAGRAM_INK, width=lw)
        for k in range(1, cols):
            x = x0 + (x1 - x0) * k / cols
            self.draw.line([(x, y0), (x, y1)], fill=DIAGRAM_INK, width=lw)
        rng = np.random.default_rng(int(abs(u * 1000) + abs(v * 1000) * 7))
        for r in range(rows):
            for c in range(cols):
                if rng.random() < 0.8:
                    cx = x0 + (x1 - x0) * (c + 0.5) / cols
                    cy = y0 + (y1 - y0) * (r + 0.5) / rows
                    half = (x1 - x0) / cols * rng.uniform(0.15, 0.35)
                    self.draw.line([(cx - half, cy), (cx + half, cy)], fill=DIAGRAM_INK, width=lw)

    def dial(self, u, v, w, h, black, scale=None, marks=None, unit=''):
        """An instrument face under the cabin's bezel: a black (or white) face with a printed scale, its needle at rest
        and a hub. Round when w == h (a 270 degree scale, 0 to 10, as on the vehicle's dash gauges in rwd_mb3
        9a33_31/32), else a rectangular meter with a 100 degree arc. The scale itself is generic: no source gives
        the dial markings, only that the faces are black with white print. A fixture that gives `marks` (its figures,
        read from a photo) has them printed evenly over the span, with its `unit` under the hub."""
        face = (22, 22, 21) if black else (222, 220, 208)
        ink = (226, 224, 214) if black else (28, 28, 26)
        lw = max(1, int(round(0.0006 * self.ppm)))
        d = self.draw
        if scale == 'compass':
            return self.compass(u, v, w, h, face, ink, lw)
        if scale == 'counters':
            return self.counters(u, v, w, h)
        if scale == 'hours':
            return self.hour_meter(u, v, w)
        round_dial = abs(w - h) < 1e-6
        if round_dial:
            cx, cy = self.px(u, v)
            r = 0.43 * w * self.ppm
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=face)
            pivot, radius, start, span, n_major = (u, v), 0.36 * w, -135.0, 270.0, 10
        else:
            (x0, y0), (x1, y1) = self.px(u - 0.46 * w, v + 0.46 * h), self.px(u + 0.46 * w, v - 0.46 * h)
            d.rectangle([x0, y0, x1, y1], fill=face)
            pivot, radius, start, span, n_major = (u, v - 0.32 * h), 0.78 * h, -50.0, 100.0, 5

        def at(angle_deg, rad):
            a = math.radians(angle_deg)
            return self.px(pivot[0] + math.sin(a) * rad, pivot[1] + math.cos(a) * rad)

        if marks:
            n_major = len(marks) - 1
        steps = n_major * 5
        for k in range(steps + 1):
            a = start + span * k / steps
            major = k % 5 == 0
            d.line([at(a, radius * (0.78 if major else 0.89)), at(a, radius)], fill=ink, width=lw * (2 if major else 1))
        f = self.font((0.085 if marks else 0.07) * w if round_dial else (0.1 * h))
        for k in range(n_major + 1):
            a = start + span * k / n_major
            x, y = at(a, radius * (0.6 if marks else 0.5))   # a photographed dial's figures stand just inside its ticks
            text = marks[k] if marks else str(k * (2 if n_major == 5 else 1))
            d.text((x, y), text, fill=ink, font=f, anchor='mm')
        if unit:
            x, y = self.px(pivot[0], pivot[1] - (0.2 * w if round_dial else 0.12 * h))
            d.text((x, y), unit, fill=ink, font=self.font((0.06 * w) if round_dial else (0.08 * h)), anchor='mm')
        # needle at rest: on the photographed dial's zero (the ammeter's is in the middle), else a little off the stop
        rest = (marks.index('0') if '0' in marks else 0) / n_major if marks else 0.18
        a = start + rest * span
        d.line([at(a, 0), at(a, radius * 0.92)], fill=ink if black else (150, 30, 20), width=lw * 2)
        hx, hy = self.px(*pivot)
        hr = (0.05 * w if round_dial else 0.07 * h) * self.ppm
        d.ellipse([hx - hr, hy - hr, hx + hr, hy + hr], fill=(70, 70, 68) if black else (60, 60, 58))

    def hour_meter(self, u, v, w):
        """The engine-hour meter on the driver's instrument plate (wwp_gecko g085-a): a round grey face with a black
        window of white digit wheels across its middle. The hours shown are a placeholder."""
        d = self.draw
        cx, cy = self.px(u, v)
        r = 0.45 * w * self.ppm
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(150, 152, 150), outline=(95, 96, 94),
                  width=max(1, int(round(0.0008 * self.ppm))))
        (x0, y0), (x1, y1) = self.px(u - 0.27 * w, v + 0.075 * w), self.px(u + 0.27 * w, v - 0.075 * w)
        d.rectangle([x0, y0, x1, y1], fill=(18, 18, 17))
        f = self.font(0.11 * w)
        for k, ch in enumerate('01234'):
            x, _ = self.px(u - 0.216 * w + 0.108 * w * k, v)
            d.text((x, cy), ch if k < 4 else '7', fill=(222, 220, 210) if k < 4 else (200, 40, 30), font=f, anchor='mm')

    def counters(self, u, v, w, h):
        """The SOC/SSC readout strip (0094-5; opisybroni 54539, and a Polish-modernised console in user_modernised_2,
        where this unit is unchanged): a pale window holding eight round amber-lit digit counters, each in a dark
        bezel, with a lamp at the right end. The digits shown are placeholders ("000"); the captions printed under
        the real counters are not drawn (the unit is too short for them here)."""
        d = self.draw
        (x0, y0), (x1, y1) = self.px(u - 0.5 * w, v + 0.5 * h), self.px(u + 0.5 * w, v - 0.5 * h)
        d.rounded_rectangle([x0, y0, x1, y1], radius=0.2 * (y1 - y0), fill=(206, 204, 194), outline=(60, 58, 52),
                            width=max(1, int(round(0.0007 * self.ppm))))
        usable = 0.88 * w
        pitch = usable / 8.0
        dia = min(0.84 * h, 0.8 * pitch)
        f = self.font(0.3 * dia)
        for k in range(8):
            cx, cy = self.px(u - 0.5 * w + 0.03 * w + pitch * (k + 0.5), v)
            r = 0.5 * dia * self.ppm
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(40, 32, 26))
            r2 = 0.84 * r
            d.ellipse([cx - r2, cy - r2, cx + r2, cy + r2], fill=(232, 152, 52))
            d.text((cx, cy), '000', fill=(70, 34, 8), font=f, anchor='mm')
        lx, ly = self.px(u + 0.5 * w - 0.045 * w, v)
        lr = 0.2 * dia * self.ppm
        d.ellipse([lx - lr, ly - lr, lx + lr, ly + lr], fill=(150, 90, 40), outline=(60, 58, 52))

    def compass(self, u, v, w, h, face, ink, lw):
        """A gyro-compass repeater's card: a rotating ring graduated every 5 degrees with the tens of degrees
        (0 to 35) every 30, a lubber mark at the top and a hub, on a face of w x h (the card fits the shorter side).
        Generic: the unit's real card is not legible in rwd_mb3 9a33_52."""
        d = self.draw
        (x0, y0), (x1, y1) = self.px(u - 0.5 * w, v + 0.5 * h), self.px(u + 0.5 * w, v - 0.5 * h)
        d.rectangle([x0, y0, x1, y1], fill=face)
        r = 0.44 * min(w, h)
        cx, cy = self.px(u, v)
        d.ellipse([cx - r * self.ppm, cy - r * self.ppm, cx + r * self.ppm, cy + r * self.ppm], outline=ink, width=lw)

        def at(deg, rad):
            a = math.radians(deg)
            return self.px(u + math.sin(a) * rad, v + math.cos(a) * rad)

        for k in range(72):
            deg = 5.0 * k
            major = k % 6 == 0
            d.line([at(deg, r * (0.84 if major else 0.92)), at(deg, r)], fill=ink, width=lw * (2 if major else 1))
        f = self.font(0.1 * min(w, h))
        for k in range(12):
            x, y = at(30.0 * k, r * 0.68)
            d.text((x, y), str(3 * k), fill=ink, font=f, anchor='mm')
        top = self.px(u, v + r + 0.004)
        tri = 0.012 * min(w, h) / 0.12 * self.ppm
        d.polygon([(top[0] - tri, top[1] - tri * 1.6), (top[0] + tri, top[1] - tri * 1.6), (top[0], top[1])], fill=(190, 40, 30))
        hr = 0.04 * min(w, h) * self.ppm
        d.ellipse([cx - hr, cy - hr, cx + hr, cy + hr], fill=(70, 70, 68))

    def red_ring(self, u, v, radius_m):
        """Nothing: the red working-position dots are modelled by the cabin."""


def position_angles(style, positions):
    """simcore::CabinLayout::PositionAngleDeg for every position."""
    if style == 'toggle':
        return [180.0 if p == 0 else 0.0 for p in range(positions)]
    count = max(positions, 2)
    step = min(30.0, 240.0 / (count - 1))
    return [(p - 0.5 * (count - 1)) * step for p in range(positions)]


def control_style(ctl, defn, layout_controls):
    if ctl.get('style'):
        return ctl['style']
    kind = defn.get('kind')
    if kind == 'button':
        return 'button'
    if kind == 'wheel':
        return 'wheel'
    if kind == 'key':
        return 'key'
    n = len(defn.get('position_labels') or [])
    return 'rotary' if n > 2 else 'toggle'


def item_height(look, default):
    w = look.get('size_m')
    if isinstance(w, list):
        return w[1]
    return w or default


def bake_panel(layout, console, panel, ppm, seed):
    face = Face(panel, ppm)
    blocks = [b for b in layout.get('blocks', []) if b['panel'] == panel['id']]
    face.plates(blocks, seed)
    defs = {c['id']: c for c in console['controls']}
    pid = panel['id']
    lamp_labels = [(lp['at_m'], lp.get('look', {}).get('label') or lp['label']) for lp in layout['lamps'] if lp['panel'] == pid]
    face.anchors = ([c['at_m'] for c in layout['controls'] if c['panel'] == pid and c.get('look', {}).get('label')]
                    + [lp['at_m'] for lp in layout['lamps'] if lp['panel'] == pid and lp.get('look', {}).get('label')]
                    + [f['at_m'] for f in layout.get('fixtures', []) if f['panel'] == pid and f.get('label')
                       and f['kind'] not in ('nameplate', 'bezel', 'window', 'legend', 'diagram')])
    # controls: grime where hands go, then labels
    for ctl in layout['controls']:
        if ctl['panel'] != pid:
            continue
        look = ctl.get('look', {})
        defn = defs.get(ctl['control'], {})
        style = control_style(ctl, defn, layout['controls'])
        size = item_height(look, 0.12 if style == 'wheel' else 0.03)
        u, v = ctl['at_m']
        face.handled(u, v, size, 0.05 if style in ('wheel', 'button') else 0.035)
        label = look.get('label', '')
        if not label:
            continue
        # a button under a lamp of the same name: the lamp's label serves both (ПА, ТСЦ, ЦУ, АЗ, РУ)
        if any(lab == label and abs(at[0] - u) < 0.02 and 0 < at[1] - v < 0.06 for at, lab in lamp_labels):
            continue
        parts = split_parts(label)
        positions = defn.get('position_labels') or []
        if any(f['kind'] == 'scale' and f['panel'] == pid and
               ((abs(f['at_m'][0] - u) < 0.002 and abs(f['at_m'][1] - v) < 0.002) or
                (any(f.get('marks', [])) and math.hypot(f['at_m'][0] - u, f['at_m'][1] - v) < 0.008))
               for f in layout.get('fixtures', [])):
            continue                                    # its printed scale carries its legends (and a text its title)
        if style in ('rotary', 'key') and len(positions) > 2:
            angles = position_angles('rotary', len(positions))
            if len(parts) == len(positions):
                face.ticks(u, v, size / 2, angles, parts)
            elif all(is_panel_legend(p) for p in positions) and all(len(p) <= 8 for p in positions):
                face.ticks(u, v, size / 2, angles, positions)
                face.label_above(u, v, size + 0.026, label)
            else:
                face.label_above(u, v, size, label)
        elif style == 'toggle' and len(parts) == 2 and look.get('kind') not in ('jack', 'socket'):
            title, up, down = toggle_legends(parts, positions)
            if look.get('up', 1) == 0:             # position 0 stands up (УГОЛ ЗРЕНИЯ's ШИРОКИЙ on SAM's AK photo)
                up, down = down, up
            lever = size + TOGGLE_LEVER_M          # the up legend clears the lever's tip (SAM's AK photo: 16 mm up, 14 down)
            face.label_above(u, v, lever, up)
            face.label_below(u, v, size, down)
            if title:
                face.label_above(u, v, lever + 2 * (CAP_HEIGHT_M + GAP_M), title)
        else:
            w = look.get('size_m', size)
            face.label_at(look.get('label_at'), u, v, w[0] if isinstance(w, list) else w, size, label)
    # lamps (legend lamps carry their own text)
    for lamp in layout['lamps']:
        if lamp['panel'] != pid:
            continue
        look = lamp.get('look', {})
        if look.get('kind') == 'legend' or not look.get('label'):
            continue
        size = item_height(look, 0.022) * (1.4 if look.get('kind') in (None, 'lamp', 'lamp_button') else 1.0)
        face.label_at(look.get('label_at'), lamp['at_m'][0], lamp['at_m'][1], size, size, look['label'])
    # fixtures
    for fx in layout.get('fixtures', []):
        if fx['panel'] != pid:
            continue
        kind = fx['kind']
        u, v = fx['at_m']
        size = fx.get('size_m', 0.02)
        w, h = (size if isinstance(size, list) else (size, size))
        if kind == 'diagram':
            face.diagram(u, v, w, h)
            continue
        if kind == 'text':
            cap = h if isinstance(size, list) else size
            if fx.get('angle_deg'):
                face.text_turned(u, v, fx['label'], cap, fx['angle_deg'])
            else:                                       # "label_at": "right" runs right from its point, "left" ends there
                face.text(u, v, fx['label'], cap_m=cap, max_w_m=w if isinstance(size, list) else 0.2,
                          anchor={'right': 'lm', 'left': 'rm'}.get(fx.get('label_at'), 'mm'))
            continue
        if kind == 'scale':
            face.scale(u, v, w, fx['angles_deg'], fx['marks'], fx.get('arrow', ''))
            continue
        if kind == 'frame':
            face.frame(u, v, w, h)
            continue
        if kind in ('nameplate', 'bezel', 'window', 'legend'):
            continue
        if kind == 'gauge':
            face.dial(u, v, w, h, fx.get('color') == 'black', fx.get('scale'), fx.get('marks'), fx.get('unit', ''))
            if fx.get('scale') == 'counters':
                continue
        if kind in ('knob', 'pointer_knob', 'bar_knob', 'trim', 'toggle', 'lever', 'button', 'lamp_button'):
            face.handled(u, v, max(w, h), 0.025)
        label = fx.get('label', '')
        if not label:
            continue
        sides = [p.strip() for p in label.split('—')]
        if kind == 'toggle' and len(sides) == 2:            # "UP — DOWN"; either side may be empty ("— ГОТ.": below only)
            if sides[0]:
                face.label_above(u, v, h, sides[0])
            if sides[1]:
                face.label_below(u, v, h, sides[1])
        else:
            face.label_at(fx.get('label_at'), u, v, w, h if kind == 'oval' else max(w, h), label)
    face.scratches(seed, int(panel['size_m'][0] * panel['size_m'][1] * 160))
    return face.finish()


def pow2(n, cap=4096):
    p = 256
    while p < n and p < cap:
        p *= 2
    return p


def bake_panels(out_dir, preview_dir=None):
    with open(os.path.join(REPO, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8') as f:
        layout = json.load(f)
    with open(os.path.join(REPO, 'data', 'procedures', 'osa_ak.json'), encoding='utf-8') as f:
        console = json.load(f)
    written = []
    for i, panel in enumerate(layout['panels']):
        pid = panel['id']
        dense = pid in ('fo_panel', 'so_panel', 'op_panel')
        ppm = 4000 if dense else 3000
        img = bake_panel(layout, console, panel, ppm, 11 + i)
        size = (pow2(img.width), pow2(img.height))
        img.resize(size, Image.LANCZOS).save(os.path.join(out_dir, 'T_Panel_%s.png' % pid))
        written.append(pid)
        if preview_dir:
            os.makedirs(preview_dir, exist_ok=True)
            img.thumbnail((1400, 1400))
            img.save(os.path.join(preview_dir, '%s.jpg' % pid), quality=88)
    return written


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', choices=['panels', 'details'])
    ap.add_argument('--preview', help='also write small JPEG previews of the panel faces here')
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    if args.only != 'panels':
        print('details:', ', '.join(bake_details(OUT)))
    if args.only != 'details':
        print('panels:', ', '.join(bake_panels(OUT, args.preview)))


if __name__ == '__main__':
    main()
