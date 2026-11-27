"""The right post between SP-2M2 and SP-3M, the housing beside it and the latch's plates, rebuilt from the photographs
(docs/OSA_MODELLING_PROCESS.md; part C of docs/OSA_MODELLING_HANDOFF.md). Evidence: tools/traces/osa_interior/evidence/
g098-a, g096-a, g092-a (picked edge chains and masks); cameras: tools/traces/osa_interior/cameras.json, grade A.

Reading (stage 2), from g098-a, g096-a and g092-a at full size:
- The post is one pressed member. Its forward face lies in one plane across the vehicle; its aisle edge is one unbroken
  line from under the lamp down past the latch to the floor, raking towards the aisle as it goes down.
- Behind its lower part stands a sheet-metal housing: a flat forward face with a bright rolled rim, a flat top, a flat
  side towards the aisle, and a large rounded corner between top and side. The first-aid box leans on its side and the
  chest stands behind it. This is what section 41 called the cover.
- On the housing's face: the latch's keeper bracket, a slotted plate under it and a strap with two bolts at the foot.
  The latch's bail hooks from the post onto the keeper (not rebuilt here).

    python tools/recon/osa/recipes/right_post.py     # writes the parts into data/cabins/osa_ak_parts.json
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

VIEWS = ('g098-a', 'g096-a', 'g092-a')
FORWARD = np.array([0.0, 1.0, 0.0])
HOUSING_DEPTH = 0.16          # (c) what shows of its top and side before the cloth, the first-aid box and the chest hide the rest (g096-a's band: 0.15; g098-a's top: about 0.18)
SP2M_FACE_X = C.V.load_cabin()['panels']['sp2m_panel']['centre_m'][0]     # the frame's face (control.json): what is hidden is closed against it
HOUSING_BEHIND_POST_X = SP2M_FACE_X - 0.012  # (c) the housing's wall side is hidden by the post: closed just short of SP-2M2's face plane
POST_DEPTH = 0.045            # (c) the post's thickness fore and aft, the earlier shell's channel depth: no photo shows its back
POST_BACK_X = SP2M_FACE_X + 0.168   # (c) how far the post's web reaches behind the rack's end: hidden
FLOOR_Z, LINING_Z = 0.83, 2.12   # the aisle's floor and the lining there (data/cabins/osa_ak_shell.json): the post is closed at them
PLATE_T = 0.005               # (c) the plates' thickness


def obs(part, feature, what='pieces'):
    out = []
    for v in VIEWS:
        it = E.items(v, part, feature)
        if it:
            out.append((v, C.camera(v, 'A'), it[0][what]))
    return out


_WALL = {}


def wall():
    """The plane both housings' forward faces lie in: the right housing's rim in three photos and the cabinet-side housing's
    face in two, each laid on its own other sightings, one plane for all. Free to turn, it stands about 12 degrees towards
    the aisle (the 0096-9M2 beside it stands 16), and the cabinet-side face then closes to 2-3 px where square it left 4."""
    if not _WALL:
        o, groups, names = [], [], []
        for part, views in (('housing.right', VIEWS), ('housing.cabinet', ('g096-a', 'g098-a'))):
            for v in views:
                it = E.items(v, part, 'rim')
                if it:
                    o.append((C.camera(v, 'A'), it[0]['pieces']))
                    groups.append(part)
                    names.append('%s %s' % (part.split('.')[1], v))
        n, d, left = F.plane_of_curves(o, FORWARD, 0.35, free_turn=True, groups=groups)
        _WALL.update(n=n, d=d, agree={'%s->%s' % (names[int(k.split('->')[0])], names[int(k.split('->')[1])]): v['median_px'] for k, v in left.items()})
    return _WALL['n'], _WALL['d'], _WALL['agree']


def housing():
    o = obs('housing.right', 'rim')
    n, d, agree = wall()
    fr = P.Frame.on_plane(n, d, [SP2M_FACE_X - 0.012, 0.3, 1.2])
    prof, _, gap = F.star_profile([(c, p) for _, c, p in o], fr)
    # the aisle side runs on down past the strap's foot to the floor box (g098-a sees it): carried down along that photo's line
    low = fr.coords(F.cast(C.camera('g098-a', 'A'), E.items('g098-a', 'housing.right', 'rim')[0]['line'], n, d))[0]
    low = low[np.isfinite(low).all(1)]
    upper = prof[prof[:, 1] >= prof[:, 1].min() + 0.04]
    upper = upper[int(np.argmax(upper[:, 0])):] if np.argmax(upper[:, 0]) < len(upper) // 2 else upper
    low = low[(low[:, 1] < upper[0, 1]) & (np.abs(low[:, 0] - upper[0, 0]) < 0.08)]
    low = low[np.argsort(low[:, 1])]
    seen = np.vstack([low, upper]) if len(low) else upper
    X = fr.at(seen)
    a_wall = float(fr.coords([[HOUSING_BEHIND_POST_X, d, 1.2]])[0][0, 0])
    closed = np.vstack([seen, [a_wall, seen[-1, 1]], [a_wall, seen[0, 1]]])
    part = P.Extrusion(fr, closed, HOUSING_DEPTH)
    return part, {'plane': {'normal': [round(float(v), 4) for v in n], 'd_m': round(float(d), 4), 'yaw_deg': round(float(np.degrees(np.arctan2(-n[0], n[1]))), 1), 'tilt_deg': round(float(np.degrees(np.arcsin(n[2]))), 1)},
                  'views_agree_px': {k: v for k, v in agree.items() if k.startswith('right')}, 'seen_points': len(seen),
                  'top_z_m': round(float(X[:, 2].max()), 3), 'aisle_side_x_m': round(float(X[:, 0].min()), 3), 'seen_down_to_z_m': round(float(X[:, 2].min()), 3)}


def housing_cabinet(face):
    """The lower housing between the right housing and the 0096-9M2: one flat face in the same wall, its top edge sloping down
    towards the cabinet. Its outline is g096-a's (the only photo that shows all of it), g098-a has its top and wall-side edges,
    and its sloped top is carried back to where g098-a shows it end."""
    n, d, agree = wall()
    fr = P.Frame(face.origin, face.right, face.up)
    X = F.cast(C.camera('g096-a', 'A'), E.items('g096-a', 'housing.cabinet', 'rim')[0]['line'], n, d)
    prof = fr.coords(X[np.isfinite(X).all(1)])[0][::2]
    back = E.items('g098-a', 'housing.cabinet', 'back')
    depth, left = F.depth_of([(C.camera('g098-a', 'A'), back[0]['pieces'])], fr, prof, d0=0.2) if back else (0.2, None)
    Xp = fr.at(prof)
    return P.Extrusion(fr, prof, depth), {'views_agree_px': {k: v for k, v in agree.items() if k.startswith('cabinet')}, 'depth_m': round(depth, 3), 'depth_left_px': left,
                                          'x_m': [round(float(Xp[:, 0].min()), 3), round(float(Xp[:, 0].max()), 3)], 'z_m': [round(float(Xp[:, 2].min()), 3), round(float(Xp[:, 2].max()), 3)]}


def post():
    o = obs('post.right', 'edge_aisle')
    n, d, left = F.plane_of_curves([(c, p) for _, c, p in o], FORWARD, 0.45, free_turn=False)
    X = np.vstack([F.cast(c, p, n, d) for _, c, p in o])
    X = X[np.isfinite(X).all(1)]
    zs = np.arange(np.floor(X[:, 2].min() / 0.02) * 0.02, X[:, 2].max(), 0.02)
    edge = np.array([[np.median(X[(X[:, 2] >= z) & (X[:, 2] < z + 0.02), 0]), z + 0.01] for z in zs if ((X[:, 2] >= z) & (X[:, 2] < z + 0.02)).sum() >= 3])
    k = 5
    edge[:, 0] = np.convolve(np.pad(edge[:, 0], k // 2, mode='edge'), np.ones(k) / k, mode='valid')
    lo_slope = np.polyfit(edge[:6, 1], edge[:6, 0], 1)[0]
    low = np.array([[edge[0, 0] + lo_slope * (FLOOR_Z - edge[0, 1]), FLOOR_Z]])
    top = np.array([[edge[-1, 0], LINING_Z]])
    aisle = np.vstack([low, edge, top] if edge[-1, 1] < LINING_Z else [low, edge])
    # the other crease of the forward face, where g096-a shows it: how wide the face is above the knee
    w = None
    it = E.items('g096-a', 'post.right', 'edge_wall')
    if it:
        W = F.cast(C.camera('g096-a', 'A'), it[0]['pieces'], n, d)
        W = W[np.isfinite(W).all(1)]
        xa = np.interp(W[:, 2], edge[:, 1], edge[:, 0])
        w = float(np.median(W[:, 0] - xa))
    w = w if w and 0.02 < w < 0.2 else 0.06
    knee = 1.5
    wall = [[x + w, z] for x, z in aisle[::-1] if z >= knee] + [[POST_BACK_X, knee], [POST_BACK_X, FLOOR_Z]]
    body = np.vstack([aisle, wall])
    fr = P.Frame.on_plane(n, d, [SP2M_FACE_X - 0.012, 0.45, 1.3])
    part = P.Extrusion(fr, fr.coords(np.column_stack([body[:, 0], np.full(len(body), d), body[:, 1]]))[0], POST_DEPTH)
    rake = float(np.degrees(np.arctan(np.polyfit(edge[(edge[:, 1] > 1.0) & (edge[:, 1] < 1.45), 1], edge[(edge[:, 1] > 1.0) & (edge[:, 1] < 1.45), 0], 1)[0])))
    edge3 = np.column_stack([edge[:, 0], np.full(len(edge), d), edge[:, 1]])
    return part, edge3, {'plane_y_m': round(d, 4), 'views_agree_px': {'%s->%s' % (o[int(k[0])][0], o[int(k[-1])][0]): v['median_px'] for k, v in left.items()}, 'edge_seen_z_m': [round(float(edge[0, 1]), 2), round(float(edge[-1, 1]), 2)],
                         'face_width_m': round(w, 3), 'leg_rake_deg_from_vertical': round(abs(rake), 1), 'edge_x_at_z': {('%.2f' % z): round(float(np.interp(z, edge[:, 1], edge[:, 0])), 3) for z in (1.0, 1.2, 1.4, 1.6, 1.8)}}


def plate(evidence_part, face):
    """A plate on the housing's face: its outline in g096-a (the squarest view of the face) cast onto a plane in front of the face.
    How far in front is fitted: the same plate, through g098-a's and g092-a's cameras, against a mask SAM 2 gives there when prompted at where the plate lands."""
    it = E.items('g096-a', evidence_part, 'rim')[0]
    cam0 = C.camera('g096-a', 'A')

    def at(proud):
        fr = P.Frame(face.origin + proud * face.normal, face.right, face.up)
        X = F.cast(cam0, it['line'], fr.normal, float(fr.normal @ fr.origin))
        return P.Extrusion(fr, fr.coords(X[np.isfinite(X).all(1)])[0][::3], PLATE_T, material='Paint_Interior')
    first = at(0.01)
    masks = {}
    for v in ('g098-a', 'g092-a'):
        cam = C.camera(v, 'A')
        c = C.project(cam, first.mesh()[0].mean(0))[0]
        m, score = E.prompt_mask(v, [c.tolist()])
        area_model = area_px(C.project(cam, first.features()['rim'][0]))
        if score >= 0.7 and 0.5 < m.sum() / max(area_model, 1.0) < 2.0:          # the mask is of something of the plate's size: taken as the plate
            masks[v] = (cam, E.resample(E.mask_outline(m)), [round(float(q), 1) for q in c], round(score, 2))
    best = (None, 1e9)
    for proud in np.arange(0.0, 0.0801, 0.004):
        rim = at(proud).features()['rim'][0]
        e = [np.median(F.outline_error(cam, np.vstack([rim, rim[:1]]), [line])) for cam, line, _, _ in masks.values()]
        if e and np.mean(e) < best[1]:
            best = (proud, float(np.mean(e)))
    proud = best[0] if best[0] is not None else 0.006
    return at(proud), {'proud_of_face_m': round(float(proud), 3), 'other_views_px': round(best[1], 1) if masks else None, 'other_views': {v: {'prompt_px': m[2], 'score': m[3]} for v, m in masks.items()}}, masks


def area_px(p):
    import cv2
    p = p[np.isfinite(p).all(1)]
    return float(cv2.contourArea(p.astype(np.float32))) if len(p) >= 3 else 0.0


def main():
    h, hm = housing()
    p, edge3, pm = post()
    hc, hcm = housing_cabinet(h.frame)
    src = 'wwp_gecko g098-a, g096-a, g092-a: %s (tools/recon/osa/recipes/right_post.py; docs/OSA_MODELLING_PROCESS.md)'
    out = [
        store.entry('housing.right', 'Housing beside the right post', h, hm, src % 'its rim picked as edge chains in the three photos, cast through their certified cameras onto one plane and made one outline', 'medium',
                    measured=['the face plane, with the cabinet-side housing\'s face (about 12 degrees towards the aisle)', 'the rim: top, rounded corner and aisle side, down to the floor box'],
                    chosen=['depth %.2f m: only what shows before the first-aid box and the cloth' % HOUSING_DEPTH, 'the side behind the post, closed at x %.2f' % HOUSING_BEHIND_POST_X, 'its foot is closed level where the floor box hides it'],
                    features={'rim': 'rim'}, supersedes=['posts.right.shroud']),
        store.entry('housing.cabinet', 'Housing between the right housing and the 0096-9M2', hc, hcm, 'wwp_gecko g096-a, g098-a: %s (tools/recon/osa/recipes/right_post.py; docs/OSA_MODELLING_PROCESS.md)' %
                    'its face\'s outline picked as edge chains in g096-a, its top and wall-side edges in g098-a, on the plane the two photos and the right housing\'s rim share; four tie points inside the face lie on that plane', 'medium',
                    measured=['the face\'s plane (with the right housing\'s)', 'the face\'s outline', 'how far the sloped top runs back, from g098-a'],
                    chosen=['its far side and what is under the top are not seen', 'the pale box in front of it on the floor is not rebuilt'],
                    features={'rim': 'rim', 'back': 'back'}, supersedes=['floor.raised.housing_beside_cabinet']),
        store.entry('post.right', 'Right post between SP-2M2 and SP-3M', p, pm, src % 'its aisle edge picked as edge chains in the three photos (which chains are the same edge was settled by which fit one plane), cast onto that plane', 'medium',
                    measured=['the forward face\'s plane', 'the aisle edge from z %.2f to %.2f' % tuple(pm['edge_seen_z_m']), 'the forward face\'s width above the knee, from g096-a\'s second crease'],
                    chosen=['thickness %.3f m fore and aft' % POST_DEPTH, 'the web behind the rack\'s end out to x %.2f' % POST_BACK_X, 'the edge carried straight to the floor and the lining beyond what is seen', 'one flat member: its flanges and creases are not modelled'],
                    features={'edge_aisle': 'rim'}, feature_lines={'edge_aisle': edge3.round(4).tolist()}, supersedes=['posts.right']),
    ]
    for name, label in (('latch.bracket', 'Latch keeper bracket on the housing'), ('latch.slotted_plate', 'Slotted plate under the keeper'), ('latch.strap', "Strap at the housing's foot")):
        q, qm, masks = plate(name, h.frame)
        for v, (cam, line, prompt, score) in masks.items():            # the prompted masks become this plate's evidence in those photos
            path = os.path.join(C.EVIDENCE, v + '.json')
            doc = C.load(path)
            doc['items'] = [i for i in doc['items'] if not (i['part'] == name and i['feature'] == 'rim')] + [{'part': name, 'feature': 'rim', 'kind': 'mask', 'prompt': {'pos': [prompt]}, 'prompted_by': "where the plate lands through this photo's camera", 'score': score, 'closed': True, 'pieces': [np.round(line, 1).tolist()]}]
            C.save(path, doc, indent=None)
        out.append(store.entry(name, label, q, qm, "wwp_gecko g096-a: its outline as a SAM 2 mask, cast onto a plane in front of the housing's face; how far in front fitted in g098-a and g092-a (tools/recon/osa/recipes/right_post.py)", 'low',
                               measured=['the outline, in g096-a', 'how far it stands in front of the face, where another photo gave a mask of it'], chosen=['thickness %.3f m' % PLATE_T, "bolts, slots and the keeper's loop are not modelled"], features={'rim': 'rim'}, parent='housing.right'))
    store.put(out)
    for e in out:
        print(e['id'], {k: v for k, v in e['fit'].items() if k != 'views_agree_px'}, e['fit'].get('views_agree_px', ''))


if __name__ == '__main__':
    main()
