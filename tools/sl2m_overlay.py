"""SŁ-2M's Blender check renders (tools/blender/sl2m_check.py) against SAM Simulator's AK photo, pixel for pixel.

For each column: the photo's window that the render covers, the render, and the two blended, side by side; plus the
render's edges drawn on the photo. The photo lives in reference/ (git-ignored, never committed); so do the outputs.

    python tools/sl2m_overlay.py RENDER_DIR PHOTO OUT_DIR
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

PANELS = ('op_panel', 'so_panel', 'fo_panel')


def window(photo, win):
    """The photo's pixels under the render (grey where the render runs off the photo)."""
    x0, y0 = int(round(win['x0'])), int(round(win['y0']))
    w, h = int(round(win['w'])), int(round(win['h']))
    out = Image.new('RGB', (w, h), (90, 90, 90))
    box = (max(0, x0), max(0, y0), min(photo.width, x0 + w), min(photo.height, y0 + h))
    if box[2] > box[0] and box[3] > box[1]:
        out.paste(photo.crop(box), (box[0] - x0, box[1] - y0))
    return out


def edges(img):
    g = np.asarray(img.convert('L').filter(ImageFilter.FIND_EDGES), dtype=float)
    return g > 40


def compose(render_dir, photo_path, out_dir):
    photo = Image.open(photo_path).convert('RGB')
    os.makedirs(out_dir, exist_ok=True)
    made = []
    for pid in PANELS:
        png = os.path.join(render_dir, pid + '.png')
        if not os.path.exists(png):
            continue
        win = json.load(open(os.path.join(render_dir, pid + '.json')))
        rend = Image.open(png).convert('RGB')
        ph = window(photo, win).resize(rend.size)
        blend = Image.blend(ph, rend, 0.5)
        side = Image.new('RGB', (rend.width * 3 + 20, rend.height), (0, 0, 0))
        for i, im in enumerate((ph, rend, blend)):
            side.paste(im, (i * (rend.width + 10), 0))
        p1 = os.path.join(out_dir, pid + '_side.jpg')
        side.save(p1, quality=90)
        ov = np.asarray(ph).copy()
        ov[edges(rend)] = (255, 40, 200)
        p2 = os.path.join(out_dir, pid + '_edges.jpg')
        Image.fromarray(ov).save(p2, quality=90)
        made += [p1, p2]
    return made


if __name__ == '__main__':
    print('\n'.join(compose(*sys.argv[1:4])))
