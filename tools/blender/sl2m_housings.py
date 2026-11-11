"""SŁ-2M's raised housings: the bodies that stand proud of its three columns' block plates round the displays (the range
indicator's hood, the search indicator's flange, the TV's cast bezel, the elevation scope's ring, the dials' cups and
the strips' frames). Pure Python, no Blender: build_cabin_interior.py draws them into SM_RackSL2M, and
tools/tests/test_sl2m_housings.py checks them against the game's layout.

Every housing is centred on a display of `data/cabins/osa_ak.json` (or at a fixed point of its panel) in the panel's
(u, v) metres, so the displays, controls and lamps stay where the cabin file puts them; the housings only fill the room
round them. Shapes after the samsim_panels photographs of the Osa-AK's SŁ-2M (reference/samsim/columns_three.jpg)
checked against wwp_gecko g094-a; sizes are proportions read off those photographs against the cabin file's display
sizes, about +-5 mm, and the depths by eye, about +-1 cm.

The game draws each display's screen 1.3 cm out from the plate with a dark bezel 0-1.2 cm out (SimCabinDisplayComponent):
a housing's hole is never smaller than the screen except where `crop` says how much of the screen's edge its mask
hides (the TV's rounded mask, as on the real monitor).
"""
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CABIN = ROOT / 'data' / 'cabins' / 'osa_ak.json'
PANELS = ('op_panel', 'so_panel', 'fo_panel')
CLEAR_M = 0.003          # a housing keeps this far from any control, lamp or fixture of its panel

