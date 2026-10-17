"""Draw the traced R-123M front (tools/blender/r123m_trace.py) as a clean sketch, and over the flattened photograph it was
traced from, to check the trace.

    python tools/r123m_sketch.py                                 # out/r123m/front_sketch.png
    python tools/r123m_sketch.py --photo out/traces/r123m_front/front_mm.png   # also out/r123m/front_overlay.png
    python tools/r123m_sketch.py --out docs/research/r123m_front_trace.png

The drawing is at 5 px/mm with a 40 mm margin: the frame of the flattened photo tools/photo_trace.py rectify writes (front_mm.png), so
the two lie on each other. Standard library plus Pillow.
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
K, MARGIN = 5.0, 40.0          # px per mm, mm round the front (as tools/photo_trace.py)


def font(px):
    for f in (FONT, '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf', '/Library/Fonts/Arial Narrow.ttf'):
        if os.path.exists(f):
            return ImageFont.truetype(f, max(8, int(px)))
    return ImageFont.load_default()


def p(x, y):
    return ((x + MARGIN) * K, (y + MARGIN) * K)


def draw_trace(img, ink, text=True):
    d = ImageDraw.Draw(img)

    def circle(c, r, width=2):
        x, y = p(*c)
        d.ellipse([x - r * K, y - r * K, x + r * K, y + r * K], outline=ink, width=width)

    def rect(r, width=2, radius=0):
        x0, y0 = p(r[0], r[1])
        x1, y1 = p(r[2], r[3])
        d.rounded_rectangle([x0, y0, x1, y1], radius=radius * K, outline=ink, width=width)

    def poly_m(pts, width=2):
        # metres (a, b) -> px
        q = [p(a * 1000 + T.W_MM / 2, T.H_MM / 2 - b * 1000) for a, b in pts]
        d.line(q + [q[0]], fill=ink, width=width)

    def words(t, c, h, rot=0):
        if not text:
            return
        f = font(h * K / 0.72)
        w = int(d.textlength(t, font=f)) + 8
        tile = Image.new('RGBA', (w, int(h * K / 0.72 * 1.4) + 4), (0, 0, 0, 0))
        ImageDraw.Draw(tile).text((4, 0), t, font=f, fill=ink)
        tile = tile.rotate(rot, expand=True, resample=Image.BICUBIC)
        x, y = p(*c)
        img.paste(tile, (int(x - tile.width / 2), int(y - tile.height / 2)), tile)

    poly_m(T.plate_outline_m(), 3)
    for w in T.WELLS:
        circle(w['c'], w['r'], 3)
    circle(T.DRUM['c'], T.DRUM['r_opening'], 3)
    circle(T.DRUM['c'], T.DRUM['r_cap'], 1)
    rect(T.DRUM['hinge'])
    rect(T.DRUM['plate'])
    for x, y, r in T.HOLES:
        circle((x, y), r)
    for s in T.SCREWS + T.FIXATORS['screws'] + T.LAMP_PLATE['screws'] + T.HOOD['screws']:
        circle(s, 2.6)
    for x, y, r in T.BRASS_HEADS + T.BRASS_IN_HOLE:
        circle((x, y), r)
    for pl in T.PLATES:
        rect(pl['rect'], radius=1.5)
        for i, line in enumerate(pl['text']):
            c = pl.get('at', ((pl['rect'][0] + pl['rect'][2]) / 2, (pl['rect'][1] + pl['rect'][3]) / 2))
            off = (i - (len(pl['text']) - 1) / 2) * pl['h'] * 1.5
            rot = pl.get('rot', 0)
            r = math.radians(rot)
            words(line, (c[0] + off * math.sin(r), c[1] + off * math.cos(r)), pl['h'], rot)
    rect(T.VOLT_PLATE, radius=1.5)
    rect(T.POCKET, radius=3)
    for c in T.BAT_HANDLES:
        circle(c, 3.6)
    for t, c, h, rot, _ in T.LABELS:
        words(t, c, h, rot)
    for arc in T.ARCS:
        wc = T.well(arc['well'])['c']
        r0, r1 = arc['r']
        a0, a1 = arc['a']
        pts = []
        for k in range(25):
            t = math.radians(a0 + (a1 - a0) * k / 24)
            pts.append(p(wc[0] + r1 * math.cos(t), wc[1] - r1 * math.sin(t)))
        for k in range(25):
            t = math.radians(a1 - (a1 - a0) * k / 24)
            pts.append(p(wc[0] + r0 * math.cos(t), wc[1] - r0 * math.sin(t)))
        d.line(pts + [pts[0]], fill=ink, width=2)
        for _, ang in arc['studs']:
            t = math.radians(ang)
            circle((wc[0] + arc['r_stud'] * math.cos(t), wc[1] - arc['r_stud'] * math.sin(t)), 1.8)
    hd = T.HOOD
    rect(hd['body'])
    rect(hd['window'], 3)
    rect(hd['lip'], radius=hd['lip_r'])
    words(hd['label'][0], hd['label'][1], hd['label'][2])
    circle(T.LENS['c'], T.LENS['r'], 3)
    circle(T.LENS['c'], T.LENS['r'] * 0.6)
    circle(T.RED_LAMP['c'], T.RED_LAMP['r'], 3)
    fx = T.FIXATORS
    rect(fx['recess'], 3)
    rect(fx['panel'])
    rect(fx['flap'])
    poly_m(T.arch_m(fx['notch']))
    for c in fx['buttons']:
        circle(c, fx['r'], 3)
    for c in fx['rivets']:
        circle(c, 1.3)
    for t, c, h in T.MARKS:
        words(t, c, h)
    poly_m(T.arch_m(T.DWIN['rect']), 3)
    poly_m(T.arch_m(T.DWIN['rect'], grow=T.DWIN['bezel_grow']))
    rect(T.LAMP_PLATE['rect'], radius=2)
    for c in T.LAMP_PLATE['c']:
        circle(c, T.LAMP_PLATE['r'])
    rect(T.CONNECTOR['housing'], radius=4)
    circle(T.CONNECTOR['c'], T.CONNECTOR['r'], 3)
    rect(T.NAMEPLATE['rect'])
    for name, c in T.CONTROL_MM.items():
        x, y = p(*c)
        d.line([x - 12, y, x + 12, y], fill=(0, 140, 255), width=2)
        d.line([x, y - 12, x, y + 12], fill=(0, 140, 255), width=2)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--photo', help='the flattened photo (tools/photo_trace.py rectify writes out/traces/r123m_front/front_mm.png)')
    ap.add_argument('--out', default=os.path.join(REPO, 'out', 'r123m', 'front_sketch.png'))
    args = ap.parse_args()
    size = (int((T.W_MM + 2 * MARGIN) * K), int((T.H_MM + 2 * MARGIN) * K))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    sheet = Image.new('RGB', size, (250, 250, 247))
    draw_trace(sheet, (30, 30, 30))
    ImageDraw.Draw(sheet).text((MARGIN * K, 10), 'R-123M front, traced (%d x %d mm)' % (T.W_MM, T.H_MM), font=font(28),
                               fill=(90, 90, 90))
    sheet.save(args.out)
    print('wrote', args.out)
    if args.photo:
        photo = Image.open(args.photo).convert('RGB')
        if photo.size != size:
            sys.exit('the photo is %s, the sketch frame %s: use tools/photo_trace.py rectify output' % (photo.size, size))
        draw_trace(photo, (255, 40, 40))
        out = os.path.join(os.path.dirname(os.path.abspath(args.out)), 'front_overlay.png')
        photo.save(out)
        print('wrote', out)


if __name__ == '__main__':
    main()
