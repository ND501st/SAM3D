"""The driver's and commander's seat, traced (no bpy): docs/OSA_INTERIOR_HOLISTIC.md section 23.

Sources, all wwp_gecko (a BM3-1, session of g082-g089), read by hand:
- g088-c: the commander's back from behind, flattened by its four corners to a square (tile out/sp1m/seat_flat.jpg);
- g088-a: both cushions from above, the backs' top rims with their domed rivets;
- g082-b: both backs from behind, the hoops, the bags, the hinge arms down to the rails;
- g087-c, g088-b: the pads from the front, the rims' worn welt.
The yardstick: the seat's width, 0.50 m. mon_2112_80 Fig. 3 draws the seats 0.49 and 0.51 wide at its calibrated scale,
and the width over the seats' centre spacing is 0.77 in Fig. 3, 0.77 in g082-b and 0.78-0.80 in g088-a. About +-0.02 m.

Face coordinates: mm, x to the right seen from behind, y down from the back's top edge, the frame of g088-c. The back's
shape is its own measure (square within 1 %); everything out of its plane (Z) is read by eye from g082-b, g087-c, about
+-10 mm. The lean is by eye too, and is what the photos show the seat parked at.
"""

W_MM = 500.0            # across the back and the cushion
H_MM = 500.0            # the back, its top edge to its foot (g088-c: square)
D_CUSHION_MM = 460.0    # the cushion, front to back: 0.51 m the seat in Fig. 3, less the back's foot
CORNER_MM = 55.0        # the outline's corners (g088-a, g088-c)

# Depths, mm, out of the back's plane: + towards the rear (the pan's side), - towards the sitter
Z = {
    'pan_rear': 0.0,      # the pan's rear face
    'pan_rim': 6.0,       # the rolled rim stands this far behind the pan's rear face
    'pad_rear': -30.0,    # the pan's front, where the pad starts
    'pad_front': -95.0,   # the pad's front, crowned to this at its middle
    'hoop': 40.0,         # the hoop's centre behind the pan
    'lug': 16.0,          # the lugs that carry the hoop, their free ends
    'bag_rear': 150.0,    # the gas-mask bag hanging on the hoop, its outermost
    'boss': 12.0,         # the pressed boss round the wing bolt
    'wing': 22.0,         # the wing bolt
}

# The pressed boss and its wing bolt
BOSS = (205.0, 73.0, 308.0, 122.0)
WING = (256.0, 97.0)

# The hoop's centre line: an inverted U over the upper half, its legs carrying on down to the lower lugs
HOOP = [(31.0, 371.0), (34.0, 310.0), (38.0, 261.0), (45.0, 231.0), (63.0, 201.0), (94.0, 172.0), (137.0, 147.0), (186.0, 131.0),
        (249.0, 125.0), (311.0, 129.0), (367.0, 144.0), (410.0, 168.0), (443.0, 202.0), (465.0, 235.0), (475.0, 264.0), (480.0, 320.0), (484.0, 377.0)]
HOOP_R_MM = 9.5         # tube, 19 mm across (g088-c: 23 px of 1660 for the pan's 500 mm, about 7 % either way)

# The tabs holding the hoop to the pan, riveted (g088-c: (x, y))
LUGS = [(142.0, 166.0), (393.0, 173.0), (42.0, 252.0), (480.0, 266.0), (31.0, 371.0), (484.0, 377.0)]
LUG_MM = 26.0

# The hinge arms below the back, flat steel: from the pan's side to the pivot and past it to the foot
ARMS_X = (37.0, 463.0)
ARM_W_MM = 38.0
ARM_TOP_MM = 390.0
PIVOT_Y_MM = 507.0
ARM_FOOT_MM = 577.0      # where the flattened frame ends; the arms carry on to the rails (`build`)
ROD = (131.0, 425.0, 526.0)   # the cross rod: x0, x1, y
ROD_R_MM = 6.0

# The rim's domed rivets (pattern from the visible ones: the top edge 7 at about 75 mm, the sides 6)
RIVET_R_MM = 5.0


def rivets():
    """The rim's rivets, (x, y) on the rear face, in from the edge by 10 mm."""
    out = []
    for i in range(7):
        out.append((40.0 + i * 70.0, 10.0))
    for i in range(7):
        out.append((40.0 + i * 70.0, H_MM - 10.0))
    for i in range(1, 6):
        y = 10.0 + i * (H_MM - 20.0) / 6.0
        out.append((10.0, y))
        out.append((W_MM - 10.0, y))
    return out


# The bag: on the hoop, its flap down over the front (g082-b, g088-c). x and y, the bag's outline on the back
BAG = (130.0, 165.0, 400.0, 460.0)
BAG_THICK_MM = 95.0
FLAP = (155.0, 170.0, 375.0, 355.0)
STRAP = (266.0, 355.0, 296.0, 470.0)

# How the back stands on the seat
LEAN_DEG = 30.0         # from the vertical, its top to the rear: g082-b's pans read 0.40 m tall for 0.50 true, and g088-c
                        # shows the pan square-on from above; the drive position is not photographed
