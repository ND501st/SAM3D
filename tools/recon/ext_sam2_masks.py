"""Stage S4 with SAM 2: a vehicle mask per photo, run in the ML venv by tools/recon/osa_recon.py.

    <ml-venv>/python tools/recon/ext_sam2_masks.py todo.txt out_dir --prompt vehicle [--checkpoint sam2.1_hiera_large.pt]

Prompting: Grounding DINO's box for the text prompt when `groundingdino` is installed (the vehicle among people,
trees and other vehicles), else the photo's frame minus 3 % (book photos are framed on the vehicle). SAM 2 returns three
masks for a box; the largest connected one that touches the box's middle is kept. Camouflage nets and branches are
part of the vehicle's silhouette in a photo: the catalogue's `issues` marks those photos so the fit can down-weight
them. Needs a CUDA GPU with 8 GB for the large model (RTX 3060 Ti: fine at 1024 px); falls back to the base model on
out-of-memory (the orchestrator retries).
"""
import argparse
import os
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('todo')
    ap.add_argument('out_dir')
    ap.add_argument('--prompt', default='vehicle')
    ap.add_argument('--checkpoint', default='facebook/sam2.1-hiera-large')
    ap.add_argument('--max-px', type=int, default=1024)
    a = ap.parse_args()
    import numpy as np
    import cv2
    import torch
    from sam2.sam2_image_predictor import SAM2ImagePredictor
    try:
        pred = SAM2ImagePredictor.from_pretrained(a.checkpoint)
    except torch.cuda.OutOfMemoryError:
        print('out of memory loading the large model', file=sys.stderr)
        raise
    dino = None
    try:
        from groundingdino.util.inference import load_model, predict  # noqa: F401
        dino = True
    except ImportError:
        pass
    os.makedirs(a.out_dir, exist_ok=True)
    for path in open(a.todo, encoding='utf-8').read().split('\n'):
        if not path.strip():
            continue
        img = cv2.imread(path)[..., ::-1]
        h, w = img.shape[:2]
        box = np.array([0.03 * w, 0.03 * h, 0.97 * w, 0.97 * h])
        with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
            pred.set_image(img)
            masks, scores, _ = pred.predict(box=box[None, :], multimask_output=True)
        best = None
        cx, cy = int(w / 2), int(h / 2)
        for m, s in sorted(zip(masks, scores), key=lambda t: -t[1]):
            m = (m > 0).astype(np.uint8)
            n, lab = cv2.connectedComponents(m)
            if n > 1 and lab[cy, cx] > 0:
                best = (lab == lab[cy, cx]).astype(np.uint8) * 255
                break
        if best is None:
            best = (masks[int(np.argmax(scores))] > 0).astype(np.uint8) * 255
        cv2.imwrite(os.path.join(a.out_dir, os.path.basename(path)), best)
        print('mask', os.path.basename(path), flush=True)


if __name__ == '__main__':
    main()
