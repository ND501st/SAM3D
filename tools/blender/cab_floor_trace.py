"""The cab's floor from the book's photos (docs/OSA_INTERIOR_HOLISTIC.md section 24): the raised area the seats stand on and the
footwell ahead of it. Body frame, metres; heights are given above the cabin's floor. No bpy.

Measured (the forward group's solve, points drawn on the photos): STEP_M, TOE_RISE_M, TUBE_ABOVE_MAT_M.
By eye from g082-b, g083-a, g088-a: the frame between the seats, the mat, its edge, the tubes' places, the arches.
SAM3D's choice: the area's outline in plan, fitted between the front racks, and where it ends aft (the model's aisle stays at
the cabin's floor, so its after face is a step the photographs do not settle)."""

STEP_M = 0.36              # deck over the footwell; the solve gives 0.35-0.40 (mat 0.24-0.29, tubes crossing the seats' gap 0.64)
TOE_RISE_M = 0.18          # the mat 0.24 to the commander's floor 0.42 further forward, g083-a and g085-d
TOE_Y = 4.55               # where the floor starts up towards the bow, by eye
TUBE_ABOVE_MAT_M = 0.395   # solve, g083-a and g088-a, 34-35 points

DECK_X = (-0.64, 0.66)     # stops short of the R-124 boxes on the racks' lower walls (faces x -0.651, 0.674)
DECK_AFT_Y = 2.60          # at least this far aft: g082-b's frame reaches y 2.6 at the deck's level and shows no step (docs/OSA_INTERIOR_HOLISTIC.md section 29)
DECK_FORE_Y = 4.02         # the cushions' fronts are at 3.98
GAP_X = (-0.10, 0.07)      # the opening between the seats, as the seats' inner edges
GAP_AFT_Y = 3.42           # the deck runs across the gap aft of here: g082-b's mat is continuous behind the seats

FRAME_T_M = 0.03           # the pressed cross-member at the front of the seats (g083-a)
FRAME_X = (-0.215, 0.185)
FRAME_HOLE = (-0.015, 0.18, 0.17, 0.24, 0.05)   # centre across, centre above the floor, width, height, corner radius

TUBES = ((3.52, 0.03), (3.66, 0.0))          # the two tubes across the gap: y, height against the deck's top
TUBE_R_M = 0.012
ROD = (-0.015, 3.46, 0.48)                      # the vertical rod in the gap: x, y, height above the floor
ROD_R_M = 0.009

MAT_X = (-0.255, 0.535)    # g082-b, camera on the mat's own vanishing point (docs/OSA_INTERIOR_HOLISTIC.md section 30): 0.79 wide, right of the seats' middle
MAT_Y = (2.78, 3.33)       # the same: its width against the horizon agrees at both ends (0.97, 0.96)
RIB_PITCH_M = 0.014
MAT_BOLTS = [(-0.24, 2.795), (0.52, 2.795), (-0.24, 3.315), (0.52, 3.315), (0.14, 2.795)]
DISC_PLATE = (-0.09, 0.13, 3.19, 3.27)          # the plate with the red disc, on the mat 0.1 from its forward end (g082-b, section 30)
DISC = (0.02, 3.23, 0.055)

FOOT_MAT_X = (-0.70, 0.62)  # g083-a: one wide ribbed mat across the footwell
FOOT_MAT_Y = (4.06, 4.60)

ARCH_Y = (4.06, 4.62)      # the front wheels' housings along the turned-in corners (g083-a, right; the left is mirrored)
ARCH_SECTION = [(0.84, 0.0), (1.19, 0.0), (1.19, 0.56), (1.0, 0.50), (0.90, 0.40), (0.84, 0.24)]   # x, height above the floor

GEAR_BRACKET_Y = (4.05, 4.14)
GEAR_TOP_Z = 1.26          # by eye from g083-a, about 0.3 above the frame
