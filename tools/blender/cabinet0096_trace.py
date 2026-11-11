"""The distribution cabinet 0096-9M2, traced from the book's photos (wwp_gecko p. 96-97, 100). No bpy: cabinet0096_model.py and
tools/tests/test_cabinet0096_trace.py read this.

How it was measured (docs/MODELLING_FROM_PHOTOS.md):

1. **Two flats.** g096-a (nearly square-on, the whole face) and g097-b (close, oblique, the sharpest) are flattened through the four
   corners of the body's front at 4 px/mm (tools/traces/cabinet0096_g096a.json, cabinet0096_g097b.json; out/traces/cabinet0096_*/).
2. **The size.** The millimetres here take the width as the plan's 0.66 m (mon_2112_80 Fig. 3), and the cabinet in the game is all of them times `SCALE`, the photos' size (see there). The height is **680 mm, a ratio of 1.03, not the 1.12 the raw photo
   suggests**: g097-b's vanishing points give f = 51 mm (35 mm equivalent) and 1.03, g096-a 1.028 at 50 mm; with H = 680 the round lamp bezels come out
   round to 1.4 %; the book's drawing g097-c gives 1.05 for the body and 1.07 for the door. 1.12 was read straight off the raw photo.
3. **Cameras and parallax.** Both cameras are solved in the frame below (rms 1.2 and 0.5 px). Triangulating a part seen in both photos gives
   its true place and height. The door stands 15 mm proud of the frame and each part stands above the door, so a part read on the
   g096-a flat is shifted toward the camera's foot, up to 17 mm at the right of the face: every reading below is kept as read,
   with the height of what was read, and `fix` puts it back on the door.
4. **Heights** are the triangulated ones (m); where a height could not be read it is marked (c).

Frame: x mm to the right as you face the cabinet (0 = the body's hinge side), y mm down from the body front's top edge, h mm out of the door's plane
(`DOOR_PROUD` in front of the frame). z mm into the cabinet from the frame's plane, used for the body and the lid.
The cabinet's local frame (`to_local`): x from the hinge side (+) to the free side (-), y out of the door, z up.

(m) measured, (c) SAM3D's choice. The door's legends are not drawn (short labels below are only to name the parts); no photograph is baked in.
The photos are the book's session A vehicle (a 9A33BM2 or BM3, not identified).
"""
W_MM = 660.0          # (m) mon_2112_80 Fig. 3
H_MM = 680.0          # (m) see above
DEPTH_MM = 108.0      # (m) the side wall's rear edge, back-projected from g097-b onto x = 0 (103-109). The plan's 0.19 m is not what the photo shows; it may be the plinth's
DOOR_PROUD = 15.0     # (m) the door's front plane, in front of the frame's (flush features triangulated at z = -15)
CAM_A = (-649.5, 159.1, -3011.6)   # (m) g096-a's camera in this frame, z into the cabinet

# The cabinet's local frame (metres), see to_local
LOCAL_HINGE_X = 0.33   # x = 0 in the trace
LOCAL_TOP_Z = 0.48     # y = 0 in the trace (the placement point's frame)
LOCAL_DOOR_Y = 0.095   # h = 0 (today's `plate`)
# (m) The cabinet as it stands in the book's photos: 0.865 of the plan's size. The millimetres in this file keep the plan's 0.66 m as their unit, because every reading was taken on flats made at that width;
# the model is built at them and scaled once, about the mesh's origin, so what stands above the door (and the depth) scales with the face. Three readings of the width:
# the plan of the older 9A33BM2, 0.66 (a different mark of cabinet); the door's corners triangulated from g096-a and g097-b in the adjusted solve, 0.564-0.579 (height 0.597-0.601);
# g096-a's corner rays on the plane through the solve's own 287 points on the face (rms 4 mm), 0.561-0.570 (height 0.587-0.592). The last two are used.
# (m, 11 Nov 2026) Measured again with the door as a control plate solved together with certified cameras (tools/recon/osa/cameras.py; docs/research/OSA_REBUILD_C_2026-11.md): 0.9199 of the plan's size, the body 0.607 m wide. The 0.865 before it came through cameras the audit found 40-97 px out. It hangs on SP-2M2's assumed 1.89 m like everything else in the compartment.
SCALE = 0.9199
LOCAL_ORIGIN = (0.0, 0.01625, 0.0)   # the old mesh's bounding box centre in the old builder's coordinates; the placement point stays here


