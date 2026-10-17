"""The PU-12M's exterior traced into metres (docs/MODELLING_FROM_PHOTOS.md, adapted to a vehicle; docs/PU12_DEVELOPMENT.md P5).

No bpy: plain data for the Blender model (pu12m_model.py), the tests (tools/tests/test_pu12m_trace.py) and Unreal's mast.

How it was traced:
* The hull is the BTR-60PB's without the turret (flak11 PU12-0: the BTR-60PU base, the turret opening plated over).
* Side profile: tools/traces/pu12m_side.json, read off a BTR-60PB's right side (toadman_btr60pb) with tools/vehicle_trace.py:
  along the hull a 1-D projective map through the four hubs on the published wheelbase, heights scaled so the roof is the
  published 2.055 m (valka_pu12). Checks: ground clearance 0.49 m (published 0.475), the BTR-60PB's turret top 2.39 m
  (published 2.42, btvt_btr60), the tyre 1.05-1.12 m (13.00-18). The photo is taken from the front quarter (yaw about
  17 degrees); one view cannot separate its foreshortening from the lateral offsets of the hull's ends, so the overhangs
  are stretched to the published length (7.22 m, valka_pu12 and btvt_btr60) keeping the photo's split (rear 1.60 m, front
  1.22 m; an artist's drawing has 1.35 / 1.29 m).
* Plan widths: the ČSLA manual's equipment plan (csla_pu12_manual, the hull outline): the bow plate 0.46 of the full width,
  full width from 12.8 % of the length behind the bow to 86.7 %, the stern 0.69 of it. Scaled to the published width
  (2.825 m, valka_pu12) separately from the length (the drawing's ratio is 4 % off the published one).
* The roof's and the bottom's widths: 0.75 and 0.64 of the full width, from a front-view drawing and the museum's front
  photograph (opisybroni_pu12): scaled, about 5 %.
* PU-12M fittings: the mast on the right at 30 % of the length from the bow, 0.69 m off the centreline (csla_pu12_manual
  item 10; a museum photograph, wikimedia_pu12_spb, shows it at the right roof edge); stowed it stands about 1.5 m above the
  roof (wikimedia_pu12_vtm), raised 16 m (flak11 PU12-1); guyed at two heights (csla_pu12_manual). The roof box in place of
  the turret (flak11 PU12-0), the whips, rails and lights are placed from the photographs: scaled, about 10 %.

Frame: X forward from the middle of the wheelbase, Y to the left, Z up from the ground; metres.
"""

# ---------------------------------------------------------------------------------------------- published

LENGTH = 7.22          # valka_pu12 (7 220 mm), btvt_btr60
WIDTH = 2.825          # valka_pu12
ROOF_Z = 2.055         # valka_pu12: height to the top of the armour
CLEARANCE = 0.475      # valka_pu12
WHEELBASE = 4.4        # ru_wiki_btr60
TRACK = 2.38           # ru_wiki_btr60
TYRE_RADIUS = 0.55     # 13.00-18: 457 mm rim + 2 x 0.9 x 330 mm gives 0.525; the camera fit reads 0.573
TYRE_WIDTH = 0.33      # 13.00-18 section width
MAST_TOP_RAISED = 16.0  # flak11 PU12-1, valka_pu12 (the signalman extends the 16 m mast): top above the ground

HALF_W = WIDTH / 2.0

# Axles from the photo's hub spacings (1.393 : 1.614 : 1.393 of the 4.4 m wheelbase), X from the middle.
AXLES_X = (-2.2, -0.807, 0.807, 2.2)

# The hull's ends, the bow's edge and the stern box: fitted with the photo's camera (tools/vehicle_fit.py, pu12m_side.json:
# the focal length from the photo's EXIF, the published length, roof height, track and plan widths as priors; 3.5 px rms
# over 12 points). The fit puts the wheelbase at 4.40 m with the published value only a weak prior.
STERN_X = -2.2 - 1.29
BOW_X = 2.2 + 1.53

