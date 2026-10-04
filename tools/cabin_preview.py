"""Draw a cabin panel in 2D from data/cabins/<system>.json alone, to check the layout against a reference picture.

Blocks, fixtures, controls, lamps and displays are drawn at their places and sizes (1 mm = `--px-per-mm` pixels),
looking roughly like what they are (`look` in the cabin data). With `--photo` the picture is shown beside the drawing
and blended over it, so misplaced items stand out. Reference pictures (e.g. SAM Simulator's panel photos) stay in
reference/ and the outputs in out/: neither is committed.

    python tools/cabin_preview.py op_panel --out out/preview/op_panel.png
    python tools/cabin_preview.py op_panel --photo reference/samsim/exe_img/j_007660125_610x982.jpg --out out/preview/op_cmp.png
"""
import argparse
import json
import os

from PIL import Image, ImageDraw, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = 'C:/Windows/Fonts/ARIALN.TTF' if os.path.exists('C:/Windows/Fonts/ARIALN.TTF') else None

COLORS = {
    'black': (25, 25, 25), 'white': (225, 222, 210), 'grey': (150, 150, 145), 'chrome': (200, 200, 200),
    'brass': (190, 160, 90), 'red': (190, 30, 25), 'green': (40, 200, 70), 'amber': (230, 150, 30),
    'yellow': (225, 190, 80), 'orange': (200, 90, 40),
}
PLATE = (205, 203, 192)
PLATE_EDGE = (120, 118, 108)
BACK = (60, 60, 58)
INK = (40, 40, 40)


def font(size):
    return ImageFont.truetype(FONT, size) if FONT else ImageFont.load_default()


def load(system):
    cab = json.load(open(os.path.join(REPO, 'data', 'cabins', system + '.json'), encoding='utf-8'))
    con = json.load(open(os.path.join(REPO, 'data', 'procedures', system + '.json'), encoding='utf-8'))
    return cab, {c['id']: c for c in con['controls']}