def fix(x, y, h=0.0):
    """A point read on g096-a's flat at height h above the door, back on its true (x, y)."""
    s = (DOOR_PROUD + h) / -CAM_A[2]
    return (round(x * (1.0 - s) + CAM_A[0] * s, 2), round(y * (1.0 - s) + CAM_A[1] * s, 2))


def fix_rect(r, h=0.0):
    a, b = fix(r[0], r[1], h), fix(r[2], r[3], h)
    return (a[0], a[1], b[0], b[1])


def to_local(x, y, h=0.0):
    """Trace mm to the cabinet's local frame in metres (x, y, z)."""
    return (LOCAL_HINGE_X - x / 1000.0, LOCAL_DOOR_Y + h / 1000.0, LOCAL_TOP_Z - y / 1000.0)


def frame_h(z):
    """The height above the door's plane of a point z mm into the cabinet."""
    return -z - DOOR_PROUD


# ---- the door, read on g096-a at its own plane (m)
DOOR = fix_rect((28.0, 10.0, 653.5, 680.0))
DOOR_RADIUS = 15.0    # (m) g096-a

# ---- the body's section (y, z) along x: the top slopes up to the rear at about 45 degrees (side-top points (-48, 46), (-108, 108) triangulated)
BODY_SECTION = [(0.0, 0.0), (-108.0, 108.0), (H_MM, 108.0), (H_MM, 0.0)]

# ---- the lid over the repair toggles (closed), z relative to the frame's plane, all triangulated from g096-a and g097-b (m, miss 2-9 mm)
LID_X = (8.0, 540.0)
LID_SECTION = [(0.0, 2.0), (-20.0, -26.0), (-140.0, 103.0), (-108.0, 108.0)]   # lip's foot, lip's top, the rear corner, down onto the slope
LID_LIP_PLATE = (168.0, 385.0)         # (m) x of the black plate on the lip, which fills it; its legend is not drawn
LID_SCREWS = (((77.9, -42.1, -20.0), 14.0), ((480.6, -39.2, -14.7), 9.0))   # (m) the tip (x, y, z); (c) its height off the lid
# The card holder lying on the lid (m): its foot on the lid, the front's foot, the bar, the rear corner, its foot on the lid
HOLDER_X = (130.0, 426.0)
HOLDER_SECTION = [(-30.0, -12.0), (-38.0, -22.0), (-60.0, -38.0), (-156.0, 56.0), (-127.0, 85.0)]
HOLDER_CATCH_X = (252.0, 278.0)        # (m) the tab at the front's middle, g097-b
HOLDER_HINGE_T = (0.32, 0.50, 0.68)    # (c) three knuckles along the left edge (g097-b; their spacing estimated)
LID_STUDS = (((441.0, -118.6, -5.1), 'stud'), ((473.9, -104.3, -17.0), 'seal'))   # (m) the tips; what they stand on could not be read (see the model)

# ---- the side (x = 0), read on g097-b back-projected on the wall (m)
LOUVRES_UPPER = (24.0, 60.0, 94.0, 129.0, 165.0, 193.0)  # y of each slit, at its rear
LOUVRES_LOWER = (566.0, 595.0, 624.0, 660.0)
LOUVRE_Z = (17.0, 104.0)
RED_PLATE = (199.0, 362.0, 13.0, 103.0)                  # y0, y1, z0, z1; a legend on the real one, not drawn

# ---- what is on the door: every reading as taken on the g096-a flat, h the height of what was read
Z = {
    'rivet': 1.5,        # (c)
    'captive': 3.0,      # (m)
    'knob': 25.0,        # (m) tips 22-27
    'knob_green': 12.0,  # (m) 10-13
    'capsule': 3.0,      # (m) 1-4
    'cap_button': 13.0,  # (m)
    'cap_toggle': 15.0,  # (m)
    'fuse_disc': 14.5,   # (m)
    'fuse_bulb': 19.0,   # (m)
    'push': 4.0,         # (m) the ring
    'meter': 27.0,       # (m)
    'dome': 32.0,        # (m) at its top
    'rail': 24.0,        # (m) the feet, where the tube comes out of the door and bends
    'rail_rod': 21.0,    # (m) the rod's run: its y on both flats (see RAIL_ROD_Y)
    'dial': 10.0,        # (m) what lies behind the bezel's front: g097-b sees it shifted against the bezel's screws by about this much less
    'tov': 5.0,          # (m)
    'strip': 5.0,        # (c)
}

PARTS = []


