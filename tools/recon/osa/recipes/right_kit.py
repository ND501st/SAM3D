"""The first-aid box by the right post, fitted to its outline in two photographs at once (docs/OSA_MODELLING_PROCESS.md).
Evidence: SAM 2 masks in g098-a and g096-a; cameras grade A. It is loose kit in a strap holder (the hangar vehicle of
g092-a and g096-a and the session A vehicle of g098 carry it in the same place).

The part is a box with rounded edges. Its mesh stays SM_FirstAid, with its cross, seam and strap
(tools/blender/build_cabin_interior.py build_first_aid), now set on this box's front face.

    python tools/recon/osa/recipes/right_kit.py
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import core as C  # noqa: E402
import evidence as E  # noqa: E402
import fit as F  # noqa: E402
import parts as P  # noqa: E402
import store  # noqa: E402

VIEWS = ('g098-a', 'g096-a')


def main():
    obs = [(C.camera(v, 'A'), E.items(v, 'first_aid', 'outline')[0]['pieces']) for v in VIEWS]
    # a start: on the two cameras' rays through the outline's middle, where they pass nearest each other
    rays = []
    for cam, pieces in obs:
        mid = np.vstack(pieces).mean(0)
        rays.append((cam.centre(), C.rays(cam, [mid])[0]))
    (o1, d1), (o2, d2) = rays
    A = np.array([[d1 @ d1, -d1 @ d2], [d1 @ d2, -d2 @ d2]])
    t = np.linalg.solve(A, [(o2 - o1) @ d1, (o2 - o1) @ d2])
    mid = 0.5 * (o1 + t[0] * d1 + o2 + t[1] * d2)
    start = P.RoundedBox(mid, [0.0, 0.0, 0.0], [0.1, 0.25, 0.3], 0.02, 'Paint_Letters')
    box, left = F.rounded_box(obs, start, starts=10)
    fitm = {'views_px': {v: l for v, l in zip(VIEWS, left)}, 'size_m': sorted([round(float(x), 3) for x in box.size], reverse=True)}
    e = store.entry('first_aid', 'First-aid box by the right post', box, fitm,
                    'wwp_gecko g098-a, g096-a: its outline as SAM 2 masks in the two photos, a box with rounded edges fitted to both at once through certified cameras (tools/recon/osa/recipes/right_kit.py)', 'medium',
                    measured=['its place and lean', 'its two larger sizes'], chosen=['its thickness is weakly held: both photos see it nearly face on', 'the cross, the seam, the latch and the strap are SM_FirstAid\'s own, set on this box\'s front'],
                    features={'outline': 'hull'})
    e['mesh'] = 'SM_FirstAid'
    store.put([e])
    print('first_aid', fitm, 'centre', box.centre.round(3).tolist())


if __name__ == '__main__':
    main()
