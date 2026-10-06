"""Photograph the 3D cabin from any point: starts Play if needed, puts the cabin camera at body-frame views
(ASimOsaVehicle::SetCabinViewOverride) and takes a HighResShot of each. Needs the editor open (tools/ue_remote.py).

    python tools/ue_cabin_shots.py out/cabin_shots                                 # the default set of views
    python tools/ue_cabin_shots.py out/shots "cab:-0.36,3.72,1.82,0,-16,80"        # name:x,y,z,yaw,pitch,fov

Body frame of data/cabins/<system>.json: metres, x right, y forward, z up; yaw clockwise from forward, pitch up.
Env: SHOOT_WAIT (s after Play starts, default 12), SHOOT_SETTLE (s per view, default 2.5). Leaves Play running:
stop it before importing assets (importing during Play has crashed the editor).
"""
import glob
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_remote import Editor  # noqa: E402

SHOTS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'unreal', 'SAM3D', 'Saved',
                     'Screenshots', 'WindowsEditor')
DEFAULT = {
    'bench_senior': (-0.33, 1.555, 1.83, -90, -8, 80),
    'guidance': (-0.33, 2.2, 1.83, -90, -10, 80),
    'range': (-0.33, 0.91, 1.83, -90, -10, 80),
    'aisle_aft': (0.15, 3.35, 1.78, 180, -14, 90),
    'aisle_fwd': (0.3, -0.55, 1.72, 2, -8, 90),
    'cab_driver': (-0.36, 3.72, 1.82, 0, -16, 80),
    'cab_commander': (0.33, 3.72, 1.82, 0, -16, 80),
    'right_racks': (-0.25, 1.6, 1.7, 90, -6, 90),
    'operator_turned': (-0.33, 0.91, 1.83, 150, -18, 85),
}
Q = r'''
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cars = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimOsaVehicle) if world else []
print('CARS', len(cars))
'''
VIEW = r'''
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
car = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimOsaVehicle)[0]
car.set_cabin_view_override(unreal.Vector(%f, %f, %f), unreal.Rotator(roll=0.0, pitch=%f, yaw=%f), %f)
print('ok')
'''


def shoot(ue, dest):
    before = set(glob.glob(os.path.join(SHOTS, '*.png')))
    ue.run("unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)"
           ".get_game_world(), 'HighResShot 1')")
    for _ in range(80):
        new = set(glob.glob(os.path.join(SHOTS, '*.png'))) - before
        if new:
            time.sleep(0.5)
            shutil.move(new.pop(), dest)
            return True
        time.sleep(0.25)
    return False


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    views = {}
    for a in sys.argv[2:]:
        n, _, v = a.partition(':')
        views[n] = tuple(float(t) for t in v.split(','))
    views = views or DEFAULT
    with Editor(timeout_s=10) as ue:
        ok, o = ue.run(Q)
        if 'CARS 0' in o or not ok:
            ue.run("unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()")
            for _ in range(120):
                time.sleep(1)
                ok, o = ue.run(Q)
                if ok and 'CARS 1' in o:
                    break
            time.sleep(float(os.environ.get('SHOOT_WAIT', '12')))
        for name, (x, y, z, yaw, pitch, fov) in views.items():
            ok, o = ue.run(VIEW % (x, y, z, pitch, yaw, fov))
            if not ok:
                print('view failed', name, o)
                continue
            time.sleep(float(os.environ.get('SHOOT_SETTLE', '2.5')))
            print(name, shoot(ue, os.path.join(out, name + '.png')))


if __name__ == '__main__':
    main()