def _add(kind, x, y, h=None, **kw):
    h = Z[kind] if h is None else h
    p = {'kind': kind, 'c': fix(x, y, h), 'h': h}
    p.update(kw)
    PARTS.append(p)
    return p


def _rect(kind, r, h=None, **kw):
    h = Z[kind] if h is None else h
    p = {'kind': kind, 'r': fix_rect(r, h), 'h': h}
    p.update(kw)
    PARTS.append(p)
    return p


for _xy in ((284.6, 55.2), (311, 55), (338.5, 54.4), (365, 55), (284.6, 105.4), (310.6, 105), (338.5, 105), (365, 105), (201.5, 122.3), (324.6, 122.3),
            (609.4, 97.3), (429, 293.8), (157.5, 255.4), (84.4, 341), (265, 341), (212, 489), (105.6, 487), (429, 429), (477, 429), (429, 451), (477, 451),
            (90, 554.6), (276, 636), (427, 635.4), (531, 556.3), (531.5, 590.6)):
    _add('rivet', _xy[0], _xy[1], d=6.0)

for _x, _y, _r in ((356.0, 37.0, 7.0), (642.9, 38.3, 8.0), (639.6, 348.5, 8.2), (351.3, 658.8, 7.5)):
    _add('captive', _x, _y, d=2.0 * _r)

# knurled black lamp holders (m), by their short labels on the door where one could be read
for _id, _x, _y in (('stab', 210.6, 165.2), ('soc_ant', 354.8, 165.2), ('svr1', 210.6, 257.3), ('el', 245.2, 257.3), ('svr2', 277.5, 257.3), ('az', 310.8, 257.3),
                    ('k457a', 457.5, 275.2), ('phase_break', 407.1, 309.2), ('k457b', 457.5, 309.2), ('k300', 300.0, 447.5), ('compressor', 120.4, 510.8), ('k229', 228.8, 511.0),
                    ('gok', 213.0, 580.0), ('meas1', 512.5, 472.0), ('meas2', 512.0, 507.0), ('meas3', 512.5, 541.0), ('meas4', 510.5, 577.0)):
    _add('knob', _x, _y, id=_id, d=21.0)
_add('knob_green', 576.7, 75.8, id='unlocked_1', d=24.8)
_add('knob_green', 612.7, 75.8, id='unlocked_beta', d=24.8)
_add('lamp_ring', 537.3, 69.0, 3.0, d=16.0, nub=fix(548.0, 68.5, 3.0))
_add('adjust', 489.0, 48.0, 0.5, d=14.0)

_add('push', 210.2, 84.4, id='equip_on', colour='black', d=30.8)
_add('push', 255.0, 84.4, id='equip_off', colour='red', d=30.8)

# the cream moulded covers; each carries two buttons or a toggle
CAPSULES = {
    'relay_heat': (104.6, 214.6, 167.0, 244.8),
    'summer_winter': (96.3, 281.0, 160.0, 310.6),
    'stab_pair': (183.7, 193.5, 280.4, 223.7),
    'dr_unlock': (284.0, 193.5, 380.8, 223.7),
    'el_pair': (183.7, 280.4, 280.4, 310.2),
    'az_pair': (283.3, 280.4, 380.2, 310.2),
    'phases': (427.0, 215.0, 524.8, 244.8),
    'normal': (425.0, 353.8, 488.8, 383.1),
    'emergency': (425.0, 391.3, 488.8, 420.6),
}
for _id, _r in CAPSULES.items():
    _rect('capsule', _r, id=_id)

for _id, _x, _y in (('stab_on', 211.0, 207.7), ('stab_off', 254.0, 207.7), ('el_on', 210.6, 295.4), ('el_off', 254.0, 295.4), ('az_on', 310.8, 295.4), ('az_off', 353.8, 295.4),
                    ('phase_a', 454.6, 228.0), ('phase_b', 497.0, 228.5), ('normal_item', 456.7, 368.0)):
    _add('cap_button', _x, _y, id=_id, d=13.0)
for _id, _x, _y in (('relay_heat', 135.4, 229.4), ('summer_winter', 128.3, 295.8), ('dr', 311.0, 207.7), ('soc_lock', 353.8, 207.0), ('emergency', 456.7, 405.8)):
    _add('cap_toggle', _x, _y, id=_id, d=12.0)
_add('tov', 407.0, 226.5, id='tov', d=22.0, tip=fix(405.0, 243.0, 15.0))

