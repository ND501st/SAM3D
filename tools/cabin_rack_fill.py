"""Fill the 3D cabin's right-wall racks and the gyro rack with the kinds of parts their units carry.

    python tools/cabin_rack_fill.py            # rewrite the generated fixtures in data/cabins/osa_ak.json
    python tools/cabin_rack_fill.py --check    # exit 1 if the file isn't what this script would write

What is known (data/cabins/osa_ak.json panel notes): which units sit where (mon_2112_80 Fig. 3) and what the real
units look like (rwd_mb3 9a33_41, 45, 46, 50, 51, 52: bronze round tuning knobs in rows, small coaxial sockets, rows
of small lamps, printed tables, chrome toggles). What is NOT known: the position of any single control on these
units. So the fixtures this script writes are filler: representative parts in plausible rows, placed clear of each
unit's latches and nameplate, chosen deterministically from the block's id. Each carries a "notes" starting "rack fill" so a rerun
replaces them; nothing here is a source for anything, and no procedure refers to them.
"""
import argparse
import hashlib
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
NOTE = 'rack fill (estimate; kinds from rwd_mb3 9a33_41/45/46, positions not measured)'
MARK = '"notes": "rack fill'
RACKS = ('sp1m_panel', 'sp2m_panel', 'sp3m_panel')
NL = chr(10)

# Extra content for the gyro rack (rwd_mb3 9a33_51/52): the printed tables beside the meters and the label strip
# beside the lamp column of OK81-4M-II. Positions estimated from the photographs' proportions.
GYRO = [
    ('diagram', (-0.09, 0.2), [0.1, 0.12], None),
    ('diagram', (-0.52, -0.217), [0.05, 0.17], None),
    ('diagram', (0.19, -0.04), [0.2, 0.05], None),
] + [('lamp', (-0.58, -0.145 - 0.017 * k), 0.007, 'red' if k % 3 == 0 else 'amber') for k in range(8)]


def pick(block_id, n):
    return int(hashlib.sha1(block_id.encode()).hexdigest(), 16) % n


def fx(panel, at, kind, size, color=None):
    d = {'panel': panel, 'at_m': [round(at[0], 4), round(at[1], 4)], 'kind': kind, 'size_m': size}
    if color:
        d['color'] = color
    d['notes'] = NOTE
    return d


def unit_fixtures(block):
    """Filler for one block: rows of bronze knobs and sockets, a table with knobs, or lamps with toggles."""
    panel, (cx, cy) = block['panel'], block['at_m']
    w, h = block['size_m']
    out = []
    usable = w - 0.14                   # clear of the latches at the block's ends
    rows = max(1, int(round(h / 0.2)))
    style = pick(block['id'], 3)
    for r in range(rows):
        v0 = cy - h / 2 + (r + 0.5) * h / rows
        top, bottom = v0 + 0.028, v0 - 0.04          # the nameplate sits at the block's top edge: stay below it
        if usable < 0.12:
            out.append(fx(panel, (cx - 0.02, top), 'knob', 0.026, 'brass'))
            out.append(fx(panel, (cx + 0.04, top), 'knob', 0.026, 'brass'))
            out.append(fx(panel, (cx, bottom), 'socket', 0.012, 'chrome'))
            continue
        n = max(2, min(8, int(usable / 0.075)))
        if style == 0 or r % 2:
            for k in range(n):
                u = cx - usable / 2 + usable * (k + 0.5) / n
                out.append(fx(panel, (u, top), 'knob', 0.028, 'brass'))
                out.append(fx(panel, (u, bottom), 'socket', 0.012, 'chrome'))
        elif style == 1:
            out.append(fx(panel, (cx - usable / 2 + 0.07, v0 - 0.005), 'diagram', [0.12, 0.085]))
            m = max(2, (n - 2) // 2 + 1)
            for k in range(m):
                u = cx - usable / 2 + 0.17 + (usable - 0.2) * (k + 0.5) / m
                out.append(fx(panel, (u, top), 'knob', 0.028, 'brass'))
                out.append(fx(panel, (u, bottom), 'socket', 0.012, 'chrome'))
        else:
            for k in range(n):
                u = cx - usable / 2 + usable * (k + 0.5) / n
                out.append(fx(panel, (u, top), 'lamp', 0.008, ('amber', 'red', 'green')[(k + r) % 3]))
                out.append(fx(panel, (u, bottom), 'toggle', 0.012))
    return out


def generated(layout):
    out = []
    for b in layout['blocks']:
        if b['panel'] in RACKS:
            out += unit_fixtures(b)
    for kind, at, size, colour in GYRO:
        out.append(fx('gyro_panel', at, kind, size, colour))
    return out


def line(d):
    parts = []
    for k, v in d.items():
        parts.append('"%s": %s' % (k, json.dumps(v, ensure_ascii=False)))
    return '    { ' + ', '.join(parts) + ' }'


def rewrite(text):
    lines = [l for l in text.split(NL) if MARK not in l]      # drop the previous run's filler
    start = next(i for i, l in enumerate(lines) if re.match(r'\s*"fixtures":\s*\[', l))
    end = next(i for i in range(start + 1, len(lines)) if re.match(r'\s*\],?\s*$', lines[i]))
    lines[end - 1] = lines[end - 1].rstrip().rstrip(',')
    new = [line(d) + ',' for d in generated(json.loads(NL.join(lines)))]
    new[-1] = new[-1][:-1]
    lines[end - 1] += ','
    lines[end:end] = new
    return NL.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    with open(PATH, encoding='utf-8', newline='') as f:
        text = f.read()
    crlf = '\r\n' in text
    text = text.replace('\r\n', '\n')
    out = rewrite(text)
    json.loads(out)
    if args.check:
        sys.exit(0 if out == text else 1)
    if crlf:
        out = out.replace('\n', '\r\n')
    with open(PATH, 'w', encoding='utf-8', newline='') as f:
        f.write(out)
    print('fixtures written:', out.count(MARK))


if __name__ == '__main__':
    main()
