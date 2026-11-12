"""The ПАНЕЛЬ ИНДИКАЦИИ (indication panel), traced from wwp_gecko g095-b. No bpy: panel_ind_model.py, the cabin layout and
tools/tests/test_panel_ind_trace.py read this.

How it was measured (docs/MODELLING_FROM_PHOTOS.md):

1. **One flat.** g095-b sees the box from aft and inboard. Its face is flattened on its own edges (the top, the selector
   plate's top, the СЭП plate's top and the two long sides: within 0.6 degrees of their vanishing points) at 5 px/mm
   (tools/traces/panel_ind_g095b.json; out/traces/panel_ind_g095b/). The face is not parallel to SŁ-2M's: fo_panel's top
   edge lies 6 degrees off the box's vanishing point.
2. **The size.** No source gives it. The height is 545 mm: the face's corners back-projected through g095-b's registered
   camera onto the plane measured on its depth map (tools/recon/holistic_diff.py measure: 536-548 mm), about 5 %. The
   width follows at the same scale, with the flat stretched across by `aspect` 1.227 so that the lamp caps come out round
   (they read 0.80 and 0.83 of round without it): 58 mm.
3. **Heights** out of the face: g095-b is oblique but shows no part's side clearly, so every height is SAM3D's (c), from
   the parts' kinds. The box's depth is SAM3D's too (c): the photo shows only a narrow strip of its aft side.

Frame: x mm to the right as you face it (towards the front of the vehicle), y mm down from the face's top, h mm out of the
face. `to_m` gives metres on the cabin panel `ind_panel` (a to the right, b up, from the face's centre).

(m) measured on the flat, about 1 mm, (c) SAM3D's choice. The legends are not drawn (decals, roadmap I10).
"""
W_MM = 58.0          # (m) see above
H_MM = 545.0         # (m) see above
DEPTH_MM = 50.0      # (c) the box behind the face
CORNER_MM = 4.0      # (m) the face's rounded corners

# The raised frame round the legend caps: bounds, corner radius, rim width and height (m bounds and radius; c rim)
CAP_FRAME = (2.0, 62.5, 57.0, 207.0)
CAP_FRAME_RADIUS = 8.0
CAP_FRAME_RIM = 2.5
CAP_FRAME_H = 4.0
CAP_FRAME_SCREWS = ((24.0, 71.5), (34.0, 202.0))   # (m) the frame's two captive screws, top and bottom

# The legend caps (m): two staggered columns, 21.5 mm across, standing CAP_H out of the face (c)
CAP_D = 21.5
CAP_H = 6.0
CAPS = [
    {'id': 'cap_l1', 'c': (15.5, 91.2)}, {'id': 'cap_l2', 'c': (15.5, 116.2)}, {'id': 'cap_l3', 'c': (15.5, 141.0)},
    {'id': 'cap_l4', 'c': (15.5, 165.0)}, {'id': 'cap_l5', 'c': (15.5, 189.5)},
    {'id': 'cap_r1', 'c': (38.8, 80.8)}, {'id': 'cap_r2', 'c': (38.8, 104.7)}, {'id': 'cap_r3', 'c': (38.8, 129.5)},
    {'id': 'cap_r4', 'c': (38.8, 154.0)}, {'id': 'cap_r5', 'c': (38.8, 177.5), 'lamp': 'dver_zakryta'},
]

# The selector's black plate (m): a rectangle with a U cut up round the knob, an arch over it
SEL_PLATE = (0.5, 221.8, 56.5, 263.0)
SEL_ARCH_C = (28.5, 252.5)
SEL_ARCH_R = 16.5
SEL_PLATE_SCREWS = ((3.5, 224.5), (53.5, 224.5), (3.5, 260.0), (53.5, 260.0))
SEL_KNOB = (28.5, 251.0)      # (m) the pointer knob's shaft
SEL_KNOB_D = 20.0             # (m) across its body
# (m) the seven positions, degrees clockwise from straight up: 3—II, 2—II, 1—II, ОТКЛ., 1—I, 2—I, 3—I (the legends at
# (4, 253), (7, 238.5), (15, 230), (28, 230) and their mirror; the names voenizdat_osa_1991 p247's); the mesh marks them
SEL_POSITIONS_DEG = (-90.0, -60.0, -30.0, 0.0, 30.0, 60.0, 90.0)
SEL_MARK_R = 18.5             # (c) the marks' radius, on the plate just outside the arch