# fuse holders: the disc read at its own height; the bulb 20 mm below it
FUSE_DISC_R, FUSE_BULB_R, FUSE_BULB_DY = 10.8, 12.5, 20.0   # (m)
FUSE_ROWS = {
    1: (366.7, (87.0, 122.0, 156.7, 193.5, 229.0, 264.6, 300.2, 335.5, 371.5, 406.5)),
    2: (429.2, (87.0, 122.0, 156.7, 193.5, 229.0, 264.6, 335.0, 371.5, 406.5)),
    3: (492.0, (87.0, 264.6, 300.2, 334.5, 370.6, 405.8, 441.0, 476.4)),
    4: (559.8, (370.2, 406.2, 440.8, 476.0)),
}
for _row, (_y, _xs) in FUSE_ROWS.items():
    for _x in _xs:
        _add('fuse_disc', _x, _y, row=_row, bulb=fix(_x, _y + FUSE_BULB_DY, Z['fuse_bulb']))

# the guard rails over the fuses: (left foot, right foot), read at the rod
RAILS = tuple((fix(a[0], a[1], Z['rail']), fix(b[0], b[1], Z['rail'])) for a, b in (
    ((59.4, 405.2), (424.0, 404.0)), ((59.4, 533.8), (486.0, 532.0)), ((59.4, 603.0), (486.0, 602.0))))
RAIL_ROD_Y = (407.0, 536.7, 606.7)   # (m) the rod runs 6-9 mm below its feet: its bright edge on the two flats, solved for y and h
RAIL_ROD_R = 3.0      # (m) about 6 mm across
RAIL_FOOT_R = 6.0     # (m)

# the two black strips of five, at the right; (rect, screws)
STRIPS = (((560.2, 429.2, 578.0, 515.8), ((569.0, 421.5), (567.5, 522.3))),
          ((559.8, 553.7, 575.8, 638.7), ((567.5, 545.8), (566.5, 646.0))))
for _r, _sc in STRIPS:
    _rect('strip', _r, screws=[fix(s[0], s[1], Z['strip']) for s in _sc])

_rect('typebox', (553.0, 21.0, 605.0, 34.4), 0.5)
_rect('typebox', (609.4, 21.0, 629.6, 34.4), 0.5)
_rect('tag', (317.0, 671.3, 334.0, 677.5), 0.5, colour='red')
_rect('tag', (393.0, 670.4, 410.0, 676.0), 0.5, colour='blue')

# ---- the hinges, on the hinge side: (barrel y0, y1, ear y top, ear y bottom, ear x, leaf y0, y1, screws)
HINGES = (
    (27.0, 49.0, 20.0, 56.0, 20.0, 30.0, 45.0, ((50.8, 37.0), (65.0, 37.0))),
    (337.0, 359.0, 329.4, 365.0, 17.7, 341.0, 357.0, ((48.8, 348.8), (63.3, 348.8))),
    (649.0, 669.4, 637.7, 672.3, 17.7, 649.0, 665.6, ((48.8, 657.0), (63.3, 657.0))),
)
HINGE_BARREL_X = fix(20.0, 0.0, 0.0)[0]   # (m) the barrel's axis
HINGE_LEAF_X = fix(70.0, 0.0, 1.0)[0]     # (m) the leaf's free end
HINGE_EAR_HEIGHT = -8.0                   # (m) on the frame, below the door's plane

# ---- the meters (m), read at the bezel's front
# The bezels' screws are the only points read at the bezel's own height, so they give each meter's axis; what is behind the bezel's front (dial, sticker) is drawn at Z['dial'] about that axis.
HOURS = {'screws': ((69.0, 88.0), (159.5, 88.0), (69.0, 179.4), (159.4, 179.4)), 'small_screws': ((69.0, 147.0), (159.5, 145.0)),
         'flange_r': 55.0, 'ear_r': 12.0,       # (m) the round bezel with four ears (g097-b)
         'ring_r': 52.0, 'aperture_r': 46.5,    # (m)
         'window_r': 49.0, 'window_corner_dy': 5.0, 'window_centre_dy': -6.0,   # (m, g097-b) its top arc about the axis; its bottom an arc rising to the middle
         'drums_dx': -4.0, 'drums_dy': -28.0}   # (m)