# kind: 'ring' (outer and hole diameters) or 'rrect' (outer and hole [w, h], corner radii).
# style decides the section the builder turns or sweeps (build_cabin_interior.sl2m_housings).
HOUSINGS = [
    # ---- the operator's column (range): OS81-9M, OS81-18M
    {'id': 'range_hood', 'panel': 'op_panel', 'on': 'ssc_range', 'kind': 'rrect', 'style': 'hood',
     'outer': [0.200, 0.176], 'outer_r': 0.034,
     'hole': [0.150, 0.120], 'hole_r': 0.012, 'depth': 0.050, 'mat': 'Paint_Panel',
     'why': 'OS81-9M: a white moulded hood round the range tube, its corners well rounded, its front edge soft. On '
            'g094-a\'s flat its foot reaches u 132 on the forward side and its front edge -107 aft, its side wall seen '
            '47 mm wide: 50 mm deep, 193 x 175 mm (samsim_panels: 220 x 190 px at its 8 % larger scale)'},
    {'id': 'range_dial', 'panel': 'op_panel', 'at': [0.038, -0.252], 'kind': 'rrect', 'style': 'dial_plate',
     'outer': [0.176, 0.106], 'outer_r': 0.012, 'depth': 0.008, 'mat': 'Paint_Panel',
     'dial': {'at': [0.004, 0.012], 'd': 0.07}, 'notch': {'control': 'ssc.range_wheel', 'clear': 0.0045},
     'why': 'OS81-18M: the range dial (Г.О. 1 дел. — 1000 м, Т.О. 1 дел. — 5 м) behind a white raised plate, under a '
            'half-cone visor; the plate seen flat on g094-a at u -54..127, v -306..-199, its window high in the cowl '
            '(samsim_panels; g094-a)'},
    # ---- the senior operator's column (search): 0094-5, OP81-16M, OS52-12M
    {'id': 'ppi_flange', 'panel': 'so_panel', 'on': 'soc_ppi', 'kind': 'ring', 'style': 'flange',
     'outer': 0.378, 'hole': 0.290, 'depth': 0.046, 'cone': [0.0175, 0.031], 'mat': 'Paint_LightGrey',
     'screws': 16, 'knobs': 4,
     'why': 'OP81-16M: a pale grey drum round the search tube, sixteen screws and four red-brown locking knobs on its '
            'face, a dark scale cone from the face down to the screen (samsim_panels; g094-a). On g094-a\'s flat the '
            'drum is 389 mm high (380 without the 9 mm its front edge is seen low; 378 drawn, clear of the oval mount below it) and 419 wide: its side wall, the '
            'light crescent forward of the screen, adds 26 mm, '
            '46 mm deep at the camera\'s 0.56 mm per mm; the cone shows 35 mm wide aft of the screen and none forward, '
            '17.5 mm wide and 31 mm deep, which puts the screen 15 mm out (the depth map: 25)'},
    {'id': 'beta_visor', 'panel': 'so_panel', 'on': 'beta_c', 'kind': 'arc', 'style': 'visor',
     'outer': 0.124, 'inner': 0.108, 'arc_deg': [12, 168], 'depth': 0.026, 'mat': 'Paint_Panel',
     'why': 'βс: a white visor over the dial\'s top (samsim_panels); its lower half open, the azimuth wheel beside it'},
    # ---- the deputy commander's column (TV, elevation, launch): VPU-44, OS81-10M, 0004-11M, OS52-14M
    {'id': 'tv_bezel', 'panel': 'fo_panel', 'on': 'tov_monitor', 'kind': 'rrect', 'style': 'cast',
     'outer': [0.282, 0.238], 'outer_r': 0.022, 'offset': [0.0, -0.011],
     'hole': [0.247, 0.180], 'hole_r': 0.018, 'crop': 0.014, 'depth': 0.030, 'mat': 'Paint_Radio',
     'why': 'VPU-44: a dark grey hammered cast bezel on the grey block (g094-a; samsim_panels shows it lighter), its mask rounding the screen\'s corners, deeper below the screen '
            '(samsim_panels; g094-a)'},
    {'id': 'elev_ring', 'panel': 'fo_panel', 'on': 'elev_search', 'kind': 'ring', 'style': 'knurl',
     'outer': 0.126, 'hole': 0.098, 'depth': 0.036, 'mat': 'Paint_Panel',
     'why': 'OS81-10M: the yellow elevation tube in a deep knurled white ring (samsim_panels)'},
    {'id': 'eps_visor', 'panel': 'fo_panel', 'on': 'epsilon_c', 'kind': 'arc', 'style': 'visor',
     'outer': 0.124, 'inner': 0.108, 'arc_deg': [12, 168], 'depth': 0.026, 'mat': 'Paint_Panel',
     'why': 'εс: a white visor over the dial\'s top, as βс (samsim_panels)'},
    {'id': 'dts_housing', 'panel': 'fo_panel', 'on': 'dts_voltmeter', 'kind': 'rrect', 'style': 'frame',
     'outer': [0.084, 0.072], 'outer_r': 0.010, 'offset': [0.001, -0.001], 'hole': [0.072, 0.062], 'hole_r': 0.004,
     'depth': 0.014,
     'mat': 'Paint_Panel', 'why': 'ДЦ: the meter in a raised white housing (samsim_panels)'},
]


def load_cabin(path=CABIN):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def centre(h, cabin):
    """A housing's centre on its panel (u, v): its display's, moved by its offset, or its own `at`."""
    if 'on' in h:
        d = next(d for d in cabin['displays'] if d['id'] == h['on'])
        u, v = d['at_m']
    else:
        u, v = h['at']
    du, dv = h.get('offset', (0.0, 0.0))
    return u + du, v + dv


def hole_centre(h, cabin):
    """The hole's centre: on the display itself (an offset moves only the outer edge)."""
    if 'on' in h:
        return tuple(next(d for d in cabin['displays'] if d['id'] == h['on'])['at_m'])
    return tuple(h['at'])


