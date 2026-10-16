"""Draw the traced R-123M front panel (tools/blender/r123m_trace.py) as a clean sketch, and optionally over the photograph it
was traced from, to check the trace.

    python tools/r123m_sketch.py                              # out/r123m/front_sketch.png
    python tools/r123m_sketch.py --photo path/to/photo.jpg    # also out/r123m/front_overlay.png (the trace in red on the photo)
    python tools/r123m_sketch.py --out docs/research/r123m_front_trace.png

Standard library plus Pillow. The sketch is drawn at the photo's own pixel frame (2000 x 1500), cropped to the plate.
"""
import argparse
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'blender'))
import r123m_trace as T  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = r'C:\Windows\Fonts\ARIALN.TTF'


def font(px):
    for f in (FONT, '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf', '/Library/Fonts/Arial Narrow.ttf'):
        if os.path.exists(f):
            return ImageFont.truetype(f, max(8, int(px)))
    return ImageFont.load_default()


def draw_trace(d, ink, fill_plate=None, scale=1.0, dx=0, dy=0, text=True):
    def p(x, y):
        return ((x - dx) * scale, (y - dy) * scale)

    def circle(c, r, **kw):
        x, y = p(*c)
        rr = r * scale
        d.ellipse([x - rr, y - rr, x + rr, y + rr], **kw)

    def rect(r, **kw):
        x0, y0 = p(r[0], r[1])
        x1, y1 = p(r[2], r[3])
        d.rectangle([x0, y0, x1, y1], **kw)

    x0, y0 = p(T.PLATE_PX[0], T.PLATE_PX[1])
    x1, y1 = p(T.PLATE_PX[2], T.PLATE_PX[3])
    d.rounded_rectangle([x0, y0, x1, y1], radius=T.CORNER_PX * scale, outline=ink, width=3, fill=fill_plate)
    for w in T.WELLS:
        circle(w['c'], w['r'], outline=ink, width=3)
        circle(w['c'], w['r'] * 0.86, outline=ink, width=1)
        x, y = p(*w['c'])
        d.line([x - 6, y, x + 6, y], fill=ink, width=2)
        d.line([x, y - 6, x, y + 6], fill=ink, width=2)
    circle(T.DRUM['c'], T.DRUM['r_opening'], outline=ink, width=3)
    circle(T.DRUM['c'], T.DRUM['r_cap'], outline=ink, width=2)
    cx, cy = T.DRUM['c']
    hw, hh = T.DRUM['latch_w'] / 2, T.DRUM['latch_h'] / 2
    d.polygon([p(cx - hw, cy), p(cx - hw * 0.4, cy - hh), p(cx + hw * 0.4, cy - hh), p(cx + hw, cy), p(cx + hw * 0.4, cy + hh),
               p(cx - hw * 0.4, cy + hh)], outline=ink)
    rect(T.DRUM['hinge'], outline=ink, width=2)
    for (hx, hy, hr) in T.HOLES:
        circle((hx, hy), hr, outline=ink, width=2)
    for s in T.SCREWS:
        circle(s, 11, outline=ink, width=2)
        x, y = p(*s)
        d.line([x - 7 * scale, y - 4 * scale, x + 7 * scale, y + 4 * scale], fill=ink, width=2)
    for pl in T.PLATES:
        rect(pl['rect'], outline=ink, width=2)
    def arch(r, **kw):
        a0, b0, a1, b1 = r
        rr = (a1 - a0) / 2
        cy = b0 + rr
        pts = [p(a0, cy)] + [p(0.5 * (a0 + a1) - rr * math.cos(math.pi * k / 16), cy - rr * math.sin(math.pi * k / 16))
                             for k in range(17)] + [p(a1, b1), p(a0, b1), p(a0, cy)]
        d.line(pts, fill=kw.get('outline'), width=kw.get('width', 2))

    for win in T.WINDOWS:
        (arch if win.get('arch') else rect)(win['rect'], outline=ink, width=3)
        if win['bezel']:
            rect(win['bezel'], outline=ink, width=2)
    circle(T.LENS['c'], T.LENS['r'], outline=ink, width=3)
    circle(T.LENS['c'], T.LENS['r'] * 0.6, outline=ink, width=2)
    circle(T.LENS['c'], T.LENS['r'] * 0.25, outline=ink, width=2)
    circle(T.RED_LAMP['c'], T.RED_LAMP['r'], outline=ink, width=3)
    rect(T.LAMP_WINDOWS['panel'], outline=ink, width=2)
    for c in T.LAMP_WINDOWS['c']:
        circle(c, T.LAMP_WINDOWS['r'], outline=ink, width=2)
    rect(T.FIXATORS['panel'], outline=ink, width=3)
    rect(T.FIXATORS['flap'], outline=ink, width=2)
    arch(T.FIXATORS['notch'], outline=ink, width=2)
    for c in T.FIXATORS['buttons']:
        circle(c, T.FIXATORS['r'], outline=ink, width=3)
    rect(T.CONNECTOR['housing'], outline=ink, width=2)
    circle(T.CONNECTOR['c'], T.CONNECTOR['r'], outline=ink, width=3)
    rect(T.SOCKET_PANEL, outline=ink, width=2)
    for c in T.SMALL_SOCKETS:
        circle(c, 24, outline=ink, width=2)
    for c in T.SIDE_BOLTS:
        circle(c, 16, outline=ink, width=2)
    if text:
        for pl in T.PLATES:
            cxp = 0.5 * (pl['rect'][0] + pl['rect'][2])
            cyp = 0.5 * (pl['rect'][1] + pl['rect'][3])
            if pl.get('label_at'):
                cxp, cyp = pl['label_at']
            lines = pl['text']
            h = pl['h']
            for i, line in enumerate(lines):
                off = (i - (len(lines) - 1) / 2) * h * 1.25
                img = Image.new('RGBA', (int(len(line) * h * 0.8 * scale + 20), int(h * 1.8 * scale)), (0, 0, 0, 0))
                ImageDraw.Draw(img).text((4, 2), line, font=font(h * scale), fill=ink)
                rot = pl.get('rot', 0)
                img = img.rotate(rot, expand=True)
                px, py = p(cxp, cyp + (off if rot == 0 else 0))
                d._image.paste(img, (int(px - img.width / 2), int(py - img.height / 2)), img)
        for t, c, h, rot in T.VOLT_LABELS:
            img = Image.new('RGBA', (int(len(t) * h * 0.8 * scale + 20), int(h * 1.8 * scale)), (0, 0, 0, 0))
            ImageDraw.Draw(img).text((4, 2), t, font=font(h * scale), fill=ink)
            img = img.rotate(rot, expand=True)
            px, py = p(*c)
            d._image.paste(img, (int(px - img.width / 2), int(py - img.height / 2)), img)
        for t, c, h in T.MARKS:
            x, y = p(*c)
            d.text((x - h * 0.3 * scale, y - h * 0.6 * scale), t, font=font(h * scale), fill=ink)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--photo')
    ap.add_argument('--out', default=os.path.join(REPO, 'out', 'r123m', 'front_sketch.png'))
    args = ap.parse_args()
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    pad = 30
    x0, y0, x1, y1 = T.PLATE_PX
    sk = Image.new('RGB', (x1 - x0 + 2 * pad, y1 - y0 + 2 * pad), (246, 244, 236))
    d = ImageDraw.Draw(sk)
    draw_trace(d, (30, 30, 34), fill_plate=(232, 230, 220), dx=x0 - pad, dy=y0 - pad)
    d.text((pad + 6, pad // 4), 'R-123M front plate, traced: %.0f x %.0f mm; knob wells, plates, windows, drum and fixators' %
           (T.PLATE_W_M * 1000, T.PLATE_H_M * 1000), font=font(20), fill=(90, 90, 90))
    sk.save(args.out)
    print('wrote', args.out)
    if args.photo:
        photo = Image.open(args.photo).convert('RGB').resize((T.REF_W, T.REF_H))
        ov = photo.copy()
        draw_trace(ImageDraw.Draw(ov), (255, 30, 30), text=False)
        out = os.path.join(os.path.dirname(args.out), 'front_overlay.png')
        ov.crop((0, 100, 2000, 1300)).save(out)
        print('wrote', out)


if __name__ == '__main__':
    main()
