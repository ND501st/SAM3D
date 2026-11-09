"""MoGe-2 on a list of photographs (docs/OSA_INTERIOR_HOLISTIC.md section 31). Runs in its own environment, E:/SAM3D-tools/venv-depth; holistic_depth.py calls it.

    python holistic_mono_infer.py job.json

job.json: {"size": longest side in px, "photos": [{"id", "image", "fov_x_deg" or null, "out": .npz}]}. A photo whose .npz exists is skipped.
Each .npz holds, at the job's size: depth (m along the optical axis, NaN where the model marks the pixel invalid), normal (camera frame), fov_x_deg.
"""
import json
import os
import sys
import time

import cv2
import numpy as np
import torch
from huggingface_hub import hf_hub_download
from moge.model.v2 import MoGeModel

WEIGHTS = ('Ruicheng/moge-2-vitl-normal', 'model.pt')   # MIT, code and weights. MoGe-3's refinement needs Triton kernels that do not compile on Windows (section 31)


def main(path):
    with open(path, encoding='utf-8') as f:
        job = json.load(f)
    todo = [p for p in job['photos'] if not os.path.exists(p['out'])]
    print(len(job['photos']), 'photos,', len(todo), 'to do', flush=True)
    if not todo:
        return 0
    device = torch.device('cuda')
    model = MoGeModel.from_pretrained(hf_hub_download(*WEIGHTS)).to(device).eval()
    for n, p in enumerate(todo):
        t0 = time.time()
        im = cv2.imread(p['image'])
        h, w = im.shape[:2]
        s = min(1.0, job['size'] / max(w, h))
        im = cv2.cvtColor(cv2.resize(im, (round(w * s), round(h * s)), interpolation=cv2.INTER_AREA), cv2.COLOR_BGR2RGB)
        ten = torch.from_numpy(im.astype(np.float32) / 255.0).permute(2, 0, 1).to(device)
        with torch.inference_mode():
            o = model.infer(ten, fov_x=p.get('fov_x_deg'), use_fp16=True)
        depth = o['depth'].float().cpu().numpy()
        K = o['intrinsics'].float().cpu().numpy()
        fov = float(np.degrees(2 * np.arctan(0.5 / K[0, 0])))        # MoGe's intrinsics are normalised by the image's width
        normal = o['normal'].float().cpu().numpy() if 'normal' in o else np.zeros(depth.shape + (3,), np.float32)
        np.savez_compressed(p['out'], depth=depth.astype(np.float32), normal=(normal * 1000).astype(np.int16), fov_x_deg=fov)
        print('%3d/%d %s %dx%d fov %.1f %.1fs' % (n + 1, len(todo), p['id'], im.shape[1], im.shape[0], fov, time.time() - t0), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
