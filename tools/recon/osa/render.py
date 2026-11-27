"""The built cabin rendered through a photo's certified camera, beside the photo: what the owner's eye judges
(docs/OSA_MODELLING_PROCESS.md stage 6). Blender in the background, tools/blender/holistic_scene.py.

    python tools/recon/osa/render.py g098-a g096-a g092-a [--width 1600] [--crop x0,y0,x1,y1 as shares]

Out: out/recon/osa/renders/<photo>.jpg (photo | model), <photo>_model.png.
"""
import os
import subprocess
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import core as C  # noqa: E402
from core import V  # noqa: E402

OUT = os.path.join(C.OUT, 'renders')
FILL = 0.62          # what faces down is lifted, so a roof's shape shows (as holistic_diff's renders)


def render(photo, width=1600):
    os.makedirs(OUT, exist_ok=True)
    cam = C.camera(photo)
    cam = cam.scaled(width, int(round(cam.h * width / cam.w)))
    state = V.load(V.view_path(photo)).get('state', {}) if os.path.exists(V.view_path(photo)) else {}
    job = {'camera': cam.blender(), 'out': os.path.join(OUT, photo + '_'), 'passes': ['model'], 'fill': True, 'repo': C.REPO, 'rays': [], 'hide': state.get('hide', [])}
    jp = os.path.join(OUT, photo + '_job.json')
    V.save(jp, job)
    r = subprocess.run([V.BLENDER, '--background', '--factory-startup', '--python', V.SCENE, '--', jp], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0 or not os.path.exists(job['out'] + 'model.png'):
        raise SystemExit('%s did not render: %s' % (photo, r.stderr[-600:]))
    lit = np.maximum(cv2.imread(job['out'] + 'model.png'), (cv2.imread(job['out'] + 'model_flat.png') * FILL).astype(np.uint8))
    lit = cv2.resize(lit, (cam.w, cam.h))
    cv2.imwrite(os.path.join(OUT, photo + '_model.png'), lit)
    K = np.array([[cam.f, 0, cam.cx], [0, cam.f, cam.cy], [0, 0, 1.0]])
    im = cv2.resize(C.photo(photo), (cam.w, cam.h), interpolation=cv2.INTER_AREA)
    im = cv2.undistort(im, K, np.array([cam.k1, 0, 0, 0.0]))          # the render is a pinhole's: the photo's lens term is taken out to match
    return im, lit


def main(argv):
    width = int(argv[argv.index('--width') + 1]) if '--width' in argv else 1600
    crop = [float(v) for v in argv[argv.index('--crop') + 1].split(',')] if '--crop' in argv else None
    for photo in [a for a in argv if a.startswith('g')]:
        im, lit = render(photo, width)
        if crop:
            h, w = im.shape[:2]
            x0, y0, x1, y1 = int(crop[0] * w), int(crop[1] * h), int(crop[2] * w), int(crop[3] * h)
            im, lit = im[y0:y1, x0:x1], lit[y0:y1, x0:x1]
        cv2.imwrite(os.path.join(OUT, photo + '.jpg'), np.hstack([im, lit]), [cv2.IMWRITE_JPEG_QUALITY, 90])
        print(photo, 'rendered', flush=True)


if __name__ == '__main__':
    main(sys.argv[1:])
