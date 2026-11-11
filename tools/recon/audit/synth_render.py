"""Synthetic round trip, step 1 (handoff section 8.1): the current model rendered through each view's registered camera
as a pinhole, to be treated as a photograph whose geometry is known exactly.

    python tools/recon/audit/synth_render.py [photo ...]

Out: out/recon/audit/synth/photos/<photo>.png (1200 px wide), the camera beside it (<photo>.json).
"""
import json
import os
import subprocess
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402
from common import DF, V  # noqa: E402

OUT = os.path.join(A.OUT, 'synth', 'photos')
WIDTH = 1200


def main(argv):
    os.makedirs(OUT, exist_ok=True)
    for k in argv or A.registered_views():
        final = os.path.join(OUT, k + '.png')
        if os.path.exists(final):
            continue
        cam = A.reg_camera(k)
        cam = cam.scaled(WIDTH, int(round(cam.h * WIDTH / cam.w)))
        cam.k1 = 0.0
        state = V.load(V.view_path(k)).get('state', {}) if os.path.exists(V.view_path(k)) else {}
        job = {'camera': cam.blender(), 'out': os.path.join(OUT, k + '_'), 'passes': ['model'], 'fill': True, 'repo': V.REPO, 'rays': [], 'hide': state.get('hide', [])}
        jp = os.path.join(OUT, k + '_job.json')
        V.save(jp, job)
        r = subprocess.run([V.BLENDER, '--background', '--factory-startup', '--python', V.SCENE, '--', jp], capture_output=True, text=True, encoding='utf-8', errors='replace')
        if r.returncode != 0 or not os.path.exists(job['out'] + 'model.png'):
            print(k, 'did not render:', r.stderr[-600:], flush=True)
            continue
        lit = np.maximum(cv2.imread(job['out'] + 'model.png'), (cv2.imread(job['out'] + 'model_flat.png') * DF.RENDER_FILL).astype(np.uint8))
        cv2.imwrite(final, cv2.resize(lit, (cam.w, cam.h)))
        with open(os.path.join(OUT, k + '.json'), 'w', encoding='utf-8') as f:
            json.dump({'camera': cam.to_dict(), 'hide': state.get('hide', [])}, f, indent=1)
        print(k, 'rendered', flush=True)


if __name__ == '__main__':
    main(sys.argv[1:])
