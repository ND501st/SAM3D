"""SŁ-1M 40 mm forward, and the indicator box at its measured place (16 Nov 2026, the project owner's decision). Run once,
after tools/blender/build_cabin_interior.py --only SM_RackSL1M,SM_DriverUnits,SM_CabUnits (SL1M_FORWARD); kept for the
record.

The ПАНЕЛЬ ИНДИКАЦИИ (ind_panel) was measured at 2.597 m forward through g095-b's registered camera, but stood 40 mm aft
of it because SŁ-1M's frame flange, placed from Fig. 3 (mon_2112_80), began at 2.5875. With SŁ-2M's guidance column
widened to its blocks (to 2.555) the box covered the column's forward strip. SŁ-1M is placed from a drawing scaled at
about 4.8 mm a pixel and the box from a photo good to a centimetre or two; the owner chose the photo: SŁ-1M and
everything placed by its front end (its rack's posts and top, the gyro panel and its АРРЕТИР., the vent hatch's lever
over its aft half, the R-124 boxes on its doors, the grey pillar beside its front end with its valve, and the web from
the pillar to the wall) move 40 mm forward; the box goes back to 2.597. The hull's turn-in (CAB_Y, 4.06) stays: the
rack's outboard corner sinks 9 mm into it and the web ends at the wall.

After it ran, the fire system's canister and two lying bottles were moved 20 mm along the corner wall to clear the
moved web (build_fire_system) and the shell part fire_system given the built mesh's size.

    python tools/recon/osa/migrations/sl1m_forward_2026_11.py [--dry]
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import jsonedit as J  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
LAYOUT = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
MANIFEST = os.path.join(REPO, 'art', 'cabin_interior', 'manifest.json')
MOVE = 0.04
PANELS = ('gyro_panel', 'gyro_arrest_panel', 'vent_hatch_left', 'r124_a2_panel', 'post_valve_panel', 'stowage_panel',
          'ind_panel')
SHELL = ('sl1m_face', 'panel_ind', 'r124_a2', 'r124_a4')          # placed by their centres
BUILT = {'rack_sl1m': 'SM_RackSL1M', 'driver_units': 'SM_DriverUnits'}   # built in place: their centres and sizes as built
WHY = ' 40 mm forward with SŁ-1M on 16 Nov 2026 (tools/recon/osa/migrations/sl1m_forward_2026_11.py).'


def main(dry):
    t = J.read(LAYOUT)
    if abs(J.resolve(t, ['panels', {'id': 'ind_panel'}, 'centre_m', 1])[1] - 2.557) > 1e-6:
        raise SystemExit('already moved: ind_panel is not at 2.557')
    for pid in PANELS:
        t = J.shift(t, ['panels', {'id': pid}, 'centre_m', 1], MOVE)
        if 'notes' in J.resolve(t, ['panels', {'id': pid}])[1] and pid != 'ind_panel':
            t = J.append_to_string(t, ['panels', {'id': pid}, 'notes'], WHY)
    t = J.append_to_string(t, ['panels', {'id': 'ind_panel'}, 'notes'],
                           ' Back at its measured 2.597 on 16 Nov 2026: SŁ-1M moved 40 mm forward instead (the project '
                           'owner\'s decision, tools/recon/osa/migrations/sl1m_forward_2026_11.py).')
    for sid in SHELL:
        t = J.shift(t, ['shell', {'id': sid}, 'centre_m', 1], MOVE)
        if 'notes' in J.resolve(t, ['shell', {'id': sid}])[1]:
            t = J.append_to_string(t, ['shell', {'id': sid}, 'notes'], WHY)
    with open(MANIFEST, encoding='utf-8') as f:
        built = json.load(f)
    for sid, mesh in BUILT.items():
        t = J.replace_value(t, ['shell', {'id': sid}, 'centre_m'], json.dumps(built[mesh]['centre_m']))
        t = J.replace_value(t, ['shell', {'id': sid}, 'size_m'], json.dumps(built[mesh]['size_m']))
        if 'notes' in J.resolve(t, ['shell', {'id': sid}])[1]:
            t = J.append_to_string(t, ['shell', {'id': sid}, 'notes'], WHY)
    if dry:
        print('dry run: nothing written')
        return
    J.write(LAYOUT, t)
    print('SŁ-1M and its front end 40 mm forward; the indicator box at 2.597')


if __name__ == '__main__':
    main('--dry' in sys.argv)