VOLT = {'bezel': (380.0, 78.0, 495.6, 190.0), 'screws': ((390.0, 85.4), (489.8, 85.4), (389.4, 181.9), (486.9, 181.9)),
        'aperture_inset': 11.0,                 # (m) the frame round the dial's recess, from the bezel's outline
        'dial_half_w': 47.0, 'dial_top_dy': -38.0, 'dial_bottom_dy': 8.0,    # (m) about the axis, g097-b at Z['dial']
        'sticker': (455.0, 150.0, 476.0, 170.0)}


def axis_of(screws, h):
    pts = [fix(a, b, h) for a, b in screws]
    return (sum(q[0] for q in pts) / len(pts), sum(q[1] for q in pts) / len(pts))


HOURS['axis'] = axis_of(HOURS['screws'], Z['meter'])
VOLT['axis'] = axis_of(VOLT['screws'], Z['meter'])
DOME = {'left': fix(400.0, 78.0, 0.0)[0], 'right': fix(484.0, 78.0, 0.0)[0], 'top': fix(442.0, 40.0, 0.0)[1], 'base': fix(442.0, 78.0, 0.0)[1],   # (m) read where it meets the door
        'ear_screws': (fix(405.0, 49.0, 3.0), fix(474.4, 49.0, 3.0))}

# ---- the six plugs under the body (m, triangulated)
PLUG_X = tuple(80.0 + 77.0 * k for k in range(6))   # Ш6 ... Ш1 left to right
PLUG_Z = 60.0         # the axis, mm into the cabinet
PLUG_R = 35.0         # the shell (m, about)
PLUG_BANDS = {3: 'red', 4: 'blue'}

# ---- the working controls the simulation has: the part each one is, as (read on the flat, height of what was read)
CONTROL_READ = {
    'pwr.equipment.on': ('push: the black button of the pair', 210.2, 84.4, 4.0),
    'pwr.equipment.off': ('push: the red button', 255.0, 84.4, 4.0),
    'stab.power.on': ('capsule button', 211.0, 207.7, 13.0),
    'stab.power.off': ('capsule button', 254.0, 207.7, 13.0),
    'lamp.stab_on': ('lamp holder', 210.6, 165.2, 25.0),
    'soc.rotation_lock': ('capsule toggle', 353.8, 207.0, 15.0),
    'lamp.soc_rot_locked': ('lamp holder', 354.8, 165.2, 25.0),
    'tov.supply': ('toggle', 407.0, 226.5, 5.0),
    'drv.el.on': ('capsule button', 210.6, 295.4, 13.0),
    'drv.el.off': ('capsule button', 254.0, 295.4, 13.0),
    'lamp.drive_el_on': ('lamp holder', 245.2, 257.3, 25.0),
    'drv.az.on': ('capsule button', 310.8, 295.4, 13.0),
    'drv.az.off': ('capsule button', 353.8, 295.4, 13.0),
    'lamp.drive_az_on': ('lamp holder', 310.8, 257.3, 25.0),
    'lamp.apu_unlocked': ('green lamp at the right', 612.7, 75.8, 12.0),
    'apu.lock_candidate': ('capsule toggle, NOT identified as apu.lock', 311.0, 207.7, 15.0),
}
CONTROL_MM = {k: fix(v[1], v[2], v[3]) for k, v in CONTROL_READ.items()}


def control_local():
    """Each control's local (x, z) in metres."""
    out = {}
    for k, v in CONTROL_READ.items():
        p = to_local(CONTROL_MM[k][0], CONTROL_MM[k][1], v[3])
        out[k] = (round(p[0], 4), round(p[2], 4))
    return out


def panel_frame():
    """The door as a panel: its centre (x, z) in the local frame and its size (m) as built, before the placement."""
    x0, y0, x1, y1 = DOOR
    a, b = to_local(x0, y0), to_local(x1, y1)
    return ((a[0] + b[0]) / 2, (a[2] + b[2]) / 2), (SCALE * abs(a[0] - b[0]), SCALE * abs(a[2] - b[2]))


def panel_at(name):
    """Where a traced control or lamp sits on the panel (at_m: u to the right as you face it, v up, from the door's centre, in the game's size). A control read as a pair of buttons is the point between them."""
    (cx, cz), _ = panel_frame()
    loc = control_local()
    pts = [loc[k] for k in loc if k == name or k in (name + '.on', name + '.off')]
    x = sum(p[0] for p in pts) / len(pts)
    z = sum(p[1] for p in pts) / len(pts)
    return (round(-SCALE * (x - cx), 4), round(SCALE * (z - cz), 4))


def parts(kind=None):
    return [p for p in PARTS if kind is None or p['kind'] == kind]