def rrect(cx, cy, w, h, r, seg=6):
    """A rounded rectangle's outline, anticlockwise (as build_cabin_interior.rrect)."""
    r = min(r, w / 2 - 1e-4, h / 2 - 1e-4)
    pts = []
    for ox, oy, a0 in ((cx + w / 2 - r, cy - h / 2 + r, -90), (cx + w / 2 - r, cy + h / 2 - r, 0),
                       (cx - w / 2 + r, cy + h / 2 - r, 90), (cx - w / 2 + r, cy - h / 2 + r, 180)):
        for k in range(seg + 1):
            a = math.radians(a0 + 90 * k / seg)
            pts.append((ox + r * math.cos(a), oy + r * math.sin(a)))
    return pts


def circle(cx, cy, r, seg=48):
    return [(cx + r * math.cos(2 * math.pi * k / seg), cy + r * math.sin(2 * math.pi * k / seg)) for k in range(seg)]


def arc(cx, cy, r, a0, a1, seg):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * k / seg)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * k / seg))) for k in range(seg + 1)]


def densify(pts, step=0.002):
    """A closed outline with no edge longer than `step`."""
    out = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
        n = max(1, int(math.ceil(math.hypot(x1 - x0, y1 - y0) / step)))
        out += [(x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n) for k in range(n)]
    return out


def notch_circle(h, cabin):
    """The circle a housing's outline is cut back to, round a control that stands in front of its corner: (u, v, r)."""
    n = h.get('notch')
    if n is None:
        return None
    c = next(c for c in cabin['controls'] if c['control'] == n['control'] and c['panel'] == h['panel'])
    size = c.get('look', {}).get('size_m', 0.12)
    return c['at_m'][0], c['at_m'][1], size / 2 + n['clear']


def outer_outline(h, cabin, seg=8):
    """The housing's outer edge on the plate, anticlockwise. An arc is the annular sector of its visor; a notch cuts the
    outline back round its control (every point inside the circle pushed out to it)."""
    cx, cy = centre(h, cabin)
    if h['kind'] == 'ring':
        return circle(cx, cy, h['outer'] / 2, 48)
    if h['kind'] == 'arc':
        a0, a1 = h['arc_deg']
        return arc(cx, cy, h['outer'] / 2, a0, a1, 24) + arc(cx, cy, h['inner'] / 2, a1, a0, 24)
    pts = rrect(cx, cy, h['outer'][0], h['outer'][1], h.get('outer_r', 0.0), seg)
    cut = notch_circle(h, cabin)
    if cut is None:
        return pts
    nu, nv, nr = cut
    out = []
    for (x, y) in densify(pts):
        d = math.hypot(x - nu, y - nv)
        if d < nr:
            x, y = nu + (x - nu) * nr / d, nv + (y - nv) * nr / d
        out.append((x, y))
    return out


def hole_outline(h, cabin, seg=8):
    if 'hole' not in h:
        return None
    cx, cy = hole_centre(h, cabin)
    if h['kind'] == 'ring':
        return circle(cx, cy, h['hole'] / 2, 48)
    return rrect(cx, cy, h['hole'][0], h['hole'][1], h.get('hole_r', 0.0), seg)


# ------------------------------------------------------------------------------------------------ the layout's items

def footprints(cabin, console, panel):
    """Every control, lamp and fixture on a panel as (what, label, centre, size, round), sized as the game draws it."""
    import sys
    sys.path.insert(0, str(ROOT / 'tools'))
    import panel_sheet
    items = []
    for it in panel_sheet.build_items(console, cabin):
        if it['panel'] != panel or it['what'] == 'display':
            continue
        rnd = it['style'] in panel_sheet.ROUND and abs(it['size'][0] - it['size'][1]) < 1e-9
        items.append((it['what'], it['id'], tuple(it['at']), tuple(it['size']), rnd))
    for k, fx in enumerate(cabin.get('fixtures', [])):
        if fx['panel'] != panel:
            continue
        s = fx.get('size_m', 0.02)
        size = (s, s) if isinstance(s, (int, float)) else tuple(s)
        rnd = isinstance(s, (int, float))
        items.append(('fixture', f"{fx.get('kind')}#{k} {fx.get('label', '')}".strip(), tuple(fx['at_m']), size, rnd))
    return items


