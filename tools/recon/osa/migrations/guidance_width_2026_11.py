"""SŁ-2M's front column (fo_panel) widened to its blocks, with what SAM Simulator's photo cut off (16 Nov 2026, the
project owner's go-ahead). Run once; kept for the record.

Measured on the view flat fo_panel_g094a (wwp_gecko g094-a through its certified grade-A camera; out/recon, not in
git), checked against g095-b (grade B, the whole column and the indicator box beside it): the blocks 0004-14M,
OS81-10M, 0004-11M and OS52-14M run to u +0.350 m, where the layout, drawn from SAM Simulator's 610 mm photo
(samsim_panels), stopped them at +0.300. Near the cut the flat agrees with the layout's items to about 2 mm
(OS81-10M's lock and knobs); further aft it reads up to 10 mm aft and 6 mm low (a 2 % stretch and 0.7 degrees of
tilt), so the places below are read beside items already placed.

So the panel grows 50 mm forward (0.61 -> 0.66 m, its forward edge 5 mm past the blocks), its centre moves 25 mm
forward and every item on it moves 25 mm aft in the panel's frame, staying where it was in the cabin; the four blocks
grow 50 mm forward, their aft edges where they were. In the strip that was cut off:
- the forward key locks of 0004-14M (behind a bag in g094-a, its edge seen in g095-b: placed level with its aft lock),
  0004-11M and OS52-14M, each with the printed arc and ОТКР. / ЗАКР. as SAM Simulator's photo has the search
  column's forward locks;
- OS81-10M's three slotted potentiometers, ГОРИЗ. СМЕЩ., ВЕРТ. СМЕЩ. and АМПЛ. РАЗВ. ε (voenizdat_osa_1991 p98-99:
  "ручки КОНТРАСТ., ЯРКОСТЬ, ФОКУС и шлицы потенциометров ГОРИЗ. СМЕЩ., ВЕРТ. СМЕЩ., АМПЛ. РАЗВ."; the legends read on
  g095-b). ГОРИЗ. was a fourth black knob, read off the cut edge of SAM Simulator's photo, which shows a pale part
  there: the slot's bushing;
- the lamp ЗАХ. АПЧ I under 0004-11M's forward lock, the mirror of ЗАХ. АПЧ II at its aft end. The game had put
  ЗАХВ. АПЧ-I on the hex-ringed part under АВАРИЙНЫЙ ПУСК (SAM3D's choice), which stays as an unnamed fixture.

After it ran, the racks were rebuilt (SŁ-1M's aft post from SŁ-2M's new edge, SL2M_FORWARD_Y in
tools/blender/build_cabin_interior.py) and the shell parts rack_sl2m and rack_sl1m given the built meshes' sizes.

    python tools/recon/osa/migrations/guidance_width_2026_11.py [--dry]
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import jsonedit as J  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
LAYOUT = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
CONSOLE = os.path.join(REPO, 'data', 'procedures', 'osa_ak.json')
VIEW = os.path.join(REPO, 'tools', 'traces', 'osa_interior', 'views', 'g094-a.json')

PANEL = 'fo_panel'
OLD_W, NEW_W = 0.61, 0.66
MOVE = (NEW_W - OLD_W) / 2.0                     # the panel's centre forward, every item aft in its frame
GROW = 0.05                                      # the blocks that reached the old edge
WIDENED = ('g_0004_14m', 'g_os81_10m', 'g_0004_11m', 'g_os52_14m')
FLAT = 'wwp_gecko g094-a (the view flat fo_panel_g094a through its certified camera)'
WHY = (' Widened 50 mm forward on 16 Nov 2026: SAM Simulator\'s 610 mm photo cuts the column; on ' + FLAT +
       ' its blocks run to 350 mm forward of the old centre (g095-b agrees). Centre y 2.225, every item u - 25 mm in '
       'this frame, so each stays where it was; the four blocks that reached the old edge grow 50 mm forward '
       '(tools/recon/osa/migrations/guidance_width_2026_11.py).')


def _at(u, v):
    return '[%s, %s]' % (('%.4f' % u).rstrip('0').rstrip('.'), ('%.4f' % v).rstrip('0').rstrip('.'))


def _new_fixtures():
    """The strip's fixtures, in the widened panel's frame."""
    out = []
    locks = (('0004-14M', 0.297, 0.515, 'behind a bag in g094-a; g095-b shows it at the block\'s forward edge level with the '
                                         'bat knob: placed level with its aft lock (medium)'),
             ('0004-11M', 0.296, 0.052, 'read on the flat beside OS81-10M\'s lock, about 2 mm'),
             ('OS52-14M', 0.298, -0.203, 'read on the flat beside the elevation wheel, about 2 mm'))
    for block, u, v, how in locks:
        src = '%s (%s); g095-b' % (FLAT, how)
        out.append('{"panel": "fo_panel", "at_m": %s, "kind": "latch", "size_m": 0.022, "color": "chrome", "notes": '
                   '%s}' % (_at(u, v), json.dumps("%s's forward key lock: %s." % (block, src), ensure_ascii=False)))
        # the search column's forward locks' arc and legends (samsim_panels), at the same offsets from the lock
        out.append('{"panel": "fo_panel", "at_m": %s, "kind": "scale", "size_m": 0.0456, "angles_deg": [10, 116], '
                   '"marks": ["", ""], "arrow": "first", "notes": "The lock\'s printed arc, its arrow at the top pointing to '
                   'ОТКР., drawn as the search column\'s forward locks have it (samsim_panels); g095-b shows ОТКР. above it and '
                   'the arc running down its forward side."}' % _at(u - 0.0034, v + 0.0091))
        out.append('{"panel": "fo_panel", "at_m": %s, "kind": "text", "size_m": 0.0036, "label": "ОТКР.", "notes": '
                   '"Engraved at the arc\'s arrow (samsim_panels\' forward locks; g095-b)."}' % _at(u - 0.0114, v + 0.0321))
        out.append('{"panel": "fo_panel", "at_m": %s, "kind": "text", "size_m": 0.0036, "label": "ЗАКР.", "angle_deg": -80, '
                   '"notes": "Engraved along the lock\'s arc, turned to read down it (samsim_panels\' forward locks; g095-b)."}'
                   % _at(u + 0.0186, v - 0.0099))
    return out


CONTROLS = [
    # (id, label, at in the widened frame, legend, what it does)
    ('fo.el_sweep', 'АМПЛ. РАЗВ. ε', (0.288, 0.345), 'АМПЛ. РАЗВ. ε',
     'Sets the height of the elevation sweep; nothing reads it (the raster\'s height is fixed in SAM3D).'),
    ('fo.el_vshift', 'ВЕРТ. СМЕЩ. (ε)', (0.291, 0.313), 'ВЕРТ. СМЕЩ.',
     'Moves the elevation scope\'s raster up and down (ScopePictureOf, data/cabins/osa_ak.json "picture").'),
    ('fo.el_hshift', 'ГОРИЗ. СМЕЩ. (ε)', (0.272, 0.2664), 'ГОРИЗ. СМЕЩ.',
     'Moves the elevation scope\'s raster across (ScopePictureOf, data/cabins/osa_ak.json "picture").'),
]
SOURCE = ('voenizdat_osa_1991 p98-99 (OS81-10M: slotted potentiometers ГОРИЗ. СМЕЩ., ВЕРТ. СМЕЩ., АМПЛ. РАЗВ. set the '
          'indicator\'s picture); wwp_gecko g094-a (its place, the view flat fo_panel_g094a) and g095-b (its legend); '
          'samsim_panels cuts it off')


def main(dry):
    t = J.read(LAYOUT)
    if abs(J.resolve(t, ['panels', {'id': PANEL}, 'size_m', 0])[1] - OLD_W) > 1e-6:
        raise SystemExit('already widened: fo_panel is not %.2f m wide' % OLD_W)
    doc = json.loads(t)
    t = J.shift(t, ['panels', {'id': PANEL}, 'centre_m', 1], MOVE)
    t = J.set_value(t, ['panels', {'id': PANEL}, 'size_m', 0], NEW_W, 2)
    t = J.append_to_string(t, ['panels', {'id': PANEL}, 'notes'], WHY)
    moved = 0
    for key in ('controls', 'lamps', 'displays', 'blocks', 'fixtures'):
        for i, item in enumerate(doc[key]):
            if item.get('panel') != PANEL:
                continue
            if key == 'blocks' and item['id'] in WIDENED:
                t = J.shift(t, [key, i, 'size_m', 0], GROW)       # aft edge kept: centre +25 mm, then -25 mm in the frame
                t = J.append_to_string(t, [key, i, 'notes'], ' Grown 50 mm forward to the photo\'s block (%s), 16 Nov 2026.' % FLAT) \
                    if 'notes' in item else t
            else:
                t = J.shift(t, [key, i, 'at_m', 0], -MOVE, 4)
            moved += 1
    doc = json.loads(t)

    # ГОРИЗ.'s knob gives way to its potentiometer
    t = J.remove_items(t, ['fixtures'], lambda f: f.get('panel') == PANEL and f.get('label') == 'ГОРИЗ.' and f.get('kind') == 'knob')
    doc = json.loads(t)
    after = next(i for i, f in enumerate(doc['fixtures']) if f.get('panel') == PANEL and "0004-14M's aft key lock" in f.get('notes', ''))
    t = J.insert_items(t, ['fixtures'], _new_fixtures(), before=after + 1)

    doc = json.loads(t)
    after = next(i for i, c in enumerate(doc['controls']) if c['control'] == 'fo.el_focus')
    raws = ['{"control": "%s", "panel": "fo_panel", "at_m": %s, "look": {"kind": "trim", "size_m": 0.017, "color": "chrome", '
            '"label": "%s", "label_at": "below"}}' % (cid, _at(*at), legend) for cid, label, at, legend, what in reversed(CONTROLS)]
    t = J.insert_items(t, ['controls'], raws, before=after + 1)

    # ЗАХ. АПЧ I where the photo has it; the hex-ringed part it stood on stays, unnamed
    doc = json.loads(t)
    old = next(lamp for lamp in doc['lamps'] if lamp['id'] == 'apch_1')['at_m']
    t = J.replace_value(t, ['lamps', {'id': 'apch_1'}, 'at_m'], _at(0.300, -0.021))
    t = J.replace_value(t, ['lamps', {'id': 'apch_1'}, 'confidence'], '"medium"')
    t = J.replace_value(t, ['lamps', {'id': 'apch_1'}, 'notes'], json.dumps(
        'Place: the dark red lamp under 0004-11M\'s forward lock, ЗАХ. АПЧ I over it, the mirror of ЗАХ. АПЧ II at the '
        'block\'s aft end (%s, cut off on samsim_panels; g095-b reads the legend). 16 Nov 2026; until then SAM3D had it on '
        'the hex-ringed part under АВАРИЙНЫЙ ПУСК.' % FLAT, ensure_ascii=False))
    t = J.replace_value(t, ['lamps', {'id': 'apch_1'}, 'look'],
                        '{"kind": "signal", "ring": "gunmetal", "size_m": 0.014, "label": "ЗАХ. АПЧ I"}')
    t = J.insert_items(t, ['fixtures'], [
        '{"panel": "fo_panel", "at_m": %s, "kind": "hex_button", "size_m": 0.02, "notes": "A hex-ringed part under '
        'АВАРИЙНЫЙ ПУСК on samsim_panels (SAM Simulator\'s AK photo), its legend not legible; the photographed BM3-1 '
        '(g094-a) has none. SAM3D took it for ЗАХВ. АПЧ-I until 16 Nov 2026."}' % _at(*old)],
        before=None)

    # ГОРИЗ. СМЕЩ. and ВЕРТ. СМЕЩ. set the elevation scope's picture
    pic = ['displays', {'id': 'elev_search'}, 'picture']
    t = J.add_key(t, pic, 'hshift', '"fo.el_hshift"')
    t = J.add_key(t, pic, 'vshift', '"fo.el_vshift"')
    t = J.replace_value(t, pic + ['notes'], json.dumps(
        'OS81-10M\'s КОНТРАСТ., ЯРКОСТЬ, ФОКУС and its slotted ГОРИЗ. СМЕЩ., ВЕРТ. СМЕЩ. (voenizdat_osa_1991 p98-99); '
        'АМПЛ. РАЗВ. ε is not read.', ensure_ascii=False))

    c = J.read(CONSOLE)
    cdoc = json.loads(c)
    at = next(i for i, x in enumerate(cdoc['controls']) if x['id'] == 'fo.el_focus')
    craws = ['{"id": "%s", "label": "%s", "station": "deputy_commander", "kind": "wheel", "min": 0.0, "max": 1.0, '
             '"initial_value": 0.5, "unit": "", "source": %s, "confidence": "high", "notes": %s}'
             % (cid, label, json.dumps(SOURCE, ensure_ascii=False), json.dumps('A slotted potentiometer, set with a screwdriver. ' + what, ensure_ascii=False))
             for cid, label, _, _, what in CONTROLS]
    c = J.insert_items(c, ['controls'], craws, before=at + 1)

    v = J.read(VIEW)
    def back(m):
        return 'panel:fo_panel@%.3f,%s' % (float(m.group(1)) - MOVE, m.group(2))
    v2 = re.sub(r'panel:fo_panel@(-?[0-9.]+),(-?[0-9.]+)', back, v)

    if dry:
        print('dry run: %d items would move; nothing written' % moved)
        return
    J.write(LAYOUT, t)
    J.write(CONSOLE, c)
    J.write(VIEW, v2)
    print('fo_panel widened to %.2f m: %d items moved %.0f mm aft in its frame, %d blocks grown; strip parts added' %
          (NEW_W, moved, MOVE * 1000, len(WIDENED)))


if __name__ == '__main__':
    main('--dry' in sys.argv)
