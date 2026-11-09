"""The SP-2M2 rack's front (the computer 9S456's blocks, behind the senior operator's seat), traced in 3D from two of the
book's photographs (no bpy: the Blender builder, the tests and the cabin layout read it). Generated from the measuring
run's files (docs/OSA_INTERIOR_PIPELINE.md, "The first face"); edit by hand only what the run cannot see.

How it was traced (docs/MODELLING_FROM_PHOTOS.md, with the interior pipeline's tools, tools/recon/interior.py):

1. The photos: `wwp_gecko` p. 93 photo b (g093-b), the ex-NVA 9A33BM3-1's SP-2M2 square on from the aisle, and p. 92
   photo a (g092-a), the same rack from the aisle's rear end, about 30 degrees off. The scans' halftone screen is taken
   out by notch filtering before anything is measured (`descreen_notch`).
2. Flattened: the face's outer edges, each a line fitted to the plate/recess step at 9-37 places, give the homography to
   a true-angle front (2 px/mm). The aspect is checked by the round bezels: five clean ones come out round to 0.5 %
   (x/y 1.005 +- 0.004). A small correcting homography (shear -0.25 %, aspect 1.0063, a slight keystone) makes the
   five rows' tops equally spaced and the bezels round together; the rows' pitch still grows 240.5 -> 246.7 mm from top
   to bottom, which is either the camera's or the rack's and is left as about +-3 mm of uncertainty in y.
3. The blocks' edges: each side found on its brightness profile across the boundary (plate -> lip line -> light frame
   rail -> shadow band -> next plate: the first fall out of the plate) and then fitted along its whole length (sub-pixel
   step fit, one-sided window), the corners where the side lines meet. Side rms 0.2-0.4 mm; the sides only the profile
   found (no clean step along them) are good to about 1 mm. Where a bottom edge caught the rail below, the block's
   height is the median of the rest (230.2 mm); the right column's right edges run off the print and stop at the face's
   width.
4. The parts: read off the flattened front in 3 px/mm tiles block by block (507 items: screws, locks, keys, pointer
   knobs, fuses, lamps, jacks, toggles, strips, nameplates, the OR-31M's dial bezels, meters, the OR-11M's socket field,
   the raised tables on their feet, handles, rails, the OK52-15M modules' lever locks, plugs), and every round part
   re-centred on its own blob in the image (sub-pixel centroid, diameter from its area; 311 parts, median shift 0.6 mm).
   Measured sizes: screw heads 6.2 mm, lamps 10.8, jacks 20.1, locks 27.0, the bezels 62.6 outside with a 39 mm window.
   Where the overlap tests caught two parts in one place, the photo was looked at again: a jack's chain read as a key
   was dropped, four blobs that took in a neighbour or a chain were given their parts' sizes, and R3's lock, running off
   the print, took its right edge out to the face's width.
5. Heights out of the plate: g092-a matched to the flattened front by ALIKED + LightGlue (3697 matches), the plate's plane
   by MAGSAC (1044 inliers, 2.0 px), g092-a's camera from that plane (focal 3497 px, rms 3.3 px after a one-view
   calibration), g093-b's camera over the face's middle 1.1 m out; then every match's height from its parallax between
   the two views. The plate-level matches scatter +-3 mm, so heights are good to about 3 mm where a part carries enough
   matches: the tables stand 16-19 mm out, a pointer knob 19, the meters' faces 4, the socket field 6, the locks 4-10, the
   bezels' rims up to 7, the label strips and the rails 0-1. Parts the matches miss take SAM3D's heights (`Z`, marked).

Scale: the one length not measured here. The face is taken as 1890 mm wide, SŁ-2M's width, which the cabin layout and
the plan's scale (mon_2112_80 Fig. 3) already rest on: SŁ-2M's three columns in SAM Simulator's pictures at 1 mm a
pixel (`samsim_panels`, whole blocks across, so the widths hold where the heights don't). At that width SP-2M2's three
columns come out 0.60-0.61 m each, the same block standard; good to about +-2 % (+-40 mm on the width). No part of known
size could be read to check it (the meters' types are illegible). Everything else is in proportion to that width, so a
better yardstick later rescales the whole trace by one factor.

Frame: x mm to the right as the operator faces the rack, y mm down, from the face's top left (the left column's left
edge, the top row's top). `Z` is out of the plate, mm. `to_m` turns (x, y) into (a, b) metres on the cabin panel: a
right, b up, from the face's centre.
"""
import math

W_MM, H_MM = 1890.0, 1240.0         # the face: the five rows of blocks and the bottom rail under them
SX = SY = 0.001

# Out of the plate (+) or behind it (-), mm. (m) measured from the two photos' parallax (section 5); (c) SAM3D's choice,
# from what the part is and the photos' shading, about +-5 mm.
Z = {
    'frame': -8.0,           # (c) the frame rails and recesses between the blocks: the shadow band reads 5 mm wide
    'block_plate': 3.0,      # (c) a block's front plate thickness (its face is Z = 0)
    'screw': 2.0,            # (c)
    'lock': 6.0,             # (m) 4-10 on three locks
    'key': 8.0,              # (c)
    'knob': 19.0,            # (m) one pointer knob, its top
    'knob_skirt': 7.0,       # (c)
    'cyl_knob': 20.0,        # (c)
    'knob_round': 15.0,      # (c)
    'fuse_base': 4.0,        # (m) one fuse holder's base
    'fuse_cap': 16.0,        # (c)
    'lamp': 10.0,            # (c) the lamp holder's lens
    'jack': 6.0,             # (c)
    'toggle_nut': 4.0,       # (c)
    'toggle_bat': 18.0,      # (c)
    'strip': 1.0,            # (m) 0-1 on three strips
    'plate': 0.8,            # (c) nameplates
    'bezel': 5.0,            # (m) 0-7 on four bezels
    'bezel_glass': -3.0,     # (c)
    'meter': 4.0,            # (m) the two OR-11M meters' scales (the matches on their white faces)
    'meter_rim': 15.0,       # (c) their housings' rims: g092-a shows the housings' sides, about 10-15 mm
    'meter_hood': 12.0,      # (c) the light shade over the OR-11M meters' tops
    'sockets': 6.0,          # (m) the OR-11M's socket field
    'socket_hole': -12.0,    # (c)
    'table': 18.0,           # (m) 16-19 on four tables
    'table_t': 3.0,          # (c) the table plate's thickness
    'handle': 30.0,          # (c)
    'bar': 2.0,              # (m) 1 on one rail
    'lever_lock': 8.0,       # (c)
    'plug': 25.0,            # (c)
    'cable': 15.0,           # (c)
    'button': 10.0,          # (c)
    'socket_round': 12.0,    # (c)
    'divider': 3.0,          # (c)
    'frame_connector': 30.0, # (c)
    'stud': 6.0,             # (c)
}


# The blocks: their outer edges, face mm (x0, y0, x1, y1), and how each side was found (m: measured by its edge
# profile, about 0.3 mm; p: the profile only, about 1 mm; c: set by the row height or the face width, see above).
BLOCKS = {
    'L0':   ((    0.3,     1.3,   598.6,   233.7), 'mmpm'),
    'M0':   ((  646.2,    -1.3,  1250.9,   228.9), 'mmmc'),
    'R0_0': (( 1298.2,     1.2,  1435.4,   227.3), 'mmmm'),
    'R0_1': (( 1449.8,     0.7,  1587.3,   227.7), 'mmmm'),
    'R0_2': (( 1601.3,     0.8,  1738.2,   228.7), 'pmmm'),
    'R0_3': (( 1752.0,     3.9,  1890.0,   230.0), 'pmcm'),
    'L1':   ((    0.2,   241.4,   600.8,   471.6), 'pmmp'),
    'M1':   ((  639.9,   240.4,  1252.8,   470.6), 'mmmc'),
    'R1':   (( 1295.3,   244.8,  1890.0,   472.1), 'mmcp'),
    'L2':   ((   -1.0,   482.6,   594.5,   712.8), 'mmmc'),
    'M2':   ((  639.6,   483.9,  1252.4,   714.1), 'ppmc'),
    'R2':   (( 1282.5,   486.6,  1890.0,   715.8), 'mmcp'),
    'L3':   ((    0.5,   727.2,   602.1,   958.1), 'pmmp'),
    'M3':   ((  645.3,   729.1,  1253.3,   959.3), 'mmmc'),
    'R3':   (( 1296.0,   729.8,  1890.0,   960.7), 'mpwp'),
    'L4':   ((  146.8,   973.4,   602.6,  1201.6), 'pmpm'),
    'M4':   ((  641.1,   976.4,  1254.5,  1206.2), 'mmmp'),
    'R4':   (( 1295.1,   975.1,  1750.8,  1199.4), 'mmmp'),
}