def draw_panel(cab, controls, panel_id, k):
    panel = next(p for p in cab['panels'] if p['id'] == panel_id)
    w_m, h_m = panel['size_m']
    img = Image.new('RGB', (round(w_m * 1000 * k), round(h_m * 1000 * k)), BACK)
    d = ImageDraw.Draw(img)

    def px(u, v):
        return ((u + w_m / 2) * 1000 * k, (h_m / 2 - v) * 1000 * k)

    def size(look, default):
        s = look.get('size_m', default)
        return (s, s) if isinstance(s, (int, float)) else tuple(s)

    def circle(c, dia, fill, outline=INK, width=1):
        r = dia * 1000 * k / 2
        d.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r], fill=fill, outline=outline, width=width)

    def label(c, text, dy):
        if text:
            f = font(max(8, round(7 * k)))
            d.text((c[0], c[1] - dy), text, fill=INK, font=f, anchor='md')

    def item(c, kind, look, default_size, color=None):
        sw, sh = size(look, default_size)
        col = COLORS.get(look.get('color', color or 'black'), COLORS['black'])
        ring = COLORS.get(look.get('ring', ''), None)
        hw, hh = sw * 1000 * k / 2, sh * 1000 * k / 2
        if kind == 'lamp' and abs(sw - sh) > 1e-6:
            d.rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], fill=col, outline=INK)
        elif kind in ('knob', 'pointer_knob', 'socket', 'jack', 'lamp', 'trim', 'latch'):
            circle(c, sw, col)
            if kind == 'pointer_knob':
                d.line([c, (c[0], c[1] - hw)], fill=(240, 240, 240), width=max(1, round(k)))
            if kind == 'trim':
                d.line([(c[0] - hw * 0.7, c[1]), (c[0] + hw * 0.7, c[1])], fill=INK, width=max(1, round(k)))
            if kind == 'socket':
                circle(c, sw * 0.45, INK)
            if kind == 'latch':
                d.rectangle([c[0] - hw * 0.25, c[1] - hw * 0.6, c[0] + hw * 0.25, c[1] + hw * 0.6], fill=INK)
        elif kind == 'legend':
            # Black glass with the label on it, drawn unlit (white letters).
            if abs(sw - sh) > 1e-6:
                d.rounded_rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], radius=4 * k, fill=INK)
            else:
                circle(c, sw, INK)
            text = look.get('label', '')
            lines = text.split(' ', 1) if len(text) > 6 and ' ' in text else [text]
            f = font(max(6, round(min(hh, 5 * k) * 1.3 / len(lines))))
            d.multiline_text(c, '\n'.join(lines), fill=COLORS['white'], font=f,
                             anchor='mm', align='center', spacing=0)
            return
        elif kind in ('lamp_button', 'button'):
            circle(c, sw, ring or COLORS['chrome'])
            circle(c, sw * 0.65, col)
        elif kind == 'toggle':
            circle(c, sw, COLORS['chrome'])
            d.line([c, (c[0], c[1] - hw * 1.6)], fill=(235, 235, 235), width=max(2, round(2 * k)))
        elif kind == 'lever':
            d.rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], fill=col, outline=INK)
        elif kind == 'nameplate':
            d.rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], fill=(230, 228, 218), outline=INK)
            d.text(c, look.get('label', ''), fill=INK, font=font(max(8, round(9 * k))), anchor='mm')
            return
        elif kind in ('bezel', 'window', 'oval'):
            if kind == 'oval':
                d.ellipse([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], fill=col, outline=INK)
            else:
                d.rounded_rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], radius=12 * k, fill=col, outline=INK)
        elif kind == 'diagram':
            d.rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], outline=(140, 120, 70), width=max(1, round(k)))
        elif kind == 'wheel':
            circle(c, sw, col, width=2)
            circle(c, sw * 0.15, INK)
        if kind not in ('bezel', 'window', 'oval', 'diagram', 'nameplate'):
            label(c, look.get('label', ''), sh * 1000 * k / 2 + 2)

    for b in cab.get('blocks', []):
        if b['panel'] != panel_id:
            continue
        c = px(*b['at_m'])
        hw, hh = b['size_m'][0] * 500 * k, b['size_m'][1] * 500 * k
        d.rounded_rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], radius=6 * k, fill=PLATE, outline=PLATE_EDGE,
                            width=max(1, round(2 * k)))
    order = {'bezel': 0, 'window': 0, 'oval': 0, 'diagram': 0}
    for f in sorted([f for f in cab.get('fixtures', []) if f['panel'] == panel_id], key=lambda f: order.get(f['kind'], 1)):
        item(px(*f['at_m']), f['kind'], f, 0.02)
    for disp in cab['displays']:
        if disp['panel'] != panel_id:
            continue
        c = px(*disp['at_m'])
        hw, hh = disp['size_m'][0] * 500 * k, disp['size_m'][1] * 500 * k
        if disp['kind'] in ('dial', 'meter'):
            d.rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], fill=(230, 225, 200), outline=INK)
        elif disp['kind'] in ('ppi', 'elevation_scope'):
            d.ellipse([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], fill=(15, 30, 15), outline=INK)
        else:
            d.rectangle([c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh], fill=(15, 30, 15), outline=INK)
        d.text(c, disp['label'], fill=(120, 200, 120), font=font(max(8, round(8 * k))), anchor='mm')
    for c in cab['controls']:
        if c['panel'] != panel_id:
            continue
        look = dict(c.get('look', {}))
        kind = look.get('kind')
        cdef = controls[c['control']]
        if not kind:
            kind = {'button': 'button', 'wheel': 'wheel'}.get(cdef['kind'],
                                                              'pointer_knob' if len(cdef.get('position_labels', [])) > 2
                                                              else 'toggle')
        look.setdefault('label', cdef['label'])
        default = 0.12 if kind == 'wheel' else 0.025
        item(px(*c['at_m']), kind, look, default, 'white' if kind == 'wheel' else 'black')
    for l in cab['lamps']:
        if l['panel'] != panel_id:
            continue
        look = dict(l.get('look', {}))
        look.setdefault('color', l['color'])
        look.setdefault('label', l['label'])
        item(px(*l['at_m']), look.get('kind', 'lamp'), look, 0.016, l['color'])
    return img


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('panel')
    ap.add_argument('--system', default='osa_ak')
    ap.add_argument('--px-per-mm', type=float, default=1.0)
    ap.add_argument('--photo', help='reference picture of the same panel at the same scale')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    cab, controls = load(a.system)
    img = draw_panel(cab, controls, a.panel, a.px_per_mm)
    if a.photo:
        photo = Image.open(a.photo).convert('RGB').resize(img.size)
        blend = Image.blend(photo, img, 0.5)
        out = Image.new('RGB', (img.width * 3 + 20, img.height), (0, 0, 0))
        out.paste(photo, (0, 0))
        out.paste(img, (img.width + 10, 0))
        out.paste(blend, (2 * img.width + 20, 0))
        img = out
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    img.save(a.out)
    print(a.out)


if __name__ == '__main__':
    main()
