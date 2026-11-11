"""The 0096-9M2 put where the photographs have it, and at their size (11 Nov 2026, the project owner's decision;
docs/research/OSA_REBUILD_C_2026-11.md). Run once after `cameras.py solve`; kept for the record.

Its door is a control plate (tools/traces/osa_interior/control.json): its shape is the trace's, and its place, turn and
one scale are solved with the certified cameras. Section 33 had placed and sized it through the cameras of
holistic_refine.py, which the audit found 40-97 px out. What the plate solved to goes into:

    tools/blender/cabinet0096_trace.py   SCALE (the game's size over the trace's millimetres)
    data/cabins/osa_ak.json              the panel pwr_cabinet (the door), its controls and lamps, the mesh cabinet_0096
    data/cabins/osa_ak_shell.json        the plinth under it, which moves with it

    python tools/recon/osa/migrations/cabinet_2026_11.py [--dry]
"""
import importlib
import math
import os
import re
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OSA = os.path.dirname(HERE)
sys.path.insert(0, OSA)
import jsonedit as J  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(OSA)))
sys.path.insert(0, os.path.join(REPO, 'tools', 'blender'))
LAYOUT = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
SHELL = os.path.join(REPO, 'data', 'cabins', 'osa_ak_shell.json')
TRACE = os.path.join(REPO, 'tools', 'blender', 'cabinet0096_trace.py')
CAMERAS = os.path.join(REPO, 'tools', 'traces', 'osa_interior', 'cameras.json')


def main(dry):
    import json
    face = json.load(open(CAMERAS, encoding='utf-8'))['faces']['cab0096']
    scale, moved, n = face['scale'], np.array(face['moved_m']), np.array(face['normal'])
    if abs(scale - 1.0) < 0.004 and np.linalg.norm(moved) < 0.004:
        raise SystemExit('the plate is where the cabin has it already: nothing to do')
    facing = round(math.degrees(math.atan2(n[0], n[1])), 1)
    t = J.read(LAYOUT)
    _, panel, _ = J.resolve(t, ['panels', {'id': 'pwr_cabinet'}])
    _, mesh, _ = J.resolve(t, ['shell', {'id': 'cabinet_0096'}])
    old_c, old_m = np.array(panel['centre_m']), np.array(mesh['centre_m'])
    new_c = np.array(face['centre_m'])
    dyaw = math.radians(facing - panel['facing_deg'])            # facing and yaw: clockwise from forward, seen from above
    R = np.array([[math.cos(dyaw), math.sin(dyaw), 0], [-math.sin(dyaw), math.cos(dyaw), 0], [0, 0, 1.0]])
    new_m = new_c + scale * (R @ (old_m - old_c))
    # the trace's scale
    src = open(TRACE, encoding='utf-8', newline='').read()
    m = re.search(r'^SCALE = ([0-9.]+)', src, re.M)
    old_scale = float(m.group(1))
    new_scale = round(old_scale * scale, 4)
    src2 = src[:m.start(1)] + ('%.4f' % new_scale) + src[m.end(1):]
    note = ("# (m, 11 Nov 2026) Measured again with the door as a control plate solved together with certified cameras (tools/recon/osa/cameras.py; docs/research/OSA_REBUILD_C_2026-11.md): %.4f of the plan's size, "
            "the body %.3f m wide. The %.3f before it came through cameras the audit found 40-97 px out. It hangs on SP-2M2's assumed 1.89 m like everything else in the compartment.\n" % (new_scale, new_scale * 0.66, old_scale))
    src2 = src2[:src2.index('SCALE = ')] + note + src2[src2.index('SCALE = '):]
    print('door: centre %s -> %s, facing %.1f -> %.1f, scale x %.4f (trace SCALE %.4f -> %.4f)' % (old_c.round(4).tolist(), new_c.round(4).tolist(), panel['facing_deg'], facing, scale, old_scale, new_scale))
    print('mesh: centre %s -> %s' % (old_m.round(4).tolist(), new_m.round(4).tolist()))
    if dry:
        return
    open(TRACE, 'w', encoding='utf-8', newline='').write(src2)
    import cabinet0096_trace as T
    importlib.reload(T)
    for i in range(3):
        t = J.set_value(t, ['panels', {'id': 'pwr_cabinet'}, 'centre_m', i], float(new_c[i]), 4)
        t = J.set_value(t, ['shell', {'id': 'cabinet_0096'}, 'centre_m', i], float(new_m[i]), 4)
    t = J.set_value(t, ['panels', {'id': 'pwr_cabinet'}, 'facing_deg'], facing, 1)
    t = J.set_value(t, ['shell', {'id': 'cabinet_0096'}, 'yaw_deg'], facing, 1)
    _, (w, h) = T.panel_frame()
    t = J.set_value(t, ['panels', {'id': 'pwr_cabinet'}, 'size_m', 0], round(w, 4), 4)
    t = J.set_value(t, ['panels', {'id': 'pwr_cabinet'}, 'size_m', 1], round(h, 4), 4)
    why = (' Measured again on 11 Nov 2026 with the door as a control plate solved with certified cameras (docs/research/OSA_REBUILD_C_2026-11.md): %.2f m further %s, %.2f m further forward, %.1f %% larger, facing %.1f.'
           % (abs(new_c[0] - old_c[0]), 'right' if new_c[0] > old_c[0] else 'left', new_c[1] - old_c[1], 100 * (scale - 1), facing))
    t = J.append_to_string(t, ['shell', {'id': 'cabinet_0096'}, 'notes'], why)
    doc = json.loads(t)
    keys = set(T.control_local())
    for kind in ('controls', 'lamps'):
        for i, c in enumerate(doc[kind]):
            if c.get('panel') != 'pwr_cabinet':
                continue
            name = c.get('control') or c.get('id')
            if name in keys or (name + '.on') in keys:
                at = T.panel_at(name)
            else:
                at = (round(c['at_m'][0] * scale, 4), round(c['at_m'][1] * scale, 4))
            t = J.set_value(t, [kind, i, 'at_m', 0], at[0], 4)
            t = J.set_value(t, [kind, i, 'at_m', 1], at[1], 4)
            print('   %-8s %-22s at %s -> %s' % (kind, name, c['at_m'], list(at)))
    J.write(LAYOUT, t)
    # the plinth it stands on goes with it
    s = J.read(SHELL)
    d = new_c - old_c
    for key in ('x_from_m', 'x_to_m'):
        s = J.shift(s, ['floor', 'raised', {'id': 'plinth_under_cabinet'}, key], float(d[0]))
    s = J.shift_column(s, ['floor', 'raised', {'id': 'plinth_under_cabinet'}, 'section_yz_m'], 0, float(d[1]))
    J.write(SHELL, s)
    print('written: the trace\'s SCALE, the panel with its controls and lamps, the mesh\'s place, the plinth')


if __name__ == '__main__':
    main('--dry' in sys.argv)
