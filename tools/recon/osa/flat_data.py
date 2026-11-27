"""The cabin data's items drawn on a panel's view flat (a flat laid through a certified camera in the data's own
millimetres), each with its number, and the list of what each number is: the check of a whole panel at once.

    flat_data.py <flat id> [--zoom 0.5]      # out/recon/osa/panels/flats/<id>_data.jpg and <id>_data.txt

Run with E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8.
"""
import argparse
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import panels as P  # noqa: E402
import panel_flats as F  # noqa: E402


def draw(fid, zoom=0.5):
    meta = F.meta(fid)
    img = F.image(fid)
    ppm, (ou, ov) = meta['px_per_mm'], meta['origin_mm']
    pid = meta.get('panel')
    if not pid:
        sys.exit('%s is not a view flat: it has no panel of the data' % fid)
    its = [it for it in P.items() if it['panel'] == pid]

    def to_px(uv_m):
        uv = np.asarray(uv_m, float) * 1000.0
        return np.stack([(uv[..., 0] + ou) * ppm, (ov - uv[..., 1]) * ppm], axis=-1)

    out = img.copy()
    for it in its:
        cv2.polylines(out, [np.round(to_px(P.outline(it))).astype(np.int32)], False, P.COLOURS[it['what']], 2, cv2.LINE_AA)
    lines = []
    for n, it in enumerate(its):
        x, y = to_px(it['at'])
        cv2.putText(out, str(n), (int(x) + 6, int(y) - 6), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 2, cv2.LINE_AA)
        lines.append('%3d %-8s %-28s at %s mm  %s' % (n, it['what'], it.get('id') or '', [round(v * 1000) for v in it['at']], it.get('label') or ''))
    if zoom != 1.0:
        out = cv2.resize(out, None, fx=zoom, fy=zoom, interpolation=cv2.INTER_AREA)
    base = os.path.join(F.OUT, fid + '_data')
    cv2.imwrite(base + '.jpg', out, [cv2.IMWRITE_JPEG_QUALITY, 88])
    with open(base + '.txt', 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(base + '.jpg', len(its), 'items')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('flat')
    ap.add_argument('--zoom', type=float, default=0.5)
    a = ap.parse_args()
    draw(a.flat, a.zoom)
