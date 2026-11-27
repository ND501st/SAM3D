"""OS81-9M's indicator controls as SAM Simulator's photo shows them (16 Nov 2026, the project owner's review). Run once;
kept for the record.

Read on samsim_panels (SAM Simulator's AK photo) at 10x, about 1.12 px a millimetre there, placed by the layout's own
ВЕРТ. СМЕЩ. I and II (which agree with the photo to 1 mm, as do the ГОРИЗ. СМЕЩ.):
- ВЕРТ. СМЕЩ. and ГОРИЗ. СМЕЩ. (I and II) are slotted potentiometers, as voenizdat_osa_1991 p98-99 names OS81-10M's: a
  black cup 27.6 mm across, a cream knurled bushing 19.6 mm in it, the slotted shaft 8 mm in that. They were drawn as
  flat brass trims. Now look "trim" with "ring": "black" (SimCabinComponent's SM_PotCup, SM_PotBushing, SM_PotShaft).
- АСТИГМАТ. I and II: a cream plate, an elongated hexagon 32 x 15 mm, turned in the face on a slotted screw near one
  point (I's aft, II's forward), a pressed boss near the other. Taken for a cover over the astigmatism trimmer (SAM3D's
  reading; the project owner's too): the trimmer, a slot 6 mm across, under the boss; the cover a control of its own
  (ssc.astig_cover_1, _2: shut, turned aside), guarding the trimmer. The trimmers had been drawn as levers 10 mm above
  the plates, on the engraved АСТИГМАТ., which is now a text of its own.

    python tools/recon/osa/migrations/os81_9m_parts_2026_11.py [--dry]
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import jsonedit as J  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
LAYOUT = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
CONSOLE = os.path.join(REPO, 'data', 'procedures', 'osa_ak.json')

PHOTO = 'samsim_panels (SAM Simulator\'s AK photo at 10x, 16 Nov 2026)'
POTS = {'ssc.vshift_1': 'left', 'ssc.hshift_1': 'left', 'ssc.vshift_2': 'right', 'ssc.hshift_2': 'right'}
# per channel: the pivot screw and the boss as read (u, v m on op_panel), the cover's open angle, the legend's place
ASTIG = {
    1: {'pivot': (-0.0933, 0.4002), 'boss': (-0.0772, 0.3953), 'open': 100, 'legend': (-0.0848, 0.4109), 'end': 'aft'},
    2: {'pivot': (0.1578, 0.3984), 'boss': (0.1373, 0.3975), 'open': -100, 'legend': (0.1475, 0.4090), 'end': 'forward'},
}
BOSS_MM = 18.0          # the pivot to the boss along the plate (sl2m_covers.ASTIG)
CENTRE_MM = 9.0         # the pivot to the plate's middle (its points 7.5 mm behind the pivot and 25.5 mm ahead)


def r4(x):
    return float('%.4f' % x)


def plan():
    out = {}
    for ch, a in ASTIG.items():
        du, dv = a['boss'][0] - a['pivot'][0], a['boss'][1] - a['pivot'][1]
        n = math.hypot(du, dv)
        d = (du / n, dv / n)
        boss = (a['pivot'][0] + BOSS_MM / 1000 * d[0], a['pivot'][1] + BOSS_MM / 1000 * d[1])
        centre = (a['pivot'][0] + CENTRE_MM / 1000 * d[0], a['pivot'][1] + CENTRE_MM / 1000 * d[1])
        hinge = (a['pivot'][0] - centre[0], a['pivot'][1] - centre[1])
        axis = math.degrees(math.atan2(d[0], d[1])) % 360.0
        out[ch] = {'boss': tuple(map(r4, boss)), 'centre': tuple(map(r4, centre)), 'hinge': tuple(map(r4, hinge)),
                   'axis': round(axis, 1)}
    return out


def main(dry):
    t = J.read(LAYOUT)
    doc = json.loads(t)
    if any(c['control'] == 'ssc.astig_cover_1' for c in doc['controls']):
        raise SystemExit('already done: ssc.astig_cover_1 is in the layout')
    p = plan()
    for cid, side in POTS.items():
        look = next(c for c in doc['controls'] if c['control'] == cid)['look']
        t = J.replace_value(t, ['controls', {'control': cid}, 'look'], json.dumps(
            {'kind': 'trim', 'size_m': 0.027, 'color': 'ivory', 'ring': 'black', 'label': look['label'], 'label_at': side},
            ensure_ascii=False))
        t = J.add_key(t, ['controls', {'control': cid}], 'notes', json.dumps(
            'A slotted potentiometer: a black cup 27.6 mm across (drawn 27 mm, clear of the range hood), a cream knurled bushing, the slot in the shaft (%s).'
            % PHOTO, ensure_ascii=False))
    for ch, a in p.items():
        cid = 'ssc.astig_%d' % ch
        t = J.replace_value(t, ['controls', {'control': cid}, 'at_m'], json.dumps(list(a['boss'])))
        t = J.replace_value(t, ['controls', {'control': cid}, 'look'], '{"kind": "trim", "size_m": 0.006, "color": "chrome"}')
        t = J.add_key(t, ['controls', {'control': cid}], 'notes', json.dumps(
            'The trimmer\'s slot under its cover\'s boss (%s): the cover is ssc.astig_cover_%d. It had been drawn as a '
            'lever 10 mm higher, on the engraved АСТИГМАТ.' % (PHOTO, ch), ensure_ascii=False))
    doc = json.loads(t)
    covers = []
    for ch, a in p.items():
        covers.append(
            '{"control": "ssc.astig_cover_%d", "panel": "op_panel", "at_m": %s, "flap": {"parts": [{"part": "SM_CoverAstig", '
            '"color": "ivory"}], "fixed": [{"part": "SM_CoverAstigPost", "color": "chrome"}], "hinge_m": %s, "lift_m": 0.003, '
            '"axis_deg": %s, "swing": "plane", "angles_deg": [0, %d], "notes": %s}}'
            % (ch, json.dumps(list(a['centre'])), json.dumps(list(a['hinge'])), a['axis'], ASTIG[ch]['open'], json.dumps(
                'АСТИГМАТ. %s\'s cover: a cream plate 32 x 15 mm turned on the slotted screw at its %s point, its boss over '
                'the trimmer (%s; tools/blender/sl2m_covers.py ASTIG). Open, it lies turned up clear of the trimmer.'
                % ('I' * ch, ASTIG[ch]['end'], PHOTO), ensure_ascii=False)))
    at = next(i for i, c in enumerate(doc['controls']) if c['control'] == 'ssc.astig_1')
    t = J.insert_items(t, ['controls'], covers, before=at)
    doc = json.loads(t)
    after = next(i for i, f in enumerate(doc['fixtures']) if f.get('panel') == 'op_panel' and f.get('label') == '— I —')
    texts = ['{"panel": "op_panel", "at_m": %s, "kind": "text", "size_m": 0.0036, "label": "АСТИГМАТ.", "notes": %s}'
             % (json.dumps(list(ASTIG[ch]['legend'])), json.dumps('Engraved over the cover (%s).' % PHOTO, ensure_ascii=False))
             for ch in (1, 2)]
    t = J.insert_items(t, ['fixtures'], texts, before=after + 1)

    c = J.read(CONSOLE)
    cdoc = json.loads(c)
    at = next(i for i, x in enumerate(cdoc['controls']) if x['id'] == 'ssc.astig_1')
    craws = ['{"id": "ssc.astig_cover_%d", "label": "cover over АСТИГМАТ. %s", "station": "operator", "kind": "switch", '
             '"position_labels": ["closed", "open"], "source": %s, "confidence": "low", "notes": %s}'
             % (ch, 'I' * ch, json.dumps('samsim_panels (the AK console\'s photo: a cream plate on a screw over the trimmer)'),
                json.dumps('Turned aside to reach the trimmer with a screwdriver; nothing else reads it. That the plate is a '
                           'cover is SAM3D\'s reading of the photo.'))
             for ch in (1, 2)]
    c = J.insert_items(c, ['controls'], craws, before=at)
    for ch in (1, 2):
        cid = 'ssc.astig_%d' % ch
        c = J.add_key(c, ['controls', {'id': cid}], 'guard', '"ssc.astig_cover_%d"' % ch)
        c = J.replace_value(c, ['controls', {'id': cid}, 'notes'], json.dumps(
            'The range indicator\'s %s trace\'s astigmatism, a slotted trimmer under a cover (ssc.astig_cover_%d): off its '
            'middle it smears the trace as focus does (ssc_range\'s picture).' % ('first' if ch == 1 else 'second', ch),
            ensure_ascii=False))
    if dry:
        print(json.dumps(p, indent=1))
        return
    J.write(LAYOUT, t)
    J.write(CONSOLE, c)
    print('OS81-9M: 4 pots in their cups, 2 astigmatism trimmers under their covers')


if __name__ == '__main__':
    main('--dry' in sys.argv)