# Every part: (block, kind, geometry). Round parts: centre and diameter (fitted, else read); rectangles: (x0, y0, x1, y1).
# A height measured from the two photos (75th percentile of the matches on it, mm) is the last field, else None.
PARTS = [
    ('L0', 'lock', {'c': (24.7, 111), 'd': 28.4}, None),
    ('L0', 'lock', {'c': (568.2, 111.2), 'd': 23.5}, None),
    ('L0', 'key', {'c': (576, 133.6)}, None),
    ('L0', 'knob', {'c': (85, 92), 'size': (36.7, 25), 'angle': -20, 'label': 'КОНТРОЛЬ НАПРЯЖЕНИЯ'}, None),
    ('L0', 'fuse', {'c': (149.6, 112), 'size': (26.7, 38.3)}, None),
    ('L0', 'fuse', {'c': (186.6, 112), 'size': (26.7, 38.3)}, None),
    ('L0', 'fuse', {'c': (224.3, 112), 'size': (26.7, 38.3)}, None),
    ('L0', 'lamp', {'c': (148.6, 165.6), 'd': 15.1, 'colour': 'orange'}, None),
    ('L0', 'lamp', {'c': (186.1, 165.7), 'd': 15.2, 'colour': 'orange'}, None),
    ('L0', 'strip', {'rect': (504.3, 57, 516, 122)}, None),
    ('L0', 'screw', {'c': (510.3, 53.6)}, None),
    ('L0', 'screw', {'c': (510.3, 127)}, None),
    ('L0', 'plate', {'rect': (529.3, 10.3, 566, 20.3)}, None),
    ('L0', 'plate', {'rect': (567.6, 10.3, 589.3, 20.3)}, None),
    ('L0', 'screw', {'c': (77.7, 23.8), 'd': 6.8}, None),
    ('L0', 'screw', {'c': (187.2, 23.6), 'd': 6.8}, None),
    ('L0', 'screw', {'c': (297.9, 22.7), 'd': 6.8}, None),
    ('L0', 'screw', {'c': (409.1, 21.8), 'd': 6.7}, None),
    ('L0', 'screw', {'c': (520.8, 20.3), 'd': 5.7}, None),
    ('L0', 'screw', {'c': (357.7, 38.1), 'd': 6.4}, None),
    ('L0', 'screw', {'c': (422.6, 37.2), 'd': 5.8}, None),
    ('L0', 'screw', {'c': (357.1, 105.2), 'd': 6.3}, None),
    ('L0', 'screw', {'c': (423.1, 105.8), 'd': 4.4}, None),
    ('L0', 'screw', {'c': (75.8, 204.2), 'd': 6.7}, None),
    ('L0', 'screw', {'c': (186.9, 203.8), 'd': 5.6}, None),
    ('L0', 'screw', {'c': (297.7, 203.7), 'd': 5.7}, None),
    ('L0', 'screw', {'c': (409.4, 203.1), 'd': 3.7}, None),
    ('L0', 'screw', {'c': (519.9, 202.2), 'd': 3.1}, None),
    ('M0', 'bezel', {'c': (763.5, 44.4), 'plug': (723.5, 44.4), 'studs': ((740.2, 29.4), (786.9, 54.4))}, 0.0),
    ('M0', 'bezel', {'c': (893.5, 44.4), 'plug': (853.5, 44.4), 'studs': ((871.9, 29.4), (918.5, 54.4))}, None),
    ('M0', 'bezel', {'c': (1026.9, 44.4), 'plug': (985.2, 44.4), 'studs': ((1001.9, 30), (1049.5, 55))}, None),
    ('M0', 'lock', {'c': (675.7, 110.3), 'd': 22.4}, None),
    ('M0', 'lock', {'c': (1224, 109), 'd': 27.4}, None),
    ('M0', 'toggle', {'c': (1223.5, 157.7)}, None),
    ('M0', 'fuse', {'c': (676.9, 164.4), 'size': (23.3, 38.3)}, None),
    ('M0', 'lamp', {'c': (704.7, 197.8), 'd': 10.4, 'colour': 'red'}, None),
    ('M0', 'lamp', {'c': (719.5, 197.1), 'd': 9.9, 'colour': 'green'}, None),
    ('M0', 'strip', {'rect': (725.2, 191, 788.5, 204.4)}, 1.0),
    ('M0', 'lamp', {'c': (794.4, 197.9), 'd': 9.8, 'colour': 'green'}, None),
    ('M0', 'jack', {'c': (826.1, 196.8), 'd': 19.7, 'colour': 'black'}, None),
    ('M0', 'jack', {'c': (857.2, 197.2), 'd': 20.2, 'colour': 'orange'}, None),
    ('M0', 'jack', {'c': (898.5, 197.1), 'd': 20, 'colour': 'black'}, None),
    ('M0', 'jack', {'c': (929.7, 197.3), 'd': 20.5, 'colour': 'orange'}, None),
    ('M0', 'jack', {'c': (971, 197), 'd': 20.4, 'colour': 'black'}, None),
    ('M0', 'jack', {'c': (1001.8, 197.5), 'd': 20.2, 'colour': 'orange'}, None),
    ('M0', 'lamp', {'c': (1104.5, 197.9), 'd': 10.2, 'colour': 'green'}, None),
    ('M0', 'strip', {'rect': (1111.9, 192.7, 1173.5, 204.4)}, None),
    ('M0', 'lamp', {'c': (1179.6, 198.5), 'd': 10.7, 'colour': 'green'}, None),
    ('M0', 'lamp', {'c': (1194.1, 198.6), 'd': 11.7, 'colour': 'red'}, None),
    ('M0', 'plate', {'rect': (1173.5, 4.4, 1218.5, 14.4)}, None),
    ('M0', 'plate', {'rect': (1220.2, 4.4, 1241.9, 14.4)}, None),
    ('M0', 'screw', {'c': (675.6, 23), 'd': 2.9}, None),
    ('M0', 'screw', {'c': (1221.8, 24.2), 'd': 6.8}, None),
    ('M0', 'screw', {'c': (823.5, 72.7)}, None),
    ('M0', 'screw', {'c': (954, 72.7), 'd': 6.6}, None),
    ('M0', 'screw', {'c': (1083.1, 72.6), 'd': 6.1}, None),
    ('M0', 'screw', {'c': (1045.5, 173.1), 'd': 4.7}, None),
    ('M0', 'screw', {'c': (1221.8, 192.8), 'd': 3.5}, None),
    ('M0', 'screw', {'c': (675.2, 192.7)}, None),
    ('L1', 'bezel', {'c': (120.2, 290.4), 'plug': (84.2, 290.4), 'studs': ((95.9, 276.1), (144.2, 302.1))}, -0.3),
    ('L1', 'bezel', {'c': (376.9, 288.7), 'plug': (337.5, 288.7), 'studs': ((351.5, 275.4), (399.2, 301.4))}, None),
    ('L1', 'cyl_knob', {'c': (212.5, 273.6), 'd': 31.8}, None),
    ('L1', 'cyl_knob', {'c': (269.6, 273.6), 'd': 31.5}, None),
    ('L1', 'cyl_knob', {'c': (211.9, 315.4), 'd': 31.5}, None),
    ('L1', 'cyl_knob', {'c': (269.6, 314.9), 'd': 31.1}, None),
    ('L1', 'lamp', {'c': (215.6, 342.8), 'd': 11.2, 'colour': 'orange'}, None),
    ('L1', 'lamp', {'c': (272.4, 342.6), 'd': 11.3, 'colour': 'orange'}, None),
    ('L1', 'lamp', {'c': (216.1, 357.6), 'd': 11.1, 'colour': 'orange'}, None),
    ('L1', 'lamp', {'c': (272.7, 357.5), 'd': 10.7, 'colour': 'orange'}, None),
    ('L1', 'toggle', {'c': (481.5, 290.4)}, None),
    ('L1', 'toggle', {'c': (481.5, 342.1)}, None),
    ('L1', 'lock', {'c': (25.4, 354.3), 'd': 28}, None),
    ('L1', 'lock', {'c': (570.6, 356), 'd': 20.8}, None),
    ('L1', 'jack', {'c': (25.4, 399), 'd': 18, 'colour': 'black'}, None),
    ('L1', 'jack', {'c': (26.3, 417.2), 'd': 18, 'colour': 'orange'}, None),
    ('L1', 'jack', {'c': (571.8, 377.2), 'd': 17.5, 'colour': 'black'}, None),
    ('L1', 'jack', {'c': (570.2, 396.2), 'd': 20.1, 'colour': 'red'}, None),
    ('L1', 'fuse', {'c': (572.5, 418.1), 'size': (18.3, 20)}, 3.8),
    ('L1', 'plate', {'rect': (524.2, 248.7, 557.5, 258.7)}, None),
    ('L1', 'plate', {'rect': (559.2, 248.7, 590.9, 258.7)}, None),
    ('L1', 'screw', {'c': (27.6, 268.1), 'd': 6.7}, None),
    ('L1', 'screw', {'c': (570.5, 266.4), 'd': 5.6}, None),
    ('L1', 'screw', {'c': (182.4, 317), 'd': 3.6}, None),
    ('L1', 'screw', {'c': (307.5, 316.4)}, None),
    ('L1', 'screw', {'c': (435.9, 317.4), 'd': 5.8}, None),
    ('L1', 'screw', {'c': (27.5, 437), 'd': 6.5}, None),
    ('L1', 'screw', {'c': (181.8, 436.2), 'd': 5}, None),
    ('L1', 'screw', {'c': (306.9, 436.4)}, None),
    ('L1', 'screw', {'c': (435.9, 437.1), 'd': 6.7}, None),
    ('L1', 'screw', {'c': (570.8, 435.4), 'd': 6.8}, None),
    ('M1', 'meter', {'rect': (708.9, 281.1, 810.6, 382.7), 'inner': (717.2, 254.4, 802.2, 281.1)}, 4.3),
    ('M1', 'meter', {'rect': (1093.9, 281.1, 1197.2, 382.7), 'inner': (1103.9, 254.4, 1187.2, 281.1)}, 4.3),
    ('M1', 'sockets', {'rect': (850.6, 258.4, 1060.6, 322.7), 'holes': ((862.2, 269.4), (885.6, 269.4), (908.9, 269.4), (932.2, 269.4), (955.6, 269.4), (978.9, 269.4), (1003.9, 269.4), (1027.2, 269.4), (1048.9, 269.4), (873.9, 289.4), (897.2, 289.4), (920.6, 289.4), (943.9, 289.4), (967.2, 289.4), (990.6, 289.4), (1013.9, 289.4), (1037.2, 289.4), (862.2, 310.4), (885.6, 310.4), (908.9, 310.4), (932.2, 310.4), (955.6, 310.4), (978.9, 310.4), (1003.9, 310.4), (1027.2, 310.4), (1048.9, 310.4)), 'hole_d': 20.7}, 5.8),
    ('M1', 'knob', {'c': (857.2, 374.4), 'size': (23.3, 36.7), 'angle': 25, 'label': 'РУ (РУ1)'}, None),
    ('M1', 'knob', {'c': (950.6, 357.7), 'size': (36.7, 23.3), 'angle': -30, 'label': 'КОНТР. СРЗ'}, None),
    ('M1', 'knob', {'c': (760.6, 411.1), 'size': (38.3, 21.7), 'angle': 15, 'label': 'ПРЕДЕЛ ИЗМ. I'}, None),
    ('M1', 'knob', {'c': (1148.9, 411.1), 'size': (38.3, 21.7), 'angle': 15, 'label': 'ПРЕДЕЛ ИЗМ. II'}, 18.8),
    ('M1', 'lock', {'c': (676.8, 358.7), 'd': 15.7}, None),
    ('M1', 'lock', {'c': (1226.4, 355.6), 'd': 27.6}, 6.2),
    ('M1', 'key', {'c': (1223.9, 374.4)}, None),
    ('M1', 'toggle', {'c': (1222.2, 331.1)}, None),
    ('M1', 'toggle', {'c': (1079.9, 351.1)}, None),
    ('M1', 'toggle', {'c': (1027.2, 412.7)}, None),
    ('M1', 'jack', {'c': (679.7, 290.3), 'd': 9.9, 'colour': 'black'}, None),
    ('M1', 'lamp', {'c': (722.8, 408.8), 'd': 7.8, 'colour': 'red'}, None),
    ('M1', 'lamp', {'c': (1106.6, 407.5), 'd': 7.7, 'colour': 'red'}, None),
    ('M1', 'lamp', {'c': (816.4, 421.4), 'd': 9.9, 'colour': 'brown'}, None),
    ('M1', 'lamp', {'c': (1087.8, 420.7), 'd': 11.1, 'colour': 'brown'}, None),
    ('M1', 'lamp', {'c': (950, 336), 'd': 7.5, 'colour': 'red'}, None),
    ('M1', 'plate', {'rect': (1180.6, 250.4, 1215.6, 259.4)}, None),
    ('M1', 'plate', {'rect': (1217.2, 250.4, 1240.6, 259.4)}, None),
    ('M1', 'screw', {'c': (729, 260.5), 'd': 6.5}, None),
    ('M1', 'screw', {'c': (789.9, 260.2), 'd': 6}, None),
    ('M1', 'screw', {'c': (1114.5, 258.3), 'd': 6.8}, None),
    ('M1', 'screw', {'c': (1180.5, 258.1), 'd': 6.7}, None),
    ('M1', 'screw', {'c': (711.2, 268.6), 'd': 6.1}, None),
    ('M1', 'screw', {'c': (713.1, 393.9), 'd': 4.8}, None),
    ('M1', 'screw', {'c': (711.4, 419.2), 'd': 3.6}, None),
    ('M1', 'screw', {'c': (1188.4, 393.7), 'd': 5.6}, None),
    ('M1', 'screw', {'c': (1185, 417.1), 'd': 5.7}, None),
    ('M1', 'screw', {'c': (676.8, 440.4), 'd': 5.8}, None),
    ('M1', 'screw', {'c': (734.8, 448.8), 'd': 4.4}, None),
    ('M1', 'screw', {'c': (842.8, 446.9), 'd': 4.7}, None),
    ('M1', 'screw', {'c': (952.1, 447.7), 'd': 5.6}, None),
    ('M1', 'screw', {'c': (1057.9, 448.1), 'd': 5}, None),
    ('M1', 'screw', {'c': (1166, 448.2), 'd': 5.2}, None),
    ('M1', 'screw', {'c': (1225.7, 438.5), 'd': 6.8}, None),
    ('L2', 'strip', {'rect': (90.7, 521.6, 106.3, 644.9)}, None),
    ('L2', 'handle', {'rect': (141.3, 511.6, 153, 581.6)}, None),
    ('L2', 'handle', {'rect': (185.7, 511.6, 197, 581.6)}, None),
    ('L2', 'jack', {'c': (167.2, 519.4), 'd': 20.5, 'colour': 'black'}, None),
    ('L2', 'jack', {'c': (177.9, 557.5), 'd': 21.7, 'colour': 'black'}, None),
    ('L2', 'table', {'rect': (206.3, 511.6, 383, 673.3)}, 16.6),
    ('L2', 'foot', {'c': (230.1, 681.1), 'd': 15.5}, None),
    ('L2', 'foot', {'c': (360.5, 681.7), 'd': 14.7}, None),
    ('L2', 'stud', {'c': (293.2, 507.6), 'd': 7.2}, None),
    ('L2', 'handle', {'rect': (403, 511.6, 413, 548.3)}, None),
    ('L2', 'handle', {'rect': (439, 511.6, 448, 548.3)}, None),
    ('L2', 'jack', {'c': (425.8, 519.2), 'd': 18.6, 'colour': 'black'}, None),
    ('L2', 'strip', {'rect': (491.3, 519.9, 508, 643.3)}, -1.2),
    ('L2', 'lock', {'c': (24.2, 597.2), 'd': 28.1}, None),
    ('L2', 'lock', {'c': (574.7, 595.7), 'd': 26.8}, 4.3),
    ('L2', 'plate', {'rect': (521.3, 492.6, 556.3, 502.6)}, None),
    ('L2', 'plate', {'rect': (558, 492.6, 589.7, 502.6)}, None),
    ('L2', 'screw', {'c': (86.1, 500), 'd': 6.8}, None),
    ('L2', 'screw', {'c': (191.3, 499.9)}, None),
    ('L2', 'screw', {'c': (296.8, 501.2), 'd': 5.6}, None),
    ('L2', 'screw', {'c': (404.4, 499.8), 'd': 5.7}, None),
    ('L2', 'screw', {'c': (511.3, 499.8), 'd': 6.2}, None),
    ('L2', 'screw', {'c': (61.2, 512.5), 'd': 6.8}, None),
    ('L2', 'screw', {'c': (536.5, 511.6), 'd': 6.7}, None),
    ('L2', 'screw', {'c': (61.2, 638.4), 'd': 6.3}, None),
    ('L2', 'screw', {'c': (536.7, 638.4), 'd': 4.5}, None),
    ('L2', 'screw', {'c': (26.4, 683.4), 'd': 6.8}, None),
    ('L2', 'screw', {'c': (61.2, 683.4), 'd': 6.7}, None),
    ('L2', 'screw', {'c': (86.1, 683.3), 'd': 6.8}, None),
    ('L2', 'screw', {'c': (190.7, 683.6), 'd': 4.6}, None),
    ('L2', 'screw', {'c': (297.6, 683.5), 'd': 5.8}, None),
    ('L2', 'screw', {'c': (404.3, 684), 'd': 6.2}, None),
    ('L2', 'screw', {'c': (511.4, 683.6), 'd': 6.4}, None),
    ('L2', 'screw', {'c': (536.2, 684.3), 'd': 6.5}, None),
    ('M2', 'bezel', {'c': (748.6, 537.9), 'plug': (786.9, 532.9), 'studs': ((724.3, 520.6), (772.9, 546.2))}, 6.9),
    ('M2', 'bezel', {'c': (876.9, 532.9), 'plug': (915.3, 532.9), 'studs': ((853.6, 520.6), (901.9, 547.2))}, None),
    ('M2', 'bezel', {'c': (1005.3, 532.9), 'plug': (1043.6, 531.2), 'studs': ((984.3, 520.6), (1033.6, 546.2))}, 2.8),
    ('M2', 'bezel', {'c': (1133.6, 532.9), 'plug': (1171.9, 532.9), 'studs': ((1113.6, 520.6), (1161.9, 547.2))}, None),
    ('M2', 'lock', {'c': (673.3, 601.6), 'd': 25.4}, None),
    ('M2', 'lock', {'c': (1225.1, 601.2), 'd': 22}, None),
    ('M2', 'jack', {'c': (1232, 620.3), 'd': 19, 'colour': 'black'}, None),
    ('M2', 'fuse', {'c': (1227.6, 649.6), 'size': (15, 20)}, None),
    ('M2', 'lamp', {'c': (703.3, 689.4), 'd': 10.1, 'colour': 'red'}, None),
    ('M2', 'lamp', {'c': (717.4, 688.6), 'd': 9.1, 'colour': 'green'}, None),
    ('M2', 'strip', {'rect': (713.6, 682.9, 788.6, 694.6)}, None),
    ('M2', 'lamp', {'c': (794.1, 689.2), 'd': 9, 'colour': 'green'}, None),
    ('M2', 'bar', {'rect': (803.6, 684.6, 1100.3, 697.9)}, 1.1),
    ('M2', 'jack', {'c': (825.6, 686.8), 'd': 15.3, 'colour': 'black'}, None),
    ('M2', 'jack', {'c': (856.2, 687.5), 'd': 14.9, 'colour': 'orange'}, None),
    ('M2', 'jack', {'c': (897.6, 686.9), 'd': 15.3, 'colour': 'black'}, None),
    ('M2', 'jack', {'c': (928.9, 684.9), 'd': 13.8, 'colour': 'orange'}, None),
    ('M2', 'jack', {'c': (968.7, 687.4), 'd': 15.9, 'colour': 'black'}, None),
    ('M2', 'jack', {'c': (1000.8, 687.4), 'd': 17.1, 'colour': 'orange'}, None),
    ('M2', 'jack', {'c': (1041.2, 687.1), 'd': 16.9, 'colour': 'black'}, None),
    ('M2', 'jack', {'c': (1073, 687.1), 'd': 17.1, 'colour': 'orange'}, None),
    ('M2', 'lamp', {'c': (1105.4, 688.7), 'd': 9.8, 'colour': 'green'}, None),
    ('M2', 'strip', {'rect': (1110.3, 683.9, 1175.3, 695.2)}, 0.1),
    ('M2', 'lamp', {'c': (1182.2, 688.9), 'd': 9.4, 'colour': 'green'}, None),
    ('M2', 'lamp', {'c': (1195.3, 689.9), 'd': 11, 'colour': 'red'}, None),
    ('M2', 'plate', {'rect': (1175.3, 493.9, 1215.3, 502.9)}, None),
    ('M2', 'plate', {'rect': (1216.9, 493.9, 1243.6, 502.9)}, None),
    ('M2', 'screw', {'c': (673.7, 509.8), 'd': 6.6}, None),
    ('M2', 'screw', {'c': (1221.8, 509.5), 'd': 6.8}, None),
    ('M2', 'screw', {'c': (816.3, 561.2), 'd': 6.8}, None),
    ('M2', 'screw', {'c': (947.1, 560.7), 'd': 4.4}, None),
    ('M2', 'screw', {'c': (1076.9, 561.9), 'd': 4.3}, None),
    ('M2', 'screw', {'c': (851.2, 664.1), 'd': 6.1}, None),
    ('M2', 'screw', {'c': (1047.3, 661.3), 'd': 4.8}, None),
    ('M2', 'screw', {'c': (673.8, 682.7), 'd': 6.5}, None),
    ('M2', 'screw', {'c': (1223.5, 681.9), 'd': 6.7}, None),
    ('L3', 'strip', {'rect': (90.5, 766.2, 107.2, 891.2)}, None),
    ('L3', 'handle', {'rect': (142.8, 758.5, 154.5, 827.9)}, None),
    ('L3', 'handle', {'rect': (187.2, 758.5, 197.8, 827.9)}, None),
    ('L3', 'jack', {'c': (167.9, 767), 'd': 20.2, 'colour': 'black'}, None),
    ('L3', 'jack', {'c': (178.7, 804.6), 'd': 22.1, 'colour': 'black'}, None),
    ('L3', 'table', {'rect': (209.5, 757.9, 386.2, 921.2)}, 16.1),
    ('L3', 'foot', {'c': (230.5, 928.1), 'd': 15.8}, None),
    ('L3', 'foot', {'c': (361.6, 928.5), 'd': 14.2}, None),
    ('L3', 'stud', {'c': (296.3, 754.6), 'd': 8.2}, None),
    ('L3', 'handle', {'rect': (402.8, 758.5, 412.8, 796.2)}, None),
    ('L3', 'handle', {'rect': (439.5, 758.5, 448.5, 796.2)}, None),
    ('L3', 'jack', {'c': (426.8, 767.1), 'd': 19.3, 'colour': 'black'}, None),
    ('L3', 'strip', {'rect': (492.8, 766.2, 507.8, 892.9)}, None),
    ('L3', 'lock', {'c': (26.1, 843.7), 'd': 28}, None),
    ('L3', 'lock', {'c': (571, 847.2), 'd': 24}, None),
    ('L3', 'key', {'c': (567.8, 864.5)}, None),
    ('L3', 'plate', {'rect': (521.2, 738.5, 557.8, 747.9)}, None),
    ('L3', 'plate', {'rect': (559.5, 738.5, 589.5, 747.9)}, None),
    ('L3', 'screw', {'c': (87.5, 744.5), 'd': 6.4}, None),
    ('L3', 'screw', {'c': (194.8, 745.6), 'd': 1.6}, None),
    ('L3', 'screw', {'c': (298.3, 744.5), 'd': 6.3}, None),
    ('L3', 'screw', {'c': (405, 745.5), 'd': 5.6}, None),
    ('L3', 'screw', {'c': (512.8, 745.9), 'd': 5.3}, None),
    ('L3', 'screw', {'c': (62.5, 757.1), 'd': 6.2}, None),
    ('L3', 'screw', {'c': (537.3, 757.3), 'd': 5.3}, None),
    ('L3', 'screw', {'c': (62.7, 884.4), 'd': 6.8}, None),
    ('L3', 'screw', {'c': (537.8, 884.6), 'd': 6.2}, None),
    ('L3', 'screw', {'c': (29.4, 929.5), 'd': 6.8}, None),
    ('L3', 'screw', {'c': (62.7, 929.5), 'd': 6.8}, None),
    ('L3', 'screw', {'c': (87.8, 929.5), 'd': 6.8}, None),
    ('L3', 'screw', {'c': (193.7, 929.6), 'd': 6.8}, None),
    ('L3', 'screw', {'c': (298, 929.9), 'd': 5.8}, None),
    ('L3', 'screw', {'c': (406, 930.5), 'd': 3.5}, None),
    ('L3', 'screw', {'c': (512.9, 929.3), 'd': 5.5}, None),
    ('L3', 'screw', {'c': (538.6, 930.6), 'd': 4.2}, None),
    ('L3', 'screw', {'c': (571, 929.1), 'd': 4.6}, None),
    ('M3', 'strip', {'rect': (738.6, 768.1, 755.3, 894.8)}, None),
    ('M3', 'handle', {'rect': (799.3, 759.8, 806, 831.4)}, None),
    ('M3', 'handle', {'rect': (841, 759.8, 849.3, 831.4)}, None),
    ('M3', 'jack', {'c': (819.4, 768.4), 'd': 20.6, 'colour': 'black'}, None),
    ('M3', 'jack', {'c': (829.1, 806.3), 'd': 19, 'colour': 'black'}, None),
    ('M3', 'table', {'rect': (861, 759.8, 1039.3, 926.4)}, 19.1),
    ('M3', 'foot', {'c': (881.9, 931.7), 'd': 13}, None),
    ('M3', 'foot', {'c': (1012.7, 932.3), 'd': 15.1}, None),
    ('M3', 'stud', {'c': (949.6, 758.9), 'd': 7.2}, None),
    ('M3', 'handle', {'rect': (1061, 759.8, 1069.3, 799.8)}, None),
    ('M3', 'handle', {'rect': (1096, 759.8, 1104.3, 799.8)}, None),
    ('M3', 'jack', {'c': (1080, 769), 'd': 21.4, 'colour': 'black'}, None),
    ('M3', 'strip', {'rect': (1144.3, 768.1, 1161, 894.8)}, None),
    ('M3', 'lock', {'c': (673.5, 846.9), 'd': 22.9}, None),
    ('M3', 'lock', {'c': (1225.8, 846.9), 'd': 27.1}, None),
    ('M3', 'plate', {'rect': (1172.6, 743.1, 1221, 753.1)}, None),
    ('M3', 'plate', {'rect': (1222.6, 743.1, 1244.3, 753.1)}, None),
    ('M3', 'screw', {'c': (733.7, 746.9), 'd': 6.1}, None),
    ('M3', 'screw', {'c': (841.6, 746.8), 'd': 5.2}, None),
    ('M3', 'screw', {'c': (948.8, 747.5), 'd': 6}, None),
    ('M3', 'screw', {'c': (1056.8, 746.5), 'd': 4}, None),
    ('M3', 'screw', {'c': (1164.4, 747.3), 'd': 5.8}, None),
    ('M3', 'screw', {'c': (708.8, 756.8), 'd': 5.4}, None),
    ('M3', 'screw', {'c': (1188.7, 757.1), 'd': 5.4}, None),
    ('M3', 'screw', {'c': (710.7, 886.8), 'd': 2.4}, None),
    ('M3', 'screw', {'c': (1189.3, 885.5), 'd': 6.8}, None),
    ('M3', 'screw', {'c': (675.4, 931.1), 'd': 4.9}, None),
    ('M3', 'screw', {'c': (708.6, 931.6), 'd': 3.4}, None),
    ('M3', 'screw', {'c': (733.7, 931.7), 'd': 4.9}, None),
    ('M3', 'screw', {'c': (840.7, 931.9), 'd': 2.3}, None),
    ('M3', 'screw', {'c': (948.4, 932.2), 'd': 4.3}, None),
    ('M3', 'screw', {'c': (1056.2, 932.2), 'd': 6.5}, None),
    ('M3', 'screw', {'c': (1164.2, 932.4), 'd': 6.8}, None),
    ('M3', 'screw', {'c': (1189, 932.1), 'd': 6.6}, None),
    ('M3', 'screw', {'c': (1223.9, 932.5), 'd': 6.6}, None),
    ('M4', 'strip', {'rect': (741.1, 1016.4, 756.8, 1141.1)}, None),
    ('M4', 'handle', {'rect': (801.1, 1008.7, 807.8, 1080.4)}, None),
    ('M4', 'handle', {'rect': (843.4, 1008.7, 851.8, 1080.4)}, None),
    ('M4', 'jack', {'c': (821, 1017.5), 'd': 20.1, 'colour': 'black'}, None),
    ('M4', 'jack', {'c': (831.6, 1055.3), 'd': 20.2, 'colour': 'black'}, None),
    ('M4', 'table', {'rect': (861.8, 1007.1, 1040.1, 1175.4)}, 19.4),
    ('M4', 'foot', {'c': (889.8, 1180.9), 'd': 14.4}, None),
    ('M4', 'foot', {'c': (1013.5, 1180.1), 'd': 15.5}, None),
    ('M4', 'stud', {'c': (950.5, 1006), 'd': 7.7}, None),
    ('M4', 'handle', {'rect': (1061.8, 1007.7, 1070.1, 1049.7)}, None),
    ('M4', 'handle', {'rect': (1096.8, 1007.7, 1106.8, 1049.7)}, None),
    ('M4', 'jack', {'c': (1082.5, 1017.2), 'd': 21.3, 'colour': 'black'}, None),
    ('M4', 'strip', {'rect': (1145.1, 1016.4, 1162.4, 1141.1)}, None),
    ('M4', 'lock', {'c': (676.3, 1094.8), 'd': 26.3}, None),
    ('M4', 'lock', {'c': (1227.8, 1095.1), 'd': 27.8}, None),
    ('M4', 'key', {'c': (1226.8, 1117.1)}, None),
    ('M4', 'plate', {'rect': (1175.1, 989.7, 1221.8, 998.7)}, None),
    ('M4', 'plate', {'rect': (1223.4, 989.7, 1245.1, 998.7)}, None),
    ('M4', 'screw', {'c': (733.5, 995.3), 'd': 3.7}, None),
    ('M4', 'screw', {'c': (843.1, 994.5), 'd': 5.8}, None),
    ('M4', 'screw', {'c': (951.8, 994.4)}, None),
    ('M4', 'screw', {'c': (1058.8, 995), 'd': 5.8}, None),
    ('M4', 'screw', {'c': (1165.1, 994.2), 'd': 5.7}, None),
    ('M4', 'screw', {'c': (710.1, 1007.1)}, None),
    ('M4', 'screw', {'c': (1190.7, 1006.3), 'd': 6.2}, None),
    ('M4', 'screw', {'c': (710.3, 1133.9), 'd': 6.2}, None),
    ('M4', 'screw', {'c': (1190.3, 1133), 'd': 6.8}, None),
    ('M4', 'screw', {'c': (677, 1179.7), 'd': 5.6}, None),
    ('M4', 'screw', {'c': (710.3, 1178.8), 'd': 6.2}, None),
    ('M4', 'screw', {'c': (735, 1178.1), 'd': 5.4}, None),
    ('M4', 'screw', {'c': (843.3, 1178.4), 'd': 6}, None),
    ('M4', 'screw', {'c': (951.6, 1179.9), 'd': 6.6}, None),
    ('M4', 'screw', {'c': (1058.5, 1178.6), 'd': 6.8}, None),
    ('M4', 'screw', {'c': (1165.2, 1178.7), 'd': 6.8}, None),
    ('M4', 'screw', {'c': (1190.3, 1178.7), 'd': 6.8}, None),
    ('M4', 'screw', {'c': (1225.1, 1178.6), 'd': 6.8}, None),
    ('L4', 'lock', {'c': (173, 1044), 'd': 27.7}, None),
    ('L4', 'lock', {'c': (572.3, 1049), 'd': 25.7}, None),
    ('L4', 'key', {'c': (580.8, 1065.7)}, None),
    ('L4', 'jack', {'c': (174.5, 1099), 'd': 12.9, 'colour': 'red'}, None),
    ('L4', 'jack', {'c': (572.3, 1103.3), 'd': 11.2, 'colour': 'red'}, None),
    ('L4', 'plate', {'rect': (532.5, 984.7, 569.1, 994.1)}, None),
    ('L4', 'plate', {'rect': (572.5, 984.7, 592.5, 994.1)}, None),
    ('L4', 'screw', {'c': (214.1, 998), 'd': 6.8}, None),
    ('L4', 'screw', {'c': (320.2, 999.2), 'd': 5.9}, None),
    ('L4', 'screw', {'c': (426.8, 999.6), 'd': 5.4}, None),
    ('L4', 'screw', {'c': (533.4, 1000.8), 'd': 4.1}, None),
    ('L4', 'screw', {'c': (214.2, 1172.3), 'd': 6.8}, None),
    ('L4', 'screw', {'c': (320.8, 1172.3), 'd': 6.8}, None),
    ('L4', 'screw', {'c': (426.8, 1174), 'd': 6.8}, None),
    ('L4', 'screw', {'c': (533.9, 1174.7), 'd': 6.4}, None),
    ('R0_0', 'plate', {'rect': (1355.2, 8.9, 1404.5, 19.9)}, None),
    ('R0_0', 'plate', {'rect': (1407.2, 8.9, 1427.9, 19.9)}, None),
    ('R0_0', 'lamp', {'c': (1345.3, 83.9), 'd': 10.3, 'colour': 'orange'}, None),
    ('R0_0', 'lamp', {'c': (1370.2, 84.3), 'd': 10.8, 'colour': 'orange'}, None),
    ('R0_0', 'lamp', {'c': (1395.2, 84.7), 'd': 10.4, 'colour': 'orange'}, None),
    ('R0_0', 'screw', {'c': (1321.9, 24.7), 'd': 6.3}, None),
    ('R0_0', 'screw', {'c': (1417.6, 26.4), 'd': 5.3}, None),
    ('R0_0', 'screw', {'c': (1353.7, 38.1), 'd': 6.2}, None),
    ('R0_0', 'screw', {'c': (1384.7, 38.1), 'd': 5.4}, None),
    ('R0_0', 'screw', {'c': (1321.9, 87.3), 'd': 6}, None),
    ('R0_0', 'screw', {'c': (1417.4, 89.2), 'd': 4.6}, None),
    ('R0_0', 'screw', {'c': (1322.2, 197.9)}, None),
    ('R0_0', 'screw', {'c': (1418.9, 198.9)}, None),
    ('R0_0', 'lever_lock', {'c': (1375.7, 196.2), 'angle': 270}, None),
    ('R0_1', 'plate', {'rect': (1506.8, 8.4, 1556.1, 19.4)}, None),
    ('R0_1', 'plate', {'rect': (1558.8, 8.4, 1579.5, 19.4)}, None),
    ('R0_1', 'lamp', {'c': (1497, 83.8), 'd': 10.8, 'colour': 'orange'}, None),
    ('R0_1', 'lamp', {'c': (1522.3, 84.2), 'd': 11.5, 'colour': 'orange'}, None),
    ('R0_1', 'lamp', {'c': (1547.4, 84.4), 'd': 11.3, 'colour': 'orange'}, None),
    ('R0_1', 'screw', {'c': (1473.5, 24.3), 'd': 6.4}, None),
    ('R0_1', 'screw', {'c': (1569.2, 26), 'd': 5.4}, None),
    ('R0_1', 'screw', {'c': (1505, 37.6), 'd': 5.6}, None),
    ('R0_1', 'screw', {'c': (1536.3, 37.8), 'd': 5.7}, None),
    ('R0_1', 'screw', {'c': (1473, 86.7), 'd': 5.8}, None),
    ('R0_1', 'screw', {'c': (1569.6, 88.9), 'd': 5.6}, None),
    ('R0_1', 'screw', {'c': (1473.8, 197.4)}, None),
    ('R0_1', 'screw', {'c': (1570.5, 198.4)}, None),
    ('R0_1', 'lever_lock', {'c': (1527.3, 195.7), 'angle': 225}, None),
    ('R0_2', 'plate', {'rect': (1658.3, 8.5, 1707.6, 19.5)}, None),
    ('R0_2', 'plate', {'rect': (1710.3, 8.5, 1731, 19.5)}, None),
    ('R0_2', 'lamp', {'c': (1648.7, 84.6), 'd': 11.3, 'colour': 'orange'}, None),
    ('R0_2', 'lamp', {'c': (1673.6, 84.7), 'd': 11.7, 'colour': 'orange'}, None),
    ('R0_2', 'lamp', {'c': (1698.9, 84.7), 'd': 11.7, 'colour': 'orange'}, None),
    ('R0_2', 'screw', {'c': (1624.5, 24.6), 'd': 6.2}, None),
    ('R0_2', 'screw', {'c': (1721, 26), 'd': 6.1}, None),
    ('R0_2', 'screw', {'c': (1656.5, 37.7), 'd': 5.9}, None),
    ('R0_2', 'screw', {'c': (1687.9, 38), 'd': 5.7}, None),
    ('R0_2', 'screw', {'c': (1624.9, 87.3), 'd': 6.5}, None),
    ('R0_2', 'screw', {'c': (1720.9, 88.6), 'd': 5.2}, None),
    ('R0_2', 'screw', {'c': (1625.3, 196), 'd': 5}, None),
    ('R0_2', 'screw', {'c': (1721, 196.8), 'd': 4}, None),
    ('R0_2', 'lever_lock', {'c': (1678.8, 195.8), 'angle': 200}, None),
    ('R0_3', 'plate', {'rect': (1809, 11.6, 1858.3, 22.6)}, None),
    ('R0_3', 'plate', {'rect': (1861, 11.6, 1881.7, 22.6)}, None),
    ('R0_3', 'lamp', {'c': (1798.6, 87), 'd': 10.6, 'colour': 'orange'}, None),
    ('R0_3', 'lamp', {'c': (1823.4, 87), 'd': 10.2, 'colour': 'orange'}, None),
    ('R0_3', 'lamp', {'c': (1848.6, 86.9), 'd': 10.1, 'colour': 'orange'}, None),
    ('R0_3', 'screw', {'c': (1774.2, 27.9), 'd': 4.7}, None),
    ('R0_3', 'screw', {'c': (1872.7, 29.2)}, None),
    ('R0_3', 'screw', {'c': (1808, 41.2)}, None),
    ('R0_3', 'screw', {'c': (1839.7, 41.2)}, None),
    ('R0_3', 'screw', {'c': (1775.2, 90), 'd': 5.8}, None),
    ('R0_3', 'screw', {'c': (1872.7, 92.6)}, None),
    ('R0_3', 'screw', {'c': (1776, 200.6)}, None),
    ('R0_3', 'screw', {'c': (1872.7, 201.6)}, None),
    ('R0_3', 'lever_lock', {'c': (1829.5, 198.9), 'angle': 260}, None),
    ('R1', 'lock', {'c': (1326.9, 354.7), 'd': 26.3}, None),
    ('R1', 'lock', {'c': (1872, 357.9), 'd': 26}, None),
    ('R1', 'key', {'c': (1876, 375.5)}, None),
    ('R1', 'fuse', {'c': (1797.6, 297.1), 'size': (30, 40)}, None),
    ('R1', 'lamp', {'c': (1398.1, 350.2), 'd': 11.2, 'colour': 'orange'}, None),
    ('R1', 'lamp', {'c': (1456.1, 350.1), 'd': 11.3, 'colour': 'orange'}, None),
    ('R1', 'lamp', {'c': (1513.8, 350.2), 'd': 11, 'colour': 'orange'}, None),
    ('R1', 'lamp', {'c': (1664.4, 350.2), 'd': 12.2, 'colour': 'orange'}, None),
    ('R1', 'lamp', {'c': (1721.9, 350.7), 'd': 12.3, 'colour': 'orange'}, None),
    ('R1', 'lamp', {'c': (1778.8, 350.8), 'd': 12.4, 'colour': 'orange'}, None),
    ('R1', 'plate', {'rect': (1821, 252.8, 1859.3, 262.8)}, None),
    ('R1', 'plate', {'rect': (1869.3, 252.8, 1887.6, 262.8)}, None),
    ('R1', 'screw', {'c': (1322.9, 268.5), 'd': 5.3}, None),
    ('R1', 'screw', {'c': (1595.6, 268.6), 'd': 6.6}, None),
    ('R1', 'screw', {'c': (1865.3, 270.4), 'd': 6.8}, None),
    ('R1', 'screw', {'c': (1385.6, 287.7), 'd': 5.8}, None),
    ('R1', 'screw', {'c': (1417.8, 287.9), 'd': 6.1}, None),
    ('R1', 'screw', {'c': (1500, 420.5), 'd': 6.7}, None),
    ('R1', 'screw', {'c': (1693.2, 420.5), 'd': 6.8}, None),
    ('R1', 'screw', {'c': (1323.3, 438.8)}, None),
    ('R1', 'screw', {'c': (1865.9, 438.8), 'd': 6.8}, None),
    ('R2', 'lock', {'c': (1329.2, 600.6), 'd': 26.7}, None),
    ('R2', 'lock', {'c': (1875.3, 600.5), 'd': 29.3}, None),
    ('R2', 'strip', {'rect': (1390.8, 523.9, 1407.5, 648.9)}, None),
    ('R2', 'strip', {'rect': (1791.5, 523.9, 1808.8, 648.9)}, None),
    ('R2', 'plate', {'rect': (1821.5, 497.9, 1868.2, 507.9)}, None),
    ('R2', 'plate', {'rect': (1869.8, 497.9, 1888.2, 507.9)}, None),
    ('R2', 'screw', {'c': (1399.7, 516.6), 'd': 6.3}, None),
    ('R2', 'screw', {'c': (1399.7, 653.6), 'd': 6.4}, None),
    ('R2', 'screw', {'c': (1800.1, 517.3), 'd': 6.7}, None),
    ('R2', 'screw', {'c': (1800.1, 654.7), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1383.8, 504.6)}, None),
    ('R2', 'screw', {'c': (1491.3, 504.4), 'd': 6.6}, None),
    ('R2', 'screw', {'c': (1598.2, 504.7), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1704.8, 504.7), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1811.6, 505.6), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1359.9, 514.4), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1836.4, 514.6), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1358.2, 642.3)}, None),
    ('R2', 'screw', {'c': (1325.9, 686.7), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1384.9, 687.3), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1491.7, 687.2), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1598.3, 687.3), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1704.9, 687.3), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1811.5, 687.2), 'd': 6.8}, None),
    ('R2', 'screw', {'c': (1869.8, 686.6), 'd': 6.8}, None),
    ('R3', 'bar', {'rect': (1390, 770.5, 1828.3, 778.8)}, None),
    ('R3', 'bar', {'rect': (1388.3, 872.1, 1828.3, 880.5)}, None),
    ('R3', 'fuse', {'c': (1420, 798.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1458.3, 798.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1496.7, 798.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1535, 798.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1675, 798.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1711.7, 798.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1750, 798.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1786.7, 798.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1383.3, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1420, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1458.3, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1496.7, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1535, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1675, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1711.7, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1750, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1786.7, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'fuse', {'c': (1825, 848.8), 'size': (28.3, 33.3)}, None),
    ('R3', 'jack', {'c': (1382.8, 803.7), 'd': 25.2, 'colour': 'black'}, None),
    ('R3', 'jack', {'c': (1824.7, 803.4), 'd': 23.7, 'colour': 'black'}, None),
    ('R3', 'meter_white', {'rect': (1551.7, 765.5, 1658.3, 863.8), 'inner': (1565, 780.5, 1651.7, 842.1)}, None),
    ('R3', 'button', {'c': (1635, 854.5), 'size': (20, 18.3), 'colour': 'green'}, None),
    ('R3', 'knob_round', {'c': (1609.7, 853.4), 'd': 13}, None),
    ('R3', 'toggle_guarded', {'c': (1328.3, 783.8)}, None),
    ('R3', 'lock', {'c': (1329, 847.7), 'd': 28.2}, 10.1),
    ('R3', 'lock', {'c': (1877.3, 847), 'd': 28.8}, None),
    ('R3', 'jack', {'c': (1381.2, 894.5), 'd': 16.8, 'colour': 'black'}, None),
    ('R3', 'jack', {'c': (1381.3, 913.8), 'd': 16.8, 'colour': 'black'}, None),
    ('R3', 'jack', {'c': (1408, 915), 'd': 22, 'colour': 'black'}, None),
    ('R3', 'socket_round', {'c': (1480.2, 906.4), 'd': 34.9}, None),
    ('R3', 'knob', {'c': (1609, 913.8), 'size': (31.7, 40), 'angle': 0, 'label': 'ПЕРЕКЛ. ИСТОЧНИКОВ'}, None),
    ('R3', 'plate', {'rect': (1827.3, 742.1, 1868.3, 752.1)}, None),
    ('R3', 'plate', {'rect': (1870, 742.1, 1886.7, 752.1)}, None),
    ('R3', 'screw', {'c': (1375.5, 756.7), 'd': 3}, None),
    ('R3', 'screw', {'c': (1487.3, 755.5)}, None),
    ('R3', 'screw', {'c': (1599.1, 755.5), 'd': 6.2}, None),
    ('R3', 'screw', {'c': (1711.1, 755.2), 'd': 6.1}, None),
    ('R3', 'screw', {'c': (1822.6, 755.5), 'd': 6.7}, None),
    ('R3', 'screw', {'c': (1376, 935.5)}, None),
    ('R3', 'screw', {'c': (1486.9, 935.8), 'd': 5.4}, None),
    ('R3', 'screw', {'c': (1599.4, 935.7), 'd': 6.5}, None),
    ('R3', 'screw', {'c': (1711.1, 935.6), 'd': 6.4}, None),
    ('R3', 'screw', {'c': (1822.5, 935.4), 'd': 6.8}, None),
    ('R4', 'divider', {'rect': (1524.1, 985.8, 1524.1, 1182.4)}, None),
    ('R4', 'bar', {'rect': (1339.1, 1034.1, 1519.1, 1035.8)}, None),
    ('R4', 'bar', {'rect': (1562.4, 1024.1, 1729.1, 1024.1)}, None),
    ('R4', 'bar', {'rect': (1337.4, 1145.1, 1500.8, 1145.8)}, None),
    ('R4', 'bar', {'rect': (1534.1, 1142.4, 1729.1, 1142.4)}, None),
    ('R4', 'fuse', {'c': (1354.1, 1049.1), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1390.8, 1049.1), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1375.1, 1115.8), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1412.4, 1115.8), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1467.4, 1115.8), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1505.1, 1115.8), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1545.8, 1112.4), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1584.1, 1112.4), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1628.4, 1110.8), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse', {'c': (1665.8, 1110.8), 'size': (28.3, 33.3)}, None),
    ('R4', 'fuse_black', {'c': (1430.8, 1034.1), 'size': (28.3, 30)}, None),
    ('R4', 'fuse_black', {'c': (1465.8, 1032.4), 'size': (28.3, 30)}, None),
    ('R4', 'fuse_black', {'c': (1507.4, 1034.1), 'size': (28.3, 30)}, None),
    ('R4', 'fuse_black', {'c': (1549.1, 1032.4), 'size': (28.3, 30)}, None),
    ('R4', 'fuse_black', {'c': (1587.4, 1034.1), 'size': (28.3, 30)}, None),
    ('R4', 'fuse_black', {'c': (1627.4, 1030.8), 'size': (28.3, 30)}, None),
    ('R4', 'fuse_black', {'c': (1429.1, 1070.8), 'size': (28.3, 30)}, None),
    ('R4', 'fuse_black', {'c': (1569.1, 1072.4), 'size': (28.3, 30)}, None),
    ('R4', 'fuse_black', {'c': (1607.4, 1069.1), 'size': (28.3, 30)}, None),
    ('R4', 'toggle', {'c': (1487.4, 1060.8)}, None),
    ('R4', 'toggle', {'c': (1487.4, 1085.8)}, None),
    ('R4', 'toggle', {'c': (1645.8, 1062.4)}, None),
    ('R4', 'toggle', {'c': (1647.4, 1085.8)}, None),
    ('R4', 'lock', {'c': (1328.3, 1095.8), 'd': 28.5}, None),
    ('R4', 'lock', {'c': (1728.6, 1089.3), 'd': 26.6}, None),
    ('R4', 'key', {'c': (1739.1, 1109.1)}, None),
    ('R4', 'plug', {'rect': (1402.4, 1155.8, 1502.4, 1179.1)}, None),
    ('R4', 'plug', {'rect': (1530.8, 1144.1, 1644.1, 1169.1)}, None),
    ('R4', 'cable', {'rect': (1332.4, 1150.8, 1399.1, 1202.4)}, None),
    ('R4', 'cable', {'rect': (1642.4, 1155.8, 1755.8, 1195.8)}, None),
    ('R4', 'plate', {'rect': (1677.4, 983.1, 1719.1, 995.1)}, None),
    ('R4', 'plate', {'rect': (1722.4, 983.1, 1740.8, 995.1)}, None),
    ('R4', 'screw', {'c': (1340.8, 1002.4)}, None),
    ('R4', 'screw', {'c': (1509.1, 999.1)}, None),
    ('R4', 'screw', {'c': (1666.1, 996.1), 'd': 5.6}, None),
    (None, 'frame_connector', {'c': (101, 1010), 'size': (54, 66)}, None),
]