# ---------------------------------------------------------------------------------------------- hull stations
# Each: X, then (Z, half-width) at the bottom, the knee (the top of the lower side plate, which stands nearly upright
# behind the tyres), the chine (the widest line: a ledge that overhangs the tyres) and the top of the side (the roof edge,
# or the top of the sloping end plates). X and Z of the corners back-projected with the fitted camera onto the plane each
# lies in (vehicle_fit.py backproject): the chine's rear end (-3.26, 1.37), its front end (2.87, 1.39), the lower bow
# plate's foot (3.09, 0.88), the roof's rear corner (-2.55); the roof's front corner (2.30) between its two readings.
# Widths from the plan; the knee read off the photograph (the dark lower plate behind the tyres, about 1.25 m up):
# scaled, about 5 %.
ROOF_HALF_W = 0.75 * HALF_W
BOTTOM_HALF_W = 0.64 * HALF_W
KNEE_HALF_W = 0.98
KNEE_Z = 1.25
STERN_HALF_W = 0.69 * HALF_W
BOW_HALF_W = 0.46 * HALF_W
BOW_EDGE_Z = 1.609

STATIONS = [
    # X,      bottom (z, w),             knee (z, w),           chine (z, w),       top (z, w)
    (-3.257, (0.829, 0.80),             (1.20, 0.92),          (1.366, 1.20),      (1.366, 1.20)),         # the rear slope meets the chine
    (-2.547, (0.60, BOTTOM_HALF_W),     (KNEE_Z, KNEE_HALF_W), (1.38, HALF_W),     (ROOF_Z, ROOF_HALF_W)),  # the roof's rear corner
    (-2.2,   (CLEARANCE, BOTTOM_HALF_W), (KNEE_Z, KNEE_HALF_W), (1.39, HALF_W),    (ROOF_Z, ROOF_HALF_W)),
    (2.30,   (CLEARANCE, BOTTOM_HALF_W), (KNEE_Z, KNEE_HALF_W), (1.39, HALF_W),    (ROOF_Z, ROOF_HALF_W)),  # the roof's front corner
    (2.84,   (0.70, 0.86),              (1.24, 0.96),          (1.39, HALF_W),     (1.887, 0.905)),        # full width ends (plan)
    (3.09,   (0.884, 0.78),             (1.26, 0.90),          (1.40, 1.205),      (1.809, 0.834)),        # the lower bow plate's foot
    (BOW_X,  (BOW_EDGE_Z, BOW_HALF_W),  (BOW_EDGE_Z, BOW_HALF_W), (BOW_EDGE_Z, BOW_HALF_W), (BOW_EDGE_Z, BOW_HALF_W)),  # the bow's edge
]
# The water-jet housing under the stern: a box behind the hull's rear plate (fitted: Z 0.83-1.32).
STERN_BOX = {'x0': STERN_X, 'x1': -3.257, 'z0': 0.829, 'z1': 1.321, 'half_w': STERN_HALF_W}

# ---------------------------------------------------------------------------------------------- wheels

WHEELS = [(x, side * TRACK / 2.0) for x in AXLES_X for side in (1.0, -1.0)]

# ---------------------------------------------------------------------------------------------- roof and fittings

