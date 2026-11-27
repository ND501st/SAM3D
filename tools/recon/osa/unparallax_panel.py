"""Move a panel's raised items from where a photo's camera sees them to where they stand: an item placed on a view flat
at the centre of its part was placed where the camera's ray through the part meets the panel's plane, pushed away from
the camera's foot by h / d of its distance from it (h the part's height out of the plate, d the eye's distance from the
plane). Each item goes back along that ray by its kind's height (KIND_H_MM, the height of a part's seen centre).

    unparallax_panel.py <panel> <eye x,y,z> [--write]     # e.g. so_panel 0.824,2.635,1.924 (wwp_gecko g094-a's camera)

Flat items (nameplates, diagrams, windows, legends, bezels) do not move; nor do the blocks. Run once per panel: the file
does not record that it was done (the panel's notes should). Run with E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8.
"""
import argparse
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import jsonedit as J  # noqa: E402
import panels as P  # noqa: E402

# A part's seen centre's height out of the plate, mm: about half the part's own height (SAM 2's mask takes its side too).
KIND_H_MM = {'knob': 15, 'pointer_knob': 15, 'bar_knob': 15, 'trim': 10, 'lever': 8, 'toggle': 8, 'button': 10,
             'lamp_button': 10, 'lamp': 6, 'jack': 4, 'socket': 6, 'latch': 8, 'oval': 3, 'gauge': 4, 'legend': 5,
             'key': 10, 'cover': 12, 'wheel': 12}
FLAT = ('nameplate', 'diagram', 'window', 'bezel')


def true_place(panel, at_m, h_m, eye):
    n, r, up = P.axes(panel)
    c = np.asarray(panel['centre_m'], float)
    X = c + r * at_m[0] + up * at_m[1]
    E = np.asarray(eye, float)
    d = float((E - X) @ n)
    Xt = X + (E - X) * h_m / d
    return [round(float((Xt - c) @ r), 4), round(float((Xt - c) @ up), 4)]


def kind_of(row, what):
    if what == 'fixtures':
        return row['kind']
    look = row.get('look', {})
    if look.get('kind'):
        return look['kind']
    if what == 'lamps':
        return 'lamp'
    return row.get('style', 'wheel' if 'wheel' in row.get('control', '') else 'knob')


def run(pid, eye, write=False):
    text = J.read(P.CABIN)
    cab = json.loads(text)
    panel = next(p for p in cab['panels'] if p['id'] == pid)
    moved = []
    for what, key in (('controls', 'control'), ('lamps', 'id'), ('fixtures', None)):
        for k, row in enumerate(cab[what]):
            if row.get('panel') != pid:
                continue
            kind = kind_of(row, what)
            if kind in FLAT:
                continue
            h = KIND_H_MM.get(kind, 8) / 1000.0
            new = true_place(panel, row['at_m'], h, eye)
            sel = [what, {key: row[key]}] if key else [what, k]
            text = J.replace_value(text, sel + ['at_m'], json.dumps(new))
            moved.append((what[:-1], row.get(key, kind) if key else kind, row['at_m'], new))
    for m in moved:
        print('%-8s %-24s %s -> %s' % m)
    print(len(moved), 'moved on', pid)
    if write:
        J.write(P.CABIN, text)
        print('written', P.CABIN)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('panel')
    ap.add_argument('eye')
    ap.add_argument('--write', action='store_true')
    a = ap.parse_args()
    run(a.panel, [float(x) for x in a.eye.split(',')], a.write)
