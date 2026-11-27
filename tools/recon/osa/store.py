"""The parts file, data/cabins/osa_ak_parts.json: every part rebuilt by docs/OSA_MODELLING_PROCESS.md, with its shape,
where it came from, what was measured and what was chosen, and what it was accepted at."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import core as C  # noqa: E402
import parts as P  # noqa: E402

ABOUT = ("Parts of the Osa-AK's interior rebuilt from the book's photographs by the process of docs/OSA_MODELLING_PROCESS.md: each a part object "
         "(tools/recon/osa/parts.py) with its own frame, fitted to outlines measured in two or more photographs through certified cameras "
         "(tools/traces/osa_interior/cameras.json). `measured` is what the photographs fix; `chosen` is SAM3D's choice where they do not. "
         "`accepted` holds each view's outline error in pixels when the part was accepted: tools/tests/test_osa_parts.py holds them to it.")
FRAME = ("The body frame, x from the vehicle's centre line. Across the vehicle it is tied to the photographs by SP-2M2's traced face (control.json), "
         "0.914 m right of the turret's axis (docs/research/OSA_REBUILD_C_2026-11.md). Its scale is SP-2M2's 1.89 m width, which is assumed: "
         "by SL-2M's sourced 1.89 m the frame is about 4.5 % large, by the aft door's 0.59 m about 9 %. One factor will rescale every part here when it is settled.")


def entry(pid, label, shape, fit, source, confidence, measured, chosen, features, supersedes=(), parent=None, feature_lines=None):
    e = {'id': pid, 'label': label, 'shape': shape.to_dict(), 'fit': fit, 'source': source, 'confidence': confidence, 'measured': measured, 'chosen': chosen, 'evidence_features': features}
    if supersedes:
        e['supersedes'] = list(supersedes)
    if parent:
        e['parent'] = parent
    if feature_lines:
        e['feature_lines_m'] = feature_lines
    return e


def load():
    return C.load(C.PARTS) if os.path.exists(C.PARTS) else {'about': ABOUT, 'frame': FRAME, 'parts': []}


def put(entries):
    doc = load()
    doc['about'], doc['frame'] = ABOUT, FRAME
    old = {p['id']: p for p in doc['parts']}
    for e in entries:
        if e['id'] in old and 'accepted' in old[e['id']]:
            e.setdefault('accepted', old[e['id']]['accepted'])
        old[e['id']] = e
    doc['parts'] = list(old.values())
    C.save(C.PARTS, doc, indent=None)
    return doc


def parts():
    return {p['id']: p for p in load()['parts']}


def shape(p):
    return P.from_dict(p['shape'])
