"""Smoke test of the photo-measurement tools installed by setup_tools.ps1, on a real book photo and its flattened face.

    E:\\SAM3D-tools\\venv\\Scripts\\python.exe tools/recon/smoke_tools.py <clean photo> <flattened face> [--root E:\\SAM3D-tools]

Each tool runs once and prints one line: what it found, or why it failed. Nothing is written to the repo.
"""
import argparse
import os
import subprocess
import sys
import time
import traceback


def step(name, fn):
    t = time.time()
    try:
        out = fn()
        print('OK   %-10s %5.1fs  %s' % (name, time.time() - t, out), flush=True)
    except Exception as e:  # noqa: BLE001  (a smoke test reports every failure and carries on)
        print('FAIL %-10s %5.1fs  %s: %s' % (name, time.time() - t, type(e).__name__, str(e).splitlines()[0][:160]), flush=True)
        if os.environ.get('SMOKE_TRACE'):
            traceback.print_exc()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('photo')
    ap.add_argument('face')
    ap.add_argument('--root', default=r'E:\SAM3D-tools')
    a = ap.parse_args()
    import cv2
    import numpy as np
    img = cv2.imread(a.photo)
    face = cv2.imread(a.face)
    small = cv2.resize(face, None, fx=0.25, fy=0.25, interpolation=cv2.INTER_AREA)  # about 1 px per mm

    def torch_():
        import torch
        return '%s, CUDA %s, %s' % (torch.__version__, torch.cuda.is_available(), torch.cuda.get_device_name(0))
    step('torch', torch_)

    def sam2_():
        import torch
        from sam2.build_sam import build_sam2
        from sam2.sam2_image_predictor import SAM2ImagePredictor
        ck = os.path.join(a.root, 'models', 'sam2.1_hiera_base_plus.pt')
        m = build_sam2('configs/sam2.1/sam2.1_hiera_b+.yaml', ck, device='cuda')
        p = SAM2ImagePredictor(m)
        with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
            p.set_image(cv2.cvtColor(small, cv2.COLOR_BGR2RGB))
            h, w = small.shape[:2]
            # a point on the middle column's row-2 left meter (about 0.39 x, 0.27 y of the face)
            masks, scores, _ = p.predict(point_coords=np.array([[0.40 * w, 0.27 * h]]), point_labels=np.array([1]), multimask_output=True)
        i = int(np.argmax(scores))
        ys, xs = np.nonzero(masks[i])
        return 'mask %d px, box %dx%d px (about mm), score %.2f' % (masks[i].sum(), np.ptp(xs) + 1, np.ptp(ys) + 1, scores[i])
    step('sam2', sam2_)

    def paddle_():
        os.environ.setdefault('PADDLE_PDX_DISABLE_MODEL_SOURCE_CHECK', 'True')
        from paddleocr import PaddleOCR
        # enable_mkldnn=False: Paddle 3's oneDNN path fails on Windows CPUs with PP-OCRv5 (ConvertPirAttribute2RuntimeAttribute)
        ocr = PaddleOCR(lang='ru', use_doc_orientation_classify=False, use_doc_unwarping=False, use_textline_orientation=False,
                        enable_mkldnn=False)
        crop = face[int(0.42 * face.shape[0]):int(0.56 * face.shape[0]), int(0.36 * face.shape[1]):int(0.55 * face.shape[1])]
        res = ocr.predict(crop)
        texts = [t for r in res for t in r['rec_texts']]
        return '%d lines: %s' % (len(texts), ' | '.join(texts[:8]))
    step('paddleocr', paddle_)

    def tesseract_():
        exe = r'C:\Program Files\Tesseract-OCR\tesseract.exe'
        crop = face[int(0.42 * face.shape[0]):int(0.56 * face.shape[0]), int(0.36 * face.shape[1]):int(0.55 * face.shape[1])]
        p = os.path.join(a.root, 'downloads', 'smoke_plate.png')
        cv2.imwrite(p, crop)
        env = dict(os.environ, TESSDATA_PREFIX=os.path.join(a.root, 'tessdata'))
        r = subprocess.run([exe, p, '-', '-l', 'rus+eng', '--psm', '6'], capture_output=True, text=True, encoding='utf-8', env=env, timeout=120)
        lines = [x for x in r.stdout.splitlines() if x.strip()]
        return '%d lines: %s' % (len(lines), ' | '.join(lines[:6]))
    step('tesseract', tesseract_)

    def lightglue_():
        import torch
        from lightglue import ALIKED, LightGlue
        from lightglue.utils import rbd
        ext = ALIKED(max_num_keypoints=2048).eval().cuda()
        lg = LightGlue(features='aliked').eval().cuda()
        g = cv2.GaussianBlur(cv2.cvtColor(img, cv2.COLOR_BGR2GRAY), (0, 0), 1.2)
        H = np.array([[0.97, 0.03, 20], [-0.02, 1.0, 10], [2e-5, 0, 1]])
        g2 = cv2.warpPerspective(g, H, g.shape[::-1])
        t = lambda x: torch.from_numpy(x).float()[None, None].cuda() / 255
        with torch.inference_mode():
            f0, f1 = ext.extract(t(g)), ext.extract(t(g2))
            m = rbd(lg({'image0': f0, 'image1': f1}))['matches']
        k0, k1 = rbd(f0)['keypoints'][m[:, 0]].cpu().numpy(), rbd(f1)['keypoints'][m[:, 1]].cpu().numpy()
        Hf, inl = cv2.findHomography(k0, k1, cv2.RANSAC, 2.0)
        err = np.abs(cv2.perspectiveTransform(k0[None], H)[0] - k1).mean()
        return '%d matches, %d inliers, mean error against the true warp %.2f px' % (len(m), inl.sum(), err)
    step('lightglue', lightglue_)

    def pycolmap_():
        import pycolmap
        return 'pycolmap %s, CUDA %s' % (pycolmap.__version__, pycolmap.has_cuda)
    step('pycolmap', pycolmap_)

    def colmap_():
        r = subprocess.run([os.path.join(a.root, 'colmap', 'bin', 'colmap.exe'), 'help'], capture_output=True, text=True, timeout=60)
        return (r.stdout or r.stderr).splitlines()[0]
    step('colmap', colmap_)

    def hloc_():
        import hloc
        from hloc import extract_features, match_features
        return 'hloc %s, features %s; matchers %s' % (getattr(hloc, '__version__', '?'), sorted(k for k in extract_features.confs if k in ('aliked-n16', 'disk')),
                                                     sorted(k for k in match_features.confs if 'lightglue' in k)[:4])
    step('hloc', hloc_)

    def geocalib_():
        import torch
        from geocalib import GeoCalib
        m = GeoCalib().cuda()
        x = torch.from_numpy(cv2.cvtColor(img, cv2.COLOR_BGR2RGB)).permute(2, 0, 1).float().cuda() / 255
        r = m.calibrate(x)
        f = float(r['camera'].f[0, 0])
        return 'f %.0f px = %.2f frame widths (a prior: principal point fixed at the centre)' % (f, f / img.shape[1])
    step('geocalib', geocalib_)

    def anylabeling_():
        exe = os.path.join(a.root, 'venv-label', 'Scripts', 'xanylabeling.exe')
        r = subprocess.run([exe, '--help'], capture_output=True, text=True, timeout=120)
        return 'runs: ' + ((r.stdout or '') + (r.stderr or '')).strip().splitlines()[0][:100]
    step('anylabel', anylabeling_)

    def depth_():
        code = ('import moge, numpy as np, trimesh; from trimesh.ray.ray_pyembree import RayMeshIntersector as R; '
                'm = trimesh.creation.box(); r = R(m); '
                'print(int((r.intersects_first(np.zeros((1000, 3)) - [[0, 0, 3]], np.tile([[0, 0, 1.0]], (1000, 1))) >= 0).sum()), "of 1000 rays hit")')
        r = subprocess.run([os.path.join(a.root, 'venv-depth', 'Scripts', 'python.exe'), '-c', code], capture_output=True, text=True, timeout=300)
        if r.returncode:
            raise RuntimeError(r.stderr.strip().splitlines()[-1])
        return 'moge and embree: ' + r.stdout.strip()
    step('depth', depth_)

    step('brush', lambda: '%s (%d MB)' % (os.path.join(a.root, 'brush', 'brush_app.exe'),
                                          os.path.getsize(os.path.join(a.root, 'brush', 'brush_app.exe')) // 2**20))


if __name__ == '__main__':
    sys.exit(main())