def _inside(p, poly):
    x, y = p
    inside = False
    for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]):
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            inside = not inside
    return inside


def _seg_dist(p, a, b):
    ax, ay = a
    bx, by = b
    px, py = p
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def signed_distance(p, poly):
    """Distance from p to a polygon's edge, negative inside it."""
    d = min(_seg_dist(p, a, b) for a, b in zip(poly, poly[1:] + poly[:1]))
    return -d if _inside(p, poly) else d


def item_outline(at, size, rnd):
    if rnd:
        return circle(at[0], at[1], size[0] / 2, 24)
    return rrect(at[0], at[1], size[0], size[1], 0.0, 1)


def gap(h, item, cabin):
    """The clear gap between a housing's body (outer outline less its hole) and an item; negative where they overlap.
    An item wholly inside the hole is the display's own and is not in the housing's way (gap None)."""
    _, _, at, size, rnd = item
    outer = outer_outline(h, cabin)
    hole = hole_outline(h, cabin)
    pts = item_outline(at, size, rnd)
    if hole is not None and all(_inside(p, hole) for p in pts):
        return None
    best = min(signed_distance(p, outer) for p in pts + [at])
    for q in outer:                                       # the housing's corners inside a large item
        if _inside(q, pts):
            best = min(best, -0.001)
    return best


def problems(cabin=None, console=None):
    """What is wrong with the housings against the layout: overlaps, holes smaller than their screens, housings over
    their panel's edge."""
    cabin = cabin or load_cabin()
    if console is None:
        with open(ROOT / 'data' / 'procedures' / 'osa_ak.json', encoding='utf-8') as f:
            console = json.load(f)
    out = []
    panels = {p['id']: p for p in cabin['panels']}
    for h in HOUSINGS:
        items = footprints(cabin, console, h['panel'])
        for it in items:
            g = gap(h, it, cabin)
            if g is not None and g < CLEAR_M:
                out.append(f"{h['id']}: {it[0]} {it[1]} {g * 1000:.1f} mm away")
        p = panels[h['panel']]
        for (u, v) in outer_outline(h, cabin):
            if abs(u) > p['size_m'][0] / 2 or abs(v) > p['size_m'][1] / 2:
                out.append(f"{h['id']}: over the panel's edge at ({u:.3f}, {v:.3f})")
                break
        if 'on' in h and 'hole' in h:
            d = next(d for d in cabin['displays'] if d['id'] == h['on'])
            crop = h.get('crop', 0.0)
            need = d['size_m'][0] if h['kind'] == 'ring' else d['size_m']
            if h['kind'] == 'ring':
                if h['hole'] < need - crop:
                    out.append(f"{h['id']}: hole {h['hole']} smaller than the screen {need}")
            elif h['hole'][0] < need[0] - crop or h['hole'][1] < need[1] - crop:
                out.append(f"{h['id']}: hole {h['hole']} smaller than the screen {need}")
        if 'hole' in h:
            hole = hole_outline(h, cabin)
            outer = outer_outline(h, cabin)
            if min(-signed_distance(q, outer) for q in hole) < 0.003:
                out.append(f"{h['id']}: hole within 3 mm of the outer edge")
    return out


def room(h, cabin=None, console=None):
    """The nearest items to a housing, nearest first: [(gap m, what, id)]."""
    cabin = cabin or load_cabin()
    if console is None:
        with open(ROOT / 'data' / 'procedures' / 'osa_ak.json', encoding='utf-8') as f:
            console = json.load(f)
    rows = []
    for it in footprints(cabin, console, h['panel']):
        g = gap(h, it, cabin)
        if g is not None:
            rows.append((g, it[0], it[1]))
    return sorted(rows)


if __name__ == '__main__':
    cab = load_cabin()
    for hh in HOUSINGS:
        print(hh['id'], [(round(g * 1000, 1), w, i) for g, w, i in room(hh, cab)[:4]])
    print('\n'.join(problems(cab)) or 'clear')
