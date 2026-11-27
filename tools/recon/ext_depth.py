"""Stage S5 (optional): Depth Anything V2 relative depth and normals per photo, run in the ML venv.

    <ml-venv>/python tools/recon/ext_depth.py photos_dir out_dir [--encoder vitl] [--max-px 1024]

Writes <photo>_depth.png (16-bit, relative inverse depth) and <photo>_normal.png (camera-frame normals from the depth's
gradients, 8-bit RGB). They are priors, not measurements: the fit uses the normals to tell a hull plate's facets apart
(each plate one normal, so a plate's polygon must not straddle a normal change) and to keep thin parts (the antenna
frames, the missile containers' ribs) from being merged into the hull's silhouette. Their scale is unknown and is
never used as a distance.
"""
import argparse
import os


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('photos')
    ap.add_argument('out')
    ap.add_argument('--encoder', default='vitl')
    ap.add_argument('--max-px', type=int, default=1024)
    a = ap.parse_args()
    import numpy as np
    import cv2
    import torch
    from transformers import pipeline
    from PIL import Image
    model = {'vits': 'Small', 'vitb': 'Base', 'vitl': 'Large'}[a.encoder]
    pipe = pipeline('depth-estimation', model='depth-anything/Depth-Anything-V2-%s-hf' % model,
                    device=0 if torch.cuda.is_available() else -1)
    os.makedirs(a.out, exist_ok=True)
    for name in sorted(os.listdir(a.photos)):
        im = Image.open(os.path.join(a.photos, name)).convert('RGB')
        im.thumbnail((a.max_px, a.max_px))
        d = np.asarray(pipe(im)['predicted_depth'].squeeze().cpu().numpy(), np.float32)
        d = cv2.resize(d, im.size, interpolation=cv2.INTER_CUBIC)
        dn = (d - d.min()) / max(1e-6, d.max() - d.min())
        cv2.imwrite(os.path.join(a.out, name[:-4] + '_depth.png'), (dn * 65535).astype(np.uint16))
        gy, gx = np.gradient(cv2.GaussianBlur(dn, (0, 0), 1.5))
        nrm = np.dstack([-gx, -gy, np.full_like(dn, 1.0 / max(dn.shape))])
        nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
        cv2.imwrite(os.path.join(a.out, name[:-4] + '_normal.png'), ((nrm[..., ::-1] * 0.5 + 0.5) * 255).astype(np.uint8))
        print('depth', name, flush=True)


if __name__ == '__main__':
    main()
