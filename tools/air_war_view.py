"""Read an air war campaign as a page: the enemy's decisions beside what our side learned (docs/air_war/).

    python tools/air_war_view.py out/air_war/air_war_nato.json     # writes out/air_war/air_war_nato.html

The input is the journal air_war_demo writes (simcore/examples/air_war_demo.cpp): every entry with its campaign hour,
kind, who and text, and whether our side knows it ("visible"), plus the enemy's staff as it really was and the last
intelligence summary. The page shows them on one timeline, day by day, the enemy's mind in one column and our side's
knowledge in the other, with filters by kind, and the reveal at the end. Standard library only.
"""
import html
import json
import sys
from pathlib import Path

KINDS = {
    "plan": "the planner's packages",
    "decision": "the commander's decisions",
    "result": "what happened",
    "career": "careers",
    "politics": "politics",
    "intel": "intelligence",
    "own_intel": "what our side learned",
    "player_mission": "the player's raids",
}

CSS = """
:root { --bg:#14170f; --panel:#1d2117; --ink:#ece9db; --dim:#9fa391; --amber:#ffc75a; --red:#ff6b55; --blue:#8fbfff; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--ink); font:14px/1.45 system-ui, sans-serif; }
header { padding:16px 20px; border-bottom:1px solid #3a3f2e; }
h1 { margin:0 0 4px; color:var(--amber); font-size:20px; }
.sub { color:var(--dim); }
.filters { margin-top:10px; display:flex; flex-wrap:wrap; gap:6px; }
.filters label { background:var(--panel); padding:3px 8px; border-radius:4px; cursor:pointer; }
main { padding:12px 20px 40px; max-width:1400px; }
h2 { color:var(--amber); font-size:16px; margin:22px 0 8px; border-bottom:1px solid #3a3f2e; }
.row { display:grid; grid-template-columns:72px 1fr 1fr; gap:12px; padding:4px 0; border-bottom:1px solid #22261b; }
.clock { color:var(--dim); font-variant-numeric:tabular-nums; }
.enemy .who, .ours .who { color:var(--dim); font-size:12px; }
.ours { color:var(--blue); }
.k-result .text { color:#ffd9a0; }
.k-career .text, .k-politics .text { color:var(--red); }
pre { white-space:pre-wrap; background:var(--panel); padding:12px; border-radius:6px; font:12.5px/1.4 ui-monospace, monospace; }
@media (max-width: 800px) { .row { grid-template-columns:60px 1fr; } .row > .ours { grid-column:2; } }
"""

JS = """
for (const box of document.querySelectorAll('.filters input')) {
  box.addEventListener('change', () => {
    for (const r of document.querySelectorAll('.k-' + box.value)) r.style.display = box.checked ? '' : 'none';
  });
}
"""


def day_of(clock):
    """'D3 05:30' -> 'D3'."""
    return clock.split(" ")[0] if clock else "?"


def render(doc):
    # In time order (stable: within an hour, the order things were written); the opening plan was made before the war.
    entries = sorted(doc.get("journal", []), key=lambda e: e.get("h", 0.0))
    faction = doc.get("faction", "?")
    out = ["<!doctype html><html lang='en'><head><meta charset='utf-8'>",
           "<meta name='viewport' content='width=device-width, initial-scale=1'>",
           f"<title>Air war: {html.escape(faction)}</title><style>{CSS}</style></head><body>",
           f"<header><h1>Air war: {html.escape(faction)}</h1>",
           f"<div class='sub'>{len(entries)} journal entries over {doc.get('days', '?')} days. Left: the enemy's mind (hidden from the "
           "player until the debrief). Right: what our side learned.</div><div class='filters'>"]
    present = sorted({e.get("kind", "") for e in entries})
    for k in present:
        out.append(f"<label><input type='checkbox' value='{html.escape(k)}' checked> {html.escape(KINDS.get(k, k))}</label>")
    out.append("</div></header><main>")
    day = None
    for e in entries:
        d = day_of(e.get("clock", ""))
        if d != day:
            day = d
            out.append(f"<h2>{html.escape(d)}</h2>")
        cell = (f"<div class='who'>{html.escape(e.get('who', ''))}</div>"
                f"<div class='text'>{html.escape(e.get('text', ''))}</div>")
        enemy, ours = ("", cell) if e.get("visible") else (cell, "")
        out.append(f"<div class='row k-{html.escape(e.get('kind', ''))}'>"
                   f"<div class='clock'>{html.escape(e.get('clock', '').split(' ')[-1])}</div>"
                   f"<div class='enemy'>{enemy}</div><div class='ours'>{ours}</div></div>")
    out.append("<h2>Our last intelligence summary</h2><pre>" + html.escape(doc.get("intsum", "")) + "</pre>")
    out.append("<h2>The enemy's staff, as it really was</h2><pre>" + html.escape(doc.get("enemy_mind", "")) + "</pre>")
    out.append(f"</main><script>{JS}</script></body></html>")
    return "\n".join(out)


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if not argv:
        print(__doc__)
        return 2
    src = Path(argv[0])
    dst = Path(argv[1]) if len(argv) > 1 else src.with_suffix(".html")
    doc = json.loads(src.read_text(encoding="utf-8"))
    dst.write_text(render(doc), encoding="utf-8")
    print(f"Wrote {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