# ОТКАЗ: the black plate and the red lamp under it (m)
OTKAZ_PLATE = (11.5, 292.0, 45.0, 305.0)
OTKAZ_PLATE_SCREWS = ((13.5, 298.5), (43.0, 298.5))
OTKAZ_LAMP = (28.0, 325.0)
OTKAZ_BEZEL_R = 13.5          # (m) the chrome ring
OTKAZ_LENS_R = 10.0           # (m) the red lens

# СЭП АВАР. ОТКЛ. and УКЧ: the black plate, the grey sub-plate the two sit on, their red rings (m). The left is a lamp
# (a dark red lens: the lit push-button АВАР. ОТКЛ. СЭП, voenizdat_osa_1991 p163), the right a toggle (its lever)
SEP_PLATE = (1.5, 484.5, 54.0, 533.5)
SEP_SUBPLATE = (2.0, 497.0, 42.0, 520.0)
SEP_PLATE_SCREWS = ((4.0, 487.5), (50.5, 487.5), (4.0, 531.0), (50.5, 531.0))
SEP_LAMP = (9.5, 508.5)
UKCH_TOGGLE = (29.5, 508.5)
GUARD_RING = (6.2, 7.5)       # (m) inner and outer radius of the red and brass rings round them

# The plugs (m outline on the flat, c depth): Ш2 under the box, Ш1 on its forward side
PLUG_SH2_PLATE = (6.5, 546.0, 56.0, 563.5)     # the label plate across its front
PLUG_SH2_SHELL = (17.0, 563.5, 46.0, 600.0)    # the plug's shell below it
PLUG_SH1 = (60.0, 432.0, 100.0, 535.0)         # x beyond the face: it stands on the forward side
PLUG_SH1_H = (6.0, 36.0)                       # (c) out of the face: proud of it, in front of SŁ-1M's frame flange (g091-a)

# Where the cabin's controls and lamps stand on the face (mm): the mesh leaves them to the cabin
CONTROL_MM = {
    'pwr.emergency_off': SEP_LAMP,
    'sep_avar_otkl': SEP_LAMP,
    'pwr.ukch': UKCH_TOGGLE,
    'ind.select': SEL_KNOB,
    'dver_zakryta': (38.8, 177.5),
}
CONTROL_D_MM = {'pwr.emergency_off': 11.0, 'sep_avar_otkl': 11.0, 'pwr.ukch': 12.0, 'ind.select': 20.0, 'dver_zakryta': 21.5}


def to_m(x, y):
    """Face mm to metres on the cabin panel: a to the right, b up, from the face's centre."""
    return ((x - W_MM / 2.0) * 0.001, (H_MM / 2.0 - y) * 0.001)


def sel_plate_outline(seg=16):
    """The selector plate's outline (mm, clockwise from its top left), the U cut and its arch."""
    import math
    x0, y0, x1, y1 = SEL_PLATE
    cx, cy = SEL_ARCH_C
    r = SEL_ARCH_R
    pts = [(x0, y0), (x1, y0), (x1, y1), (cx + r, y1), (cx + r, cy)]
    for k in range(1, seg):
        a = math.pi * k / seg                       # from the right round over the top to the left
        pts.append((cx + r * math.cos(a), cy - r * math.sin(a)))
    pts += [(cx - r, cy), (cx - r, y1), (x0, y1)]
    return pts