# The blocks' names as the cabin layout has them (data/cabins/osa_ak.json, sp2_*).
BLOCK_IDS = {'L0': 'sp2_f0', 'L1': 'sp2_f1', 'L2': 'sp2_f2', 'L3': 'sp2_f3', 'L4': 'sp2_f4',
             'M0': 'sp2_m0', 'M1': 'sp2_m1', 'M2': 'sp2_m2', 'M3': 'sp2_m3', 'M4': 'sp2_m4',
             'R0_0': 'sp2_r0_0', 'R0_1': 'sp2_r0_1', 'R0_2': 'sp2_r0_2', 'R0_3': 'sp2_r0_3',
             'R1': 'sp2_r1', 'R2': 'sp2_r2', 'R3': 'sp2_r3', 'R4': 'sp2_r4'}

# Where the cabin's simulated controls stand on the face (mm). The OR-31M carries the follow-up systems' scales in its
# three bezels (voenizdat_osa_1991 p52); which one is the Qc scale the photos don't say: the middle one (SAM3D's choice).
CONTROL_MM = {'or31_qc': (893.5, 44.4)}
CONTROL_D_MM = {'or31_qc': 39.0}    # the bezel's window


def to_m(x, y):
    """Trace mm (x right, y down from the top left) -> (a, b) metres on the face: a right, b up, from its centre."""
    return ((x - W_MM / 2) * SX, (H_MM / 2 - y) * SY)


