"""Stage S12 (optional): the plate hull as a STEP solid, run by FreeCAD's command-line interpreter.

    freecadcmd tools/recon/freecad_step.py <hull_faces.json> <out.step>

The hull is welded flat plates, so its exact B-rep is planar faces bounded by the plates' polygons: OpenCASCADE sews
them into a shell and a solid with no surface fitting at all. Curved parts (tyres, the tracking dish, the missile
bodies) stay meshes in the game model; for CAD they would be added as primitives (cylinders, a paraboloid of
revolution) from their sourced sizes, not fitted.
"""
import json
import sys

import FreeCAD  # noqa: F401  (freecadcmd provides it)
import Part

faces_path, out = sys.argv[-2], sys.argv[-1]
data = json.load(open(faces_path, encoding='utf-8'))
faces = []
for f in data['faces']:
    pts = [FreeCAD.Vector(*(1000.0 * c for c in p)) for p in f['points_m']]  # STEP in millimetres
    faces.append(Part.Face(Part.makePolygon(pts + [pts[0]])))
shell = Part.makeShell(faces)
shell = shell.copy()
shell.sewShape()
try:
    solid = Part.makeSolid(shell)
    ok = solid.isValid() and solid.Volume > 0
except Exception:  # an open shell (a plate not traced yet): write the shell so the gap can be found
    solid, ok = shell, False
solid.exportStep(out)
print('STEP', out, 'solid' if ok else 'open shell', len(faces), 'faces')