# Roof hatches (centre X, Y, length along X, width): the driver's and commander's at the front of the roof, the two
# troop hatches in the middle (the BTR-60PB's; the left one is under the roof box on the PU-12M). Scaled from the
# photographs and the drawing: about 10 %.
ROOF_HATCHES = [
    (2.02, 0.45, 0.50, 0.50),   # over the front seats (csla_pu12_manual plan: the seats 1.2-1.7 m behind the bow)
    (2.02, -0.45, 0.50, 0.50),
    (0.10, -0.45, 0.80, 0.62),
]
# The engine deck's grilles over the rear (X from, to; Y half-width).
ENGINE_GRILLES = [(-2.55, -1.45, 0.42, 0.2), (-2.55, -1.45, -0.42, 0.2)]
# Vision blocks along the top of the bow plate (X, Y): three each side, the driver's and the commander's.
VISION_BLOCKS = [(2.45, y) for y in (0.75, 0.55, 0.35, -0.35, -0.55, -0.75)]
# Headlights at the bow's top corners, the searchlight beside the commander (X, Y, Z).
HEADLIGHTS = [(2.55, 0.86, 1.98), (2.55, -0.86, 1.98)]
SEARCHLIGHT = (2.15, 0.88, 2.30)
# The roof box in place of the turret (flak11 PU12-0): on the left, over the second axle from the rear.
ROOF_BOX = {'x0': -1.25, 'x1': -0.35, 'y0': 0.15, 'y1': 0.85, 'z0': ROOF_Z, 'z1': ROOF_Z + 0.65}
# Whip antennas (X, Y): at the right front corner of the roof and on the left over the middle (csla_pu12_manual items 3
# and 28; the photographs).
WHIPS = [(1.95, -0.95), (-0.10, 0.98)]
WHIP_LENGTH = 2.6
# Rails along the roof's sides (X from, to; Y): a long one on the left over the rear half, one on the right forward of
# the middle (wikimedia_pu12_spb): scaled, about 15 %.
RAILS = [(-2.4, -0.4, 0.98), (0.0, 1.4, -0.98)]
RAIL_HEIGHT = 0.55
# Side hatches (centre X, Z, length, height; both sides): the large hatch forward of the middle (toadman_btr60pb, read with
# the fitted camera) and the PU-12's large hatch at the middle, ahead of the stowage box (wikimedia_pu12_spb). Scaled,
# about 10 %.
SIDE_HATCHES = [(1.05, 1.72, 0.70, 0.45), (-0.05, 1.70, 0.75, 0.42)]
# The stowage box on the right upper side over the rear half (wikimedia_pu12_spb): X from, to; Z from, to; how far it
# stands off the plate. Scaled, about 15 %.
SIDE_BOX = {'x0': -2.0, 'x1': -0.6, 'z0': 1.62, 'z1': 1.86, 'depth': 0.12}
# The exhaust pipe along the right rear (from, to X; Z; Y).
EXHAUST = (-3.3, -2.0, 1.75, -1.18)

# ---------------------------------------------------------------------------------------------- the mast

MAST_X = BOW_X - 0.30 * LENGTH   # 30 % of the length from the bow (csla_pu12_manual item 10)
MAST_Y = -0.69                   # on the right (negative Y)
MAST_FOOT_Z = 0.65               # it stands on the floor inside the hull and comes up through the roof (csla_pu12_manual item 10)
MAST_BASE_TOP_Z = ROOF_Z + 1.2   # the fixed base tube's top, 1.2 m above the roof
MAST_STOWED_TOP_Z = ROOF_Z + 1.5  # retracted, about 1.5 m above the roof (the photographs)
# Sections from the base outward: radius and length; each slides out of the one below it. Stowed, they nest down into the
# hull to its floor. Radii scaled from the photographs (about 15 %); the lengths are SAM3D's (long enough to overlap
# 0.3 m when raised).
MAST_SECTIONS = [(0.075, MAST_BASE_TOP_Z - MAST_FOOT_Z), (0.065, 2.9), (0.055, 2.9), (0.046, 2.9), (0.038, 2.9), (0.030, 2.9)]
# Guys at two heights (fraction of the raised height), four each, to stakes this far out (csla_pu12_manual's drawing).
GUY_LEVELS = (0.5, 0.95)
GUY_RADIUS = 6.0


def mast_section_tops(extension):
    """Z of each section's top for an extension from 0 (stowed) to 1 (raised to MAST_TOP_RAISED). Section 0 is the
    fixed base; each moving section rises an equal share of the travel."""
    tops = [MAST_BASE_TOP_Z]
    moving = len(MAST_SECTIONS) - 1
    travel = (MAST_TOP_RAISED - MAST_STOWED_TOP_Z) / moving
    e = max(0.0, min(1.0, extension))
    for i in range(1, len(MAST_SECTIONS)):
        stowed_top = MAST_STOWED_TOP_Z - (moving - i) * 0.05  # nested inside each other, a little lower each
        tops.append(stowed_top + e * travel * i)
    return tops


# ---------------------------------------------------------------------------------------------- the AB-1 set

# Set on the ground behind the vehicle in the combat position (csla_pu12_manual's drawing): a small petrol set on a frame.
# Its size is SAM3D's estimate (a 1 kW set of the AB-1 class).
AB1 = {'x': STERN_X - 1.5, 'y': -0.9, 'size': (0.80, 0.50, 0.60)}