def rect_m(r):
    x0, y0, x1, y1 = r
    a0, b1 = to_m(x0, y0)
    a1, b0 = to_m(x1, y1)
    return a0, b0, a1, b1


def parts(kind=None, block=None):
    """The parts as dicts: block, kind, the geometry fields and `h` (measured height or None)."""
    out = []
    for b, k, g, h in PARTS:
        if (kind is None or k == kind or (isinstance(kind, (set, tuple, list)) and k in kind)) and (block is None or b == block):
            d = dict(g)
            d.update(block=b, kind=k, h=h)
            out.append(d)
    return out


def height(p):
    """A part's height out of the plate, mm: the trace's Z for its kind."""
    return Z.get({'fuse': 'fuse_cap', 'fuse_black': 'fuse_cap', 'toggle': 'toggle_bat', 'toggle_guarded': 'toggle_bat',
                  'meter_white': 'meter', 'foot': 'table'}.get(p['kind'], p['kind']), 0.0)


def radius(p):
    """A round part's footprint radius, mm."""
    if p['kind'] == 'bezel':
        return 31.3
    if 'd' in p:
        return p['d'] / 2
    if 'size' in p:
        return min(p['size']) / 2      # fuses, pointer knobs, buttons: their round part
    return {'screw': 3.1, 'key': 4.0, 'toggle': 6.0, 'toggle_guarded': 6.0, 'lever_lock': 10.0}.get(p['kind'], 5.0)
