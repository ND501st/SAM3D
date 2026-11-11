"""The compartment's right side moved to where the photographs put it across the vehicle (11 Nov 2026, the project
owner's decision; docs/research/OSA_REBUILD_C_2026-11.md section 4). Run once; kept for the record.

Measured (tools/recon/osa/cameras.py, grade A cameras): SL-2M's face stands 2.01 m from SP-2M2's, and the turret's
axis (the canvas boot round its central duct, in g096-a and g100-b) stands 0.914 m from SP-2M2's face and 1.096 m from
SL-2M's. The cabin had SL-2M's face 1.104 m from the centreline, which agrees to a centimetre, and SP-2M2's 0.612 m
(mon_2112_80 Fig. 3), which does not. So the left side stays, and everything that hangs on the right racks moves
0.302 m outboard.

    python tools/recon/osa/migrations/frame_2026_11.py [--dry]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import jsonedit as J  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
LAYOUT = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
SHELL = os.path.join(REPO, 'data', 'cabins', 'osa_ak_shell.json')
SHIFT = 0.302
WHY = (' Moved 0.302 m outboard on 11 Nov 2026 with everything that hangs on the right racks: the turret\'s axis, cast through certified cameras (wwp_gecko g096-a, g100-b), stands 0.914 m from SP-2M2\'s face, '
       'where mon_2112_80 Fig. 3 gave 0.612 (docs/research/OSA_REBUILD_C_2026-11.md).')


def main(dry):
    t = J.read(LAYOUT)
    if abs(J.resolve(t, ['panels', {'id': 'sp2m_panel'}, 'centre_m', 0])[1] - 0.612) > 1e-6:
        raise SystemExit('already moved: sp2m_panel is not at 0.612')
    for pid in ('sp2m_panel', 'sp3m_panel'):
        t = J.shift(t, ['panels', {'id': pid}, 'centre_m', 0], SHIFT)
        t = J.append_to_string(t, ['panels', {'id': pid}, 'notes'], WHY)
    for sid in ('sp2m_face', 'sp3m_face', 'sp3m_flap', 'chest_5937', 'first_aid', 'lamp_blue_post', 'lamp_blue', 'light_blue'):
        t = J.shift(t, ['shell', {'id': sid}, 'centre_m', 0], SHIFT)
    s = J.read(SHELL)
    for path in (['wheel_arch_right', 'x_from_m'], ['wheel_arch_right', 'x_to_m'], ['floor', 'step_forward_right', 'x_from_m'], ['floor', 'step_forward_right', 'x_to_m'], ['floor', 'spine', 'x_to_m'],
                 ['floor', 'trough_right', 'x_from_m'], ['floor', 'trough_right', 'x_to_m'], ['floor', 'arch_forward_right', 'x_from_m'], ['floor', 'arch_forward_right', 'x_to_m'], ['lower_side_wall', 'aft_right', 'x_foot_m']):
        s = J.shift(s, path, SHIFT)
    for path in (['floor', 'plinth_aft_right', 'section_xz_m'], ['floor', 'raised', {'id': 'corner_slope_right'}, 'section_xz_m'], ['roof_details', 'insulators', 'line_xyz_m']):
        s = J.shift_column(s, path, 0, SHIFT)
    if dry:
        print('dry run: nothing written')
        return
    J.write(LAYOUT, t)
    J.write(SHELL, s)
    print('the right racks and what hangs on them moved %.3f m outboard in %s and %s' % (SHIFT, os.path.basename(LAYOUT), os.path.basename(SHELL)))


if __name__ == '__main__':
    main('--dry' in sys.argv)
